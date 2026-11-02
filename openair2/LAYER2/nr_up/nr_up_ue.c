/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up_ue.h"

#include <string.h>

#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "common/utils/utils.h"
#include "nr_up_backend_if.h"
#include "nr_up_rlc_queue.h"

/* Allow stale budget if the RLC worker has not refreshed this DRB budget for too long.
 * Without this, DROP with an empty queue never syncs again and can stall UL. */
#define NR_UP_UE_BUDGET_STALE_MS 20u

/** @brief Enqueue a PDCP RB PDU toward RLC on the UE path
 * Copies the PDU into a memblock and enqueues to the RLC queue */
static nr_up_dl_transfer_response_t nr_up_ue_deliver_rb(const nr_up_dl_transfer_req_t *req, srb_flag_t srb_flag)
{
  DevAssert(req);
  protocol_ctxt_t ctxt = {.enb_flag = 0, .rntiMaybeUEid = req->ue_id};
  uint8_t *memblock = malloc16(req->pdu.len);
  if (memblock == NULL) {
    LOG_E(NR_UP, "%s(): malloc16 failed size %zu\n", __func__, req->pdu.len);
    return NR_UP_DL_ERROR;
  }

  memcpy(memblock, req->pdu.buf, req->pdu.len);
  LOG_D(NR_UP,
        "%s(): (%s %u) calling rlc_data_req size %zu\n",
        __func__,
        srb_flag ? "srb" : "drb",
        req->drb_id,
        req->pdu.len);
  nr_up_enqueue_rlc_data_req(&ctxt, srb_flag, req->drb_id, req->sdu_id, req->pdu.len, memblock, req->ue_id);
  return NR_UP_DL_OK;
}

static nr_up_dl_transfer_response_t nr_up_ue_deliver_drb(const nr_up_dl_transfer_req_t *req)
{
  const nr_up_dl_transfer_response_t rc = nr_up_ue_deliver_rb(req, SRB_FLAG_NO);
  if (rc == NR_UP_DL_OK) {
    nr_up_drb_budget_consume(req->ue_id, req->drb_id, req->pdu.len);
  }
  return rc;
}

static nr_up_dl_transfer_response_t nr_up_ue_deliver_srb(const nr_up_dl_transfer_req_t *req)
{
  return nr_up_ue_deliver_rb(req, SRB_FLAG_YES);
}

/** @brief UE UL congestion precheck with stale-cache ALLOW
 * if last budget sync is older than threshold */
static nr_up_congestion_action_t nr_up_ue_ul_congestion_precheck(ue_id_t ue_id, rb_id_t drb_id, size_t pdu_len)
{
  nr_up_drb_budget_t *drb_budget;
  uint64_t ms_since_sync;

  nr_up_manager_lock();
  drb_budget = nr_up_manager_lookup_drb(ue_id, drb_id);
  if (drb_budget != NULL) {
    ms_since_sync = nr_up_current_time_ms() - drb_budget->last_budget_sync;
    /* RLC has not refreshed this budget for too long: allow the packet.
     * The old occupancy value may be wrong, and keeping DROP would stop all UL. */
    if (ms_since_sync > NR_UP_UE_BUDGET_STALE_MS) {
      nr_up_manager_unlock();
      return NR_UP_CONGESTION_ALLOW;
    }
  }
  nr_up_manager_unlock();
  return nr_up_drb_budget_precheck(ue_id, drb_id, pdu_len);
}

/** @brief Starts the RLC enqueue worker and binds the UE nr-up backend on iface */
void nr_up_init_ue(nr_up_if_t *iface)
{
  DevAssert(iface);
  nr_up_manager_init();
  nr_up_rlc_queue_init();
  iface->deliver_drb = nr_up_ue_deliver_drb;
  iface->deliver_srb = nr_up_ue_deliver_srb;
  iface->dl_congestion_precheck = nr_up_ue_ul_congestion_precheck;
  iface->budget_sync = nr_up_drb_budget_sync;
}
