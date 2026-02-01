/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#include "mac_defs.h"
#include "mac_proto.h"
#include "openair2/LAYER2/NR_MAC_COMMON/nr_mac_common.h"
#include "executables/softmodem-common.h"
#include "executables/nr-uesoftmodem.h"

#define SL_DEBUG

static const int sequence_cyclic_shift_harq_ack_or_ack_or_only_nack[2]
/* Sequence cyclic shift */ = {  0, 6 };

void print_prb_set_allocation(psfch_params_t *psfch_params, uint8_t psfch_period, uint8_t  num_subchannels) {
  LOG_D(NR_PHY, "PSSCH Slot mod PSFCH period |   Subchannel   |   Start PRB   |    End PRB\n");
  for (int i = 0; i < psfch_period; i++) {
    for (int j = 0; j < num_subchannels; j++) {
      LOG_D(NR_PHY, "\t\t    %d \t\t|\t%d\t|\t%d\t| \t %d\n", i, j, psfch_params->prbs_sets->start_prb[i][j], psfch_params->prbs_sets->end_prb[i][j]);
    }
  }
}

uint8_t sl_process_TDD_UL_DL_config_patterns(NR_TDD_UL_DL_ConfigCommon_t *TDD_UL_DL_Config,
                                             uint8_t mu,
                                             double *slot_period_P,
                                             uint8_t *w)
{

  uint8_t return_value = 255;
  *w = 0;
  int pattern1_dlul_period = TDD_UL_DL_Config->pattern1.dl_UL_TransmissionPeriodicity;

#ifdef SL_DEBUG

  printf("INPUT VALUES: function: %s\n", __func__);
  printf("pattern1 periodicity:%d\n", pattern1_dlul_period);
  if (TDD_UL_DL_Config->pattern1.ext1 != NULL && TDD_UL_DL_Config->pattern1.ext1->dl_UL_TransmissionPeriodicity_v1530 != NULL )
    printf("pattern1 periodicity_v1530:%ld\n", *TDD_UL_DL_Config->pattern1.ext1->dl_UL_TransmissionPeriodicity_v1530);
  if (TDD_UL_DL_Config->pattern2 != NULL) {
    printf("mu:%d, pattern2 periodicity:%d\n", mu, pattern1_dlul_period);
    if (TDD_UL_DL_Config->pattern2->ext1 != NULL && TDD_UL_DL_Config->pattern2->ext1->dl_UL_TransmissionPeriodicity_v1530 != NULL )
      printf("pattern2 periodicity_v1530:%ld\n", *TDD_UL_DL_Config->pattern2->ext1->dl_UL_TransmissionPeriodicity_v1530);
  }

#endif

  return_value = pattern1_dlul_period;
  switch (pattern1_dlul_period) {
    case 0:
      *slot_period_P = 0.5;
      break;
    case 1:
      *slot_period_P = 0.625;
      break;
    case 2:
      *slot_period_P = 1.0;
      break;
    case 3:
      *slot_period_P = 1.25;
      break;
    case 4:
      *slot_period_P = 2.0;
      break;
    case 5:
      *slot_period_P = 2.5;
      break;
    case 6:
      *slot_period_P = 5.0;
      return_value = 7;
      break;
    case 7:
      *slot_period_P = 10.0;
      return_value = 8;
      break;
    default:
      AssertFatal(1==0,"Incorrect value of dl_UL_TransmissionPeriodicity\n");
      break;
  }

  if (TDD_UL_DL_Config->pattern1.ext1 != NULL &&
      TDD_UL_DL_Config->pattern1.ext1->dl_UL_TransmissionPeriodicity_v1530 != NULL ) {
    if (*TDD_UL_DL_Config->pattern1.ext1->dl_UL_TransmissionPeriodicity_v1530 == 1) {
      *slot_period_P = 4.0;
      return_value = 6;
    } else {
      *slot_period_P = 3.0;
      return_value = 255;
    }
  }

  if (TDD_UL_DL_Config->pattern2 != NULL) {

    return_value = 255;
    *w = 1;

    if ((*slot_period_P == 4.0 ) && (TDD_UL_DL_Config->pattern2->dl_UL_TransmissionPeriodicity == 1)) {
      return_value = 13;
      *w = (mu == 3)? 2: 1;
    } else if ((*slot_period_P == 3.0 ) && (TDD_UL_DL_Config->pattern2->dl_UL_TransmissionPeriodicity == 4)) {
      return_value = 12;
      *w = (mu == 3)? 2: 1;
    } else if ((*slot_period_P == 3.0 ) && (TDD_UL_DL_Config->pattern2->dl_UL_TransmissionPeriodicity == 2)) {
      return_value = 8;
      *w = (mu == 3)? 2: 1;
    } else {

      switch (pattern1_dlul_period) {
        case 7:
          if (TDD_UL_DL_Config->pattern2->dl_UL_TransmissionPeriodicity == 7) {
            return_value = 15;
            *w = 1<<mu;
          }
          break;
        case 6:
          if (TDD_UL_DL_Config->pattern2->dl_UL_TransmissionPeriodicity == 6) {
            return_value = 14;
            *w = (mu==0)?1:1<<(mu-1);
          }
          break;
        case 5:
          if (TDD_UL_DL_Config->pattern2->dl_UL_TransmissionPeriodicity == 5) {
            return_value = 11;
            *w = (mu == 3)? 2: 1;
          }
          break;
        case 4:
          if (TDD_UL_DL_Config->pattern2->dl_UL_TransmissionPeriodicity == 0) {
            return_value = 5;
          }
          else if (TDD_UL_DL_Config->pattern2->dl_UL_TransmissionPeriodicity == 4) {
            return_value = 7;
          }
          else if (TDD_UL_DL_Config->pattern2->ext1 != NULL && *TDD_UL_DL_Config->pattern2->ext1->dl_UL_TransmissionPeriodicity_v1530 == 0) {
            return_value = 10;
            *w = (mu == 3)? 2: 1;
          }
          break;
        case 3:
          if (TDD_UL_DL_Config->pattern2->dl_UL_TransmissionPeriodicity == 3) {
            return_value = 4;
          }
          break;
        case 2:
          if (TDD_UL_DL_Config->pattern2->dl_UL_TransmissionPeriodicity == 2) {
            return_value = 2;
          }
          else if (TDD_UL_DL_Config->pattern2->ext1 != NULL && *TDD_UL_DL_Config->pattern2->ext1->dl_UL_TransmissionPeriodicity_v1530 == 0) {
            return_value = 6;
            *w = (mu == 3)? 2: 1;
          }
          else if (TDD_UL_DL_Config->pattern2->ext1 != NULL && *TDD_UL_DL_Config->pattern2->ext1->dl_UL_TransmissionPeriodicity_v1530 == 1) {
            return_value = 9;
            *w = (mu == 3)? 2: 1;
          }
          break;
        case 1:
          if (TDD_UL_DL_Config->pattern2->dl_UL_TransmissionPeriodicity == 1) {
            return_value = 1;
          }
          break;
        case 0:
          if (TDD_UL_DL_Config->pattern2->dl_UL_TransmissionPeriodicity == 0) {
            return_value = 0;
          }
          else if (TDD_UL_DL_Config->pattern2->dl_UL_TransmissionPeriodicity == 4) {
            return_value = 3;
          }
          break;
        default:
          AssertFatal(1==0,"Incorrect value of dl_UL_TransmissionPeriodicity");
      }
    }
  }

#ifdef SL_DEBUG
  printf("OUTPUT VALUES: function %s\n",__func__);
  printf("return_value:%d, *w:%d, slot_period_P:%f\n", return_value, *w, *slot_period_P);
#endif

  return return_value;
}

/*
This procedures prepares the psbch payload of tdd configuration according
to section 16.1 in 38.213
*/
void sl_prepare_psbch_payload(NR_TDD_UL_DL_ConfigCommon_t *TDD_UL_DL_Config,
                              uint8_t *bits_0_to_7, uint8_t *bits_8_to_11,
                              uint8_t mu, uint8_t L, uint8_t Y)
{

  uint8_t w = 0, a1_to_a4 = 0;
  uint8_t mu_ref = 0, diff = 0;
  uint8_t u_slots = 0, u_sym = 0, I1 = 0;
  uint8_t u_sl_slots = 0, u_sl_slots_2 = 0;
  double slot_period_P = 0.0;

  *bits_0_to_7  = 0xFF; // If TDD_UL_DL_Config = NULL all 12 bits are set to 1
  *bits_8_to_11 = 0xF0;

  if (TDD_UL_DL_Config != NULL) {

    mu_ref = TDD_UL_DL_Config->referenceSubcarrierSpacing;
    diff = 1 << (mu-mu_ref);
    u_slots = TDD_UL_DL_Config->pattern1.nrofUplinkSlots;
    u_sym = TDD_UL_DL_Config->pattern1.nrofUplinkSymbols;
    I1 = ((u_sym * diff) % L >= (L-Y)) ? 1 : 0;

#ifdef SL_DEBUG
    printf("INPUT VALUES: function %s\n", __func__);
    printf("numerology:%d, number of symbols:%d, sl-startSymbol:%d\n", mu, L, Y);
    printf("mu_ref:%d, u_slots:%d, u_sym:%d\n", mu_ref, u_slots, u_sym);
    if (TDD_UL_DL_Config->pattern2 != NULL)
      printf("u_slots_2:%ld, u_sym_2:%ld\n", TDD_UL_DL_Config->pattern2->nrofUplinkSlots,
                                             TDD_UL_DL_Config->pattern2->nrofUplinkSymbols);
#endif

    u_sl_slots = (u_slots * diff) + floor((u_sym*diff)/L) + I1;
    a1_to_a4 = sl_process_TDD_UL_DL_config_patterns(TDD_UL_DL_Config, mu, &slot_period_P, &w);
    AssertFatal(a1_to_a4 != 255,"Incorrect return value, wrong configuration.\n");

#ifdef SL_DEBUG
    printf("I1:%d, a1_to_a2:%d, u_sl_slots:%d\n", I1, a1_to_a4, u_sl_slots);
#endif

    if (TDD_UL_DL_Config->pattern2 != NULL) {

      uint8_t u_slots_2 = TDD_UL_DL_Config->pattern2->nrofUplinkSlots;
      uint8_t u_sym_2 = TDD_UL_DL_Config->pattern2->nrofUplinkSymbols;
      uint8_t I2 = ((u_sym_2 * diff) % L >= (L-Y)) ? 1 : 0;
      uint16_t val = floor(((u_slots_2 * diff) + floor((u_sym_2*diff)/L) + I2)/w);

      u_sl_slots_2 = val * ceil((slot_period_P*(1<<mu)+1)/w) + floor(u_sl_slots/w);

      *bits_0_to_7 = 0x80 | (a1_to_a4 << 3) | ((u_sl_slots_2 & 0x70) >> 4);
      *bits_8_to_11 = (u_sl_slots_2 & 0x0F) << 4;

#ifdef SL_DEBUG
    printf("I2:%d, val:%d, u_sl_slots_2:%d\n", I2, val, u_sl_slots_2);
#endif

    } else {
      *bits_0_to_7 = 0x00 | (a1_to_a4 << 3) | ((u_sl_slots & 0x70) >> 4);
      *bits_8_to_11 = (u_sl_slots & 0x0F) << 4;
    }
  }

#ifdef SL_DEBUG
    printf("OUTPUT VALUES: function %s\n", __func__);
    printf("12 bits payload buf[0]:%x, buf[1]:%x\n", *bits_0_to_7, *bits_8_to_11);
#endif

}

