/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef GNB_SCHEDULER_ULSCH_MU_MIMO_POLICIES_H
#define GNB_SCHEDULER_ULSCH_MU_MIMO_POLICIES_H

#include "LAYER2/NR_MAC_gNB/nr_mac_gNB.h"

int nr_ul_pf_mu_mimo(const nr_ul_sched_params_t *params, nr_ul_candidate_t *candidates, int n_candidates);

#define NR_SRS_SIG_MAX_STRIDE ((MAX_BWP_SIZE + 7) & ~7)

/// SRS effective channel of each layer of a UE, normalized per PRG, for the pairing correlation
typedef struct nr_srs_sig_s {
  uint8_t num_layers;
  uint16_t num_rx;
  uint16_t num_prg;
  uint16_t prg_stride; // num_prg rounded up for vectorization, padding is 0
  uint8_t *prg_valid; // [num_layers][num_prg] false if the channel of the PRG is 0
  float *re; // [num_layers][num_rx][prg_stride]
  float *im; // [num_layers][num_rx][prg_stride]
} nr_srs_sig_t;

nr_srs_sig_t *nr_srs_sig_create(const c16_t *channel_matrix,
                                int num_prg,
                                int num_rx,
                                int num_ports,
                                int ul_ri,
                                long transform_precoding,
                                uint8_t tpmi);
void nr_srs_sig_free(nr_srs_sig_t *sig);
float nr_srs_pair_correlation(const nr_srs_sig_t *a, const nr_srs_sig_t *b);

#endif // GNB_SCHEDULER_ULSCH_MU_MIMO_POLICIES_H
