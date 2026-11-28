/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PHY/NR_REFSIG/nr_refsig.h"
#include "SCHED_NR_UE/defs.h"
#include "nr_phy_common_csi_rs.h"
#include "radio/COMMON/common_lib.h"

openair0_config_t openair0_cfg_g[MAX_CARDS];
static int measurement_count;
static int indication_count;
static fapi_nr_l1_measurements_t last_measurement;
static uint32_t last_modulation_length;

void __real_nr_modulation(const uint32_t *in, uint32_t length, uint16_t mod_order, int16_t *out);

void __wrap_nr_modulation(const uint32_t *in, uint32_t length, uint16_t mod_order, int16_t *out)
{
  last_modulation_length = length;
  __real_nr_modulation(in, length, mod_order, out);
}

void nr_fill_rx_indication(fapi_nr_rx_indication_t *rx_ind,
                           uint8_t pdu_type,
                           PHY_VARS_NR_UE *ue,
                           int cw_idx,
                           int harq_pid,
                           NR_UE_DLSCH_t *dlsch,
                           const UE_nr_rxtx_proc_t *proc,
                           void *typeSpecific)
{
  DevAssert(pdu_type == FAPI_NR_MEAS_IND);
  measurement_count++;
  last_measurement = *(const fapi_nr_l1_measurements_t *)typeSpecific;
  rx_ind->number_pdus = 1;
}

static int receive_indication(nr_downlink_indication_t *indication)
{
  DevAssert(indication->rx_ind->number_pdus == 1);
  indication_count++;
  return 0;
}

void trs_freq_correction(PHY_VARS_NR_UE *ue, int cfo)
{
  AssertFatal(false, "Unexpected frequency correction in CSI-RS test\n");
}

typedef struct {
  const char *name;
  uint16_t start_rb;
  uint16_t nr_of_rbs;
  uint8_t row;
  uint8_t density;
  uint8_t measurements;
  bool tracking;
  bool expected_success;
  bool use_csi_im;
} csi_rs_test_case_t;

static int check_resource(const csi_rs_test_case_t *test)
{
  enum { carrier_n_rb = 64, fft_size = 1024, symbols_per_slot = 14, samples_per_slot = symbols_per_slot * fft_size };
  PHY_VARS_NR_UE *ue = calloc(1, sizeof(*ue));
  DevAssert(ue != NULL);
  nr_ue_if_module_t interface = {.dl_indication = receive_indication};
  ue->if_inst = &interface;
  ue->frame_parms.nb_antennas_rx = 1;
  ue->frame_parms.N_RB_DL = carrier_n_rb;
  ue->frame_parms.ofdm_symbol_size = fft_size;
  ue->frame_parms.symbols_per_slot = symbols_per_slot;
  ue->frame_parms.samples_per_slot_wCP = samples_per_slot;

  // Rows 1 and 2 both use one antenna port (TS 38.211 Table 7.4.1.5.3-1).
  c16_t generated[samples_per_slot] = {0};
  c16_t *generated_ports[] = {generated};
  nr_csi_info_t csi_info = {
      .csi_rs_generated_signal = generated_ports,
      .csi_im_meas_computed = test->use_csi_im,
      .interference_plus_noise_power = 17,
  };
  ue->nr_csi_info = &csi_info;
  c16_t rxdataF[1][samples_per_slot];
  for (int i = 0; i < samples_per_slot; i++)
    rxdataF[0][i] = (c16_t){100, (i & 1) ? 50 : -50};
  c16_t trs_estimates[1][1][fft_size];
  memset(trs_estimates, 0x5a, sizeof(trs_estimates));
  c16_t original_estimates[1][1][fft_size];
  memcpy(original_estimates, trs_estimates, sizeof(trs_estimates));

  fapi_nr_dl_config_csirs_pdu_rel15_t pdu = {
      .start_rb = test->start_rb,
      .nr_of_rbs = test->nr_of_rbs,
      .csi_type = test->tracking ? 0 : 1,
      .row = test->row,
      .freq_domain = 1,
      .symb_l0 = 4,
      .freq_density = test->density,
      .power_control_offset_ss = 1,
      .measurement_bitmap = test->measurements,
  };
  const UE_nr_rxtx_proc_t proc = {0};
  measurement_count = 0;
  indication_count = 0;
  memset(&last_measurement, 0xff, sizeof(last_measurement));
  const bool measured = nr_ue_csi_rs_procedures(ue, &proc, rxdataF, &pdu, trs_estimates, 0, -1);
  const int expected_reports = test->expected_success && !test->tracking;
  bool failed = measured != test->expected_success || measurement_count != expected_reports || indication_count != expected_reports;
  if (expected_reports) {
    // 12500 in Q30, normalized by a 1024-point FFT, is about -49.4 dBm at zero RX gain.
    failed |= last_measurement.meas_type != NFAPI_NR_CSI_MEAS || last_measurement.rsrp_dBm < -50 || last_measurement.rsrp_dBm > -48;
  }
  if (!test->expected_success)
    failed |= memcmp(trs_estimates, original_estimates, sizeof(trs_estimates)) != 0;
  else if (test->tracking)
    failed |= memcmp(trs_estimates, original_estimates, sizeof(trs_estimates)) == 0;
  printf("%s: %s, measured=%d, reports=%d\n", failed ? "FAIL" : "PASS", test->name, measured, indication_count);
  free(ue);
  return failed;
}

