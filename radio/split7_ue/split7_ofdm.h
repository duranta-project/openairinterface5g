/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Split 7.1 OFDM core: stateless per-symbol modulation/demodulation, with no
 * radio, thread or ring knowledge. The frequency-domain side uses the L1's
 * layout: one symbol is fft_size bins, FFT shifted (the carrier's n_rb * 12 REs
 * contiguous from index 0, lowest frequency first). Results are bit-exact with
 * the monolithic paths (nr_tx_rotation_and_ofdm_mod(), nr_slot_fep(),
 * generate_nr_prach()).
 */

#ifndef SPLIT7_OFDM_H
#define SPLIT7_OFDM_H

#include "PHY/defs_nr_common.h"

/// Samples (CP included) of one OFDM symbol.
static inline int split7_symbol_samples(const NR_DL_FRAME_PARMS *fp, int slot, int symbol)
{
  return get_samples_symbol_duration(fp, slot, symbol, 1);
}

/// Modulates one symbol: 38.211 §5.3 rotation for link, inverse FFT shift, IDFT, CP.
/// @param in fft_size bins, FFT shifted, unrotated; only the first n_rb * 12 are read
/// @param scratch fft_size bins, 64-byte aligned
/// @param out split7_symbol_samples() samples
void split7_ofdm_mod(const NR_DL_FRAME_PARMS *fp,
                     enum nr_Link link,
                     int n_rb,
                     int slot,
                     int symbol,
                     const c16_t *in,
                     c16_t *scratch,
                     c16_t *out);

/// Demodulates one symbol: DFT over the window starting 1/ofdm_offset_divisor of a
/// normal CP before the end of the CP (as nr_slot_fep()), FFT shift, rotation removal.
/// @param in first sample of the symbol (start of its CP)
/// @param out fft_size bins, FFT shifted
void split7_ofdm_demod(const NR_DL_FRAME_PARMS *fp, enum nr_Link link, int n_rb, int slot, int symbol, const c16_t *in, c16_t *out);

/// Modulates a window of arbitrary numerology (PRACH): IDFT of dftlen bins (native
/// layout), CP made of the last ncp samples, the IDFT output repeated reps times.
/// @param scratch dftlen samples, 64-byte aligned
/// @param out ncp + reps * dftlen samples
void split7_ofdm_window_mod(const c16_t *bins, int dftlen, int ncp, int reps, c16_t *scratch, c16_t *out);

#endif
