/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Spectrum SM (RAN Function ID = 1): sensing-range telemetry out.
 *
 * Telemetry emitted (TIDs 1-5): shm-reference indications into the
 * /e3_l2_sensing ring, which is written on EVERY MAC sensing publish while the
 * subscription periodicity throttles only the indications. The worker thread,
 * ring and encoders are driven from this SM's libe3 lifecycle callbacks. IQ
 * stays on the L1 path (/e3_ran_buffers, RF=2 L1-KPM SM).
 */

#include "spectrum_sm.h"
#include "../../e3_log.h"
#include "spectrum_enc.h"
#include "spectrum_sensing_ring.h"

#include "../../e3_agent.h"

#include "common/utils/LOG/log.h"
#include "LAYER2/NR_MAC_gNB/gNB_scheduler_ul_sensing_types.h" /* sensing publish/range API */

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../e3_sm_worker.h"

static spectrum_sm_context_t spectrum_ctx = {.lock = PTHREAD_MUTEX_INITIALIZER};
static uint8_t *spectrum_ran_function_data = NULL;
static size_t spectrum_ran_function_data_len = 0;
static int spectrum_ran_function_data_ready = 0;

/* e3_service_model_emit_message_ack response codes (libe3 convention). */
#define SPECTRUM_SM_ACK_POSITIVE 0
#define SPECTRUM_SM_ACK_NEGATIVE 1

/* Sensing-range telemetry stream TIDs (advertised in the setupResponse). */
static uint32_t spectrum_telemetry_ids[] = {
    SPECTRUM_SM_TID_SENSING_RANGES,
    SPECTRUM_SM_TID_TIMESTAMP,
    SPECTRUM_SM_TID_SFN,
    SPECTRUM_SM_TID_SLOT,
    SPECTRUM_SM_TID_BEAM,
};

static e3_error_t spectrum_sm_init(void *sm_context);
static void spectrum_sm_destroy(void *sm_context);
static e3_error_t spectrum_sm_start(void *sm_context);
static void spectrum_sm_stop(void *sm_context);
static int spectrum_sm_is_running(void *sm_context);

static e3_c_service_model_desc_t spectrum_sm_desc = {
    .name = "spectrum_sm",
    .version = 1,
    .ran_function_id = E3_SM_ID_SPECTRUM,
    .telemetry_ids = spectrum_telemetry_ids,
    .telemetry_ids_len = sizeof(spectrum_telemetry_ids) / sizeof(spectrum_telemetry_ids[0]),
    .ran_function_data = NULL,
    .ran_function_data_len = 0,
    .sm_init = spectrum_sm_init,
    .sm_destroy = spectrum_sm_destroy,
    .sm_start = spectrum_sm_start,
    .sm_stop = spectrum_sm_stop,
    .sm_is_running = spectrum_sm_is_running,
    .sm_context = &spectrum_ctx,
};

/* ============================================================================
 * Sensing-range telemetry engine.
 *
 * The RF=1 telemetry-out path, split producer/sampler: for EVERY MAC sensing
 * publish the worker fetches the (beam, slot) ranges and writes them into the
 * /e3_l2_sensing shm ring, so the ring carries the full sensing record --
 * symmetric with the KPM side, whose /e3_ran_buffers is written on every
 * UL/MIXED slot. The emission mode (spectrum_telemetry_set_period_us) then
 * throttles only the indications: on-data (period_us == 0, the default) emits
 * one shm-reference indication per publish; periodic (the periodicity the
 * subscribed dApps declare through their subscription) samples the latest
 * pending snapshot once per period.
 * ============================================================================ */
#define SPECTRUM_TELEMETRY_DEFAULT_PERIOD_US 0u

/* Worker-thread vtable hooks (defined below; forward-declared for the vtable). */
static bool telemetry_wait_and_fetch(void *iteration_buffer, uint64_t wait_ns, uint64_t *caller_sequence);
static bool telemetry_emit(void *iteration_buffer, uint64_t batch_count);
static void telemetry_on_start(void);

