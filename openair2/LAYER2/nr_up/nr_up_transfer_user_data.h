/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_TRANSFER_USER_DATA_H
#define NR_UP_TRANSFER_USER_DATA_H

#include "nr_up.h"

nr_up_transfer_user_data_action_t nr_up_transfer_user_data_precheck(ue_id_t ue_id, rb_id_t drb_id, size_t pdu_max_size);
void nr_up_deliver_pdu_drb(nr_up_transfer_user_data_req_t *req);
void nr_up_transfer_user_data(const nr_up_transfer_user_data_req_t *req);

#endif /* NR_UP_TRANSFER_USER_DATA_H */
