/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**
 * @file l1_kpm_sm.c
 * @brief L1-KPM SM: streams post-FFT IQ snapshots (RF=2) to subscribers.
 *
 * This file adds the L1-specific parts: where snapshots come from (/e3_ran_buffers), how
 * they are encoded, and the SM descriptor.
 */
#include "l1_kpm_sm.h"
#include "../../e3_log.h"
#include "l1_kpm_enc.h"
#include "e3_ran_buffers.h"
#include "common/utils/LOG/log.h"
#include "../../e3_agent.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../e3_sm_worker.h"

/* Default 0 (on-data): one indication per IQ snapshot so the dashboard sees
 * the full publish rate. The agent overrides this with the periodicity the
 * subscribed dApps declared (e3_agent_get_subscription_periodicity). */
#define L1_KPM_DEFAULT_PERIOD_US 0u

/* Worker-thread vtable hooks (defined below; forward-declared for the vtable). */
static bool l1_wait_and_fetch(void *iteration_buffer, uint64_t wait_ns, uint64_t *caller_sequence);
static bool l1_emit(void *iteration_buffer, uint64_t batch_count);

/* Worker-thread-only scratch: the latest /e3_ran_buffers snapshot. */
static e3_ran_buffers_slot_info_t g_l1_slot;

static const e3_sm_worker_vtable_t g_l1_vtable = {
    .ran_function_id = L1_KPM_SM_RAN_FUNCTION_ID,
    .log_tag = "KPM-SM",
    .iteration_buffer = &g_l1_slot,
    .wait_and_fetch = l1_wait_and_fetch,
    .emit = l1_emit,
    .signal_shutdown = e3_ran_buffers_signal_shutdown,
    .on_start = NULL,
    .on_stop = NULL,
    .on_destroy = e3_ran_buffers_destroy,
};

static e3_sm_worker_t g_kpm_worker = E3_SM_WORKER_INITIALIZER(&g_l1_vtable, L1_KPM_DEFAULT_PERIOD_US);

/* Encode one indication and send it to every current subscriber. Subscribers
 * are fetched once per batch (a single shared-lock acquire) to avoid starving
 * dApp setup at high cadence; the bytes are identical for all of them. */
static void emit_batch(const e3_ran_buffers_slot_info_t *slot, uint64_t batch_count_for_logging)
{
  size_t num_dapps = 0;
  uint32_t *subscribers = e3_agent_get_ran_function_subscribers(e3.agent, L1_KPM_SM_RAN_FUNCTION_ID, &num_dapps);

  if (subscribers && num_dapps > 0) {
    uint8_t encoded_buffer[512];
    int encoded_len = l1_kpm_enc_indication(slot, encoded_buffer, sizeof(encoded_buffer));
    size_t num_sent = 0, num_skipped = 0;

    if (encoded_len < 0) {
      static int warned = 0;
      if (!warned) {
        warned = 1;
        KPM_LOG_E("indication encode failed (overflow); silenced\n");
      }
      num_skipped = num_dapps;
    } else {
      for (size_t i = 0; i < num_dapps; ++i) {
        if (e3_sm_worker_emit_to_dapp(&g_kpm_worker, subscribers[i], (const uint8_t *)encoded_buffer, (size_t)encoded_len))
          num_sent++;
        else
          num_skipped++;
      }
    }

    if (batch_count_for_logging == 1) {
      KPM_LOG_I(
          "first indication batch: subs=%zu (sent=%zu skipped=%zu) "
          "sfn=%u slot=%u fh=(%u,%u) size=%dB\n",
          num_dapps,
          num_sent,
          num_skipped,
          (unsigned)slot->sfn,
          (unsigned)slot->slot,
          (unsigned)slot->fh_buffer_index,
          (unsigned)slot->fh_write_index,
          encoded_len);
    } else if ((batch_count_for_logging % 1024) == 0) {
      KPM_LOG_I("emitted %" PRIu64 " batches (latest sfn=%u slot=%u sent=%zu)\n",
                batch_count_for_logging,
                (unsigned)slot->sfn,
                (unsigned)slot->slot,
                num_sent);
    }
  }
  e3_agent_free_uint32_array(subscribers);
}

/* ---- Worker-thread vtable hooks ---- */

/* Block for the next /e3_ran_buffers publish and report whether a slot is
 * ready. Fetch into a local first: a timed-out wait must leave the buffer
 * untouched, since in periodic mode it may still hold the pending snapshot
 * for this period. */