/* Worker-thread-only scratch: the latest publish meta + its ring reference
 * (the ranges themselves are written to the ring at fetch time; the emit step
 * only references them). */
typedef struct {
  nr_mac_sensing_publish_meta_t meta;
  uint8_t n_ranges;
  uint32_t shm_write_idx;
  bool shm_ok;
} telemetry_iter_t;
static telemetry_iter_t g_telemetry_iter;

static const e3_sm_worker_vtable_t g_telemetry_vtable = {
    .ran_function_id = E3_SM_ID_SPECTRUM, /* emits on the Spectrum SM (RF=1) */
    .log_tag = "SPECTRUM-SM",
    .iteration_buffer = &g_telemetry_iter,
    .wait_and_fetch = telemetry_wait_and_fetch,
    .emit = telemetry_emit,
    .signal_shutdown = nr_mac_signal_sensing_shutdown,
    .on_start = telemetry_on_start, /* bring up /e3_l2_sensing ring */
    .on_stop = spectrum_sensing_ring_destroy, /* tear it down after join */
    .on_destroy = NULL,
};

static e3_sm_worker_t g_spectrum_telemetry = E3_SM_WORKER_INITIALIZER(&g_telemetry_vtable, SPECTRUM_TELEMETRY_DEFAULT_PERIOD_US);

/* Encode one batch (a fixed-size reference to an already-written ring entry,
 * identical for all subscribers) and fan it out to every current Spectrum SM
 * (RF=1) subscriber. Without the shm ring there is nothing to point at, so
 * "no shm, no indication". */
static void emit_batch(const nr_mac_sensing_publish_meta_t *meta,
                       uint8_t n_ranges,
                       uint32_t shm_write_idx,
                       bool shm_ok,
                       uint64_t batch_count_for_logging)
{
  size_t num_dapps = 0;
  uint32_t *subscribers = e3_agent_get_ran_function_subscribers(e3.agent, E3_SM_ID_SPECTRUM, &num_dapps);

  if (subscribers && num_dapps > 0) {
    size_t num_sent = 0, num_skipped = 0;
    int encoded_len = -1;
    uint8_t encoded_buffer[512];

    if (!shm_ok) {
      static int warned = 0;
      if (!warned) {
        warned = 1;
        SPEC_LOG_E("/e3_l2_sensing unavailable; indications skipped\n");
      }
      num_skipped = num_dapps;
    } else {
      encoded_len = spectrum_encode_indication(meta, shm_write_idx, n_ranges, encoded_buffer, sizeof(encoded_buffer));
      if (encoded_len < 0) {
        static int warned = 0;
        if (!warned) {
          warned = 1;
          SPEC_LOG_E("indication encode failed (overflow); silenced\n");
        }
        num_skipped = num_dapps;
      } else {
        for (size_t i = 0; i < num_dapps; ++i) {
          if (e3_sm_worker_emit_to_dapp(&g_spectrum_telemetry,
                                        subscribers[i],
                                        (const uint8_t *)encoded_buffer,
                                        (size_t)encoded_len))
            num_sent++;
          else
            num_skipped++;
        }
      }
    }

    if (batch_count_for_logging == 1) {
      SPEC_LOG_I(
          "first indication batch: subs=%zu (sent=%zu skipped=%zu) "
          "sfn=%u slot=%u beam=%u ranges=%u size=%dB\n",
          num_dapps,
          num_sent,
          num_skipped,
          (unsigned)meta->frame,
          (unsigned)meta->slot,
          (unsigned)meta->beam,
          (unsigned)n_ranges,
          encoded_len);
    } else if ((batch_count_for_logging % 1024) == 0) {
      SPEC_LOG_I("emitted %" PRIu64 " batches (latest sfn=%u slot=%u sent=%zu)\n",
                 batch_count_for_logging,
                 (unsigned)meta->frame,
                 (unsigned)meta->slot,
                 num_sent);
    }
  }
  e3_agent_free_uint32_array(subscribers);
}

