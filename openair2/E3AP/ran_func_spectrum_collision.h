/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef RAN_FUNC_SPECTRUM_COLLISION_H
#define RAN_FUNC_SPECTRUM_COLLISION_H

#include <stdint.h>
#include <string.h> // memset() used by linear_alloc.h below
#include "common/utils/collection/linear_alloc.h" // uid_t

/* A block was installed: log every registered signal it hits. UL signals are
 * checked against blocked_ul, DL against blocked_dl (each MAX_BWP_SIZE entries,
 * NULL = no block). Returns the number of collisions. */
int blocked_prbs_check_on_policy_update(const uint16_t *blocked_dl, const uint16_t *blocked_ul);

/* A UE was (re)configured: same check for that UE's signals only. */
int blocked_prbs_check_on_ue_config(uid_t uid, const uint16_t *blocked_dl, const uint16_t *blocked_ul);

#endif // RAN_FUNC_SPECTRUM_COLLISION_H
