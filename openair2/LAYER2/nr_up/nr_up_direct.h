/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_DIRECT_H
#define NR_UP_DIRECT_H

#include "nr_up.h"

/** @brief Bind local RLC UP backend (mono gNB or UE, selected at compile time). */
void nr_up_init_direct(nr_up_if_t *iface);
void nr_up_direct_ul_user_data(protocol_ctxt_t *ctxt, ue_id_t rlc_ue_id, rb_id_t drb_id, uint8_t *buf, size_t len, int tx_space);

#endif /* NR_UP_DIRECT_H */
