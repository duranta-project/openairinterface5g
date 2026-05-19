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
