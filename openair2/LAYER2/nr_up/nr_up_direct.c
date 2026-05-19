/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <string.h>
#include "nr_up/nr_up_direct.h"
#include "nr_up/nr_up_rlc_queue.h"
#include "common/utils/LOG/log.h"
#include "common/utils/utils.h"
#include "openair2/F1AP/f1ap_ids.h"

/** @brief Enqueue a DL DRB PDU toward RLC on the monolithic gNB path
 * Copies the PDU into a memblock and enqueues to the RLC queue */
static nr_up_dl_transfer_response_t nr_up_mono_deliver_drb(const nr_up_dl_transfer_req_t *req)
{
  DevAssert(req);
  f1_ue_data_t ue_data = cu_get_f1_ue_data(req->ue_id);
  protocol_ctxt_t ctxt = {.enb_flag = 1, .rntiMaybeUEid = ue_data.secondary_ue};
  uint8_t *memblock = malloc16(req->pdu.len);
  if (memblock == NULL) {
    LOG_E(NR_UP, "%s(): malloc16 failed size %zu\n", __func__, req->pdu.len);
    return NR_UP_DL_ERROR;
  }

  memcpy(memblock, req->pdu.buf, req->pdu.len);
  LOG_D(NR_UP, "%s(): (drb %u) calling rlc_data_req size %zu\n", __func__, req->drb_id, req->pdu.len);
  nr_up_enqueue_rlc_data_req(&ctxt, SRB_FLAG_NO, req->drb_id, req->sdu_id, req->pdu.len, memblock);
  return NR_UP_DL_OK;
}

/** @brief Starts the RLC enqueue worker and binds the mono nr-up backend on iface */
void nr_up_init_direct(nr_up_if_t *iface)
{
  nr_up_rlc_queue_init();
  iface->deliver_drb = nr_up_mono_deliver_drb;
}