/*
This procedures prepares the psbch payload of tdd configuration according
to section 16.1 in 38.213
*/
uint8_t sl_decode_sl_TDD_Config(NR_TDD_UL_DL_ConfigCommon_t *TDD_UL_DL_Config,
                                uint8_t bits_0_to_7, uint8_t bits_8_to_11,
                                uint8_t mu, uint8_t L, uint8_t Y)
{

  AssertFatal(TDD_UL_DL_Config, "TDD_UL_DL_Config cannot be null");
  uint16_t num_SL_slots = 0, mixed_slot_numsym = 0;

  TDD_UL_DL_Config->pattern1.nrofDownlinkSlots = 0;
  TDD_UL_DL_Config->pattern1.nrofDownlinkSymbols = 0;
  TDD_UL_DL_Config->pattern1.nrofUplinkSlots = 0;
  TDD_UL_DL_Config->pattern1.nrofUplinkSymbols = 0;
  TDD_UL_DL_Config->referenceSubcarrierSpacing = mu;
  TDD_UL_DL_Config->pattern1.ext1 = NULL;

  LOG_D(MAC, "bits_0_to_7:%x, bits_8_to_11:%x, mu:%d, L:%d, Y:%d\n",
                                                  bits_0_to_7, bits_8_to_11,mu, L, Y);

  //If all bits are 1 - indicates that no TDD config was present.
  if ((bits_0_to_7 == 0xFF) && ((bits_8_to_11 & 0xF0) == 0xF0)) {
    //If no TDD config present - use all slots for Sidelink.
    //Spec not clear -- TBD....
    return 0;
  }

  //Bit A0 if 1 indicates pattern2 as present.
  if (bits_0_to_7 & 0x80) {
    //Pattern1 and Pattern2 Present.
    TDD_UL_DL_Config->pattern2 = malloc16_clear(sizeof(*TDD_UL_DL_Config->pattern2));
    AssertFatal(1==0,"Decoding Pattern2 - NOT YET IMPLEMENTED\n");
  } else {

    //Only Pattern1 Present. bits a1..a4 identify the periodicity.
    uint8_t val = (bits_0_to_7 & 0x78) >> 3;
    if (val >= 7)
      TDD_UL_DL_Config->pattern1.dl_UL_TransmissionPeriodicity = val-1;

    if (val == 6) {
      if (TDD_UL_DL_Config->pattern1.ext1 == NULL)
        TDD_UL_DL_Config->pattern1.ext1 = calloc(1, sizeof(*TDD_UL_DL_Config->pattern1.ext1));
      if (TDD_UL_DL_Config->pattern1.ext1->dl_UL_TransmissionPeriodicity_v1530 == NULL)
        TDD_UL_DL_Config->pattern1.ext1->dl_UL_TransmissionPeriodicity_v1530 = calloc(1, sizeof(long));
      *TDD_UL_DL_Config->pattern1.ext1->dl_UL_TransmissionPeriodicity_v1530 = 1;
    }

    //a5,a6..a11 bits from the 7th to 1st LSB of num SL slots
    num_SL_slots = ((bits_0_to_7 & 0x07) << 4 ) | ((bits_8_to_11 & 0xF0) >> 4);

    TDD_UL_DL_Config->pattern1.nrofUplinkSlots = num_SL_slots;
    TDD_UL_DL_Config->pattern1.nrofUplinkSymbols = mixed_slot_numsym;

    LOG_D(MAC, "SIDELINK: EXtracted TDD config from 12 bits - Sidelink Slots:%ld, Mixed_slot_symbols:%ld,dl_UL_TransmissionPeriodicity:%ld\n",
                                TDD_UL_DL_Config->pattern1.nrofUplinkSlots, TDD_UL_DL_Config->pattern1.nrofUplinkSymbols,
                                TDD_UL_DL_Config->pattern1.dl_UL_TransmissionPeriodicity);
  }
  return 1;
}

/*Function used to prepare Sidelink MIB*/
uint32_t sl_prepare_MIB(NR_TDD_UL_DL_ConfigCommon_t *TDD_UL_DL_Config,
                        uint8_t incoverage, uint8_t mu,
                        uint8_t start_symbol, uint8_t L) {

  uint8_t  sl_mib_payload[4] = {0,0,0,0};
  //int mu = UE->sl_frame_params.numerology_index, start_symbol = UE->start_symbol;
  uint8_t byte0, byte1;
  //int L = (UE->sl_frame_params.Ncp == 0) ? 14 : 12;
  uint32_t sl_mib=0;

  sl_prepare_psbch_payload(TDD_UL_DL_Config, &byte0, &byte1, mu, L, start_symbol);
  sl_mib_payload[0] = byte0;
  sl_mib_payload[1] = byte1;

  AssertFatal(incoverage <= 1, "Invalid value for incoverage paramter for SL-MIB. Accepted values 0 or 1\n");
  sl_mib_payload[1] |= (incoverage << 3);

  sl_mib =  sl_mib_payload[1]<<8  | sl_mib_payload[0];

#ifdef SL_DEBUG
  printf("SIDELINK PSBCH SIM: NUM SYMBOLS:%d, mu:%d, start_symbol:%d incoverage:%d \n",
                                      L, mu, start_symbol, incoverage);
  printf("SIDELINK PSBCH PAYLOAD: psbch_a:%x, sl_mib_payload:%x %x %x %x\n",
                                sl_mib, sl_mib_payload[0],sl_mib_payload[1], sl_mib_payload[2], sl_mib_payload[3]);
#endif

  return sl_mib;
}

uint16_t sl_get_subchannel_size(NR_SL_ResourcePool_r16_t *rpool)
{

  uint16_t subch_size = 0;
  const uint8_t subchsizes[8] = {10, 12, 15, 20, 25, 50, 75, 100};
  subch_size = (rpool->sl_SubchannelSize_r16)
                   ? subchsizes[*rpool->sl_SubchannelSize_r16] : 0;

  AssertFatal(subch_size,"Subch Size cannot be 0.Resource Pool Configuration Error\n");

  return subch_size;
}

uint16_t sl_get_num_subch(NR_SL_ResourcePool_r16_t *rpool)
{

  uint16_t num_subch = 0;
  uint16_t subch_size = sl_get_subchannel_size(rpool);
  uint16_t num_rbs = (rpool->sl_RB_Number_r16) ? *rpool->sl_RB_Number_r16 : 0;

  AssertFatal(num_rbs,"NumRbs in rpool cannot be 0.Resource Pool Configuration Error\n");

  num_subch = num_rbs/subch_size;

  LOG_D(NR_MAC, "Subch_size:%d, numRBS:%d, num_subch:%d\n",
                                          subch_size, num_rbs, num_subch);

  return (num_subch);
}

