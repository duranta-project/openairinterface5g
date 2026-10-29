/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Split 7.1 timebase: maps absolute symbol indices to radio sample timestamps.
 * An absolute symbol is ((abs_frame * slots_per_frame) + slot) * symbols_per_slot + symbol,
 * with abs_frame % 1024 == SFN, so no caller ever has to unwrap a frame number.
 */

#ifndef SPLIT7_TIMEBASE_H
#define SPLIT7_TIMEBASE_H

#include <stdint.h>
#include "PHY/defs_nr_common.h"

#define SPLIT7_TIMEBASE_VERSIONS 16

typedef struct {
  int64_t anchor_ts; ///< sample timestamp of the start of anchor_frame
  uint64_t anchor_frame; ///< absolute frame number
  /// Timing shifts, each in force from its slot on: RX and TX use the same shift
  /// for the same slot whenever they compute it. Sorted, accumulated.
  int num_shifts;
  struct {
    uint64_t from_slot;
    int64_t samples; ///< accumulated, subtracted from timestamps
  } shift[SPLIT7_TIMEBASE_VERSIONS];
} split7_timebase_t;

static inline uint64_t split7_abs_symbol(const NR_DL_FRAME_PARMS *fp, uint64_t abs_frame, int slot, int symbol)
{
  return (abs_frame * fp->slots_per_frame + slot) * fp->symbols_per_slot + symbol;
}

static inline uint64_t split7_abs_slot(const NR_DL_FRAME_PARMS *fp, uint64_t abs_frame, int slot)
{
  return abs_frame * fp->slots_per_frame + slot;
}

/// Sample timestamp of the start of abs_sym (start of its CP).
int64_t split7_symbol_ts(const split7_timebase_t *tb, const NR_DL_FRAME_PARMS *fp, uint64_t abs_sym);

/// First symbol starting at or after ts (ignoring timing shifts).
uint64_t split7_symbol_at(const split7_timebase_t *tb, const NR_DL_FRAME_PARMS *fp, int64_t ts);

/// Moves the frame boundary earlier by shift samples from from_slot on (monolithic
/// shiftForNextFrame convention). from_slot must not precede earlier shifts' slots.
/// The oldest shift is folded into the anchor when the table is full: timestamps of
/// slots before it are no longer exact.
void split7_timebase_shift(split7_timebase_t *tb, uint64_t from_slot, int32_t shift);

#endif
