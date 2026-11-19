/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* pthread_setname_np */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "e3_pending_ctrl.h"
#include "service_models/spectrum_sm/spectrum_enc.h"

#include "openair2/E2AP/flexric/src/agent/e2_agent_api.h"
#include "openair2/E2AP/flexric/src/sm/dapp_sm/enc/dapp_enc_asn.h"
#include "openair2/E2AP/flexric/src/sm/dapp_sm/ie/dapp_data_ie.h"
#include "openair2/E2AP/flexric/src/util/time_now_us.h"

#include "common/utils/LOG/log.h"
#include "common/utils/assertions.h"

#include <libe3/error_codes.h>

#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/* ---------------------------------------------------------------------------
 * Deferred control acknowledges
 *
 * write_ctrl_dapp_sm() answers NONE and opens a pending entry; the E3 control
 * handler and the MAC tick fill it in. This is the other end: a thread that
 * turns each completed entry into the E2SM-DAPP control outcome the xApp has
 * been waiting for, and retires anything that waited too long.
 *
 * It runs here rather than on either of those threads because encoding and
 * handing to the agent is neither's job -- the MAC's tick has a slot deadline,
 * and the E3 handler has already returned to libe3 by the time the mask reaches
 * the air.
 * ------------------------------------------------------------------------- */

/* The E3 agent's strong definition of the MAC's weak hook. Runs on the
 * scheduler thread inside the slot deadline, so it does exactly one clock read
 * and one store into a slot the relay drains (see e3_pending_ctrl.h). */
void prb_block_on_air_hook(uint32_t sequence_id, uint16_t sfn, uint16_t slot)
{
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  e3_pending_ctrl_on_air(sequence_id, (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec, sfn, slot);
}

/* The MAC's weak supersede hook. A later install replaced this control's mask
 * before any tick ran with it, so the procedure ends here: installed, never on
 * the air. Same constraints as the on-air hook -- scheduler thread, so this
 * only flips a state the relay picks up. */
void prb_block_superseded_hook(uint32_t sequence_id)
{
  e3_pending_ctrl_superseded(sequence_id);
}

static void send_deferred_ctrl_ack(const e3_ctrl_outcome_t* outcome)
{
  spectrum_apply_outcome_t apply = {
      .built_ts_ns = (int64_t)time_now_ns(),
      .installed_ts_ns = outcome->installed_ts_ns,
      .on_air_ts_ns = outcome->on_air_ts_ns,
      .sfn = outcome->sfn,
      .slot = outcome->slot,
  };

  e2sm_dapp_ctrl_out_t ctrl_out = {0};
  ctrl_out.format = FORMAT_1_E2SM_DAPP_CTRL_OUT;
  ctrl_out.frmt_1.timestamp_ns = apply.built_ts_ns;
  ctrl_out.frmt_1.sequence_id = outcome->sequence_id;

  /* The apply outcome is the Service Model's to describe, and it rides the
   * generic envelope as opaque octets. A control that never applied carries
   * none: the acknowledge still goes out, saying only that the procedure
   * ended. An install with no on-air instant is the superseded case -- applied
   * to the MAC, replaced before the scheduler ever ran with it -- and the
   * encoder omits onAirTimestamp, which is what says so on the wire. */
  uint8_t* sm_outcome = NULL;
  size_t sm_outcome_size = 0;
  if (outcome->installed_ts_ns != 0 || outcome->on_air_ts_ns != 0) {
    if (spectrum_encode_apply_outcome(&apply, &sm_outcome, &sm_outcome_size) == E3_SUCCESS) {
      ctrl_out.frmt_1.e3_control_outcome = sm_outcome;
      ctrl_out.frmt_1.e3_control_outcome_size = (uint32_t)sm_outcome_size;
    } else {
      LOG_W(E3AP, "[RAN FUNC DAPP CTRL] failed to encode the apply outcome for seq=%u\n", outcome->sequence_id);
    }
  }

  byte_array_t ba = dapp_enc_ctrl_out_asn(&ctrl_out);
  free(sm_outcome);

  /* Ownership of ba.buf passes to the agent, which frees it once the
   * acknowledge is on the wire. */
  complete_control_agent_api(outcome->sequence_id, ba.buf, ba.len);
}

static void* dapp_ctrl_ack_relay(void* arg)
{
  (void)arg;
  while (true) {
    e3_ctrl_outcome_t done[E3_PENDING_CTRL_MAX];
    size_t n = e3_pending_ctrl_drain(done, E3_PENDING_CTRL_MAX);
    for (size_t i = 0; i < n; ++i)
      send_deferred_ctrl_ack(&done[i]);

    /* Retire whatever the dApp never answered. Rare, so it shares the poll
     * rather than getting a timer of its own. */
    size_t t = e3_pending_ctrl_sweep(done, E3_PENDING_CTRL_MAX);
    for (size_t i = 0; i < t; ++i) {
      LOG_W(E3AP, "[RAN FUNC DAPP CTRL] procedure %u timed out before the mask went on air\n", done[i].sequence_id);
      send_deferred_ctrl_ack(&done[i]);
    }

    /* 1 ms: an order below the loop it is reporting on, and cheap -- the drain
     * is a scan of 64 relaxed loads when there is nothing to do. */
    usleep(1000);
  }
  return NULL;
}

void start_dapp_ctrl_ack_relay(void)
{
  e3_pending_ctrl_init();

  pthread_t thrd;
  const int rc = pthread_create(&thrd, NULL, dapp_ctrl_ack_relay, NULL);
  AssertFatal(rc == 0, "failed to start the dApp control-acknowledge relay: %d\n", rc);
  pthread_setname_np(thrd, "dapp_ack_relay");
  pthread_detach(thrd);
}
