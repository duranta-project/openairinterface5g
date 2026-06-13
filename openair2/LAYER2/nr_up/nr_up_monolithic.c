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
#define NR_UP_MONO_BUDGET_STALE_MS 3000u

static nr_up_congestion_action_t nr_up_mono_dl_congestion_precheck(ue_id_t ue_id, rb_id_t drb_id, size_t pdu_len)
{
  nr_up_drb_budget_t *drb_budget;
  struct timespec now;
  size_t ms_since_refresh;
  uint32_t drops_logged = 0;

  nr_up_manager_lock();
  drb_budget = nr_up_manager_lookup_drb(ue_id, drb_id);
  if (drb_budget == NULL) {
    nr_up_manager_unlock();
    return NR_UP_CONGESTION_ALLOW;
  }

  if (pdu_len <= drb_budget->available_tx_bytes) {
    nr_up_manager_unlock();
    return NR_UP_CONGESTION_ALLOW;
  }

  clock_gettime(CLOCK_MONOTONIC, &now);
  ms_since_refresh = nr_up_timespec_diff_ms(&now, &drb_budget->last_rlc_refresh);
  if (ms_since_refresh > NR_UP_MONO_BUDGET_STALE_MS) {
    nr_up_manager_unlock();
    return NR_UP_CONGESTION_ALLOW;
  }

  nr_up_drb_dl_drop_log_t *drop_log = &drb_budget->dl_drop_log;
  drop_log->drops_pending++;
  if (now.tv_sec > drop_log->last_log_sec) {
    drops_logged = drop_log->drops_pending;
    drop_log->drops_pending = 0;
    drop_log->last_log_sec = now.tv_sec;
  }
  nr_up_manager_unlock();

  if (drops_logged != 0) {
    LOG_W(NR_UP, "%u DL SDUs dropped, congestion precheck\n", drops_logged);
  }
  return NR_UP_CONGESTION_DROP;
}

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
  iface->dl_congestion_precheck = nr_up_mono_dl_congestion_precheck;
}
