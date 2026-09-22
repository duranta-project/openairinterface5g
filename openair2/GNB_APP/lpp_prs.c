/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "lpp_prs.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "LPP_DL-PRS-MutingOption1-r16.h"
#include "LPP_DL-PRS-MutingOption2-r16.h"
#include "LPP_DL-PRS-QCL-Info-r16.h"
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
#include "PHY/defs_gNB.h"
#include "NR_MAC_gNB/mac_proto.h"
#include "NR_MAC_gNB/nr_mac_gNB.h"
#include "asn_SEQUENCE_OF.h"
#include "constraints.h"
#include "common/utils/LOG/log.h"
#include "xer_encoder.h"

#define LPP_PRS_RESOURCE_SET_ID 0
#define LPP_PRS_FREQUENCY_LAYER_INDEX 0
#define LPP_PRS_TRP_INDEX 0
#define LPP_PRS_DL_PRS_ID 0

static void *lpp_calloc(size_t size)
{
  void *ptr = calloc(1, size);
  AssertFatal(ptr != NULL, "could not allocate LPP PRS assistance data\n");
  return ptr;
}

static long *new_long(long value)
{
  long *ptr = lpp_calloc(sizeof(*ptr));
  *ptr = value;
  return ptr;
}

static void add_to_sequence(void *list, void *element)
{
  AssertFatal(ASN_SEQUENCE_ADD(list, element) == 0, "could not extend LPP PRS ASN.1 sequence\n");
}

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
  *encoded = new_long(value);
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
  *encoded = new_long(value);
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
      asn_period->choice.scs15_r16 = lpp_calloc(sizeof(*asn_period->choice.scs15_r16));
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
      asn_period->choice.scs30_r16 = lpp_calloc(sizeof(*asn_period->choice.scs30_r16));
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
      asn_period->choice.scs60_r16 = lpp_calloc(sizeof(*asn_period->choice.scs60_r16));
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
      asn_period->choice.scs120_r16 = lpp_calloc(sizeof(*asn_period->choice.scs120_r16));
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

static bool validate_prs_config(const NR_gNB_PRS *prs, const NR_ServingCellConfigCommon_t *scc, long *bandwidth)
{
  if (prs->NumPRSResources == 0)
    return false;
  if (scc == NULL || scc->physCellId == NULL || scc->ssb_PositionsInBurst == NULL
      || scc->ssbSubcarrierSpacing == NULL || scc->ssb_periodicityServingCell == NULL
      || scc->downlinkConfigCommon == NULL || scc->downlinkConfigCommon->frequencyInfoDL == NULL
      || scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencySSB == NULL
      || scc->downlinkConfigCommon->frequencyInfoDL->scs_SpecificCarrierList.list.count == 0) {
    LOG_E(GNB_APP, "Cannot generate NR-DL-TDOA assistance data: serving-cell frequency configuration is incomplete\n");
    return false;
  }

  const prs_config_t *first = &prs->prs_cfg[0];
  if (!map_resource_bandwidth(first->NumRB, bandwidth))
    return false;
  if (first->RBOffset > 2176) {
    LOG_E(GNB_APP, "Cannot generate NR-DL-TDOA assistance data: PRS RBOffset %u exceeds 2176\n", first->RBOffset);
    return false;
  }

  for (int i = 0; i < prs->NumPRSResources; ++i) {
    const prs_config_t *cfg = &prs->prs_cfg[i];
    if (cfg->NumRB != first->NumRB || cfg->RBOffset != first->RBOffset || cfg->CombSize != first->CombSize
        || cfg->PRSResourceSetPeriod[0] != first->PRSResourceSetPeriod[0]
        || cfg->PRSResourceSetPeriod[1] != first->PRSResourceSetPeriod[1]
        || cfg->PRSResourceRepetition != first->PRSResourceRepetition
        || cfg->PRSResourceTimeGap != first->PRSResourceTimeGap || cfg->NumPRSSymbols != first->NumPRSSymbols) {
      LOG_E(GNB_APP,
            "Cannot generate NR-DL-TDOA assistance data: PRS resource %d has values that differ from resource-set values\n",
            i);
      return false;
    }
    if (cfg->NPRSID > 4095 || cfg->REOffset >= cfg->CombSize || cfg->PRSResourceOffset > 511
        || cfg->SymbolStart > 12 || cfg->SymbolStart + cfg->NumPRSSymbols > 14) {
      LOG_E(GNB_APP, "Cannot generate NR-DL-TDOA assistance data: PRS resource %d contains an invalid ASN.1 value\n", i);
      return false;
    }
  }
  return true;
}

