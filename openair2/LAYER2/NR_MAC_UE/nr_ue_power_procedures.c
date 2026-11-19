/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Routines for UE MAC-layer power control procedures
 */

#include "LAYER2/NR_MAC_UE/mac_proto.h"

#define DEFAULT_P0_NOMINAL_PUCCH_0_DBM 0
#define DEFAULT_DELTA_F_PUCCH_0_DB 0

// TODO: This should be part of mac instance
/* TS 38.213 9.2.5.2 UE procedure for multiplexing HARQ-ACK/SR and CSI in a PUCCH */
/* this is a counter of number of pucch format 4 per subframe */
static int nb_pucch_format_4_in_subframes[NR_NUMBER_OF_SUBFRAMES_PER_FRAME] = {0};

/* TS 38.211 Table 6.4.1.3.3.2-1: DM-RS positions for PUCCH format 3 and 4 */
static const int nb_symbols_excluding_dmrs[11][2][2]
= {
/*                     No additional DMRS            Additional DMRS   */
/* PUCCH length      No hopping   hopping         No hopping   hopping */
/* index                  0          1                 0          1    */
/*    4     */    {{      3    ,     2   }   ,  {      3     ,    2    }},
/*    5     */    {{      3    ,     3   }   ,  {      3     ,    3    }},
/*    6     */    {{      4    ,     4   }   ,  {      4     ,    4    }},
/*    7     */    {{      5    ,     5   }   ,  {      5     ,    5    }},
/*    8     */    {{      6    ,     6   }   ,  {      6     ,    6    }},
/*    9     */    {{      7    ,     7   }   ,  {      7     ,    7    }},
/*   10     */    {{      8    ,     8   }   ,  {      6     ,    6    }},
/*   11     */    {{      9    ,     9   }   ,  {      7     ,    7    }},
/*   12     */    {{     10    ,    10   }   ,  {      8     ,    8    }},
/*   13     */    {{     11    ,    11   }   ,  {      9     ,    9    }},
/*   14     */    {{     12    ,    12   }   ,  {     10     ,   10    }},
};

// ∆MPR according to Table 6.2.2-3 38.101-1
static float get_delta_mpr(uint16_t nr_band, frame_type_t frame_type, int scs, int channel_bandwidth, int power_class)
{
  if (compare_relative_ul_channel_bw(nr_band, scs, channel_bandwidth, frame_type)) {
    if (power_class == 3) {
      if ((nr_band == 28 || nr_band == 83) && channel_bandwidth == 30) {
        return 0.5f;
      }
    }
    if (power_class == 3 || power_class == 2) {
      if ((nr_band == 40 || nr_band == 97) && channel_bandwidth == 100) {
        return 1.0f;
      }
    }
  }
  return 0;
}

// MPR according to 38-101 6.2.2
static float get_mpr(int Qm, int N_RB_UL, bool is_transform_precoding, int n_prbs, int start_prb, int power_class)
{
  float MPR = 0;
  int rb_low = (n_prbs / 2) > 1 ? (n_prbs / 2) : 1;
  int rb_high = N_RB_UL - rb_low - n_prbs;
  bool is_inner_rb = start_prb >= rb_low && start_prb <= rb_high && n_prbs <= ((N_RB_UL / 2) + (N_RB_UL & 1));
  switch (power_class) {
    case 3:
      // Table 6.2.2-1 in 38.101
      switch (Qm) {
        case 1:
          AssertFatal(false, "MPR for Pi/2 BPSK not implemented yet\n");
          break;
        case 2:
          if (is_transform_precoding) {
            if (!is_inner_rb)
              MPR = 1;
          } else {
            if (is_inner_rb)
              MPR = 1.5;
            else
              MPR = 3;
          }
          break;
        case 4:
          if (is_transform_precoding) {
            if (is_inner_rb)
              MPR = 1;
            else
              MPR = 2;
          } else {
            if (is_inner_rb)
              MPR = 2;
            else
              MPR = 3;
          }
          break;
        case 6:
          if (is_transform_precoding)
            MPR = 2.5;
          else
            MPR = 3.5;
          break;
        case 8:
          if (is_transform_precoding)
            MPR = 4.5;
          else
            MPR = 6.5;
          break;
          break;
        default:
          AssertFatal(false, "Invalid Qm %d\n", Qm);
      }
      break;
    default:
      AssertFatal(false, "PowerClass != 3 not implemented\n");
  }
  return MPR;
}

