/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up_pdcp_if.h"
#include "assertions.h"

/** @brief Deliver a DL DRB PDU through the active nr-up backend */
void nr_up_user_data(const nr_up_user_data_req_t *req)
{
  DevAssert(req);
  nr_up_if_t *iface = get_nr_up_if();
  DevAssert(iface);
  DevAssert(iface->up_user_data);
  iface->up_user_data(req);
}
