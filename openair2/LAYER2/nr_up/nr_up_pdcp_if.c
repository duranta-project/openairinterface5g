/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up/nr_up_pdcp_if.h"
#include "assertions.h"

/** @brief Deliver a DL DRB PDU through the active nr-up backend */
nr_up_dl_transfer_response_t nr_up_dl_transfer(const nr_up_dl_transfer_req_t *req)
{
  DevAssert(req);
  nr_up_if_t *iface = get_nr_up_if();
  DevAssert(iface);
  DevAssert(iface->deliver_drb);
  return iface->deliver_drb(req);
}

/** @brief Deliver a DL SRB PDU through the active nr-up backend */
nr_up_dl_transfer_response_t nr_up_srb_transfer(const nr_up_dl_transfer_req_t *req)
{
  DevAssert(req);
  nr_up_if_t *iface = get_nr_up_if();
  DevAssert(iface);
  DevAssert(iface->deliver_srb);
  return iface->deliver_srb(req);
}

/** @brief Run DL congestion precheck through the active nr-up backend */
nr_up_congestion_action_t nr_up_dl_congestion_precheck(ue_id_t ue_id, rb_id_t rb_id, size_t pdu_len)
{
  nr_up_if_t *iface = get_nr_up_if();
  DevAssert(iface);
  DevAssert(iface->dl_congestion_precheck);
  return iface->dl_congestion_precheck(ue_id, rb_id, pdu_len);
}

/** @brief Sync the per-DRB TX budget through the active nr-up backend.
 * Does nothing when budget_sync is unset (e.g. UE). */
void nr_up_dl_budget_sync(ue_id_t ue_id, rb_id_t drb_id, uint32_t tx_space)
{
  nr_up_if_t *iface = get_nr_up_if();
  DevAssert(iface);
  if (iface->budget_sync == NULL) {
    return;
  }
  iface->budget_sync(ue_id, drb_id, tx_space);
}
