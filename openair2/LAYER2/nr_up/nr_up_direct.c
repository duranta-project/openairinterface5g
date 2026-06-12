/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <string.h>
#include <time.h>
#include "nr_up/nr_up_direct.h"
#include "nr_up/nr_up_backend_if.h"
#include "nr_up/nr_up_rlc_queue.h"
#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "common/utils/utils.h"
#include "openair2/F1AP/f1ap_ids.h"

/* Allow stale budget if the RLC worker has not refreshed this DRB budget for too long.
 * Without this, DROP with an empty queue never syncs again and can stall DL. */
#define NR_UP_MONO_BUDGET_STALE_MS 20u

/** @brief Enqueue a DL DRB PDU toward RLC on the monolithic gNB path
 * Copies the PDU into a memblock and enqueues to the RLC queue */
static nr_up_dl_transfer_response_t nr_up_mono_deliver_drb(const nr_up_dl_transfer_req_t *req)
{
  DevAssert(req);
  f1_ue_data_t ue_data = cu_get_f1_ue_data(req->ue_id);
  protocol_ctxt_t ctxt = {.enb_flag = 1, .rntiMaybeUEid = ue_data.secondary_ue};
  uint8_t *memblock = malloc16(req->pdu.len);
  if (memblock == NULL) {
    LOG_E(NR_UP, "%s(): malloc16 failed size %zu\n", __func__, req->pdu.len);
    return NR_UP_DL_ERROR;
  }

  memcpy(memblock, req->pdu.buf, req->pdu.len);
  LOG_D(NR_UP, "%s(): (drb %u) calling rlc_data_req size %zu\n", __func__, req->drb_id, req->pdu.len);
  nr_up_enqueue_rlc_data_req(&ctxt, SRB_FLAG_NO, req->drb_id, req->sdu_id, req->pdu.len, memblock);
  nr_up_drb_budget_consume(req->ue_id, req->drb_id, req->pdu.len);
  return NR_UP_DL_OK;
}

/** @brief Mono congestion precheck with stale-cache ALLOW if last budget sync
 * is older than threshold */
static nr_up_congestion_action_t nr_up_mono_dl_congestion_precheck(ue_id_t ue_id, rb_id_t drb_id, size_t pdu_len)
{
  nr_up_drb_budget_t *drb_budget;
  struct timespec now;
  size_t ms_since_sync;

  nr_up_manager_lock();
  drb_budget = nr_up_manager_lookup_drb(ue_id, drb_id);
  if (drb_budget != NULL) {
    clock_gettime(CLOCK_MONOTONIC, &now);
    ms_since_sync = nr_up_timespec_diff_ms(&now, &drb_budget->last_budget_sync);
    /* RLC has not refreshed this budget for too long: allow the packet.
     * The old occupancy value may be wrong, and keeping DROP would stop all DL. */
    if (ms_since_sync > NR_UP_MONO_BUDGET_STALE_MS) {
      nr_up_manager_unlock();
      return NR_UP_CONGESTION_ALLOW;
    }
  }
  nr_up_manager_unlock();
  return nr_up_drb_budget_precheck(ue_id, drb_id, pdu_len);
}

/** @brief Remap RLC ue_id to CU ue_id, then sync the DRB budget store */
static void nr_up_mono_budget_sync(ue_id_t rlc_ue_id, rb_id_t drb_id, uint32_t tx_space)
{
  f1_ue_data_t ue_data = du_get_f1_ue_data(rlc_ue_id);
  nr_up_drb_budget_sync(ue_data.secondary_ue, drb_id, tx_space);
}

/** @brief Starts the RLC enqueue worker and binds the mono nr-up backend on iface */
void nr_up_init_direct(nr_up_if_t *iface)
{
  nr_up_manager_init();
  nr_up_rlc_queue_init();
  iface->deliver_drb = nr_up_mono_deliver_drb;
  iface->dl_congestion_precheck = nr_up_mono_dl_congestion_precheck;
  iface->budget_sync = nr_up_mono_budget_sync;
}
