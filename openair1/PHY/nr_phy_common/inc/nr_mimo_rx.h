/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!\file nr_mimo_rx.h
 * \brief 2-layer MIMO receiver selection of the gNB (PUSCH) and the UE (PDSCH): all receivers are members of the L-best
 * detector family (the seed is the unbiased MMSE estimate; L = 1 keeps the seed alone, i.e. linear MMSE).
 */

#ifndef NR_MIMO_RX_H
#define NR_MIMO_RX_H

#include <stdbool.h>
#include <string.h>

/// 2-layer detector of the L-best family (the lbest argument of nr_inner_rx / nr_compute_ML_llr)
#define NR_LBEST_ML 0 ///< full max-log ML (64/256QAM full grid)
#define NR_LBEST_WINDOW 1 ///< 3x3 window around the MMSE seed, widened to the full grid on low-SINR REs
#define NR_LBEST_WINDOW_HIRES 2 ///< NR_LBEST_WINDOW with the fp32 256QAM metric (= NR_LBEST_WINDOW for 64QAM)
#define NR_LBEST_L1 3 ///< L = 1: the MMSE seed alone with piecewise-linear LLRs (linear MMSE), all modulations

/// 2-layer receiver (gNB pusch_mimo_receiver, UE --pdsch-mimo-receiver). AUTO is 0 so an unconfigured gNB / UE gets
/// the default.
typedef enum {
  NR_MIMO_RX_AUTO = 0, ///< per allocation: ML for QPSK/16QAM, L = 1 for 64/256QAM with >= 4 Rx, L-best otherwise
  NR_MIMO_RX_LBEST, ///< 3x3 window around the MMSE seed (QPSK/16QAM: exact ML, same kernel)
  NR_MIMO_RX_MMSE, ///< L = 1 (linear MMSE): cheapest for 64/256QAM
  NR_MIMO_RX_ML, ///< exact max-log ML (64/256QAM full grid): most expensive
} nr_mimo_rx_t;

/// Receiver used for one allocation: AUTO resolves on its modulation order and the number of Rx streams. With 4 Rx,
/// L = 1 matches L-best and ML for 64/256QAM up to MCS 24 (within ~0.4 dB, TDL-A) at a third of the cost; with 2 Rx the
/// near-ML receivers gain 1-4 dB; for QPSK/16QAM exact ML costs no more than L = 1 and gains up to 1 dB.
static inline nr_mimo_rx_t nr_mimo_rx_resolve(nr_mimo_rx_t rx, int qm, int nb_rx)
{
  if (rx != NR_MIMO_RX_AUTO)
    return rx;
  if (qm <= 4)
    return NR_MIMO_RX_ML;
  return nb_rx >= 4 ? NR_MIMO_RX_MMSE : NR_MIMO_RX_LBEST;
}

/// lbest argument of nr_inner_rx for a resolved receiver; hires: the fp32 256QAM metric of the window
static inline int nr_mimo_rx_lbest(nr_mimo_rx_t rx, bool hires)
{
  switch (rx) {
    case NR_MIMO_RX_LBEST:
      return hires ? NR_LBEST_WINDOW_HIRES : NR_LBEST_WINDOW;
    case NR_MIMO_RX_ML:
      return NR_LBEST_ML;
    default:
      return NR_LBEST_L1;
  }
}

/// Parses "auto" | "lbest" | "mmse" | "ml"; returns -1 if unknown.
static inline int nr_mimo_rx_from_str(const char *s)
{
  if (!strcmp(s, "auto"))
    return NR_MIMO_RX_AUTO;
  if (!strcmp(s, "lbest"))
    return NR_MIMO_RX_LBEST;
  if (!strcmp(s, "mmse"))
    return NR_MIMO_RX_MMSE;
  if (!strcmp(s, "ml"))
    return NR_MIMO_RX_ML;
  return -1;
}

#endif