static int check_half_density_generation(void)
{
  enum {
    carrier_n_rb = 64,
    fft_size = 1024,
    symbols_per_slot = 14,
    samples_per_slot = symbols_per_slot * fft_size,
    symbol = 4,
  };
  const NR_DL_FRAME_PARMS fp = {
      .N_RB_DL = carrier_n_rb,
      .ofdm_symbol_size = fft_size,
      .symbols_per_slot = symbols_per_slot,
  };
  const struct {
    uint16_t start_rb;
    uint16_t nr_of_rbs;
    uint8_t density;
    uint32_t minimum_bits[2]; // Rows 2 and 3, including the last occupied PRB.
  } tests[] = {
      {0, 1, 0, {2, 4}},
      {0, 20, 0, {20, 40}},
      {0, 21, 0, {22, 44}},
      {1, 20, 0, {22, 44}},
      {12, 9, 0, {22, 44}},
      {1, 1, 1, {2, 6}},
      {0, 20, 1, {20, 42}},
      {0, 21, 1, {20, 42}},
  };
  int failures = 0;
  for (int row = 2; row <= 3; row++) {
    const csi_mapping_parms_t mapping = get_csi_mapping_parms(row, 1, symbol, 0);
    const int cdm_type = row == 2 ? 0 : 1;
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
      c16_t reference[2][samples_per_slot] = {0};
      c16_t generated[2][samples_per_slot] = {0};
      c16_t *reference_ports[] = {reference[0], reference[1]};
      c16_t *generated_ports[] = {generated[0], generated[1]};
      // A full carrier ending on an odd PRB supplies a reference for either density.
      nr_generate_csi_rs(&fp,
                         &mapping,
                         AMP,
                         0,
                         tests[i].density,
                         0,
                         carrier_n_rb,
                         symbol,
                         0,
                         row,
                         123,
                         1,
                         cdm_type,
                         reference_ports);
      last_modulation_length = 0;
      nr_generate_csi_rs(&fp,
                         &mapping,
                         AMP,
                         0,
                         tests[i].density,
                         tests[i].start_rb,
                         tests[i].nr_of_rbs,
                         symbol,
                         0,
                         row,
                         123,
                         1,
                         cdm_type,
                         generated_ports);
      // Check the real modulator request so stale stack samples cannot hide under-generation.
      bool failed = last_modulation_length < tests[i].minimum_bits[row - 2];
      if (!failed) {
        for (int rb = tests[i].start_rb; rb < tests[i].start_rb + tests[i].nr_of_rbs; rb++) {
          if (rb % 2 != tests[i].density)
            continue;
          for (int port = 0; port < mapping.ports; port++) {
            for (int kp = 0; kp <= mapping.kprime; kp++) {
              const int k = symbol * fft_size + rb * NR_NB_SC_PER_RB + kp;
              failed |= reference[port][k].r == 0 || reference[port][k].i == 0 || generated[port][k].r != reference[port][k].r
                        || generated[port][k].i != reference[port][k].i;
            }
          }
        }
      }
      printf("%s: row %d half-density %u, RBs %u+%u, modulation bits %u (minimum %u)\n",
             failed ? "FAIL" : "PASS",
             row,
             tests[i].density,
             tests[i].start_rb,
             tests[i].nr_of_rbs,
             last_modulation_length,
             tests[i].minimum_bits[row - 2]);
      failures += failed;
    }
  }
  return failures;
}

int main(int argc, char **argv)
{
  const csi_rs_test_case_t tests[] = {
      {"20-RB RSRP", 0, 20, 2, 2, 1, false, true, false},
      {"20-RB channel estimation", 0, 20, 2, 2, 26, false, true, false},
      {"21-RB effective bandwidth", 0, 21, 2, 2, 1, false, true, false},
      {"20-RB nonzero start", 12, 20, 2, 2, 1, false, true, false},
      {"20-RB even density", 0, 20, 2, 0, 1, false, true, false},
      {"20-RB odd density", 0, 20, 2, 1, 1, false, true, false},
      {"21-RB even density channel estimation", 0, 21, 2, 0, 26, false, true, false},
      {"21-RB odd density channel estimation", 0, 21, 2, 1, 26, false, true, false},
      {"52-RB baseline", 0, 52, 2, 2, 1, false, true, false},
      {"one RB with three samples", 0, 1, 1, 3, 1, false, true, false},
      {"empty resource", 0, 0, 2, 2, 1, false, false, false},
      {"one RB with no CSI-RS samples", 0, 1, 2, 1, 1, false, false, false},
      {"one-RB RSRP without noise samples", 0, 1, 2, 2, 1, false, true, false},
      {"one-RB PMI without noise samples", 0, 1, 2, 2, 26, false, false, false},
      {"one-RB PMI with CSI-IM noise", 0, 1, 2, 2, 26, false, true, true},
      {"one-RB even density PMI with CSI-IM noise", 0, 1, 2, 0, 26, false, true, true},
      {"20-RB first tracking estimate", 0, 20, 1, 3, 0, true, true, false},
      {"failed first tracking estimate", 0, 0, 1, 3, 0, true, false, false},
  };
  if (argc > 2) {
    fprintf(stderr, "Usage: %s [test name]\n", argv[0]);
    return EXIT_FAILURE;
  }
  logInit();
  nr_generate_modulation_table();
  int failures = 0;
  int selected = 0;
  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    if (argc == 2 && strcmp(argv[1], tests[i].name) != 0)
      continue;
    selected++;
    failures += check_resource(&tests[i]);
  }
  if (argc == 1 || strcmp(argv[1], "half-density generation") == 0) {
    selected++;
    failures += check_half_density_generation();
  }
  logTerm();
  if (selected == 0) {
    fprintf(stderr, "Unknown test: %s\n", argv[1]);
    return EXIT_FAILURE;
  }
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
