/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "mac_defs.h"
#include "mac_proto.h"
#include "common/utils/bits.h"
#include "openair2/LAYER2/NR_MAC_COMMON/nr_mac_common.h"
#include "executables/softmodem-common.h"
#include "executables/nr-uesoftmodem.h"
#include "LAYER2/nr_rlc/nr_rlc_oai_api.h"

#define SL_DEBUG

static const int sequence_cyclic_shift_harq_ack_or_ack_or_only_nack[2]
/* Sequence cyclic shift */ = {  0, 6 };

typedef enum {
  SL_PSFCH_NO_FEEDBACK,
  SL_PSFCH_ACK_NACK,
  SL_PSFCH_NACK_ONLY,
} sl_psfch_feedback_mode_t;

/* SCI-1A encodes SCI-2A/2B/2C as 0/1/2.  TS 38.213 16.3 uses the
 * ACK/NACK cyclic-shift table for SCI-2A cast types 01 and 10 and for
 * SCI-2C.  SCI-2B and SCI-2A cast type 11 use the NACK-only table. */
static sl_psfch_feedback_mode_t get_psfch_feedback_mode(uint8_t second_stage_sci_format,
                                                        uint8_t cast_type,
                                                        bool harq_feedback_enabled)
{
  if (!harq_feedback_enabled)
    return SL_PSFCH_NO_FEEDBACK;

  switch (second_stage_sci_format) {
    case 0: /* SCI-2A */
      if (cast_type == 1 || cast_type == 2)
        return SL_PSFCH_ACK_NACK;
      if (cast_type == 3)
        return SL_PSFCH_NACK_ONLY;
      return SL_PSFCH_NO_FEEDBACK;
    case 1: /* SCI-2B */
      return SL_PSFCH_NACK_ONLY;
    case 2: /* SCI-2C */
      return SL_PSFCH_ACK_NACK;
    default:
      return SL_PSFCH_NO_FEEDBACK;
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
                        uint8_t start_symbol, uint8_t L)
{

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

uint16_t sl_get_num_subch(NR_SL_ResourcePool_r16_t *rpool)
{

  //sl-NumSubchannel - Indicates the number of subchannels in the corresponding resource pool
  //which consists of contiguous PRBs only.
  uint16_t num_subch = (rpool->sl_NumSubchannel_r16) ? *rpool->sl_NumSubchannel_r16 : 0;

  AssertFatal(num_subch,"NUM Subchannels cannot be 0. Resource Pool Configuration Error\n");

  return num_subch;
}

uint16_t sl_get_subchannel_size(NR_SL_ResourcePool_r16_t *rpool)
{

  uint16_t num_subch = sl_get_num_subch(rpool);

  //sl-RB-Number - Indicates the number of PRBs in the corresponding resource pool.
  //which consists of contiguous PRBs only.The remaining RB cannot be used
  uint16_t num_rbs = (rpool->sl_RB_Number_r16) ? *rpool->sl_RB_Number_r16 : 0;

  AssertFatal(num_rbs,"NumRbs in rpool cannot be 0.Resource Pool Configuration Error\n");

  uint16_t subch_size = 0;

  subch_size = num_rbs/num_subch;

  LOG_D(NR_MAC, "Subch_size:%d, numRBS:%d, num_subch:%d\n",
                                          subch_size,num_rbs,num_subch);

  return (subch_size);
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
    num_bits = ceil_log2_u32(n_subch * (n_subch + 1) * (2*n_subch + 1) / 6);
    sci_1a_len += num_bits;
    sci_1a->frequency_resource_assignment.nbits = num_bits;
    sci_1a_len += 9;
    sci_1a->time_resource_assignment.nbits = 9;
  } else {
    num_bits = ceil_log2_u32((n_subch * (n_subch + 1)) >> 1);
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
    num_bits = ceil_log2_u32(n_rsvperiod);
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
    num_bits = ceil_log2_u32(n_dmrspatterns);
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

  LOG_D(NR_MAC,
        "sci 1A - additional_table:%ld, sci 1a len:%d, additional table nbits:%d\n",
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

  AssertFatal((num_reservedbits >= 2) && (num_reservedbits <= 4),
              "Num Reserved bits can only be 2 or 3 or 4. Resource Pool Configuration Error.\n");
  sci_1a_len += num_reservedbits;
  sci_1a->reserved_bits.nbits = num_reservedbits;
  LOG_D(NR_MAC,
        "sci 1A - reserved_bits:%d, sci 1a len:%d, sci_1a->reserved_bits.nbits:%d\n",
        num_reservedbits,
        sci_1a_len,
        sci_1a->reserved_bits.nbits);

  LOG_D(NR_MAC,"sci 1A Length in bits: %d \n",sci_1a_len);

  return sci_1a_len;
}

uint16_t count_on_bits(const uint8_t *buf, size_t size)
{
  uint16_t count = 0;
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

static void compute_params(int module_idP,
                           const NR_SL_ResourcePool_r16_t *resource_pool,
                           psfch_params_t *psfch_params,
                           uint16_t pssch_start_subchannel,
                           uint16_t pssch_num_subchannels,
                           uint16_t source_id,
                           uint16_t destination_id,
                           uint8_t cast_type,
                           uint16_t feedback_member_id)
{
  (void)module_idP;
  if (!resource_pool || !resource_pool->sl_PSFCH_Config_r16
      || resource_pool->sl_PSFCH_Config_r16->present != NR_SetupRelease_SL_PSFCH_Config_r16_PR_setup)
      return;

  NR_SL_PSFCH_Config_r16_t *sl_psfch_config = resource_pool->sl_PSFCH_Config_r16->choice.setup;
  const int sl_num_muxcs_pair[4] = {1, 2, 3, 6};
  uint8_t *rb_buf = sl_psfch_config->sl_PSFCH_RB_Set_r16->buf;
  size_t size = sl_psfch_config->sl_PSFCH_RB_Set_r16->size / sizeof(rb_buf[0]);
  uint16_t m_psfch_prb_set = count_on_bits(rb_buf, size);
  long sl_numsubchannel = *resource_pool->sl_NumSubchannel_r16;
  const uint8_t psfch_periods[] = {0,1,2,4};
  long n_psfch_pssch = (sl_psfch_config->sl_PSFCH_Period_r16)
                        ? psfch_periods[*sl_psfch_config->sl_PSFCH_Period_r16] : 0;
  long n_psfch_cs = *sl_psfch_config->sl_NumMuxCS_Pair_r16;

  AssertFatal(n_psfch_cs >= 0 && n_psfch_cs < 4, "Invalid sl-NumMuxCS-Pair index %ld\n", n_psfch_cs);
  AssertFatal(n_psfch_pssch > 0 && sl_numsubchannel > 0,
              "Invalid PSFCH dimensions: period %ld, subchannels %ld\n",
              n_psfch_pssch,
              sl_numsubchannel);
  AssertFatal(m_psfch_prb_set % (sl_numsubchannel * n_psfch_pssch) == 0,
              "PSFCH RB set size %u is not divisible by subchannels*period %ld\n",
              m_psfch_prb_set,
              sl_numsubchannel * n_psfch_pssch);
  const uint16_t m_psfch_subch_slot = m_psfch_prb_set / (sl_numsubchannel * n_psfch_pssch);
  AssertFatal(m_psfch_subch_slot > 0,
              "PSFCH RB set size %u cannot cover %ld subchannel/slot groups\n",
              m_psfch_prb_set,
              sl_numsubchannel * n_psfch_pssch);
  const uint16_t num_cs_pairs = sl_num_muxcs_pair[n_psfch_cs];
  AssertFatal(pssch_num_subchannels > 0 && pssch_start_subchannel + pssch_num_subchannels <= sl_numsubchannel,
              "Invalid PSSCH subchannels: start %u length %u pool %ld\n",
              pssch_start_subchannel,
              pssch_num_subchannels,
              sl_numsubchannel);

  /* TS 38.213 16.3 candidateResourceType=startSubCH uses only the
   * lowest allocated subchannel.  allocSubCH uses the concatenation of the
   * candidate sets for every allocated subchannel. */
  const bool alloc_subch = sl_psfch_config->sl_PSFCH_CandidateResourceType_r16
                           && *sl_psfch_config->sl_PSFCH_CandidateResourceType_r16
                                  != NR_SL_PSFCH_Config_r16__sl_PSFCH_CandidateResourceType_r16_startSubCH;
  const uint16_t candidate_subchannels = alloc_subch ? pssch_num_subchannels : 1;
  const uint16_t r_psfch_prb_cs = candidate_subchannels * m_psfch_subch_slot * num_cs_pairs;
  AssertFatal(r_psfch_prb_cs > 0, "Invalid number of PSFCH candidate resources\n");
  /* TS 38.213 16.3: P_ID is the SCI source ID.  M_ID is the identity of
   * the PSSCH receiver only for SCI-2A cast type 01; it is zero otherwise.
   * Both ends obtain the receiver identity from persistent state rather than
   * the position of a decoded result. */
  const uint16_t m_id = cast_type == 1 ? feedback_member_id : 0;
  const uint16_t psfch_rsc_idx = ((uint32_t)source_id + m_id) % r_psfch_prb_cs;
  const uint16_t cyclic_shift_pair = psfch_rsc_idx % num_cs_pairs;
  const uint16_t candidate_prb = psfch_rsc_idx / num_cs_pairs;
  const uint16_t relative_subchannel = alloc_subch ? candidate_prb / m_psfch_subch_slot : 0;
  const uint16_t selected_subchannel = pssch_start_subchannel + relative_subchannel;
  const uint16_t prb_offset = candidate_prb % m_psfch_subch_slot;
  LOG_D(NR_MAC,
        "PSFCH SRC %u DST %u cast %u member %u PSSCH subchannels %u+%u\n",
        source_id,
        destination_id,
        cast_type,
        feedback_member_id,
        pssch_start_subchannel,
        pssch_num_subchannels);
  LOG_D(NR_MAC, "size %lu, m_psfch_prb_set %d, sl_numsubchannel %ld, n_psfch_pssch %ld, n_psfch_cs %d\n", size, m_psfch_prb_set, sl_numsubchannel, n_psfch_pssch, sl_num_muxcs_pair[n_psfch_cs]);
  LOG_D(NR_MAC,
        "m_psfch_subch_slot %u, candidate_subchannels %u, r_psfch_prb_cs %u, psfch_rsc_idx %u\n",
        m_psfch_subch_slot,
        candidate_subchannels,
        r_psfch_prb_cs,
        psfch_rsc_idx);
  LOG_D(NR_MAC, "PSFCH resource %u: PRB offset %u, cyclic-shift pair %u\n", psfch_rsc_idx, prb_offset, cyclic_shift_pair);
  psfch_params->m0 = table_16_3_1[n_psfch_cs][cyclic_shift_pair];

  /* Keep the actual configured RB indices.  The RB set is an ASN.1 BIT
   * STRING (MSB first) and is not required to be contiguous. */
  psfch_params->rb_set = calloc(m_psfch_prb_set, sizeof(*psfch_params->rb_set));
  const size_t num_rb_bits = (size << 3) - sl_psfch_config->sl_PSFCH_RB_Set_r16->bits_unused;
  uint16_t rb_ordinal = 0;
  for (size_t rb = 0; rb < num_rb_bits; rb++) {
    if (rb_buf[rb >> 3] & (1U << (7 - (rb & 7))))
      psfch_params->rb_set[rb_ordinal++] = rb;
  }
  DevAssert(rb_ordinal == m_psfch_prb_set);
  psfch_params->num_psfch_slots = n_psfch_pssch;
  psfch_params->selected_subchannel = selected_subchannel;
  psfch_params->num_rbs = m_psfch_prb_set;
  psfch_params->prbs_per_subchannel_slot = m_psfch_subch_slot;
  psfch_params->prb_ordinal_base = selected_subchannel * n_psfch_pssch * m_psfch_subch_slot + prb_offset;
    }

static void free_psfch_params(psfch_params_t *psfch_params, long psfch_period)
{
  (void)psfch_period;
  free(psfch_params->rb_set);
  psfch_params->rb_set = NULL;
}

void configure_psfch_params_tx(int module_idP,
                               NR_UE_MAC_INST_t *mac,
                               sl_nr_rx_indication_t *rx_ind,
                               int pdu_id)
{
  // TODO: May need to update in case of multiple UEs
  const uint8_t psfch_periods[] = {0,1,2,4};
  /* Feedback transmitted for a received PSSCH uses the resource pool of that
   * PSSCH: the local RX pool (TS 38.213 16.3). */
  NR_SL_PSFCH_Config_r16_t *sl_psfch_config = mac->sl_rx_res_pool->sl_PSFCH_Config_r16->choice.setup;
  long psfch_period = (sl_psfch_config->sl_PSFCH_Period_r16)
                        ? psfch_periods[*sl_psfch_config->sl_PSFCH_Period_r16] : 0;

  int scs = get_softmodem_params()->numerology;
  /* The minimum PSFCH gap is measured from the actual PSSCH reception slot;
   * the RX-to-TX processing duration must not be added a second time. */
  uint16_t tx_slot = rx_ind->slot;
  uint16_t tx_frame = rx_ind->sfn;

  sl_nr_slsch_pdu_t *rx_slsch = &(rx_ind->rx_indication_body + pdu_id)->rx_slsch_pdu;
  uint8_t ack_nack = rx_slsch->ack_nack;
  const sl_psfch_feedback_mode_t feedback_mode =
      get_psfch_feedback_mode(rx_slsch->second_stage_sci_format, rx_slsch->cast_type, rx_slsch->harq_feedback);
  /* NACK-only feedback is transmitted only when PSSCH decoding failed. */
  if (feedback_mode == SL_PSFCH_NO_FEEDBACK || (feedback_mode == SL_PSFCH_NACK_ONLY && ack_nack != 0))
    return;
  LOG_D(NR_MAC, "tx_frame %4u.%2u, ack_nack %d rx: %4u.%2u\n", tx_frame, tx_slot, ack_nack, rx_ind->sfn, rx_ind->slot);
  psfch_params_t psfch_params = {0};
  compute_params(module_idP,
                 mac->sl_rx_res_pool,
                 &psfch_params,
                 rx_slsch->pssch_start_subchannel,
                 rx_slsch->pssch_num_subchannels,
                 rx_slsch->source_id,
                 rx_slsch->dest_id,
                 rx_slsch->cast_type,
                 mac->src_id);
  const int nr_slots_frame = nr_slots_per_frame[scs];
  int psfch_index = nr_ue_sl_acknack_scheduling(mac, rx_ind, psfch_period, tx_frame, tx_slot, nr_slots_frame);
  if (psfch_index != -1)
    fill_psfch_params_tx(mac,
                         rx_ind,
                         pdu_id,
                         psfch_period,
                         tx_frame,
                         tx_slot,
                         ack_nack,
                         &psfch_params,
                         nr_slots_frame,
                         psfch_index);
  free_psfch_params(&psfch_params, psfch_period);
}

int get_psfch_index(const frame_structure_t *fs, int frame, int slot, int n_slots_frame, const NR_TDD_UL_DL_Pattern_t *tdd, int sched_psfch_max_size)
{
  // PUCCH structures are indexed by slot in the PUCCH period determined by sched_psfch_max_size number of UL slots
  // this functions return the index to the structure for slot passed to the function

  const int first_ul_slot_period = tdd ? get_first_ul_slot(fs, false) : 0;
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

int get_feedback_frame_slot(NR_UE_MAC_INST_t *mac, NR_TDD_UL_DL_Pattern_t *tdd,
                            uint8_t feedback_offset, uint8_t psfch_min_time_gap,
                            const int nr_slots_frame, uint16_t frame, uint16_t slot,
                            long psfch_period, int *psfch_frame, int *psfch_slot) {

  AssertFatal(tdd != NULL, "Expecting valid tdd configurations");
  const int first_ul_slot_period = tdd ? get_first_ul_slot(&mac->frame_structure, false) : 0;
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

/* get_feedback_slot() was removed.  The active path uses get_feedback_abs_slot()
 * together with the resource-pool bitmap, which correctly derives the PSFCH
 * occasion for any configuration (TS 38.213 s16.3).  The old function contained
 * a hardcoded slot table valid only for one specific TDD layout and would
 * AssertFatal() on any other slot number. */

int nr_ue_sl_acknack_scheduling(NR_UE_MAC_INST_t *mac, sl_nr_rx_indication_t *rx_ind,
                                long psfch_period, uint16_t frame, uint16_t slot, const int nr_slots_frame) {
  // TODO: needs to be updated for multi-subchannels
  int psfch_frame, psfch_slot;
  sl_nr_ue_mac_params_t *sl_mac =  mac->SL_MAC_PARAMS;
  /* Derive feedback slot by walking the physical SL bitmap -- independent of TDD layout */
  uint8_t pool_id = 0;
  SL_ResourcePool_params_t *sl_rx_rsrc_pool = sl_mac->sl_RxPool[pool_id];
  size_t phy_map_sz = (sl_rx_rsrc_pool->phy_sl_bitmap.size << 3) - sl_rx_rsrc_pool->phy_sl_bitmap.bits_unused;
  NR_SL_PSFCH_Config_r16_t *local_psfch_cfg = mac->sl_rx_res_pool->sl_PSFCH_Config_r16->choice.setup;
  const uint8_t psfch_time_gaps[] = {2, 3};
  uint8_t min_tg = local_psfch_cfg->sl_MinTimeGapPSFCH_r16
                       ? psfch_time_gaps[*local_psfch_cfg->sl_MinTimeGapPSFCH_r16] : 2;
  uint64_t tx_abs = (uint64_t)frame * nr_slots_frame + slot;
  int64_t fb_abs = get_feedback_abs_slot(&sl_rx_rsrc_pool->phy_sl_bitmap, phy_map_sz,
                                         tx_abs, min_tg, (uint8_t)psfch_period);
  if (fb_abs < 0) {
    LOG_W(NR_MAC, "No PSFCH slot found for tx %4d.%2d psfch_period %ld\n", frame, slot, psfch_period);
    return -1;
  }
  psfch_frame = (int)((fb_abs / nr_slots_frame) % 1024);
  psfch_slot = (int)(fb_abs % nr_slots_frame);

  NR_SL_UE_sched_ctrl_t  *sched_ctrl = &mac->sl_info.list[0]->UE_sched_ctrl;
  /* Do not index sidelink feedback through the cellular UL/TDD layout.  TS
   * 38.213 16.3 defines PSFCH occasions on the resource-pool slot axis and
   * several PSSCH slots can map to the same occasion, so allocate a distinct
   * free entry for every response that has to be multiplexed in that slot. */
  int psfch_index = -1;
  for (int i = 0; i < sched_ctrl->sched_psfch_size; i++) {
    if (sched_ctrl->sched_psfch[i].feedback_slot < 0) {
      psfch_index = i;
      break;
    }
  }
  if (psfch_index < 0) {
    LOG_E(NR_MAC,
          "No free outgoing PSFCH entry for PSSCH %4u.%2u -> PSFCH %4d.%2d\n",
          frame,
          slot,
          psfch_frame,
          psfch_slot);
    return -1;
  }
  SL_sched_feedback_t  *curr_psfch = &sched_ctrl->sched_psfch[psfch_index];
  LOG_D(NR_MAC, "%s tx %4d.%2d -> fb %4d.%2d psfch_period %ld\n",
        __FUNCTION__, frame, slot, psfch_frame, psfch_slot, psfch_period);
  curr_psfch->feedback_frame = psfch_frame;
  curr_psfch->feedback_slot = psfch_slot;
  curr_psfch->dai_c = psfch_index;
  LOG_D(NR_MAC, "Rx SLSCH %4d.%2d, SL_ACK %4d.%2d in current PSFCH: psfch_index %d dai_c %u curr_psfch %p\n",
        rx_ind->sfn,
        rx_ind->slot,
        psfch_frame,
        psfch_slot,
        psfch_index,
        curr_psfch->dai_c,
        curr_psfch);
  LOG_D(NR_MAC, "SL %4d.%2d, Couldn't find scheduling occasion for this HARQ process\n", rx_ind->sfn, rx_ind->slot);
  return psfch_index;
}

void fill_psfch_params_tx(NR_UE_MAC_INST_t *mac,
                          sl_nr_rx_indication_t *rx_ind,
                          int pdu_id,
                          long psfch_period,
                          uint16_t frame,
                          uint16_t slot,
                          uint8_t ack_nack,
                          psfch_params_t *psfch_params,
                          const int nr_slots_frame,
                          int psfch_index)
{
  NR_SL_BWP_Generic_r16_t *sl_bwp = mac->sl_bwp->sl_BWP_Generic_r16;
  const sl_nr_slsch_pdu_t *rx_slsch = &rx_ind->rx_indication_body[pdu_id].rx_slsch_pdu;

  SL_sched_feedback_t  *sched_psfch = &mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch[psfch_index];
  LOG_D(NR_MAC, "psfch_period %ld, feedback frame:slot %d:%d, frame:slot %d:%d, harq feedback %d psfch_index %d\n",
        psfch_period,
        sched_psfch->feedback_frame,
        sched_psfch->feedback_slot,
        rx_ind->sfn,
        rx_ind->slot,
        rx_slsch->harq_feedback,
        psfch_index);
  sched_psfch->initial_cyclic_shift = psfch_params->m0;
  sched_psfch->source_id = rx_slsch->source_id;
  sched_psfch->dest_id = rx_slsch->dest_id;
  sched_psfch->cast_type = rx_slsch->cast_type;
  sched_psfch->second_stage_sci_format = rx_slsch->second_stage_sci_format;
  sched_psfch->pssch_start_subchannel = rx_slsch->pssch_start_subchannel;
  sched_psfch->pssch_num_subchannels = rx_slsch->pssch_num_subchannels;
  const sl_psfch_feedback_mode_t feedback_mode =
      get_psfch_feedback_mode(rx_slsch->second_stage_sci_format, rx_slsch->cast_type, rx_slsch->harq_feedback);
  if (feedback_mode == SL_PSFCH_ACK_NACK) {
    sched_psfch->mcs = sequence_cyclic_shift_harq_ack_or_ack_or_only_nack[ack_nack];
    sched_psfch->bit_len_harq = 1;
    LOG_D(NR_MAC, "mcs %i, ack_nack: %i, sched_psfch->initial_cyclic_shift %i\n",
          sched_psfch->mcs, ack_nack, sched_psfch->initial_cyclic_shift);
  } else {
    DevAssert(feedback_mode == SL_PSFCH_NACK_ONLY && ack_nack == 0);
    sched_psfch->mcs = sequence_cyclic_shift_harq_ack_or_ack_or_only_nack[0];
    sched_psfch->bit_len_harq = 0;
  }
  const uint8_t values[] = {7, 8, 9, 10, 11, 12, 13, 14};
  uint8_t sl_num_symbols = *sl_bwp->sl_LengthSymbols_r16 ? values[*sl_bwp->sl_LengthSymbols_r16] : 0;
  // start_symbol_index has been used as lprime check 38.213 16.3
  sched_psfch->start_symbol_index = *sl_bwp->sl_StartSymbol_r16 + sl_num_symbols - 2;
  LOG_D(NR_PHY, "sl_StartSymbol_r16 %ld, sl_num_symbols: %d, start sym index %d, mcs %d\n",
        *sl_bwp->sl_StartSymbol_r16, sl_num_symbols, sched_psfch->start_symbol_index, sched_psfch->mcs);
  sched_psfch->hopping_id = *mac->sl_rx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_HopID_r16;
  /* PRB row index (TS 38.213 s16.3): PSSCH slot position in the group sharing this
   * PSFCH occasion.  Use the pool-cycle SL-slot ordinal mod psfch_period, not
   * slot-in-frame parity, which collapses non-consecutive SL slots onto one row. */
  {
    uint8_t mu_tx = (uint8_t)get_softmodem_params()->numerology;
    SL_ResourcePool_params_t *sl_rx_pool = mac->SL_MAC_PARAMS->sl_RxPool[0];
    size_t rx_map_sz = (sl_rx_pool->phy_sl_bitmap.size << 3) - sl_rx_pool->phy_sl_bitmap.bits_unused;
    frameslot_t pssch_fs_tx = {rx_ind->sfn, rx_ind->slot};
    uint64_t pssch_abs_tx = normalize(&pssch_fs_tx, mu_tx);
    int prb_row = sl_psfch_pssch_slot_index(&sl_rx_pool->phy_sl_bitmap, rx_map_sz, pssch_abs_tx, (uint8_t)psfch_period);
    const uint16_t rb_ordinal = psfch_params->prb_ordinal_base + prb_row * psfch_params->prbs_per_subchannel_slot;
    DevAssert(prb_row >= 0 && prb_row < psfch_params->num_psfch_slots);
    DevAssert(rb_ordinal < psfch_params->num_rbs);
    sched_psfch->prb = psfch_params->rb_set[rb_ordinal];
    LOG_D(NR_PHY,
          "slot %d, prb_row %d subchannel %u psfch_period %ld, sched_psfch->prb %d\n",
          rx_ind->slot,
          prb_row,
          psfch_params->selected_subchannel,
          psfch_period,
          sched_psfch->prb);
  }
  int locbw = sl_bwp->sl_BWP_r16->locationAndBandwidth;
  sched_psfch->sl_bwp_start   = NRRIV2PRBOFFSET(locbw, MAX_BWP_SIZE);
  sched_psfch->freq_hop_flag  = 0;
  sched_psfch->group_hop_flag = 0;
  sched_psfch->second_hop_prb = 0;
  sched_psfch->sequence_hop_flag = 0;
  sched_psfch->harq_feedback = rx_slsch->harq_feedback;
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
  const int slots_per_frame = mac->frame_structure.numb_slots_frame;
  const int sfn_cycle_slots = 1024 * slots_per_frame;
  const int now = frame * slots_per_frame + slot;
  int cur = sched_ctrl->feedback_sl_harq.head;
  while (cur != -1) {
    // remove_nr_list() resets next[cur] to -1, so preserve the traversal link
    // before removing an expired HARQ process.
    int next = sched_ctrl->feedback_sl_harq.next[cur];
    NR_UE_sl_harq_t *harq = &sched_ctrl->sl_harq_processes[cur];
    const int feedback = harq->feedback_frame * slots_per_frame + harq->feedback_slot;
    const int forward_distance = (feedback - now + sfn_cycle_slots) % sfn_cycle_slots;
    const bool feedback_is_past = harq->feedback_slot >= 0 && forward_distance > sfn_cycle_slots / 2;
    if (feedback_is_past) {
      remove_nr_list(&sched_ctrl->feedback_sl_harq, cur);
      harq->feedback_slot = -1;
      harq->is_waiting = false;
      if (harq->round >= HARQ_ROUND_MAX - 1) {
        abort_nr_ue_sl_harq(mac, cur, UE);
      } else {
        add_tail_nr_list(&sched_ctrl->retrans_sl_harq, cur);
        harq->round++;
      }
    }
    cur = next;
  }
}

void configure_psfch_params_rx(int module_idP,
                            NR_UE_MAC_INST_t *mac,
                            sl_nr_rx_config_request_t *rx_config)
{
  const uint16_t slot = rx_config->slot;
  frame_t frame = rx_config->sfn;
  const uint8_t psfch_periods[] = {0,1,2,4};
  /* PSFCH received here acknowledges a PSSCH transmitted by this UE, so its
   * resources come from the TX resource pool of that PSSCH (TS 38.213 16.3.1). */
  NR_SL_PSFCH_Config_r16_t *sl_psfch_config = mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup;
  long psfch_period = (sl_psfch_config->sl_PSFCH_Period_r16)
                        ? psfch_periods[*sl_psfch_config->sl_PSFCH_Period_r16] : 0;
  uint16_t num_subch = sl_get_num_subch(mac->sl_tx_res_pool);
  const int max_psfch_pdus = psfch_period * num_subch;
  free(rx_config->sl_rx_config_list[0].rx_psfch_pdu_list);
  rx_config->sl_rx_config_list[0].rx_psfch_pdu_list = calloc(max_psfch_pdus, sizeof(sl_nr_tx_rx_config_psfch_pdu_t));
  rx_config->sl_rx_config_list[0].num_psfch_pdus = 0;
  NR_SL_UEs_t *UE_info = &mac->sl_info;

  if (*(UE_info->list) == NULL) {
    LOG_D(NR_MAC, "UE list is empty\n");
    return;
  }

  SL_UE_iterator(UE_info->list, UE)
  {
    NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    NR_UE_sl_harq_t **matched_harqs = (NR_UE_sl_harq_t **) calloc(sched_ctrl->feedback_sl_harq.len, sizeof(NR_UE_sl_harq_t *));
    int matched_sz = find_current_slot_harqs(frame, slot, sched_ctrl, matched_harqs);
    LOG_D(NR_MAC, "%s matched_sz %d\n", __FUNCTION__, matched_sz);
    for (int i = 0; i < matched_sz; i++) {
      NR_UE_sl_harq_t *cur_harq = matched_harqs[i];
      const sl_psfch_feedback_mode_t feedback_mode = get_psfch_feedback_mode(cur_harq->sched_pssch.second_stage_sci_format,
                                                                             cur_harq->sched_pssch.cast_type,
                                                                             cur_harq->sched_pssch.harq_feedback);
      if (feedback_mode == SL_PSFCH_NO_FEEDBACK)
        continue;
      int pdu_index = rx_config->sl_rx_config_list[0].num_psfch_pdus;
      AssertFatal(pdu_index < max_psfch_pdus, "PSFCH RX needs more than the configured %d resources\n", max_psfch_pdus);
      psfch_params_t psfch_params = {0};
      /* Each pending HARQ retains the identity and frequency allocation of
       * its actual PSSCH transmission.  Multiple HARQs may share this PSFCH
       * occasion, so derive every resource independently. */
      compute_params(module_idP,
                     mac->sl_tx_res_pool,
                     &psfch_params,
                     cur_harq->sched_pssch.pssch_start_subchannel,
                     cur_harq->sched_pssch.pssch_num_subchannels,
                     cur_harq->sched_pssch.source_id,
                     cur_harq->sched_pssch.dest_id,
                     cur_harq->sched_pssch.cast_type,
                     UE->uid);
      sl_nr_tx_rx_config_psfch_pdu_t *psfch_pdu = &rx_config->sl_rx_config_list[0].rx_psfch_pdu_list[pdu_index];
      fill_psfch_params_rx(rx_config, psfch_pdu, &psfch_params, cur_harq, mac, psfch_period, slot, UE->uid);
      free_psfch_params(&psfch_params, psfch_period);
    }
    free(matched_harqs);
    matched_harqs = NULL;
  }
}

void fill_psfch_params_rx(sl_nr_rx_config_request_t *rx_config, sl_nr_tx_rx_config_psfch_pdu_t *psfch_pdu, psfch_params_t *psfch_params, NR_UE_sl_harq_t *cur_harq, NR_UE_MAC_INST_t *mac, long psfch_period, const uint16_t slot, uint16_t peer_id) {
  rx_config->sl_rx_config_list[0].num_psfch_pdus++;
  psfch_pdu->peer_id = peer_id;
  psfch_pdu->harq_pid = cur_harq->sl_harq_pid;
  psfch_pdu->initial_cyclic_shift = psfch_params->m0;
  LOG_D(NR_MAC, "psfch_pdu->initial_cyclic_shift %i\n", psfch_pdu->initial_cyclic_shift);
  const uint8_t values[] = {7, 8, 9, 10, 11, 12, 13, 14};
  NR_SL_BWP_Generic_r16_t *sl_bwp = mac->sl_bwp->sl_BWP_Generic_r16;
  uint8_t sl_num_symbols = *sl_bwp->sl_LengthSymbols_r16 ? values[*sl_bwp->sl_LengthSymbols_r16] : 0;
  // start_symbol_index has been used as lprime check 38.213 16.3
  psfch_pdu->start_symbol_index = *sl_bwp->sl_StartSymbol_r16 + sl_num_symbols - 2;
  LOG_D(NR_PHY, "Rx sl_StartSymbol_r16 %ld, sl_num_symbols: %d, start sym index %d, mcs %d\n", *sl_bwp->sl_StartSymbol_r16, sl_num_symbols, psfch_pdu->start_symbol_index, psfch_pdu->mcs);
  psfch_pdu->hopping_id = *mac->sl_bwp->sl_BWP_PoolConfigCommon_r16->sl_TxPoolSelectedNormal_r16->list.array[0]->sl_ResourcePool_r16->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_HopID_r16;
  /* PRB row index (TS 38.213 s16.3): PSSCH slot position in the group sharing this
   * PSFCH occasion.  Must match fill_psfch_params_tx on the peer UE.  Use the
   * pool-cycle SL-slot ordinal mod psfch_period via sl_psfch_pssch_slot_index,
   * not slot-in-frame parity which collapses non-consecutive SL slots. */
  uint8_t index;
  {
    uint8_t mu_rx = (uint8_t)get_softmodem_params()->numerology;
    SL_ResourcePool_params_t *sl_tx_pool = mac->SL_MAC_PARAMS->sl_TxPool[0];
    size_t tx_map_sz = (sl_tx_pool->phy_sl_bitmap.size << 3) - sl_tx_pool->phy_sl_bitmap.bits_unused;
    frameslot_t pssch_fs_rx = {cur_harq->sched_pssch.frame, cur_harq->sched_pssch.slot};
    uint64_t pssch_abs_rx = normalize(&pssch_fs_rx, mu_rx);
    index = (uint8_t)sl_psfch_pssch_slot_index(&sl_tx_pool->phy_sl_bitmap, tx_map_sz, pssch_abs_rx, (uint8_t)psfch_period);
  }
  const uint16_t rb_ordinal = psfch_params->prb_ordinal_base + index * psfch_params->prbs_per_subchannel_slot;
  DevAssert(index < psfch_params->num_psfch_slots);
  DevAssert(rb_ordinal < psfch_params->num_rbs);
  psfch_pdu->prb = psfch_params->rb_set[rb_ordinal];
  LOG_D(NR_PHY,
        "Rx slot %d, slsch tx %d.%d, prb_row %d subchannel %u psfch_period %ld, start_prb %d\n",
        slot,
        cur_harq->sched_pssch.frame,
        cur_harq->sched_pssch.slot,
        index,
        psfch_params->selected_subchannel,
        psfch_period,
        psfch_pdu->prb);
  int locbw = sl_bwp->sl_BWP_r16->locationAndBandwidth;
  psfch_pdu->sl_bwp_start   = NRRIV2PRBOFFSET(locbw, MAX_BWP_SIZE);
  psfch_pdu->freq_hop_flag  = 0;
  psfch_pdu->group_hop_flag = 0;
  psfch_pdu->second_hop_prb = 0;
  psfch_pdu->sequence_hop_flag = 0;
  const sl_psfch_feedback_mode_t feedback_mode = get_psfch_feedback_mode(cur_harq->sched_pssch.second_stage_sci_format,
                                                                         cur_harq->sched_pssch.cast_type,
                                                                         cur_harq->sched_pssch.harq_feedback);
  DevAssert(feedback_mode != SL_PSFCH_NO_FEEDBACK);
  psfch_pdu->bit_len_harq = feedback_mode == SL_PSFCH_ACK_NACK ? 1 : 0;
  /* This function is called only for a matched feedback occasion.  A PSFCH
   * occasion always reserves AGC + PSFCH + guard, with one PSFCH symbol. */
  int num_psfch_symbols = psfch_period > 0 ? 3 : 0;
  psfch_pdu->nr_of_symbols = num_psfch_symbols ? num_psfch_symbols - 2 : 0; // (num_psfch_symbols - 2) excludes PSFCH AGC and Guard
  LOG_D(NR_PHY, "%s start_symbol_index %d, sl_bwp_start %d, sequence_hop_flag %d, \
        second_hop_prb %d, prb %d, nr_of_symbols %d, initial_cyclic_shift %d, hopping_id %d, \
        group_hop_flag %d, freq_hop_flag %d, bit_len_harq %d----> Setting pdu type SL_NR_CONFIG_TYPE_RX_PSFCH  \n",
        __FUNCTION__,
        psfch_pdu->start_symbol_index, psfch_pdu->sl_bwp_start,
        psfch_pdu->sequence_hop_flag, psfch_pdu->second_hop_prb, psfch_pdu->prb,
        psfch_pdu->nr_of_symbols, psfch_pdu->initial_cyclic_shift, psfch_pdu->hopping_id,
        psfch_pdu->group_hop_flag, psfch_pdu->freq_hop_flag, psfch_pdu->bit_len_harq);
}

int sl_num_slsch_feedbacks(NR_UE_MAC_INST_t *mac) {
  return mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch_size;
}

bool is_feedback_scheduled(NR_UE_MAC_INST_t *mac, int frameP,int slotP) {
  for (int i = 0; i < sl_num_slsch_feedbacks(mac); i++) {
    SL_sched_feedback_t  *sched_psfch = &mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch[i];
    LOG_D(NR_MAC, "frame.slot %4d.%2d, harq_feedback %d\n", frameP, slotP, sched_psfch->harq_feedback);
    if (frameP == sched_psfch->feedback_frame && slotP == sched_psfch->feedback_slot && sched_psfch->harq_feedback) {
      return true;
    }
  }
  return false;
}

void reset_sched_psfch(NR_UE_MAC_INST_t *mac, int frameP,int slotP) {

  for (int i = 0; i < sl_num_slsch_feedbacks(mac); i++) {
    SL_sched_feedback_t  *sched_psfch = &mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch[i];
    if (frameP == sched_psfch->feedback_frame && slotP == sched_psfch->feedback_slot) {
      sched_psfch->feedback_frame = -1;
      sched_psfch->feedback_slot = -1;
      sched_psfch->harq_feedback = 0;
    }
  }
}

static bool sl_rx_harq_should_deliver(NR_SL_UE_info_t *UE,
                                      const sl_nr_slsch_pdu_t *rx_slsch,
                                      const NR_SLSCH_MAC_SUBHEADER_FIXED *sl_sch_subheader)
{
  if (rx_slsch->harq_pid >= NR_MAX_HARQ_PROCESSES) {
    LOG_W(NR_MAC, "Ignoring SL-SCH with invalid HARQ process ID %u\n", rx_slsch->harq_pid);
    return false;
  }

  NR_UE_sl_rx_harq_t *harq = &UE->UE_sched_ctrl.sl_rx_harq_processes[rx_slsch->harq_pid];
  /* SCI-2 and the SL-SCH fixed header carry complementary portions of each
   * 24-bit Layer-2 identity. Keep both portions in the receiving-process key. */
  const bool same_process = harq->valid && harq->sci_source_id == rx_slsch->source_id
                            && harq->mac_source_id == sl_sch_subheader->SRC
                            && harq->sci_dest_id == rx_slsch->dest_id
                            && harq->mac_dest_id == sl_sch_subheader->DST;

  /* TS 38.321 5.22.2.2.1: associate the receiving sidelink process with the
   * source/destination Layer-2 identities and sidelink process ID. For that
   * process, toggled NDI identifies a new transport block. */
  if (!same_process || harq->ndi != rx_slsch->ndi) {
    harq->valid = true;
    harq->sci_source_id = rx_slsch->source_id;
    harq->mac_source_id = sl_sch_subheader->SRC;
    harq->sci_dest_id = rx_slsch->dest_id;
    harq->mac_dest_id = sl_sch_subheader->DST;
    harq->ndi = rx_slsch->ndi;
    harq->delivered = false;
  }

  if (harq->delivered)
    return false;

  harq->delivered = true;
  return true;
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
  if (!pduP) {
    return;
  }
  uint8_t psfch_period = 0;
  // sl_PSFCH_Period_r16 is an enum index (0..3); map it to the actual slot period
  // {0,1,2,4} as everywhere else (raw index 3 maps to period 4).
  const uint8_t psfch_periods[] = {0, 1, 2, 4};
  /* This UE transmits PSFCH for a received PSSCH.  Its enablement and
   * resources therefore belong to the RX pool on which that PSSCH arrived,
   * not to the pool used for this UE's own PSSCH transmissions. */
  NR_SetupRelease_SL_PSFCH_Config_r16_t *rx_pool_psfch =
      mac->sl_rx_res_pool ? mac->sl_rx_res_pool->sl_PSFCH_Config_r16 : NULL;
  if (rx_pool_psfch && rx_pool_psfch->present == NR_SetupRelease_SL_PSFCH_Config_r16_PR_setup
      && rx_pool_psfch->choice.setup && rx_pool_psfch->choice.setup->sl_PSFCH_Period_r16)
    psfch_period = psfch_periods[*rx_pool_psfch->choice.setup->sl_PSFCH_Period_r16];
  if (psfch_period && rx_slsch_pdu->harq_feedback) {
    configure_psfch_params_tx(module_idP, mac, rx_ind, pdu_id);
  }

  NR_SL_UE_info_t *UE = find_UE(mac, rx_slsch_pdu->source_id);

  if (UE == NULL)
    return;

  if (pdu_type == SL_NR_RX_PDU_TYPE_SLSCH_PSFCH) {
    handle_nr_ue_sl_harq(module_idP, frame, slot, rx_slsch_pdu, rx_slsch_pdu->source_id);
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

  LOG_D(NR_MAC, "%4d.%2d ack_nack %d pdu_type %d\n",
        frame, slot, rx_slsch_pdu->ack_nack, pdu_type);
  if (rx_slsch_pdu->ack_nack == 0) {
    LOG_D(NR_MAC, "%4d.%2d SLSCH HARQ PID %u decode failed (NACK), not delivering to RLC\n", frame, slot, rx_slsch_pdu->harq_pid);
    return;
  }
  if (pdu_len < (int32_t)sizeof(NR_SLSCH_MAC_SUBHEADER_FIXED)) {
    LOG_W(NR_MAC, "%4d.%2d SL-SCH PDU is too short for the fixed header: %d bytes\n", frame, slot, pdu_len);
    return;
  }
  NR_SLSCH_MAC_SUBHEADER_FIXED *sl_sch_subheader = (NR_SLSCH_MAC_SUBHEADER_FIXED *)pduP;
  if (!sl_rx_harq_should_deliver(UE, rx_slsch_pdu, sl_sch_subheader)) {
    LOG_D(NR_MAC, "%4d.%2d Suppressing duplicate SL-SCH delivery for HARQ PID %u\n", frame, slot, rx_slsch_pdu->harq_pid);
    return;
  }

  LOG_D(NR_MAC, "In %s : processing PDU %d (with length %d) of %d total number of PDUs...\n", __FUNCTION__, pdu_id, pdu_len, rx_ind->number_pdus);
  LOG_D(NR_PHY, "%4d.%2d Rx V %d R %d SRC %d DST %d\n", frame, slot, sl_sch_subheader->V, sl_sch_subheader->R, sl_sch_subheader->SRC, sl_sch_subheader->DST);
  pduP += sizeof(*sl_sch_subheader);
  pdu_len -= sizeof(*sl_sch_subheader);
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

        #if 0
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
        #endif

        nr_rlc_data_ind_t data_ind = {.ch = rx_lcid, .buf = pduP + mac_subheader_len, .len = mac_len};
        nr_mac_rlc_data_ind(mac->ue_id, mac->ue_id, false, &data_ind, 1);

	      break;
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
      default:
        // TS 38.321 Table 6.2.4-1 assigns the SL-SCH LCID values. This
        // integration cannot determine the size of unsupported sub-PDUs, so
        // stop before payload bytes can be interpreted as another subheader.
        LOG_W(NR_MAC, "%4d.%2d Unsupported SL-SCH LCID %d; discarding remaining PDU\n", frame, slot, rx_lcid);
        mac_len = pdu_len - mac_subheader_len;
        done = 1;
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
  const int first_ul_slot_period = tdd ? get_first_ul_slot(&mac->frame_structure, false) : 0;
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
  /* t2min is stored in ms (TS 38.214 8.1.4: T2min in {1,5,10,20} ms).
   * Convert to slots: 1 ms = 2^mu slots at SCS 2^mu x 15 kHz. */
  return sl_mac->sl_TxPool[pool_id]->t2min << mu;
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

/* TS 38.331: sl_SensingWindow_r16 enum {ms100=0, ms1100=1} -> ms */
uint16_t nr_sl_sensing_window_to_ms(long idx)
{
  static const uint16_t tbl[] = {100, 1100};
  AssertFatal(idx == 0 || idx == 1,
              "Invalid sl-SensingWindow enum %ld (0=ms100, 1=ms1100)\n", idx);
  return tbl[idx];
}

/* TS 38.331: sl_SelectionWindow_r16 enum {n1=0, n5=1, n10=2, n20=3} -> ms */
uint16_t nr_sl_sw_to_ms(long idx)
{
  static const uint16_t tbl[] = {1, 5, 10, 20};
  AssertFatal(idx >= 0 && idx <= 3,
              "Invalid sl-SelectionWindow enum %ld (0=n1, 1=n5, 2=n10, 3=n20)\n", idx);
  return tbl[idx];
}

/* TS 38.331: sl-ResourceReservePeriod-r16 -> ms.
 * period1 enum {ms0=0, ms100=1, ..., ms1000=10}.
 * period2 INTEGER (1..99): the value is the reservation period in ms.
 * Returns 0 for absent or ms0 (non-periodic). */
uint16_t nr_sl_rrp_to_ms(const NR_SL_ResourceReservePeriod_r16_t *p)
{
  if (!p || p->present == NR_SL_ResourceReservePeriod_r16_PR_NOTHING)
    return 0;
  if (p->present == NR_SL_ResourceReservePeriod_r16_PR_sl_ResourceReservePeriod1_r16) {
    static const uint16_t tbl[] = {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
    long idx = p->choice.sl_ResourceReservePeriod1_r16;
    AssertFatal(idx >= 0 && idx <= 10,
                "Invalid sl-ResourceReservePeriod1 enum %ld (must be 0..10)\n", idx);
    return tbl[idx];
  }
  /* period2: INTEGER (1..99), expressed directly in ms */
  long val = p->choice.sl_ResourceReservePeriod2_r16;
  AssertFatal(val >= 1 && val <= 99,
              "Invalid sl-ResourceReservePeriod2 %ld (must be 1..99)\n", val);
  return (uint16_t)val;
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
