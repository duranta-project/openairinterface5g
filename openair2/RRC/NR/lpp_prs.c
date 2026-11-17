/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "lpp_prs.h"
#include "f1ap_messages_types.h"

#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "LPP_DL-PRS-MutingOption1-r16.h"
#include "LPP_DL-PRS-MutingOption2-r16.h"
#include "LPP_DL-PRS-QCL-Info-r16.h"
#include "LPP_AssistanceDataSIBelement-r15.h"
#include "LPP_DL-SelectedPRS-ResourceIndex-r16.h"
#include "LPP_DL-SelectedPRS-ResourceSetIndex-r16.h"
#include "LPP_LPP-Message.h"
#include "LPP_LPP-MessageBody.h"
#include "LPP_LPP-TransactionID.h"
#include "LPP_NCGI-r15.h"
#include "LPP_NR-DL-PRS-AssistanceData-r16.h"
#include "LPP_NR-DL-PRS-AssistanceDataPerFreq-r16.h"
#include "LPP_NR-DL-PRS-AssistanceDataPerTRP-r16.h"
#include "LPP_NR-DL-PRS-Periodicity-and-ResourceSetSlotOffset-r16.h"
#include "LPP_NR-DL-PRS-Resource-r16.h"
#include "LPP_NR-DL-PRS-ResourceSet-r16.h"
#include "LPP_NR-DL-TDOA-ProvideAssistanceData-r16.h"
#include "LPP_NR-MutingPattern-r16.h"
#include "LPP_NR-SSB-Config-r16.h"
#include "LPP_NR-SelectedDL-PRS-IndexList-r16.h"
#include "LPP_NR-SelectedDL-PRS-IndexPerTRP-r16.h"
#include "LPP_NR-SelectedDL-PRS-PerFreq-r16.h"
#include "LPP_ProvideAssistanceData-r9-IEs.h"
#include "LPP_ProvideAssistanceData.h"
#include "NR_BCCH-DL-SCH-Message.h"
#include "common/utils/oai_asn1.h"
#include "constraints.h"
#include "common/utils/LOG/log.h"
#include "uper_encoder.h"

#define LPP_PRS_RESOURCE_SET_ID 0
#define LPP_PRS_FREQUENCY_LAYER_INDEX 0
#define LPP_PRS_TRP_INDEX 0
#define LPP_PRS_DL_PRS_ID 0

static bool map_resource_bandwidth(uint16_t num_rb, long *encoded_bandwidth)
{
  if (num_rb < 24 || num_rb > 272 || (num_rb - 24) % 4 != 0) {
    LOG_E(GNB_APP,
          "Cannot generate NR-DL-TDOA assistance data: PRS NumRB %u is not representable by "
          "dl-PRS-ResourceBandwidth-r16 (expected 24..272 PRBs in steps of 4)\n",
          num_rb);
    return false;
  }

  *encoded_bandwidth = (num_rb - 20) / 4;
  return true;
}

static bool map_comb_size(uint8_t comb_size, long *frequency_comb, int *resource_present)
{
  switch (comb_size) {
    case 2:
      *frequency_comb = LPP_NR_DL_PRS_PositioningFrequencyLayer_r16__dl_PRS_CombSizeN_r16_n2;
      *resource_present = LPP_NR_DL_PRS_Resource_r16__dl_PRS_CombSizeN_AndReOffset_r16_PR_n2_r16;
      return true;
    case 4:
      *frequency_comb = LPP_NR_DL_PRS_PositioningFrequencyLayer_r16__dl_PRS_CombSizeN_r16_n4;
      *resource_present = LPP_NR_DL_PRS_Resource_r16__dl_PRS_CombSizeN_AndReOffset_r16_PR_n4_r16;
      return true;
    case 6:
      *frequency_comb = LPP_NR_DL_PRS_PositioningFrequencyLayer_r16__dl_PRS_CombSizeN_r16_n6;
      *resource_present = LPP_NR_DL_PRS_Resource_r16__dl_PRS_CombSizeN_AndReOffset_r16_PR_n6_r16;
      return true;
    case 12:
      *frequency_comb = LPP_NR_DL_PRS_PositioningFrequencyLayer_r16__dl_PRS_CombSizeN_r16_n12;
      *resource_present = LPP_NR_DL_PRS_Resource_r16__dl_PRS_CombSizeN_AndReOffset_r16_PR_n12_r16;
      return true;
    default:
      LOG_E(GNB_APP, "Cannot generate NR-DL-TDOA assistance data: unsupported PRS CombSize %u\n", comb_size);
      return false;
  }
}

