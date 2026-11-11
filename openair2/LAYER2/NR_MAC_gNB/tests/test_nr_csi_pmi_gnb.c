/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* PMI report encoding (UE) against decoding (gNB). A CSI-MeasConfig with a Type I single-panel
 * codebook (codebookMode 1) goes through compute_csi_bitlen(), whose PMI sub-field widths must be
 * those of TS 38.212 Table 6.3.1.1.2-1 and whose codebook subset restriction must be the one of the
 * BIT STRING. For each PMI (i_1,1, i_1,2, i_1,3, i_2) of TS 38.214 Tables 5.2.2.2.1-5/-6, the UE
 * packs X1 (nr_pack_pmi_x1()) with these widths, and the gNB maps the report to a precoder of its
 * codebook (get_pm_index(), init_DL_MIMO_codebook()), which must be the precoder of the spec. */

#include <complex.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/utils/assertions.h"
#include "common/utils/oai_asn1.h"
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

// Pattern of the codebook subset restriction a_k in the test configurations, half the bits set, not symmetric under
// bit reversal for any number of bits of the test
static bool subset_bit(int k)
{
  return ((k * 37 + 11) >> 2) & 1;
}

// a_{A_c - 1} ... a_0 of TS 38.214 Section 5.2.2.2.1 as a BIT STRING, a_{A_c - 1} first
static void set_subset_restriction(BIT_STRING_t *bs, int num_bits)
{
  bs->size = (num_bits + 7) / 8;
  bs->bits_unused = bs->size * 8 - num_bits;
  bs->buf = calloc_or_fail(bs->size, 1);
  for (int k = 0; k < num_bits; k++) {
    const int pos = num_bits - 1 - k;
    if (subset_bit(k))
      bs->buf[pos / 8] |= 0x80 >> (pos % 8);
  }
}

static BIT_STRING_t *n1_n2_restriction(
    struct NR_CodebookConfig__codebookType__type1__subType__typeI_SinglePanel__nrOfAntennaPorts__moreThanTwo *more,
    int n1,
    int n2)
{
  typeof(more->n1_n2) *c = &more->n1_n2;
  if (n1 == 2 && n2 == 1) {
    c->present =
        NR_CodebookConfig__codebookType__type1__subType__typeI_SinglePanel__nrOfAntennaPorts__moreThanTwo__n1_n2_PR_two_one_TypeI_SinglePanel_Restriction;
    return &c->choice.two_one_TypeI_SinglePanel_Restriction;
  }
  if (n1 == 2 && n2 == 2) {
    c->present =
        NR_CodebookConfig__codebookType__type1__subType__typeI_SinglePanel__nrOfAntennaPorts__moreThanTwo__n1_n2_PR_two_two_TypeI_SinglePanel_Restriction;
    return &c->choice.two_two_TypeI_SinglePanel_Restriction;
  }
  if (n1 == 4 && n2 == 1) {
    c->present =
        NR_CodebookConfig__codebookType__type1__subType__typeI_SinglePanel__nrOfAntennaPorts__moreThanTwo__n1_n2_PR_four_one_TypeI_SinglePanel_Restriction;
    return &c->choice.four_one_TypeI_SinglePanel_Restriction;
  }
  if (n1 == 3 && n2 == 2) {
    c->present =
        NR_CodebookConfig__codebookType__type1__subType__typeI_SinglePanel__nrOfAntennaPorts__moreThanTwo__n1_n2_PR_three_two_TypeI_SinglePanel_Restriction;
    return &c->choice.three_two_TypeI_SinglePanel_Restriction;
  }
  if (n1 == 6 && n2 == 1) {
    c->present =
        NR_CodebookConfig__codebookType__type1__subType__typeI_SinglePanel__nrOfAntennaPorts__moreThanTwo__n1_n2_PR_six_one_TypeI_SinglePanel_Restriction;
    return &c->choice.six_one_TypeI_SinglePanel_Restriction;
  }
  AssertFatal(false, "(N1 %d, N2 %d) not in the test\n", n1, n2);
  return NULL;
}