// Implementation of 6.2.4 Configured ransmitted power
// 3GPP TS 38.101-1 version 16.5.0 Release 16
// -
// The UE is allowed to set its configured maximum output power PCMAX,f,c for carrier f of serving cell c in each slot.
// The configured maximum output power PCMAX,f,c is set within the following bounds: PCMAX_L,f,c <=  PCMAX,f,c <=  PCMAX_H,f,c
// -
// Measurement units:
// - p_max:              dBm
// - delta_TC_c:         dB
// - P_powerclass:       dBm
// - delta_P_powerclass: dB
// - MPR_c:              dB
// - delta_MPR_c:        dB
// - delta_T_IB_c        dB
// - delta_rx_SRS        dB
// note:
// - Assuming:
// -- Powerclass 3 capable UE (which is default power class unless otherwise stated)
// -- Maximum power reduction (MPR_c) for power class 3
// -- no additional MPR (A_MPR_c)
float nr_get_Pcmax(int p_Max,
                   uint16_t nr_band,
                   frame_type_t frame_type,
                   frequency_range_t frequency_range,
                   int channel_bandwidth,
                   int Qm,
                   bool powerBoostPi2BPSK,
                   int scs,
                   int N_RB_UL,
                   bool is_transform_precoding,
                   int n_prbs,
                   int start_prb)
{
  const int power_class = 3; // Assume power class 3
  if (frequency_range == FR1) {
    // TODO configure P-MAX from the upper layers according to 38.331
    int p_powerclass = 23; // dBm assuming poweclass 3 UE
    const bool has_p_max = p_Max != INT_MIN;
    int p_emax = has_p_max ? p_Max : p_powerclass;
    int delta_P_powerclass = 0; // for powerclass 2 needs to be changed
    if (has_p_max && Qm == 1 && powerBoostPi2BPSK
        && (nr_band == 40 || nr_band == 41 || nr_band == 77 || nr_band == 78 || nr_band == 79)) {
      p_emax += 3;
      delta_P_powerclass -= 3;
    }

    // TODO to be set for CA and DC
    int delta_T_IB = 0;

    // TODO in case of band 41 and PRB allocation within 4MHz of the upper or lower limit of the band -> delta_TC = 1.5
    if (nr_band == 41)
      LOG_E(NR_MAC, "Need to implement delta_TC for band 41\n");
    int delta_TC = 0;

    float MPR = get_mpr(Qm, N_RB_UL, is_transform_precoding, n_prbs, start_prb, power_class);
    float delta_MPR = get_delta_mpr(nr_band, frame_type, scs, channel_bandwidth, power_class);
    int A_MPR = 0; // TODO too complicated to implement for now (see 6.2.3 in 38.101-1)
    int delta_rx_SRS = 0; // TODO for SRS
    int P_MPR = 0; // to ensure compliance with applicable electromagnetic energy absorption requirements

    float total_reduction = max(max(MPR + delta_MPR, A_MPR) + delta_T_IB + delta_TC + delta_rx_SRS, P_MPR);

    float pcmax_high, pcmax_low;
    if (has_p_max) {
      pcmax_high = p_emax < (p_powerclass - delta_P_powerclass) ? p_emax : (p_powerclass - delta_P_powerclass);
      pcmax_low = (p_emax - delta_TC) < (p_powerclass - delta_P_powerclass - total_reduction)
                      ? (p_emax - delta_TC)
                      : (p_powerclass - delta_P_powerclass - total_reduction);
    } else {
      pcmax_high = p_powerclass - delta_P_powerclass;
      pcmax_low = p_powerclass - delta_P_powerclass - total_reduction;
    }
    // TODO we need a strategy to select a value between minimum and maximum allowed PC_max
    float pcmax = (pcmax_low + pcmax_high) / 2;
    LOG_D(MAC,
          "Configured maximum output power:  %f dBm <= PCMAX %f dBm <= %f dBm MPR=%.2f deltaMPR=%.2f\n",
          pcmax_low,
          pcmax,
          pcmax_high,
          MPR,
          delta_MPR);
    return pcmax;
  } else {
    // FR2 TODO it is even more complex because it is radiated power
    return 23;
  }
}

