/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "openair1/PHY/defs_nr_common.h"
#include "openair1/PHY/defs_nr_UE.h"
#include "openair1/SCHED_NR_UE/defs.h"
#include "openair1/PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "openair1/PHY/nr_phy_common/inc/nr_phy_common_csi_rs.h"
#include "radio/COMMON/common_lib.h"

// Stubs for external dependencies of csi_rx.c
openair0_config_t openair0_cfg_g[MAX_CARDS];
int cpu_meas_enabled = 0;
void insert_in_time_stats_sorted_list(time_stats_sorted_list_t *list, oai_cputime_t time)
{
}
void trs_freq_correction(PHY_VARS_NR_UE *ue, int cfo)
{
}
void nr_fill_rx_indication(fapi_nr_rx_indication_t *rx_ind,
                           uint8_t PDU_type,
                           PHY_VARS_NR_UE *ue,
                           int pdu_index,
                           int harq_pid,
                           NR_UE_DLSCH_t *dlsch,
                           const UE_nr_rxtx_proc_t *proc,
                           void *meas)
{
}
void nr_a_sum_b(c16_t *input_x, c16_t *input_y, unsigned short nb_rb)
{
}
int8_t dB_fixed(uint32_t x)
{
  return 0;
}
uint8_t log2_approx(uint32_t x)
{
  return 15;
}
csi_mapping_parms_t get_csi_mapping_parms(int row, int freq_domain, int symb_l0, int symb_l1)
{
  csi_mapping_parms_t m = {0};
  m.ports = 1;
  return m;
}
int get_cdm_group_size(int cdm_type)
{
  return 1;
}
void nr_generate_csi_rs(const NR_DL_FRAME_PARMS *frame_parms,
                        const csi_mapping_parms_t *phy_csi_parms,
                        const int16_t amp,
                        const int slot,
                        const uint8_t freq_density,
                        const uint16_t start_rb,
                        const uint16_t nr_of_rbs,
                        const uint8_t symb_l0,
                        const uint8_t symb_l1,
                        const uint8_t row,
                        const uint16_t scramb_id,
                        const uint8_t power_control_offset_ss,
                        const uint8_t cdm_type,
                        c16_t **dataF)
{
}

#include "openair1/PHY/NR_UE_TRANSPORT/csi_rx.c"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(cond, msg)                                         \
  do {                                                                 \
    if (!(cond)) {                                                     \
      printf("  FAILED: %s (line %d): %s\n", __func__, __LINE__, msg); \
      return -1;                                                       \
    }                                                                  \
  } while (0)