static void set_bit_string36(BIT_STRING_t *bit_string, uint64_t value)
{
  bit_string->buf = lpp_calloc(5);
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
    add_to_sequence(list, new_long(value / divisor % 10));
    divisor /= 10;
  }
}

static LPP_NCGI_r15_t *create_ncgi(const f1ap_served_cell_info_t *cell)
{
  LPP_NCGI_r15_t *ncgi = lpp_calloc(sizeof(*ncgi));
  add_decimal_digits(&ncgi->mcc_r15.list, cell->plmn.mcc, 3);
  add_decimal_digits(&ncgi->mnc_r15.list, cell->plmn.mnc, cell->plmn.mnc_digit_length);
  set_bit_string36(&ncgi->nr_cellidentity_r15, cell->nr_cellid);
  return ncgi;
}

static int first_active_ssb(const NR_ServingCellConfigCommon_t *scc)
{
  for (int i = 0; i < 64; ++i) {
    if (is_ssb_configured(scc, i))
      return i;
  }
  return -1;
}

static LPP_DL_PRS_QCL_Info_r16_t *create_qcl(const NR_ServingCellConfigCommon_t *scc, int ssb_index)
{
  if (ssb_index < 0)
    return NULL;
  LPP_DL_PRS_QCL_Info_r16_t *qcl = lpp_calloc(sizeof(*qcl));
  qcl->present = LPP_DL_PRS_QCL_Info_r16_PR_ssb_r16;
  qcl->choice.ssb_r16 = lpp_calloc(sizeof(*qcl->choice.ssb_r16));
  qcl->choice.ssb_r16->pci_r16 = *scc->physCellId;
  qcl->choice.ssb_r16->ssb_Index_r16 = ssb_index;
  qcl->choice.ssb_r16->rs_Type_r16 = LPP_DL_PRS_QCL_Info_r16__ssb_r16__rs_Type_r16_typeD;
  return qcl;
}

static LPP_NR_DL_PRS_Resource_r16_t *create_resource(const prs_config_t *cfg,
                                                      int resource_id,
                                                      int ssb_index,
                                                      const NR_ServingCellConfigCommon_t *scc)
{
  LPP_NR_DL_PRS_Resource_r16_t *resource = lpp_calloc(sizeof(*resource));
  long unused_comb;
  int resource_present = 0;
  bool mapped = map_comb_size(cfg->CombSize, &unused_comb, &resource_present);
  AssertFatal(mapped, "validated PRS comb size could not be mapped\n");

  resource->nr_DL_PRS_ResourceID_r16 = resource_id;
  resource->dl_PRS_SequenceID_r16 = cfg->NPRSID;
  resource->dl_PRS_CombSizeN_AndReOffset_r16.present = resource_present;
  switch (cfg->CombSize) {
    case 2: resource->dl_PRS_CombSizeN_AndReOffset_r16.choice.n2_r16 = cfg->REOffset; break;
    case 4: resource->dl_PRS_CombSizeN_AndReOffset_r16.choice.n4_r16 = cfg->REOffset; break;
    case 6: resource->dl_PRS_CombSizeN_AndReOffset_r16.choice.n6_r16 = cfg->REOffset; break;
    case 12: resource->dl_PRS_CombSizeN_AndReOffset_r16.choice.n12_r16 = cfg->REOffset; break;
    default: AssertFatal(false, "validated PRS comb size is invalid\n");
  }
  resource->dl_PRS_ResourceSlotOffset_r16 = cfg->PRSResourceOffset;
  resource->dl_PRS_ResourceSymbolOffset_r16 = cfg->SymbolStart;
  resource->dl_PRS_QCL_Info_r16 = create_qcl(scc, ssb_index);
  return resource;
}

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
  bits->buf = lpp_calloc(bits->size);
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
                       const prs_config_t *cfg,
                       size_t pattern1_length,
                       size_t pattern2_length)
{
  if (pattern1_length > 0) {
    set->dl_PRS_MutingOption1_r16 = lpp_calloc(sizeof(*set->dl_PRS_MutingOption1_r16));
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
    set->dl_PRS_MutingOption1_r16->dl_prs_MutingBitRepetitionFactor_r16 = new_long(repetition);
    if (!set_muting_pattern(&set->dl_PRS_MutingOption1_r16->nr_option1_muting_r16,
                            cfg->MutingPattern1,
                            pattern1_length))
      return false;
  }
  if (pattern2_length > 0) {
    set->dl_PRS_MutingOption2_r16 = lpp_calloc(sizeof(*set->dl_PRS_MutingOption2_r16));
    if (!set_muting_pattern(&set->dl_PRS_MutingOption2_r16->nr_option2_muting_r16,
                            cfg->MutingPattern2,
                            pattern2_length))
      return false;
  }
  return true;
}