static bool map_num_symbols(uint8_t num_symbols, long *encoded)
{
  switch (num_symbols) {
    case 2: *encoded = LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_NumSymbols_r16_n2; return true;
    case 4: *encoded = LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_NumSymbols_r16_n4; return true;
    case 6: *encoded = LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_NumSymbols_r16_n6; return true;
    case 12: *encoded = LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_NumSymbols_r16_n12; return true;
    default:
      LOG_E(GNB_APP,
            "Cannot generate NR-DL-TDOA assistance data: unsupported PRS NumPRSSymbols %u\n",
            num_symbols);
      return false;
  }
}

static bool map_repetition(uint8_t repetition, long **encoded)
{
  if (repetition == 1) {
    *encoded = NULL;
    return true;
  }

  long value;
  switch (repetition) {
    case 2: value = LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_ResourceRepetitionFactor_r16_n2; break;
    case 4: value = LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_ResourceRepetitionFactor_r16_n4; break;
    case 6: value = LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_ResourceRepetitionFactor_r16_n6; break;
    case 8: value = LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_ResourceRepetitionFactor_r16_n8; break;
    case 16: value = LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_ResourceRepetitionFactor_r16_n16; break;
    case 32: value = LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_ResourceRepetitionFactor_r16_n32; break;
    default:
      LOG_E(GNB_APP,
            "Cannot generate NR-DL-TDOA assistance data: unsupported PRSResourceRepetition %u\n",
            repetition);
      return false;
  }
  asn1cCallocOne(*encoded, value);
  return true;
}

static bool map_time_gap(uint8_t repetition, uint8_t gap, long **encoded)
{
  if (repetition == 1) {
    *encoded = NULL;
    return true;
  }

  long value;
  switch (gap) {
    case 1: value = LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_ResourceTimeGap_r16_s1; break;
    case 2: value = LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_ResourceTimeGap_r16_s2; break;
    case 4: value = LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_ResourceTimeGap_r16_s4; break;
    case 8: value = LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_ResourceTimeGap_r16_s8; break;
    case 16: value = LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_ResourceTimeGap_r16_s16; break;
    case 32: value = LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_ResourceTimeGap_r16_s32; break;
    default:
      LOG_E(GNB_APP,
            "Cannot generate NR-DL-TDOA assistance data: unsupported PRSResourceTimeGap %u\n",
            gap);
      return false;
  }
  asn1cCallocOne(*encoded, value);
  return true;
}

#define PERIOD_CASE(inner, prefix, value) \
  case value:                             \
    (inner)->present = prefix##_n##value##_r16; \
    (inner)->choice.n##value##_r16 = offset;   \
    return true

