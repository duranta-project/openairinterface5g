/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up/nr_up_f1u.h"

#include <openair3/ocp-gtpu/gtp_itf.h>

#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "openair2/F1AP/f1ap_common.h"

static nr_up_dl_transfer_result_t nr_up_f1u_deliver_drb(const nr_up_dl_transfer_req_t *req)
{
  const f1ap_cudu_inst_t *inst = getCxt(0);
  DevAssert(req);
  DevAssert(inst);
  LOG_D(NR_UP, "%s(): (drb %u) sending message to gtp size %zu\n", __func__, req->drb_id, req->pdu.len);
  gtpv1uSendDirectWithNRUSeqNum(inst->gtpInst, req->ue_id, req->drb_id, req->pdu.buf, req->pdu.len);
  return NR_UP_DL_OK;
}

void nr_up_init_f1u(nr_up_if_t *iface)
{
  DevAssert(iface);
  iface->deliver_drb = nr_up_f1u_deliver_drb;
}