float nr_get_Pcmin(int bandwidth_index) {
  const float table_38101_6_3_1_1[] = {
    -40, -40, -40, -40, -39, -38.2, -37.5, -37, -36.5, -36, -35.2, -34.6, -34, -33.5, -33
  };
  return table_38101_6_3_1_1[bandwidth_index];
}

// This is not entirely correct. In certain k2/k1/k0 settings we might postpone accumulating delta_PUCCH until next HARQ feedback
// slot. The correct way to do this would be to calculate the K_PUCCH (delta_PUCCH summation window end) for each PUCCH occasion and
// compare PUCCH transmission symbol with the reception symbol of the DCI containing delta_PUCCH to determine if the delta_PUCCH
// should be added at each occasion.
int get_sum_delta_pucch(NR_UE_MAC_INST_t *mac, int slot, frame_t frame)
{
  int delta_tpc_sum = 0;
  const int num_dl_harq = get_nrofHARQ_ProcessesForPDSCH(&mac->sc_info);

  for (int i = 0; i < num_dl_harq; i++) {
    for (int c = 0; c < 2; c++) {
      if (mac->dl_harq_info[i][c].active && mac->dl_harq_info[i][c].ul_slot == slot && mac->dl_harq_info[i][c].ul_frame == frame) {
        delta_tpc_sum += mac->dl_harq_info[i][c].delta_pucch;
        mac->dl_harq_info[i][c].delta_pucch = 0;
      }
    }
  }
  return delta_tpc_sum;
}

static int get_deltatf(uint16_t nb_of_prbs,
                       uint8_t N_symb_PUCCH,
                       uint8_t freq_hop_flag,
                       uint8_t add_dmrs_flag,
                       int N_sc_ctrl_RB,
                       int O_UCI)
{
  int DELTA_TF;
  int O_CRC = compute_pucch_crc_size(O_UCI);
  int N_symb = N_symb_PUCCH < 4 ? N_symb_PUCCH : nb_symbols_excluding_dmrs[N_symb_PUCCH - 4][add_dmrs_flag][freq_hop_flag];
  float N_RE = nb_of_prbs * N_sc_ctrl_RB * N_symb;
  float K1 = 6;
  if (O_UCI + O_CRC < 12)
    DELTA_TF = 10 * log10((double)(((K1 * (O_UCI)) / N_RE)));
  else {
    float K2 = 2.4;
    float BPRE = (O_UCI + O_CRC) / N_RE;
    DELTA_TF = 10 * log10((double)(pow(2, (K2 * BPRE)) - 1));
  }
  return DELTA_TF;
}

