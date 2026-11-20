/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_DU_H
#define NR_UP_DU_H

#include <stdbool.h>
#include <stdint.h>

#include "common/platform_types.h"
#include "nr_up.h"

void nr_up_init_du(nr_up_if_t *iface);

bool nr_up_f1u_dl_user_data_recv(protocol_ctxt_t *ctxt_pP,
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

#endif /* NR_UP_DU_H */
