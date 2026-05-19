/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_DIRECT_H
#define NR_UP_DIRECT_H

#include "nr_up.h"

/** @brief Bind local RLC UP backend (mono gNB or UE, selected at compile time). */
void nr_up_init_direct(nr_up_if_t *iface);

#endif /* NR_UP_DIRECT_H */
