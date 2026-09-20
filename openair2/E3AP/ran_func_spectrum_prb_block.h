/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Private to the Spectrum RAN function. What the MAC may call is in
 * ran_func_spectrum_extern.h, what the Spectrum SM may call in
 * ran_func_spectrum_types.h.
 */

#ifndef RAN_FUNC_SPECTRUM_PRB_BLOCK_H
#define RAN_FUNC_SPECTRUM_PRB_BLOCK_H

#include <pthread.h>
#include <stdint.h>

#include "openair2/E3AP/ran_func_spectrum_extern.h"

/* dApp PRB-blocking state. mask_dl/ul are per-PRB 14-bit symbol bitmaps (vrb_map
 * encoding) OR'd into the VRB maps at slot start, so every allocator sees blocked
 * PRBs as occupied. prev_mask_* hold just-released bits that may still linger in
 * not-yet-reseeded vrb_map ring slices; helpers read mask_*|prev_mask_* until the
 * retire tick clears them. All fields under lock. */
typedef struct prb_block_state_s {
  pthread_mutex_t lock;
  bool active_dl;
  bool active_ul;
  bool needs_ul_full_stamp; /* one-shot: next apply stamps mask_ul into every ring slice */
  /* The xApp procedure whose control this install carries out, or 0 when the
   * block was the dApp's own decision. Reported once the mask is on the air,
   * so the RAN can tell that xApp when its decision took effect. */
  uint32_t pending_sequence_id;
  uint16_t mask_dl[MAX_BWP_SIZE]; /* per-PRB symbol bitmap (absolute PRB index) */
  uint16_t mask_ul[MAX_BWP_SIZE];
  uint16_t prev_mask_dl[MAX_BWP_SIZE]; /* bits dropped from mask_dl, still in the ring */
  uint16_t prev_mask_ul[MAX_BWP_SIZE];
  int64_t apply_counter; /* ++ per apply_prb_block_masks */
  int64_t dl_retire_at; /* zero prev_mask_dl when apply_counter >= this (INT64_MAX = idle) */
  int64_t ul_retire_at;
} prb_block_state_t;

/* Allocate / release a cell's block state. Called from e3_spectrum_mac_attach_cell()
 * and e3_spectrum_mac_detach_cell(); the state is owned here, not by the MAC. */
void prb_block_attach(const nr_cell_sched_t *cell);
void prb_block_detach(const nr_cell_sched_t *cell);

/* Copy the effective UL mask (mask_ul | prev_mask_ul) into out; true iff any bit set. */
bool get_effective_prb_block_mask_ul(const nr_cell_sched_t *cell, uint16_t out[MAX_BWP_SIZE]);

/* Copy the effective DL mask (mask_dl | prev_mask_dl) into out; true iff any bit set. */
bool get_effective_prb_block_mask_dl(const nr_cell_sched_t *cell, uint16_t out[MAX_BWP_SIZE]);

/* OR the active UL block into one vrb_map_UL row (no-op if no UL block). Re-applies
 * the block after a sensing-reserved row was reset to ulprbbl. */
void prb_block_reapply_ul_row(const nr_cell_sched_t *cell, uint16_t *row);

/* True iff conflict bits `alloc` are entirely the dApp UL block, so the caller can
 * skip the OR instead of asserting. Caller pre-filters alloc == 0. */
bool vrb_map_UL_conflict_is_dapp_block_only(const nr_cell_sched_t *cell, int rb, uint16_t alloc);

/* One rate-limited LOG_W summarizing a channel's per-RB collisions. No-op if count 0. */
void log_prb_block_collision_summary(const char *site,
                                     int first_rb,
                                     int last_rb,
                                     int count,
                                     uint16_t mask,
                                     frame_t frame,
                                     slot_t slot,
                                     rnti_t rnti);

#endif /* RAN_FUNC_SPECTRUM_PRB_BLOCK_H */
