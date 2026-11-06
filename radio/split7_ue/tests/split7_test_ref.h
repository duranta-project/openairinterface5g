/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * C side of the split7 tests: the monolithic UE paths the OFDM core is checked
 * against, whose headers C++ cannot include, and the setup of the PHY libraries.
 */

#ifndef SPLIT7_TEST_REF_H
#define SPLIT7_TEST_REF_H

#include "PHY/defs_nr_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Loads the libraries and fills fp: band 78, 106 PRB, 30 kHz, 1x1.
void split7_test_init(NR_DL_FRAME_PARMS *fp);

/// UL slot of fp->symbols_per_slot symbols of txdataF (fft size each, all used) through
/// nr_tx_rotation_and_ofdm_mod(): samples_per_slot samples into out.
void split7_test_ref_mod(NR_DL_FRAME_PARMS *fp, int slot, const c16_t *txdataF, c16_t *out);

/// DL symbol of rxdata (two frames, from timestamp 0) through nr_slot_fep(): fft size
/// bins into out.
void split7_test_ref_demod(NR_DL_FRAME_PARMS *fp, int slot, int symbol, c16_t *rxdata, c16_t *out);

#ifdef __cplusplus
}
#endif

#endif