//This function determines SCI 1A Len in bits based on the configuration in the resource pool.
uint8_t sl_determine_sci_1a_len(uint16_t *num_subchannels,
                                NR_SL_ResourcePool_r16_t *rpool,
                                sidelink_sci_format_1a_fields_t *sci_1a)
{

  uint8_t num_bits = 0;

  //Size of Fixed fields prio (3), sci_2ndstage(2),
  //betaoffsetindicator(2), num dmrs ports (1), mcs (5bits)
  uint8_t sci_1a_len = SL_SCI_FORMAT_1A_LEN_IN_BITS_FIXED_FIELDS;

  *num_subchannels = sl_get_num_subch(rpool);

  uint16_t n_subch = *num_subchannels;

  LOG_D(NR_MAC,"Determine SCI-1A len - Num Subch:%d, sci 1A len fixed fields:%d\n",
                                                           *num_subchannels, sci_1a_len);

  NR_SL_UE_SelectedConfigRP_r16_t *selectedconfigRP = rpool->sl_UE_SelectedConfigRP_r16;
  const uint8_t maxnum_values[] = {2,3};
  uint8_t sl_MaxNumPerReserve =   (selectedconfigRP &&
                                   selectedconfigRP->sl_MaxNumPerReserve_r16)
                                   ? maxnum_values[*selectedconfigRP->sl_MaxNumPerReserve_r16]
                                   : 0;

  //Determine bits for Freq and Time Resource assignment
  if (sl_MaxNumPerReserve == 3) {
    num_bits = ceil(log2(n_subch * (n_subch + 1) * (2*n_subch + 1)/6));
    sci_1a_len += num_bits;
    sci_1a->frequency_resource_assignment.nbits = num_bits;
    sci_1a_len += 9;
    sci_1a->time_resource_assignment.nbits = 9;
  } else {
    num_bits = ceil(log2((n_subch * (n_subch + 1)) >> 1));
    sci_1a_len += num_bits;
    sci_1a->frequency_resource_assignment.nbits = num_bits;
    sci_1a_len += 5;
    sci_1a->time_resource_assignment.nbits = 5;
  }

  LOG_D(NR_MAC,"sci 1A - sl_MaxNumPerReserve:%d, sci 1a len:%d, FRA nbits:%d, TRA nbits:%d\n",
                                                                    sl_MaxNumPerReserve,sci_1a_len,
                                                                    sci_1a->frequency_resource_assignment.nbits,
                                                                    sci_1a->time_resource_assignment.nbits);

  //Determine bits for res reservation period
  uint8_t n_rsvperiod =  (selectedconfigRP &&
                          selectedconfigRP->sl_ResourceReservePeriodList_r16)
                          ? selectedconfigRP->sl_ResourceReservePeriodList_r16->list.count : 0;

  #define SL_IE_ENABLED 0
  if (selectedconfigRP &&
      selectedconfigRP->sl_MultiReserveResource_r16 == SL_IE_ENABLED) {
    num_bits = ceil(log2(n_rsvperiod));
    sci_1a_len += num_bits;
    sci_1a->resource_reservation_period.nbits = num_bits;
  } else
    sci_1a->resource_reservation_period.nbits = 0;

  LOG_D(NR_MAC,"sci 1A - n_rsvperiod:%d, sci 1a len:%d, res reserve period.nbits:%d\n",
                                                      n_rsvperiod, sci_1a_len,
                                                      sci_1a->resource_reservation_period.nbits);


  uint8_t n_dmrspatterns = 0;
  if (rpool->sl_PSSCH_Config_r16 &&
      rpool->sl_PSSCH_Config_r16->present == NR_SetupRelease_SL_PSSCH_Config_r16_PR_setup) {
    NR_SL_PSSCH_Config_r16_t *pssch_cfg = rpool->sl_PSSCH_Config_r16->choice.setup;

    //Determine bits for DMRS PATTERNS
    n_dmrspatterns = (pssch_cfg && pssch_cfg->sl_PSSCH_DMRS_TimePatternList_r16)
                         ? pssch_cfg->sl_PSSCH_DMRS_TimePatternList_r16->list.count : 0;
  }

  AssertFatal((n_dmrspatterns>=1) && (n_dmrspatterns <=3),
                          "Number of DMRS Patterns should be 1or2or3. Resource Pool Configuration Error.\n");

  if (n_dmrspatterns) {
    num_bits = ceil(log2(n_dmrspatterns));
    sci_1a_len += num_bits;
    sci_1a->dmrs_pattern.nbits = num_bits;
  }

  LOG_D(NR_MAC,"sci 1A -  n_dmrspatterns:%d, sci 1a len:%d, dmrs_pattern.nbits:%d\n",
                                                  n_dmrspatterns, sci_1a_len, sci_1a->dmrs_pattern.nbits);

  //Determine bits for Additional MCS table
  if (rpool->sl_Additional_MCS_Table_r16) {
    int numbits = (*rpool->sl_Additional_MCS_Table_r16 > 1) ? 2 : 1;
    sci_1a_len += numbits;
    sci_1a->additional_mcs_table_indicator.nbits = numbits;
    AssertFatal(*rpool->sl_Additional_MCS_Table_r16<=2, "additional table value cannot be > 2. Resource Pool Configuration Error.\n");
  }

  LOG_D(NR_MAC,"sci 1A - additional_table:%ld, sci 1a len:%d, additional table nbits:%d\n",
                                                                rpool->sl_Additional_MCS_Table_r16 ? *rpool->sl_Additional_MCS_Table_r16 : 0,
                                                                sci_1a_len,
                                                                sci_1a->additional_mcs_table_indicator.nbits);

  uint8_t psfch_period = 0;
  if (rpool->sl_PSFCH_Config_r16 &&
      rpool->sl_PSFCH_Config_r16->present == NR_SetupRelease_SL_PSFCH_Config_r16_PR_setup) {
    NR_SL_PSFCH_Config_r16_t *psfch_config = rpool->sl_PSFCH_Config_r16->choice.setup;

    //Determine bits for PSFCH overhead indication
    const uint8_t psfch_periods[] = {0,1,2,4};
    psfch_period = (psfch_config->sl_PSFCH_Period_r16)
                          ? psfch_periods[*psfch_config->sl_PSFCH_Period_r16] : 0;
  }

  if ((psfch_period == 2) || (psfch_period == 4)) {
    sci_1a_len += 1;
    sci_1a->psfch_overhead_indication.nbits = 1;
  } else
    sci_1a->psfch_overhead_indication.nbits = 0;

  LOG_D(NR_MAC,"sci 1A - psfch_period:%d, sci 1a len:%d, psfch overhead nbits:%d\n",
                                                            psfch_period, sci_1a_len,
                                                            sci_1a->psfch_overhead_indication.nbits);

  //Determine number of reserved bits
  uint8_t num_reservedbits =  0;
  if (rpool->sl_PSCCH_Config_r16 &&
      rpool->sl_PSCCH_Config_r16->present == NR_SetupRelease_SL_PSCCH_Config_r16_PR_setup) {
    NR_SL_PSCCH_Config_r16_t *pscch_config = rpool->sl_PSCCH_Config_r16->choice.setup;

    num_reservedbits = (pscch_config->sl_NumReservedBits_r16)
                          ? *pscch_config->sl_NumReservedBits_r16 : 0;
  }

  AssertFatal((num_reservedbits>=2) || (num_reservedbits<=4) ,
                      "Num Reserved bits can only be 2or3or4. Resource Pool Configuration Error.\n");
  sci_1a_len += num_reservedbits;
  sci_1a->reserved_bits.nbits = num_reservedbits;
  LOG_D(NR_MAC,"sci 1A - reserved_bits:%d, sci 1a len:%d, sci_1a->reserved_bits.nbits:%d\n",
                                                        num_reservedbits, sci_1a_len, sci_1a->reserved_bits.nbits);


  LOG_D(NR_MAC,"sci 1A Length in bits: %d \n",sci_1a_len);

  return sci_1a_len;
}

uint8_t count_on_bits(uint8_t* buf, size_t size) {
  uint8_t count = 0;
  uint8_t byte;
  for (size_t i = 0; i < size; i++) {
    byte = buf[i];
    while(byte) {
      count += byte & 1;
      byte >>= 1;
    }
  }
  return count;
}

static void compute_params(int module_idP, psfch_params_t* psfch_params) {
  NR_UE_MAC_INST_t *mac = get_mac_inst(module_idP);
  if (!mac->sl_tx_res_pool->sl_PSFCH_Config_r16 &&
      mac->sl_tx_res_pool->sl_PSFCH_Config_r16->present != NR_SetupRelease_SL_PSFCH_Config_r16_PR_setup)
      return;

  psfch_params->prbs_sets = calloc(1, sizeof(prbs_set_t));
  NR_SL_PSFCH_Config_r16_t *sl_psfch_config = mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup;
  const int sl_num_muxcs_pair[4] = {1, 2, 3, 6};
  uint8_t sci2_src_id = mac->sci_pdu_rx.source_id;
  uint8_t *rb_buf = sl_psfch_config->sl_PSFCH_RB_Set_r16->buf;
  size_t size = sl_psfch_config->sl_PSFCH_RB_Set_r16->size / sizeof(rb_buf[0]);
  uint8_t m_psfch_prb_set = count_on_bits(rb_buf, size);
  long sl_numsubchannel = *mac->sl_tx_res_pool->sl_NumSubchannel_r16;
  const uint8_t psfch_periods[] = {0,1,2,4};
  long n_psfch_pssch = (sl_psfch_config->sl_PSFCH_Period_r16)
                        ? psfch_periods[*sl_psfch_config->sl_PSFCH_Period_r16] : 0;
  long n_psfch_cs = *sl_psfch_config->sl_NumMuxCS_Pair_r16;

  double m_psfch_subch_slot = m_psfch_prb_set / (sl_numsubchannel * n_psfch_pssch);
  // FIXME: Add second condition from spec. 38213 16.3, current implementation assuming single subchannel
  long n_psfch_type = *sl_psfch_config->sl_PSFCH_CandidateResourceType_r16 ? sl_numsubchannel : 1;
  uint16_t r_psfch_prb_cs = n_psfch_type * m_psfch_subch_slot * sl_num_muxcs_pair[n_psfch_cs];
  uint8_t psfch_rsc_idx = (sci2_src_id + mac->src_id) / r_psfch_prb_cs;
  LOG_D(NR_MAC, "sci2_src_id %d, UE id %d\n", sci2_src_id, mac->src_id);
  LOG_D(NR_MAC, "size %lu, m_psfch_prb_set %d, sl_numsubchannel %ld, n_psfch_pssch %ld, n_psfch_cs %d\n", size, m_psfch_prb_set, sl_numsubchannel, n_psfch_pssch, sl_num_muxcs_pair[n_psfch_cs]);
  LOG_D(NR_MAC, "m_psfch_subch_slot %f, n_psfch_type %ld, r_psfch_prb_cs %d, psfch_rsc_idx %d\n", m_psfch_subch_slot, n_psfch_type, r_psfch_prb_cs, psfch_rsc_idx);
  psfch_params->m0 = table_16_3_1[n_psfch_cs][psfch_rsc_idx];

  // 38213 16.3 Compute PRB allocation
  psfch_params->prbs_sets->start_prb = (uint16_t**)calloc(n_psfch_pssch, sizeof(uint16_t*));
  psfch_params->prbs_sets->end_prb = (uint16_t**)calloc(n_psfch_pssch, sizeof(uint16_t*));
  for (int k=0; k<n_psfch_pssch; k++) {
    psfch_params->prbs_sets->start_prb[k] = (uint16_t*)calloc(sl_numsubchannel, sizeof(uint16_t));
    psfch_params->prbs_sets->end_prb[k] = (uint16_t*)calloc(sl_numsubchannel, sizeof(uint16_t));
  }
  for (int i = 0; i < n_psfch_pssch; i++) {
    for (int j = 0; j < sl_numsubchannel; j++) {
      psfch_params->prbs_sets->start_prb[i][j] = (i + j * n_psfch_pssch) * m_psfch_subch_slot;
      psfch_params->prbs_sets->end_prb[i][j] = (i + 1 + j * n_psfch_pssch) * m_psfch_subch_slot - 1;
    }
  }
}

void configure_psfch_params_tx(int module_idP,
                               NR_UE_MAC_INST_t *mac,
                               sl_nr_rx_indication_t *rx_ind,
                               int pdu_id)
{
  // TODO: May need to update in case of multiple UEs  !!!!Jin!!!
  const uint8_t psfch_periods[] = {0,1,2,4};
  NR_SL_PSFCH_Config_r16_t *sl_psfch_config = mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup;
  long psfch_period = (sl_psfch_config->sl_PSFCH_Period_r16)
                        ? psfch_periods[*sl_psfch_config->sl_PSFCH_Period_r16] : 0;

  int scs = get_softmodem_params()->numerology;
  uint16_t tx_slot = (rx_ind->slot + DURATION_RX_TO_TX) % nr_slots_per_frame[scs];
  uint16_t tx_frame = (rx_ind->sfn + (rx_ind->slot + DURATION_RX_TO_TX) / nr_slots_per_frame[scs]) % 1024;

  uint8_t ack_nack = (rx_ind->rx_indication_body + pdu_id)->rx_slsch_pdu.ack_nack;
  LOG_D(NR_MAC, "tx_frame %4u.%2u, ack_nack %d rx: %4u.%2u\n", tx_frame, tx_slot, ack_nack, rx_ind->sfn, rx_ind->slot);
  psfch_params_t *psfch_params = calloc(1, sizeof(psfch_params_t));
  compute_params(module_idP, psfch_params);
  const int nr_slots_frame = nr_slots_per_frame[scs];
  int psfch_index = nr_ue_sl_acknack_scheduling(mac, rx_ind, psfch_period, tx_frame, tx_slot, nr_slots_frame);
  if (psfch_index != -1)
    fill_psfch_params_tx(mac, rx_ind, psfch_period, tx_frame, tx_slot, ack_nack, psfch_params, nr_slots_frame, psfch_index);
  free(psfch_params);
  psfch_params = NULL;
}