static bool set_periodicity(LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16_t *asn_period,
                            long scs,
                            uint16_t period,
                            uint16_t offset)
{
  if (offset >= period) {
    LOG_E(GNB_APP,
          "Cannot generate NR-DL-TDOA assistance data: PRS period/offset [%u, %u] has offset >= period\n",
          period,
          offset);
    return false;
  }

  switch (scs) {
    case 0: {
      asn_period->present = LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16_PR_scs15_r16;
      asn_period->choice.scs15_r16 = calloc_or_fail(1, sizeof(*asn_period->choice.scs15_r16));
      struct LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs15_r16 *inner =
          asn_period->choice.scs15_r16;
      switch (period) {
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs15_r16_PR, 4);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs15_r16_PR, 5);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs15_r16_PR, 8);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs15_r16_PR, 10);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs15_r16_PR, 16);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs15_r16_PR, 20);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs15_r16_PR, 32);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs15_r16_PR, 40);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs15_r16_PR, 64);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs15_r16_PR, 80);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs15_r16_PR, 160);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs15_r16_PR, 320);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs15_r16_PR, 640);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs15_r16_PR, 1280);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs15_r16_PR, 2560);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs15_r16_PR, 5120);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs15_r16_PR, 10240);
        default: break;
      }
      break;
    }
    case 1: {
      asn_period->present = LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16_PR_scs30_r16;
      asn_period->choice.scs30_r16 = calloc_or_fail(1, sizeof(*asn_period->choice.scs30_r16));
      struct LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16 *inner =
          asn_period->choice.scs30_r16;
      switch (period) {
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR, 8);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR, 10);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR, 16);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR, 20);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR, 32);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR, 40);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR, 64);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR, 80);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR, 128);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR, 160);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR, 320);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR, 640);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR, 1280);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR, 2560);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR, 5120);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR, 10240);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR, 20480);
        default: break;
      }
      break;
    }
    case 2: {
      asn_period->present = LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16_PR_scs60_r16;
      asn_period->choice.scs60_r16 = calloc_or_fail(1, sizeof(*asn_period->choice.scs60_r16));
      struct LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs60_r16 *inner =
          asn_period->choice.scs60_r16;
      switch (period) {
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs60_r16_PR, 16);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs60_r16_PR, 20);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs60_r16_PR, 32);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs60_r16_PR, 40);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs60_r16_PR, 64);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs60_r16_PR, 80);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs60_r16_PR, 128);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs60_r16_PR, 160);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs60_r16_PR, 256);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs60_r16_PR, 320);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs60_r16_PR, 640);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs60_r16_PR, 1280);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs60_r16_PR, 2560);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs60_r16_PR, 5120);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs60_r16_PR, 10240);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs60_r16_PR, 20480);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs60_r16_PR, 40960);
        default: break;
      }
      break;
    }
    case 3: {
      asn_period->present = LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16_PR_scs120_r16;
      asn_period->choice.scs120_r16 = calloc_or_fail(1, sizeof(*asn_period->choice.scs120_r16));
      struct LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs120_r16 *inner =
          asn_period->choice.scs120_r16;
      switch (period) {
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs120_r16_PR, 32);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs120_r16_PR, 40);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs120_r16_PR, 64);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs120_r16_PR, 80);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs120_r16_PR, 128);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs120_r16_PR, 160);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs120_r16_PR, 256);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs120_r16_PR, 320);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs120_r16_PR, 512);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs120_r16_PR, 640);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs120_r16_PR, 1280);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs120_r16_PR, 2560);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs120_r16_PR, 5120);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs120_r16_PR, 10240);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs120_r16_PR, 20480);
        PERIOD_CASE(inner, LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs120_r16_PR, 40960);
        default: break;
      }
      break;
    }
    default:
      LOG_E(GNB_APP, "Cannot generate NR-DL-TDOA assistance data: unsupported PRS SCS value %ld\n", scs);
      return false;
  }

  LOG_E(GNB_APP,
        "Cannot generate NR-DL-TDOA assistance data: PRS period %u is invalid for SCS value %ld\n",
        period,
        scs);
  return false;
}

#undef PERIOD_CASE

static void set_bit_string36(BIT_STRING_t *bit_string, uint64_t value)
{
  bit_string->buf = calloc_or_fail(1, 5);
  bit_string->size = 5;
  bit_string->bits_unused = 4;
  bit_string->buf[0] = (value >> 28) & 0xff;
  bit_string->buf[1] = (value >> 20) & 0xff;
  bit_string->buf[2] = (value >> 12) & 0xff;
  bit_string->buf[3] = (value >> 4) & 0xff;
  bit_string->buf[4] = (value & 0x0f) << 4;
}

static void add_decimal_digits(void *list, unsigned value, unsigned digits)
{
  unsigned divisor = 1;
  for (unsigned i = 1; i < digits; ++i)
    divisor *= 10;
  for (unsigned i = 0; i < digits; ++i) {
    long *digit;
    asn1cCallocOne(digit, value / divisor % 10);
    asn1cSeqAdd(list, digit);
    divisor /= 10;
  }
}

typedef struct {
  uint32_t MutingPattern1[32];
  uint32_t MutingPattern2[32];
  uint8_t MutingBitRepetition;
} lpp_prs_muting_config_t;

