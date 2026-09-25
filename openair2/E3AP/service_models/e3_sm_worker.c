/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**
 * @file e3_sm_worker.c
 * @brief Shared telemetry-worker driver (see e3_sm_worker.h).
 */
#include "e3_sm_worker.h"

#include <inttypes.h>
#include <string.h>
#include <time.h>

#include "common/utils/LOG/log.h"

static uint64_t e3_now_monotonic_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

void e3_sm_worker_set_handle(e3_sm_worker_t *worker, e3_service_model_handle_t *handle)
{
  pthread_mutex_lock(&worker->lock);
  worker->sm_handle = handle;
  pthread_mutex_unlock(&worker->lock);
}

bool e3_sm_worker_is_running(e3_sm_worker_t *worker)
{
  return atomic_load(&worker->running);
}

void e3_sm_worker_set_period_us(e3_sm_worker_t *worker, uint32_t period_us)
{
  if (atomic_exchange(&worker->period_us, period_us) == period_us)
    return;
  LOG_I(E3AP,
        "[%s] emission cadence set to %s (period_us=%u)\n",
        worker->vtable->log_tag,
        period_us == 0 ? "on-data" : "periodic",
        period_us);
}

bool e3_sm_worker_emit_to_dapp(e3_sm_worker_t *worker, uint32_t dapp_id, const uint8_t *payload, size_t len)
{
  e3_service_model_handle_t *handle;
  pthread_mutex_lock(&worker->lock);
  handle = worker->sm_handle;
  pthread_mutex_unlock(&worker->lock);

  /* !handle = registration race during startup; count it as a failed emit
   * (and warn) instead of silently pretending the indication went out. */
  e3_error_t result = handle ? e3_service_model_emit_indication(handle, dapp_id, worker->vtable->ran_function_id, payload, len)
                             : E3_NOT_INITIALIZED;
  if (result != E3_SUCCESS && !worker->emit_warned) {
    worker->emit_warned = true;
    LOG_W(E3AP, "[%s] emit to dApp %u failed (rc=%d); subsequent failures silent\n", worker->vtable->log_tag, dapp_id, (int)result);
  }
  return result == E3_SUCCESS;
}

static void *e3_sm_worker_thread(void *arg)
{
  e3_sm_worker_t *worker = (e3_sm_worker_t *)arg;
  const e3_sm_worker_vtable_t *vtable = worker->vtable;
  LOG_I(E3AP, "[%s] worker thread started\n", vtable->log_tag);

  uint64_t emit_count = 0;
  /* Prime so periodic mode waits a full period on the first iteration
   * instead of busy-looping. */
  uint64_t last_emit_ns = e3_now_monotonic_ns();
  uint64_t observed_sequence = 0;
  /* Periodic mode: a snapshot was fetched into iteration_buffer during the
   * current period and not emitted yet. Publishes are bursty (TDD UL slots),
   * so the deadline usually falls in a publish gap; the pending snapshot is
   * what gets emitted there (latest wins, each fetch overwrites it). */
  bool pending = false;

  while (atomic_load(&worker->running)) {
    const uint32_t period_us = atomic_load(&worker->period_us);

    /* on-data (period 0): wait until a publish/shutdown wakes us.
     * periodic: wait the time left in the current period. */
    uint64_t wait_ns;
    if (period_us == 0) {
      wait_ns = UINT64_MAX;
    } else {
      const uint64_t period_ns = (uint64_t)period_us * 1000ULL;
      const uint64_t elapsed = e3_now_monotonic_ns() - last_emit_ns;
      wait_ns = (elapsed >= period_ns) ? 0 : (period_ns - elapsed);
    }

    const bool has_data = vtable->wait_and_fetch(vtable->iteration_buffer, wait_ns, &observed_sequence);

    if (!atomic_load(&worker->running))
      break;
    if (has_data)
      pending = true;

    if (period_us == 0) {
      if (!has_data)
        continue; /* shutdown poke or spurious wake */
    } else {
      const uint64_t period_ns = (uint64_t)period_us * 1000ULL;
      if (e3_now_monotonic_ns() - last_emit_ns < period_ns)
        continue; /* deadline not reached: keep draining, latest wins */
      if (!pending) {
        /* Deadline passed with nothing fetched all period (idle
         * source): re-arm so the next wait blocks a full period. */
        last_emit_ns = e3_now_monotonic_ns();
        continue;
      }
    }

    const uint64_t candidate = emit_count + 1;
    if (vtable->emit(vtable->iteration_buffer, candidate)) {
      emit_count = candidate;
      last_emit_ns = e3_now_monotonic_ns();
      pending = false;
    }
  }

  LOG_I(E3AP, "[%s] worker thread stopped (emitted %" PRIu64 " batches)\n", vtable->log_tag, emit_count);
  return NULL;
}

e3_error_t e3_sm_worker_init(e3_sm_worker_t *worker)
{
  LOG_I(E3AP, "[%s] init (RF=%u)\n", worker->vtable->log_tag, (unsigned)worker->vtable->ran_function_id);
  return E3_SUCCESS;
}

void e3_sm_worker_destroy(e3_sm_worker_t *worker)
{
  if (worker->vtable->on_destroy)
    worker->vtable->on_destroy();
  LOG_I(E3AP, "[%s] destroy\n", worker->vtable->log_tag);
}

e3_error_t e3_sm_worker_start(e3_sm_worker_t *worker)
{
  pthread_mutex_lock(&worker->lock);
  if (worker->thread_started) {
    pthread_mutex_unlock(&worker->lock);
    return E3_SUCCESS;
  }
  atomic_store(&worker->running, true);

  /* Optional pre-thread bring-up (e.g. shm ring); it logs its own non-fatal
   * failures and we proceed so the worker can retry later. */
  if (worker->vtable->on_start)
    worker->vtable->on_start();

  int result = pthread_create(&worker->thread, NULL, e3_sm_worker_thread, worker);
  if (result != 0) {
    atomic_store(&worker->running, false);
    pthread_mutex_unlock(&worker->lock);
    LOG_E(E3AP, "[%s] pthread_create failed: %s\n", worker->vtable->log_tag, strerror(result));
    return E3_INTERNAL_ERROR;
  }
  worker->thread_started = true;
  pthread_mutex_unlock(&worker->lock);
  LOG_I(E3AP, "[%s] started (first subscription)\n", worker->vtable->log_tag);
  return E3_SUCCESS;
}

void e3_sm_worker_stop(e3_sm_worker_t *worker)
{
  pthread_mutex_lock(&worker->lock);
  if (!worker->thread_started) {
    pthread_mutex_unlock(&worker->lock);
    return;
  }
  atomic_store(&worker->running, false);
  pthread_t thread = worker->thread;
  worker->thread_started = false;
  pthread_mutex_unlock(&worker->lock);

  /* Wake the worker if it's blocked in its wait. */
  if (worker->vtable->signal_shutdown)
    worker->vtable->signal_shutdown();

  pthread_join(thread, NULL);

  /* Keep worker->sm_handle: it is the registered SM handle, owned by libe3
   * for the process lifetime, and the next sm_start reuses it (nothing
   * re-sets it on restart). The worker is joined, so no late emit exists. */

  /* Optional post-join teardown (e.g. shm ring). */
  if (worker->vtable->on_stop)
    worker->vtable->on_stop();

  LOG_I(E3AP, "[%s] stopped (last subscription gone)\n", worker->vtable->log_tag);
}
