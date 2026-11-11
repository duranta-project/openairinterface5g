/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* PMI report encoding (UE) against decoding (gNB). For each PMI (i_1,1, i_1,2, i_1,3, i_2) of
 * TS 38.214 Tables 5.2.2.2.1-5/-6 (codebookMode 1), the UE packs X1 (nr_pack_pmi_x1()) with the
 * sub-field widths of TS 38.212 Table 6.3.1.1.2-1, and the gNB maps the report to a precoder of
 * its codebook (get_pm_index(), init_DL_MIMO_codebook()), which must be the precoder of the spec. */

#include <complex.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/utils/assertions.h"
#include "LAYER2/NR_MAC_COMMON/nr_mac_common.h"
#include "LAYER2/NR_MAC_gNB/mac_proto.h"
#include "LAYER2/NR_MAC_gNB/nr_mac_gNB.h"

// gNB/config.c, not in a header
nfapi_nr_pm_list_t init_DL_MIMO_codebook(nr_cell_sched_t *cell, nr_pdsch_AntennaPorts_t antenna_ports);

#define MAX_PORTS 12 // the gNB codebook stops below 16 ports

static int ceil_log2(int x)
{
  int b = 0;
  while ((1 << b) < x)
    b++;
  return b;
}

// TS 38.212 Table 6.3.1.1.2-1, Type I single-panel, codebookMode 1, more than 2 ports
static void spec_bitlen(int n1, int n2, int rank, CSI_Meas_bitlen_t *bitlen)
{
  const int o1 = 4;
  const int o2 = n2 > 1 ? 4 : 1;
  const int i = rank - 1;
  bitlen->pmi_i11_bitlen[i] = ceil_log2(n1 * o1);
  bitlen->pmi_i12_bitlen[i] = ceil_log2(n2 * o2);
  bitlen->pmi_i13_bitlen[i] = rank == 1 ? 0 : (n1 == 2 && n2 == 1 ? 1 : 2);
  bitlen->pmi_x1_bitlen[i] = bitlen->pmi_i11_bitlen[i] + bitlen->pmi_i12_bitlen[i] + bitlen->pmi_i13_bitlen[i];
  bitlen->pmi_x2_bitlen[i] = rank == 1 ? 2 : 1;
}

