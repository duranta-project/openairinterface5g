/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef GNB_SCHEDULER_UL_SENSING_H
#define GNB_SCHEDULER_UL_SENSING_H

#include "LAYER2/NR_MAC_gNB/nr_mac_gNB.h"
/* sensing_range_t / MAX_SENSING_RANGES / SENSING_RNTI come via nr_mac_gNB.h
 * (which includes gNB_scheduler_ul_sensing_types.h). */

/* True if the slot is UL or MIXED. Note is_ul_slot() alone also requires
 * num_ul_symbols > 0 on a MIXED slot, so we OR in is_mixed_slot() to catch every
 * MIXED slot — matching the PHY IQ tap, which fires on all UL/MIXED slots. */
static inline bool nr_slot_is_ul_or_mixed(const frame_structure_t *fs, slot_t slot)
{
  return is_ul_slot(slot, fs) || is_mixed_slot(slot, fs);
}

/* --- Sensing policy state (dApp control) -------------------------------- */
/* Per-frame mask: for each slot index in the frame, a 14-bit bitmap of
 * symbols the dApp wants freed for sensing. Read by the TDA selector,
 * written by set_sensing_policy(). */
typedef struct {
  pthread_mutex_t lock;
  bool active; /* false = no mask, behave like default */
  uint16_t mask[MAX_NUM_SLOTS_ALLOWED]; /* per-slot symbol bitmap */
  int n_slots; /* slots-per-frame the mask was set for */
} sensing_policy_state_t;

#ifdef E3_AGENT

/* The cell the E3 service models act on (cell 0 today). NULL before MAC init.
 * Weak no-op stub in nr_mac_sensing_stub.c so nr-cuup links without the MAC. */
nr_cell_sched_t *nr_mac_e3_default_cell(void);

/* Allocate / release this cell's sensing-policy state, which backs
 * cell->sched_stateful_data. Called from nr_mac_config_scc() and the per-cell
 * teardown loop in mac_top_destroy_gNB(). */
void sensing_policy_init(nr_cell_sched_t *cell);
void sensing_policy_free(nr_cell_sched_t *cell);

/* Replace the current sensing mask. Set active=false (n_slots=0) to clear.
 * n_slots must match cell->frame_structure.numb_slots_frame, otherwise the
 * call is rejected (caller-side TDD mismatch). Returns true on success. */
bool set_sensing_policy(nr_cell_sched_t *cell, const uint16_t *mask, int n_slots);

/* --- TDA selector ---------------------------------------------- *
 * Mask-aware UL TDA selector (installed as ul_tda_select when sensing is on):
 * picks each candidate's TDA against the per-slot sensing mask so symbols stay
 * free for sensing; with no active policy it behaves like the default selector.
 * Returns the number of candidates given a valid TDA. */
int nr_ul_tda_select_sensing(gNB_MAC_INST *mac,
                             nr_cell_sched_t *cell,
                             nr_ul_candidate_t *cands,
                             int n_cand,
                             frame_t sched_frame,
                             slot_t sched_slot,
                             int k2);

/* Per-slot sensing glue, called unconditionally from gNB_dlsch_ulsch_scheduler().
 * reserve/restore bracket the UE allocators (block the slot, then free it for the
 * scan) and are no-ops unless the slot is in sensing_target_slots[].
 * scan_and_publish runs the scan + (Aerial) capture + (E3) publish, and is inert
 * unless sensing_enabled (the master switch). */
/* Is this (absolute) slot hard-reserved for sensing? The UL conflict-tolerance
 * sites (PRACH/Msg3) consult it before asserting on an occupied VRB entry. */
bool nr_mac_ul_slot_is_sensing_reserved(const nr_cell_sched_t *cell, int slot);

void nr_mac_sensing_reserve_ul_slot(nr_cell_sched_t *cell, int prev_slot, frame_t frame);
void nr_mac_sensing_restore_ul_slot(nr_cell_sched_t *cell, frame_t frame, slot_t slot);
void nr_mac_sensing_scan_and_publish(gNB_MAC_INST *mac, nr_cell_sched_t *cell, frame_t frame, slot_t slot);

/* Store this slot's sensing ranges for async E3 readout and wake the consumer.
 * Called from the UL scheduler after nr_scan_sensing_tiles(); the matching read
 * accessors live in gNB_scheduler_ul_sensing_types.h. */
void nr_mac_record_sensing_ranges(NR_COMMON_channels_t *cc,
                                  int beam,
                                  int frame,
                                  int slot,
                                  const sensing_range_t *ranges,
                                  int n_ranges);

#endif /* E3_AGENT */

#ifdef ENABLE_AERIAL
/* Inject one sensing-RNTI "capture" PUSCH so Aerial's L1 materializes the slot's
 * IQ (it only captures where a PDU points); one per slot suffices. No-op if a
 * real PUSCH/PUCCH/SRS is already scheduled. Defined in
 * gNB_scheduler_ul_sensing_aerial.c; called from nr_mac_sensing_scan_and_publish. */
void nr_fill_sensing_pusch(nr_cell_sched_t *cell, frame_t frame, slot_t slot, const sensing_range_t *ranges, int n_ranges);
#endif

#endif /* GNB_SCHEDULER_UL_SENSING_H */
