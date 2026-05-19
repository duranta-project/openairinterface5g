/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up_pdcp_if.h"
#include "assertions.h"

/** @brief Deliver a DL DRB PDU (or poll-only empty) through the active nr-up backend.
 * Consumes DBS once after the backend send when a PDU was delivered. */
void nr_up_user_data(const nr_up_user_data_req_t *req)
{
  DevAssert(req);
  nr_up_user_data_req_t local = *req;
  local.report_polling = nr_up_report_polling(local.ue_id, local.drb_id);
  if ((local.pdu.buf == NULL || local.pdu.len == 0) && !local.report_polling)
    return;

  nr_up_if_t *iface = get_nr_up_if();
  DevAssert(iface);
  DevAssert(iface->up_user_data);
  iface->up_user_data(&local);
  if (local.pdu.buf != NULL && local.pdu.len > 0)
    nr_up_consume_dbs(local.ue_id, local.drb_id, local.pdu.len);
}
