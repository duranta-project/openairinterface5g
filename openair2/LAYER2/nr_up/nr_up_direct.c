/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <string.h>
#include "nr_up_direct.h"
#include "assertions.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_data_req_queue.h"
#include "nr_up.h"
#include "nr_up_pdcp_if.h"
#include "common/utils/LOG/log.h"
#include "common/utils/utils.h"
#ifdef PDCP_CUCP_CUUP
#include "openair2/F1AP/f1ap_ids.h"
#endif

/** @brief Enqueue a DRB PDU toward local RLC (mono gNB or UE).
 * NULL PDU with report_polling: enqueue a DBS refresh only. */
static void nr_up_direct_dl_user_data(const nr_up_user_data_req_t *req)
{
  DevAssert(req);
#ifdef PDCP_CUCP_CUUP
  f1_ue_data_t ue_data = cu_get_f1_ue_data(req->ue_id);
  protocol_ctxt_t ctxt = {.enb_flag = 1, .rntiMaybeUEid = ue_data.secondary_ue};
#else
  protocol_ctxt_t ctxt = {.enb_flag = 0, .rntiMaybeUEid = req->ue_id};
#endif

  uint8_t *memblock = NULL;
  sdu_size_t len = req->pdu.len;

  if (req->pdu.buf != NULL && len > 0) {
    memblock = malloc16(len);
    AssertFatal(memblock != NULL, "malloc16 failed size %zu", req->pdu.len);
    memcpy(memblock, req->pdu.buf, len);
  } else {
    DevAssert(req->report_polling);
    len = 0;
  }

  LOG_D(NR_UP, "%s(): (drb %u) calling rlc_data_req size %d\n", __func__, req->drb_id, len);
  nr_rlc_enqueue_data_req(&ctxt, SRB_FLAG_NO, req->drb_id, req->sdu_id, len, memblock, req->ue_id, req->report_polling);
  if (req->pdu.buf != NULL && len > 0)
    nr_up_consume_dbs(req->ue_id, req->drb_id, len);
}

/** @brief Starts the RLC enqueue worker and binds the local nr-up backend on iface */
void nr_up_init_direct(nr_up_if_t *iface)
{
  DevAssert(iface);
  nr_up_manager_init();
  iface->up_user_data = nr_up_direct_dl_user_data;
  iface->up_ddds_indication = nr_up_sync_dbs;
}