int get_psfch_index(int frame, int slot, int n_slots_frame, const NR_TDD_UL_DL_Pattern_t *tdd, int sched_psfch_max_size)
{
  // PUCCH structures are indexed by slot in the PUCCH period determined by sched_psfch_max_size number of UL slots
  // this functions return the index to the structure for slot passed to the function

  const int first_ul_slot_period = tdd ? get_first_ul_slot(tdd->nrofDownlinkSlots, tdd->nrofDownlinkSymbols, tdd->nrofUplinkSymbols) : 0;
  const int n_ul_slots_period = tdd ? tdd->nrofUplinkSlots + (tdd->nrofUplinkSymbols > 0 ? 1 : 0) : n_slots_frame;
  const int nr_slots_period = tdd ? n_slots_frame / get_nb_periods_per_frame(tdd->dl_UL_TransmissionPeriodicity) : n_slots_frame;
  const int n_ul_slots_frame = n_slots_frame / nr_slots_period * n_ul_slots_period;
  // (frame * n_ul_slots_frame) adds up the number of UL slots in the previous frames
  const int frame_start      = frame * n_ul_slots_frame;
  // ((slot / nr_slots_period) * n_ul_slots_period) adds up the number of UL slots in the previous TDD periods of this frame
  const int ul_period_start  = (slot / nr_slots_period) * n_ul_slots_period;
  // ((slot % nr_slots_period) - first_ul_slot_period) gives the progressive number of the slot in this TDD period
  const int ul_period_slot   = (slot % nr_slots_period) - first_ul_slot_period;
  // the sum gives the index of current UL slot in the frame which is normalized wrt sched_psfch_max_size
  return (frame_start + ul_period_start + ul_period_slot) % sched_psfch_max_size;

}

int get_pssch_to_harq_feedback(uint8_t *pssch_to_harq_feedback, uint8_t psfch_min_time_gap, NR_TDD_UL_DL_Pattern_t *tdd, const int nr_slots_frame) {
  int n_ul_slots_period = tdd ? tdd->nrofUplinkSlots + (tdd->nrofUplinkSymbols > 0 ? 1 : 0) : nr_slots_frame;
  for (int i = 0; i < n_ul_slots_period; i++) {
    pssch_to_harq_feedback[i] = psfch_min_time_gap + i + 1;
  }
  return n_ul_slots_period;
}

void get_csirs_to_csi_report(uint8_t *csirs_to_csi_report, uint8_t sl_latencyboundcsi_report, const int nr_slots_frame) {
  for (int i = 0; i < sl_latencyboundcsi_report; i++)
    csirs_to_csi_report[i] = i + 1;
}

int get_feedback_frame_slot(NR_UE_MAC_INST_t *mac, NR_TDD_UL_DL_Pattern_t *tdd,
                            uint8_t feedback_offset, uint8_t psfch_min_time_gap,
                            const int nr_slots_frame, uint16_t frame, uint16_t slot,
                            long psfch_period, int *psfch_frame, int *psfch_slot) {

  AssertFatal(tdd != NULL, "Expecting valid tdd configurations");
  const int first_ul_slot_period = tdd ? get_first_ul_slot(tdd->nrofDownlinkSlots, tdd->nrofDownlinkSymbols, tdd->nrofUplinkSymbols) : 0;
  const int nr_slots_period = tdd ? nr_slots_frame / get_nb_periods_per_frame(tdd->dl_UL_TransmissionPeriodicity) : nr_slots_frame;
  // can't schedule ACKNACK before minimum feedback time
  if(feedback_offset < psfch_min_time_gap)
    return -1;

  *psfch_slot = (slot + feedback_offset) % nr_slots_frame;
  // check if the slot is UL
  if(*psfch_slot % nr_slots_period < first_ul_slot_period)
    return -1;

  if (*psfch_slot % psfch_period > 0)
    return -1;

  *psfch_frame = (frame + ((slot + feedback_offset) / nr_slots_frame)) & 1023;

  return 0;
}

int16_t get_feedback_slot(long psfch_period, uint16_t slot) {
  int16_t feedback_slot = -1;
  if (psfch_period == 1) {
    switch(slot) {
      case 0:
        feedback_slot = 6;
      break;
      case 1:
        feedback_slot = 7;
      break;
      case 2:
        feedback_slot = 8;
      break;
      case 3:
        feedback_slot = 9;
      break;
      case 10:
        feedback_slot = 16;
      break;
      case 11:
        feedback_slot = 17;
      break;
      case 12:
        feedback_slot = 18;
      break;
      case 13:
        feedback_slot = 19;
      break;
      default:
        AssertFatal(1 == 0, "Invalid slot %d\n", slot);
    }
  } else if (psfch_period == 2) {
    switch(slot) {
      case 0:
      case 1:
        feedback_slot = 7;
      break;
      case 2:
      case 3:
        feedback_slot = 9;
      break;
      case 10:
      case 11:
        feedback_slot = 17;
      break;
      case 12:
      case 13:
        feedback_slot = 19;
      break;
      default:
        AssertFatal(1 == 0, "Invalid slot %d\n", slot);
    }
  } else if (psfch_period == 4) {
    switch(slot) {
      case 0:
      case 1:
      case 2:
      case 3:
        feedback_slot = 9;
      break;
      case 10:
      case 11:
      case 12:
      case 13:
        feedback_slot = 19;
      break;
      default:
        AssertFatal(1 == 0, "Invalid slot %d\n", slot);
    }
  }
  return feedback_slot;
}

int nr_ue_sl_acknack_scheduling(NR_UE_MAC_INST_t *mac, sl_nr_rx_indication_t *rx_ind,
                                long psfch_period, uint16_t frame, uint16_t slot, const int nr_slots_frame) {
  LOG_I(NR_MAC,//Jin add
      "[JIN DEBUG SL ACKNAKC SL-RX-IND] me=%u rx_ind sfn=%u slot=%u num_pdus=%u\n",
      mac->src_id, rx_ind->sfn, rx_ind->slot, rx_ind->number_pdus);
  // TODO: needs to be updated for multi-subchannels  Jin : ?????
  int psfch_frame, psfch_slot;
  sl_nr_ue_mac_params_t *sl_mac =  mac->SL_MAC_PARAMS;
  NR_TDD_UL_DL_Pattern_t *tdd = &sl_mac->sl_TDD_config->pattern1;
  const int n_ul_slots_period = tdd ? tdd->nrofUplinkSlots + (tdd->nrofUplinkSymbols > 0 ? 1 : 0) : nr_slots_frame;

  uint16_t num_subch = sl_get_num_subch(mac->sl_tx_res_pool);
  int n_ul_buf_max_size = n_ul_slots_period * num_subch;

  psfch_slot = get_feedback_slot(psfch_period, slot);
  const int psfch_index = get_psfch_index(rx_ind->sfn, rx_ind->slot, nr_slots_frame, tdd, n_ul_buf_max_size);
  //NR_SL_UE_sched_ctrl_t  *sched_ctrl = &mac->sl_info.list[0]->UE_sched_ctrl; //Jin replace
  //Jin patch
  const uint16_t peer_id = mac->sci_pdu_rx.source_id;
  NR_SL_UE_info_t *peer = find_UE(mac, peer_id);
  if (!peer || !peer->UE_sched_ctrl.sched_psfch) {
    LOG_W(NR_MAC, "%s: no peer or sched_psfch for peer_id=%u\n", __FUNCTION__, peer_id);
    return -1;
  }
    NR_SL_UE_sched_ctrl_t *sched_ctrl = &peer->UE_sched_ctrl;
  //Jin end

  SL_sched_feedback_t  *curr_psfch = &sched_ctrl->sched_psfch[psfch_index];
  psfch_frame = frame;
  frameslot_t fs;
  fs.frame = psfch_frame;
  fs.slot = psfch_slot;
  uint8_t pool_id = 0;
  uint64_t tx_abs_slot = normalize(&fs, get_softmodem_params()->numerology);
  SL_ResourcePool_params_t *sl_tx_rsrc_pool = sl_mac->sl_TxPool[pool_id];
  size_t phy_map_sz = (sl_tx_rsrc_pool->phy_sl_bitmap.size << 3) - sl_tx_rsrc_pool->phy_sl_bitmap.bits_unused;
  bool sl_has_psfch = slot_has_psfch(mac, &sl_tx_rsrc_pool->phy_sl_bitmap, tx_abs_slot, psfch_period, phy_map_sz, mac->SL_MAC_PARAMS->sl_TDD_config);
  LOG_D(NR_MAC, "%s %4d.%2d sl_has_psfch %d\n", __FUNCTION__, psfch_frame, psfch_slot, sl_has_psfch);
  curr_psfch->feedback_frame = psfch_frame;
  curr_psfch->feedback_slot = psfch_slot;
  curr_psfch->dai_c = psfch_index;
  LOG_D(NR_MAC, "Rx SLSCH %4d.%2d, SL_ACK %4d.%2d in current PSFCH: psfch_index %d, n_ul_slots_period %d dai_c %u curr_psfch %p\n",
        rx_ind->sfn,
        rx_ind->slot,
        psfch_frame,
        psfch_slot,
        psfch_index,
        n_ul_slots_period,
        curr_psfch->dai_c,
        curr_psfch);
  LOG_D(NR_MAC, "SL %4d.%2d, Couldn't find scheduling occasion for this HARQ process\n", rx_ind->sfn, rx_ind->slot);
  return psfch_index;
}