static LPP_NR_DL_PRS_ResourceSet_r16_t *create_resource_set(const NR_gNB_PRS *prs,
                                                            const NR_ServingCellConfigCommon_t *scc,
                                                            long scs,
                                                            int ssb_index,
                                                            size_t pattern1_length,
                                                            size_t pattern2_length)
{
  const prs_config_t *cfg = &prs->prs_cfg[0];
  LPP_NR_DL_PRS_ResourceSet_r16_t *set = lpp_calloc(sizeof(*set));
  set->nr_DL_PRS_ResourceSetID_r16 = LPP_PRS_RESOURCE_SET_ID;
  if (!set_periodicity(&set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16,
                       scs,
                       cfg->PRSResourceSetPeriod[0],
                       cfg->PRSResourceSetPeriod[1])
      || !map_repetition(cfg->PRSResourceRepetition, &set->dl_PRS_ResourceRepetitionFactor_r16)
      || !map_time_gap(cfg->PRSResourceRepetition, cfg->PRSResourceTimeGap, &set->dl_PRS_ResourceTimeGap_r16)
      || !map_num_symbols(cfg->NumPRSSymbols, &set->dl_PRS_NumSymbols_r16)
      || !add_muting(set, cfg, pattern1_length, pattern2_length)) {
    ASN_STRUCT_FREE(asn_DEF_LPP_NR_DL_PRS_ResourceSet_r16, set);
    return NULL;
  }
  set->dl_PRS_ResourcePower_r16 = scc->ss_PBCH_BlockPower;
  for (int i = 0; i < prs->NumPRSResources; ++i)
    add_to_sequence(&set->dl_PRS_ResourceList_r16.list, create_resource(&prs->prs_cfg[i], i, ssb_index, scc));
  return set;
}

static void copy_bit_string(BIT_STRING_t *destination, const BIT_STRING_t *source)
{
  destination->size = source->size;
  destination->bits_unused = source->bits_unused;
  destination->buf = lpp_calloc(source->size);
  memcpy(destination->buf, source->buf, source->size);
}

