/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_RLC_QUEUE_H
#define NR_UP_RLC_QUEUE_H

#include "common/platform_types.h"

void nr_up_rlc_queue_init(void);

void nr_up_enqueue_rlc_data_req(const protocol_ctxt_t *ctxt_pP,
                                srb_flag_t srb_flagP,
                                rb_id_t rb_idP,
                                int sdu_id,
                                sdu_size_t sdu_sizeP,
                                uint8_t *sdu_pP);

#endif /* NR_UP_RLC_QUEUE_H */