void fill_psfch_params_tx(NR_UE_MAC_INST_t *mac, sl_nr_rx_indication_t *rx_ind,
                          long psfch_period, uint16_t frame, uint16_t slot,
                          uint8_t ack_nack, psfch_params_t *psfch_params,
                          const int nr_slots_frame, int psfch_index) {

  NR_SL_BWP_Generic_r16_t *sl_bwp = mac->sl_bwp->sl_BWP_Generic_r16;

  //SL_sched_feedback_t  *sched_psfch = &mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch[psfch_index];  //Jin replace
  //Jin replace
 
  const uint16_t peer_id = mac->sci_pdu_rx.source_id;
  NR_SL_UE_info_t *peer = find_UE(mac, peer_id);
  if (!peer) {
    LOG_W(NR_MAC, "%s: no peer struct for source_id=%u (psfch_index=%d)\n",
          __FUNCTION__, peer_id, psfch_index);
    return;
  }
  if (!peer->UE_sched_ctrl.sched_psfch) {
    LOG_W(NR_MAC, "%s: peer_id=%u has NULL sched_psfch\n",
          __FUNCTION__, peer_id);
    return;
  }
  const int n = sl_num_slsch_feedbacks(mac);
  if (psfch_index < 0 || psfch_index >= n) {
    LOG_W(NR_MAC, "%s: psfch_index=%d out of range (n=%d) peer_id=%u\n",
          __FUNCTION__, psfch_index, n, peer_id);
    return;
  }
  SL_sched_feedback_t *sched_psfch = &peer->UE_sched_ctrl.sched_psfch[psfch_index];
  //jIN END



  LOG_D(NR_MAC, "psfch_period %ld, feedback frame:slot %d:%d, frame:slot %d:%d, harq feedback %d psfch_index %d\n",
        psfch_period,
        sched_psfch->feedback_frame,
        sched_psfch->feedback_slot,
        rx_ind->sfn,
        rx_ind->slot,
        mac->sci_pdu_rx.harq_feedback,
        psfch_index);
  sched_psfch->initial_cyclic_shift = psfch_params->m0;
  if ((mac->sci1_pdu.second_stage_sci_format == 0 && (mac->sci_pdu_rx.cast_type == 1 ||
      mac->sci_pdu_rx.cast_type == 2)) || mac->sci1_pdu.second_stage_sci_format == 2) {
    sched_psfch->mcs = sequence_cyclic_shift_harq_ack_or_ack_or_only_nack[ack_nack];
    sched_psfch->bit_len_harq = 1;
    LOG_D(NR_MAC, "mcs %i, ack_nack: %i, sched_psfch->initial_cyclic_shift %i\n",
          sched_psfch->mcs, ack_nack, sched_psfch->initial_cyclic_shift);
  } else if (mac->sci1_pdu.second_stage_sci_format == 1 ||
            (mac->sci1_pdu.second_stage_sci_format == 0 && mac->sci_pdu_rx.cast_type == 3)) {
    sched_psfch->mcs = sequence_cyclic_shift_harq_ack_or_ack_or_only_nack[0];
    sched_psfch->bit_len_harq = 0;
  }
  const uint8_t values[] = {7, 8, 9, 10, 11, 12, 13, 14};
  uint8_t sl_num_symbols = *sl_bwp->sl_LengthSymbols_r16 ? values[*sl_bwp->sl_LengthSymbols_r16] : 0;
  // start_symbol_index has been used as lprime check 38.213 16.3
  sched_psfch->start_symbol_index = *sl_bwp->sl_StartSymbol_r16 + sl_num_symbols - 2;
  LOG_D(NR_PHY, "sl_StartSymbol_r16 %ld, sl_num_symbols: %d, start sym index %d, mcs %d\n",
        *sl_bwp->sl_StartSymbol_r16, sl_num_symbols, sched_psfch->start_symbol_index, sched_psfch->mcs);
  sched_psfch->hopping_id = *mac->sl_bwp->sl_BWP_PoolConfigCommon_r16->sl_TxPoolSelectedNormal_r16->list.array[0]->sl_ResourcePool_r16->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_HopID_r16;
  sched_psfch->prb = psfch_params->prbs_sets->start_prb[rx_ind->slot % psfch_period][0]; // FIXME [0] is based on assumption of number of subchannels = 1; 0 is channel id
  print_prb_set_allocation(psfch_params, psfch_period, 1);
  LOG_D(NR_PHY, "slot %d, slot mode psfch_period %ld, sched_psfch->prb %d, start_prb %d\n",
        rx_ind->slot, rx_ind->slot%psfch_period, sched_psfch->prb,
        psfch_params->prbs_sets->start_prb[rx_ind->slot%psfch_period][0]);
  int locbw = sl_bwp->sl_BWP_r16->locationAndBandwidth;
  sched_psfch->sl_bwp_start   = NRRIV2PRBOFFSET(locbw, MAX_BWP_SIZE);
  sched_psfch->freq_hop_flag  = 0;
  sched_psfch->group_hop_flag = 0;
  sched_psfch->second_hop_prb = 0;
  sched_psfch->sequence_hop_flag = 0;
  sched_psfch->harq_feedback = mac->sci_pdu_rx.harq_feedback;
  LOG_D(NR_MAC, "Filled psfch pdu\n");
}

int find_current_slot_harqs(frame_t frame, sub_frame_t slot, NR_SL_UE_sched_ctrl_t * sched_ctrl, NR_UE_sl_harq_t **matched_harqs)
{
  int cur = sched_ctrl->feedback_sl_harq.head;
  int k = 0;
  while (cur != -1) {
    NR_UE_sl_harq_t *harq = &sched_ctrl->sl_harq_processes[cur];
    LOG_D(NR_MAC, "%s %4d.%2d feedback %4d.%2d\n", __FUNCTION__, frame, slot, harq->feedback_frame, harq->feedback_slot);
    if (harq->feedback_frame == frame && harq->feedback_slot == slot) {
      if (matched_harqs) {
        matched_harqs[k] = harq;
        LOG_D(NR_MAC, "%s matched_harqs[%d] %4d.%2d %d slot %d\n",
              __FUNCTION__,
              k,
              matched_harqs[k]->feedback_frame,
              matched_harqs[k]->feedback_slot,
              matched_harqs[k]->sl_harq_pid,
              matched_harqs[k]->sched_pssch.slot);
      }
      k++;
    }
    cur = sched_ctrl->feedback_sl_harq.next[cur];
  }
  return k;
}

/*
Following function is used to remove the harq_pids from the feedback list;
Adds back to available list or retransmission list based on round value.
*/

void update_harq_lists(NR_UE_MAC_INST_t *mac, frame_t frame, sub_frame_t slot, NR_SL_UE_info_t* UE)
{
  NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  int cur = sched_ctrl->feedback_sl_harq.head;
  while (cur != -1) {
    NR_UE_sl_harq_t *harq = &sched_ctrl->sl_harq_processes[cur];
    if ((harq->feedback_frame < frame
         || (harq->feedback_frame == frame && harq->feedback_slot < slot))) {
      remove_nr_list(&sched_ctrl->feedback_sl_harq, cur);
      harq->feedback_slot = -1;
      harq->is_waiting = false;

      //Jin disactive
      /*
      if (harq->round >= HARQ_ROUND_MAX - 1) {
        abort_nr_ue_sl_harq(mac, cur, UE);
      } else {
        add_tail_nr_list(&sched_ctrl->retrans_sl_harq, cur);
        harq->round++;
      }
      */
    }
    cur = sched_ctrl->feedback_sl_harq.next[cur];
  }
}

void configure_psfch_params_rx(int module_idP,
                            NR_UE_MAC_INST_t *mac,
                            sl_nr_rx_config_request_t *rx_config)
{
  const uint16_t slot = rx_config->slot;
  frame_t frame = rx_config->sfn;
  const uint8_t psfch_periods[] = {0,1,2,4};
  NR_SL_PSFCH_Config_r16_t *sl_psfch_config = mac->sl_rx_res_pool->sl_PSFCH_Config_r16->choice.setup;
  long psfch_period = (sl_psfch_config->sl_PSFCH_Period_r16)
                        ? psfch_periods[*sl_psfch_config->sl_PSFCH_Period_r16] : 0;
  uint16_t num_subch = sl_get_num_subch(mac->sl_rx_res_pool);
  rx_config->sl_rx_config_list[0].rx_psfch_pdu_list = calloc(psfch_period*num_subch, sizeof(sl_nr_tx_rx_config_psfch_pdu_t));
  NR_SL_UEs_t *UE_info = &mac->sl_info;

  if (*(UE_info->list) == NULL) {
    LOG_D(NR_MAC, "UE list is empty\n");
    return;
  }

  psfch_params_t *psfch_params = calloc(1, sizeof(psfch_params_t));
  compute_params(module_idP, psfch_params);
  SL_UE_iterator(UE_info->list, UE) {
    NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    NR_UE_sl_harq_t **matched_harqs = (NR_UE_sl_harq_t **) calloc(sched_ctrl->feedback_sl_harq.len, sizeof(NR_UE_sl_harq_t *));
    //int matched_sz = find_current_slot_harqs(frame, slot, sched_ctrl, matched_harqs);
    int matched_sz = 0; //Jin desable SCI also 
    LOG_D(NR_MAC, "%s matched_sz %d\n", __FUNCTION__, matched_sz);
    rx_config->sl_rx_config_list[0].num_psfch_pdus = 0;
    for (int i = 0; i < matched_sz; i++) {
      AssertFatal(i < UE->UE_sched_ctrl.feedback_sl_harq.len, "k MUST be smaller than feedback_sl_harq length\n");
      AssertFatal(i < (psfch_period * num_subch), "k MUST be smaller than %ld\n", (psfch_period * num_subch));
      NR_UE_sl_harq_t *cur_harq = matched_harqs[i];
      sl_nr_tx_rx_config_psfch_pdu_t *psfch_pdu = &rx_config->sl_rx_config_list[0].rx_psfch_pdu_list[i];
      fill_psfch_params_rx(rx_config, psfch_pdu, psfch_params, cur_harq, mac, psfch_period, slot);
    }
    free(matched_harqs);
    matched_harqs = NULL;
  }
}

