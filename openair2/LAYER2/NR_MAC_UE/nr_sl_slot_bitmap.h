/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_SL_SLOT_BITMAP_H
#define NR_SL_SLOT_BITMAP_H

#include <stddef.h>
#include <stdint.h>

int get_bit_from_map(const uint8_t *buf, size_t bit_pos);
void append_bit(uint8_t *buf, size_t bit_pos, int bit_value);

/* TS 38.214 clause 8 requires every symbol from sl-StartSymbol through
 * sl-LengthSymbols to be semi-statically configured as UL. */
int nr_sl_tdd_slot_supports_symbols(size_t relative_slot,
                                    size_t slots_per_period,
                                    size_t full_ul_slots,
                                    size_t mixed_ul_symbols,
                                    size_t sl_start_symbol,
                                    size_t sl_length_symbols);

/* Apply the reserved-slot removal and sl-TimeResource stages of TS 38.214
 * clause 8. eligible_bitmap has already had S-SSB and non-SL slots removed.
 * The output bitmap is indexed by physical slot over the complete DFN/SFN
 * cycle. It must be zero-initialized by the caller. */
size_t nr_sl_build_physical_pool_bitmap(const uint8_t *sl_time_resource,
                                        size_t sl_bitmap_bits,
                                        const uint8_t *eligible_bitmap,
                                        size_t physical_slots,
                                        uint8_t *physical_pool_bitmap);

/* TS 38.214 clause 8: t'^SL_k is the successive index of enabled pool slots. */
int32_t nr_sl_pool_slot_index(const uint8_t *bitmap, size_t bitmap_bits, uint32_t absolute_slot);

/* TS 38.213 clause 16.3: position of a PSSCH pool slot among the slots
 * associated with one PSFCH occasion. */
int nr_sl_psfch_row(int32_t pool_slot_index, uint8_t min_time_gap, uint8_t psfch_period);

#endif