// TS 38.214 Table 5.2.2.2.1-3: (k1, k2) of i_1,3, in units of O1 and O2
static void spec_k1_k2(int n1, int n2, int i13, int *k1, int *k2)
{
  static const int k_n1_eq_n2[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
  static const int k_n1_gt_n2_gt_1[4][2] = {{0, 0}, {1, 0}, {0, 1}, {2, 0}};
  if (n2 == 1) {
    *k1 = i13;
    *k2 = 0;
  } else if (n1 == n2) {
    *k1 = k_n1_eq_n2[i13][0];
    *k2 = k_n1_eq_n2[i13][1];
  } else {
    *k1 = k_n1_gt_n2_gt_1[i13][0];
    *k2 = k_n1_gt_n2_gt_1[i13][1];
  }
}

// TS 38.214 Section 5.2.2.2.1: v_{l,m}, element a1 * N2 + a2 = exp(j2pi (l a1 / (O1 N1) + m a2 / (O2 N2)))
static void spec_v(int n1, int n2, int o1, int o2, int l, int m, double complex *v)
{
  for (int a1 = 0; a1 < n1; a1++)
    for (int a2 = 0; a2 < n2; a2++)
      v[a1 * n2 + a2] = cexp(2 * M_PI * I * ((double)l * a1 / (o1 * n1) + (double)m * a2 / (o2 * n2)));
}

// TS 38.214 Tables 5.2.2.2.1-5 (rank 1) and 5.2.2.2.1-6 (rank 2), without the scaling: W[layer][port]
static void spec_precoder(int n1, int n2, int rank, int i11, int i12, int i13, int i2, double complex W[2][MAX_PORTS])
{
  const int o1 = 4;
  const int o2 = n2 > 1 ? 4 : 1;
  const int half = n1 * n2;
  const double complex phi = cexp(I * M_PI * i2 / 2);
  int k1, k2;
  spec_k1_k2(n1, n2, i13, &k1, &k2);
  for (int layer = 0; layer < rank; layer++) {
    double complex v[MAX_PORTS / 2];
    spec_v(n1, n2, o1, o2, i11 + layer * k1 * o1, i12 + layer * k2 * o2, v);
    const double complex sign = layer == 0 ? 1 : -1;
    for (int k = 0; k < half; k++) {
      W[layer][k] = v[k];
      W[layer][half + k] = sign * phi * v[k];
    }
  }
}

// largest difference between the gNB precoder q and W scaled as in the gNB codebook (1/sqrt(rank), Q15)
static double precoder_error(const nfapi_nr_pm_pdu_t *q, double complex W[2][MAX_PORTS], int rank, int ports)
{
  double err = 0;
  for (int l = 0; l < rank; l++) {
    for (int k = 0; k < ports; k++) {
      const double complex expected = SHRT_MAX * W[l][k] / sqrt(rank);
      const double complex got = q->weights[l][k].r + I * q->weights[l][k].i;
      err = fmax(err, cabs(got - expected));
    }
  }
  return err;
}

int test_csi_pmi_gnb(int n1, int n2, int rank)
{
  const int ports = 2 * n1 * n2;
  AssertFatal(ports <= MAX_PORTS && rank >= 1 && rank <= 2, "unsupported (N1 %d, N2 %d) rank %d\n", n1, n2, rank);
  const nr_pdsch_AntennaPorts_t antenna_ports = {.N1 = n1, .N2 = n2, .XP = 2};
  nr_cell_sched_t *cell = calloc_or_fail(1, sizeof(*cell));
  NR_UE_info_t *UE = calloc_or_fail(1, sizeof(*UE));
  const nfapi_nr_pm_list_t codebook = init_DL_MIMO_codebook(cell, antenna_ports);

  nr_csi_report_t *csi_report = &UE->csi_report_template[0];
  csi_report->N1 = n1;
  csi_report->N2 = n2;
  spec_bitlen(n1, n2, rank, &csi_report->csi_meas_bitlen);
  struct CRI_RI_LI_PMI_CQI *report = &UE->UE_sched_ctrl.CSI_report.cri_ri_li_pmi_cqi_report;
  report->csi_report_id = 0;
  report->ri = rank - 1;

  const int o1 = 4;
  const int o2 = n2 > 1 ? 4 : 1;
  // i_1,3 values of TS 38.214 Table 5.2.2.2.1-3
  const int num_i13 = rank == 1 ? 1 : (n1 == 2 && n2 == 1 ? 2 : 4);
  const int num_i2 = rank == 1 ? 4 : 2;

  int failures = 0;
  for (int i11 = 0; i11 < n1 * o1; i11++)
    for (int i12 = 0; i12 < n2 * o2; i12++)
      for (int i13 = 0; i13 < num_i13; i13++)
        for (int i2 = 0; i2 < num_i2; i2++) {
          double complex W[2][MAX_PORTS];
          spec_precoder(n1, n2, rank, i11, i12, i13, i2, W);
          report->pmi_x1 = nr_pack_pmi_x1(&csi_report->csi_meas_bitlen, rank - 1, i11, i12, i13);
          report->pmi_x2 = i2;
          const int pm_index = get_pm_index(cell, UE, NR_DL_DCI_FORMAT_1_1, rank, antenna_ports.XP);
          double err = INFINITY;
          if (pm_index > 0 && pm_index <= codebook.num_pm_idx && codebook.pmi_pdu[pm_index - 1].numLayers == rank)
            err = precoder_error(&codebook.pmi_pdu[pm_index - 1], W, rank, ports);
          if (err > 1) { // Q15 rounding
            if (failures < 5)
              fprintf(stderr,
                      "(N1 %d, N2 %d) rank %d: PMI (%d, %d, %d, %d) -> X1 %d X2 %d -> gNB precoder %d, error %.1f\n",
                      n1,
                      n2,
                      rank,
                      i11,
                      i12,
                      i13,
                      i2,
                      report->pmi_x1,
                      report->pmi_x2,
                      pm_index,
                      err);
            failures++;
          }
        }
  free(codebook.pmi_pdu);
  free(UE);
  free(cell);
  return failures;
}
