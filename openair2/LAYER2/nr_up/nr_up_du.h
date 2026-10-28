/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_DU_H
#define NR_UP_DU_H

#include <stdint.h>

#include "common/platform_types.h"

typedef int (*nr_up_du_ddds_tx_fn_t)(ue_id_t ue_id, rb_id_t rb_id, uint32_t desired_buffer_size);

void nr_up_init_du(nr_up_du_ddds_tx_fn_t send_ddds);
void nr_up_du_add_drb(ue_id_t ue_id, rb_id_t drb_id);
void nr_up_du_update_ue_id(ue_id_t old_ue_id, ue_id_t new_ue_id);
uint32_t nr_up_du_poll_ddds(ue_id_t ue_id, rb_id_t drb_id);
void nr_up_du_tx_space_sync(ue_id_t ue_id, rb_id_t drb_id, uint32_t space);
void nr_up_du_release_drb(ue_id_t ue_id, rb_id_t drb_id);
void nr_up_du_release_ue(ue_id_t ue_id);

#endif /* NR_UP_DU_H */
