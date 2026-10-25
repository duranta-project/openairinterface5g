/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "gtest/gtest.h"
extern "C" {
#include "openair2/LAYER2/NR_MAC_UE/mac_proto.h"
#include "executables/softmodem-common.h"
static softmodem_params_t softmodem_params;
softmodem_params_t* get_softmodem_params(void)
{
  return &softmodem_params;
}
}
#include <cstdio>
#include "common/utils/LOG/log.h"

TEST(test_pcmax, test_mpr)
{
  // Inner PRB, MPR = 1.5, no delta MPR
  int prb_start = 4;
  int N_RB_UL = 51; // 10Mhz
  int nr_band = 20;
  float expected_power = 23 - (1.5 / 2);
  frame_type_t frame_type = TDD;
  int channel_bandwidth = 20;
  EXPECT_EQ(expected_power,
            nr_get_Pcmax(23, nr_band, frame_type, FR1, channel_bandwidth, 2, false, 1, N_RB_UL, false, 6, prb_start));

  // Outer PRB, MPR = 3, no delta MPR
  prb_start = 0;
  expected_power = 23 - (3.0 / 2);
  EXPECT_EQ(expected_power,
            nr_get_Pcmax(23, nr_band, frame_type, FR1, channel_bandwidth, 2, false, 1, N_RB_UL, false, 6, prb_start));

  // Outer PRB on band 28, MPR = 3, delta MPR = 0.5 dB
  N_RB_UL = 78;
  nr_band = 28;
  expected_power = 23 - ((3.0 + 0.5) / 2);
  EXPECT_EQ(expected_power, nr_get_Pcmax(23, nr_band, frame_type, FR1, 30, 2, false, 1, N_RB_UL, false, 100, prb_start));
}

TEST(test_pcmax, test_not_implemented)
{
  int N_RB_UL = 51;
  EXPECT_DEATH(nr_get_Pcmax(23, 20, TDD, FR1, 20, 1, false, 1, N_RB_UL, false, 6, 0), "MPR for Pi/2 BPSK not implemented yet");
}

TEST(test_pcmax, test_pucch_max_power)
{
  // Format 2, transform precoding, MPR = 1
  int prb_start = 0;
  int N_RB_UL = 51; // 10Mhz
  float expected_power = 23 - (1.0 / 2);
  int channel_bandwidth = 20;
  EXPECT_EQ(expected_power, nr_get_Pcmax(23, 20, TDD, FR1, channel_bandwidth, 2, false, 1, N_RB_UL, true, 1, prb_start));

  // Other fromats, no transform precoding, MPR = 3
  expected_power = 23 - (3.0 / 2);
  EXPECT_EQ(expected_power, nr_get_Pcmax(23, 20, TDD, FR1, channel_bandwidth, 2, false, 1, N_RB_UL, false, 1, prb_start));
}

