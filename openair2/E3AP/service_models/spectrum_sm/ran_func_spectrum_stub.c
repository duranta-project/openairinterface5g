/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Weak fallback definitions for the sensing accessors the Spectrum SM calls.
 * The strong definitions are in ../../ran_func_spectrum.c, which needs the gNB
 * MAC and is therefore linked into L2_NR (see openair2/E3AP/CMakeLists.txt).
 *
 * libspectrum_sm.a reaches every consumer of SCHED_NR_LIB, because the PHY tap
 * needs e3ap (top-level CMakeLists.txt). Most of those also link L2_NR, where
 * the strong definitions win over these weak ones. The rest -- nr_pbchsim,
 * nr_pucchsim, nr_prachsim, nr_srssim, nr_dlschsim, nr_ulschsim, nr_psbchsim --
 * have no MAC at all, so these stubs are the only definitions they can resolve.
 * No dApp can subscribe there, the SM worker is never started, and the stubs
 * only keep the link step happy.
 */
#include "openair2/E3AP/ran_func_spectrum_types.h"

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
