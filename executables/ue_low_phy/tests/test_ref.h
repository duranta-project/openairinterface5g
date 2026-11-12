/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * C side of the UE Low-PHY tests: the PHY modulation calls (monolithic slot paths and
 * the per-symbol ones the device makes), whose headers C++ cannot include, and the
 * setup of the PHY libraries.
 */

#ifndef UE_LOW_PHY_TEST_REF_H
#define UE_LOW_PHY_TEST_REF_H

#include "PHY/defs_nr_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Loads the libraries and fills fp: band 78, 106 PRB, 30 kHz, 1x1.
void ue_low_phy_test_init(NR_DL_FRAME_PARMS *fp);

/// UL slot of fp->symbols_per_slot symbols of txdataF (fft size each, all used) through
/// nr_tx_rotation_and_ofdm_mod(): samples_per_slot samples into out.
void ue_low_phy_test_ref_mod(NR_DL_FRAME_PARMS *fp, int slot, const c16_t *txdataF, c16_t *out);

/// DL symbol of rxdata (two frames, from timestamp 0) through nr_slot_fep(): fft size
/// bins into out.
void ue_low_phy_test_ref_demod(NR_DL_FRAME_PARMS *fp, int slot, int symbol, c16_t *rxdata, c16_t *out);

/// One symbol through nr_ofdm_mod_symbol() as the device modulates it: n_rb * 12 REs of
/// re (n_rb of link), get_samples_symbol_duration() samples into out.
void ue_low_phy_test_mod_symbol(NR_DL_FRAME_PARMS *fp, enum nr_Link link, int slot, int symbol, const c16_t *re, c16_t *out);

/// One symbol through nr_symbol_fep() as the device demodulates it (1 RX antenna).
/// @param in first sample of the symbol (start of its CP)
void ue_low_phy_test_demod_symbol(NR_DL_FRAME_PARMS *fp, int slot, int symbol, const c16_t *in, c16_t *out);

/// nr_prach_ofdm_mod(): ncp + reps * dftlen samples into out.
void ue_low_phy_test_prach_mod(const c16_t *bins, int dftlen, int ncp, int reps, c16_t *out);

#ifdef __cplusplus
}
#endif

#endif