TEST(test_pucch_power_state, test_accumulated_delta_pucch)
{
  NR_UE_MAC_INST_t mac = {0};
  mac.p_Max = 23;
  NR_UE_UL_BWP_t current_UL_BWP = {0};
  current_UL_BWP.scs = 1;
  current_UL_BWP.BWPSize = 106;
  NR_PUCCH_ConfigCommon_t pucch_ConfigCommon = {0};
  mac.current_UL_BWP = &current_UL_BWP;
  mac.current_UL_BWP->pucch_ConfigCommon = &pucch_ConfigCommon;
  mac.nr_band = 20;
  NR_PUCCH_Config_t pucch_Config = {0};
  struct NR_PUCCH_PowerControl power_config;
  pucch_Config.pucch_PowerControl = &power_config;
  mac.G_b_f_c = 0;
  mac.pucch_power_control_initialized = true;
  mac.frame_structure.frame_type = TDD;

  int scs = 1;
  int sum_delta_pucch = 3;
  uint8_t format_type = 1;
  uint16_t nb_of_prbs = 1;
  uint8_t freq_hop_flag = 0;
  uint8_t add_dmrs_flag = 0;
  uint8_t N_symb_PUCCH = 12;
  int subframe_number = 0;
  int O_uci = 2;
  uint16_t start_prb = 0;
  int P_CMAX = nr_get_Pcmax(23,
                            mac.nr_band,
                            mac.frame_structure.frame_type,
                            FR1,
                            current_UL_BWP.channel_bandwidth,
                            2,
                            false,
                            current_UL_BWP.scs,
                            current_UL_BWP.BWPSize,
                            false,
                            nb_of_prbs,
                            start_prb);
  int pucch_power_prev = get_pucch_tx_power_ue(&mac,
                                               scs,
                                               &pucch_Config,
                                               sum_delta_pucch,
                                               format_type,
                                               nb_of_prbs,
                                               freq_hop_flag,
                                               add_dmrs_flag,
                                               N_symb_PUCCH,
                                               subframe_number,
                                               O_uci,
                                               start_prb);
  EXPECT_LT(pucch_power_prev, P_CMAX);
  for (int i = 0; i < 10; i++) {
    int pucch_power_state = mac.G_b_f_c;
    int pucch_power = get_pucch_tx_power_ue(&mac,
                                            scs,
                                            &pucch_Config,
                                            sum_delta_pucch,
                                            format_type,
                                            nb_of_prbs,
                                            freq_hop_flag,
                                            add_dmrs_flag,
                                            N_symb_PUCCH,
                                            subframe_number,
                                            O_uci,
                                            start_prb);
    if (pucch_power_prev == P_CMAX) {
      EXPECT_EQ(pucch_power_state, mac.G_b_f_c) << "PUCCH power control state increased after reaching max TX power";
    }
    EXPECT_LE(pucch_power, P_CMAX) << "PUUCH TX power above P_CMAX";
    EXPECT_EQ(std::min(P_CMAX, pucch_power_prev + sum_delta_pucch), pucch_power)
        << "PUCCH power expected to change by delta pucch only, between P_CMAX and P_CMIN";
    pucch_power_prev = pucch_power;

    if (i > 5) {
      EXPECT_EQ(pucch_power, P_CMAX) << "Expected to reach MAX PUCCH TX power";
    }
  }

  sum_delta_pucch = -15;
  for (int i = 0; i < 10; i++) {
    int pucch_power_state = mac.G_b_f_c;
    int pucch_power = get_pucch_tx_power_ue(&mac,
                                            scs,
                                            &pucch_Config,
                                            sum_delta_pucch,
                                            format_type,
                                            nb_of_prbs,
                                            freq_hop_flag,
                                            add_dmrs_flag,
                                            N_symb_PUCCH,
                                            subframe_number,
                                            O_uci,
                                            start_prb);
    EXPECT_LE(mac.G_b_f_c, pucch_power_state) << "PUCCH power control state increased with negative delta pucch";
    pucch_power_prev = pucch_power;
  }
}

TEST(pc_min, check_all_bw_indexes)
{
  const int bws[] = {5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 60, 70, 80, 90, 100};
  for (auto i = 0U; i < sizeofArray(bws); i++) {
    (void)nr_get_Pcmin(i);
  }
}