// PUCCH Power control according to 38.213 section 7.2.1
int16_t get_pucch_tx_power_ue(NR_UE_MAC_INST_t *mac,
                              int scs,
                              NR_PUCCH_Config_t *pucch_Config,
                              int sum_delta_pucch,
                              uint8_t format_type,
                              uint16_t nb_of_prbs,
                              uint8_t freq_hop_flag,
                              uint8_t add_dmrs_flag,
                              uint8_t N_symb_PUCCH,
                              int subframe_number,
                              int O_uci,
                              uint16_t start_prb)
{
  NR_UE_UL_BWP_t *current_UL_BWP = mac->current_UL_BWP;
  AssertFatal(current_UL_BWP && current_UL_BWP->pucch_ConfigCommon,
              "Missing configuration: need UL_BWP and pucch_ConfigCommon to calculate PUCCH tx power\n");
  int PUCCH_POWER_DEFAULT = 0;
  // p0_nominal is optional
  int16_t P_O_NOMINAL_PUCCH = DEFAULT_P0_NOMINAL_PUCCH_0_DBM;
  if (current_UL_BWP->pucch_ConfigCommon->p0_nominal != NULL) {
    P_O_NOMINAL_PUCCH = *current_UL_BWP->pucch_ConfigCommon->p0_nominal;
  }

  struct NR_PUCCH_PowerControl *power_config = pucch_Config ? pucch_Config->pucch_PowerControl : NULL;

  if (!power_config)
    return (PUCCH_POWER_DEFAULT);

  int16_t P_O_UE_PUCCH = 0;

  if (pucch_Config->spatialRelationInfoToAddModList != NULL) {  /* FFS TODO NR */
    LOG_D(MAC,"PUCCH Spatial relation infos are not yet implemented\n");
    return (PUCCH_POWER_DEFAULT);
  }

  int G_b_f_c = 0;
  if (power_config->p0_Set != NULL) {
    P_O_UE_PUCCH = power_config->p0_Set->list.array[0]->p0_PUCCH_Value; /* get from index 0 if no spatial relation set */
  }

  int P_O_PUCCH = P_O_NOMINAL_PUCCH + P_O_UE_PUCCH;

  int16_t delta_F_PUCCH = DEFAULT_DELTA_F_PUCCH_0_DB;
  long *delta_F_PUCCH_config = NULL;
  int DELTA_TF;
  uint16_t N_ref_PUCCH;
  int N_sc_ctrl_RB = 0;

  /* computing of pucch transmission power adjustment */
  switch (format_type) {
    case 0:
      N_ref_PUCCH = 2;
      DELTA_TF = 10 * log10(N_ref_PUCCH/N_symb_PUCCH);
      delta_F_PUCCH_config = power_config->deltaF_PUCCH_f0;
      break;
    case 1:
      N_ref_PUCCH = 14;
      DELTA_TF = 10 * log10(N_ref_PUCCH/N_symb_PUCCH * O_uci);
      delta_F_PUCCH_config = power_config->deltaF_PUCCH_f1;
      break;
    case 2:
      N_sc_ctrl_RB = 10;
      DELTA_TF = get_deltatf(nb_of_prbs, N_symb_PUCCH, freq_hop_flag, add_dmrs_flag, N_sc_ctrl_RB, O_uci);
      delta_F_PUCCH_config = power_config->deltaF_PUCCH_f2;
      break;
    case 3:
      N_sc_ctrl_RB = 14;
      DELTA_TF = get_deltatf(nb_of_prbs, N_symb_PUCCH, freq_hop_flag, add_dmrs_flag, N_sc_ctrl_RB, O_uci);
      delta_F_PUCCH_config = power_config->deltaF_PUCCH_f3;
      break;
    case 4:
      N_sc_ctrl_RB = 14/(nb_pucch_format_4_in_subframes[subframe_number]);
      DELTA_TF = get_deltatf(nb_of_prbs, N_symb_PUCCH, freq_hop_flag, add_dmrs_flag, N_sc_ctrl_RB, O_uci);
      delta_F_PUCCH_config = power_config->deltaF_PUCCH_f4;
      break;
    default:
    {
      LOG_E(MAC,"PUCCH unknown pucch format %d\n", format_type);
      return (0);
    }
  }
  if (delta_F_PUCCH_config != NULL) {
    delta_F_PUCCH = *delta_F_PUCCH_config;
  }

  // 38.101-1: The allowed MPR for SRS, PUCCH formats 0, 1, 3 and 4, and PRACH shall be as specified for QPSK modulated DFT-
  // s-OFDM of equivalent RB allocation. The allowed MPR for PUCCH format 2 shall be as specified for QPSK
  // modulated CP-OFDM of equivalent RB allocation.
  int P_CMAX = nr_get_Pcmax(mac->p_Max,
                            mac->nr_band,
                            mac->frame_structure.frame_type,
                            mac->frequency_range,
                            current_UL_BWP->channel_bandwidth,
                            2,
                            false,
                            mac->current_UL_BWP->scs,
                            mac->current_UL_BWP->BWPSize,
                            format_type == 2,
                            1,
                            start_prb);
  float P_CMIN = current_UL_BWP->P_CMIN;
  int16_t pathloss = compute_nr_SSB_PL(mac);

  if (power_config->twoPUCCH_PC_AdjustmentStates && *power_config->twoPUCCH_PC_AdjustmentStates > 1) {
    LOG_E(MAC,"PUCCH power control adjustment states with 2 states not yet implemented\n");
    return (PUCCH_POWER_DEFAULT);
  }
  int M_pucch_component = (10 * log10((double)(pow(2,scs) * nb_of_prbs)));

  int16_t pucch_power_without_g_pucch = P_O_PUCCH + M_pucch_component + pathloss + delta_F_PUCCH + DELTA_TF;

  if (power_config->p0_Set == NULL) {
    if (mac->pucch_power_control_initialized == false) {
      // Initialize power control state
      // Assuming only sending on PCell
      NR_PRACH_RESOURCES_t *prach_res = &mac->ra.prach_resources;
      float DELTA_P_rampup_requested = (prach_res->preamble_power_ramping_cnt - 1) * prach_res->preamble_power_ramping_step;
      float DELTA_P_rampup = P_CMAX - (P_O_PUCCH + pathloss + delta_F_PUCCH + DELTA_TF + sum_delta_pucch);
      DELTA_P_rampup = max(min(0, DELTA_P_rampup), DELTA_P_rampup_requested);
      mac->G_b_f_c = DELTA_P_rampup + sum_delta_pucch;
      mac->pucch_power_control_initialized = true;
    }
    else {
      // PUCCH closed loop power control state
      G_b_f_c = mac->G_b_f_c;
      if (!((pucch_power_without_g_pucch + G_b_f_c >= P_CMAX && sum_delta_pucch > 0) ||
        (pucch_power_without_g_pucch + G_b_f_c <= P_CMIN && sum_delta_pucch < 0))) {
        G_b_f_c += sum_delta_pucch;
      }
      mac->G_b_f_c = G_b_f_c;
    }
  }


  int pucch_power = min(P_CMAX, pucch_power_without_g_pucch + G_b_f_c);

  LOG_D(MAC, "PUCCH ( Tx power : %d dBm ) ( 10Log(...) : %d ) ( from Path Loss : %d ) ( delta_F_PUCCH : %d ) ( DELTA_TF : %d ) ( G_b_f_c : %d ) \n",
        pucch_power, M_pucch_component, pathloss, delta_F_PUCCH, DELTA_TF, G_b_f_c);

  return pucch_power;
}

