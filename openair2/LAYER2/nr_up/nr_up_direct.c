/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <string.h>
#include "nr_up_direct.h"
#include "assertions.h"
#include "openair2/LAYER2/nr_pdcp/nr_pdcp_oai_api.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_data_req_queue.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h"
#include "nr_up.h"
#include "nr_up_ddds_policy.h"
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

/** @brief Local DDDS analogue: apply Desired Buffer Size into the CU budget.
 * pdu unused (no F1-U, UL SDU is delivered to PDCP separately). */
static void nr_up_direct_ddds(const nr_up_ddds_req_t *req)
{
  DevAssert(req);
  if (!req->report_ddds)
    return;
  nr_up_if_t *iface = get_nr_up_if();
  DevAssert(iface != NULL && iface->up_ddds_indication != NULL);
  iface->up_ddds_indication(req->ue_id, req->drb_id, req->desired_buffer_size);
}

/** @brief Mono/UE UL DRB: maybe sync DBS, then deliver SDU to PDCP.
 * @param ctxt       PDCP ctxt (CU ue_id already in rntiMaybeUEid on mono gNB)
 * @param rlc_ue_id  RLC/MAC UE id
 * @param tx_space   RLC TX space, read by the caller under the RLC lock
 */
void nr_up_direct_ul_user_data(protocol_ctxt_t *ctxt, ue_id_t rlc_ue_id, rb_id_t drb_id, uint8_t *buf, size_t len, int tx_space)
{
  UNUSED(rlc_ue_id);
  DevAssert(ctxt != NULL);
  DevAssert(buf != NULL || len == 0);

  const ue_id_t pdcp_ue_id = ctxt->rntiMaybeUEid;
  uint32_t space = 0;
  if (tx_space > 0)
    space = tx_space;
  nr_up_ddds_t ddds = nr_up_get_ddds(pdcp_ue_id, drb_id);
  if (nr_up_ddds_precheck(&ddds, space, false)) {
    nr_up_set_ddds(pdcp_ue_id, drb_id, ddds);
    nr_up_if_t *iface = get_nr_up_if();
    DevAssert(iface != NULL && iface->up_send_ddds != NULL);
    const nr_up_ddds_req_t req = {.ue_id = pdcp_ue_id, .drb_id = drb_id, .desired_buffer_size = space, .report_ddds = true};
    iface->up_send_ddds(&req);
  }

  uint8_t *memblock = malloc16(len);
  if (memblock == NULL) {
    LOG_E(NR_UP, "%s(): malloc16 failed size %zu\n", __func__, len);
    return;
  }
  memcpy(memblock, buf, len);
  if (!nr_pdcp_data_ind(ctxt, SRB_FLAG_NO, drb_id, len, memblock)) {
    LOG_E(NR_UP, "%s(): nr_pdcp_data_ind failed ue %lx drb %ld\n", __func__, pdcp_ue_id, drb_id);
  }
}

/** @brief Starts the RLC enqueue worker and binds the local nr-up backend on iface */
void nr_up_init_direct(nr_up_if_t *iface)
{
  DevAssert(iface);
  nr_up_manager_init();
  iface->up_user_data = nr_up_direct_dl_user_data;
  iface->up_ddds_indication = nr_up_sync_dbs;
  iface->up_send_ddds = nr_up_direct_ddds;
  iface->ul_user_data = nr_up_direct_ul_user_data;
}