void fill_psfch_params_rx(sl_nr_rx_config_request_t *rx_config, sl_nr_tx_rx_config_psfch_pdu_t *psfch_pdu, psfch_params_t *psfch_params, NR_UE_sl_harq_t *cur_harq, NR_UE_MAC_INST_t *mac, long psfch_period, const uint16_t slot) {
  rx_config->sl_rx_config_list[0].num_psfch_pdus++;
  psfch_pdu->initial_cyclic_shift = psfch_params->m0;
  LOG_D(NR_MAC, "psfch_pdu->initial_cyclic_shift %i\n", psfch_pdu->initial_cyclic_shift);
  const uint8_t values[] = {7, 8, 9, 10, 11, 12, 13, 14};
  NR_SL_BWP_Generic_r16_t *sl_bwp = mac->sl_bwp->sl_BWP_Generic_r16;
  uint8_t sl_num_symbols = *sl_bwp->sl_LengthSymbols_r16 ? values[*sl_bwp->sl_LengthSymbols_r16] : 0;
  // start_symbol_index has been used as lprime check 38.213 16.3
  psfch_pdu->start_symbol_index = *sl_bwp->sl_StartSymbol_r16 + sl_num_symbols - 2;
  LOG_D(NR_PHY, "Rx sl_StartSymbol_r16 %ld, sl_num_symbols: %d, start sym index %d, mcs %d\n", *sl_bwp->sl_StartSymbol_r16, sl_num_symbols, psfch_pdu->start_symbol_index, psfch_pdu->mcs);
  psfch_pdu->hopping_id = *mac->sl_bwp->sl_BWP_PoolConfigCommon_r16->sl_TxPoolSelectedNormal_r16->list.array[0]->sl_ResourcePool_r16->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_HopID_r16;
  uint8_t index = cur_harq->sched_pssch.slot%psfch_period;
  psfch_pdu->prb = psfch_params->prbs_sets->start_prb[index][0]; // FIXME [0] is based on assumption of number of subchannels = 1; 0 is channel id
  print_prb_set_allocation(psfch_params, psfch_period, 1);
  LOG_D(NR_PHY, "Rx slot %d, slsch tx slot %d, tx slot mode psfch_period %d, start_prb %d\n", slot, cur_harq->sched_pssch.slot, index, psfch_params->prbs_sets->start_prb[index][0]);
  int locbw = sl_bwp->sl_BWP_r16->locationAndBandwidth;
  psfch_pdu->sl_bwp_start   = NRRIV2PRBOFFSET(locbw, MAX_BWP_SIZE);
  psfch_pdu->freq_hop_flag  = 0;
  psfch_pdu->group_hop_flag = 0;
  psfch_pdu->second_hop_prb = 0;
  psfch_pdu->sequence_hop_flag = 0;
  psfch_pdu->bit_len_harq = 1;
  int num_psfch_symbols = 0;
  if (psfch_period == 1) num_psfch_symbols = 3;
  else if (psfch_period == 2 || psfch_period == 4) {
    num_psfch_symbols = mac->SL_MAC_PARAMS->sl_RxPool[0]->sci_1a.psfch_overhead_indication.nbits ? 3 : 0;
  }
  psfch_pdu->nr_of_symbols = num_psfch_symbols ? num_psfch_symbols - 2 : 0; // (num_psfch_symbols - 2) excludes PSFCH AGC and Guard
  rx_config->sl_rx_config_list[0].pdu_type = SL_NR_CONFIG_TYPE_RX_PSSCH_SLSCH_PSFCH;
  LOG_D(NR_PHY, "%s start_symbol_index %d, sl_bwp_start %d, sequence_hop_flag %d, \
        second_hop_prb %d, prb %d, nr_of_symbols %d, initial_cyclic_shift %d, hopping_id %d, \
        group_hop_flag %d, freq_hop_flag %d, bit_len_harq %d----> Setting pdu type SL_NR_CONFIG_TYPE_RX_PSSCH_SLSCH_PSFCH  \n",
        __FUNCTION__,
        psfch_pdu->start_symbol_index, psfch_pdu->sl_bwp_start,
        psfch_pdu->sequence_hop_flag, psfch_pdu->second_hop_prb, psfch_pdu->prb,
        psfch_pdu->nr_of_symbols, psfch_pdu->initial_cyclic_shift, psfch_pdu->hopping_id,
        psfch_pdu->group_hop_flag, psfch_pdu->freq_hop_flag, psfch_pdu->bit_len_harq);
}

void set_csi_report_params(NR_UE_MAC_INST_t* mac, NR_SL_UE_sched_ctrl_t *sched_ctrl) {
  SL_CSI_Report_t *csi_report = &sched_ctrl->sched_csi_report;
  csi_report->cqi = mac->csirs_measurements.cqi;
  csi_report->ri = mac->csirs_measurements.rank_indicator;
}

uint8_t sl_num_slsch_feedbacks(NR_UE_MAC_INST_t *mac) {
  sl_nr_ue_mac_params_t *sl_mac =  mac->SL_MAC_PARAMS;
  NR_TDD_UL_DL_Pattern_t *tdd = &sl_mac->sl_TDD_config->pattern1;
  uint8_t scs = sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;
  const int nr_slots_frame = nr_slots_per_frame[scs];
  const int n_ul_slots_period = tdd ? tdd->nrofUplinkSlots + (tdd->nrofUplinkSymbols > 0 ? 1 : 0) : nr_slots_frame;
  uint16_t num_subch = sl_get_num_subch(mac->sl_tx_res_pool);
  return n_ul_slots_period * num_subch;
}

bool is_feedback_scheduled(NR_UE_MAC_INST_t *mac, int frameP,int slotP) {


  //for (int i = 0; i < sl_num_slsch_feedbacks(mac); i++) {    //Jin replace 
  //  SL_sched_feedback_t  *sched_psfch = &mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch[i];   //JIn replace 
  
  //Jin replace the loop begining 
  const int n = sl_num_slsch_feedbacks(mac);

  //NR_SL_UE_info_t *UE = NULL;  // required by some SL_UE_iterator macro variants
  SL_UE_iterator(mac->sl_info.list, UE) {
    SL_sched_feedback_t *arr = UE->UE_sched_ctrl.sched_psfch;
    if (!arr)
      continue;
    for (int i = 0; i < n; i++) {
      SL_sched_feedback_t *sched_psfch = &arr[i];
  //Jin end

      LOG_D(NR_MAC, "frame.slot %4d.%2d, harq_feedback %d\n", frameP, slotP, sched_psfch->harq_feedback);
      if (frameP == sched_psfch->feedback_frame && slotP == sched_psfch->feedback_slot && sched_psfch->harq_feedback) {
        return true;
        }
    }
  }
  return false;
}

