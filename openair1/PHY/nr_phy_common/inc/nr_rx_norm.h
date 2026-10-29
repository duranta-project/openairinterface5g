/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_RX_NORM_H
#define NR_RX_NORM_H


/* Amplitude-invariant receiver scaling (default on). The fixed-point receivers used to derive their output shifts
 * from log2(|h|^2)/2, i.e. their outputs (compensated symbols, LLRs) followed the received amplitude, which made
 * every downstream fixed-point stage (int8 decoder input, +-32 PBCH/PDCCH clips, int16 metrics) work only near one
 * input level. Each item can be switched back to the legacy behaviour with an environment variable
 * OAI_RX_LEGACY_<NAME>=1, or all of them with OAI_RX_LEGACY_SCALING=1. */
typedef enum {
  NR_RX_NORM_PUSCH = 0, ///< PUSCH channel compensation shift log2(|h|^2)-based (1 and 2 layers)
  NR_RX_NORM_PDSCH, ///< PDSCH channel compensation shift (1 and 2 layers)
  NR_RX_NORM_PDCCH, ///< PDCCH shift (LLR scale before the [-32,31] clip)
  NR_RX_NORM_PBCH, ///< PBCH shift (LLR scale before the [-32,32] clip)
  NR_RX_NORM_QPSK, ///< rounding instead of truncating shift in the QPSK LLR
  NR_RX_NORM_DELAY, ///< delay estimation: rescale the LS estimate when its IDFT saturates
  NR_RX_NORM_PUCCH2, ///< PUCCH format 2: 64-bit non-coherent metrics, per-PUCCH LLR normalisation
  NR_RX_NORM_UCI, ///< UCI on PUSCH: LLR normalisation before the polar / small-block decoders
  NR_RX_NORM_COUNT
} nr_rx_norm_t;

/// 1 if the amplitude-invariant scaling of item f is enabled (default), 0 for the legacy behaviour
int nr_rx_norm_enabled(nr_rx_norm_t f);

#endif
