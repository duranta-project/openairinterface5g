/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_CSI_PMI_H
#define NR_CSI_PMI_H

#include <stdbool.h>
#include <stdint.h>
#include "common/platform_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Type I single-panel PMI indices (TS 38.214 Section 5.2.2.2.1)
typedef struct {
  uint8_t i_1_1; ///< first DFT beam index, 0 .. N1*O1-1
  uint8_t i_1_2; ///< second DFT beam index, 0 .. N2*O2-1
  uint8_t i_1_3; ///< beam pair (k1, k2) selection, rank 2 only
  uint8_t i_2; ///< co-phasing index, 0..3 for rank 1, 0..1 for rank 2
} nr_csi_pmi_t;

/// Number of CSI-RS ports supported by nr_csi_pmi_type1_single_panel()
#define NR_CSI_PMI_MAX_PORTS 16

/**
 * @brief Select the Type I single-panel PMI for 4 to 16 CSI-RS ports, codebookMode 1.
 *
 * The codebook is the one of TS 38.214 Tables 5.2.2.2.1-5 (rank 1) and 5.2.2.2.1-6 (rank 2) with
 * (N1, N2) from Table 5.2.2.2.1-2, O1 = 4 and O2 = 4 (1 if N2 = 1). Rank 1 maximizes the received
 * power of the precoded channel, rank 2 maximizes log det(I + W^H R W / noise).
 *
 * @param n1 N1 (horizontal antenna ports per polarization)
 * @param n2 N2 (vertical antenna ports per polarization)
 * @param rank number of layers, 1 or 2
 * @param R channel covariance: sum over the measured REs and RX antennas of h^H h, where h is the 1 x P
 *          row of the CSI-RS channel estimate and P = 2*n1*n2; P x P, row-major, only the upper triangle
 *          is read
 * @param num_re number of REs summed in R
 * @param noise noise plus interference power per RE and RX antenna, in the scale of |h|^2
 * @param pmi selected PMI
 * @param sinr_db SINR per layer after precoding, in dB
 * @return false if the configuration is not supported
 */
bool nr_csi_pmi_type1_single_panel(int n1,
                                   int n2,
                                   int rank,
                                   const c64_t *R,
                                   int num_re,
                                   uint32_t noise,
                                   nr_csi_pmi_t *pmi,
                                   int *sinr_db);

#ifdef __cplusplus
}
#endif

#endif
