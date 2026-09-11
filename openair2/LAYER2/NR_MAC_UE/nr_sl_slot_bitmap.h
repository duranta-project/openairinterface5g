/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_SL_SLOT_BITMAP_H
#define NR_SL_SLOT_BITMAP_H

#include <stddef.h>
#include <stdint.h>

static inline int nr_sl_bitmap_test(const uint8_t *bitmap, size_t bit)
{
  return (bitmap[bit / 8] >> (7 - bit % 8)) & 1U;
}

static inline void nr_sl_bitmap_set(uint8_t *bitmap, size_t bit)
{
  bitmap[bit / 8] |= (uint8_t)(1U << (7 - bit % 8));
}

/* TS 38.214 clause 8 requires every symbol from sl-StartSymbol through
 * sl-LengthSymbols to be semi-statically configured as UL. */
static inline int nr_sl_tdd_slot_supports_symbols(size_t relative_slot,
                                                  size_t slots_per_period,
                                                  size_t full_ul_slots,
                                                  size_t mixed_ul_symbols,
                                                  size_t sl_start_symbol,
                                                  size_t sl_length_symbols)
{
  if (slots_per_period == 0 || full_ul_slots > slots_per_period || sl_length_symbols == 0
      || sl_start_symbol + sl_length_symbols > 14 || mixed_ul_symbols > 14)
    return 0;

  relative_slot %= slots_per_period;
  if (relative_slot >= slots_per_period - full_ul_slots)
    return 1;

  const size_t mixed_slot = slots_per_period - full_ul_slots - 1;
  const size_t first_ul_symbol = 14 - mixed_ul_symbols;
  return mixed_ul_symbols > 0 && relative_slot == mixed_slot && sl_start_symbol >= first_ul_symbol;
}

/* Apply the reserved-slot removal and sl-TimeResource stages of TS 38.214
 * clause 8. eligible_bitmap has already had S-SSB and non-SL slots removed.
 * The output bitmap is indexed by physical slot over the complete DFN/SFN
 * cycle. It must be zero-initialized by the caller. */
static inline size_t nr_sl_build_physical_pool_bitmap(const uint8_t *sl_time_resource,
                                                       size_t sl_bitmap_bits,
                                                       const uint8_t *eligible_bitmap,
                                                       size_t physical_slots,
                                                       uint8_t *physical_pool_bitmap)
{
  if (!sl_time_resource || sl_bitmap_bits == 0 || !eligible_bitmap || !physical_pool_bitmap)
    return 0;

  size_t eligible_slots = 0;
  for (size_t slot = 0; slot < physical_slots; slot++)
    eligible_slots += nr_sl_bitmap_test(eligible_bitmap, slot);

  const size_t reserved_slots = eligible_slots % sl_bitmap_bits;
  size_t reserved_index = 0;
  size_t next_reserved = reserved_slots ? 0 : physical_slots;
  size_t eligible_index = 0;
  size_t logical_slot = 0;
  size_t pool_slots = 0;

  for (size_t slot = 0; slot < physical_slots; slot++) {
    if (!nr_sl_bitmap_test(eligible_bitmap, slot))
      continue;

    if (eligible_index == next_reserved) {
      reserved_index++;
      next_reserved = reserved_index < reserved_slots
                          ? (reserved_index * eligible_slots) / reserved_slots
                          : physical_slots;
    } else {
      if (nr_sl_bitmap_test(sl_time_resource, logical_slot % sl_bitmap_bits)) {
        nr_sl_bitmap_set(physical_pool_bitmap, slot);
        pool_slots++;
      }
      logical_slot++;
    }
    eligible_index++;
  }

  return pool_slots;
}

/* TS 38.214 clause 8: t'^SL_k is the successive index of enabled pool slots. */
static inline int32_t nr_sl_pool_slot_index(const uint8_t *bitmap, size_t bitmap_bits, uint32_t absolute_slot)
{
  if (!bitmap || bitmap_bits == 0 || !nr_sl_bitmap_test(bitmap, absolute_slot % bitmap_bits))
    return -1;

  uint32_t enabled_per_period = 0;
  for (size_t bit = 0; bit < bitmap_bits; bit++)
    enabled_per_period += nr_sl_bitmap_test(bitmap, bit);

  const uint32_t position = absolute_slot % bitmap_bits;
  uint32_t index = (absolute_slot / bitmap_bits) * enabled_per_period;
  for (uint32_t bit = 0; bit < position; bit++)
    index += nr_sl_bitmap_test(bitmap, bit);
  return index;
}

#endif
