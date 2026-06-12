/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <string.h>
#include "nr_up/nr_up_backend_if.h"
#include "nr_up/nr_up_rlc_queue.h"
#include "common/utils/LOG/log.h"
#include "common/utils/utils.h"
#include "openair2/F1AP/f1ap_ids.h"

#define NR_UP_MONO_SRB_FLAG 0

static nr_up_dl_transfer_result_t nr_up_mono_deliver_drb(const nr_up_dl_transfer_req_t *req)
{
  f1_ue_data_t ue_data = cu_get_f1_ue_data(req->ue_id);
  protocol_ctxt_t ctxt = {.enb_flag = 1, .rntiMaybeUEid = ue_data.secondary_ue};
  uint8_t *memblock = malloc16(req->pdu.len);
  if (memblock == NULL) {
    LOG_E(NR_UP, "%s(): malloc16 failed size %zu\n", __func__, req->pdu.len);
    return NR_UP_DL_ERROR;
  }

  memcpy(memblock, req->pdu.buf, req->pdu.len);
  LOG_D(NR_UP, "%s(): (drb %u) calling rlc_data_req size %zu\n", __func__, req->drb_id, req->pdu.len);
  nr_up_enqueue_rlc_data_req(&ctxt, NR_UP_MONO_SRB_FLAG, req->drb_id, req->sdu_id, req->pdu.len, memblock);
  return NR_UP_DL_OK;
}

void nr_up_init_monolithic(nr_up_if_t *iface)
{
  nr_up_rlc_queue_init();
  iface->deliver_drb = nr_up_mono_deliver_drb;
}