// Returns the pathloss in dB for the active UL BWP on the selected carrier based on the DL RS associated with the PRACH transmission
// computation according to clause 7.4 (Physical random access channel) of 3GPP TS 38.213 version 16.3.0 Release 16
// Assumptions:
// - PRACH transmission from a UE is not in response to a detection of a PDCCH order by the UE
// Measurement units:
// - referenceSignalPower:   dBm/RE (average EPRE of the resources elements that carry secondary synchronization signals in dBm)
int16_t compute_nr_SSB_PL(NR_UE_MAC_INST_t *mac)
{
  // getting the maximum SSB RSRP from the SSB measurements
  int max_ssb_rsrp_dBm = mac->ssb_measurements[mac->mib_ssb].ssb_rsrp_dBm;
  fapi_nr_config_request_t *cfg = &mac->phy_config.config_req;
  int referenceSignalPower = cfg->ssb_config.ss_pbch_power;

  //TODO improve PL measurements. Probably not correct as it is.

  int16_t pathloss = (int16_t)(referenceSignalPower - max_ssb_rsrp_dBm);

  LOG_D(NR_MAC, "pathloss %d dB, referenceSignalPower %d dBm/RE (%f mW), RSRP %d dBm (%f mW)\n",
        pathloss,
        referenceSignalPower,
        pow(10, referenceSignalPower/10),
        max_ssb_rsrp_dBm,
        pow(10, max_ssb_rsrp_dBm/10));

  return pathloss;
}

