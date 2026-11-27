/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up_cu.h"

#include <openair3/ocp-gtpu/gtp_itf.h>

#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "openair2/F1AP/f1ap_common.h"
#include "openair2/LAYER2/nr_pdcp/nr_pdcp_oai_api.h"
#include "nr_up.h"

/** @brief Send a DL DRB PDU on F1-U via GTP-U with NR-U sequence number */
static void nr_up_f1u_dl_user_data(const nr_up_transfer_user_data_req_t *req)
{
  const f1ap_cudu_inst_t *inst = getCxt(0);
  DevAssert(req);
  DevAssert(inst);
  LOG_D(NR_UP,
        "%s(): (drb %u) sending message to gtp size %zu, report_polling %d, pdcp_sn %u\n",
        __func__,
        req->drb_id,
        req->pdu.len,
        req->report_polling,
        req->pdcp_sn);
  gtpv1uSendDirectWithNRUSeqNum(inst->gtpInst, req->ue_id, req->drb_id, req->pdu.buf, req->pdu.len, req->report_polling, req->pdcp_sn);
}

/** @brief GTP DDDS RX adapter: apply Desired Buffer Size into the CU budget */
void nr_up_f1u_ddds_indication(ue_id_t ue_id, rb_id_t drb_id, uint32_t dbs)
{
  LOG_D(NR_UP, "Sync DBS pdcp_ue_id=%ld DRB=%ld dbs=%d\n", ue_id, drb_id, dbs);
  nr_up_sync_dbs(ue_id, drb_id, dbs);
}

/** @brief F1-U UL USER DATA RX on CU: sync PDCP on the GTP buffer */
bool nr_up_f1u_ul_user_data_recv(protocol_ctxt_t *ctxt_pP,
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
  UNUSED(muiP);
  UNUSED(confirmP);
  UNUSED(modeP);
  UNUSED(sourceL2Id);
  UNUSED(destinationL2Id);
  UNUSED(is_poll);

  do_pdcp_data_ind(ctxt_pP, srb_flagP, rb_idP, sdu_buffer_sizeP, sdu_buffer_pP);
  return true;
}

/** @brief Binds the F1-U nr-up backend on iface (CU side) */
void nr_up_init_cu(nr_up_if_t *iface)
{
  DevAssert(iface);
  nr_up_manager_init();
  iface->transfer_user_data = nr_up_f1u_dl_user_data;
}