TEST(pusch_power_control, pusch_power_control_msg3)
{
  NR_UE_MAC_INST_t mac = {0};
  mac.p_Max = 23;
  NR_UE_UL_BWP_t current_UL_BWP = {0};
  current_UL_BWP.scs = 1;
  current_UL_BWP.BWPSize = 106;
  current_UL_BWP.channel_bandwidth = 40;
  mac.current_UL_BWP = &current_UL_BWP;
  mac.nr_band = 78;
  NR_PUSCH_Config_t pusch_Config = {0};
  current_UL_BWP.pusch_Config = &pusch_Config;
  NR_PUSCH_PowerControl pusch_PowerControl = {0};
  pusch_Config.pusch_PowerControl = &pusch_PowerControl;
  pusch_PowerControl.tpc_Accumulation = (long*)1;
  mac.pusch_power_control_initialized = true;
  mac.frame_structure.frame_type = TDD;

  // msg3 cofiguration as in 5g_rfsimulator testcase
  int num_rb = 8;
  int start_prb = 0;
  uint16_t nb_symb_sch = 3;
  uint16_t nb_dmrs_prb = 12;
  uint16_t nb_ptrs_prb = 0;
  uint16_t Qm = 2;
  uint16_t R = 1570;
  uint16_t beta_offset_csi1 = 0;
  uint32_t sum_bits_in_codeblocks = 56;
  int delta_pusch = 0;
  bool is_rar_tx_retx = true;

  int P_CMAX = nr_get_Pcmax(23,
                            mac.nr_band,
                            mac.frame_structure.frame_type,
                            FR1,
                            current_UL_BWP.channel_bandwidth,
                            Qm,
                            false,
                            current_UL_BWP.scs,
                            current_UL_BWP.BWPSize,
                            false,
                            num_rb,
                            start_prb);

  int preambleReceivedTargetPower = -96;
  mac.ra.prach_resources.ra_preamble_rx_target_power = preambleReceivedTargetPower;

  int power = get_pusch_tx_power_ue(&mac,
                                    num_rb,
                                    start_prb,
                                    nb_symb_sch,
                                    nb_dmrs_prb,
                                    nb_ptrs_prb,
                                    Qm,
                                    R,
                                    beta_offset_csi1,
                                    sum_bits_in_codeblocks,
                                    delta_pusch,
                                    is_rar_tx_retx,
                                    false);
  EXPECT_EQ(power, -84);
  EXPECT_LT(power, P_CMAX);
  mac.ra.prach_resources.ra_preamble_rx_target_power -= 2;

  int reduced_power = get_pusch_tx_power_ue(&mac,
                                            num_rb,
                                            start_prb,
                                            nb_symb_sch,
                                            nb_dmrs_prb,
                                            nb_ptrs_prb,
                                            Qm,
                                            R,
                                            beta_offset_csi1,
                                            sum_bits_in_codeblocks,
                                            delta_pusch,
                                            is_rar_tx_retx,
                                            false);
  EXPECT_EQ(std::min(P_CMAX, power - 2), reduced_power) << "Incorrect handling of preambleReceivedTargetPower";
  EXPECT_LT(reduced_power, P_CMAX) << "Power above P_CMAX";

  delta_pusch = 4;
  int increased_power = get_pusch_tx_power_ue(&mac,
                                              num_rb,
                                              start_prb,
                                              nb_symb_sch,
                                              nb_dmrs_prb,
                                              nb_ptrs_prb,
                                              Qm,
                                              R,
                                              beta_offset_csi1,
                                              sum_bits_in_codeblocks,
                                              delta_pusch,
                                              is_rar_tx_retx,
                                              false);
  EXPECT_EQ(std::min(P_CMAX, reduced_power + delta_pusch), increased_power) << "delta_pusch should increase tx power";
  EXPECT_LT(increased_power, P_CMAX) << "Power above P_CMAX";
}

TEST(pusch_power_control, pusch_power_data)
{
  NR_UE_MAC_INST_t mac = {0};
  mac.p_Max = 23;
  NR_UE_UL_BWP_t current_UL_BWP = {0};
  current_UL_BWP.scs = 1;
  current_UL_BWP.BWPSize = 106;
  current_UL_BWP.channel_bandwidth = 40;
  mac.current_UL_BWP = &current_UL_BWP;
  NR_RACH_ConfigCommon_t nr_rach_ConfigCommon = {0};
  current_UL_BWP.rach_ConfigCommon = &nr_rach_ConfigCommon;
  mac.nr_band = 78;
  mac.frame_structure.frame_type = TDD;

  bool is_rar_tx_retx = false;
  int num_rb = 5;
  int start_prb = 0;
  uint16_t nb_symb_sch = 3;
  uint16_t nb_dmrs_prb = 6;
  uint16_t nb_ptrs_prb = 0;
  uint16_t Qm = 2;
  uint16_t R = 6790;
  uint16_t beta_offset_csi1 = 0;
  uint32_t sum_bits_in_codeblocks = 192;
  int delta_pusch = 4;
  bool transform_precoding = false;
  NR_PUSCH_Config_t pusch_Config = {0};
  current_UL_BWP.pusch_Config = &pusch_Config;
  NR_PUSCH_PowerControl pusch_PowerControl = {0};
  pusch_Config.pusch_PowerControl = &pusch_PowerControl;
  pusch_PowerControl.tpc_Accumulation = (long*)1;
  long p0_NominalWithGrant = 0;
  current_UL_BWP.p0_NominalWithGrant = &p0_NominalWithGrant;
  pusch_PowerControl.deltaMCS = (long*)1;

  int P_CMAX = nr_get_Pcmax(23,
                            mac.nr_band,
                            mac.frame_structure.frame_type,
                            FR1,
                            current_UL_BWP.channel_bandwidth,
                            Qm,
                            false,
                            current_UL_BWP.scs,
                            current_UL_BWP.BWPSize,
                            transform_precoding,
                            num_rb,
                            start_prb);

  int power = get_pusch_tx_power_ue(&mac,
                                    num_rb,
                                    start_prb,
                                    nb_symb_sch,
                                    nb_dmrs_prb,
                                    nb_ptrs_prb,
                                    Qm,
                                    R,
                                    beta_offset_csi1,
                                    sum_bits_in_codeblocks,
                                    delta_pusch,
                                    is_rar_tx_retx,
                                    transform_precoding);
  EXPECT_LE(power, P_CMAX);
  EXPECT_EQ(power, 18);

  const int BETA_OFFSET_CSI1_DEFAULT = 13;
  sum_bits_in_codeblocks = 0; // CSI-only
  power = get_pusch_tx_power_ue(&mac,
                                num_rb,
                                start_prb,
                                nb_symb_sch,
                                nb_dmrs_prb,
                                nb_ptrs_prb,
                                Qm,
                                R,
                                BETA_OFFSET_CSI1_DEFAULT,
                                sum_bits_in_codeblocks,
                                delta_pusch,
                                is_rar_tx_retx,
                                transform_precoding);
  EXPECT_EQ(power, P_CMAX) << "Expecting max tx power because of deltaMCS with CSI-only";
}

