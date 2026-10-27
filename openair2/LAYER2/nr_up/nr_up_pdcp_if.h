/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_PDCP_IF_H
#define NR_UP_PDCP_IF_H

#include "nr_up.h"

void nr_up_user_data(const nr_up_user_data_req_t *req);
/** @brief PDCP discardTimer: discard the DRB PDUs up to sdu_id that the lower layers did not transmit yet */
void nr_up_drb_discard(ue_id_t ue_id, rb_id_t drb_id, int sdu_id);

#endif /* NR_UP_PDCP_IF_H */