/* ---- Worker-thread vtable hooks ---- */

/* Block for the next MAC sensing publish; report whether one arrived. This is
 * the producer step, run for EVERY publish (not just the emitted ones): the
 * (beam, slot) ranges are fetched while the publish is fresh -- the seqlock
 * slot storage is recycled by newer publishes -- and written into the
 * /e3_l2_sensing ring, so the ring carries the full sensing record and the
 * emission period throttles only the indications. Fetch into locals first: a
 * timed-out wait or a failed fetch must leave the buffer untouched, since in
 * periodic mode it may still hold the pending snapshot for this period. The
 * ranges fetch retries torn seqlock reads internally; one extra attempt
 * covers a write landing between the two. */
static bool telemetry_wait_and_fetch(void *iteration_buffer, uint64_t wait_ns, uint64_t *caller_sequence)
{
  telemetry_iter_t *it = (telemetry_iter_t *)iteration_buffer;
  nr_mac_sensing_publish_meta_t meta;
  memset(&meta, 0, sizeof(meta));
  nr_mac_wait_for_sensing_publish(wait_ns, caller_sequence, &meta);
  if (meta.timestamp_ns == 0)
    return false;

  sensing_range_t ranges[MAX_SENSING_RANGES];
  uint8_t n_ranges = 0;
  bool ok = false;
  for (int attempt = 0; attempt < 2 && !ok; ++attempt) {
    ok = nr_mac_get_sensing_ranges(/*mod_id=*/0, meta.beam, meta.slot, ranges, MAX_SENSING_RANGES, &n_ranges);
  }
  if (!ok)
    return false; /* transient (writer churn); drop this publish */

  uint32_t shm_write_idx = 0;
  it->shm_ok = (spectrum_sensing_ring_write(&meta, ranges, n_ranges, &shm_write_idx) == 0);
  it->meta = meta;
  it->n_ranges = n_ranges;
  it->shm_write_idx = shm_write_idx;
  return true;
}

/* Emit the pending snapshot: a pure sampling step, the ring entry it
 * references was already written at fetch time. */
static bool telemetry_emit(void *iteration_buffer, uint64_t batch_count)
{
  telemetry_iter_t *it = (telemetry_iter_t *)iteration_buffer;
  emit_batch(&it->meta, it->n_ranges, it->shm_write_idx, it->shm_ok, batch_count);
  return true;
}

/* Bring up the /e3_l2_sensing shm ring before the worker can emit, so the
 * subscribing dApp can mmap it promptly. Non-fatal: the worker lazily retries
 * on the first write. */
static void telemetry_on_start(void)
{
  if (spectrum_sensing_ring_init() != 0)
    SPEC_LOG_W("/e3_l2_sensing init failed at start; will retry on first write\n");
}

/* Public API (spectrum_sm.h) for e3_agent.c; the rest of the SM drives the
 * telemetry worker directly via e3_sm_worker_*(&g_spectrum_telemetry). */
void spectrum_telemetry_set_period_us(uint32_t period_us)
{
  e3_sm_worker_set_period_us(&g_spectrum_telemetry, period_us);
}

void spectrum_sm_set_handle(e3_service_model_handle_t *sm_handle)
{
  /* The telemetry worker is the only consumer of the handle (it emits through
   * it); e3_sm_worker_set_handle takes its own lock. */
  e3_sm_worker_set_handle(&g_spectrum_telemetry, sm_handle);
}

