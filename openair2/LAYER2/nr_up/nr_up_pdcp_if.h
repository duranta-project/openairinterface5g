/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_PDCP_IF_H
#define NR_UP_PDCP_IF_H

#include "nr_up.h"

nr_up_dl_transfer_response_t nr_up_dl_transfer(const nr_up_dl_transfer_req_t *req);
nr_up_dl_transfer_response_t nr_up_srb_transfer(const nr_up_dl_transfer_req_t *req);

nr_up_congestion_action_t nr_up_dl_congestion_precheck(ue_id_t ue_id, rb_id_t rb_id, size_t pdu_len);
void nr_up_dl_budget_sync(ue_id_t ue_id, rb_id_t drb_id, uint32_t tx_space);
/** @brief PDCP discardTimer: discard the DRB PDUs up to sdu_id that the lower layers did not transmit yet */
void nr_up_drb_discard(ue_id_t ue_id, rb_id_t drb_id, int sdu_id);

/** @brief Remove the per-DRB TX budget cache entry for ue_id and drb_id */
void nr_up_release_drb(ue_id_t ue_id, rb_id_t drb_id);

#endif /* NR_UP_PDCP_IF_H */