static bool set_muting_pattern(LPP_NR_MutingPattern_r16_t *pattern, const uint32_t *values, size_t length)
{
  BIT_STRING_t *bits = NULL;
  switch (length) {
    case 2: pattern->present = LPP_NR_MutingPattern_r16_PR_po2_r16; bits = &pattern->choice.po2_r16; break;
    case 4: pattern->present = LPP_NR_MutingPattern_r16_PR_po4_r16; bits = &pattern->choice.po4_r16; break;
    case 6: pattern->present = LPP_NR_MutingPattern_r16_PR_po6_r16; bits = &pattern->choice.po6_r16; break;
    case 8: pattern->present = LPP_NR_MutingPattern_r16_PR_po8_r16; bits = &pattern->choice.po8_r16; break;
    case 16: pattern->present = LPP_NR_MutingPattern_r16_PR_po16_r16; bits = &pattern->choice.po16_r16; break;
    case 32: pattern->present = LPP_NR_MutingPattern_r16_PR_po32_r16; bits = &pattern->choice.po32_r16; break;
    default:
      LOG_E(GNB_APP, "Cannot generate NR-DL-TDOA assistance data: muting pattern length %zu is invalid\n", length);
      return false;
  }
  bits->size = (length + 7) / 8;
  bits->bits_unused = bits->size * 8 - length;
  bits->buf = calloc_or_fail(1, bits->size);
  for (size_t i = 0; i < length; ++i) {
    if (values[i] > 1) {
      LOG_E(GNB_APP, "Cannot generate NR-DL-TDOA assistance data: muting pattern element %zu is not binary\n", i);
      return false;
    }
    bits->buf[i / 8] |= values[i] << (7 - i % 8);
  }
  return true;
}

static bool add_muting(LPP_NR_DL_PRS_ResourceSet_r16_t *set,
                       const lpp_prs_muting_config_t *cfg,
                       size_t pattern1_length,
                       size_t pattern2_length)
{
  if (pattern1_length > 0) {
    set->dl_PRS_MutingOption1_r16 = calloc_or_fail(1, sizeof(*set->dl_PRS_MutingOption1_r16));
    long repetition;
    switch (cfg->MutingBitRepetition) {
      case 1: repetition = LPP_DL_PRS_MutingOption1_r16__dl_prs_MutingBitRepetitionFactor_r16_n1; break;
      case 2: repetition = LPP_DL_PRS_MutingOption1_r16__dl_prs_MutingBitRepetitionFactor_r16_n2; break;
      case 4: repetition = LPP_DL_PRS_MutingOption1_r16__dl_prs_MutingBitRepetitionFactor_r16_n4; break;
      case 8: repetition = LPP_DL_PRS_MutingOption1_r16__dl_prs_MutingBitRepetitionFactor_r16_n8; break;
      default:
        LOG_E(GNB_APP,
              "Cannot generate NR-DL-TDOA assistance data: MutingBitRepetition %u is invalid\n",
              cfg->MutingBitRepetition);
        return false;
    }
    asn1cCallocOne(set->dl_PRS_MutingOption1_r16->dl_prs_MutingBitRepetitionFactor_r16, repetition);
    if (!set_muting_pattern(&set->dl_PRS_MutingOption1_r16->nr_option1_muting_r16,
                            cfg->MutingPattern1,
                            pattern1_length))
      return false;
  }
  if (pattern2_length > 0) {
    set->dl_PRS_MutingOption2_r16 = calloc_or_fail(1, sizeof(*set->dl_PRS_MutingOption2_r16));
    if (!set_muting_pattern(&set->dl_PRS_MutingOption2_r16->nr_option2_muting_r16,
                            cfg->MutingPattern2,
                            pattern2_length))
      return false;
  }
  return true;
}

static bool check_asn1_constraints(const asn_TYPE_descriptor_t *type, const void *value, const char *name)
{
  char error[1024] = {0};
  size_t error_length = sizeof(error);
  if (asn_check_constraints(type, value, error, &error_length) == 0)
    return true;

  LOG_E(GNB_APP, "Cannot generate PRS PosSIB: %s ASN.1 constraint failed: %s\n", name, error);
  return false;
}

static bool encode_uper_to_octet_string(const asn_TYPE_descriptor_t *type,
                                        const void *value,
                                        OCTET_STRING_t *encoded,
                                        const char *name)
{
  void *buffer = NULL;
  const ssize_t size = uper_encode_to_new_buffer(type, NULL, value, &buffer);
  if (size < 0) {
    LOG_E(GNB_APP, "Cannot generate PRS PosSIB: UPER encoding of %s failed\n", name);
    return false;
  }

  encoded->buf = buffer;
  encoded->size = size;
  return true;
}

