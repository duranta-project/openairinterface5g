/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_RLC_QUEUE_H
#define NR_UP_RLC_QUEUE_H

#include "common/platform_types.h"
#include "nr_up/nr_up.h"

void nr_up_rlc_queue_init(void);

typedef void (*nr_up_budget_refresh_fn_t)(ue_id_t cu_ue_id, rb_id_t drb_id, int tx_space);
void nr_up_rlc_queue_set_budget_refresh_cb(nr_up_budget_refresh_fn_t cb);

void nr_up_enqueue_rlc_data_req(const protocol_ctxt_t *ctxt_pP,
                                srb_flag_t srb_flagP,
                                rb_id_t rb_idP,
                                int sdu_id,
                                sdu_size_t sdu_sizeP,
                                uint8_t *sdu_pP,
                                ue_id_t cu_ue_id);

#endif /* NR_UP_RLC_QUEUE_H */
