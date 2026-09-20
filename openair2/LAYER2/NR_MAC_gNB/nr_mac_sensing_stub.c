/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Weak fallback definitions for the MAC sensing accessors that the Spectrum SM
 * calls. The strong defs live next to this file in gNB_scheduler_ul_sensing.c.
 *
 * libspectrum_sm.a is linked into every E3-aware binary:
 *   - nr-softmodem : MAC present     → the strong defs win over these weak ones.
 *   - nr-cuup      : MAC NOT present → these weak stubs are the resolved symbols.
 *
 * The CU-UP hosts no gNB MAC scheduler, so a dApp never subscribes to RF=1
 * there and the SM worker is never started. These stubs only keep the nr-cuup
 * link step happy (per GNU ld, a strong definition always beats a weak one).
 *
 * Lives in the MAC tree (with the accessor decls and the strong defs) but is
 * compiled into the spectrum_sm library, because nr-cuup links that library and
 * not the MAC — see openair2/E3AP/service_models/spectrum_sm/CMakeLists.txt.
 */
#include "gNB_scheduler_ul_sensing_types.h"

#include <stdbool.h>
#include <stdint.h>

__attribute__((weak)) bool nr_mac_get_sensing_ranges(int mod_id,
                                                     int beam,
                                                     int slot,
                                                     sensing_range_t *out_ranges,
                                                     int max_out,
                                                     uint8_t *out_n)
{
  (void)mod_id;
  (void)beam;
  (void)slot;
  (void)out_ranges;
  (void)max_out;
  if (out_n)
    *out_n = 0;
  return false;
}

__attribute__((weak)) bool nr_mac_wait_for_sensing_publish(uint64_t timeout_ns,
                                                           uint64_t *inout_seq,
                                                           nr_mac_sensing_publish_meta_t *out_meta)
{
  (void)timeout_ns;
  (void)inout_seq;
  if (out_meta) {
    out_meta->beam = 0;
    out_meta->frame = 0;
    out_meta->slot = 0;
    out_meta->timestamp_ns = 0;
  }
  return false;
}

__attribute__((weak)) void nr_mac_signal_sensing_shutdown(void)
{
}