static LPP_NR_SSB_Config_r16_t *create_ssb_config(const NR_ServingCellConfigCommon_t *scc)
{
  const NR_FrequencyInfoDL_t *dl = scc->downlinkConfigCommon->frequencyInfoDL;
  LPP_NR_SSB_Config_r16_t *ssb = lpp_calloc(sizeof(*ssb));
  ssb->nr_PhysCellID_r16 = *scc->physCellId;
  ssb->nr_ARFCN_r16 = *dl->absoluteFrequencySSB;
  ssb->ss_PBCH_BlockPower_r16 = scc->ss_PBCH_BlockPower;
  ssb->halfFrameIndex_r16 = 0;
  ssb->ssb_periodicity_r16 = *scc->ssb_periodicityServingCell;
  ssb->ssb_SubcarrierSpacing_r16 = *scc->ssbSubcarrierSpacing;
  ssb->sfn_SSB_Offset_r16 = 0;
  ssb->ssb_PositionsInBurst_r16 = lpp_calloc(sizeof(*ssb->ssb_PositionsInBurst_r16));
  switch (scc->ssb_PositionsInBurst->present) {
    case NR_ServingCellConfigCommon__ssb_PositionsInBurst_PR_shortBitmap:
      ssb->ssb_PositionsInBurst_r16->present = LPP_NR_SSB_Config_r16__ssb_PositionsInBurst_r16_PR_shortBitmap_r16;
      copy_bit_string(&ssb->ssb_PositionsInBurst_r16->choice.shortBitmap_r16,
                      &scc->ssb_PositionsInBurst->choice.shortBitmap);
      break;
    case NR_ServingCellConfigCommon__ssb_PositionsInBurst_PR_mediumBitmap:
      ssb->ssb_PositionsInBurst_r16->present = LPP_NR_SSB_Config_r16__ssb_PositionsInBurst_r16_PR_mediumBitmap_r16;
      copy_bit_string(&ssb->ssb_PositionsInBurst_r16->choice.mediumBitmap_r16,
                      &scc->ssb_PositionsInBurst->choice.mediumBitmap);
      break;
    case NR_ServingCellConfigCommon__ssb_PositionsInBurst_PR_longBitmap:
      ssb->ssb_PositionsInBurst_r16->present = LPP_NR_SSB_Config_r16__ssb_PositionsInBurst_r16_PR_longBitmap_r16;
      copy_bit_string(&ssb->ssb_PositionsInBurst_r16->choice.longBitmap_r16,
                      &scc->ssb_PositionsInBurst->choice.longBitmap);
      break;
    default:
      AssertFatal(false, "ServingCellConfigCommon has no SSB position bitmap\n");
  }
  return ssb;
}

