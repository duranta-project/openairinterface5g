/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "lpp_prs_ue.h"

#include <stddef.h>
#include <string.h>

#include "LPP_DL-PRS-MutingOption1-r16.h"
#include "LPP_DL-PRS-MutingOption2-r16.h"
#include "LPP_NR-DL-PRS-AssistanceData-r16.h"
#include "LPP_NR-DL-PRS-AssistanceDataPerFreq-r16.h"
#include "LPP_NR-DL-PRS-AssistanceDataPerTRP-r16.h"
#include "LPP_NR-DL-PRS-Periodicity-and-ResourceSetSlotOffset-r16.h"
#include "LPP_NR-DL-PRS-PositioningFrequencyLayer-r16.h"
#include "LPP_NR-DL-PRS-Resource-r16.h"
#include "LPP_NR-DL-PRS-ResourceSet-r16.h"
#include "LPP_NR-MutingPattern-r16.h"
#include "common/utils/LOG/log.h"

static bool map_comb_size(long encoded, uint8_t *comb_size)
{
  static const uint8_t values[] = {2, 4, 6, 12};
  if (encoded < 0 || encoded >= (long)sizeofArray(values))
    return false;
  *comb_size = values[encoded];
  return true;
}

static bool map_repetition(const long *encoded, uint8_t *repetition)
{
  static const uint8_t values[] = {2, 4, 6, 8, 16, 32};
  if (encoded == NULL) {
    *repetition = 1;
    return true;
  }
  if (*encoded < 0 || *encoded >= (long)sizeofArray(values))
    return false;
  *repetition = values[*encoded];
  return true;
}

static bool map_time_gap(const long *encoded, uint8_t *time_gap)
{
  static const uint8_t values[] = {1, 2, 4, 8, 16, 32};
  if (encoded == NULL) {
    *time_gap = 1;
    return true;
  }
  if (*encoded < 0 || *encoded >= (long)sizeofArray(values))
    return false;
  *time_gap = values[*encoded];
  return true;
}

static bool map_num_symbols(long encoded, uint8_t *num_symbols)
{
  static const uint8_t values[] = {2, 4, 6, 12};
  if (encoded < 0 || encoded >= (long)sizeofArray(values))
    return false;
  *num_symbols = values[encoded];
  return true;
}

static bool map_muting_pattern(const LPP_NR_MutingPattern_r16_t *pattern,
                               uint32_t values[NR_MAX_PRS_MUTING_PATTERN_LENGTH],
                               uint8_t *length)
{
  const BIT_STRING_t *bits = NULL;
  switch (pattern->present) {
    case LPP_NR_MutingPattern_r16_PR_po2_r16: bits = &pattern->choice.po2_r16; *length = 2; break;
    case LPP_NR_MutingPattern_r16_PR_po4_r16: bits = &pattern->choice.po4_r16; *length = 4; break;
    case LPP_NR_MutingPattern_r16_PR_po6_r16: bits = &pattern->choice.po6_r16; *length = 6; break;
    case LPP_NR_MutingPattern_r16_PR_po8_r16: bits = &pattern->choice.po8_r16; *length = 8; break;
    case LPP_NR_MutingPattern_r16_PR_po16_r16: bits = &pattern->choice.po16_r16; *length = 16; break;
    case LPP_NR_MutingPattern_r16_PR_po32_r16: bits = &pattern->choice.po32_r16; *length = 32; break;
    default: return false;
  }
  if (bits->buf == NULL || bits->size * 8 - bits->bits_unused != *length)
    return false;
  for (int i = 0; i < *length; ++i)
    values[i] = (bits->buf[i / 8] >> (7 - i % 8)) & 1;
  return true;
}

static bool map_muting(const LPP_NR_DL_PRS_ResourceSet_r16_t *set, nr_ue_prs_resource_config_t *resource)
{
  resource->muting_bit_repetition = 1;
  if (set->dl_PRS_MutingOption1_r16 != NULL) {
    const LPP_DL_PRS_MutingOption1_r16_t *option1 = set->dl_PRS_MutingOption1_r16;
    if (!map_muting_pattern(&option1->nr_option1_muting_r16, resource->muting_pattern1, &resource->muting_pattern1_length))
      return false;
    if (option1->dl_prs_MutingBitRepetitionFactor_r16 != NULL) {
      static const uint8_t values[] = {1, 2, 4, 8};
      const long factor = *option1->dl_prs_MutingBitRepetitionFactor_r16;
      if (factor < 0 || factor >= (long)sizeofArray(values))
        return false;
      resource->muting_bit_repetition = values[factor];
    }
  }
  if (set->dl_PRS_MutingOption2_r16 != NULL
      && !map_muting_pattern(&set->dl_PRS_MutingOption2_r16->nr_option2_muting_r16,
                             resource->muting_pattern2,
                             &resource->muting_pattern2_length))
    return false;
  return true;
}

