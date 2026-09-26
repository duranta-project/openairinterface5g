/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Detection of collisions between blocked PRBs (radar avoidance,
 *        spectrum sharing) and UE-specific, RRC-configured periodic signals
 *        (PUCCH/SRS/CSI-RS), read from the periodic allocation registry.
 *
 * The registry is queried in BOTH directions, each tied to a distinct event:
 *
 *   - Event A: a (new) PRB-blocking policy is applied. We scan the existing
 *     allocations and report those the block now lands on.
 *     Entry: blocked_prbs_check_on_policy_update().
 *     Handlers: on_<signal>_hit_by_block().
 *
 *   - Event B: a UE's UE-specific signals are (re)configured (attach /
 *     reconfiguration / BWP switch). We check that UE's allocations against the
 *     currently-active block and report those placed on a blocked PRB.
 *     Entry: blocked_prbs_check_on_ue_config().
 *     Handlers: on_<signal>_placed_on_block().
 *
 * Each (signal, event) has its own handler (currently logging only) so the two
 * cases can later trigger different reactions.
 *
 * Cell-wide signals (SSB, CORESET0, PRACH) are out of scope here.
 */

#ifndef BLOCKED_PRBS_COLLISION_HANDLER_H
#define BLOCKED_PRBS_COLLISION_HANDLER_H

#include <stdint.h>
#include <string.h> // memset() used by linear_alloc.h below
#include "common/utils/collection/linear_alloc.h" // uid_t

#ifdef E3_AGENT

/*! \brief Event A: a blocking policy was applied. Scans all configured
 *  UE-specific periodic signals and reports those the block hits. Each entry is
 *  checked against the mask for its own direction (UL signals vs the UL block,
 *  DL signals vs the DL block).
 *  \param blocked_dl DL block, MAX_BWP_SIZE entries (NULL = no DL block).
 *  \param blocked_ul UL block, MAX_BWP_SIZE entries (NULL = no UL block).
 *  \return number of collisions reported. */
int blocked_prbs_check_on_policy_update(const uint16_t *blocked_dl, const uint16_t *blocked_ul);

/*! \brief Event B: a UE was (re)configured. Checks that UE's UE-specific
 *  periodic signals against the active block (per direction) and reports those
 *  placed on a blocked PRB. Silent when there is no collision.
 *  \param uid        owner UE (UE->uid).
 *  \param blocked_dl DL block, MAX_BWP_SIZE entries (NULL = no DL block).
 *  \param blocked_ul UL block, MAX_BWP_SIZE entries (NULL = no UL block).
 *  \return number of collisions reported. */
int blocked_prbs_check_on_ue_config(uid_t uid, const uint16_t *blocked_dl, const uint16_t *blocked_ul);

#endif /* E3_AGENT */

#endif // BLOCKED_PRBS_COLLISION_HANDLER_H