void reset_sched_psfch(NR_UE_MAC_INST_t *mac, int frameP,int slotP) {
  //for (int i = 0; i < sl_num_slsch_feedbacks(mac); i++) {   //Jin replace
  //SL_sched_feedback_t  *sched_psfch = &mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch[i];   //jin replace
    //Jin replace whole loop beginning 
  const int n = sl_num_slsch_feedbacks(mac);
  SL_UE_iterator(mac->sl_info.list, UE) {

    SL_sched_feedback_t *arr = UE->UE_sched_ctrl.sched_psfch;
    if (!arr)
      continue;

    for (int i = 0; i < n; i++) {
      SL_sched_feedback_t *sched_psfch = &arr[i];
  //Jin end

      if (frameP == sched_psfch->feedback_frame && slotP == sched_psfch->feedback_slot) {
        sched_psfch->feedback_frame = -1;
        sched_psfch->feedback_slot = -1;
        sched_psfch->harq_feedback = 0;
        }
    }
  }
}

 
void nr_ue_process_mac_sl_pdu(int module_idP,
                              sl_nr_rx_indication_t *rx_ind,
                              int pdu_id)
{   
  int8_t pdu_type = (rx_ind->rx_indication_body + pdu_id)->pdu_type;
  sl_nr_slsch_pdu_t *rx_slsch_pdu = &(rx_ind->rx_indication_body + pdu_id)->rx_slsch_pdu;
  uint8_t *pduP          = rx_slsch_pdu->pdu;
  int32_t pdu_len        = (int32_t)rx_slsch_pdu->pdu_length;
  uint8_t done           = 0;


  NR_UE_MAC_INST_t *mac = get_mac_inst(module_idP);
  int frame = rx_ind->sfn;
  int slot = rx_ind->slot;
  if (!pduP){
    return;
  }

  // ---------------- Jin DEBUG: dump first bytes and scan for LCID=4 ----------------
  //LOG_I(NR_MAC, "[SL-RX-DBG] me=%u pdu_len=%d pdu_type=%d frame=%d slot=%d\n",
  //      mac->src_id, pdu_len, pdu_type, frame, slot);

  if (pdu_len >= 16) {
    //LOG_I(NR_MAC,
     //     "[SL-RX-DBG] pdu@0 first16: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
     //     pduP[0], pduP[1], pduP[2], pduP[3], pduP[4], pduP[5], pduP[6], pduP[7],
     //     pduP[8], pduP[9], pduP[10], pduP[11], pduP[12], pduP[13], pduP[14], pduP[15]);
  if (pdu_len >= 5) {
    uint8_t lcid_off4 = ((NR_MAC_SUBHEADER_FIXED *)(pduP + 4))->LCID;
   // LOG_I(NR_MAC, "[SL-RX-DBG] LCID at off=4 is %u (byte=%02x)\n", lcid_off4, pduP[4]);
  }


  } else if (pdu_len > 0) {
    // print up to 8 bytes if small
    int n = pdu_len < 8 ? pdu_len : 8;
    //LOG_I(NR_MAC, "[SL-RX-DBG] pdu@0 first%d:", n);
    for (int i = 0; i < n; i++) {
     // LOG_I(NR_MAC, " %02x", pduP[i]);
    }
  }

  // Scan offsets 0..15: where does the parser "see" LCID=4?
  // This matches your existing parsing: rx_lcid = ((NR_MAC_SUBHEADER_FIXED *)(pduP))->LCID;
  int max_scan = pdu_len < 256 ? pdu_len : 256;
  for (int off = 0; off < max_scan; off++) {
    uint8_t lcid = ((NR_MAC_SUBHEADER_FIXED *)(pduP + off))->LCID;
    if (lcid == 4) {
     // LOG_I(NR_MAC, "[SL-RX-DBG] FOUND LCID=4 at off=%d (byte=%02x)\n", off, pduP[off]);
      if (pdu_len - off >= 16) {
      //  LOG_I(NR_MAC,
      //        "[SL-RX-DBG] pdu@off first16: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
       //       pduP[off+0], pduP[off+1], pduP[off+2], pduP[off+3],
       //       pduP[off+4], pduP[off+5], pduP[off+6], pduP[off+7],
       //       pduP[off+8], pduP[off+9], pduP[off+10], pduP[off+11],
       //       pduP[off+12], pduP[off+13], pduP[off+14], pduP[off+15]);
      }
    }
  }
  // ---------------- end Jin DEBUG ----------------



  NR_SLSCH_MAC_SUBHEADER_FIXED *sl_sch_subheader = (NR_SLSCH_MAC_SUBHEADER_FIXED *) pduP;
  uint8_t psfch_period = 0;
  if (mac->sl_tx_res_pool->sl_PSFCH_Config_r16 &&
      mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16)
    psfch_period = *mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16;
  if (psfch_period && mac->sci_pdu_rx.harq_feedback) {
    configure_psfch_params_tx(module_idP, mac, rx_ind, pdu_id);
  }

  NR_SL_UE_info_t *UE = find_UE(mac, sl_sch_subheader->SRC);

  if (UE == NULL)
    return;

  if (pdu_type == SL_NR_RX_PDU_TYPE_SLSCH_PSFCH) {
    handle_nr_ue_sl_harq(module_idP, frame, slot, rx_slsch_pdu, sl_sch_subheader->SRC);
    int r0 = UE->mac_sl_stats.cumul_round[0];
    int r1 = UE->mac_sl_stats.cumul_round[1];
    int r2 = UE->mac_sl_stats.cumul_round[2];
    int r3 = UE->mac_sl_stats.cumul_round[3];
    int r4 = UE->mac_sl_stats.cumul_round[4];
    int round_sum = r1 + 2 * r2 + 3 * r3 + 4 * r4;
    int total_tx = r0 + round_sum;
    if (total_tx % 20 == 0 || (total_tx > 299 && total_tx < 305)) {
      LOG_I(NR_PHY, "[UE] %d:%d PSFCH Stats: RX round (%u %u %u %u %u), SumRetx %u TotalTx %u\n",
                                                      frame, slot,
                                                      UE->mac_sl_stats.cumul_round[0],
                                                      UE->mac_sl_stats.cumul_round[1],
                                                      UE->mac_sl_stats.cumul_round[2],
                                                      UE->mac_sl_stats.cumul_round[3],
                                                      UE->mac_sl_stats.cumul_round[4],
                                                      round_sum, total_tx
                                                      );
    }

  }

  LOG_D(NR_MAC, "%4d.%2d ack_nack %d pdu_type %d mac->sci_pdu_rx.csi_req %d\n",
        frame, slot, rx_slsch_pdu->ack_nack, pdu_type, mac->sci_pdu_rx.csi_req);
  if (rx_slsch_pdu->ack_nack == 0)
    return;

  NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  if (mac->sci_pdu_rx.csi_req) {
    LOG_D(NR_MAC, "%4d.%2d Configuring sl_csi_report parameters\n", frame, slot);
    int scs = get_softmodem_params()->numerology;
    uint16_t tx_slot = (rx_ind->slot + DURATION_RX_TO_TX) % nr_slots_per_frame[scs];
    uint16_t tx_frame = (rx_ind->sfn + (rx_ind->slot + DURATION_RX_TO_TX) / nr_slots_per_frame[scs]) % 1024;
    set_csi_report_params(mac, sched_ctrl);
    nr_ue_sl_csi_report_scheduling(module_idP,
                                   sched_ctrl,
                                   tx_frame,
                                   tx_slot);
  }

  LOG_I(NR_MAC, "In %s : processing PDU %d (with length %d) of %d total number of PDUs...\n", __FUNCTION__, pdu_id, pdu_len, rx_ind->number_pdus);
  LOG_I(NR_PHY, "%4d.%2d Rx V %d R %d SRC %d DST %d\n", frame, slot, sl_sch_subheader->V, sl_sch_subheader->R, sl_sch_subheader->SRC, sl_sch_subheader->DST);
  pduP += sizeof(*sl_sch_subheader);
  pdu_len -= sizeof(*sl_sch_subheader);
/*
LOG_I(NR_MAC,
      "[Jin debug !!!!!!!!!!!!!SL-RX-DBG] after fixedhdr: me=%u SRC=%u DST=%u pdu_len=%d next16: "
      "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
      mac->src_id,
      sl_sch_subheader->SRC,
      sl_sch_subheader->DST,
      pdu_len,
      pduP[0], pduP[1], pduP[2], pduP[3], pduP[4], pduP[5], pduP[6], pduP[7],
      pduP[8], pduP[9], pduP[10], pduP[11], pduP[12], pduP[13], pduP[14], pduP[15]);
  */


  if (frame % 20 == 0)
    LOG_D(NR_PHY, "%4d.%2d Rx V %d R %d SRC %d DST %d\n", frame, slot, sl_sch_subheader->V, sl_sch_subheader->R, sl_sch_subheader->SRC, sl_sch_subheader->DST);
  while (!done && pdu_len > 0) {
    uint16_t mac_len = 0x0000;
    uint16_t mac_subheader_len = 0x0001; //  default to fixed-length subheader = 1-oct
    uint8_t rx_lcid = ((NR_MAC_SUBHEADER_FIXED *)(pduP))->LCID;
    LOG_D(NR_MAC, "[UE %x] LCID %d, remaining pdu length %d byte(s)\n", mac->src_id, rx_lcid, pdu_len);
    switch (rx_lcid) {
      //  MAC CE
      case SL_SCH_LCID_4_19:
        if (!get_mac_len(pduP, pdu_len, &mac_len, &mac_subheader_len))
          return;
        LOG_D(NR_MAC, "%4d.%2d : SLSCH -> LCID %d %d bytes with subheader %d\n", frame, slot, rx_lcid, mac_len, mac_subheader_len);
    // Jin Debug: ICMP sequence tracker
    // -------------------------------
    /*
    uint8_t *sdu = (uint8_t *)(pduP + mac_subheader_len);
    uint32_t sdu_len = mac_len;

    if (sdu_len >= 34) {  // Enough bytes for IPv4 + ICMP
        uint8_t *iph = sdu;

        // IPv4 check: version = 4
        if ((iph[0] >> 4) == 4) {
            uint8_t ihl = (iph[0] & 0x0F) * 4;
            uint8_t protocol = iph[9];

            if (protocol == 1 && sdu_len >= ihl + 8) {   // ICMP
                uint8_t *icmph = iph + ihl;

                uint8_t type = icmph[0];
                uint8_t code = icmph[1];
                uint16_t seq = (icmph[6] << 8) | icmph[7];

                LOG_I(NR_MAC,
                      "Jin ^^^^^^^^  [ICMP-RX] frame=%d slot=%d seq=%u type=%u code=%u\n",
                      frame, slot, seq, type, code);
            }
        }
    }*/
    // -------------------------------

 


        mac_rlc_data_ind(module_idP,
                         mac->src_id, 
                         0,
                         frame,
                         ENB_FLAG_NO,
                         MBMS_FLAG_NO,
                         rx_lcid,  
                         (char *)(pduP + mac_subheader_len),
                         mac_len,
                         1,
                         NULL);
        //Jin log
        LOG_I(NR_MAC,
          "[Jin-Log check rx at pdu SL-RX-SLSCH] srcID_ME=%u sci_src=%u sci_dst=%u lcid=%u mac_len=%u frame=%u slot=%u\n",
          mac->src_id,
          mac->sci_pdu_rx.source_id,
          mac->sci_pdu_rx.dest_id,
          rx_lcid, 
          mac_len,
          frame,
          slot);
	      break;

      case SL_SCH_LCID_SL_CSI_REPORT:
        {
          NR_MAC_SUBHEADER_FIXED* sub_pdu_header = (NR_MAC_SUBHEADER_FIXED*) pduP;
          if (frame % 20 == 0)
            LOG_D(NR_MAC, "\tLCID: %i, R: %i\n", sub_pdu_header->LCID, sub_pdu_header->R);
          mac_subheader_len = sizeof(*sub_pdu_header);
          nr_sl_csi_report_t* nr_sl_csi_report = (nr_sl_csi_report_t *) (pduP + mac_len);
          mac_len = sizeof(*nr_sl_csi_report);
          if (frame % 20 == 0)
            LOG_D(NR_MAC, "\tCQI: %i RI: %i\n", nr_sl_csi_report->CQI, nr_sl_csi_report->RI);
          sched_ctrl->rx_csi_report.CQI = nr_sl_csi_report->CQI;
          sched_ctrl->rx_csi_report.RI = nr_sl_csi_report->RI;
          LOG_D(NR_MAC, "Setting to CQI %i\n", sched_ctrl->rx_csi_report.CQI);
          break;
        }
      case SL_SCH_LCID_SL_PADDING:
        {
          NR_MAC_SUBHEADER_FIXED* sub_pdu_header = (NR_MAC_SUBHEADER_FIXED*) pduP;
          mac_subheader_len = sizeof(*sub_pdu_header);
          mac_len = pdu_len - mac_subheader_len;
          LOG_D(NR_MAC, "%4d.%2d Received padding %d\n", frame, slot, pdu_len);
          done = 1;
          break;
        }
      case SL_SCH_LCID_SCCH_PC5_NOT_PROT:
      case SL_SCH_LCID_SCCH_PC5_DSMC:
      case SL_SCH_LCID_SCCH_PC5_PROT:
      case SL_SCH_LCID_SCCH_PC5_RRC:
      case SL_SCH_LCID_20_55:
      case SL_SCH_LCID_SCCH_RRC_SL_RLC0:
      case SL_SCH_LCID_SCCH_RRC_SL_RLC1:
      case SL_SCH_LCID_SCCH_SL_DISCOVERY:
      case SL_SCH_LCID_SL_INTER_UE_COORD_REQ:
      case SL_SCH_LCID_SL_INTER_UE_COORD_INFO:
      case SL_SCH_LCID_SL_DRX_CMD:
	      LOG_W(NR_MAC,"Received unsupported SL LCID %d\n",rx_lcid);
	      return;
	      break;
    }
    pduP += ( mac_subheader_len + mac_len );
    pdu_len -= ( mac_subheader_len + mac_len );
    LOG_D(NR_MAC, "mac_subhead_len + mac_len = %d\n", mac_subheader_len + mac_len);
    LOG_D(NR_MAC, "%4d.%2d : SLSCH -> LCID %d remaining pdu length %d byte(s)\n", frame, slot, rx_lcid, pdu_len);
    if (pdu_len < 0)
      LOG_E(NR_MAC, "[UE %d][%d.%d] nr_ue_process_mac_pdu_sl, residual mac pdu length %d < 0!\n", module_idP, frame, slot, pdu_len);
  }
}

