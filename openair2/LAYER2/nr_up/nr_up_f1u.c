/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up/nr_up_f1u.h"

#include <time.h>

#include <openair3/ocp-gtpu/gtp_itf.h>

#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "openair2/F1AP/f1ap_common.h"
#include "nr_up/nr_up_backend_if.h"

/* Align with DU GTPU_DDDS_TIMER_MS: max Report Polling rate while sending or stalled */
#define NR_UP_BUDGET_POLL_MS 20u

/** @brief Try a Report Polling slot if sync is stale and poll interval elapsed */
static bool nr_up_f1u_try_report_polling(ue_id_t ue_id, rb_id_t drb_id)
{
  nr_up_drb_budget_t *drb_budget;
  struct timespec now;

  clock_gettime(CLOCK_MONOTONIC, &now);
  nr_up_manager_lock();
  drb_budget = nr_up_manager_lookup_drb(ue_id, drb_id);
  if (drb_budget == NULL) { // first time we see this DRB
    drb_budget = nr_up_manager_insert_drb(ue_id, drb_id);
    drb_budget->available_tx_bytes = UINT32_MAX; /* ALLOW until real DDDS */
    drb_budget->last_status_poll = now;
    nr_up_manager_unlock();
    return true;
  }

  /* try polling if budget is stale and poll interval elapsed */
  if (nr_up_timespec_diff_ms(&now, &drb_budget->last_budget_sync) >= NR_UP_BUDGET_POLL_MS
      && nr_up_timespec_diff_ms(&now, &drb_budget->last_status_poll) >= NR_UP_BUDGET_POLL_MS) {
    drb_budget->last_status_poll = now;
    nr_up_manager_unlock();
    return true;
  }

  nr_up_manager_unlock();
  return false;
}

/** @brief Send a DL DRB PDU on F1-U via GTP-U with NR-U sequence number */
static nr_up_dl_transfer_response_t nr_up_f1u_deliver_drb(const nr_up_dl_transfer_req_t *req)
{
  const f1ap_cudu_inst_t *inst = getCxt(0);
  DevAssert(req);
  DevAssert(inst);
  const bool report_polling = nr_up_f1u_try_report_polling(req->ue_id, req->drb_id);
  LOG_D(NR_UP,
        "%s(): (drb %u) sending message to gtp size %zu pdcp_sn %u report_polling %d\n",
        __func__,
        req->drb_id,
        req->pdu.len,
        req->pdcp_sn,
        report_polling);
  gtpv1uSendDirectWithNRUSeqNum(inst->gtpInst, req->ue_id, req->drb_id, req->pdu.buf, req->pdu.len, req->pdcp_sn, report_polling);
  nr_up_drb_budget_consume(req->ue_id, req->drb_id, req->pdu.len);
  return NR_UP_DL_OK;
}

/** @brief Binds the F1-U nr-up backend on iface */
void nr_up_init_f1u(nr_up_if_t *iface)
{
  DevAssert(iface);
  nr_up_manager_init();
  iface->deliver_drb = nr_up_f1u_deliver_drb;
  iface->dl_congestion_precheck = nr_up_drb_budget_precheck;
  iface->budget_sync = nr_up_drb_budget_sync;
}