#define READ_PERIOD(INNER, TABLE, PERIOD, OFFSET) do { \
  const int index = (INNER)->present - 1; \
  if (index < 0 || index >= (int)sizeofArray(TABLE)) return false; \
  (PERIOD) = (TABLE)[index]; \
  memcpy(&(OFFSET), &(INNER)->choice, sizeof(OFFSET)); \
} while (0)

static bool map_periodicity(const LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16_t *asn_period,
                            uint8_t *scs,
                            uint16_t *period,
                            uint16_t *offset)
{
  static const uint16_t scs15[] = {4, 5, 8, 10, 16, 20, 32, 40, 64, 80, 160, 320, 640, 1280, 2560, 5120, 10240};
  static const uint16_t scs30[] = {8, 10, 16, 20, 32, 40, 64, 80, 128, 160, 320, 640, 1280, 2560, 5120, 10240, 20480};
  static const uint16_t scs60[] = {16, 20, 32, 40, 64, 80, 128, 160, 256, 320, 640, 1280, 2560, 5120, 10240, 20480, 40960};
  static const uint16_t scs120[] = {32, 40, 64, 80, 128, 160, 256, 320, 512, 640, 1280, 2560, 5120, 10240, 20480, 40960, 81920};
  switch (asn_period->present) {
    case LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16_PR_scs15_r16:
      if (asn_period->choice.scs15_r16 == NULL) return false;
      *scs = 0; READ_PERIOD(asn_period->choice.scs15_r16, scs15, *period, *offset); break;
    case LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16_PR_scs30_r16:
      if (asn_period->choice.scs30_r16 == NULL) return false;
      *scs = 1; READ_PERIOD(asn_period->choice.scs30_r16, scs30, *period, *offset); break;
    case LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16_PR_scs60_r16:
      if (asn_period->choice.scs60_r16 == NULL) return false;
      *scs = 2; READ_PERIOD(asn_period->choice.scs60_r16, scs60, *period, *offset); break;
    case LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16_PR_scs120_r16:
      if (asn_period->choice.scs120_r16 == NULL) return false;
      *scs = 3; READ_PERIOD(asn_period->choice.scs120_r16, scs120, *period, *offset); break;
    default: return false;
  }
  return *offset < *period;
}