// One periodic NZP CSI-RS resource and one cri-RI-PMI-CQI report with a Type I single-panel codebook, codebookMode 1
static NR_CSI_MeasConfig_t *csi_meas_config(int n1, int n2, uint8_t ri_restriction)
{
  NR_CSI_MeasConfig_t *meas = calloc_or_fail(1, sizeof(*meas));

  asn1cCalloc(meas->nzp_CSI_RS_ResourceSetToAddModList, sets);
  asn1cSequenceAdd(sets->list, NR_NZP_CSI_RS_ResourceSet_t, set);
  set->nzp_CSI_ResourceSetId = 0;
  asn1cSequenceAdd(set->nzp_CSI_RS_Resources.list, NR_NZP_CSI_RS_ResourceId_t, res_id);
  *res_id = 0;

  asn1cCalloc(meas->csi_ResourceConfigToAddModList, res_configs);
  asn1cSequenceAdd(res_configs->list, NR_CSI_ResourceConfig_t, res_config);
  res_config->csi_ResourceConfigId = 0;
  res_config->resourceType = NR_CSI_ResourceConfig__resourceType_periodic;
  res_config->csi_RS_ResourceSetList.present = NR_CSI_ResourceConfig__csi_RS_ResourceSetList_PR_nzp_CSI_RS_SSB;
  asn1cCalloc(res_config->csi_RS_ResourceSetList.choice.nzp_CSI_RS_SSB, nzp_ssb);
  asn1cCalloc(nzp_ssb->nzp_CSI_RS_ResourceSetList, set_ids);
  asn1cSequenceAdd(set_ids->list, NR_NZP_CSI_RS_ResourceSetId_t, set_id);
  *set_id = 0;

  asn1cCalloc(meas->csi_ReportConfigToAddModList, reports);
  asn1cSequenceAdd(reports->list, NR_CSI_ReportConfig_t, report);
  report->reportConfigId = 0;
  report->resourcesForChannelMeasurement = 0;
  report->reportQuantity.present = NR_CSI_ReportConfig__reportQuantity_PR_cri_RI_PMI_CQI;
  asn1cCalloc(report->reportFreqConfiguration, freq);
  asn1cCallocOne(freq->cqi_FormatIndicator, NR_CSI_ReportConfig__reportFreqConfiguration__cqi_FormatIndicator_widebandCQI);
  asn1cCalloc(report->codebookConfig, codebook);
  codebook->codebookType.present = NR_CodebookConfig__codebookType_PR_type1;
  asn1cCalloc(codebook->codebookType.choice.type1, type1);
  type1->codebookMode = 1;
  type1->subType.present = NR_CodebookConfig__codebookType__type1__subType_PR_typeI_SinglePanel;
  asn1cCalloc(type1->subType.choice.typeI_SinglePanel, sp);
  sp->typeI_SinglePanel_ri_Restriction.size = 1;
  sp->typeI_SinglePanel_ri_Restriction.buf = calloc_or_fail(1, 1);
  sp->typeI_SinglePanel_ri_Restriction.buf[0] = ri_restriction;
  sp->nrOfAntennaPorts.present =
      NR_CodebookConfig__codebookType__type1__subType__typeI_SinglePanel__nrOfAntennaPorts_PR_moreThanTwo;
  asn1cCalloc(sp->nrOfAntennaPorts.choice.moreThanTwo, more);
  const int o2 = n2 > 1 ? 4 : 1;
  set_subset_restriction(n1_n2_restriction(more, n1, n2), n1 * 4 * n2 * o2);
  return meas;
}

// compute_csi_bitlen() of csi_meas_config() against TS 38.212 Table 6.3.1.1.2-1 and the subset restriction pattern
static int check_csi_report_template(const nr_csi_report_t *csi_report, int n1, int n2, uint8_t ri_restriction)
{
  int failures = 0;
  if (csi_report->N1 != n1 || csi_report->N2 != n2 || csi_report->codebook_mode != 1) {
    fprintf(stderr,
            "(N1 %d, N2 %d): template N1 %d N2 %d codebookMode %d\n",
            n1,
            n2,
            csi_report->N1,
            csi_report->N2,
            csi_report->codebook_mode);
    failures++;
  }
  for (int rank = 1; rank <= 8; rank++) {
    CSI_Meas_bitlen_t want = {0};
    if (((ri_restriction >> (rank - 1)) & 1) && rank <= 2)
      spec_bitlen(n1, n2, rank, &want);
    const CSI_Meas_bitlen_t *got = &csi_report->csi_meas_bitlen;
    const int i = rank - 1;
    if (got->pmi_i11_bitlen[i] != want.pmi_i11_bitlen[i] || got->pmi_i12_bitlen[i] != want.pmi_i12_bitlen[i]
        || got->pmi_i13_bitlen[i] != want.pmi_i13_bitlen[i] || got->pmi_x1_bitlen[i] != want.pmi_x1_bitlen[i]
        || got->pmi_x2_bitlen[i] != want.pmi_x2_bitlen[i]) {
      fprintf(stderr,
              "(N1 %d, N2 %d) rank %d: PMI widths i11 %d i12 %d i13 %d X1 %d X2 %d, expected %d %d %d %d %d\n",
              n1,
              n2,
              rank,
              got->pmi_i11_bitlen[i],
              got->pmi_i12_bitlen[i],
              got->pmi_i13_bitlen[i],
              got->pmi_x1_bitlen[i],
              got->pmi_x2_bitlen[i],
              want.pmi_i11_bitlen[i],
              want.pmi_i12_bitlen[i],
              want.pmi_i13_bitlen[i],
              want.pmi_x1_bitlen[i],
              want.pmi_x2_bitlen[i]);
      failures++;
    }
  }
  const int num_bits = n1 * 4 * n2 * (n2 > 1 ? 4 : 1);
  uint64_t want_subset = UINT64_MAX; // not kept beyond 64 bits
  if (num_bits <= 64) {
    want_subset = 0;
    for (int k = 0; k < num_bits; k++)
      want_subset |= (uint64_t)subset_bit(k) << k;
  }
  if (csi_report->codebook_subset != want_subset) {
    fprintf(stderr, "(N1 %d, N2 %d): codebook subset %016lx, expected %016lx\n", n1, n2, csi_report->codebook_subset, want_subset);
    failures++;
  }
  return failures;
}

