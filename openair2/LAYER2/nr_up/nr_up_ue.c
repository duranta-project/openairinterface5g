/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up/nr_up_ue.h"

#include <string.h>

#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "common/utils/utils.h"
#include "nr_up/nr_up_rlc_queue.h"

/** @brief Enqueue a DL RB PDU toward RLC on the UE path
 * Copies the PDU into a memblock and enqueues to the RLC queue */
static nr_up_dl_transfer_response_t nr_up_ue_deliver_rb(const nr_up_dl_transfer_req_t *req, srb_flag_t srb_flag)
{
  DevAssert(req);
  protocol_ctxt_t ctxt = {.enb_flag = 0, .rntiMaybeUEid = req->ue_id};
  uint8_t *memblock = malloc16(req->pdu.len);
  if (memblock == NULL) {
    LOG_E(NR_UP, "%s(): malloc16 failed size %zu\n", __func__, req->pdu.len);
    return NR_UP_DL_ERROR;
  }

  memcpy(memblock, req->pdu.buf, req->pdu.len);
  LOG_D(NR_UP,
        "%s(): (%s %u) calling rlc_data_req size %zu\n",
        __func__,
        srb_flag ? "srb" : "drb",
        req->drb_id,
        req->pdu.len);
  nr_up_enqueue_rlc_data_req(&ctxt, srb_flag, req->drb_id, req->sdu_id, req->pdu.len, memblock);
  return NR_UP_DL_OK;
}

static nr_up_dl_transfer_response_t nr_up_ue_deliver_drb(const nr_up_dl_transfer_req_t *req)
{
  return nr_up_ue_deliver_rb(req, SRB_FLAG_NO);
}

static nr_up_dl_transfer_response_t nr_up_ue_deliver_srb(const nr_up_dl_transfer_req_t *req)
{
  return nr_up_ue_deliver_rb(req, SRB_FLAG_YES);
}

/** @brief Starts the RLC enqueue worker and binds the UE nr-up backend on iface */
void nr_up_init_ue(nr_up_if_t *iface)
{
  DevAssert(iface);
  nr_up_rlc_queue_init();
  iface->deliver_drb = nr_up_ue_deliver_drb;
  iface->deliver_srb = nr_up_ue_deliver_srb;
}
