/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Registry of UE-specific, RRC-configured periodic frequency-domain
 *        allocations (PUCCH, SRS, NZP-CSI-RS).
 *
 * This is the single source of truth for "which fixed UE-specific signal sits
 * on which PRBs, and for which UE". It is populated at configuration time by
 * the code that assigns the resources (so it never drifts from the scheduler),
 * and consumed by collision detection (and, later, by mitigation that relocates
 * allocations to dodge blocked PRBs).
 *
 * Lifecycle (kept coherent with UE state):
 *   - attach / reconfigure / BWP-switch -> periodic_alloc_refresh_ue() rebuilds
 *     the UE's entries from its current active BWP config (idempotent).
 *   - detach / release                  -> periodic_alloc_unregister_ue().
 *
 * Concurrency: all functions must be called with the MAC sched_lock held (same
 * invariant as UE_info access). There is no internal locking.
 *
 * Cell-wide signals (SSB, CORESET0, PRACH) are intentionally NOT tracked here;
 * they have a different config source and mitigation path.
 */

#ifndef PERIODIC_ALLOC_REGISTRY_H
#define PERIODIC_ALLOC_REGISTRY_H

#include <stdint.h>
#include <stddef.h>
#include "NR_MAC_gNB/nr_mac_gNB.h" // NR_UE_info_t, uid_t, rnti_t

typedef enum {
  PA_PUCCH = 0, // covers HARQ-ACK, SR, periodic CSI reporting (all PUCCH resources)
  PA_SRS, // periodic SRS only
  PA_CSIRS, // NZP-CSI-RS
} pa_alloc_type_t;

typedef enum { PA_DIR_DL = 0, PA_DIR_UL } pa_dir_t;

typedef struct {
  uid_t uid; // stable UE id (owner)
  rnti_t rnti; // for logging
  pa_alloc_type_t type;
  pa_dir_t dir;
  int resource_id; // PUCCH/CSI-RS resource id; SRS: first resource id of the periodic set; -1 if n/a
  // time dimension (not populated in v1; reserved for time-domain mitigation)
  uint16_t period_slots; // 0 = not tracked (treat as every occasion)
  uint16_t offset_slots;
  uint16_t symbol_mask; // 0 = not tracked (treat as whole slot)
  // frequency dimension (absolute PRB index in the carrier grid, as vrb_map is indexed)
  uint16_t rb_start;
  uint16_t rb_count;
} periodic_alloc_t;

#ifdef E3_AGENT

/*! \brief Rebuild a UE's entries from its current active BWP config.
 *  Idempotent: clears the UE's existing entries first. Safe to call when the UE
 *  has no dedicated config yet (registers nothing). */
void periodic_alloc_refresh_ue(NR_UE_info_t *UE);

/*! \brief Remove all entries belonging to a UE (on detach/release). */
void periodic_alloc_unregister_ue(uid_t uid);

/*! \brief Number of entries currently tracked. */
size_t periodic_alloc_count(void);

/*! \brief Entry at \p index, or NULL if out of range. Valid until the registry
 *  is next mutated (i.e. for the duration of one sched_lock-held read). */
const periodic_alloc_t *periodic_alloc_at(size_t index);

#endif /* E3_AGENT */

#endif // PERIODIC_ALLOC_REGISTRY_H