// PUSCH transmission power according to 38.213 7.1
int get_pusch_tx_power_ue(NR_UE_MAC_INST_t *mac,
                          int num_rb,
                          int start_prb,
                          uint16_t nb_symb_sch,
                          uint16_t nb_dmrs_prb,
                          uint16_t nb_ptrs_prb,
                          uint16_t qm,
                          uint16_t R,
                          uint16_t beta_offset_csi1,
                          uint32_t sum_bits_in_codeblocks,
                          int delta_pusch,
                          bool is_rar_tx_retx,
                          bool transform_precoding)
{
  LOG_D(NR_MAC,
        "PUSCH tx power determination num_rb=%d start_prb=%d nb_symb_sch=%u nb_dmrs_prb=%u nb_ptrs_prb=%u Qm=%u R= %u "
        "beta_offset_cs1=%u sum_bits_in_codeblocks=%u delta_pusch=%d is_rar_tx_retx=%d transform_precoding=%d\n",
        num_rb,
        start_prb,
        nb_symb_sch,
        nb_dmrs_prb,
        nb_ptrs_prb,
        qm,
        R,
        beta_offset_csi1,
        sum_bits_in_codeblocks,
        delta_pusch,
        is_rar_tx_retx,
        transform_precoding);
  NR_UE_UL_BWP_t *current_UL_BWP = mac->current_UL_BWP;
  AssertFatal(current_UL_BWP, "Missing configuration: need UL_BWP to calculate PUSCH tx power\n");
  NR_PUSCH_Config_t *pusch_Config = current_UL_BWP->pusch_Config;
  bool has_pusch_config = pusch_Config != NULL;
  bool has_pusch_power_control_config = has_pusch_config && pusch_Config->pusch_PowerControl != NULL;
  bool is_provided_alpha_sets = has_pusch_power_control_config && pusch_Config->pusch_PowerControl->p0_AlphaSets != NULL;

  int P_O_NOMINAL_PUSCH;
  float alpha;
  const float alpha_factor_table[8] = {0.0f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
  if (is_rar_tx_retx || !is_provided_alpha_sets || current_UL_BWP->p0_NominalWithGrant == NULL) {
    int DELTA_PREAMBLE_MSG3 = 0;
    if (current_UL_BWP->msg3_DeltaPreamble) {
      DELTA_PREAMBLE_MSG3 = *current_UL_BWP->msg3_DeltaPreamble;
    }
    int P_O_PRE = mac->ra.prach_resources.ra_preamble_rx_target_power;
    P_O_NOMINAL_PUSCH = P_O_PRE + DELTA_PREAMBLE_MSG3;

    if (has_pusch_power_control_config && pusch_Config->pusch_PowerControl->msg3_Alpha) {
      alpha = alpha_factor_table[*pusch_Config->pusch_PowerControl->msg3_Alpha];
    } else {
      alpha = 1.0f;
    }
  } else {
    P_O_NOMINAL_PUSCH = *current_UL_BWP->p0_NominalWithGrant;
    if (pusch_Config->pusch_PowerControl->p0_AlphaSets->list.array[0]->alpha) {
      alpha = alpha_factor_table[*pusch_Config->pusch_PowerControl->p0_AlphaSets->list.array[0]->alpha];
    } else {
      // Default according to 38.331 P0-PUSCH-AlphaSet field descriptions
      alpha = 1.0f;
    }
  }

  int P_O_UE_PUSCH;
  if (is_rar_tx_retx || !is_provided_alpha_sets) {
    P_O_UE_PUSCH = 0;
  } else {
    if (pusch_Config->pusch_PowerControl->p0_AlphaSets->list.array[0]->p0) {
      P_O_UE_PUSCH = *pusch_Config->pusch_PowerControl->p0_AlphaSets->list.array[0]->p0;
    } else {
      // Default according to 38.331 P0-PUSCH-AlphaSet field descriptions
      P_O_UE_PUSCH = 0;
    }
  }

  int mu = current_UL_BWP->scs;

  int M_pusch_component = 10 * log10((pow(2, mu)) * num_rb);
  int P_CMAX = nr_get_Pcmax(mac->p_Max,
                            mac->nr_band,
                            mac->frame_structure.frame_type,
                            mac->frequency_range,
                            mac->current_UL_BWP->channel_bandwidth,
                            qm,
                            false,
                            mac->current_UL_BWP->scs,
                            mac->current_UL_BWP->BWPSize,
                            transform_precoding,
                            num_rb,
                            start_prb);

  int P_O_PUSCH = P_O_NOMINAL_PUSCH + P_O_UE_PUSCH;

  float DELTA_TF = 0;
  if (has_pusch_power_control_config && pusch_Config->pusch_PowerControl->deltaMCS) {
    float beta_offset = 1;
    float BPRE;
    if (sum_bits_in_codeblocks == 0) {
      float table_38_213_9_3_2[] = {
          1.125,   11.250,  21.375,  31.625,  41.750,  52.000,   62.250,   72.500,   82.875,   93.125,
          103.500, 114.000, 125.000, 136.250, 148.000, 1510.000, 1612.625, 1715.875, 1820.000,
      };
      beta_offset = table_38_213_9_3_2[beta_offset_csi1];
      BPRE = (qm * R / beta_offset) / 1024;
    } else {
      const int nb_subcarrier_per_rb = 12;
      const uint32_t N_RE = nb_subcarrier_per_rb * nb_symb_sch - nb_dmrs_prb - nb_ptrs_prb;
      BPRE = sum_bits_in_codeblocks / (float)(N_RE * num_rb);
    }
    DELTA_TF = 10 * log10(pow(2, BPRE * 1.25f) * beta_offset);
  }

  // TODO: compute pathloss using correct reference
  int16_t pathloss = compute_nr_SSB_PL(mac);
  int P_CMIN = mac->current_UL_BWP->P_CMIN;

  float pusch_power_without_f_b_f_c = P_O_PUSCH + M_pusch_component + alpha * pathloss + DELTA_TF;

  int f_b_f_c = 0;
  if (has_pusch_power_control_config && pusch_Config->pusch_PowerControl->tpc_Accumulation) {
    f_b_f_c = delta_pusch;
  } else {
    if (!mac->pusch_power_control_initialized && is_rar_tx_retx) {
      NR_PRACH_RESOURCES_t *prach_res = &mac->ra.prach_resources;
      float DELTA_P_rampup_requested = (prach_res->preamble_power_ramping_cnt - 1) * prach_res->preamble_power_ramping_step;
      float DELTA_P_rampup = P_CMAX - (P_O_PUSCH + M_pusch_component + alpha * pathloss + DELTA_TF + delta_pusch);
      DELTA_P_rampup = min(DELTA_P_rampup_requested, max(0, DELTA_P_rampup));
      mac->f_b_f_c = DELTA_P_rampup + delta_pusch;
      mac->pusch_power_control_initialized = true;
      mac->delta_msg2 = delta_pusch;
    } else {
      if (!((pusch_power_without_f_b_f_c + mac->f_b_f_c >= P_CMAX && delta_pusch > 0) ||
        (pusch_power_without_f_b_f_c + mac->f_b_f_c <= P_CMIN && delta_pusch < 0))) {
        mac->f_b_f_c += delta_pusch;
      }
    }
    f_b_f_c = mac->f_b_f_c;
  }
  LOG_D(NR_MAC,
        "PUSCH tx power components P_O_PUSCH=%d, M_pusch_component=%d, alpha*pathloss=%f, delta_TF=%f, f_b_f_c=%d\n",
        P_O_PUSCH,
        M_pusch_component,
        alpha * pathloss,
        DELTA_TF,
        f_b_f_c);
  return min(P_CMAX, P_O_PUSCH + M_pusch_component + alpha * pathloss + DELTA_TF + f_b_f_c);
}

int get_srs_tx_power_ue(NR_UE_MAC_INST_t *mac,
                        NR_SRS_Resource_t *srs_resource,
                        NR_SRS_ResourceSet_t *srs_resource_set,
                        int delta_srs,
                        bool is_configured_for_pusch_on_current_bwp)
{
  NR_UE_UL_BWP_t *current_UL_BWP = mac->current_UL_BWP;
  AssertFatal(current_UL_BWP, "Missing configuration: need UL_BWP to calculate SRS tx power\n");
  AssertFatal(srs_resource_set->p0, "P0 mantatory present at configuration\n");
  int P_0_SRS = *srs_resource_set->p0;

  // alpha is optional, use same default as PUSCH
  const float alpha_factor_table[8] = {0.0f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
  float alpha = srs_resource_set->alpha ? alpha_factor_table[*srs_resource_set->alpha] : 1.0f;
  int m_srs_component =
      10 * log10(pow(2, current_UL_BWP->scs) * get_m_srs(srs_resource->freqHopping.c_SRS, srs_resource->freqHopping.b_SRS));

  // The allowed MPR for SRS, PUCCH formats 0, 1, 3 and 4, and PRACH shall be as specified for QPSK modulated DFTs-OFDM of
  // equivalent RB allocation. The allowed MPR for PUCCH format 2 shall be as specified for QPSK modulated CP-OFDM of equivalent RB
  // allocation.
  int P_CMAX = nr_get_Pcmax(mac->p_Max,
                            mac->nr_band,
                            mac->frame_structure.frame_type,
                            mac->frequency_range,
                            mac->current_UL_BWP->channel_bandwidth,
                            2,
                            false,
                            mac->current_UL_BWP->scs,
                            mac->current_UL_BWP->BWPSize,
                            true,
                            get_m_srs(srs_resource->freqHopping.c_SRS, srs_resource->freqHopping.b_SRS),
                            0); // TODO: Determine SRS start RB

  int16_t pathloss = compute_nr_SSB_PL(mac);

  int srs_power_without_h_b_f_c = P_0_SRS + alpha * pathloss + m_srs_component;

  // Power adjustment state
  int h_b_f_c = 0;
  bool use_pusch_power_adjustment_state = srs_resource_set->srs_PowerControlAdjustmentStates == NULL;
  if (is_configured_for_pusch_on_current_bwp && use_pusch_power_adjustment_state) {
    // Case 1: Use PUSCH power control adjustment state
    h_b_f_c = mac->f_b_f_c;
  } else {
    // Case 2: Separate power control adjustment state
    AssertFatal(srs_resource_set->srs_PowerControlAdjustmentStates == NULL
                    || *srs_resource_set->srs_PowerControlAdjustmentStates
                           != NR_SRS_ResourceSet__srs_PowerControlAdjustmentStates_sameAsFci2,
                "Two PUSCH power adjustment states not supported");

    bool is_tpc_accumulation_provided = current_UL_BWP->srs_Config != NULL && current_UL_BWP->srs_Config->tpc_Accumulation != NULL;
    bool separate_pc_adjustment_state = srs_resource_set->srs_PowerControlAdjustmentStates
                                        && *srs_resource_set->srs_PowerControlAdjustmentStates
                                               == NR_SRS_ResourceSet__srs_PowerControlAdjustmentStates_separateClosedLoop;

    if (!is_tpc_accumulation_provided && (!is_configured_for_pusch_on_current_bwp || separate_pc_adjustment_state)) {
      if (!current_UL_BWP->srs_power_control_initialized) {
        NR_PRACH_RESOURCES_t *prach = &mac->ra.prach_resources;
        float DELTA_P_rampup_requested = (prach->preamble_power_ramping_cnt - 1) * prach->preamble_power_ramping_step;
        float DELTA_P_rampup = P_CMAX - (P_0_SRS + m_srs_component + alpha * pathloss);
        DELTA_P_rampup = min(DELTA_P_rampup_requested, max(0, DELTA_P_rampup));
        current_UL_BWP->srs_power_control_initialized = true;
        current_UL_BWP->h_b_f_c = DELTA_P_rampup + mac->delta_msg2;
      } else {
        int P_CMIN = mac->current_UL_BWP->P_CMIN;
        if (!((srs_power_without_h_b_f_c + current_UL_BWP->h_b_f_c >= P_CMAX && delta_srs > 0)
              || (srs_power_without_h_b_f_c + current_UL_BWP->h_b_f_c <= P_CMIN && delta_srs < 0))) {
          current_UL_BWP->h_b_f_c += delta_srs;
        }
      }
      h_b_f_c = current_UL_BWP->h_b_f_c;
    } else {
      // Case 3: No accumulation
      h_b_f_c = delta_srs;
    }
  }

  return min(P_CMAX, srs_power_without_h_b_f_c + h_b_f_c);
}