#define RUN_TEST(test_func)                    \
  do {                                         \
    tests_run++;                               \
    printf("[ RUN      ] %s\n", #test_func);   \
    if (test_func() == 0) {                    \
      printf("[       OK ] %s\n", #test_func); \
      tests_passed++;                          \
    } else {                                   \
      printf("[  FAILED  ] %s\n", #test_func); \
    }                                          \
  } while (0)

typedef struct {
  NR_DL_FRAME_PARMS fp;
  csi_mapping_parms_t mapping;
  nr_csi_info_t csi_info;
  c16_t tx_signal[14 * 1536];
  const c16_t *tx_ptr[1];
  c16_t rx_signal[1][14 * 1536];
  c16_t ls_est[1][1][1536];
  c16_t freq_est[1][1][1536];
  int16_t log2_maxh;
  fapi_nr_dl_config_csirs_pdu_rel15_t pdu;
} test_fixture_t;

static void setup_fixture(test_fixture_t *f)
{
  memset(f, 0, sizeof(*f));
  f->fp.N_RB_DL = 106;
  f->fp.ofdm_symbol_size = 1536;
  f->fp.symbols_per_slot = 14;
  f->fp.samples_per_slot_wCP = 14 * 1536;
  f->fp.nb_antennas_rx = 1;

  f->mapping.ports = 1;
  f->mapping.size = 1;
  f->mapping.kprime = 0;
  f->mapping.lprime = 0;
  f->mapping.koverline[0] = 0;
  f->mapping.loverline[0] = 0;
  f->mapping.j[0] = 0;

  f->csi_info.csi_rs_generated_signal_bits = 15;

  for (int i = 0; i < 14 * 1536; i++) {
    f->tx_signal[i].r = 1024;
    f->tx_signal[i].i = 0;
    f->rx_signal[0][i].r = 1024;
    f->rx_signal[0][i].i = 0;
  }
  f->tx_ptr[0] = f->tx_signal;

  f->pdu.csi_type = 1; // NZP-CSI-RS
  f->pdu.row = 1;
  f->pdu.symb_l0 = 0;
  f->pdu.measurement_bitmap = 0;
  f->pdu.freq_density = 2; // density 1 (every RB)
}

// 1. Valid allocation within bandwidth
static int test_valid_allocation_within_bandwidth(void)
{
  test_fixture_t f;
  setup_fixture(&f);
  f.pdu.start_rb = 0;
  f.pdu.nr_of_rbs = 24;

  int ret = nr_csi_rs_channel_estimation(&f.fp,
                                         &f.pdu,
                                         &f.csi_info,
                                         f.tx_ptr,
                                         f.rx_signal,
                                         &f.mapping,
                                         1,
                                         f.ls_est,
                                         f.freq_est,
                                         &f.log2_maxh);
  TEST_ASSERT(ret == 0, "Estimation failed for valid allocation");
  TEST_ASSERT(f.ls_est[0][0][0].r == 32, "LS estimate real component should be 32");
  TEST_ASSERT(f.ls_est[0][0][0].i == 0, "LS estimate imag component should be 0");
  TEST_ASSERT(f.freq_est[0][0][0].r == 32, "Interpolated estimate real component should be 32 (unity channel)");
  TEST_ASSERT(f.freq_est[0][0][0].i == 0, "Interpolated estimate imag component should be 0");
  return 0;
}

// 2. Resource beginning at the first valid resource block (rb = 0, k = 0)
static int test_valid_resource_at_first_rb(void)
{
  test_fixture_t f;
  setup_fixture(&f);
  f.pdu.start_rb = 0;
  f.pdu.nr_of_rbs = 48;

  int ret = nr_csi_rs_channel_estimation(&f.fp,
                                         &f.pdu,
                                         &f.csi_info,
                                         f.tx_ptr,
                                         f.rx_signal,
                                         &f.mapping,
                                         1,
                                         f.ls_est,
                                         f.freq_est,
                                         &f.log2_maxh);
  TEST_ASSERT(ret == 0, "Estimation failed at first RB");
  TEST_ASSERT(f.freq_est[0][0][0].r > 0, "Subcarrier 0 should be estimated");
  return 0;
}

// 3. Resource ending exactly at the last valid resource block (start_rb + nr_of_rbs == N_RB_DL)
static int test_valid_resource_at_last_rb(void)
{
  test_fixture_t f;
  setup_fixture(&f);
  f.pdu.start_rb = 82;
  f.pdu.nr_of_rbs = 24; // 82 + 24 = 106 == N_RB_DL

  int ret = nr_csi_rs_channel_estimation(&f.fp,
                                         &f.pdu,
                                         &f.csi_info,
                                         f.tx_ptr,
                                         f.rx_signal,
                                         &f.mapping,
                                         1,
                                         f.ls_est,
                                         f.freq_est,
                                         &f.log2_maxh);
  TEST_ASSERT(ret == 0, "Estimation failed at last RB boundary");
  TEST_ASSERT(f.freq_est[0][0][82 * 12].r > 0, "Subcarrier 82*12 should be estimated");
  return 0;
}

// 4. Valid single RB resource allocation
static int test_valid_single_rb(void)
{
  test_fixture_t f;
  setup_fixture(&f);
  f.pdu.start_rb = 0;
  f.pdu.nr_of_rbs = 1;
  int ret = nr_csi_rs_channel_estimation(&f.fp,
                                         &f.pdu,
                                         &f.csi_info,
                                         f.tx_ptr,
                                         f.rx_signal,
                                         &f.mapping,
                                         1,
                                         f.ls_est,
                                         f.freq_est,
                                         &f.log2_maxh);
  TEST_ASSERT(ret == 0, "Single RB at start failed");

  f.pdu.start_rb = 50;
  f.pdu.nr_of_rbs = 1;
  ret = nr_csi_rs_channel_estimation(&f.fp,
                                     &f.pdu,
                                     &f.csi_info,
                                     f.tx_ptr,
                                     f.rx_signal,
                                     &f.mapping,
                                     1,
                                     f.ls_est,
                                     f.freq_est,
                                     &f.log2_maxh);
  TEST_ASSERT(ret == 0, "Single RB in middle failed");
  return 0;
}

// 5. Full bandwidth valid allocation (0..106)
static int test_valid_full_bandwidth(void)
{
  test_fixture_t f;
  setup_fixture(&f);
  f.pdu.start_rb = 0;
  f.pdu.nr_of_rbs = 106;

  int ret = nr_csi_rs_channel_estimation(&f.fp,
                                         &f.pdu,
                                         &f.csi_info,
                                         f.tx_ptr,
                                         f.rx_signal,
                                         &f.mapping,
                                         1,
                                         f.ls_est,
                                         f.freq_est,
                                         &f.log2_maxh);
  TEST_ASSERT(ret == 0, "Full bandwidth estimation failed");
  return 0;
}

// 6. Invalid: start_rb equals N_RB_DL (boundary violation)
static int test_invalid_start_rb_equals_bandwidth(void)
{
  test_fixture_t f;
  setup_fixture(&f);
  f.pdu.start_rb = 106;
  f.pdu.nr_of_rbs = 1;

  int ret = nr_csi_rs_channel_estimation(&f.fp,
                                         &f.pdu,
                                         &f.csi_info,
                                         f.tx_ptr,
                                         f.rx_signal,
                                         &f.mapping,
                                         1,
                                         f.ls_est,
                                         f.freq_est,
                                         &f.log2_maxh);
  TEST_ASSERT(ret == -1, "start_rb == N_RB_DL should be rejected");
  return 0;
}

// 7. Invalid: start_rb strictly greater than N_RB_DL
static int test_invalid_start_rb_greater_than_bandwidth(void)
{
  test_fixture_t f;
  setup_fixture(&f);
  f.pdu.start_rb = 150;
  f.pdu.nr_of_rbs = 24;

  int ret = nr_csi_rs_channel_estimation(&f.fp,
                                         &f.pdu,
                                         &f.csi_info,
                                         f.tx_ptr,
                                         f.rx_signal,
                                         &f.mapping,
                                         1,
                                         f.ls_est,
                                         f.freq_est,
                                         &f.log2_maxh);
  TEST_ASSERT(ret == -1, "start_rb > N_RB_DL should be rejected");
  return 0;
}

// 8. Invalid: start_rb + nr_of_rbs exceeds N_RB_DL
static int test_invalid_span_exceeds_bandwidth(void)
{
  test_fixture_t f;
  setup_fixture(&f);
  f.pdu.start_rb = 90;
  f.pdu.nr_of_rbs = 24; // 90 + 24 = 114 > 106

  int ret = nr_csi_rs_channel_estimation(&f.fp,
                                         &f.pdu,
                                         &f.csi_info,
                                         f.tx_ptr,
                                         f.rx_signal,
                                         &f.mapping,
                                         1,
                                         f.ls_est,
                                         f.freq_est,
                                         &f.log2_maxh);
  TEST_ASSERT(ret == -1, "Span exceeding N_RB_DL should be rejected");
  return 0;
}

// 9. Invalid: Issue #799 specific reproduction configuration (start_rb=100, nr_of_rbs=50, ofdm_symbol_size=1536)
static int test_invalid_issue_799_reproduction(void)
{
  test_fixture_t f;
  setup_fixture(&f);
  f.pdu.start_rb = 100;
  f.pdu.nr_of_rbs = 50; // 150 * 12 = 1800 > ofdm_symbol_size 1536

  int ret = nr_csi_rs_channel_estimation(&f.fp,
                                         &f.pdu,
                                         &f.csi_info,
                                         f.tx_ptr,
                                         f.rx_signal,
                                         &f.mapping,
                                         1,
                                         f.ls_est,
                                         f.freq_est,
                                         &f.log2_maxh);
  TEST_ASSERT(ret == -1, "Issue #799 configuration must be safely rejected");
  return 0;
}

// 10. Invalid: Zero resource blocks
static int test_invalid_zero_rbs(void)
{
  test_fixture_t f;
  setup_fixture(&f);
  f.pdu.start_rb = 0;
  f.pdu.nr_of_rbs = 0;

  int ret = nr_csi_rs_channel_estimation(&f.fp,
                                         &f.pdu,
                                         &f.csi_info,
                                         f.tx_ptr,
                                         f.rx_signal,
                                         &f.mapping,
                                         1,
                                         f.ls_est,
                                         f.freq_est,
                                         &f.log2_maxh);
  TEST_ASSERT(ret == -1, "nr_of_rbs == 0 should be rejected");
  return 0;
}

// 11. Invalid: Arithmetic overflow in uint16_t (start_rb + nr_of_rbs wraps around)
static int test_invalid_uint16_overflow(void)
{
  test_fixture_t f;
  setup_fixture(&f);
  f.pdu.start_rb = 65530;
  f.pdu.nr_of_rbs = 100; // 65530 + 100 wraps to 94 in uint16_t

  int ret = nr_csi_rs_channel_estimation(&f.fp,
                                         &f.pdu,
                                         &f.csi_info,
                                         f.tx_ptr,
                                         f.rx_signal,
                                         &f.mapping,
                                         1,
                                         f.ls_est,
                                         f.freq_est,
                                         &f.log2_maxh);
  TEST_ASSERT(ret == -1, "Wrapped uint16_t allocation must be rejected");
  return 0;
}

// 12. Valid configurations with different frequency densities
static int test_valid_frequency_densities(void)
{
  test_fixture_t f;
  setup_fixture(&f);
  f.pdu.start_rb = 0;
  f.pdu.nr_of_rbs = 24;

  // Density 0: dot5 even PRBs
  f.pdu.freq_density = 0;
  int ret = nr_csi_rs_channel_estimation(&f.fp,
                                         &f.pdu,
                                         &f.csi_info,
                                         f.tx_ptr,
                                         f.rx_signal,
                                         &f.mapping,
                                         1,
                                         f.ls_est,
                                         f.freq_est,
                                         &f.log2_maxh);
  TEST_ASSERT(ret == 0, "Density 0 failed");

  // Density 1: dot5 odd PRBs (skips rb 0, starts at rb 1, k = 12, k - 12 >= 0)
  f.pdu.freq_density = 1;
  ret = nr_csi_rs_channel_estimation(&f.fp,
                                     &f.pdu,
                                     &f.csi_info,
                                     f.tx_ptr,
                                     f.rx_signal,
                                     &f.mapping,
                                     1,
                                     f.ls_est,
                                     f.freq_est,
                                     &f.log2_maxh);
  TEST_ASSERT(ret == 0, "Density 1 failed");

  // Density 2: 1 PRB
  f.pdu.freq_density = 2;
  ret = nr_csi_rs_channel_estimation(&f.fp,
                                     &f.pdu,
                                     &f.csi_info,
                                     f.tx_ptr,
                                     f.rx_signal,
                                     &f.mapping,
                                     1,
                                     f.ls_est,
                                     f.freq_est,
                                     &f.log2_maxh);
  TEST_ASSERT(ret == 0, "Density 2 failed");

  // Density 3: 3 PRBs
  f.pdu.freq_density = 3;
  ret = nr_csi_rs_channel_estimation(&f.fp,
                                     &f.pdu,
                                     &f.csi_info,
                                     f.tx_ptr,
                                     f.rx_signal,
                                     &f.mapping,
                                     1,
                                     f.ls_est,
                                     f.freq_est,
                                     &f.log2_maxh);
  TEST_ASSERT(ret == 0, "Density 3 failed");
  return 0;
}

// 13. Valid multiple antenna ports and CDM
static int test_valid_multiple_ports(void)
{
  test_fixture_t f;
  setup_fixture(&f);

  f.mapping.ports = 2;
  f.mapping.size = 2;
  f.mapping.kprime = 1;
  f.mapping.koverline[0] = 0;
  f.mapping.koverline[1] = 2;
  f.mapping.j[0] = 0;
  f.mapping.j[1] = 0;

  c16_t tx_multi[2][14 * 1536];
  memset(tx_multi, 0, sizeof(tx_multi));
  for (int p = 0; p < 2; p++) {
    for (int i = 0; i < 14 * 1536; i++) {
      tx_multi[p][i].r = 1024;
    }
  }
  const c16_t *tx_multi_ptr[2] = {tx_multi[0], tx_multi[1]};
  c16_t ls_multi[1][2][1536];
  c16_t freq_multi[1][2][1536];
  memset(ls_multi, 0, sizeof(ls_multi));
  memset(freq_multi, 0, sizeof(freq_multi));

  f.pdu.start_rb = 10;
  f.pdu.nr_of_rbs = 24;
  f.pdu.freq_density = 2;

  int ret = nr_csi_rs_channel_estimation(&f.fp,
                                         &f.pdu,
                                         &f.csi_info,
                                         tx_multi_ptr,
                                         f.rx_signal,
                                         &f.mapping,
                                         2,
                                         ls_multi,
                                         freq_multi,
                                         &f.log2_maxh);
  TEST_ASSERT(ret == 0, "Multiple ports estimation failed");
  TEST_ASSERT(freq_multi[0][0][10 * 12].r > 0, "Port 0 channel estimate missing");
  TEST_ASSERT(freq_multi[0][1][10 * 12].r > 0, "Port 1 channel estimate missing");
  return 0;
}

// 14. Validation rejection in nr_ue_csi_rs_procedures
static int test_procedures_early_rejection(void)
{
  test_fixture_t f;
  setup_fixture(&f);

  PHY_VARS_NR_UE ue;
  memset(&ue, 0, sizeof(ue));
  ue.frame_parms = f.fp;
  UE_nr_rxtx_proc_t proc;
  memset(&proc, 0, sizeof(proc));

  f.pdu.start_rb = 100;
  f.pdu.nr_of_rbs = 50;

  c16_t trs_estimates[1][1][1536];
  memset(trs_estimates, 0, sizeof(trs_estimates));

  // Should return early safely without accessing uninitialized buffers or crashing
  nr_ue_csi_rs_procedures(&ue, &proc, f.rx_signal, &f.pdu, trs_estimates, 0, 0);
  return 0;
}

int main(int argc, char **argv)
{
  logInit();
  printf("Running NR UE CSI-RS channel estimation tests...\n");

  RUN_TEST(test_valid_allocation_within_bandwidth);
  RUN_TEST(test_valid_resource_at_first_rb);
  RUN_TEST(test_valid_resource_at_last_rb);
  RUN_TEST(test_valid_single_rb);
  RUN_TEST(test_valid_full_bandwidth);
  RUN_TEST(test_invalid_start_rb_equals_bandwidth);
  RUN_TEST(test_invalid_start_rb_greater_than_bandwidth);
  RUN_TEST(test_invalid_span_exceeds_bandwidth);
  RUN_TEST(test_invalid_issue_799_reproduction);
  RUN_TEST(test_invalid_zero_rbs);
  RUN_TEST(test_invalid_uint16_overflow);
  RUN_TEST(test_valid_frequency_densities);
  RUN_TEST(test_valid_multiple_ports);
  RUN_TEST(test_procedures_early_rejection);

  printf("Test Summary: %d / %d tests passed.\n", tests_passed, tests_run);
  return (tests_passed == tests_run) ? 0 : 1;
}
