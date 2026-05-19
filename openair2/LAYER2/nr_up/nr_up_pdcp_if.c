/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up/nr_up_pdcp_if.h"
#include "assertions.h"

nr_up_dl_transfer_result_t nr_up_dl_transfer(const nr_up_dl_transfer_req_t *req)
{
  DevAssert(req);
  nr_up_if_t *iface = get_nr_up_if();
  DevAssert(iface);
  return iface->deliver_drb(req);
}
