/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_DDDS_POLICY_H
#define NR_UP_DDDS_POLICY_H

#include <stdbool.h>
#include <stdint.h>
#include "nr_up.h"

bool nr_up_ddds_precheck(nr_up_ddds_t *st, uint32_t space, bool is_poll);

#endif /* NR_UP_DDDS_POLICY_H */