static LPP_LPP_Message_t *create_message(const PHY_VARS_gNB *gNB,
                                         const gNB_MAC_INST *mac,
                                         long bandwidth,
                                         size_t pattern1_length,
                                         size_t pattern2_length)
{
  const NR_gNB_PRS *prs = &gNB->prs_vars;
  const NR_ServingCellConfigCommon_t *scc = mac->common_channels[0].ServingCellConfigCommon;
  const NR_FrequencyInfoDL_t *dl = scc->downlinkConfigCommon->frequencyInfoDL;
  const long scs = dl->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing;
  const int ssb_index = first_active_ssb(scc);
  const f1ap_setup_req_t *setup = mac->f1_config.setup_req;
  const f1ap_served_cell_info_t *cell = setup != NULL && setup->num_cells_available > 0 ? &setup->cell[0].info : NULL;

  LPP_NR_DL_PRS_ResourceSet_r16_t *resource_set =
      create_resource_set(prs, scc, scs, ssb_index, pattern1_length, pattern2_length);
  if (resource_set == NULL)
    return NULL;

  LPP_NR_DL_PRS_AssistanceDataPerTRP_r16_t *trp = lpp_calloc(sizeof(*trp));
  trp->dl_PRS_ID_r16 = LPP_PRS_DL_PRS_ID;
  trp->nr_PhysCellID_r16 = new_long(*scc->physCellId);
  trp->nr_CellGlobalID_r16 = cell != NULL ? create_ncgi(cell) : NULL;
  trp->nr_ARFCN_r16 = new_long(dl->absoluteFrequencyPointA);
  trp->nr_DL_PRS_SFN0_Offset_r16.sfn_Offset_r16 = 0;
  trp->nr_DL_PRS_SFN0_Offset_r16.integerSubframeOffset_r16 = 0;
  trp->nr_DL_PRS_ExpectedRSTD_r16 = 0;
  trp->nr_DL_PRS_ExpectedRSTD_Uncertainty_r16 = 0;
  add_to_sequence(&trp->nr_DL_PRS_Info_r16.nr_DL_PRS_ResourceSetList_r16.list, resource_set);

  LPP_NR_DL_PRS_AssistanceDataPerFreq_r16_t *frequency = lpp_calloc(sizeof(*frequency));
  frequency->nr_DL_PRS_PositioningFrequencyLayer_r16.dl_PRS_SubcarrierSpacing_r16 = scs;
  frequency->nr_DL_PRS_PositioningFrequencyLayer_r16.dl_PRS_ResourceBandwidth_r16 = bandwidth;
  frequency->nr_DL_PRS_PositioningFrequencyLayer_r16.dl_PRS_StartPRB_r16 = prs->prs_cfg[0].RBOffset;
  frequency->nr_DL_PRS_PositioningFrequencyLayer_r16.dl_PRS_PointA_r16 = dl->absoluteFrequencyPointA;
  long frequency_comb = 0;
  int unused_present;
  AssertFatal(map_comb_size(prs->prs_cfg[0].CombSize, &frequency_comb, &unused_present), "validated comb size failed\n");
  frequency->nr_DL_PRS_PositioningFrequencyLayer_r16.dl_PRS_CombSizeN_r16 = frequency_comb;
  frequency->nr_DL_PRS_PositioningFrequencyLayer_r16.dl_PRS_CyclicPrefix_r16 =
      LPP_NR_DL_PRS_PositioningFrequencyLayer_r16__dl_PRS_CyclicPrefix_r16_normal;
  add_to_sequence(&frequency->nr_DL_PRS_AssistanceDataPerFreq_r16.list, trp);

  LPP_NR_DL_PRS_AssistanceData_r16_t *assistance = lpp_calloc(sizeof(*assistance));
  assistance->nr_DL_PRS_ReferenceInfo_r16.dl_PRS_ID_r16 = LPP_PRS_DL_PRS_ID;
  assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceSetID_r16 = new_long(LPP_PRS_RESOURCE_SET_ID);
  assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceID_List_r16 =
      lpp_calloc(sizeof(*assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceID_List_r16));
  for (int i = 0; i < prs->NumPRSResources; ++i)
    add_to_sequence(&assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceID_List_r16->list, new_long(i));
  add_to_sequence(&assistance->nr_DL_PRS_AssistanceDataList_r16.list, frequency);
  assistance->nr_SSB_Config_r16 = lpp_calloc(sizeof(*assistance->nr_SSB_Config_r16));
  add_to_sequence(&assistance->nr_SSB_Config_r16->list, create_ssb_config(scc));

  LPP_DL_SelectedPRS_ResourceSetIndex_r16_t *selected_set = lpp_calloc(sizeof(*selected_set));
  selected_set->nr_DL_SelectedPRS_ResourceSetIndex_r16 = LPP_PRS_RESOURCE_SET_ID;
  selected_set->dl_SelectedPRS_ResourceIndexList_r16 = lpp_calloc(sizeof(*selected_set->dl_SelectedPRS_ResourceIndexList_r16));
  for (int i = 0; i < prs->NumPRSResources; ++i) {
    LPP_DL_SelectedPRS_ResourceIndex_r16_t *selected_resource = lpp_calloc(sizeof(*selected_resource));
    selected_resource->nr_DL_SelectedPRS_ResourceIdIndex_r16 = i;
    add_to_sequence(&selected_set->dl_SelectedPRS_ResourceIndexList_r16->list, selected_resource);
  }
  LPP_NR_SelectedDL_PRS_IndexPerTRP_r16_t *selected_trp = lpp_calloc(sizeof(*selected_trp));
  selected_trp->nr_SelectedTRP_Index_r16 = LPP_PRS_TRP_INDEX;
  selected_trp->dl_SelectedPRS_ResourceSetIndexList_r16 =
      lpp_calloc(sizeof(*selected_trp->dl_SelectedPRS_ResourceSetIndexList_r16));
  add_to_sequence(&selected_trp->dl_SelectedPRS_ResourceSetIndexList_r16->list, selected_set);
  LPP_NR_SelectedDL_PRS_PerFreq_r16_t *selected_frequency = lpp_calloc(sizeof(*selected_frequency));
  selected_frequency->nr_SelectedDL_PRS_FrequencyLayerIndex_r16 = LPP_PRS_FREQUENCY_LAYER_INDEX;
  selected_frequency->nr_SelectedDL_PRS_IndexListPerFreq_r16 =
      lpp_calloc(sizeof(*selected_frequency->nr_SelectedDL_PRS_IndexListPerFreq_r16));
  add_to_sequence(&selected_frequency->nr_SelectedDL_PRS_IndexListPerFreq_r16->list, selected_trp);
  LPP_NR_SelectedDL_PRS_IndexList_r16_t *selected = lpp_calloc(sizeof(*selected));
  add_to_sequence(&selected->list, selected_frequency);

  LPP_NR_DL_TDOA_ProvideAssistanceData_r16_t *tdoa = lpp_calloc(sizeof(*tdoa));
  tdoa->nr_DL_PRS_AssistanceData_r16 = assistance;
  tdoa->nr_SelectedDL_PRS_IndexList_r16 = selected;

  LPP_ProvideAssistanceData_r9_IEs_t *provide_r9 = lpp_calloc(sizeof(*provide_r9));
  provide_r9->ext2 = lpp_calloc(sizeof(*provide_r9->ext2));
  provide_r9->ext2->nr_DL_TDOA_ProvideAssistanceData_r16 = tdoa;
  LPP_ProvideAssistanceData_t *provide = lpp_calloc(sizeof(*provide));
  provide->criticalExtensions.present = LPP_ProvideAssistanceData__criticalExtensions_PR_c1;
  provide->criticalExtensions.choice.c1 = lpp_calloc(sizeof(*provide->criticalExtensions.choice.c1));
  provide->criticalExtensions.choice.c1->present =
      LPP_ProvideAssistanceData__criticalExtensions__c1_PR_provideAssistanceData_r9;
  provide->criticalExtensions.choice.c1->choice.provideAssistanceData_r9 = provide_r9;

  LPP_LPP_Message_t *message = lpp_calloc(sizeof(*message));
  message->transactionID = lpp_calloc(sizeof(*message->transactionID));
  message->transactionID->initiator = LPP_Initiator_locationServer;
  message->transactionID->transactionNumber = 0;
  message->endTransaction = false;
  message->lpp_MessageBody = lpp_calloc(sizeof(*message->lpp_MessageBody));
  message->lpp_MessageBody->present = LPP_LPP_MessageBody_PR_c1;
  message->lpp_MessageBody->choice.c1 = lpp_calloc(sizeof(*message->lpp_MessageBody->choice.c1));
  message->lpp_MessageBody->choice.c1->present = LPP_LPP_MessageBody__c1_PR_provideAssistanceData;
  message->lpp_MessageBody->choice.c1->choice.provideAssistanceData = provide;
  return message;
}

