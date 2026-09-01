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
#include <stddef.h>
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

/* Strong defs in gNB_scheduler_ul_sensing.c / gNB_scheduler_prb_block.c. No MAC
 * in a CU-UP, so these are no-ops: the accessor hands back NULL and the SM's
 * dispatchers NACK back to the dApp before ever calling the setters. Keep the
 * signatures byte-identical to the strong defs -- the linker does not check
 * them, so a mismatch would silently pass wrong arguments. */
struct gNB_MAC_INST_s;
struct nr_cell_sched_s;

__attribute__((weak)) struct nr_cell_sched_s *nr_mac_e3_default_cell(void)
{
  return NULL;
}

__attribute__((weak)) bool set_sensing_policy(struct nr_cell_sched_s *cell, const uint16_t *mask, int n_slots)
{
  (void)cell;
  (void)mask;
  (void)n_slots;
  return false;
}

/* The strong def takes prb_block_dir_t (an enum); declaring the stub with
 * `int dir` is ABI-compatible at the C level since enums pass as int. */
__attribute__((weak)) bool set_prb_block_mask(struct gNB_MAC_INST_s *mac,
                                              struct nr_cell_sched_s *cell,
                                              int dir,
                                              const uint16_t *mask,
                                              int len)
{
  (void)mac;
  (void)cell;
  (void)dir;
  (void)mask;
  (void)len;
  return false;
}
