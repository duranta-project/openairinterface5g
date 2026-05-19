/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up_transfer_user_data.h"

#include "assertions.h"
#include "common/utils/LOG/log.h"

/* Soft-drop when available bytes is below this percent of last reported TX space */
#define NR_UP_SOFT_DROP_THRESHOLD 80u
/* Aggregate DROP warnings over this managed-time window */
#define NR_UP_DROP_LOG_INTERVAL_MS 1000u

/** @brief Rate-limited log a drop */
static void nr_up_transfer_user_data_log_drop(void)
{
  static uint32_t drops_pending;
  static uint64_t last_log_ms;
  uint32_t drops_logged = 0;
  uint64_t now = nr_up_current_time_ms();

  drops_pending++;
  if (now >= last_log_ms + NR_UP_DROP_LOG_INTERVAL_MS) {
    drops_logged = drops_pending;
    drops_pending = 0;
    last_log_ms = now;
  }

  if (drops_logged != 0) {
    LOG_W(NR_UP, "%u SDUs dropped, transfer user data precheck\n", drops_logged);
  }
}

/** @brief Precheck pdu_max_size against available per-DRB TX budget.
 * On DROP, build a poll-only empty transfer so a stale DBS can refresh. */
nr_up_transfer_user_data_action_t nr_up_transfer_user_data_precheck(ue_id_t ue_id, rb_id_t drb_id, size_t pdu_max_size)
{
  nr_up_dbs_t dbs = nr_up_manager_get_dbs(ue_id, drb_id);
  const uint64_t available = dbs.available_dbs;
  const uint64_t desired = dbs.desired_buffer_size;

  /* ALLOW when:
   * - not synced yet after bearer setup
   * - last reported DBS is non-zero (desired_buffer_size == 0 means RLC reported no TX space) and
   * - consumed bytes of last reported desired buffer size is below the soft-drop threshold and
   * - pdu_max_size fits in available_dbs. */
  if (dbs.last_dbs_sync == NR_UP_DBS_NEVER_SYNCED
      || (dbs.desired_buffer_size != 0 && pdu_max_size <= dbs.available_dbs
          && available * 100u > (100u - NR_UP_SOFT_DROP_THRESHOLD) * desired)) {
    return NR_UP_TRANSFER_ALLOW;
  }

  /* DROP: RLC reported DBS 0, PDU does not fit, or soft-dropped */
  nr_up_transfer_user_data_log_drop();

  const nr_up_transfer_user_data_req_t poll = {
      .ue_id = ue_id,
      .drb_id = drb_id,
  };
  nr_up_transfer_user_data(&poll);
  return NR_UP_TRANSFER_DROP;
}

/** @brief Hands a PDCP DRB PDU to nr-up for DL delivery. */
void nr_up_deliver_pdu_drb(nr_up_transfer_user_data_req_t *req)
{
  DevAssert(req);
  DevAssert(req->pdu.buf != NULL && req->pdu.len > 0);
  nr_up_transfer_user_data(req);
}

/** @brief Deliver a DL DRB PDU (or poll-only empty) through the active nr-up backend.
 * Consumes DBS once after the backend send when a PDU was delivered. */
void nr_up_transfer_user_data(const nr_up_transfer_user_data_req_t *req)
{
  DevAssert(req);
  nr_up_transfer_user_data_req_t local = *req;
  local.report_polling = nr_up_report_polling(local.ue_id, local.drb_id);
  if ((local.pdu.buf == NULL || local.pdu.len == 0) && !local.report_polling)
    return;

  nr_up_if_t *iface = get_nr_up_if();
  DevAssert(iface);
  DevAssert(iface->transfer_user_data);
  iface->transfer_user_data(&local);
  if (local.pdu.buf != NULL && local.pdu.len > 0)
    nr_up_consume_dbs(local.ue_id, local.drb_id, local.pdu.len);
}
