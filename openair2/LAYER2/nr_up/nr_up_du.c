/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up_du.h"

#include <stdlib.h>

#include <openair3/ocp-gtpu/gtp_itf.h>

#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "common/utils/utils.h"
#include "openair2/F1AP/f1ap_common.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h"
#include "nr_up_ddds_policy.h"

static nr_up_if_t *g_nr_up_du_iface;

/** @brief F1-U UL: send G-PDU, piggyback DDDS when policy requires (TS 38.425 clause 5.4.2.1) */
void nr_up_f1u_ul_user_data(protocol_ctxt_t *ctxt, ue_id_t rlc_ue_id, rb_id_t drb_id, uint8_t *buf, size_t len, int tx_space)
{
  UNUSED(ctxt);
  uint32_t space = 0;
  if (tx_space > 0)
    space = tx_space;
  nr_up_ddds_t ddds = nr_up_get_ddds(rlc_ue_id, drb_id);
  const bool report = nr_up_ddds_precheck(&ddds, space, false);
  if (report)
    nr_up_set_ddds(rlc_ue_id, drb_id, ddds);
  DevAssert(g_nr_up_du_iface != NULL && g_nr_up_du_iface->up_send_ddds != NULL);
  const nr_up_ddds_req_t req = {
      .ue_id = rlc_ue_id,
      .drb_id = drb_id,
      .desired_buffer_size = space,
      .report_ddds = report,
      .pdu = {.len = len, .buf = buf},
  };
  g_nr_up_du_iface->up_send_ddds(&req);
}

/** @brief F1-U TX: G-PDU and/or DDDS (piggyback, TS 38.425 clause 5.4.2.1) */
void nr_up_f1u_ddds(const nr_up_ddds_req_t *req)
{
  DevAssert(req);
  const f1ap_cudu_inst_t *inst = getCxt(0);
  if (inst == NULL || inst->gtpInst < 0) {
    LOG_W(NR_UP, "%s(): no F1-U GTP instance for ue %lx drb %ld\n", __func__, req->ue_id, req->drb_id);
    return;
  }
  gtpv1uSendDirect(inst->gtpInst,
                   req->ue_id,
                   req->drb_id,
                   req->pdu.buf,
                   req->pdu.len,
                   false,
                   false,
                   req->report_ddds,
                   req->desired_buffer_size);
}

void nr_up_init_du(nr_up_if_t *iface)
{
  DevAssert(iface != NULL);
  nr_up_manager_init();
  g_nr_up_du_iface = iface;
  iface->up_send_ddds = nr_up_f1u_ddds;
  iface->ul_user_data = nr_up_f1u_ul_user_data;
}

/** @brief F1-U DL USER DATA RX (former DURecvCb): deliver to RLC and maybe send DDDS */
bool nr_up_f1u_dl_user_data_recv(protocol_ctxt_t *ctxt_pP,
                                 const srb_flag_t srb_flagP,
                                 const rb_id_t rb_idP,
                                 const mui_t muiP,
                                 const confirm_t confirmP,
                                 const sdu_size_t sdu_buffer_sizeP,
                                 unsigned char *const sdu_buffer_pP,
                                 const pdcp_transmission_mode_t modeP,
                                 const uint32_t *sourceL2Id,
                                 const uint32_t *destinationL2Id,
                                 bool is_poll)
{
  UNUSED(confirmP);
  UNUSED(modeP);
  UNUSED(sourceL2Id);
  UNUSED(destinationL2Id);

  const ue_id_t ue_id = ctxt_pP->rntiMaybeUEid;
  uint8_t *sdu = NULL;

  if (sdu_buffer_sizeP > 0) {
    /* Buffer comes from GTP-U thread: copy before RLC enqueue */
    sdu = malloc16(sdu_buffer_sizeP);
    memcpy(sdu, sdu_buffer_pP, sdu_buffer_sizeP);
  } else {
    DevAssert(is_poll);
  }

  /* PDU: enqueue. NULL/0: DBS refresh only (same as mono RLC queue). */
  const int dbs = nr_rlc_data_req(ctxt_pP, srb_flagP, rb_idP, muiP, sdu_buffer_sizeP, sdu);
  const uint32_t space = dbs >= 0 ? (uint32_t)dbs : 0;

  nr_up_ddds_t ddds = nr_up_get_ddds(ue_id, rb_idP);
  if (nr_up_ddds_precheck(&ddds, space, is_poll)) {
    nr_up_set_ddds(ue_id, rb_idP, ddds);
    DevAssert(g_nr_up_du_iface != NULL && g_nr_up_du_iface->up_send_ddds != NULL);
    const nr_up_ddds_req_t req = {.ue_id = ue_id, .drb_id = rb_idP, .desired_buffer_size = space, .report_ddds = true};
    g_nr_up_du_iface->up_send_ddds(&req);
  }
  return true;
}