static bool l1_wait_and_fetch(void *iteration_buffer, uint64_t wait_ns, uint64_t *caller_sequence)
{
  e3_ran_buffers_slot_info_t *slot = (e3_ran_buffers_slot_info_t *)iteration_buffer;
  e3_ran_buffers_slot_info_t fresh;
  memset(&fresh, 0, sizeof(fresh));
  e3_ran_buffers_wait_for_publish(wait_ns, caller_sequence, &fresh);
  if (!fresh.valid)
    return false;
  *slot = fresh;
  return true;
}

/* The snapshot is already in the buffer, so emit always succeeds here. */
static bool l1_emit(void *iteration_buffer, uint64_t batch_count)
{
  emit_batch((const e3_ran_buffers_slot_info_t *)iteration_buffer, batch_count);
  return true;
}

/* ---- Public API ---- */

void l1_kpm_sm_set_handle(e3_service_model_handle_t *sm_handle)
{
  e3_sm_worker_set_handle(&g_kpm_worker, sm_handle);
}

bool l1_kpm_sm_has_subscribers(void)
{
  return e3_sm_worker_is_running(&g_kpm_worker);
}

void l1_kpm_sm_set_period_us(uint32_t period_us)
{
  e3_sm_worker_set_period_us(&g_kpm_worker, period_us);
}

/* ---- libe3 callbacks ---- */

static e3_error_t kpm_sm_init(void *user_data)
{
  (void)user_data;
  return e3_sm_worker_init(&g_kpm_worker);
}
static void kpm_sm_destroy(void *user_data)
{
  (void)user_data;
  e3_sm_worker_destroy(&g_kpm_worker);
}
static e3_error_t kpm_sm_start(void *user_data)
{
  (void)user_data;
  return e3_sm_worker_start(&g_kpm_worker);
}
static void kpm_sm_stop(void *user_data)
{
  (void)user_data;
  e3_sm_worker_stop(&g_kpm_worker);
}
static int kpm_sm_is_running(void *user_data)
{
  (void)user_data;
  return e3_sm_worker_is_running(&g_kpm_worker) ? 1 : 0;
}

/* This SM is telemetry-only: no controls. An unset sm_process_control makes
 * libe3 reply with a negative ack. */

/* ---- Descriptor ---- */

/* RAN-function identity advertised at setup (serialized by the active encoder). */
#define L1_KPM_RF_NAME "L1-KPM"
#define L1_KPM_RF_VERSION 1
#define L1_KPM_RF_DESCRIPTION "L1-KPM SM: post-FFT IQ telemetry."

e3_c_service_model_desc_t *create_l1_kpm_sm_model(void)
{
  static const uint32_t telemetry_ids[] = {
      L1_KPM_TID_IQ_SAMPLES,
      L1_KPM_TID_TIMESTAMP,
      L1_KPM_TID_SFN,
      L1_KPM_TID_SLOT,
  };

  /* RAN-function descriptor for the setup response. Built once and cached; the
   * E3-SetupResponse ranFunctionList requires a non-empty ranFunctionData
   * (OCTET STRING SIZE(1..32768)) per advertised function — an empty one fails
   * APER encode on the ASN.1 channel. */
  static uint8_t *ran_function_data = NULL;
  static size_t ran_function_data_len = 0;
  static int ran_function_data_ready = 0;
  if (!ran_function_data_ready) {
    int rc = l1_kpm_enc_ran_function_data(L1_KPM_RF_NAME,
                                          L1_KPM_RF_VERSION,
                                          L1_KPM_RF_DESCRIPTION,
                                          &ran_function_data,
                                          &ran_function_data_len);
    if (rc != 0) {
      /* Registering an SM that cannot describe itself would only fail later, at
       * setup, for every advertised RAN function. Leave the latch clear so a
       * retry is still possible. */
      KPM_LOG_E("failed to encode RAN function data (%d); not registering the SM\n", rc);
      return NULL;
    }
    ran_function_data_ready = 1;
  }

  static e3_c_service_model_desc_t desc;
  memset(&desc, 0, sizeof(desc));
  desc.name = "L1-KPM";
  desc.version = 1;
  desc.ran_function_id = L1_KPM_SM_RAN_FUNCTION_ID;
  desc.telemetry_ids = telemetry_ids;
  desc.telemetry_ids_len = sizeof(telemetry_ids) / sizeof(telemetry_ids[0]);
  desc.control_ids = NULL;
  desc.control_ids_len = 0;
  desc.ran_function_data = ran_function_data;
  desc.ran_function_data_len = ran_function_data_len;
  desc.sm_init = kpm_sm_init;
  desc.sm_destroy = kpm_sm_destroy;
  desc.sm_start = kpm_sm_start;
  desc.sm_stop = kpm_sm_stop;
  desc.sm_is_running = kpm_sm_is_running;
  desc.sm_process_control = NULL;
  desc.sm_context = &g_kpm_worker;
  return &desc;
}
