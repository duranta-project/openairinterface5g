/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <assert.h>

#include "openair2/LAYER2/NR_MAC_UE/nr_sl_slot_bitmap.h"

int main(void)
{
  /* A full UL slot accepts the configured span. A mixed slot only accepts it
   * when every configured SL symbol is in the trailing UL-symbol region. */
  assert(nr_sl_tdd_slot_supports_symbols(7, 10, 3, 4, 0, 14));
  assert(nr_sl_tdd_slot_supports_symbols(6, 10, 3, 4, 10, 4));
  assert(!nr_sl_tdd_slot_supports_symbols(6, 10, 3, 4, 9, 4));
  assert(!nr_sl_tdd_slot_supports_symbols(5, 10, 3, 4, 10, 4));

  /* Eleven TDD/S-SSB-filtered candidates with a four-bit time-resource
   * bitmap produce three distributed reserved slots at candidate ordinals
   * 0, 3, and 7. Applying 1010 to the remaining eight logical slots enables
   * physical slots 1, 5, 8, and 12. */
  const uint8_t time_resource[] = {0xa0};
  const uint8_t eligible[] = {0xee, 0xec};
  uint8_t physical_pool[] = {0, 0};
  assert(nr_sl_build_physical_pool_bitmap(time_resource, 4, eligible, 16, physical_pool) == 4);
  assert(physical_pool[0] == 0x44);
  assert(physical_pool[1] == 0x88);

  /* Physical pool slots 6-9 and 16-19 in a 20-slot bitmap. */
  const uint8_t pool_bitmap[] = {0x03, 0xc0, 0xf0};

  assert(nr_sl_pool_slot_index(pool_bitmap, 20, 6) == 0);
  assert(nr_sl_pool_slot_index(pool_bitmap, 20, 9) == 3);
  assert(nr_sl_pool_slot_index(pool_bitmap, 20, 19) == 7);
  assert(nr_sl_pool_slot_index(pool_bitmap, 20, 5) == -1);

  /* The bitmap repeats directly. */
  assert(nr_sl_pool_slot_index(pool_bitmap, 20, 26) == 8);
  assert(nr_sl_pool_slot_index(pool_bitmap, 20, 39) == 15);

  /* A bitmap period is not shifted to align it with a radio-frame boundary. */
  const uint8_t non_frame_aligned_bitmap[] = {0xa8}; /* enabled bits 0, 2, 4 of 5 */
  assert(nr_sl_pool_slot_index(non_frame_aligned_bitmap, 5, 5) == 3);
  assert(nr_sl_pool_slot_index(non_frame_aligned_bitmap, 5, 7) == 4);

  /* PSFCH period 2 therefore selects logical pool slots 0, 2, 4, ... */
  assert(nr_sl_pool_slot_index(pool_bitmap, 20, 6) % 2 == 0);
  assert(nr_sl_pool_slot_index(pool_bitmap, 20, 7) % 2 == 1);

  return 0;
}