void nr_ue_sl_csi_report_scheduling(int mod_id,
                                    NR_SL_UE_sched_ctrl_t *sched_ctrl,
                                    uint32_t frame,
                                    uint32_t slot) {
  uint32_t sched_slot, sched_frame;
  NR_UE_MAC_INST_t *mac = get_mac_inst(mod_id);
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;

  NR_TDD_UL_DL_Pattern_t *tdd = &sl_mac->sl_TDD_config->pattern1;
  if (sched_ctrl->sched_csi_report.active == false) {
    uint8_t scs = sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;
    const int nr_slots_frame = nr_slots_per_frame[scs];
    uint8_t csirs_to_csi_report[sl_mac->sl_LatencyBoundCSI_Report];

    get_csirs_to_csi_report(csirs_to_csi_report, sl_mac->sl_LatencyBoundCSI_Report, nr_slots_frame);
    int continue_flag = 0;
    for (int f = 0; f < sl_mac->sl_LatencyBoundCSI_Report; f++) {
      continue_flag = get_csi_reporting_frame_slot(mac,
                                                   tdd,
                                                   csirs_to_csi_report[f],
                                                   nr_slots_frame,
                                                   frame,
                                                   slot,
                                                   &sched_frame,
                                                   &sched_slot);
      if (continue_flag == -1)
        continue;
      else
        break;
    }
    LOG_D(NR_MAC, "%4d.%2d Scheduling csi_report\n", sched_frame, sched_slot);
    SL_CSI_Report_t *csi_report = &sched_ctrl->sched_csi_report;
    csi_report->frame = sched_frame;
    csi_report->slot = sched_slot;
    csi_report->active = true;
  }
}

NR_SL_UE_info_t* find_UE(NR_UE_MAC_INST_t *mac,
                         uint16_t nearby_ue_id) {
  NR_SL_UEs_t *UE_info = &mac->sl_info;

  if (*(UE_info->list) == NULL) {
    LOG_D(NR_MAC, "UE list is empty\n");
    return NULL;
  }

  SL_UE_iterator(UE_info->list, UE) {
    LOG_D(NR_MAC, "%s: dest_id %d nearby id %d\n", __FUNCTION__, UE->uid, nearby_ue_id);
    if((UE->uid == nearby_ue_id)) {
      return UE;
    }
  }
  return NULL;
}

int get_csi_reporting_frame_slot(NR_UE_MAC_INST_t *mac,
                                 NR_TDD_UL_DL_Pattern_t *tdd,
                                 uint8_t csi_offset,
                                 const int nr_slots_frame,
                                 uint32_t frame,
                                 uint32_t slot,
                                 uint32_t *csi_report_frame,
                                 uint32_t *csi_report_slot) {
  AssertFatal(tdd != NULL, "Expecting valid tdd configurations");
  const int first_ul_slot_period = tdd ? get_first_ul_slot(tdd->nrofDownlinkSlots, tdd->nrofDownlinkSymbols, tdd->nrofUplinkSymbols) : 0;
  const int nr_slots_period = tdd ? nr_slots_frame / get_nb_periods_per_frame(tdd->dl_UL_TransmissionPeriodicity) : nr_slots_frame;

  *csi_report_slot = (slot + csi_offset) % nr_slots_frame;

  // check if the slot is UL
  if(*csi_report_slot % nr_slots_period < first_ul_slot_period)
    return -1;

  *csi_report_frame = (frame + ((slot + csi_offset) / nr_slots_frame)) & 1023;

  return 0;
}

void init_list(List_t* list, size_t element_size, size_t initial_capacity) {
  list->data = calloc(1, element_size * initial_capacity);
  list->element_size = element_size;
  list->size = 0;
  list->capacity = initial_capacity;
}

void push_back(List_t* list, void *element) {
  if (list->size == list->capacity) {
    list->capacity *= 2;
    list->data = realloc(list->data, list->element_size * list->capacity);
  }
  void *target = (char*)list->data + (list->size * list->element_size);
  memcpy(target, element, list->element_size);
  list->size++;
}

void* get_front(const List_t* list) {
  if (list->size == 0) {
    return NULL;
  }
  return list->data; // pointer to first element
}

void* get_back(const List_t* list) {
  if (list->size == 0) {
    return NULL;
  }
  return (char*)list->data + (list->size - 1) * list->element_size; // pointer to last element
}

void delete_at(List_t* list, size_t index) {

  if (index >= list->size) {
    LOG_E(NR_MAC, "Index of bound\n");
    return;
  }

  char* element_ptr = (char*)list->data + index * list->element_size;

  memmove(element_ptr, element_ptr + list->element_size, (list->size - index - 1) * list->element_size);

  list->size--;
}

int64_t normalize(frameslot_t *frame_slot, uint8_t mu) {
  int64_t num_slots = 0;
  uint8_t slots_per_frame = nr_slots_per_frame[mu];
  num_slots  = frame_slot->slot;
  num_slots += frame_slot->frame * slots_per_frame;
  return num_slots;
}

void de_normalize(int64_t abs_slot_idx, uint8_t mu, frameslot_t *frame_slot) {
  uint8_t slots_per_frame = nr_slots_per_frame[mu];
  frame_slot->frame = (abs_slot_idx / slots_per_frame) & 1023;
  frame_slot->slot = (abs_slot_idx % slots_per_frame);
}

frameslot_t add_to_sfn(frameslot_t* sfn, uint16_t slot_n, uint8_t mu) {
 frameslot_t temp_sfn;
 temp_sfn.frame = (sfn->frame + ((sfn->slot + slot_n) / nr_slots_per_frame[mu])) % 1024;
 temp_sfn.slot = (sfn->slot + slot_n) % nr_slots_per_frame[mu];
 return temp_sfn;
}


void update_sensing_data(List_t* sensing_data, frameslot_t *frame_slot, sl_nr_ue_mac_params_t *sl_mac, uint16_t pool_id) {
  uint8_t mu = sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;
  int64_t num_max_slots = nr_slots_per_frame[mu] * 1024;
  while(sensing_data->size > 0) {
    sensing_data_t* last_elem = (sensing_data_t*)((char*)sensing_data->data + (sensing_data->size - 1) * sensing_data->element_size);
    int64_t diff = (normalize(frame_slot, mu) - normalize(&last_elem->frame_slot, mu) + num_max_slots) % num_max_slots;
    if (diff <= get_tproc0(sl_mac, pool_id)) {
      pop_back(sensing_data);
    } else {
      break;
    }
  }
}

void update_transmit_history(List_t* transmit_history, frameslot_t *frame_slot, sl_nr_ue_mac_params_t *sl_mac, uint16_t pool_id) {
  uint8_t mu = sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;
  int64_t num_max_slots = nr_slots_per_frame[mu] * 1024;
  while(transmit_history->size > 0) {
    frameslot_t* last_frame_slot = (frameslot_t*)((uint8_t*)transmit_history->data + (transmit_history->size - 1) * transmit_history->element_size);
    int64_t diff = (normalize(frame_slot, mu) - normalize(last_frame_slot, mu) + num_max_slots) % num_max_slots;

    if (diff <= get_tproc0(sl_mac, pool_id)) {
      pop_back(transmit_history);
    } else {
      break;
    }
  }
}

void pop_back(List_t* sensing_data) {
  if(sensing_data->size > 0) {
    sensing_data->size--;
  }
}

void free_list_mem(List_t* list) {
  free(list->data);
  list->data = NULL;
  list->size = 0;
  list->capacity = 0;
}

uint16_t get_T2_min(uint16_t pool_id, sl_nr_ue_mac_params_t *sl_mac, uint8_t mu) {
  uint16_t t2min = sl_mac->sl_TxPool[pool_id]->t2min * pow(2, mu);
  return t2min;
}

uint16_t get_t2(uint16_t pool_id, uint8_t mu, nr_sl_transmission_params_t* sl_tx_params, sl_nr_ue_mac_params_t *sl_mac) {
  uint16_t t2;
  if (!(sl_tx_params->packet_delay_budget_ms == 0)) {
    // Packet delay budget is known, so use it
    uint16_t pdb_slots = time_to_slots(mu, sl_tx_params->packet_delay_budget_ms);
    t2 = min(pdb_slots, sl_mac->sl_TxPool[pool_id]->t2);
  } else {
    // Packet delay budget is not known, so use max(NrSlUeMac::T2, T2min)
    uint16_t t2min = get_T2_min(pool_id, sl_mac, mu);
    t2 = max(t2min, sl_mac->sl_TxPool[pool_id]->t2);
  }
  return t2;
}

uint16_t time_to_slots(uint8_t mu, uint16_t time) {
  uint8_t slots_per_ms = (uint8_t)pow(2, mu); // subframe is of 1 ms
  uint16_t time_in_slots = time * slots_per_ms;
  return time_in_slots;
}

uint8_t get_tproc0(sl_nr_ue_mac_params_t *sl_mac, uint16_t pool_id) {
  return sl_mac->sl_TxPool[pool_id]->tproc0;
}

void init_vector(vec_of_list_t* vec, size_t initial_capacity) {
  vec->size = 0;
  vec->capacity = initial_capacity;
  vec->lists = (List_t*)malloc16_clear(initial_capacity * sizeof(List_t));

  if (!vec->lists) {
    LOG_E(NR_MAC, "Memory allocation failed\n");
    exit(EXIT_FAILURE);
  }
}

void add_list(vec_of_list_t* vec, size_t element_size, size_t initial_list_capacity) {
  if (vec->size == vec->capacity) {
    vec->capacity *= 2;
    vec->lists = realloc(vec->lists, vec->capacity * sizeof(List_t));
    if (!vec->lists) {
      LOG_E(NR_MAC, "Memory allocation failed\n");
      exit(EXIT_FAILURE);
    }
  }
  init_list(&vec->lists[vec->size], element_size, initial_list_capacity);
  vec->size++;
}

void push_back_list(vec_of_list_t* vec, List_t* new_list) {
  if (vec->size == vec->capacity) {
    vec->capacity *= 2;
    vec->lists = realloc(vec->lists, vec->capacity * sizeof(List_t));
    if (!vec->lists) {
      LOG_E(NR_MAC, "Memory allocation failed\n");
      exit(EXIT_FAILURE);
    }
  }
  vec->lists[vec->size] = *new_list;
  vec->size++;
}

List_t* get_list(vec_of_list_t *vec, size_t index) {
  if (index >= vec->size) {
    LOG_E(NR_MAC, "Index out of bounds\n");
    return NULL;
  }
  return &vec->lists[index];
}

void free_vector(vec_of_list_t* vec) {
  for (size_t i = 0; i < vec->size; i++) {
    free_list_mem(&vec->lists[i]);
  }
  free(vec->lists);
  vec->lists = NULL;
  vec->size = 0;
  vec->capacity = 0;
}