bool lpp_prs_from_f1(const f1ap_prs_configuration_t *prs, const f1ap_ng_ran_cgi_t *cgi,
                     uint16_t pci, uint8_t **segment, size_t *segment_len)
{
  if (!segment || !segment_len)
    return false;
  *segment = NULL;
  *segment_len = 0;
  if (!prs || !cgi || prs->prs_resource_set_list_length != 1 || !prs->prs_resource_set)
    return false;
  const f1ap_prs_resource_set_t *src = &prs->prs_resource_set[0];
  long bandwidth = 0, frequency_comb = 0;
  int resource_present = 0;
  if (!src->prs_resource_list_length || !src->prs_resource || src->subcarrier_spacing > 3 || src->cp_type > 1 ||
      src->resource_set_periodicity > UINT16_MAX || src->resource_set_slot_offset > UINT16_MAX ||
      src->muting1_length > 32 || src->muting2_length > 32 ||
      !map_resource_bandwidth(src->num_rb, &bandwidth) ||
      !map_comb_size(src->comb_size, &frequency_comb, &resource_present))
    return false;
  for (int i = 0; i < src->prs_resource_list_length; ++i) {
    const f1ap_prs_resource_t *r = &src->prs_resource[i];
    if (r->has_qcl_ssb) {
      LOG_E(NR_RRC, "Cannot generate PosSIB: F1 SSB QCL does not identify the LPP QCL type\n");
      return false;
    }
    if ((r->has_qcl_prs && (!r->has_qcl_prs_resource_id || r->qcl_prs_set_id != src->prs_resource_set_id)) ||
        r->re_offset >= src->comb_size || r->resource_symbol_offset + src->resource_number_of_symbols > 14)
      return false;
    if (r->has_qcl_prs) {
      bool found = false;
      for (int j = 0; j < src->prs_resource_list_length; ++j)
        found |= src->prs_resource[j].prs_resource_id == r->qcl_prs_resource_id && j != i;
      if (!found)
        return false;
    }
    for (int j = 0; j < i; ++j)
      if (r->prs_resource_id == src->prs_resource[j].prs_resource_id)
        return false;
  }
  LPP_NR_DL_PRS_ResourceSet_r16_t *set = calloc_or_fail(1, sizeof(*set));
  set->nr_DL_PRS_ResourceSetID_r16 = src->prs_resource_set_id;
  lpp_prs_muting_config_t muting = {0};
  muting.MutingBitRepetition = src->muting1_repetition;
  for (int i = 0; i < src->muting1_length; ++i)
    muting.MutingPattern1[i] = (src->muting1_bits[i / 8] >> (7 - i % 8)) & 1;
  for (int i = 0; i < src->muting2_length; ++i)
    muting.MutingPattern2[i] = (src->muting2_bits[i / 8] >> (7 - i % 8)) & 1;
  if (!set_periodicity(&set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16, src->subcarrier_spacing,
                       src->resource_set_periodicity, src->resource_set_slot_offset) ||
      !map_repetition(src->resource_repetition_factor, &set->dl_PRS_ResourceRepetitionFactor_r16) ||
      !map_time_gap(src->resource_repetition_factor, src->resource_time_gap, &set->dl_PRS_ResourceTimeGap_r16) ||
      !map_num_symbols(src->resource_number_of_symbols, &set->dl_PRS_NumSymbols_r16) ||
      !add_muting(set, &muting, src->muting1_length, src->muting2_length)) {
    ASN_STRUCT_FREE(asn_DEF_LPP_NR_DL_PRS_ResourceSet_r16, set);
    return false;
  }
  set->dl_PRS_ResourcePower_r16 = src->prs_resource_transmit_power;
  for (int i = 0; i < src->prs_resource_list_length; ++i) {
    const f1ap_prs_resource_t *r = &src->prs_resource[i];
    LPP_NR_DL_PRS_Resource_r16_t *resource = calloc_or_fail(1, sizeof(*resource));
    resource->nr_DL_PRS_ResourceID_r16 = r->prs_resource_id;
    resource->dl_PRS_SequenceID_r16 = r->sequence_id;
    resource->dl_PRS_CombSizeN_AndReOffset_r16.present = resource_present;
    switch (src->comb_size) {
      case 2: resource->dl_PRS_CombSizeN_AndReOffset_r16.choice.n2_r16 = r->re_offset; break;
      case 4: resource->dl_PRS_CombSizeN_AndReOffset_r16.choice.n4_r16 = r->re_offset; break;
      case 6: resource->dl_PRS_CombSizeN_AndReOffset_r16.choice.n6_r16 = r->re_offset; break;
      case 12: resource->dl_PRS_CombSizeN_AndReOffset_r16.choice.n12_r16 = r->re_offset; break;
    }
    resource->dl_PRS_ResourceSlotOffset_r16 = r->resource_slot_offset;
    resource->dl_PRS_ResourceSymbolOffset_r16 = r->resource_symbol_offset;
    if (r->has_qcl_prs) {
      resource->dl_PRS_QCL_Info_r16 = calloc_or_fail(1, sizeof(*resource->dl_PRS_QCL_Info_r16));
      resource->dl_PRS_QCL_Info_r16->present = LPP_DL_PRS_QCL_Info_r16_PR_dl_PRS_r16;
      resource->dl_PRS_QCL_Info_r16->choice.dl_PRS_r16 =
          calloc_or_fail(1, sizeof(*resource->dl_PRS_QCL_Info_r16->choice.dl_PRS_r16));
      resource->dl_PRS_QCL_Info_r16->choice.dl_PRS_r16->qcl_DL_PRS_ResourceSetID_r16 = r->qcl_prs_set_id;
      resource->dl_PRS_QCL_Info_r16->choice.dl_PRS_r16->qcl_DL_PRS_ResourceID_r16 = r->qcl_prs_resource_id;
    }
    asn1cSeqAdd(&set->dl_PRS_ResourceList_r16.list, resource);
  }
  LPP_NR_DL_PRS_AssistanceDataPerTRP_r16_t *trp = calloc_or_fail(1, sizeof(*trp));
  trp->dl_PRS_ID_r16 = LPP_PRS_DL_PRS_ID;
  asn1cCallocOne(trp->nr_PhysCellID_r16, pci);
  trp->nr_CellGlobalID_r16 = calloc_or_fail(1, sizeof(*trp->nr_CellGlobalID_r16));
  add_decimal_digits(&trp->nr_CellGlobalID_r16->mcc_r15.list, cgi->plmn.mcc, 3);
  add_decimal_digits(&trp->nr_CellGlobalID_r16->mnc_r15.list, cgi->plmn.mnc, cgi->plmn.mnc_digit_length);
  set_bit_string36(&trp->nr_CellGlobalID_r16->nr_cellidentity_r15, cgi->nr_cellid);
  asn1cSeqAdd(&trp->nr_DL_PRS_Info_r16.nr_DL_PRS_ResourceSetList_r16.list, set);
  LPP_NR_DL_PRS_AssistanceDataPerFreq_r16_t *frequency = calloc_or_fail(1, sizeof(*frequency));
  frequency->nr_DL_PRS_PositioningFrequencyLayer_r16.dl_PRS_SubcarrierSpacing_r16 = src->subcarrier_spacing;
  frequency->nr_DL_PRS_PositioningFrequencyLayer_r16.dl_PRS_ResourceBandwidth_r16 = bandwidth;
  frequency->nr_DL_PRS_PositioningFrequencyLayer_r16.dl_PRS_StartPRB_r16 = src->start_prb;
  frequency->nr_DL_PRS_PositioningFrequencyLayer_r16.dl_PRS_PointA_r16 = src->point_a;
  frequency->nr_DL_PRS_PositioningFrequencyLayer_r16.dl_PRS_CombSizeN_r16 = frequency_comb;
  frequency->nr_DL_PRS_PositioningFrequencyLayer_r16.dl_PRS_CyclicPrefix_r16 = src->cp_type;
  asn1cSeqAdd(&frequency->nr_DL_PRS_AssistanceDataPerFreq_r16.list, trp);
  LPP_NR_DL_PRS_AssistanceData_r16_t *assistance = calloc_or_fail(1, sizeof(*assistance));
  assistance->nr_DL_PRS_ReferenceInfo_r16.dl_PRS_ID_r16 = LPP_PRS_DL_PRS_ID;
  asn1cCallocOne(assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceSetID_r16, src->prs_resource_set_id);
  assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceID_List_r16 =
      calloc_or_fail(1, sizeof(*assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceID_List_r16));
  for (int i = 0; i < src->prs_resource_list_length; ++i) {
    long *resource_id;
    asn1cCallocOne(resource_id, src->prs_resource[i].prs_resource_id);
    asn1cSeqAdd(&assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceID_List_r16->list, resource_id);
  }
  asn1cSeqAdd(&assistance->nr_DL_PRS_AssistanceDataList_r16.list, frequency);
  LPP_AssistanceDataSIBelement_r15_t element = {0};
  bool ok = check_asn1_constraints(&asn_DEF_LPP_NR_DL_PRS_AssistanceData_r16, assistance, "NR-DL-PRS-AssistanceData-r16") &&
            encode_uper_to_octet_string(&asn_DEF_LPP_NR_DL_PRS_AssistanceData_r16, assistance,
                                        &element.assistanceDataElement_r15, "NR-DL-PRS-AssistanceData-r16") &&
            check_asn1_constraints(&asn_DEF_LPP_AssistanceDataSIBelement_r15, &element, "AssistanceDataSIBelement-r15");
  OCTET_STRING_t encoded = {0};
  ok = ok && encode_uper_to_octet_string(&asn_DEF_LPP_AssistanceDataSIBelement_r15,
                                          &element, &encoded, "AssistanceDataSIBelement-r15");
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_LPP_AssistanceDataSIBelement_r15, &element);
  ASN_STRUCT_FREE(asn_DEF_LPP_NR_DL_PRS_AssistanceData_r16, assistance);
  if (!ok) {
    free(encoded.buf);
    return false;
  }
  *segment = encoded.buf;
  *segment_len = encoded.size;
  return true;
}

