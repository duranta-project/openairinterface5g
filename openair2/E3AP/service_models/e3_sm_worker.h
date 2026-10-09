/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**
 * @file e3_sm_worker.h
 * @brief Shared telemetry-worker driver for the E3 service models.
 *
 * One background-thread engine drives every telemetry SM: it sleeps until new
 * data arrives (or the emit period elapses), then emits one batch to the
 * subscribed dApps. An SM plugs in only its differences (where its data comes
 * from, how it encodes, which shm it owns) through the vtable below; the
 * lifecycle (init/start/stop/destroy), the on-data vs periodic wait loop and
 * the emit bookkeeping live in e3_sm_worker.c. Each SM keeps its own worker
 * instance, so SMs stay fully independent at runtime (separate threads, wake
 * sources and shared memory).
 */
#ifndef OPENAIR2_E3AP_SERVICE_MODELS_E3_SM_WORKER_H
#define OPENAIR2_E3AP_SERVICE_MODELS_E3_SM_WORKER_H

#include <libe3/c_api.h>
#include <libe3/error_codes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct e3_sm_worker_vtable {
  uint32_t ran_function_id; /* on-wire RF id, used in emit + logs */
  const char *log_tag; /* short tag for log lines, e.g. "KPM-SM" */
  void *iteration_buffer; /* SM-owned per-iteration data buffer */
  bool (*wait_and_fetch)(void *iteration_buffer, uint64_t wait_ns, uint64_t *caller_sequence);
  bool (*emit)(void *iteration_buffer, uint64_t batch_count);
  void (*signal_shutdown)(void);
  void (*on_start)(void); /* optional (NULL = no-op): pre-thread bring-up */
  void (*on_stop)(void); /* optional: post-join teardown */
  void (*on_destroy)(void); /* optional: teardown at SM destroy */
} e3_sm_worker_vtable_t;

typedef struct {
  pthread_t thread;
  _Atomic bool running;
  bool thread_started;
  pthread_mutex_t lock;
  e3_service_model_handle_t *sm_handle;
  _Atomic uint32_t period_us;
  bool emit_warned;
  const e3_sm_worker_vtable_t *vtable;
} e3_sm_worker_t;

#define E3_SM_WORKER_INITIALIZER(vtable_ptr, default_period_us)                                                       \
  {                                                                                                                   \
    .running = ATOMIC_VAR_INIT(false), .thread_started = false, .lock = PTHREAD_MUTEX_INITIALIZER, .sm_handle = NULL, \
    .period_us = ATOMIC_VAR_INIT(default_period_us), .emit_warned = false, .vtable = (vtable_ptr),                    \
  }

/* Registered SM handle used for emits; owned by libe3 for the process
 * lifetime once registered. */
void e3_sm_worker_set_handle(e3_sm_worker_t *worker, e3_service_model_handle_t *handle);

bool e3_sm_worker_is_running(e3_sm_worker_t *worker);

/* Emit cadence: 0 = on-data (one batch per publish), >0 = periodic,
 * microseconds between batches. No-op (and silent) when unchanged. */
void e3_sm_worker_set_period_us(e3_sm_worker_t *worker, uint32_t period_us);

/* Send one encoded indication to one dApp; warns once on the first failure. */
bool e3_sm_worker_emit_to_dapp(e3_sm_worker_t *worker, uint32_t dapp_id, const uint8_t *payload, size_t len);

/* Lifecycle, called from the SM's libe3 callbacks: start spawns the worker
 * thread on the first subscription (running vtable->on_start before it);
 * stop wakes+joins it on the last unsubscribe (then vtable->on_stop). */
e3_error_t e3_sm_worker_init(e3_sm_worker_t *worker);
e3_error_t e3_sm_worker_start(e3_sm_worker_t *worker);
void e3_sm_worker_stop(e3_sm_worker_t *worker);
void e3_sm_worker_destroy(e3_sm_worker_t *worker);

#endif /* OPENAIR2_E3AP_SERVICE_MODELS_E3_SM_WORKER_H */