e3_c_service_model_desc_t *create_spectrum_sm_model(void)
{
  if (!spectrum_ran_function_data_ready) {
    SPEC_LOG_I("Encoding RAN function data with %s encoder\n",
               e3_get_encoding() == E3_ENCODING_ASN1       ? "ASN.1"
               : e3_get_encoding() == E3_ENCODING_PROTOBUF ? "Protocol Buffers"
                                                           : "JSON");
    int rc = spectrum_encode_ran_function_data(&spectrum_ran_function_data, &spectrum_ran_function_data_len);
    if (rc != E3_SUCCESS) {
      /* Registering an SM that cannot describe itself would only fail later, at
       * setup, for every advertised RAN function. Leave the latch clear so a
       * retry is still possible. */
      SPEC_LOG_E("Failed to encode RAN function data (%d); not registering the SM\n", rc);
      return NULL;
    }
    spectrum_ran_function_data_ready = 1;
  }

  spectrum_sm_desc.ran_function_data = spectrum_ran_function_data;
  spectrum_sm_desc.ran_function_data_len = spectrum_ran_function_data_len;
  return &spectrum_sm_desc;
}

static e3_error_t spectrum_sm_init(void *sm_context)
{
  spectrum_sm_context_t *ctx = (spectrum_sm_context_t *)sm_context;
  if (!ctx) {
    return E3_SM_ERROR_INVALID_PARAM;
  }

  /* The lock is statically initialized (PTHREAD_MUTEX_INITIALIZER), so it is
   * valid even when spectrum_sm_set_handle() locks it before sm_init runs. Do
   * NOT memset/re-init it here (re-initing a possibly-locked mutex is UB). */
  ctx->initialized = true;

  return e3_sm_worker_init(&g_spectrum_telemetry);
}

static e3_error_t spectrum_sm_start(void *sm_context)
{
  spectrum_sm_context_t *ctx = (spectrum_sm_context_t *)sm_context;
  if (!ctx || !ctx->initialized) {
    return E3_NOT_INITIALIZED;
  }

  /* Spawn the sensing-range telemetry worker on first subscription (it brings
   * up the /e3_l2_sensing ring first so a subscribing dApp can mmap it
   * promptly). Idempotent on a second start. */
  e3_error_t err = e3_sm_worker_start(&g_spectrum_telemetry);
  if (err != E3_SUCCESS) {
    SPEC_LOG_E("telemetry worker failed to start (err=%d)\n", err);
    return err;
  }

  pthread_mutex_lock(&ctx->lock);
  ctx->running = true;
  pthread_mutex_unlock(&ctx->lock);

  return E3_SUCCESS;
}

static void spectrum_sm_stop(void *sm_context)
{
  spectrum_sm_context_t *ctx = (spectrum_sm_context_t *)sm_context;
  if (!ctx) {
    return;
  }

  pthread_mutex_lock(&ctx->lock);
  ctx->running = false;
  pthread_mutex_unlock(&ctx->lock);

  /* Join the telemetry worker and tear down the shm ring. */
  e3_sm_worker_stop(&g_spectrum_telemetry);
}

/* Caller must hold ctx->lock. */
static int is_running_locked(const spectrum_sm_context_t *ctx)
{
  return ctx->running ? 1 : 0;
}

static int spectrum_sm_is_running(void *sm_context)
{
  spectrum_sm_context_t *ctx = (spectrum_sm_context_t *)sm_context;
  if (!ctx) {
    return 0;
  }
  pthread_mutex_lock(&ctx->lock);
  int running = is_running_locked(ctx);
  pthread_mutex_unlock(&ctx->lock);
  return running;
}

static void spectrum_sm_destroy(void *sm_context)
{
  spectrum_sm_context_t *ctx = (spectrum_sm_context_t *)sm_context;
  if (!ctx) {
    return;
  }

  spectrum_sm_stop(sm_context);
  e3_sm_worker_destroy(&g_spectrum_telemetry);

  if (ctx->initialized) {
    pthread_mutex_destroy(&ctx->lock);
  }

  if (spectrum_ran_function_data) {
    free(spectrum_ran_function_data);
    spectrum_ran_function_data = NULL;
    spectrum_ran_function_data_len = 0;
    spectrum_ran_function_data_ready = 0;
    spectrum_sm_desc.ran_function_data = NULL;
    spectrum_sm_desc.ran_function_data_len = 0;
  }

  memset(ctx, 0, sizeof(*ctx));
}