static bool convert_resource_set(const LPP_NR_DL_PRS_AssistanceDataPerFreq_r16_t *frequency,
                                 const LPP_NR_DL_PRS_AssistanceDataPerTRP_r16_t *trp,
                                 const LPP_NR_DL_PRS_ResourceSet_r16_t *set,
                                 nr_ue_prs_target_config_t *target)
{
  const LPP_NR_DL_PRS_PositioningFrequencyLayer_r16_t *layer = &frequency->nr_DL_PRS_PositioningFrequencyLayer_r16;
  if (layer->dl_PRS_ResourceBandwidth_r16 < 1 || layer->dl_PRS_ResourceBandwidth_r16 > 63
      || layer->dl_PRS_StartPRB_r16 < 0 || layer->dl_PRS_StartPRB_r16 > UINT16_MAX
      || layer->dl_PRS_CyclicPrefix_r16 != LPP_NR_DL_PRS_PositioningFrequencyLayer_r16__dl_PRS_CyclicPrefix_r16_normal)
    return false;

  memset(target, 0, sizeof(*target));
  target->dl_prs_id = trp->dl_PRS_ID_r16;
  target->resource_set_id = set->nr_DL_PRS_ResourceSetID_r16;
  target->phys_cell_id = trp->nr_PhysCellID_r16 ? *trp->nr_PhysCellID_r16 : 0;
  target->point_a_arfcn = layer->dl_PRS_PointA_r16;
  target->trp_arfcn = trp->nr_ARFCN_r16 ? *trp->nr_ARFCN_r16 : layer->dl_PRS_PointA_r16;
  target->normal_cyclic_prefix = true;
  if (!map_periodicity(&set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16,
                       &target->subcarrier_spacing,
                       &target->resources[0].resource_set_period,
                       &target->resources[0].resource_set_offset)
      || target->subcarrier_spacing != layer->dl_PRS_SubcarrierSpacing_r16
      || !map_comb_size(layer->dl_PRS_CombSizeN_r16, &target->resources[0].comb_size)
      || !map_repetition(set->dl_PRS_ResourceRepetitionFactor_r16, &target->resources[0].resource_repetition)
      || !map_time_gap(set->dl_PRS_ResourceTimeGap_r16, &target->resources[0].resource_time_gap)
      || !map_num_symbols(set->dl_PRS_NumSymbols_r16, &target->resources[0].num_symbols))
    return false;

  const int count = set->dl_PRS_ResourceList_r16.list.count;
  if (count < 1 || count > NR_MAX_PRS_RESOURCES_PER_TARGET)
    return false;
  target->num_resources = count;
  for (int i = 0; i < count; ++i) {
    const LPP_NR_DL_PRS_Resource_r16_t *asn_resource = set->dl_PRS_ResourceList_r16.list.array[i];
    nr_ue_prs_resource_config_t *resource = &target->resources[i];
    *resource = target->resources[0];
    resource->num_rbs = 4 * layer->dl_PRS_ResourceBandwidth_r16 + 20;
    resource->rb_offset = layer->dl_PRS_StartPRB_r16;
    resource->resource_id = asn_resource->nr_DL_PRS_ResourceID_r16;
    resource->nprs_id = asn_resource->dl_PRS_SequenceID_r16;
    resource->resource_offset = asn_resource->dl_PRS_ResourceSlotOffset_r16;
    resource->symbol_start = asn_resource->dl_PRS_ResourceSymbolOffset_r16;
    switch (asn_resource->dl_PRS_CombSizeN_AndReOffset_r16.present) {
      case LPP_NR_DL_PRS_Resource_r16__dl_PRS_CombSizeN_AndReOffset_r16_PR_n2_r16: resource->re_offset = asn_resource->dl_PRS_CombSizeN_AndReOffset_r16.choice.n2_r16; break;
      case LPP_NR_DL_PRS_Resource_r16__dl_PRS_CombSizeN_AndReOffset_r16_PR_n4_r16: resource->re_offset = asn_resource->dl_PRS_CombSizeN_AndReOffset_r16.choice.n4_r16; break;
      case LPP_NR_DL_PRS_Resource_r16__dl_PRS_CombSizeN_AndReOffset_r16_PR_n6_r16: resource->re_offset = asn_resource->dl_PRS_CombSizeN_AndReOffset_r16.choice.n6_r16; break;
      case LPP_NR_DL_PRS_Resource_r16__dl_PRS_CombSizeN_AndReOffset_r16_PR_n12_r16: resource->re_offset = asn_resource->dl_PRS_CombSizeN_AndReOffset_r16.choice.n12_r16; break;
      default: return false;
    }
    if (resource->nprs_id > 4095 || resource->resource_offset > 511 || resource->symbol_start + resource->num_symbols > 14
        || resource->re_offset >= resource->comb_size || !map_muting(set, resource))
      return false;
  }
  return true;
}

bool lpp_nr_prs_assistance_to_configuration(const LPP_NR_DL_PRS_AssistanceData_r16_t *assistance,
                                            nr_prs_config_source_t source,
                                            nr_ue_prs_configuration_t *configuration)
{
  if (assistance == NULL || configuration == NULL)
    return false;
  memset(configuration, 0, sizeof(*configuration));
  configuration->source = source;
  for (int f = 0; f < assistance->nr_DL_PRS_AssistanceDataList_r16.list.count; ++f) {
    const LPP_NR_DL_PRS_AssistanceDataPerFreq_r16_t *frequency = assistance->nr_DL_PRS_AssistanceDataList_r16.list.array[f];
    for (int t = 0; t < frequency->nr_DL_PRS_AssistanceDataPerFreq_r16.list.count; ++t) {
      const LPP_NR_DL_PRS_AssistanceDataPerTRP_r16_t *trp = frequency->nr_DL_PRS_AssistanceDataPerFreq_r16.list.array[t];
      for (int s = 0; s < trp->nr_DL_PRS_Info_r16.nr_DL_PRS_ResourceSetList_r16.list.count; ++s) {
        if (configuration->num_targets == NR_MAX_PRS_TARGETS
            || !convert_resource_set(frequency,
                                     trp,
                                     trp->nr_DL_PRS_Info_r16.nr_DL_PRS_ResourceSetList_r16.list.array[s],
                                     &configuration->targets[configuration->num_targets])) {
          LOG_E(NR_RRC, "Unsupported or invalid NR-DL-PRS assistance data\n");
          return false;
        }
        ++configuration->num_targets;
      }
    }
  }
  return configuration->num_targets > 0;
}
