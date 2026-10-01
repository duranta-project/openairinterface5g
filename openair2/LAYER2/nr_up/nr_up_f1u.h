/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_F1U_H
#define NR_UP_F1U_H

#include "nr_up.h"

void nr_up_init_f1u(nr_up_if_t *iface);
void nr_up_f1u_ddds_indication(ue_id_t ue_id, rb_id_t drb_id, uint32_t dbs);

#endif /* NR_UP_F1U_H */