NR_BCCH_DL_SCH_Message_t *create_rrc_possib_from_segment(const uint8_t *segment, size_t length)
{
  if (!segment || !length || length > INT_MAX)
    return NULL;
  NR_SIBpos_r16_t *sibpos = calloc_or_fail(1, sizeof(*sibpos));
  if (OCTET_STRING_fromBuf(&sibpos->assistanceDataSIB_Element_r16, (const char *)segment, length) != 0) {
    ASN_STRUCT_FREE(asn_DEF_NR_SIBpos_r16, sibpos);
    return NULL;
  }

  PosSystemInformation_r16_IEs__posSIB_TypeAndInfo_r16__Member *member = calloc_or_fail(1, sizeof(*member));
  member->present = NR_PosSystemInformation_r16_IEs__posSIB_TypeAndInfo_r16__Member_PR_posSib6_1_r16;
  member->choice.posSib6_1_r16 = sibpos;

  NR_PosSystemInformation_r16_IEs_t *pos_information = calloc_or_fail(1, sizeof(*pos_information));
  asn1cSeqAdd(&pos_information->posSIB_TypeAndInfo_r16.list, member);

  NR_SystemInformation_t *system_information = calloc_or_fail(1, sizeof(*system_information));
  system_information->criticalExtensions.present =
      NR_SystemInformation__criticalExtensions_PR_criticalExtensionsFuture_r16;
  system_information->criticalExtensions.choice.criticalExtensionsFuture_r16 =
      calloc_or_fail(1, sizeof(*system_information->criticalExtensions.choice.criticalExtensionsFuture_r16));
  system_information->criticalExtensions.choice.criticalExtensionsFuture_r16->present =
      NR_SystemInformation__criticalExtensions__criticalExtensionsFuture_r16_PR_posSystemInformation_r16;
  system_information->criticalExtensions.choice.criticalExtensionsFuture_r16->choice.posSystemInformation_r16 =
      pos_information;

  NR_BCCH_DL_SCH_Message_t *message = calloc_or_fail(1, sizeof(*message));
  message->message.present = NR_BCCH_DL_SCH_MessageType_PR_c1;
  message->message.choice.c1 = calloc_or_fail(1, sizeof(*message->message.choice.c1));
  message->message.choice.c1->present = NR_BCCH_DL_SCH_MessageType__c1_PR_systemInformation;
  message->message.choice.c1->choice.systemInformation = system_information;
  return message;
}
