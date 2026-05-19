/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up_f1u.h"

#include <openair3/ocp-gtpu/gtp_itf.h>

#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "openair2/F1AP/f1ap_common.h"

/** @brief Send a DL DRB PDU on F1-U via GTP-U with NR-U sequence number */
static void nr_up_f1u_dl_user_data(const nr_up_user_data_req_t *req)
{
  const f1ap_cudu_inst_t *inst = getCxt(0);
  DevAssert(req);
  DevAssert(inst);
  LOG_D(NR_UP, "%s(): (drb %u) sending message to gtp size %zu\n", __func__, req->drb_id, req->pdu.len);
  gtpv1uSendDirectWithNRUSeqNum(inst->gtpInst, req->ue_id, req->drb_id, req->pdu.buf, req->pdu.len);
}

/** @brief Binds the F1-U nr-up backend on iface */
void nr_up_init_f1u(nr_up_if_t *iface)
{
  DevAssert(iface);
  iface->up_user_data = nr_up_f1u_dl_user_data;
}