TEST(pusch_power_control, pusch_power_control_state_initialization)
{
  NR_UE_MAC_INST_t mac = {0};
  mac.p_Max = 23;
  NR_UE_UL_BWP_t current_UL_BWP = {0};
  current_UL_BWP.scs = 1;
  current_UL_BWP.BWPSize = 106;
  current_UL_BWP.channel_bandwidth = 40;
  mac.current_UL_BWP = &current_UL_BWP;
  mac.nr_band = 78;
  NR_PUSCH_Config_t pusch_Config = {0};
  current_UL_BWP.pusch_Config = &pusch_Config;
  NR_PUSCH_PowerControl pusch_PowerControl = {0};
  pusch_Config.pusch_PowerControl = &pusch_PowerControl;
  mac.pusch_power_control_initialized = false;

  // msg3 cofiguration as in 5g_rfsimulator testcase
  int num_rb = 8;
  int start_prb = 0;
  uint16_t nb_symb_sch = 3;
  uint16_t nb_dmrs_prb = 12;
  uint16_t nb_ptrs_prb = 0;
  uint16_t Qm = 2;
  uint16_t R = 1570;
  uint16_t beta_offset_csi1 = 0;
  uint32_t sum_bits_in_codeblocks = 56;
  int delta_pusch = 0;
  bool is_rar_tx_retx = true;
  int preambleReceivedTargetPower = -96;
  mac.ra.prach_resources.ra_preamble_rx_target_power = preambleReceivedTargetPower;

  get_pusch_tx_power_ue(&mac,
                        num_rb,
                        start_prb,
                        nb_symb_sch,
                        nb_dmrs_prb,
                        nb_ptrs_prb,
                        Qm,
                        R,
                        beta_offset_csi1,
                        sum_bits_in_codeblocks,
                        delta_pusch,
                        is_rar_tx_retx,
                        false);
  EXPECT_EQ(mac.pusch_power_control_initialized, true);
}

