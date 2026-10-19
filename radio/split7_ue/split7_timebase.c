/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "split7_timebase.h"
#include "common/utils/assertions.h"

int64_t split7_symbol_ts(const split7_timebase_t *tb, const NR_DL_FRAME_PARMS *fp, uint64_t abs_sym)
{
  const uint64_t per_frame = (uint64_t)fp->slots_per_frame * fp->symbols_per_slot;
  const uint64_t frame = abs_sym / per_frame;
  const int slot = (abs_sym % per_frame) / fp->symbols_per_slot;
  const int symbol = abs_sym % fp->symbols_per_slot;
  const uint64_t abs_slot = abs_sym / fp->symbols_per_slot;
  int64_t shift = 0;
  for (int i = 0; i < tb->num_shifts && tb->shift[i].from_slot <= abs_slot; i++)
    shift = tb->shift[i].samples;
  return tb->anchor_ts + ((int64_t)frame - (int64_t)tb->anchor_frame) * fp->samples_per_frame + get_samples_slot_timestamp(fp, slot)
         + get_samples_symbol_timestamp(fp, slot, symbol) - shift;
}

void split7_timebase_shift(split7_timebase_t *tb, uint64_t from_slot, int32_t shift)
{
  const int64_t prev = tb->num_shifts ? tb->shift[tb->num_shifts - 1].samples : 0;
  AssertFatal(!tb->num_shifts || from_slot >= tb->shift[tb->num_shifts - 1].from_slot, "timing shift out of order\n");
  if (tb->num_shifts && tb->shift[tb->num_shifts - 1].from_slot == from_slot) {
    tb->shift[tb->num_shifts - 1].samples += shift;
    return;
  }
  if (tb->num_shifts == SPLIT7_TIMEBASE_VERSIONS) {
    const int64_t oldest = tb->shift[0].samples;
    tb->anchor_ts -= oldest;
    for (int i = 1; i < tb->num_shifts; i++)
      tb->shift[i - 1] = (typeof(tb->shift[0])){.from_slot = tb->shift[i].from_slot, .samples = tb->shift[i].samples - oldest};
    tb->num_shifts--;
    tb->shift[tb->num_shifts] = (typeof(tb->shift[0])){.from_slot = from_slot, .samples = prev - oldest + shift};
  } else {
    tb->shift[tb->num_shifts] = (typeof(tb->shift[0])){.from_slot = from_slot, .samples = prev + shift};
  }
  tb->num_shifts++;
}

uint64_t split7_symbol_at(const split7_timebase_t *tb, const NR_DL_FRAME_PARMS *fp, int64_t ts)
{
  const int64_t rel = ts - tb->anchor_ts;
  int64_t frame_delta = rel / fp->samples_per_frame;
  if (rel < 0 && rel % fp->samples_per_frame)
    frame_delta--;
  const uint64_t frame = tb->anchor_frame + frame_delta;
  const int64_t in_frame = rel - frame_delta * fp->samples_per_frame;
  uint64_t sym = split7_abs_symbol(fp, frame, 0, 0);
  // At most one frame of symbols to walk; called when anchoring, before any shift.
  while (split7_symbol_ts(tb, fp, sym) - (tb->anchor_ts + frame_delta * fp->samples_per_frame) < in_frame)
    sym++;
  return sym;
}
