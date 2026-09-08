/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up/nr_up_pdcp_if.h"
#include "nr_up/nr_up_backend_if.h"
#include "assertions.h"

nr_up_dl_transfer_result_t nr_up_dl_transfer(const nr_up_dl_transfer_req_t *req)
{
  DevAssert(req);
  nr_up_if_t *iface = get_nr_up_if();
  DevAssert(iface);
  return iface->deliver_drb(req);
}

nr_up_congestion_action_t nr_up_dl_congestion_precheck(ue_id_t ue_id, rb_id_t rb_id, size_t pdu_len)
{
  nr_up_if_t *iface = get_nr_up_if();
  DevAssert(iface);
  DevAssert(iface->dl_congestion_precheck);
  return iface->dl_congestion_precheck(ue_id, rb_id, pdu_len);
}

void nr_up_release_drb(ue_id_t ue_id, rb_id_t drb_id)
{
  nr_up_manager_release_drb(ue_id, drb_id);
}