TEST(pusch_power_control, pusch_power_control_state)
{
  NR_UE_MAC_INST_t mac = {0};
  mac.p_Max = 23;
  NR_UE_UL_BWP_t current_UL_BWP = {0};
  current_UL_BWP.scs = 1;
  current_UL_BWP.BWPSize = 106;
  current_UL_BWP.channel_bandwidth = 40;
  mac.current_UL_BWP = &current_UL_BWP;
  NR_RACH_ConfigCommon_t nr_rach_ConfigCommon = {0};
  current_UL_BWP.rach_ConfigCommon = &nr_rach_ConfigCommon;
  mac.nr_band = 78;
  mac.f_b_f_c = 0;
  mac.pusch_power_control_initialized = true;

  bool is_rar_tx_retx = false;
  int num_rb = 5;
  int start_prb = 0;
  uint16_t nb_symb_sch = 3;
  uint16_t nb_dmrs_prb = 6;
  uint16_t nb_ptrs_prb = 0;
  uint16_t Qm = 2;
  uint16_t R = 6790;
  uint16_t beta_offset_csi1 = 0;
  uint32_t sum_bits_in_codeblocks = 192;
  int delta_pusch = 1;
  bool transform_precoding = false;
  NR_PUSCH_Config_t pusch_Config = {0};
  current_UL_BWP.pusch_Config = &pusch_Config;
  NR_PUSCH_PowerControl pusch_PowerControl = {0};
  pusch_Config.pusch_PowerControl = &pusch_PowerControl;
  long p0_NominalWithGrant = 0;
  current_UL_BWP.p0_NominalWithGrant = &p0_NominalWithGrant;
  mac.frame_structure.frame_type = TDD;

  int P_CMAX = nr_get_Pcmax(23,
                            mac.nr_band,
                            mac.frame_structure.frame_type,
                            FR1,
                            current_UL_BWP.channel_bandwidth,
                            Qm,
                            false,
                            current_UL_BWP.scs,
                            current_UL_BWP.BWPSize,
                            transform_precoding,
                            num_rb,
                            start_prb);

  int power = get_pusch_tx_power_ue(&mac,
                                    num_rb,
                                    start_prb,
                                    nb_symb_sch,
                                    nb_dmrs_prb,
                                    nb_ptrs_prb,
                                    Qm,
                                    R,
                                    beta_offset_csi1,
                                    sum_bits_in_codeblocks,
                                    delta_pusch,
                                    is_rar_tx_retx,
                                    transform_precoding);
  EXPECT_LE(power, P_CMAX);
  EXPECT_EQ(power, 11);
  for (int i = 0; i < 20; i++) {
    int increased_power = get_pusch_tx_power_ue(&mac,
                                                num_rb,
                                                start_prb,
                                                nb_symb_sch,
                                                nb_dmrs_prb,
                                                nb_ptrs_prb,
                                                Qm,
                                                R,
                                                beta_offset_csi1,
                                                sum_bits_in_codeblocks,
                                                delta_pusch,
                                                is_rar_tx_retx,
                                                transform_precoding);
    EXPECT_GE(increased_power, power);
    EXPECT_LE(increased_power, P_CMAX);
    power = increased_power;
  }

  delta_pusch = -1;
  for (int i = 0; i < 20; i++) {
    int reduced_power = get_pusch_tx_power_ue(&mac,
                                              num_rb,
                                              start_prb,
                                              nb_symb_sch,
                                              nb_dmrs_prb,
                                              nb_ptrs_prb,
                                              Qm,
                                              R,
                                              beta_offset_csi1,
                                              sum_bits_in_codeblocks,
                                              delta_pusch,
                                              is_rar_tx_retx,
                                              transform_precoding);
    EXPECT_LE(reduced_power, power);
    EXPECT_LE(reduced_power, P_CMAX);
    power = reduced_power;
  }
}

TEST(pusch_power_control, pusch_power_100_rb)
{
  NR_UE_MAC_INST_t mac = {0};
  mac.p_Max = 23;
  NR_UE_UL_BWP_t current_UL_BWP = {0};
  current_UL_BWP.scs = 1;
  current_UL_BWP.BWPSize = 106;
  mac.current_UL_BWP = &current_UL_BWP;
  NR_RACH_ConfigCommon_t nr_rach_ConfigCommon = {0};
  current_UL_BWP.rach_ConfigCommon = &nr_rach_ConfigCommon;
  mac.nr_band = 78;
  mac.f_b_f_c = 0;
  mac.pusch_power_control_initialized = true;

  bool is_rar_tx_retx = false;
  int num_rb = 5;
  int start_prb = 0;
  uint16_t nb_symb_sch = 3;
  uint16_t nb_dmrs_prb = 6;
  uint16_t nb_ptrs_prb = 0;
  uint16_t Qm = 2;
  uint16_t R = 6790;
  uint16_t beta_offset_csi1 = 0;
  uint32_t sum_bits_in_codeblocks = 192;
  int delta_pusch = 1;
  bool transform_precoding = false;
  NR_PUSCH_Config_t pusch_Config = {0};
  current_UL_BWP.pusch_Config = &pusch_Config;
  NR_PUSCH_PowerControl pusch_PowerControl = {0};
  pusch_Config.pusch_PowerControl = &pusch_PowerControl;
  long p0_NominalWithGrant = 0;
  current_UL_BWP.p0_NominalWithGrant = &p0_NominalWithGrant;
  int power = get_pusch_tx_power_ue(&mac,
                                    num_rb,
                                    start_prb,
                                    nb_symb_sch,
                                    nb_dmrs_prb,
                                    nb_ptrs_prb,
                                    Qm,
                                    R,
                                    beta_offset_csi1,
                                    sum_bits_in_codeblocks,
                                    delta_pusch,
                                    is_rar_tx_retx,
                                    transform_precoding);
  num_rb = 100;
  sum_bits_in_codeblocks = nr_compute_tbs(Qm, R, num_rb, nb_symb_sch, nb_dmrs_prb, 0, 0, 1);

  int power_100_prbs = get_pusch_tx_power_ue(&mac,
                                             num_rb,
                                             start_prb,
                                             nb_symb_sch,
                                             nb_dmrs_prb,
                                             nb_ptrs_prb,
                                             Qm,
                                             R,
                                             beta_offset_csi1,
                                             sum_bits_in_codeblocks,
                                             delta_pusch,
                                             is_rar_tx_retx,
                                             transform_precoding);
  EXPECT_GT(power_100_prbs, power);
}

