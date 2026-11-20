/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_CU_H
#define NR_UP_CU_H

#include "nr_up.h"

void nr_up_init_cu(nr_up_if_t *iface);
void nr_up_f1u_ddds_indication(ue_id_t ue_id, rb_id_t drb_id, uint32_t dbs);
bool nr_up_f1u_ul_user_data_recv(protocol_ctxt_t *ctxt_pP,
                                 const srb_flag_t srb_flagP,
                                 const rb_id_t rb_idP,
                                 const mui_t muiP,
                                 const confirm_t confirmP,
                                 const sdu_size_t sdu_buffer_sizeP,
                                 unsigned char *const sdu_buffer_pP,
                                 const pdcp_transmission_mode_t modeP,
                                 const uint32_t *sourceL2Id,
                                 const uint32_t *destinationL2Id,
                                 bool is_poll);

#endif /* NR_UP_CU_H */
