/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_RLC_DATA_REQ_QUEUE_H
#define NR_RLC_DATA_REQ_QUEUE_H

#include "common/platform_types.h"

void nr_rlc_data_req_queue_init(void);

void nr_rlc_enqueue_data_req(const protocol_ctxt_t *ctxt_pP,
                             srb_flag_t srb_flagP,
                             rb_id_t rb_idP,
                             int sdu_id,
                             sdu_size_t sdu_sizeP,
                             uint8_t *sdu_pP,
                             ue_id_t pdcp_ue_id,
                             bool report_polling);

#endif /* NR_RLC_DATA_REQ_QUEUE_H */