TEST(test_pcmax, test_non_obvious_bwp_size)
{
  // Inner PRB, MPR = 1.5, no delta MPR
  int prb_start = 4;
  int N_RB_UL = 48;
  int nr_band = 20;
  frame_type_t frame_type = TDD;
  int channel_bandwidth = 10;
  float expected_power = 23 - 1.5 / 2;
  EXPECT_EQ(expected_power,
            nr_get_Pcmax(23, nr_band, frame_type, FR1, channel_bandwidth, 2, false, 1, N_RB_UL, false, 6, prb_start));
}

TEST(test_srs_power, use_pusch_power_adjustment_state)
{
  NR_UE_MAC_INST_t mac = {0};
  mac.p_Max = 23;
  NR_UE_UL_BWP_t current_UL_BWP = {0};
  current_UL_BWP.scs = 1;
  current_UL_BWP.BWPSize = 106;
  mac.current_UL_BWP = &current_UL_BWP;
  NR_SRS_Resource_t srs_resource = {0};
  NR_SRS_ResourceSet_t srs_resource_set = {0};
  bool is_configured_for_pusch_on_current_bwp = true;
  int delta_srs = 0;
  srs_resource_set.srs_PowerControlAdjustmentStates = NULL;
  mac.nr_band = 78;
  mac.f_b_f_c = 0;
  mac.pusch_power_control_initialized = true;
  long p0 = 0;
  srs_resource_set.p0 = &p0;

  int tx_power = get_srs_tx_power_ue(&mac, &srs_resource, &srs_resource_set, delta_srs, is_configured_for_pusch_on_current_bwp);

  mac.f_b_f_c = 4;

  int increased_tx_power =
      get_srs_tx_power_ue(&mac, &srs_resource, &srs_resource_set, delta_srs, is_configured_for_pusch_on_current_bwp);
  EXPECT_EQ(tx_power + mac.f_b_f_c, increased_tx_power);
}

TEST(test_srs_power, no_support_for_two_pusch_power_adjustment_states)
{
  NR_UE_MAC_INST_t mac = {0};
  mac.p_Max = 23;
  NR_UE_UL_BWP_t current_UL_BWP = {0};
  current_UL_BWP.scs = 1;
  current_UL_BWP.BWPSize = 106;
  mac.current_UL_BWP = &current_UL_BWP;
  NR_SRS_Resource_t srs_resource = {0};
  NR_SRS_ResourceSet_t srs_resource_set = {0};
  long p0 = 0;
  srs_resource_set.p0 = &p0;
  bool is_configured_for_pusch_on_current_bwp = true;
  int delta_srs = 0;
  long srs_PowerControlAdjustmentStates = NR_SRS_ResourceSet__srs_PowerControlAdjustmentStates_sameAsFci2;
  srs_resource_set.srs_PowerControlAdjustmentStates = &srs_PowerControlAdjustmentStates;
  mac.nr_band = 78;
  mac.f_b_f_c = 0;
  mac.pusch_power_control_initialized = true;

  EXPECT_DEATH(get_srs_tx_power_ue(&mac, &srs_resource, &srs_resource_set, delta_srs, is_configured_for_pusch_on_current_bwp),
               "Two PUSCH power adjustment states not supported");
}

