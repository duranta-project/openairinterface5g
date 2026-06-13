/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_PDCP_IF_H
#define NR_UP_PDCP_IF_H

#include "nr_up/nr_up.h"

nr_up_dl_transfer_result_t nr_up_dl_transfer(const nr_up_dl_transfer_req_t *req);

nr_up_congestion_action_t nr_up_dl_congestion_precheck(ue_id_t ue_id, rb_id_t rb_id, size_t pdu_len);

#endif /* NR_UP_PDCP_IF_H */
