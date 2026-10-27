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

/* mask_* are per-PRB symbol bitmaps (vrb_map encoding). prev_mask_* hold released
 * bits that may still sit in unreseeded vrb_map ring slices; all fields under lock. */
typedef struct prb_block_state_s {
  pthread_mutex_t lock;
  bool active_dl;
  bool active_ul;
  bool needs_ul_full_stamp; /* one-shot: next apply stamps mask_ul into every ring slice */
  /* xApp procedure this install carries out (0 = dApp's own decision); reported once on the air */
  uint32_t pending_sequence_id;
  uint16_t mask_dl[MAX_BWP_SIZE]; /* per-PRB symbol bitmap (absolute PRB index) */
  uint16_t mask_ul[MAX_BWP_SIZE];
  uint16_t prev_mask_dl[MAX_BWP_SIZE]; /* bits dropped from mask_dl, still in the ring */
  uint16_t prev_mask_ul[MAX_BWP_SIZE];
  int64_t apply_counter; /* ++ per apply_prb_block_masks */
  int64_t dl_retire_at; /* zero prev_mask_dl when apply_counter >= this (INT64_MAX = idle) */
  int64_t ul_retire_at;
} prb_block_state_t;

void prb_block_attach(const nr_cell_sched_t *cell);
void prb_block_detach(const nr_cell_sched_t *cell);

/* Copy mask | prev_mask into out; true iff any bit is set. */
bool get_effective_prb_block_mask_ul(const nr_cell_sched_t *cell, uint16_t out[MAX_BWP_SIZE]);

bool get_effective_prb_block_mask_dl(const nr_cell_sched_t *cell, uint16_t out[MAX_BWP_SIZE]);

/* Re-applies the UL block to a row reset to ulprbbl on a sensing-reserved slot. */
void prb_block_reapply_ul_row(const nr_cell_sched_t *cell, uint16_t *row);

/* True iff the conflict bits are entirely the dApp UL block (caller filters alloc == 0). */
bool vrb_map_UL_conflict_is_dapp_block_only(const nr_cell_sched_t *cell, int rb, uint16_t alloc);

/* Rate-limited per call site; no-op if count is 0. */
void log_prb_block_collision_summary(const char *site,
                                     int first_rb,
                                     int last_rb,
                                     int count,
                                     uint16_t mask,
                                     frame_t frame,
                                     slot_t slot,
                                     rnti_t rnti);

#endif /* RAN_FUNC_SPECTRUM_PRB_BLOCK_H */