void print_lpp_nr_dl_tdoa_assistance_data(const PHY_VARS_gNB *gNB,
                                          const gNB_MAC_INST *mac,
                                          size_t muting_pattern1_length,
                                          size_t muting_pattern2_length)
{
  AssertFatal(gNB != NULL && mac != NULL, "cannot create LPP PRS assistance data without gNB and MAC instances\n");
  const NR_ServingCellConfigCommon_t *scc = mac->common_channels[0].ServingCellConfigCommon;
  long bandwidth;
  if (!validate_prs_config(&gNB->prs_vars, scc, &bandwidth))
    return;

  LPP_LPP_Message_t *message = create_message(gNB, mac, bandwidth, muting_pattern1_length, muting_pattern2_length);
  if (message == NULL)
    return;

  char error[1024] = {0};
  size_t error_length = sizeof(error);
  if (asn_check_constraints(&asn_DEF_LPP_LPP_Message, message, error, &error_length) != 0) {
    LOG_E(GNB_APP, "Cannot generate NR-DL-TDOA assistance data: ASN.1 constraint failed: %s\n", error);
    ASN_STRUCT_FREE(asn_DEF_LPP_LPP_Message, message);
    return;
  }

  LOG_I(GNB_APP, "NR-DL-TDOA-ProvideAssistanceData-r16 generated from gNB PRS configuration:\n");
  xer_fprint(stdout, &asn_DEF_LPP_LPP_Message, message);
  ASN_STRUCT_FREE(asn_DEF_LPP_LPP_Message, message);
}