TEST(test_srs_power, no_tpc_accumulation)
{
  NR_UE_MAC_INST_t mac = {0};
  mac.p_Max = 23;
  NR_UE_UL_BWP_t current_UL_BWP = {0};
  current_UL_BWP.scs = 1;
  current_UL_BWP.BWPSize = 106;
  mac.current_UL_BWP = &current_UL_BWP;
  NR_SRS_Resource_t srs_resource = {0};
  NR_SRS_ResourceSet_t srs_resource_set = {0};
  long p0 = 0;
  srs_resource_set.p0 = &p0;
  bool is_configured_for_pusch_on_current_bwp = true;
  int delta_srs = 0;
  long srs_PowerControlAdjustmentStates = NR_SRS_ResourceSet__srs_PowerControlAdjustmentStates_separateClosedLoop;
  srs_resource_set.srs_PowerControlAdjustmentStates = &srs_PowerControlAdjustmentStates;
  mac.nr_band = 78;
  mac.f_b_f_c = 0;
  mac.pusch_power_control_initialized = true;

  NR_SRS_Config_t srs_Config = {0};
  long tpc_Accumulation = 0;
  srs_Config.tpc_Accumulation = &tpc_Accumulation;
  current_UL_BWP.srs_Config = &srs_Config;

  int tx_power = get_srs_tx_power_ue(&mac, &srs_resource, &srs_resource_set, delta_srs, is_configured_for_pusch_on_current_bwp);

  delta_srs = 4;

  int increased_tx_power =
      get_srs_tx_power_ue(&mac, &srs_resource, &srs_resource_set, delta_srs, is_configured_for_pusch_on_current_bwp);
  EXPECT_EQ(tx_power + delta_srs, increased_tx_power);
}

TEST(test_srs_power, tpc_accumulation)
{
  NR_UE_MAC_INST_t mac = {0};
  mac.p_Max = 23;
  NR_UE_UL_BWP_t current_UL_BWP = {0};
  current_UL_BWP.scs = 1;
  current_UL_BWP.BWPSize = 106;
  mac.current_UL_BWP = &current_UL_BWP;
  NR_SRS_Resource_t srs_resource = {0};
  NR_SRS_ResourceSet_t srs_resource_set = {0};
  long p0 = 0;
  srs_resource_set.p0 = &p0;
  bool is_configured_for_pusch_on_current_bwp = true;
  int delta_srs = 0;
  long srs_PowerControlAdjustmentStates = NR_SRS_ResourceSet__srs_PowerControlAdjustmentStates_separateClosedLoop;
  srs_resource_set.srs_PowerControlAdjustmentStates = &srs_PowerControlAdjustmentStates;
  mac.nr_band = 78;
  mac.f_b_f_c = 0;
  mac.pusch_power_control_initialized = true;
  current_UL_BWP.srs_power_control_initialized = true;

  NR_SRS_Config_t srs_Config = {0};
  current_UL_BWP.srs_Config = &srs_Config;

  int tx_power = get_srs_tx_power_ue(&mac, &srs_resource, &srs_resource_set, delta_srs, is_configured_for_pusch_on_current_bwp);

  delta_srs = 4;

  int increased_tx_power =
      get_srs_tx_power_ue(&mac, &srs_resource, &srs_resource_set, delta_srs, is_configured_for_pusch_on_current_bwp);
  EXPECT_EQ(tx_power + delta_srs, increased_tx_power);

  int more_tx_power =
      get_srs_tx_power_ue(&mac, &srs_resource, &srs_resource_set, delta_srs, is_configured_for_pusch_on_current_bwp);
  EXPECT_EQ(tx_power + delta_srs * 2, more_tx_power);
}

int main(int argc, char** argv)
{
  logInit();
  configmodule_interface_t *uniqCfg = load_configmodule(argc, argv, CONFIG_ENABLECMDLINEONLY);
  g_log->log_component[MAC].level = OAILOG_DEBUG;
  g_log->log_component[NR_MAC].level = OAILOG_DEBUG;
  testing::InitGoogleTest(&argc, argv);
  int ret = RUN_ALL_TESTS();
  end_configmodule(uniqCfg);
  return ret;
}