// Rank 2 only, reconfigured from ranks 1 and 2: no PMI widths left for rank 1, no RI field, and the RI decodes to
// rank 2
int test_csi_bitlen_ri_restriction(int n1, int n2)
{
  nr_csi_report_t csi_report_template[MAX_CSI_REPORTCONFIG] = {0};
  NR_CSI_MeasConfig_t *meas = csi_meas_config(n1, n2, 0b11);
  compute_csi_bitlen(meas, csi_report_template);
  ASN_STRUCT_FREE(asn_DEF_NR_CSI_MeasConfig, meas);
  meas = csi_meas_config(n1, n2, 0b10);
  compute_csi_bitlen(meas, csi_report_template);
  const nr_csi_report_t *csi_report = &csi_report_template[0];
  int failures = check_csi_report_template(csi_report, n1, n2, 0b10);
  const uint8_t ri_restriction = csi_report->csi_meas_bitlen.ri_restriction;
  const int ri_index = nr_ri_index(ri_restriction, 1);
  if (csi_report->csi_meas_bitlen.ri_bitlen != 0 || ri_index != 0 || nr_ri_from_index(ri_restriction, ri_index) != 1) {
    fprintf(stderr,
            "(N1 %d, N2 %d): RI width %d, rank 2 packed as %d decodes to rank %d\n",
            n1,
            n2,
            csi_report->csi_meas_bitlen.ri_bitlen,
            ri_index,
            nr_ri_from_index(ri_restriction, ri_index) + 1);
    failures++;
  }
  ASN_STRUCT_FREE(asn_DEF_NR_CSI_MeasConfig, meas);
  return failures;
}

// nr_ri_from_index() inverts nr_ri_index() for every restriction and allowed rank
int test_ri_index_round_trip(void)
{
  int failures = 0;
  for (int restriction = 1; restriction < 256; restriction++) {
    for (int ri = 0; ri < 8; ri++) {
      if (!((restriction >> ri) & 1))
        continue;
      const int got = nr_ri_from_index(restriction, nr_ri_index(restriction, ri));
      if (got != ri) {
        fprintf(stderr, "ri_restriction %02x: rank %d decodes to rank %d\n", restriction, ri + 1, got + 1);
        failures++;
      }
    }
    if (nr_ri_from_index(restriction, __builtin_popcount(restriction)) != -1) {
      fprintf(stderr, "ri_restriction %02x: out-of-range RI index accepted\n", restriction);
      failures++;
    }
  }
  return failures;
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

  NR_CSI_MeasConfig_t *meas = csi_meas_config(n1, n2, 0b11);
  compute_csi_bitlen(meas, UE->csi_report_template);
  const nr_csi_report_t *csi_report = &UE->csi_report_template[0];
  int failures = check_csi_report_template(csi_report, n1, n2, 0b11);
  struct CRI_RI_LI_PMI_CQI *report = &UE->UE_sched_ctrl.CSI_report.cri_ri_li_pmi_cqi_report;
  report->csi_report_id = 0;
  report->ri = rank - 1;

  const int o1 = 4;
  const int o2 = n2 > 1 ? 4 : 1;
  // i_1,3 values of TS 38.214 Table 5.2.2.2.1-3
  const int num_i13 = rank == 1 ? 1 : (n1 == 2 && n2 == 1 ? 2 : 4);
  const int num_i2 = rank == 1 ? 4 : 2;

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
  ASN_STRUCT_FREE(asn_DEF_NR_CSI_MeasConfig, meas);
  free(codebook.pmi_pdu);
  free(UE);
  free(cell);
  return failures;
}
