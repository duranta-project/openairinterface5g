/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Registry of per-UE periodic allocations (PUCCH/SRS/NZP-CSI-RS) for collision detection.
 * Callers hold the MAC sched_lock; there is no internal locking. */

#ifndef RAN_FUNC_SPECTRUM_REGISTRY_H
#define RAN_FUNC_SPECTRUM_REGISTRY_H

#include <stdint.h>
#include <stddef.h>
#include "LAYER2/NR_MAC_gNB/nr_mac_gNB.h" // NR_UE_info_t, uid_t, rnti_t

typedef enum {
  PA_PUCCH = 0, // all PUCCH resources
  PA_SRS, // periodic SRS only
  PA_CSIRS, // NZP-CSI-RS
} pa_alloc_type_t;

typedef enum { PA_DIR_DL = 0, PA_DIR_UL } pa_dir_t;

typedef struct {
  uid_t uid;
  rnti_t rnti; // for logging
  pa_alloc_type_t type;
  pa_dir_t dir;
  int resource_id; // PUCCH/CSI-RS resource id; SRS: first resource id of the periodic set; -1 if n/a
  // time dimension: not populated yet
  uint16_t period_slots; // 0 = not tracked (treat as every occasion)
  uint16_t offset_slots;
  uint16_t symbol_mask; // 0 = not tracked (treat as whole slot)
  // absolute PRB index, as vrb_map is indexed
  uint16_t rb_start;
  uint16_t rb_count;
} periodic_alloc_t;

/* Rebuild the UE's entries from its active BWP config; idempotent. */
void periodic_alloc_refresh_ue(NR_UE_info_t *UE);

void periodic_alloc_unregister_ue(uid_t uid);

size_t periodic_alloc_count(void);

/* Valid until the registry is next mutated. */
const periodic_alloc_t *periodic_alloc_at(size_t index);

#endif // RAN_FUNC_SPECTRUM_REGISTRY_H
