/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "f1ap_positioning.h"

#include "f1ap_lib_common.h"
#include "f1ap_lib_includes.h"
#include "f1ap_messages_types.h"

#include "common/utils/assertions.h"
#include "openair3/UTILS/conversions.h"
#include "common/utils/oai_asn1.h"
#include "common/utils/utils.h"
#include "common/utils/ds/byte_array.h"
#include "F1AP_ActiveULBWP.h"
#include "F1AP_GNBRxTxTimeDiffMeas.h"
#include "F1AP_LocationUncertainty.h"
#include "F1AP_MeasuredResultsValue.h"
#include "F1AP_PosMeasurementResult.h"
#include "F1AP_ReferencePoint.h"
#include "F1AP_ResourceSetType.h"
#include "F1AP_ResourceType.h"
#include "F1AP_ResourceTypePos.h"
#include "F1AP_SRSCarrier-List.h"
#include "F1AP_SRSConfig.h"
#include "F1AP_TRPInformation.h"
#include "F1AP_TRPInformationTypeResponseList.h"
#include "F1AP_TRPPositionDefinitionType.h"
#include "F1AP_TRPReferencePointType.h"
#include "F1AP_TimeStamp.h"
#include "F1AP_TimeStampSlotIndex.h"
#include "F1AP_TransmissionComb.h"
#include "F1AP_TransmissionCombPos.h"
#include "F1AP_UL-RTOA-MeasurementItem.h"
#include "F1AP_UplinkChannelBW-PerSCS-List.h"
#include "F1AP_PosSRSResourceID-List.h"
#include "F1AP_ProtocolIE-Container.h"
#include "F1AP_SRSResourceID-List.h"
#include "F1AP_TRPPositionDirectAccuracy.h"

static F1AP_SRSConfig_t encode_srs_config(const f1ap_srs_config_t *in_config)
{
  F1AP_SRSConfig_t out_config = {0};
  // optional: srs_resource_list
  if (in_config->srs_resource_list) {
    f1ap_srs_resource_list_t *srs_resource_list = in_config->srs_resource_list;
    uint32_t srs_resource_list_length = srs_resource_list->srs_resource_list_length;
    asn1cCalloc(out_config.sRSResource_List, f1_sRSResource_List);
    for (int i = 0; i < srs_resource_list_length; i++) {
      asn1cSequenceAdd(f1_sRSResource_List->list, F1AP_SRSResource_t, f1_srs_resource);
      f1ap_srs_resource_t *srs_resource = &srs_resource_list->srs_resource[i];
      f1_srs_resource->sRSResourceID = srs_resource->srs_resource_id;

      switch (srs_resource->nr_of_srs_ports) {
        case F1AP_SRS_NUMBER_OF_PORTS_N1:
          f1_srs_resource->nrofSRS_Ports = F1AP_SRSResource__nrofSRS_Ports_port1;
          break;
        case F1AP_SRS_NUMBER_OF_PORTS_N2:
          f1_srs_resource->nrofSRS_Ports = F1AP_SRSResource__nrofSRS_Ports_ports2;
          break;
        case F1AP_SRS_NUMBER_OF_PORTS_N4:
          f1_srs_resource->nrofSRS_Ports = F1AP_SRSResource__nrofSRS_Ports_ports4;
          break;
        default:
          AssertFatal(false, "illegal number of srs ports %d\n", srs_resource->nr_of_srs_ports);
          break;
      }

      f1ap_transmission_comb_t *srs_res_tx_comb = &srs_resource->transmission_comb;
      switch (srs_res_tx_comb->present) {
        case F1AP_TRANSMISSION_COMB_PR_NOTHING:
          if (!f1_srs_resource->transmissionComb)
            f1_srs_resource->transmissionComb = calloc_or_fail(1, sizeof(*f1_srs_resource->transmissionComb));
          f1_srs_resource->transmissionComb->present = F1AP_TransmissionComb_PR_NOTHING;
          break;
        case F1AP_TRANSMISSION_COMB_PR_N2:
          if (!f1_srs_resource->transmissionComb)
            f1_srs_resource->transmissionComb = calloc_or_fail(1, sizeof(*f1_srs_resource->transmissionComb));
          f1_srs_resource->transmissionComb->present = F1AP_TransmissionComb_PR_n2;
          asn1cCalloc(f1_srs_resource->transmissionComb->choice.n2, f1_n2);
          f1_n2->combOffset_n2 = srs_res_tx_comb->choice.n2.comb_offset_n2;
          f1_n2->cyclicShift_n2 = srs_res_tx_comb->choice.n2.cyclic_shift_n2;
          break;
        case F1AP_TRANSMISSION_COMB_PR_N4:
          if (!f1_srs_resource->transmissionComb)
            f1_srs_resource->transmissionComb = calloc_or_fail(1, sizeof(*f1_srs_resource->transmissionComb));
          f1_srs_resource->transmissionComb->present = F1AP_TransmissionComb_PR_n4;
          asn1cCalloc(f1_srs_resource->transmissionComb->choice.n4, f1_n4);
          f1_n4->combOffset_n4 = srs_res_tx_comb->choice.n4.comb_offset_n4;
          f1_n4->cyclicShift_n4 = srs_res_tx_comb->choice.n4.cyclic_shift_n4;
          break;
        default:
          AssertFatal(false, "illegal transmissionComb %d\n", srs_res_tx_comb->present);
          break;
      }

      f1_srs_resource->startPosition = srs_resource->start_position;

      switch (srs_resource->nr_of_symbols) {
        case F1AP_SRS_NUMBER_OF_SYMBOLS_N1:
          f1_srs_resource->nrofSymbols = F1AP_SRSResource__nrofSymbols_n1;
          break;
        case F1AP_SRS_NUMBER_OF_SYMBOLS_N2:
          f1_srs_resource->nrofSymbols = F1AP_SRSResource__nrofSymbols_n2;
          break;
        case F1AP_SRS_NUMBER_OF_SYMBOLS_N4:
          f1_srs_resource->nrofSymbols = F1AP_SRSResource__nrofSymbols_n4;
          break;
        default:
          AssertFatal(false, "illegal number of symbols %d\n", srs_resource->nr_of_symbols);
          break;
      }

      switch (srs_resource->repetition_factor) {
        case F1AP_SRS_REPETITION_FACTOR_RF1:
          f1_srs_resource->repetitionFactor = F1AP_SRSResource__repetitionFactor_n1;
          break;
        case F1AP_SRS_REPETITION_FACTOR_RF2:
          f1_srs_resource->repetitionFactor = F1AP_SRSResource__repetitionFactor_n2;
          break;
        case F1AP_SRS_REPETITION_FACTOR_RF4:
          f1_srs_resource->repetitionFactor = F1AP_SRSResource__repetitionFactor_n4;
          break;
        default:
          AssertFatal(false, "illegal repetition factor %d\n", srs_resource->repetition_factor);
          break;
      }

      f1_srs_resource->freqDomainPosition = srs_resource->freq_domain_position;
      f1_srs_resource->freqDomainShift = srs_resource->freq_domain_shift;
      f1_srs_resource->c_SRS = srs_resource->c_srs;
      f1_srs_resource->b_SRS = srs_resource->b_srs;
      f1_srs_resource->b_hop = srs_resource->b_hop;

      switch (srs_resource->group_or_sequence_hopping) {
        case F1AP_GROUPORSEQUENCEHOPPING_NOTHING:
          f1_srs_resource->groupOrSequenceHopping = F1AP_SRSResource__groupOrSequenceHopping_neither;
          break;
        case F1AP_GROUPORSEQUENCEHOPPING_GROUPHOPPING:
          f1_srs_resource->groupOrSequenceHopping = F1AP_SRSResource__groupOrSequenceHopping_groupHopping;
          break;
        case F1AP_GROUPORSEQUENCEHOPPING_SEQUENCEHOPPING:
          f1_srs_resource->groupOrSequenceHopping = F1AP_SRSResource__groupOrSequenceHopping_sequenceHopping;
          break;
        default:
          AssertFatal(false, "illegal groupOrSequenceHopping %d\n", srs_resource->group_or_sequence_hopping);
          break;
      }

      F1AP_ResourceType_t *f1_resourceType = (f1_srs_resource->resourceType = calloc_or_fail(1, sizeof(*f1_srs_resource->resourceType)));
      f1ap_resource_type_t *resource_type = &srs_resource->resource_type;
      if (resource_type->present == F1AP_RESOURCE_TYPE_PR_NOTHING) {
        f1_resourceType->present = F1AP_ResourceType_PR_NOTHING;
      } else if (resource_type->present == F1AP_RESOURCE_TYPE_PR_PERIODIC) {
        f1_resourceType->present = F1AP_ResourceType_PR_periodic;
        asn1cCalloc(f1_resourceType->choice.periodic, f1_periodic);
        switch (resource_type->choice.periodic.periodicity) {
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT1:
            f1_periodic->periodicity = F1AP_ResourceTypePeriodic__periodicity_slot1;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT2:
            f1_periodic->periodicity = F1AP_ResourceTypePeriodic__periodicity_slot2;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT4:
            f1_periodic->periodicity = F1AP_ResourceTypePeriodic__periodicity_slot4;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT5:
            f1_periodic->periodicity = F1AP_ResourceTypePeriodic__periodicity_slot5;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT8:
            f1_periodic->periodicity = F1AP_ResourceTypePeriodic__periodicity_slot8;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT10:
            f1_periodic->periodicity = F1AP_ResourceTypePeriodic__periodicity_slot10;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT16:
            f1_periodic->periodicity = F1AP_ResourceTypePeriodic__periodicity_slot16;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT20:
            f1_periodic->periodicity = F1AP_ResourceTypePeriodic__periodicity_slot20;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT32:
            f1_periodic->periodicity = F1AP_ResourceTypePeriodic__periodicity_slot32;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT40:
            f1_periodic->periodicity = F1AP_ResourceTypePeriodic__periodicity_slot40;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT64:
            f1_periodic->periodicity = F1AP_ResourceTypePeriodic__periodicity_slot64;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT80:
            f1_periodic->periodicity = F1AP_ResourceTypePeriodic__periodicity_slot80;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT160:
            f1_periodic->periodicity = F1AP_ResourceTypePeriodic__periodicity_slot160;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT320:
            f1_periodic->periodicity = F1AP_ResourceTypePeriodic__periodicity_slot320;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT640:
            f1_periodic->periodicity = F1AP_ResourceTypePeriodic__periodicity_slot640;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT1280:
            f1_periodic->periodicity = F1AP_ResourceTypePeriodic__periodicity_slot1280;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT2560:
            f1_periodic->periodicity = F1AP_ResourceTypePeriodic__periodicity_slot2560;
            break;
          default:
            AssertFatal(false, "illegal periodicity %d\n", resource_type->choice.periodic.periodicity);
            break;
        }
        f1_periodic->offset = resource_type->choice.periodic.offset;
      } else if (resource_type->present == F1AP_RESOURCE_TYPE_PR_SEMI_PERSISTENT) {
        f1_resourceType->present = F1AP_ResourceType_PR_semi_persistent;
        asn1cCalloc(f1_resourceType->choice.semi_persistent, f1_semi_persistent);
        switch (resource_type->choice.semi_persistent.periodicity) {
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT1:
            f1_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistent__periodicity_slot1;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT2:
            f1_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistent__periodicity_slot2;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT4:
            f1_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistent__periodicity_slot4;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT5:
            f1_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistent__periodicity_slot5;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT8:
            f1_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistent__periodicity_slot8;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT10:
            f1_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistent__periodicity_slot10;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT16:
            f1_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistent__periodicity_slot16;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT20:
            f1_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistent__periodicity_slot20;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT32:
            f1_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistent__periodicity_slot32;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT40:
            f1_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistent__periodicity_slot40;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT64:
            f1_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistent__periodicity_slot64;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT80:
            f1_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistent__periodicity_slot80;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT160:
            f1_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistent__periodicity_slot160;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT320:
            f1_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistent__periodicity_slot320;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT640:
            f1_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistent__periodicity_slot640;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT1280:
            f1_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistent__periodicity_slot1280;
            break;
          case F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT2560:
            f1_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistent__periodicity_slot2560;
            break;
          default:
            AssertFatal(false, "illegal periodicity %d\n", resource_type->choice.semi_persistent.periodicity);
            break;
        }
        f1_semi_persistent->offset = resource_type->choice.semi_persistent.offset;
      } else if (resource_type->present == F1AP_RESOURCE_TYPE_PR_APERIODIC) {
        f1_resourceType->present = F1AP_ResourceType_PR_aperiodic;
        asn1cCalloc(f1_resourceType->choice.aperiodic, f1_aperiodic);
        f1_aperiodic->aperiodicResourceType = resource_type->choice.aperiodic;
      } else {
        AssertFatal(false, "illegal resourceType %d\n", resource_type->present);
      }

      f1_srs_resource->sequenceId = srs_resource->sequence_id;
    }
  }

  // optional: pos_srs_resource_list
  if (in_config->pos_srs_resource_list) {
    f1ap_pos_srs_resource_list_t *pos_srs_resource_list = in_config->pos_srs_resource_list;
    uint32_t pos_srs_resource_list_length = pos_srs_resource_list->pos_srs_resource_list_length;
    asn1cCalloc(out_config.posSRSResource_List, f1_posSRSResource_List);
    for (int i = 0; i < pos_srs_resource_list_length; i++) {
      asn1cSequenceAdd(f1_posSRSResource_List->list, F1AP_PosSRSResource_Item_t, f1_pos_srs_resource);
      f1ap_pos_srs_resource_item_t *pos_srs_resource = &pos_srs_resource_list->pos_srs_resource_item[i];
      f1_pos_srs_resource->srs_PosResourceId = pos_srs_resource->srs_pos_resource_id;

      f1ap_transmission_comb_pos_t *pos_srs_res_tx_comb = &pos_srs_resource->transmission_comb_pos;
      switch (pos_srs_res_tx_comb->present) {
        case F1AP_TRANSMISSION_COMB_POS_PR_NOTHING:
          if (!f1_pos_srs_resource->transmissionCombPos)
            f1_pos_srs_resource->transmissionCombPos = calloc_or_fail(1, sizeof(*f1_pos_srs_resource->transmissionCombPos));
          f1_pos_srs_resource->transmissionCombPos->present = F1AP_TransmissionCombPos_PR_NOTHING;
          break;
        case F1AP_TRANSMISSION_COMB_POS_PR_N2:
          if (!f1_pos_srs_resource->transmissionCombPos)
            f1_pos_srs_resource->transmissionCombPos = calloc_or_fail(1, sizeof(*f1_pos_srs_resource->transmissionCombPos));
          f1_pos_srs_resource->transmissionCombPos->present = F1AP_TransmissionCombPos_PR_n2;
          asn1cCalloc(f1_pos_srs_resource->transmissionCombPos->choice.n2, f1_pos_srs_resource_n2);
          f1ap_transmission_comb_pos_n2_t *pos_srs_resource_n2 = &pos_srs_res_tx_comb->choice.n2;
          f1_pos_srs_resource_n2->combOffset_n2 = pos_srs_resource_n2->comb_offset_n2;
          f1_pos_srs_resource_n2->cyclicShift_n2 = pos_srs_resource_n2->cyclic_shift_n2;
          break;
        case F1AP_TRANSMISSION_COMB_POS_PR_N4:
          if (!f1_pos_srs_resource->transmissionCombPos)
            f1_pos_srs_resource->transmissionCombPos = calloc_or_fail(1, sizeof(*f1_pos_srs_resource->transmissionCombPos));
          f1_pos_srs_resource->transmissionCombPos->present = F1AP_TransmissionCombPos_PR_n4;
          asn1cCalloc(f1_pos_srs_resource->transmissionCombPos->choice.n4, f1_pos_srs_resource_n4);
          f1ap_transmission_comb_pos_n4_t *pos_srs_resource_n4 = &pos_srs_res_tx_comb->choice.n4;
          f1_pos_srs_resource_n4->combOffset_n4 = pos_srs_resource_n4->comb_offset_n4;
          f1_pos_srs_resource_n4->cyclicShift_n4 = pos_srs_resource_n4->cyclic_shift_n4;
          break;
        case F1AP_TRANSMISSION_COMB_POS_PR_N8:
          if (!f1_pos_srs_resource->transmissionCombPos)
            f1_pos_srs_resource->transmissionCombPos = calloc_or_fail(1, sizeof(*f1_pos_srs_resource->transmissionCombPos));
          f1_pos_srs_resource->transmissionCombPos->present = F1AP_TransmissionCombPos_PR_n8;
          asn1cCalloc(f1_pos_srs_resource->transmissionCombPos->choice.n8, f1_pos_srs_resource_n8);
          f1ap_transmission_comb_pos_n8_t *pos_srs_resource_n8 = &pos_srs_res_tx_comb->choice.n8;
          f1_pos_srs_resource_n8->combOffset_n8 = pos_srs_resource_n8->comb_offset_n8;
          f1_pos_srs_resource_n8->cyclicShift_n8 = pos_srs_resource_n8->cyclic_shift_n8;
          break;
        default:
          AssertFatal(false, "illegal transmissionComb %d\n", pos_srs_res_tx_comb->present);
          break;
      }

      f1_pos_srs_resource->startPosition = pos_srs_resource->start_position;

      switch (pos_srs_resource->nr_of_symbols) {
        case F1AP_SRS_RESOURCE_ITEM_NUMBER_OF_SYMBOLS_N1:
          f1_pos_srs_resource->nrofSymbols = F1AP_PosSRSResource_Item__nrofSymbols_n1;
          break;
        case F1AP_SRS_RESOURCE_ITEM_NUMBER_OF_SYMBOLS_N2:
          f1_pos_srs_resource->nrofSymbols = F1AP_PosSRSResource_Item__nrofSymbols_n2;
          break;
        case F1AP_SRS_RESOURCE_ITEM_NUMBER_OF_SYMBOLS_N4:
          f1_pos_srs_resource->nrofSymbols = F1AP_PosSRSResource_Item__nrofSymbols_n4;
          break;
        case F1AP_SRS_RESOURCE_ITEM_NUMBER_OF_SYMBOLS_N8:
          f1_pos_srs_resource->nrofSymbols = F1AP_PosSRSResource_Item__nrofSymbols_n8;
          break;
        case F1AP_SRS_RESOURCE_ITEM_NUMBER_OF_SYMBOLS_N12:
          f1_pos_srs_resource->nrofSymbols = F1AP_PosSRSResource_Item__nrofSymbols_n12;
          break;
        default:
          AssertFatal(false, "illegal number of symbols %d\n", pos_srs_resource->nr_of_symbols);
          break;
      }

      f1_pos_srs_resource->freqDomainShift = pos_srs_resource->freq_domain_shift;
      f1_pos_srs_resource->c_SRS = pos_srs_resource->c_srs;

      switch (pos_srs_resource->group_or_sequence_hopping) {
        case F1AP_GROUPORSEQUENCEHOPPING_NOTHING:
          f1_pos_srs_resource->groupOrSequenceHopping = F1AP_PosSRSResource_Item__groupOrSequenceHopping_neither;
          break;
        case F1AP_GROUPORSEQUENCEHOPPING_GROUPHOPPING:
          f1_pos_srs_resource->groupOrSequenceHopping = F1AP_PosSRSResource_Item__groupOrSequenceHopping_groupHopping;
          break;
        case F1AP_GROUPORSEQUENCEHOPPING_SEQUENCEHOPPING:
          f1_pos_srs_resource->groupOrSequenceHopping = F1AP_PosSRSResource_Item__groupOrSequenceHopping_sequenceHopping;
          break;
        default:
          AssertFatal(false, "illegal groupOrSequenceHopping %d\n", pos_srs_resource->group_or_sequence_hopping);
          break;
      }

      f1ap_resource_type_pos_t *resource_type_pos = &pos_srs_resource->resource_type_pos;
      F1AP_ResourceTypePos_t *f1_resourceTypePos = (f1_pos_srs_resource->resourceTypePos = calloc_or_fail(1, sizeof(*f1_pos_srs_resource->resourceTypePos)));
      if (resource_type_pos->present == F1AP_RESOURCE_TYPE_POS_PR_NOTHING) {
        f1_resourceTypePos->present = F1AP_ResourceTypePos_PR_NOTHING;
      } else if (resource_type_pos->present == F1AP_RESOURCE_TYPE_POS_PR_PERIODIC) {
        f1_resourceTypePos->present = F1AP_ResourceTypePos_PR_periodic;
        asn1cCalloc(f1_resourceTypePos->choice.periodic, f1_pos_periodic);
        switch (resource_type_pos->choice.periodic.periodicity) {
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT1:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot1;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT2:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot2;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT4:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot4;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT5:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot5;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT8:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot8;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT10:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot10;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT16:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot16;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT20:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot20;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT32:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot32;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT40:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot40;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT64:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot64;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT80:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot80;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT160:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot160;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT320:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot320;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT640:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot640;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT1280:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot1280;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT2560:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot2560;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT5120:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot5120;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT10240:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot10240;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT20480:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot20480;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT40960:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot40960;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT81920:
            f1_pos_periodic->periodicity = F1AP_ResourceTypePeriodicPos__periodicity_slot81920;
            break;
          default:
            AssertFatal(false, "illegal periodicity %d\n", resource_type_pos->choice.periodic.periodicity);
            break;
        }

        f1_pos_periodic->offset = resource_type_pos->choice.periodic.offset;
      } else if (resource_type_pos->present == F1AP_RESOURCE_TYPE_POS_PR_SEMI_PERSISTENT) {
        f1_resourceTypePos->present = F1AP_ResourceTypePos_PR_semi_persistent;
        asn1cCalloc(f1_resourceTypePos->choice.semi_persistent, f1_pos_semi_persistent);
        switch (resource_type_pos->choice.semi_persistent.periodicity) {
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT1:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot1;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT2:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot2;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT4:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot4;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT5:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot5;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT8:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot8;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT10:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot10;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT16:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot16;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT20:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot20;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT32:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot32;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT40:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot40;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT64:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot64;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT80:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot80;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT160:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot160;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT320:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot320;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT640:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot640;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT1280:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot1280;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT2560:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot2560;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT5120:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot5120;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT10240:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot10240;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT20480:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot20480;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT40960:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot40960;
            break;
          case F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT81920:
            f1_pos_semi_persistent->periodicity = F1AP_ResourceTypeSemi_persistentPos__periodicity_slot81920;
            break;
          default:
            AssertFatal(false, "illegal periodicity %d\n", resource_type_pos->choice.semi_persistent.periodicity);
            break;
        }
        f1_pos_semi_persistent->offset = resource_type_pos->choice.semi_persistent.offset;
      } else if (resource_type_pos->present == F1AP_RESOURCE_TYPE_POS_PR_APERIODIC) {
        f1_resourceTypePos->present = F1AP_ResourceTypePos_PR_aperiodic;
        asn1cCalloc(f1_resourceTypePos->choice.aperiodic, f1_pos_aperiodic);
        f1_pos_aperiodic->slotOffset = resource_type_pos->choice.aperiodic.slot_offset;
      } else {
        AssertFatal(false, "illegal resourceType %d\n", resource_type_pos->present);
      }

      f1_pos_srs_resource->sequenceId = pos_srs_resource->sequence_id;
    }
  }

  // optional: srs_resource_set_list
  if (in_config->srs_resource_set_list) {
    f1ap_srs_resource_set_list_t *srs_resource_set_list = in_config->srs_resource_set_list;
    uint32_t srs_resource_set_list_length = srs_resource_set_list->srs_resource_set_list_length;
    asn1cCalloc(out_config.sRSResourceSet_List, f1_sRSResourceSet_List);
    for (int i = 0; i < srs_resource_set_list_length; i++) {
      asn1cSequenceAdd(f1_sRSResourceSet_List->list, F1AP_SRSResourceSet_t, f1_srs_resource_set);
      f1ap_srs_resource_set_t *srs_resource_set = &srs_resource_set_list->srs_resource_set[i];
      f1_srs_resource_set->sRSResourceSetID = srs_resource_set->srs_resource_set_id;
      uint8_t srs_resource_id_list_length = srs_resource_set->srs_resource_id_list.srs_resource_id_list_length;
      for (int j = 0; j < srs_resource_id_list_length; j++) {
        if (!f1_srs_resource_set->sRSResourceID_List)
          f1_srs_resource_set->sRSResourceID_List = calloc_or_fail(1, sizeof(*f1_srs_resource_set->sRSResourceID_List));
        asn1cSequenceAdd(f1_srs_resource_set->sRSResourceID_List->list, F1AP_SRSResourceID_t, f1_srs_resource_id);
        *f1_srs_resource_id = srs_resource_set->srs_resource_id_list.srs_resource_id[j];
      }

      F1AP_ResourceSetType_t *f1_resourceSetType = (f1_srs_resource_set->resourceSetType = calloc_or_fail(1, sizeof(*f1_srs_resource_set->resourceSetType)));
      f1ap_resource_set_type_t *resource_set_type = &srs_resource_set->resource_set_type;
      switch (resource_set_type->present) {
        case F1AP_RESOURCE_SET_TYPE_PR_NOTHING:
          f1_resourceSetType->present = F1AP_ResourceSetType_PR_NOTHING;
          break;
        case F1AP_RESOURCE_SET_TYPE_PR_PERIODIC:
          f1_resourceSetType->present = F1AP_ResourceSetType_PR_periodic;
          asn1cCalloc(f1_resourceSetType->choice.periodic, f1_periodic_set_type);
          f1_periodic_set_type->periodicSet = resource_set_type->choice.periodic;
          break;
        case F1AP_RESOURCE_SET_TYPE_PR_SEMI_PERSISTENT:
          f1_resourceSetType->present = F1AP_ResourceSetType_PR_semi_persistent;
          asn1cCalloc(f1_resourceSetType->choice.semi_persistent, f1_semi_persistent_set_type);
          f1_semi_persistent_set_type->semi_persistentSet = resource_set_type->choice.semi_persistent;
          break;
        case F1AP_RESOURCE_SET_TYPE_PR_APERIODIC:
          f1_resourceSetType->present = F1AP_ResourceSetType_PR_aperiodic;
          asn1cCalloc(f1_resourceSetType->choice.aperiodic, f1_aperiodic_set_type);
          f1_aperiodic_set_type->sRSResourceTrigger_List = resource_set_type->choice.aperiodic.srs_resource_trigger;
          f1_aperiodic_set_type->slotoffset = resource_set_type->choice.aperiodic.slot_offset;
          break;
        default:
          AssertFatal(false, "illegal resource set type %d\n", resource_set_type->present);
          break;
      }
    }
  }

  // optional: pos_srs_resource_set_list
  if (in_config->pos_srs_resource_set_list) {
    f1ap_pos_srs_resource_set_list_t *pos_srs_resource_set_list = in_config->pos_srs_resource_set_list;
    uint32_t pos_srs_resource_set_list_length = pos_srs_resource_set_list->pos_srs_resource_set_list_length;
    asn1cCalloc(out_config.posSRSResourceSet_List, f1_posSRSResourceSet_List);
    for (int i = 0; i < pos_srs_resource_set_list_length; i++) {
      asn1cSequenceAdd(f1_posSRSResourceSet_List->list, F1AP_PosSRSResourceSet_Item_t, f1_pos_srs_resource_set);
      f1ap_pos_srs_resource_set_item_t *pos_srs_resource_set = &pos_srs_resource_set_list->pos_srs_resource_set_item[i];
      f1_pos_srs_resource_set->possrsResourceSetID = pos_srs_resource_set->pos_srs_resource_set_id;
      uint8_t pos_srs_resource_id_list_length = pos_srs_resource_set->pos_srs_resource_id_list.pos_srs_resource_id_list_length;
      for (int j = 0; j < pos_srs_resource_id_list_length; j++) {
        if (!f1_pos_srs_resource_set->possRSResourceID_List)
          f1_pos_srs_resource_set->possRSResourceID_List = calloc_or_fail(1, sizeof(*f1_pos_srs_resource_set->possRSResourceID_List));
        asn1cSequenceAdd(f1_pos_srs_resource_set->possRSResourceID_List->list, F1AP_SRSPosResourceID_t, f1_pos_srs_resource_id);
        *f1_pos_srs_resource_id = pos_srs_resource_set->pos_srs_resource_id_list.srs_pos_resource_id[j];
      }

      F1AP_PosResourceSetType_t *f1_posresourceSetType = (f1_pos_srs_resource_set->posresourceSetType = calloc_or_fail(1, sizeof(*f1_pos_srs_resource_set->posresourceSetType)));
      f1ap_pos_resource_set_type_t *pos_resource_set_type = &pos_srs_resource_set->pos_resource_set_type;
      switch (pos_resource_set_type->present) {
        case F1AP_POS_RESOURCE_SET_TYPE_PR_NOTHING:
          f1_posresourceSetType->present = F1AP_PosResourceSetType_PR_NOTHING;
          break;
        case F1AP_POS_RESOURCE_SET_TYPE_PR_PERIODIC:
          f1_posresourceSetType->present = F1AP_PosResourceSetType_PR_periodic;
          asn1cCalloc(f1_posresourceSetType->choice.periodic, f1_pos_periodic_set_type);
          f1_pos_periodic_set_type->posperiodicSet = pos_resource_set_type->choice.periodic;
          break;
        case F1AP_POS_RESOURCE_SET_TYPE_PR_SEMI_PERSISTENT:
          f1_posresourceSetType->present = F1AP_PosResourceSetType_PR_semi_persistent;
          asn1cCalloc(f1_posresourceSetType->choice.semi_persistent, f1_pos_semi_persistent_set_type);
          f1_pos_semi_persistent_set_type->possemi_persistentSet = pos_resource_set_type->choice.semi_persistent;
          break;
        case F1AP_POS_RESOURCE_SET_TYPE_PR_APERIODIC:
          f1_posresourceSetType->present = F1AP_PosResourceSetType_PR_aperiodic;
          asn1cCalloc(f1_posresourceSetType->choice.aperiodic, f1_pos_aperiodic_set_type);
          f1_pos_aperiodic_set_type->sRSResourceTrigger_List = pos_resource_set_type->choice.srs_resource;
          break;
        default:
          AssertFatal(false, "illegal resource set type pos %d\n", pos_resource_set_type->present);
          break;
      }
    }
  }
  return out_config;
}

static F1AP_SRSCarrier_List_Item_t encode_srs_carrier_list_item(const f1ap_srs_carrier_list_item_t *in_item)
{
  F1AP_SRSCarrier_List_Item_t out_item = {0};
  // pointA
  out_item.pointA = in_item->pointA;

  // Uplink Channel BW-PerSCS-List
  const f1ap_uplink_channel_bw_per_scs_list_t *uplink_channel_bw_per_scs_list = &in_item->uplink_channel_bw_per_scs_list;
  F1AP_UplinkChannelBW_PerSCS_List_t *f1_uplink_channel_bw_per_scs_list = (out_item.uplinkChannelBW_PerSCS_List = calloc_or_fail(1, sizeof(*out_item.uplinkChannelBW_PerSCS_List)));

  uint32_t scs_specific_carrier_list_length = uplink_channel_bw_per_scs_list->scs_specific_carrier_list_length;
  for (int i = 0; i < scs_specific_carrier_list_length; i++) {
    asn1cSequenceAdd(f1_uplink_channel_bw_per_scs_list->list, F1AP_SCS_SpecificCarrier_t, f1_scs_specific_carrier);
    f1ap_scs_specific_carrier_t *scs_specific_carrier = &uplink_channel_bw_per_scs_list->scs_specific_carrier[i];
    // offset to carrier
    f1_scs_specific_carrier->offsetToCarrier = scs_specific_carrier->offset_to_carrier;
    // subcarrier spacing
    switch (scs_specific_carrier->subcarrier_spacing) {
      case F1AP_SUBCARRIER_SPACING_15KHZ:
        f1_scs_specific_carrier->subcarrierSpacing = F1AP_SCS_SpecificCarrier__subcarrierSpacing_kHz15;
        break;
      case F1AP_SUBCARRIER_SPACING_30KHZ:
        f1_scs_specific_carrier->subcarrierSpacing = F1AP_SCS_SpecificCarrier__subcarrierSpacing_kHz30;
        break;
      case F1AP_SUBCARRIER_SPACING_60KHZ:
        f1_scs_specific_carrier->subcarrierSpacing = F1AP_SCS_SpecificCarrier__subcarrierSpacing_kHz60;
        break;
      case F1AP_SUBCARRIER_SPACING_120KHZ:
        f1_scs_specific_carrier->subcarrierSpacing = F1AP_SCS_SpecificCarrier__subcarrierSpacing_kHz120;
        break;
      default:
        AssertFatal(false, "illegal subcarrier spacing %d\n", scs_specific_carrier->subcarrier_spacing);
        break;
    }
    // carrier bandwidth
    f1_scs_specific_carrier->carrierBandwidth = scs_specific_carrier->carrier_bandwidth;
  }

  // Active UL BWP
  F1AP_ActiveULBWP_t *f1_active_ul_bwp = (out_item.activeULBWP = calloc_or_fail(1, sizeof(*out_item.activeULBWP)));
  const f1ap_active_ul_bwp_t *active_ul_bwp = &in_item->active_ul_bwp;

  // location and bandwidth
  f1_active_ul_bwp->locationAndBandwidth = active_ul_bwp->location_and_bandwidth;
  // subcarrier spacing
  switch (active_ul_bwp->subcarrier_spacing) {
    case F1AP_SUBCARRIER_SPACING_15KHZ:
      f1_active_ul_bwp->subcarrierSpacing = F1AP_ActiveULBWP__subcarrierSpacing_kHz15;
      break;
    case F1AP_SUBCARRIER_SPACING_30KHZ:
      f1_active_ul_bwp->subcarrierSpacing = F1AP_ActiveULBWP__subcarrierSpacing_kHz30;
      break;
    case F1AP_SUBCARRIER_SPACING_60KHZ:
      f1_active_ul_bwp->subcarrierSpacing = F1AP_ActiveULBWP__subcarrierSpacing_kHz60;
      break;
    case F1AP_SUBCARRIER_SPACING_120KHZ:
      f1_active_ul_bwp->subcarrierSpacing = F1AP_ActiveULBWP__subcarrierSpacing_kHz120;
      break;
    default:
      AssertFatal(false, "illegal subcarrier spacing %d\n", active_ul_bwp->subcarrier_spacing);
      break;
  }

  // cyclic prefix
  if (active_ul_bwp->cyclic_prefix == F1AP_CP_TYPE_NORMAL)
    f1_active_ul_bwp->cyclicPrefix = F1AP_ActiveULBWP__cyclicPrefix_normal;
  else
    f1_active_ul_bwp->cyclicPrefix = F1AP_ActiveULBWP__cyclicPrefix_extended;

  // Tx Direct Current Location
  f1_active_ul_bwp->txDirectCurrentLocation = active_ul_bwp->tx_direct_current_location;

  // SRS Config
  const f1ap_srs_config_t *sRSConfig = &active_ul_bwp->srs_config;
  asn1cCallocOne(f1_active_ul_bwp->sRSConfig, encode_srs_config(sRSConfig));
  return out_item;
}

static F1AP_SRSCarrier_List_t encode_srs_carrier_list(const f1ap_srs_carrier_list_t *in_list)
{
  F1AP_SRSCarrier_List_t out_list = {0};
  uint32_t list_len = in_list->srs_carrier_list_length;
  for (int i = 0; i < list_len; i++) {
    asn1cSequenceAdd(out_list.list, F1AP_SRSCarrier_List_Item_t, out_item);
    f1ap_srs_carrier_list_item_t *in_item = &in_list->srs_carrier_list_item[i];
    *out_item = encode_srs_carrier_list_item(in_item);
  }
  return out_list;
}

static void decode_srs_config(const F1AP_SRSConfig_t *in_config, f1ap_srs_config_t *out_config)
{
  // optional: sRSResource_List
  if (in_config->sRSResource_List) {
    out_config->srs_resource_list = calloc_or_fail(1, sizeof(*out_config->srs_resource_list));
    f1ap_srs_resource_list_t *srs_resource_list = out_config->srs_resource_list;
    uint32_t srs_resource_list_length = in_config->sRSResource_List->list.count;
    srs_resource_list->srs_resource_list_length = srs_resource_list_length;
    srs_resource_list->srs_resource = calloc_or_fail(srs_resource_list_length, sizeof(*srs_resource_list->srs_resource));
    for (int i = 0; i < srs_resource_list_length; i++) {
      f1ap_srs_resource_t *srs_resource = &srs_resource_list->srs_resource[i];
      F1AP_SRSResource_t *f1_srs_resource = in_config->sRSResource_List->list.array[i];
      srs_resource->srs_resource_id = f1_srs_resource->sRSResourceID;
      switch (f1_srs_resource->nrofSRS_Ports) {
        case F1AP_SRSResource__nrofSRS_Ports_port1:
          srs_resource->nr_of_srs_ports = F1AP_SRS_NUMBER_OF_PORTS_N1;
          break;
        case F1AP_SRSResource__nrofSRS_Ports_ports2:
          srs_resource->nr_of_srs_ports = F1AP_SRS_NUMBER_OF_PORTS_N2;
          break;
        case F1AP_SRSResource__nrofSRS_Ports_ports4:
          srs_resource->nr_of_srs_ports = F1AP_SRS_NUMBER_OF_PORTS_N4;
          break;
        default:
          AssertFatal(false, "illegal number of srs ports %ld\n", f1_srs_resource->nrofSRS_Ports);
          break;
      }

      f1ap_transmission_comb_t *srs_tx_comb = &srs_resource->transmission_comb;
      F1AP_TransmissionComb_t *f1_srs_tx_comb = f1_srs_resource->transmissionComb;
      switch (f1_srs_tx_comb->present) {
        case F1AP_TransmissionComb_PR_NOTHING:
          srs_tx_comb->present = F1AP_TRANSMISSION_COMB_PR_NOTHING;
          break;
        case F1AP_TransmissionComb_PR_n2:
          srs_tx_comb->present = F1AP_TRANSMISSION_COMB_PR_N2;
          srs_tx_comb->choice.n2.comb_offset_n2 = f1_srs_tx_comb->choice.n2->combOffset_n2;
          srs_tx_comb->choice.n2.cyclic_shift_n2 = f1_srs_tx_comb->choice.n2->cyclicShift_n2;
          break;
        case F1AP_TransmissionComb_PR_n4:
          srs_tx_comb->present = F1AP_TRANSMISSION_COMB_PR_N4;
          srs_tx_comb->choice.n4.comb_offset_n4 = f1_srs_tx_comb->choice.n4->combOffset_n4;
          srs_tx_comb->choice.n4.cyclic_shift_n4 = f1_srs_tx_comb->choice.n4->cyclicShift_n4;
          break;
        default:
          AssertFatal(false, "illegal transmissionComb %d\n", f1_srs_tx_comb->present);
          break;
      }

      srs_resource->start_position = f1_srs_resource->startPosition;

      switch (f1_srs_resource->nrofSymbols) {
        case F1AP_SRSResource__nrofSymbols_n1:
          srs_resource->nr_of_symbols = F1AP_SRS_NUMBER_OF_SYMBOLS_N1;
          break;
        case F1AP_SRSResource__nrofSymbols_n2:
          srs_resource->nr_of_symbols = F1AP_SRS_NUMBER_OF_SYMBOLS_N2;
          break;
        case F1AP_SRSResource__nrofSymbols_n4:
          srs_resource->nr_of_symbols = F1AP_SRS_NUMBER_OF_SYMBOLS_N4;
          break;
        default:
          AssertFatal(false, "illegal number of symbols %ld\n", f1_srs_resource->nrofSymbols);
          break;
      }

      switch (f1_srs_resource->repetitionFactor) {
        case F1AP_SRSResource__repetitionFactor_n1:
          srs_resource->repetition_factor = F1AP_SRS_REPETITION_FACTOR_RF1;
          break;
        case F1AP_SRSResource__repetitionFactor_n2:
          srs_resource->repetition_factor = F1AP_SRS_REPETITION_FACTOR_RF2;
          break;
        case F1AP_SRSResource__repetitionFactor_n4:
          srs_resource->repetition_factor = F1AP_SRS_REPETITION_FACTOR_RF4;
          break;
        default:
          AssertFatal(false, "illegal repetition factor %ld\n", f1_srs_resource->repetitionFactor);
          break;
      }

      srs_resource->freq_domain_position = f1_srs_resource->freqDomainPosition;
      srs_resource->freq_domain_shift = f1_srs_resource->freqDomainShift;
      srs_resource->c_srs = f1_srs_resource->c_SRS;
      srs_resource->b_srs = f1_srs_resource->b_SRS;
      srs_resource->b_hop = f1_srs_resource->b_hop;

      switch (f1_srs_resource->groupOrSequenceHopping) {
        case F1AP_SRSResource__groupOrSequenceHopping_neither:
          srs_resource->group_or_sequence_hopping = F1AP_GROUPORSEQUENCEHOPPING_NOTHING;
          break;
        case F1AP_SRSResource__groupOrSequenceHopping_groupHopping:
          srs_resource->group_or_sequence_hopping = F1AP_GROUPORSEQUENCEHOPPING_GROUPHOPPING;
          break;
        case F1AP_SRSResource__groupOrSequenceHopping_sequenceHopping:
          srs_resource->group_or_sequence_hopping = F1AP_GROUPORSEQUENCEHOPPING_SEQUENCEHOPPING;
          break;
        default:
          AssertFatal(false, "illegal groupOrSequenceHopping %ld\n", f1_srs_resource->groupOrSequenceHopping);
          break;
      }

      F1AP_ResourceType_t *f1_resourceType = f1_srs_resource->resourceType;
      f1ap_resource_type_t *resource_type = &srs_resource->resource_type;
      if (f1_resourceType->present == F1AP_ResourceType_PR_NOTHING) {
        resource_type->present = F1AP_RESOURCE_TYPE_PR_NOTHING;
      } else if (f1_resourceType->present == F1AP_ResourceType_PR_periodic) {
        resource_type->present = F1AP_RESOURCE_TYPE_PR_PERIODIC;
        f1ap_resource_type_periodic_t *periodic = &resource_type->choice.periodic;
        switch (f1_resourceType->choice.periodic->periodicity) {
          case F1AP_ResourceTypePeriodic__periodicity_slot1:
            periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT1;
            break;
          case F1AP_ResourceTypePeriodic__periodicity_slot2:
            periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT2;
            break;
          case F1AP_ResourceTypePeriodic__periodicity_slot4:
            periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT4;
            break;
          case F1AP_ResourceTypePeriodic__periodicity_slot5:
            periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT5;
            break;
          case F1AP_ResourceTypePeriodic__periodicity_slot8:
            periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT8;
            break;
          case F1AP_ResourceTypePeriodic__periodicity_slot10:
            periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT10;
            break;
          case F1AP_ResourceTypePeriodic__periodicity_slot16:
            periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT16;
            break;
          case F1AP_ResourceTypePeriodic__periodicity_slot20:
            periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT20;
            break;
          case F1AP_ResourceTypePeriodic__periodicity_slot32:
            periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT32;
            break;
          case F1AP_ResourceTypePeriodic__periodicity_slot40:
            periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT40;
            break;
          case F1AP_ResourceTypePeriodic__periodicity_slot64:
            periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT64;
            break;
          case F1AP_ResourceTypePeriodic__periodicity_slot80:
            periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT80;
            break;
          case F1AP_ResourceTypePeriodic__periodicity_slot160:
            periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT160;
            break;
          case F1AP_ResourceTypePeriodic__periodicity_slot320:
            periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT320;
            break;
          case F1AP_ResourceTypePeriodic__periodicity_slot640:
            periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT640;
            break;
          case F1AP_ResourceTypePeriodic__periodicity_slot1280:
            periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT1280;
            break;
          case F1AP_ResourceTypePeriodic__periodicity_slot2560:
            periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT2560;
            break;
          default:
            AssertFatal(false, "illegal periodicity %ld\n", f1_resourceType->choice.periodic->periodicity);
            break;
        }
        periodic->offset = f1_resourceType->choice.periodic->offset;
      } else if (f1_resourceType->present == F1AP_ResourceType_PR_semi_persistent) {
        resource_type->present = F1AP_RESOURCE_TYPE_PR_SEMI_PERSISTENT;
        f1ap_resource_type_semi_persistent_t *semi_persistent = &resource_type->choice.semi_persistent;
        switch (f1_resourceType->choice.semi_persistent->periodicity) {
          case F1AP_ResourceTypeSemi_persistent__periodicity_slot1:
            semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT1;
            break;
          case F1AP_ResourceTypeSemi_persistent__periodicity_slot2:
            semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT2;
            break;
          case F1AP_ResourceTypeSemi_persistent__periodicity_slot4:
            semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT4;
            break;
          case F1AP_ResourceTypeSemi_persistent__periodicity_slot5:
            semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT5;
            break;
          case F1AP_ResourceTypeSemi_persistent__periodicity_slot8:
            semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT8;
            break;
          case F1AP_ResourceTypeSemi_persistent__periodicity_slot10:
            semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT10;
            break;
          case F1AP_ResourceTypeSemi_persistent__periodicity_slot16:
            semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT16;
            break;
          case F1AP_ResourceTypeSemi_persistent__periodicity_slot20:
            semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT20;
            break;
          case F1AP_ResourceTypeSemi_persistent__periodicity_slot32:
            semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT32;
            break;
          case F1AP_ResourceTypeSemi_persistent__periodicity_slot40:
            semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT40;
            break;
          case F1AP_ResourceTypeSemi_persistent__periodicity_slot64:
            semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT64;
            break;
          case F1AP_ResourceTypeSemi_persistent__periodicity_slot80:
            semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT80;
            break;
          case F1AP_ResourceTypeSemi_persistent__periodicity_slot160:
            semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT160;
            break;
          case F1AP_ResourceTypeSemi_persistent__periodicity_slot320:
            semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT320;
            break;
          case F1AP_ResourceTypeSemi_persistent__periodicity_slot640:
            semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT640;
            break;
          case F1AP_ResourceTypeSemi_persistent__periodicity_slot1280:
            semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT1280;
            break;
          case F1AP_ResourceTypeSemi_persistent__periodicity_slot2560:
            semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_PERIODICITY_SLOT2560;
            break;
          default:
            AssertFatal(false, "illegal periodicity %ld\n", f1_resourceType->choice.semi_persistent->periodicity);
            break;
        }
        semi_persistent->offset = f1_resourceType->choice.semi_persistent->offset;
      } else if (f1_resourceType->present == F1AP_ResourceType_PR_aperiodic) {
        resource_type->present = F1AP_RESOURCE_TYPE_PR_APERIODIC;
        resource_type->choice.aperiodic = f1_resourceType->choice.aperiodic->aperiodicResourceType;
      } else {
        AssertFatal(false, "illegal resourceType %d\n", f1_resourceType->present);
      }

      srs_resource->sequence_id = f1_srs_resource->sequenceId;
    }
  }

  // optional: posSRSResource_List
  if (in_config->posSRSResource_List) {
    out_config->pos_srs_resource_list = calloc_or_fail(1, sizeof(*out_config->pos_srs_resource_list));
    f1ap_pos_srs_resource_list_t *pos_srs_resource_list = out_config->pos_srs_resource_list;
    uint32_t pos_srs_resource_list_length = in_config->posSRSResource_List->list.count;
    pos_srs_resource_list->pos_srs_resource_list_length = pos_srs_resource_list_length;
    pos_srs_resource_list->pos_srs_resource_item =
        calloc_or_fail(pos_srs_resource_list_length, sizeof(*pos_srs_resource_list->pos_srs_resource_item));
    for (int i = 0; i < pos_srs_resource_list_length; i++) {
      F1AP_PosSRSResource_Item_t *f1_pos_srs_resource = in_config->posSRSResource_List->list.array[i];
      f1ap_pos_srs_resource_item_t *pos_srs_resource = &pos_srs_resource_list->pos_srs_resource_item[i];
      pos_srs_resource->srs_pos_resource_id = f1_pos_srs_resource->srs_PosResourceId;

      F1AP_TransmissionCombPos_t *f1_tx_comb_pos = f1_pos_srs_resource->transmissionCombPos;
      f1ap_transmission_comb_pos_t *tx_comb_pos = &pos_srs_resource->transmission_comb_pos;
      switch (f1_tx_comb_pos->present) {
        case F1AP_TransmissionCombPos_PR_NOTHING:
          tx_comb_pos->present = F1AP_TRANSMISSION_COMB_POS_PR_NOTHING;
          break;
        case F1AP_TransmissionCombPos_PR_n2:
          tx_comb_pos->present = F1AP_TRANSMISSION_COMB_POS_PR_N2;
          tx_comb_pos->choice.n2.comb_offset_n2 = f1_tx_comb_pos->choice.n2->combOffset_n2;
          tx_comb_pos->choice.n2.cyclic_shift_n2 = f1_tx_comb_pos->choice.n2->cyclicShift_n2;
          break;
        case F1AP_TransmissionCombPos_PR_n4:
          tx_comb_pos->present = F1AP_TRANSMISSION_COMB_POS_PR_N4;
          tx_comb_pos->choice.n4.comb_offset_n4 = f1_tx_comb_pos->choice.n4->combOffset_n4;
          tx_comb_pos->choice.n4.cyclic_shift_n4 = f1_tx_comb_pos->choice.n4->cyclicShift_n4;
          break;
        case F1AP_TransmissionCombPos_PR_n8:
          tx_comb_pos->present = F1AP_TRANSMISSION_COMB_POS_PR_N8;
          tx_comb_pos->choice.n8.comb_offset_n8 = f1_tx_comb_pos->choice.n8->combOffset_n8;
          tx_comb_pos->choice.n8.cyclic_shift_n8 = f1_tx_comb_pos->choice.n8->cyclicShift_n8;
          break;
        default:
          AssertFatal(false, "illegal transmissionComb %d\n", f1_tx_comb_pos->present);
          break;
      }

      pos_srs_resource->start_position = f1_pos_srs_resource->startPosition;

      switch (f1_pos_srs_resource->nrofSymbols) {
        case F1AP_PosSRSResource_Item__nrofSymbols_n1:
          pos_srs_resource->nr_of_symbols = F1AP_SRS_RESOURCE_ITEM_NUMBER_OF_SYMBOLS_N1;
          break;
        case F1AP_PosSRSResource_Item__nrofSymbols_n2:
          pos_srs_resource->nr_of_symbols = F1AP_SRS_RESOURCE_ITEM_NUMBER_OF_SYMBOLS_N2;
          break;
        case F1AP_PosSRSResource_Item__nrofSymbols_n4:
          pos_srs_resource->nr_of_symbols = F1AP_SRS_RESOURCE_ITEM_NUMBER_OF_SYMBOLS_N4;
          break;
        case F1AP_PosSRSResource_Item__nrofSymbols_n8:
          pos_srs_resource->nr_of_symbols = F1AP_SRS_RESOURCE_ITEM_NUMBER_OF_SYMBOLS_N8;
          break;
        case F1AP_PosSRSResource_Item__nrofSymbols_n12:
          pos_srs_resource->nr_of_symbols = F1AP_SRS_RESOURCE_ITEM_NUMBER_OF_SYMBOLS_N12;
          break;
        default:
          AssertFatal(false, "illegal number of symbols %ld\n", f1_pos_srs_resource->nrofSymbols);
          break;
      }

      pos_srs_resource->freq_domain_shift = f1_pos_srs_resource->freqDomainShift;
      pos_srs_resource->c_srs = f1_pos_srs_resource->c_SRS;

      switch (f1_pos_srs_resource->groupOrSequenceHopping) {
        case F1AP_PosSRSResource_Item__groupOrSequenceHopping_neither:
          pos_srs_resource->group_or_sequence_hopping = F1AP_GROUPORSEQUENCEHOPPING_NOTHING;
          break;
        case F1AP_PosSRSResource_Item__groupOrSequenceHopping_groupHopping:
          pos_srs_resource->group_or_sequence_hopping = F1AP_GROUPORSEQUENCEHOPPING_GROUPHOPPING;
          break;
        case F1AP_PosSRSResource_Item__groupOrSequenceHopping_sequenceHopping:
          pos_srs_resource->group_or_sequence_hopping = F1AP_GROUPORSEQUENCEHOPPING_SEQUENCEHOPPING;
          break;
        default:
          AssertFatal(false, "illegal groupOrSequenceHopping %ld\n", f1_pos_srs_resource->groupOrSequenceHopping);
          break;
      }

      F1AP_ResourceTypePos_t *f1_res_type_pos = f1_pos_srs_resource->resourceTypePos;
      f1ap_resource_type_pos_t *res_type_pos = &pos_srs_resource->resource_type_pos;
      if (f1_res_type_pos->present == F1AP_ResourceTypePos_PR_NOTHING) {
        res_type_pos->present = F1AP_RESOURCE_TYPE_POS_PR_NOTHING;
      } else if (f1_res_type_pos->present == F1AP_ResourceTypePos_PR_periodic) {
        res_type_pos->present = F1AP_RESOURCE_TYPE_POS_PR_PERIODIC;
        f1ap_resource_type_periodic_pos_t *pos_periodic = &res_type_pos->choice.periodic;
        switch (f1_res_type_pos->choice.periodic->periodicity) {
          case F1AP_ResourceTypePeriodicPos__periodicity_slot1:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT1;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot2:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT2;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot4:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT4;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot5:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT5;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot8:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT8;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot10:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT10;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot16:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT16;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot20:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT20;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot32:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT32;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot40:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT40;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot64:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT64;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot80:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT80;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot160:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT160;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot320:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT320;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot640:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT640;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot1280:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT1280;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot2560:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT2560;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot5120:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT5120;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot10240:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT10240;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot20480:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT20480;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot40960:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT40960;
            break;
          case F1AP_ResourceTypePeriodicPos__periodicity_slot81920:
            pos_periodic->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT81920;
            break;
          default:
            AssertFatal(false, "illegal periodicity %ld\n", f1_res_type_pos->choice.periodic->periodicity);
            break;
        }
        pos_periodic->offset = f1_res_type_pos->choice.periodic->offset;
      } else if (f1_res_type_pos->present == F1AP_ResourceTypePos_PR_semi_persistent) {
        res_type_pos->present = F1AP_RESOURCE_TYPE_POS_PR_SEMI_PERSISTENT;
        f1ap_resource_type_semi_persistent_pos_t *pos_semi_persistent = &res_type_pos->choice.semi_persistent;
        switch (f1_res_type_pos->choice.semi_persistent->periodicity) {
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot1:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT1;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot2:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT2;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot4:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT4;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot5:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT5;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot8:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT8;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot10:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT10;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot16:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT16;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot20:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT20;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot32:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT32;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot40:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT40;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot64:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT64;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot80:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT80;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot160:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT160;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot320:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT320;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot640:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT640;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot1280:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT1280;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot2560:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT2560;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot5120:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT5120;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot10240:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT10240;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot20480:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT20480;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot40960:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT40960;
            break;
          case F1AP_ResourceTypeSemi_persistentPos__periodicity_slot81920:
            pos_semi_persistent->periodicity = F1AP_SRS_RESOURCE_TYPE_POS_PERIODICITY_SLOT81920;
            break;
          default:
            AssertFatal(false, "illegal periodicity %ld\n", f1_res_type_pos->choice.semi_persistent->periodicity);
            break;
        }
        pos_semi_persistent->offset = f1_res_type_pos->choice.semi_persistent->offset;
      } else if (f1_res_type_pos->present == F1AP_ResourceTypePos_PR_aperiodic) {
        res_type_pos->present = F1AP_RESOURCE_TYPE_POS_PR_APERIODIC;
        res_type_pos->choice.aperiodic.slot_offset = f1_res_type_pos->choice.aperiodic->slotOffset;
      } else {
        AssertFatal(false, "illegal resourceType %d\n", f1_res_type_pos->present);
      }

      pos_srs_resource->sequence_id = f1_pos_srs_resource->sequenceId;
    }
  }

  // optional: sRSResourceSet_List
  if (in_config->sRSResourceSet_List) {
    out_config->srs_resource_set_list = calloc_or_fail(1, sizeof(*out_config->srs_resource_set_list));
    f1ap_srs_resource_set_list_t *srs_resource_set_list = out_config->srs_resource_set_list;
    uint32_t srs_resource_set_list_length = in_config->sRSResourceSet_List->list.count;
    srs_resource_set_list->srs_resource_set_list_length = srs_resource_set_list_length;
    srs_resource_set_list->srs_resource_set =
        calloc_or_fail(srs_resource_set_list_length, sizeof(*srs_resource_set_list->srs_resource_set));
    for (int i = 0; i < srs_resource_set_list_length; i++) {
      F1AP_SRSResourceSet_t *f1_srs_resource_set = in_config->sRSResourceSet_List->list.array[i];
      f1ap_srs_resource_set_t *srs_resource_set = &srs_resource_set_list->srs_resource_set[i];
      srs_resource_set->srs_resource_set_id = f1_srs_resource_set->sRSResourceSetID;
      uint8_t srs_resource_id_list_length = f1_srs_resource_set->sRSResourceID_List->list.count;
      srs_resource_set->srs_resource_id_list.srs_resource_id_list_length = srs_resource_id_list_length;
      srs_resource_set->srs_resource_id_list.srs_resource_id =
          calloc_or_fail(srs_resource_id_list_length, sizeof(*srs_resource_set->srs_resource_id_list.srs_resource_id));
      for (int j = 0; j < srs_resource_id_list_length; j++) {
        F1AP_SRSResourceID_t *f1_srs_resource_id = f1_srs_resource_set->sRSResourceID_List->list.array[j];
        srs_resource_set->srs_resource_id_list.srs_resource_id[j] = *f1_srs_resource_id;
      }

      F1AP_ResourceSetType_t *f1_res_set_type = f1_srs_resource_set->resourceSetType;
      f1ap_resource_set_type_t *res_set_type = &srs_resource_set->resource_set_type;
      switch (f1_res_set_type->present) {
        case F1AP_ResourceSetType_PR_NOTHING:
          res_set_type->present = F1AP_RESOURCE_SET_TYPE_PR_NOTHING;
          break;
        case F1AP_ResourceSetType_PR_periodic:
          res_set_type->present = F1AP_RESOURCE_SET_TYPE_PR_PERIODIC;
          res_set_type->choice.periodic = f1_res_set_type->choice.periodic->periodicSet;
          break;
        case F1AP_ResourceSetType_PR_semi_persistent:
          res_set_type->present = F1AP_RESOURCE_SET_TYPE_PR_SEMI_PERSISTENT;
          res_set_type->choice.semi_persistent = f1_res_set_type->choice.semi_persistent->semi_persistentSet;
          break;
        case F1AP_ResourceSetType_PR_aperiodic:
          res_set_type->present = F1AP_RESOURCE_SET_TYPE_PR_APERIODIC;
          res_set_type->choice.aperiodic.srs_resource_trigger = f1_res_set_type->choice.aperiodic->sRSResourceTrigger_List;
          res_set_type->choice.aperiodic.slot_offset = f1_res_set_type->choice.aperiodic->slotoffset;
          break;
        default:
          AssertFatal(false, "illegal resource set type %d\n", f1_res_set_type->present);
          break;
      }
    }
  }

  // optional: posSRSResourceSet_List
  if (in_config->posSRSResourceSet_List) {
    out_config->pos_srs_resource_set_list = calloc_or_fail(1, sizeof(*out_config->pos_srs_resource_set_list));
    f1ap_pos_srs_resource_set_list_t *pos_srs_resource_set_list = out_config->pos_srs_resource_set_list;
    uint32_t pos_srs_resource_set_list_length = in_config->posSRSResourceSet_List->list.count;
    pos_srs_resource_set_list->pos_srs_resource_set_list_length = pos_srs_resource_set_list_length;
    pos_srs_resource_set_list->pos_srs_resource_set_item =
        calloc_or_fail(pos_srs_resource_set_list_length, sizeof(*pos_srs_resource_set_list->pos_srs_resource_set_item));
    for (int i = 0; i < pos_srs_resource_set_list_length; i++) {
      F1AP_PosSRSResourceSet_Item_t *f1_pos_srs_resource_set = in_config->posSRSResourceSet_List->list.array[i];
      f1ap_pos_srs_resource_set_item_t *pos_srs_resource_set = &pos_srs_resource_set_list->pos_srs_resource_set_item[i];
      pos_srs_resource_set->pos_srs_resource_set_id = f1_pos_srs_resource_set->possrsResourceSetID;
      uint8_t pos_srs_resource_id_list_length = f1_pos_srs_resource_set->possRSResourceID_List->list.count;
      pos_srs_resource_set->pos_srs_resource_id_list.pos_srs_resource_id_list_length = pos_srs_resource_id_list_length;
      pos_srs_resource_set->pos_srs_resource_id_list.srs_pos_resource_id =
          calloc_or_fail(pos_srs_resource_id_list_length,
                         sizeof(*pos_srs_resource_set->pos_srs_resource_id_list.srs_pos_resource_id));
      for (int j = 0; j < pos_srs_resource_id_list_length; j++) {
        F1AP_SRSPosResourceID_t *f1_pos_srs_resource_id = f1_pos_srs_resource_set->possRSResourceID_List->list.array[j];
        pos_srs_resource_set->pos_srs_resource_id_list.srs_pos_resource_id[j] = *f1_pos_srs_resource_id;
      }

      F1AP_PosResourceSetType_t *f1_pos_res_set_type = f1_pos_srs_resource_set->posresourceSetType;
      f1ap_pos_resource_set_type_t *pos_res_set_type = &pos_srs_resource_set->pos_resource_set_type;
      switch (f1_pos_res_set_type->present) {
        case F1AP_PosResourceSetType_PR_NOTHING:
          pos_res_set_type->present = F1AP_POS_RESOURCE_SET_TYPE_PR_NOTHING;
          break;
        case F1AP_PosResourceSetType_PR_periodic:
          pos_res_set_type->present = F1AP_POS_RESOURCE_SET_TYPE_PR_PERIODIC;
          pos_res_set_type->choice.periodic = f1_pos_res_set_type->choice.periodic->posperiodicSet;
          break;
        case F1AP_PosResourceSetType_PR_semi_persistent:
          pos_res_set_type->present = F1AP_POS_RESOURCE_SET_TYPE_PR_SEMI_PERSISTENT;
          pos_res_set_type->choice.semi_persistent = f1_pos_res_set_type->choice.semi_persistent->possemi_persistentSet;
          break;
        case F1AP_PosResourceSetType_PR_aperiodic:
          pos_res_set_type->present = F1AP_POS_RESOURCE_SET_TYPE_PR_APERIODIC;
          pos_res_set_type->choice.srs_resource = f1_pos_res_set_type->choice.aperiodic->sRSResourceTrigger_List;
          break;
        default:
          AssertFatal(false, "illegal resource set type pos %d\n", f1_pos_res_set_type->present);
          break;
      }
    }
  }
}

static void decode_srs_carrier_list_item(const F1AP_SRSCarrier_List_Item_t *in_item, f1ap_srs_carrier_list_item_t *out_item)
{
  // pointA
  out_item->pointA = in_item->pointA;

  // Uplink Channel BW-PerSCS-List
  f1ap_uplink_channel_bw_per_scs_list_t *uplink_channel_bw_per_scs_list = &out_item->uplink_channel_bw_per_scs_list;
  const F1AP_UplinkChannelBW_PerSCS_List_t *f1_uplink_channel_bw_per_scs_list = in_item->uplinkChannelBW_PerSCS_List;

  uint32_t scs_specific_carrier_list_length = f1_uplink_channel_bw_per_scs_list->list.count;
  AssertFatal(scs_specific_carrier_list_length > 0, "Atleast 1 uplink channel bw per scs list should be present\n");
  uplink_channel_bw_per_scs_list->scs_specific_carrier_list_length = scs_specific_carrier_list_length;
  uplink_channel_bw_per_scs_list->scs_specific_carrier =
      calloc_or_fail(scs_specific_carrier_list_length, sizeof(*uplink_channel_bw_per_scs_list->scs_specific_carrier));
  for (int i = 0; i < scs_specific_carrier_list_length; i++) {
    F1AP_SCS_SpecificCarrier_t *f1_scs_specific_carrier = f1_uplink_channel_bw_per_scs_list->list.array[i];
    f1ap_scs_specific_carrier_t *scs_specific_carrier = &uplink_channel_bw_per_scs_list->scs_specific_carrier[i];
    // offset to carrier
    scs_specific_carrier->offset_to_carrier = f1_scs_specific_carrier->offsetToCarrier;
    // subcarrier spacing
    switch (f1_scs_specific_carrier->subcarrierSpacing) {
      case F1AP_SCS_SpecificCarrier__subcarrierSpacing_kHz15:
        scs_specific_carrier->subcarrier_spacing = F1AP_SUBCARRIER_SPACING_15KHZ;
        break;
      case F1AP_SCS_SpecificCarrier__subcarrierSpacing_kHz30:
        scs_specific_carrier->subcarrier_spacing = F1AP_SUBCARRIER_SPACING_30KHZ;
        break;
      case F1AP_SCS_SpecificCarrier__subcarrierSpacing_kHz60:
        scs_specific_carrier->subcarrier_spacing = F1AP_SUBCARRIER_SPACING_60KHZ;
        break;
      case F1AP_SCS_SpecificCarrier__subcarrierSpacing_kHz120:
        scs_specific_carrier->subcarrier_spacing = F1AP_SUBCARRIER_SPACING_120KHZ;
        break;
      default:
        AssertFatal(false, "illegal subcarrier spacing %ld\n", f1_scs_specific_carrier->subcarrierSpacing);
        break;
    }
    // carrier bandwidth
    scs_specific_carrier->carrier_bandwidth = f1_scs_specific_carrier->carrierBandwidth;
  }

  // Active UL BWP
  const F1AP_ActiveULBWP_t *f1_active_ul_bwp = in_item->activeULBWP;
  f1ap_active_ul_bwp_t *active_ul_bwp = &out_item->active_ul_bwp;

  // location and bandwidth
  active_ul_bwp->location_and_bandwidth = f1_active_ul_bwp->locationAndBandwidth;
  // subcarrier spacing
  switch (f1_active_ul_bwp->subcarrierSpacing) {
    case F1AP_ActiveULBWP__subcarrierSpacing_kHz15:
      active_ul_bwp->subcarrier_spacing = F1AP_SUBCARRIER_SPACING_15KHZ;
      break;
    case F1AP_ActiveULBWP__subcarrierSpacing_kHz30:
      active_ul_bwp->subcarrier_spacing = F1AP_SUBCARRIER_SPACING_30KHZ;
      break;
    case F1AP_ActiveULBWP__subcarrierSpacing_kHz60:
      active_ul_bwp->subcarrier_spacing = F1AP_SUBCARRIER_SPACING_60KHZ;
      break;
    case F1AP_ActiveULBWP__subcarrierSpacing_kHz120:
      active_ul_bwp->subcarrier_spacing = F1AP_SUBCARRIER_SPACING_120KHZ;
      break;
    default:
      AssertFatal(false, "illegal subcarrier spacing %ld\n", f1_active_ul_bwp->subcarrierSpacing);
      break;
  }

  // cyclic prefix
  if (f1_active_ul_bwp->cyclicPrefix == F1AP_ActiveULBWP__cyclicPrefix_normal)
    active_ul_bwp->cyclic_prefix = F1AP_CP_TYPE_NORMAL;
  else
    active_ul_bwp->cyclic_prefix = F1AP_CP_TYPE_EXTENDED;

  // Tx Direct Current Location
  active_ul_bwp->tx_direct_current_location = f1_active_ul_bwp->txDirectCurrentLocation;

  // SRS Config
  const F1AP_SRSConfig_t *f1_sRSConfig = f1_active_ul_bwp->sRSConfig;
  f1ap_srs_config_t *sRSConfig = &active_ul_bwp->srs_config;
  decode_srs_config(f1_sRSConfig, sRSConfig);
}

static void decode_srs_carrier_list(f1ap_srs_carrier_list_t *out_list, const F1AP_SRSCarrier_List_t *in_list)
{
  uint32_t list_len = in_list->list.count;
  AssertFatal(list_len > 0, "atleast 1 SRS carrier list must be present");
  out_list->srs_carrier_list_length = list_len;
  out_list->srs_carrier_list_item = calloc_or_fail(list_len, sizeof(*out_list->srs_carrier_list_item));
  for (int i = 0; i < list_len; i++) {
    F1AP_SRSCarrier_List_Item_t *in_item = in_list->list.array[i];
    f1ap_srs_carrier_list_item_t *out_item = &out_list->srs_carrier_list_item[i];
    decode_srs_carrier_list_item(in_item, out_item);
  }
}

static f1ap_srs_config_t cp_srs_config(const f1ap_srs_config_t *in_config)
{
  f1ap_srs_config_t out_config = {0};
  // optional: srs_resource_list
  if (in_config->srs_resource_list) {
    f1ap_srs_resource_list_t *srs_resource_list = in_config->srs_resource_list;
    out_config.srs_resource_list = calloc_or_fail(1, sizeof(*out_config.srs_resource_list));
    f1ap_srs_resource_list_t *f1_srs_resource_list = out_config.srs_resource_list;
    uint32_t srs_resource_list_length = srs_resource_list->srs_resource_list_length;
    f1_srs_resource_list->srs_resource_list_length = srs_resource_list_length;
    f1_srs_resource_list->srs_resource = calloc_or_fail(srs_resource_list_length, sizeof(*f1_srs_resource_list->srs_resource));
    for (int i = 0; i < srs_resource_list_length; i++) {
      f1ap_srs_resource_t *srs_resource = &srs_resource_list->srs_resource[i];
      f1ap_srs_resource_t *f1_srs_resource = &f1_srs_resource_list->srs_resource[i];
      f1_srs_resource->srs_resource_id = srs_resource->srs_resource_id;
      f1_srs_resource->nr_of_srs_ports = srs_resource->nr_of_srs_ports;

      f1ap_transmission_comb_t *f1_srs_tx_comb = &f1_srs_resource->transmission_comb;
      f1ap_transmission_comb_t *srs_tx_comb = &srs_resource->transmission_comb;
      f1_srs_tx_comb->present = srs_tx_comb->present;
      switch (srs_tx_comb->present) {
        case F1AP_TRANSMISSION_COMB_PR_NOTHING:
          // nothing to copy
          break;
        case F1AP_TRANSMISSION_COMB_PR_N2:
          f1_srs_tx_comb->choice.n2.comb_offset_n2 = srs_tx_comb->choice.n2.comb_offset_n2;
          f1_srs_tx_comb->choice.n2.cyclic_shift_n2 = srs_tx_comb->choice.n2.cyclic_shift_n2;
          break;
        case F1AP_TRANSMISSION_COMB_PR_N4:
          f1_srs_tx_comb->choice.n4.comb_offset_n4 = srs_tx_comb->choice.n4.comb_offset_n4;
          f1_srs_tx_comb->choice.n4.cyclic_shift_n4 = srs_tx_comb->choice.n4.cyclic_shift_n4;
          break;
        default:
          AssertFatal(false, "illegal transmissionComb %d\n", srs_tx_comb->present);
          break;
      }

      f1_srs_resource->start_position = srs_resource->start_position;
      f1_srs_resource->nr_of_symbols = srs_resource->nr_of_symbols;
      f1_srs_resource->repetition_factor = srs_resource->repetition_factor;
      f1_srs_resource->freq_domain_position = srs_resource->freq_domain_position;
      f1_srs_resource->freq_domain_shift = srs_resource->freq_domain_shift;
      f1_srs_resource->c_srs = srs_resource->c_srs;
      f1_srs_resource->b_srs = srs_resource->b_srs;
      f1_srs_resource->b_hop = srs_resource->b_hop;
      f1_srs_resource->group_or_sequence_hopping = srs_resource->group_or_sequence_hopping;
      f1_srs_resource->resource_type.present = srs_resource->resource_type.present;

      f1ap_resource_type_t *f1_res_type = &f1_srs_resource->resource_type;
      f1ap_resource_type_t *res_type = &srs_resource->resource_type;
      f1_res_type->present = res_type->present;
      if (res_type->present == F1AP_RESOURCE_TYPE_PR_NOTHING) {
        // nothing to copy
      } else if (res_type->present == F1AP_RESOURCE_TYPE_PR_PERIODIC) {
        f1_res_type->choice.periodic.periodicity = res_type->choice.periodic.periodicity;
        f1_res_type->choice.periodic.offset = res_type->choice.periodic.offset;
      } else if (res_type->present == F1AP_RESOURCE_TYPE_PR_SEMI_PERSISTENT) {
        f1_res_type->choice.semi_persistent.periodicity = res_type->choice.semi_persistent.periodicity;
        f1_res_type->choice.semi_persistent.offset = res_type->choice.semi_persistent.offset;
      } else if (res_type->present == F1AP_RESOURCE_TYPE_PR_APERIODIC) {
        f1_res_type->choice.aperiodic = res_type->choice.aperiodic;
      } else {
        AssertFatal(false, "illegal resourceType %d\n", res_type->present);
      }

      f1_srs_resource->sequence_id = srs_resource->sequence_id;
    }
  }

  // optional: pos_srs_resource_list
  if (in_config->pos_srs_resource_list) {
    f1ap_pos_srs_resource_list_t *pos_srs_resource_list = in_config->pos_srs_resource_list;
    out_config.pos_srs_resource_list = calloc_or_fail(1, sizeof(*out_config.pos_srs_resource_list));
    f1ap_pos_srs_resource_list_t *f1_pos_srs_resource_list = out_config.pos_srs_resource_list;
    uint32_t pos_srs_resource_list_length = pos_srs_resource_list->pos_srs_resource_list_length;
    f1_pos_srs_resource_list->pos_srs_resource_list_length = pos_srs_resource_list_length;
    f1_pos_srs_resource_list->pos_srs_resource_item =
        calloc_or_fail(pos_srs_resource_list_length, sizeof(*f1_pos_srs_resource_list->pos_srs_resource_item));
    for (int i = 0; i < pos_srs_resource_list_length; i++) {
      f1ap_pos_srs_resource_item_t *pos_srs_resource = &pos_srs_resource_list->pos_srs_resource_item[i];
      f1ap_pos_srs_resource_item_t *f1_pos_srs_resource = &f1_pos_srs_resource_list->pos_srs_resource_item[i];
      f1_pos_srs_resource->srs_pos_resource_id = pos_srs_resource->srs_pos_resource_id;
      f1_pos_srs_resource->transmission_comb_pos.present = pos_srs_resource->transmission_comb_pos.present;

      f1ap_transmission_comb_pos_t *f1_tx_comb_pos = &f1_pos_srs_resource->transmission_comb_pos;
      f1ap_transmission_comb_pos_t *tx_comb_pos = &pos_srs_resource->transmission_comb_pos;
      switch (tx_comb_pos->present) {
        case F1AP_TRANSMISSION_COMB_POS_PR_NOTHING:
          // nothing to copy
          break;
        case F1AP_TRANSMISSION_COMB_POS_PR_N2:
          f1_tx_comb_pos->choice.n2.comb_offset_n2 = tx_comb_pos->choice.n2.comb_offset_n2;
          f1_tx_comb_pos->choice.n2.cyclic_shift_n2 = tx_comb_pos->choice.n2.cyclic_shift_n2;
          break;
        case F1AP_TRANSMISSION_COMB_POS_PR_N4:
          f1_tx_comb_pos->choice.n4.comb_offset_n4 = tx_comb_pos->choice.n4.comb_offset_n4;
          f1_tx_comb_pos->choice.n4.cyclic_shift_n4 = tx_comb_pos->choice.n4.cyclic_shift_n4;
          break;
        case F1AP_TRANSMISSION_COMB_POS_PR_N8:
          f1_tx_comb_pos->choice.n8.comb_offset_n8 = tx_comb_pos->choice.n8.comb_offset_n8;
          f1_tx_comb_pos->choice.n8.cyclic_shift_n8 = tx_comb_pos->choice.n8.cyclic_shift_n8;
          break;
        default:
          AssertFatal(false, "illegal transmissionComb %d\n", tx_comb_pos->present);
          break;
      }

      f1_pos_srs_resource->start_position = pos_srs_resource->start_position;
      f1_pos_srs_resource->nr_of_symbols = pos_srs_resource->nr_of_symbols;
      f1_pos_srs_resource->freq_domain_shift = pos_srs_resource->freq_domain_shift;
      f1_pos_srs_resource->c_srs = pos_srs_resource->c_srs;
      f1_pos_srs_resource->group_or_sequence_hopping = pos_srs_resource->group_or_sequence_hopping;

      f1ap_resource_type_pos_t *f1_res_type_pos = &f1_pos_srs_resource->resource_type_pos;
      f1ap_resource_type_pos_t *res_type_pos = &pos_srs_resource->resource_type_pos;
      f1_res_type_pos->present = res_type_pos->present;
      if (res_type_pos->present == F1AP_RESOURCE_TYPE_POS_PR_NOTHING) {
        // nothing to copy
      } else if (res_type_pos->present == F1AP_RESOURCE_TYPE_POS_PR_PERIODIC) {
        f1_res_type_pos->choice.periodic.periodicity = res_type_pos->choice.periodic.periodicity;
        f1_res_type_pos->choice.periodic.offset = res_type_pos->choice.periodic.offset;
      } else if (pos_srs_resource->resource_type_pos.present == F1AP_RESOURCE_TYPE_POS_PR_SEMI_PERSISTENT) {
        f1_res_type_pos->choice.semi_persistent.periodicity = res_type_pos->choice.semi_persistent.periodicity;
        f1_res_type_pos->choice.semi_persistent.offset = res_type_pos->choice.semi_persistent.offset;
      } else if (res_type_pos->present == F1AP_RESOURCE_TYPE_POS_PR_APERIODIC) {
        f1_res_type_pos->choice.aperiodic.slot_offset = res_type_pos->choice.aperiodic.slot_offset;
      } else {
        AssertFatal(false, "illegal resourceType %d\n", res_type_pos->present);
      }

      f1_pos_srs_resource->sequence_id = pos_srs_resource->sequence_id;
    }
  }

  // optional: srs_resource_set_list
  if (in_config->srs_resource_set_list) {
    f1ap_srs_resource_set_list_t *srs_resource_set_list = in_config->srs_resource_set_list;
    out_config.srs_resource_set_list = calloc_or_fail(1, sizeof(*out_config.srs_resource_set_list));
    f1ap_srs_resource_set_list_t *f1_srs_resource_set_list = out_config.srs_resource_set_list;
    uint32_t srs_resource_set_list_length = srs_resource_set_list->srs_resource_set_list_length;
    f1_srs_resource_set_list->srs_resource_set_list_length = srs_resource_set_list_length;
    f1_srs_resource_set_list->srs_resource_set =
        calloc_or_fail(srs_resource_set_list_length, sizeof(*f1_srs_resource_set_list->srs_resource_set));
    for (int i = 0; i < srs_resource_set_list_length; i++) {
      f1ap_srs_resource_set_t *srs_resource_set = &srs_resource_set_list->srs_resource_set[i];
      f1ap_srs_resource_set_t *f1_srs_resource_set = &f1_srs_resource_set_list->srs_resource_set[i];
      f1_srs_resource_set->srs_resource_set_id = srs_resource_set->srs_resource_set_id;
      uint8_t srs_resource_id_list_length = srs_resource_set->srs_resource_id_list.srs_resource_id_list_length;
      f1_srs_resource_set->srs_resource_id_list.srs_resource_id_list_length = srs_resource_id_list_length;
      f1_srs_resource_set->srs_resource_id_list.srs_resource_id =
          calloc_or_fail(srs_resource_id_list_length, sizeof(*f1_srs_resource_set->srs_resource_id_list.srs_resource_id));
      for (int j = 0; j < srs_resource_id_list_length; j++) {
        f1_srs_resource_set->srs_resource_id_list.srs_resource_id[j] = srs_resource_set->srs_resource_id_list.srs_resource_id[j];
      }

      f1ap_resource_set_type_t *f1_res_set_type = &f1_srs_resource_set->resource_set_type;
      f1ap_resource_set_type_t *res_set_type = &srs_resource_set->resource_set_type;
      f1_res_set_type->present = res_set_type->present;
      switch (res_set_type->present) {
        case F1AP_RESOURCE_SET_TYPE_PR_NOTHING:
          // nothing to copy
          break;
        case F1AP_RESOURCE_SET_TYPE_PR_PERIODIC:
          f1_res_set_type->choice.periodic = res_set_type->choice.periodic;
          break;
        case F1AP_RESOURCE_SET_TYPE_PR_SEMI_PERSISTENT:
          f1_res_set_type->choice.semi_persistent = res_set_type->choice.semi_persistent;
          break;
        case F1AP_RESOURCE_SET_TYPE_PR_APERIODIC:
          f1_res_set_type->choice.aperiodic.srs_resource_trigger = res_set_type->choice.aperiodic.srs_resource_trigger;
          f1_res_set_type->choice.aperiodic.slot_offset = res_set_type->choice.aperiodic.slot_offset;
          break;
        default:
          AssertFatal(false, "illegal resource set type %d\n", res_set_type->present);
          break;
      }
    }
  }

  // optional: pos_srs_resource_set_list
  if (in_config->pos_srs_resource_set_list) {
    f1ap_pos_srs_resource_set_list_t *pos_srs_resource_set_list = in_config->pos_srs_resource_set_list;
    out_config.pos_srs_resource_set_list = calloc_or_fail(1, sizeof(*out_config.pos_srs_resource_set_list));
    f1ap_pos_srs_resource_set_list_t *f1_pos_srs_resource_set_list = out_config.pos_srs_resource_set_list;
    uint32_t pos_srs_resource_set_list_length = pos_srs_resource_set_list->pos_srs_resource_set_list_length;
    f1_pos_srs_resource_set_list->pos_srs_resource_set_list_length = pos_srs_resource_set_list_length;
    f1_pos_srs_resource_set_list->pos_srs_resource_set_item =
        calloc_or_fail(pos_srs_resource_set_list_length, sizeof(*f1_pos_srs_resource_set_list->pos_srs_resource_set_item));
    for (int i = 0; i < pos_srs_resource_set_list_length; i++) {
      f1ap_pos_srs_resource_set_item_t *pos_srs_resource_set = &pos_srs_resource_set_list->pos_srs_resource_set_item[i];
      f1ap_pos_srs_resource_set_item_t *f1_pos_srs_resource_set = &f1_pos_srs_resource_set_list->pos_srs_resource_set_item[i];
      f1_pos_srs_resource_set->pos_srs_resource_set_id = pos_srs_resource_set->pos_srs_resource_set_id;
      uint8_t pos_srs_resource_id_list_length = pos_srs_resource_set->pos_srs_resource_id_list.pos_srs_resource_id_list_length;
      f1_pos_srs_resource_set->pos_srs_resource_id_list.pos_srs_resource_id_list_length = pos_srs_resource_id_list_length;
      f1_pos_srs_resource_set->pos_srs_resource_id_list.srs_pos_resource_id =
          calloc_or_fail(pos_srs_resource_id_list_length,
                         sizeof(*f1_pos_srs_resource_set->pos_srs_resource_id_list.srs_pos_resource_id));
      for (int j = 0; j < pos_srs_resource_id_list_length; j++) {
        f1_pos_srs_resource_set->pos_srs_resource_id_list.srs_pos_resource_id[j] =
            pos_srs_resource_set->pos_srs_resource_id_list.srs_pos_resource_id[j];
      }

      f1ap_pos_resource_set_type_t *f1_pos_res_set_type = &f1_pos_srs_resource_set->pos_resource_set_type;
      f1ap_pos_resource_set_type_t *pos_res_set_type = &pos_srs_resource_set->pos_resource_set_type;
      f1_pos_res_set_type->present = pos_res_set_type->present;
      switch (pos_res_set_type->present) {
        case F1AP_POS_RESOURCE_SET_TYPE_PR_NOTHING:
          // nothing to copy
          break;
        case F1AP_POS_RESOURCE_SET_TYPE_PR_PERIODIC:
          f1_pos_res_set_type->choice.periodic = pos_res_set_type->choice.periodic;
          break;
        case F1AP_POS_RESOURCE_SET_TYPE_PR_SEMI_PERSISTENT:
          f1_pos_res_set_type->choice.semi_persistent = pos_res_set_type->choice.semi_persistent;
          break;
        case F1AP_POS_RESOURCE_SET_TYPE_PR_APERIODIC:
          f1_pos_res_set_type->choice.srs_resource = pos_res_set_type->choice.srs_resource;
          break;
        default:
          AssertFatal(false, "illegal resource set type pos %d\n", pos_res_set_type->present);
          break;
      }
    }
  }
  return out_config;
}

static f1ap_srs_carrier_list_item_t cp_srs_carrier_list_item(const f1ap_srs_carrier_list_item_t *in_item)
{
  f1ap_srs_carrier_list_item_t out_item = {0};
  // pointA
  out_item.pointA = in_item->pointA;

  // Uplink Channel BW-PerSCS-List
  const f1ap_uplink_channel_bw_per_scs_list_t *uplink_channel_bw_per_scs_list = &in_item->uplink_channel_bw_per_scs_list;
  f1ap_uplink_channel_bw_per_scs_list_t *f1_uplink_channel_bw_per_scs_list = &out_item.uplink_channel_bw_per_scs_list;

  uint32_t scs_specific_carrier_list_length = uplink_channel_bw_per_scs_list->scs_specific_carrier_list_length;
  f1_uplink_channel_bw_per_scs_list->scs_specific_carrier_list_length = scs_specific_carrier_list_length;
  f1_uplink_channel_bw_per_scs_list->scs_specific_carrier =
      calloc_or_fail(scs_specific_carrier_list_length, sizeof(*f1_uplink_channel_bw_per_scs_list->scs_specific_carrier));
  for (int i = 0; i < scs_specific_carrier_list_length; i++) {
    f1ap_scs_specific_carrier_t *scs_specific_carrier = &uplink_channel_bw_per_scs_list->scs_specific_carrier[i];
    f1ap_scs_specific_carrier_t *f1_scs_specific_carrier = &f1_uplink_channel_bw_per_scs_list->scs_specific_carrier[i];
    // offset to carrier
    f1_scs_specific_carrier->offset_to_carrier = scs_specific_carrier->offset_to_carrier;
    // subcarrier spacing
    f1_scs_specific_carrier->subcarrier_spacing = scs_specific_carrier->subcarrier_spacing;
    // carrier bandwidth
    f1_scs_specific_carrier->carrier_bandwidth = scs_specific_carrier->carrier_bandwidth;
  }

  // Active UL BWP
  f1ap_active_ul_bwp_t *f1_active_ul_bwp = &out_item.active_ul_bwp;
  const f1ap_active_ul_bwp_t *active_ul_bwp = &in_item->active_ul_bwp;

  // location and bandwidth
  f1_active_ul_bwp->location_and_bandwidth = active_ul_bwp->location_and_bandwidth;
  // subcarrier spacing
  f1_active_ul_bwp->subcarrier_spacing = active_ul_bwp->subcarrier_spacing;

  // cyclic prefix
  f1_active_ul_bwp->cyclic_prefix = active_ul_bwp->cyclic_prefix;

  // Tx Direct Current Location
  f1_active_ul_bwp->tx_direct_current_location = active_ul_bwp->tx_direct_current_location;

  // SRS Config
  f1ap_srs_config_t *f1_sRSConfig = &f1_active_ul_bwp->srs_config;
  const f1ap_srs_config_t *sRSConfig = &active_ul_bwp->srs_config;
  *f1_sRSConfig = cp_srs_config(sRSConfig);
  return out_item;
}

static f1ap_srs_carrier_list_t cp_srs_carrier_list(const f1ap_srs_carrier_list_t *in_list)
{
  f1ap_srs_carrier_list_t out_list = {0};
  uint32_t list_len = in_list->srs_carrier_list_length;
  out_list.srs_carrier_list_length = list_len;
  out_list.srs_carrier_list_item = calloc_or_fail(list_len, sizeof(*out_list.srs_carrier_list_item));
  for (int i = 0; i < list_len; i++) {
    f1ap_srs_carrier_list_item_t *in_item = &in_list->srs_carrier_list_item[i];
    f1ap_srs_carrier_list_item_t *out_item = &out_list.srs_carrier_list_item[i];
    *out_item = cp_srs_carrier_list_item(in_item);
  }
  return out_list;
}

static bool eq_srs_config(const f1ap_srs_config_t *f1_sRSConfig, const f1ap_srs_config_t *sRSConfig)
{
  // optional: srs_resource_list
  if ((f1_sRSConfig->srs_resource_list == NULL) != (sRSConfig->srs_resource_list == NULL)) {
    return false;
  }
  if (sRSConfig->srs_resource_list) {
    f1ap_srs_resource_list_t *srs_resource_list = sRSConfig->srs_resource_list;
    f1ap_srs_resource_list_t *f1_srs_resource_list = f1_sRSConfig->srs_resource_list;
    uint32_t srs_resource_list_length = srs_resource_list->srs_resource_list_length;
    _EQ_CHECK_INT(f1_srs_resource_list->srs_resource_list_length, srs_resource_list_length);
    for (int i = 0; i < srs_resource_list_length; i++) {
      f1ap_srs_resource_t *srs_resource = &srs_resource_list->srs_resource[i];
      f1ap_srs_resource_t *f1_srs_resource = &f1_srs_resource_list->srs_resource[i];
      _EQ_CHECK_INT(f1_srs_resource->srs_resource_id, srs_resource->srs_resource_id);
      _EQ_CHECK_INT(f1_srs_resource->nr_of_srs_ports, srs_resource->nr_of_srs_ports);

      f1ap_transmission_comb_t *f1_srs_tx_comb = &f1_srs_resource->transmission_comb;
      f1ap_transmission_comb_t *srs_tx_comb = &srs_resource->transmission_comb;

      _EQ_CHECK_INT(f1_srs_tx_comb->present, srs_tx_comb->present);
      switch (srs_tx_comb->present) {
        case F1AP_TRANSMISSION_COMB_PR_NOTHING:
          // nothing to check
          break;
        case F1AP_TRANSMISSION_COMB_PR_N2:
          _EQ_CHECK_INT(f1_srs_tx_comb->choice.n2.comb_offset_n2, srs_tx_comb->choice.n2.comb_offset_n2);
          _EQ_CHECK_INT(f1_srs_tx_comb->choice.n2.cyclic_shift_n2, srs_tx_comb->choice.n2.cyclic_shift_n2);
          break;
        case F1AP_TRANSMISSION_COMB_PR_N4:
          _EQ_CHECK_INT(f1_srs_tx_comb->choice.n4.comb_offset_n4, srs_tx_comb->choice.n4.comb_offset_n4);
          _EQ_CHECK_INT(f1_srs_tx_comb->choice.n4.cyclic_shift_n4, srs_tx_comb->choice.n4.cyclic_shift_n4);
          break;
        default:
          AssertFatal(false, "illegal transmissionComb %d\n", srs_tx_comb->present);
          break;
      }

      _EQ_CHECK_INT(f1_srs_resource->start_position, srs_resource->start_position);
      _EQ_CHECK_INT(f1_srs_resource->nr_of_symbols, srs_resource->nr_of_symbols);
      _EQ_CHECK_INT(f1_srs_resource->repetition_factor, srs_resource->repetition_factor);
      _EQ_CHECK_INT(f1_srs_resource->freq_domain_position, srs_resource->freq_domain_position);
      _EQ_CHECK_INT(f1_srs_resource->freq_domain_shift, srs_resource->freq_domain_shift);
      _EQ_CHECK_INT(f1_srs_resource->c_srs, srs_resource->c_srs);
      _EQ_CHECK_INT(f1_srs_resource->b_srs, srs_resource->b_srs);
      _EQ_CHECK_INT(f1_srs_resource->b_hop, srs_resource->b_hop);
      _EQ_CHECK_INT(f1_srs_resource->group_or_sequence_hopping, srs_resource->group_or_sequence_hopping);

      f1ap_resource_type_t *f1_res_type = &f1_srs_resource->resource_type;
      f1ap_resource_type_t *res_type = &srs_resource->resource_type;
      _EQ_CHECK_INT(f1_res_type->present, res_type->present);
      if (res_type->present == F1AP_RESOURCE_TYPE_PR_NOTHING) {
        // nothing to check
      } else if (res_type->present == F1AP_RESOURCE_TYPE_PR_PERIODIC) {
        _EQ_CHECK_INT(f1_res_type->choice.periodic.periodicity, res_type->choice.periodic.periodicity);
        _EQ_CHECK_INT(f1_res_type->choice.periodic.offset, res_type->choice.periodic.offset);
      } else if (res_type->present == F1AP_RESOURCE_TYPE_PR_SEMI_PERSISTENT) {
        _EQ_CHECK_INT(f1_res_type->choice.semi_persistent.periodicity, res_type->choice.semi_persistent.periodicity);
        _EQ_CHECK_INT(f1_res_type->choice.semi_persistent.offset, res_type->choice.semi_persistent.offset);
      } else if (res_type->present == F1AP_RESOURCE_TYPE_PR_APERIODIC) {
        _EQ_CHECK_INT(f1_res_type->choice.aperiodic, res_type->choice.aperiodic);
      } else {
        AssertFatal(false, "illegal resourceType %d\n", res_type->present);
      }

      _EQ_CHECK_INT(f1_srs_resource->sequence_id, srs_resource->sequence_id);
    }
  }

  // optional: pos_srs_resource_list
  if ((f1_sRSConfig->pos_srs_resource_list == NULL) != (sRSConfig->pos_srs_resource_list == NULL)) {
    return false;
  }
  if (sRSConfig->pos_srs_resource_list) {
    f1ap_pos_srs_resource_list_t *pos_srs_resource_list = sRSConfig->pos_srs_resource_list;
    f1ap_pos_srs_resource_list_t *f1_pos_srs_resource_list = f1_sRSConfig->pos_srs_resource_list;
    uint32_t pos_srs_resource_list_length = pos_srs_resource_list->pos_srs_resource_list_length;
    _EQ_CHECK_INT(f1_pos_srs_resource_list->pos_srs_resource_list_length, pos_srs_resource_list_length);
    for (int i = 0; i < pos_srs_resource_list_length; i++) {
      f1ap_pos_srs_resource_item_t *pos_srs_resource = &pos_srs_resource_list->pos_srs_resource_item[i];
      f1ap_pos_srs_resource_item_t *f1_pos_srs_resource = &f1_pos_srs_resource_list->pos_srs_resource_item[i];
      _EQ_CHECK_INT(f1_pos_srs_resource->srs_pos_resource_id, pos_srs_resource->srs_pos_resource_id);
      _EQ_CHECK_INT(f1_pos_srs_resource->transmission_comb_pos.present, pos_srs_resource->transmission_comb_pos.present);

      f1ap_transmission_comb_pos_t *f1_srs_tx_comb_pos = &f1_pos_srs_resource->transmission_comb_pos;
      f1ap_transmission_comb_pos_t *srs_tx_comb_pos = &pos_srs_resource->transmission_comb_pos;
      switch (srs_tx_comb_pos->present) {
        case F1AP_TRANSMISSION_COMB_POS_PR_NOTHING:
          // nothing to check
          break;
        case F1AP_TRANSMISSION_COMB_POS_PR_N2:
          _EQ_CHECK_INT(f1_srs_tx_comb_pos->choice.n2.comb_offset_n2, srs_tx_comb_pos->choice.n2.comb_offset_n2);
          _EQ_CHECK_INT(f1_srs_tx_comb_pos->choice.n2.cyclic_shift_n2, srs_tx_comb_pos->choice.n2.cyclic_shift_n2);
          break;
        case F1AP_TRANSMISSION_COMB_POS_PR_N4:
          _EQ_CHECK_INT(f1_srs_tx_comb_pos->choice.n4.comb_offset_n4, srs_tx_comb_pos->choice.n4.comb_offset_n4);
          _EQ_CHECK_INT(f1_srs_tx_comb_pos->choice.n4.cyclic_shift_n4, srs_tx_comb_pos->choice.n4.cyclic_shift_n4);
          break;
        case F1AP_TRANSMISSION_COMB_POS_PR_N8:
          _EQ_CHECK_INT(f1_srs_tx_comb_pos->choice.n8.comb_offset_n8, srs_tx_comb_pos->choice.n8.comb_offset_n8);
          _EQ_CHECK_INT(f1_srs_tx_comb_pos->choice.n8.cyclic_shift_n8, srs_tx_comb_pos->choice.n8.cyclic_shift_n8);
          break;
        default:
          AssertFatal(false, "illegal transmissionComb %d\n", srs_tx_comb_pos->present);
          break;
      }

      _EQ_CHECK_INT(f1_pos_srs_resource->start_position, pos_srs_resource->start_position);
      _EQ_CHECK_INT(f1_pos_srs_resource->nr_of_symbols, pos_srs_resource->nr_of_symbols);
      _EQ_CHECK_INT(f1_pos_srs_resource->freq_domain_shift, pos_srs_resource->freq_domain_shift);
      _EQ_CHECK_INT(f1_pos_srs_resource->c_srs, pos_srs_resource->c_srs);
      _EQ_CHECK_INT(f1_pos_srs_resource->group_or_sequence_hopping, pos_srs_resource->group_or_sequence_hopping);

      f1ap_resource_type_pos_t *f1_res_type_pos = &f1_pos_srs_resource->resource_type_pos;
      f1ap_resource_type_pos_t *res_type_pos = &pos_srs_resource->resource_type_pos;
      _EQ_CHECK_INT(f1_res_type_pos->present, res_type_pos->present);
      if (res_type_pos->present == F1AP_RESOURCE_TYPE_POS_PR_NOTHING) {
        // nothing to check
      } else if (res_type_pos->present == F1AP_RESOURCE_TYPE_POS_PR_PERIODIC) {
        _EQ_CHECK_INT(f1_res_type_pos->choice.periodic.periodicity, res_type_pos->choice.periodic.periodicity);
        _EQ_CHECK_INT(f1_res_type_pos->choice.periodic.offset, res_type_pos->choice.periodic.offset);
      } else if (res_type_pos->present == F1AP_RESOURCE_TYPE_POS_PR_SEMI_PERSISTENT) {
        _EQ_CHECK_INT(f1_res_type_pos->choice.semi_persistent.periodicity, res_type_pos->choice.semi_persistent.periodicity);
        _EQ_CHECK_INT(f1_res_type_pos->choice.semi_persistent.offset, res_type_pos->choice.semi_persistent.offset);
      } else if (res_type_pos->present == F1AP_RESOURCE_TYPE_POS_PR_APERIODIC) {
        _EQ_CHECK_INT(f1_res_type_pos->choice.aperiodic.slot_offset, res_type_pos->choice.aperiodic.slot_offset);
      } else {
        AssertFatal(false, "illegal resourceType %d\n", res_type_pos->present);
      }

      _EQ_CHECK_INT(f1_pos_srs_resource->sequence_id, pos_srs_resource->sequence_id);
    }
  }

  // optional: srs_resource_set_list
  if ((f1_sRSConfig->srs_resource_set_list == NULL) != (sRSConfig->srs_resource_set_list == NULL)) {
    return false;
  }
  if (sRSConfig->srs_resource_set_list) {
    f1ap_srs_resource_set_list_t *srs_resource_set_list = sRSConfig->srs_resource_set_list;
    f1ap_srs_resource_set_list_t *f1_srs_resource_set_list = f1_sRSConfig->srs_resource_set_list;
    uint32_t srs_resource_set_list_length = srs_resource_set_list->srs_resource_set_list_length;
    _EQ_CHECK_INT(f1_srs_resource_set_list->srs_resource_set_list_length, srs_resource_set_list_length);
    for (int i = 0; i < srs_resource_set_list_length; i++) {
      f1ap_srs_resource_set_t *srs_resource_set = &srs_resource_set_list->srs_resource_set[i];
      f1ap_srs_resource_set_t *f1_srs_resource_set = &f1_srs_resource_set_list->srs_resource_set[i];
      _EQ_CHECK_INT(f1_srs_resource_set->srs_resource_set_id, srs_resource_set->srs_resource_set_id);
      uint8_t srs_resource_id_list_length = srs_resource_set->srs_resource_id_list.srs_resource_id_list_length;
      _EQ_CHECK_INT(f1_srs_resource_set->srs_resource_id_list.srs_resource_id_list_length, srs_resource_id_list_length);
      for (int j = 0; j < srs_resource_id_list_length; j++) {
        _EQ_CHECK_LONG(f1_srs_resource_set->srs_resource_id_list.srs_resource_id[j],
                       srs_resource_set->srs_resource_id_list.srs_resource_id[j]);
      }

      f1ap_resource_set_type_t *f1_res_set_type = &f1_srs_resource_set->resource_set_type;
      f1ap_resource_set_type_t *res_set_type = &srs_resource_set->resource_set_type;
      _EQ_CHECK_INT(f1_res_set_type->present, res_set_type->present);
      switch (res_set_type->present) {
        case F1AP_RESOURCE_SET_TYPE_PR_NOTHING:
          // nothing to check
          break;
        case F1AP_RESOURCE_SET_TYPE_PR_PERIODIC:
          _EQ_CHECK_INT(f1_res_set_type->choice.periodic, res_set_type->choice.periodic);
          break;
        case F1AP_RESOURCE_SET_TYPE_PR_SEMI_PERSISTENT:
          _EQ_CHECK_INT(f1_res_set_type->choice.semi_persistent, res_set_type->choice.semi_persistent);
          break;
        case F1AP_RESOURCE_SET_TYPE_PR_APERIODIC:
          _EQ_CHECK_INT(f1_res_set_type->choice.aperiodic.srs_resource_trigger,
                        res_set_type->choice.aperiodic.srs_resource_trigger);
          _EQ_CHECK_LONG(f1_res_set_type->choice.aperiodic.slot_offset, res_set_type->choice.aperiodic.slot_offset);
          break;
        default:
          AssertFatal(false, "illegal resource set type %d\n", res_set_type->present);
          break;
      }
    }
  }

  // optional: pos_srs_resource_set_list
  if ((f1_sRSConfig->pos_srs_resource_set_list == NULL) != (sRSConfig->pos_srs_resource_set_list == NULL)) {
    return false;
  }
  if (sRSConfig->pos_srs_resource_set_list) {
    f1ap_pos_srs_resource_set_list_t *pos_srs_resource_set_list = sRSConfig->pos_srs_resource_set_list;
    f1ap_pos_srs_resource_set_list_t *f1_pos_srs_resource_set_list = f1_sRSConfig->pos_srs_resource_set_list;
    uint32_t pos_srs_resource_set_list_length = pos_srs_resource_set_list->pos_srs_resource_set_list_length;
    _EQ_CHECK_INT(f1_pos_srs_resource_set_list->pos_srs_resource_set_list_length, pos_srs_resource_set_list_length);
    for (int i = 0; i < pos_srs_resource_set_list_length; i++) {
      f1ap_pos_srs_resource_set_item_t *pos_srs_resource_set = &pos_srs_resource_set_list->pos_srs_resource_set_item[i];
      f1ap_pos_srs_resource_set_item_t *f1_pos_srs_resource_set = &f1_pos_srs_resource_set_list->pos_srs_resource_set_item[i];
      _EQ_CHECK_INT(f1_pos_srs_resource_set->pos_srs_resource_set_id, pos_srs_resource_set->pos_srs_resource_set_id);
      uint8_t pos_srs_resource_id_list_length = pos_srs_resource_set->pos_srs_resource_id_list.pos_srs_resource_id_list_length;
      _EQ_CHECK_INT(f1_pos_srs_resource_set->pos_srs_resource_id_list.pos_srs_resource_id_list_length,
                    pos_srs_resource_id_list_length);
      for (int j = 0; j < pos_srs_resource_id_list_length; j++) {
        _EQ_CHECK_LONG(f1_pos_srs_resource_set->pos_srs_resource_id_list.srs_pos_resource_id[j],
                       pos_srs_resource_set->pos_srs_resource_id_list.srs_pos_resource_id[j]);
      }

      f1ap_pos_resource_set_type_t *f1_pos_res_set_type = &f1_pos_srs_resource_set->pos_resource_set_type;
      f1ap_pos_resource_set_type_t *pos_res_set_type = &pos_srs_resource_set->pos_resource_set_type;
      _EQ_CHECK_INT(f1_pos_res_set_type->present, pos_res_set_type->present);
      switch (pos_res_set_type->present) {
        case F1AP_POS_RESOURCE_SET_TYPE_PR_NOTHING:
          // nothing to check
          break;
        case F1AP_POS_RESOURCE_SET_TYPE_PR_PERIODIC:
          _EQ_CHECK_INT(f1_pos_res_set_type->choice.periodic, pos_res_set_type->choice.periodic);
          break;
        case F1AP_POS_RESOURCE_SET_TYPE_PR_SEMI_PERSISTENT:
          _EQ_CHECK_INT(f1_pos_res_set_type->choice.semi_persistent, pos_res_set_type->choice.semi_persistent);
          break;
        case F1AP_POS_RESOURCE_SET_TYPE_PR_APERIODIC:
          _EQ_CHECK_INT(f1_pos_res_set_type->choice.srs_resource, pos_res_set_type->choice.srs_resource);
          break;
        default:
          AssertFatal(false, "illegal resource set type pos %d\n", pos_res_set_type->present);
          break;
      }
    }
  }
  return true;
}

static bool eq_srs_carrier_list_item(const f1ap_srs_carrier_list_item_t *f1_srs_carrier_list_item,
                                     const f1ap_srs_carrier_list_item_t *srs_carrier_list_item)
{
  // pointA
  _EQ_CHECK_INT(f1_srs_carrier_list_item->pointA, srs_carrier_list_item->pointA);

  // Uplink Channel BW-PerSCS-List
  const f1ap_uplink_channel_bw_per_scs_list_t *uplink_channel_bw_per_scs_list =
      &srs_carrier_list_item->uplink_channel_bw_per_scs_list;
  const f1ap_uplink_channel_bw_per_scs_list_t *f1_uplink_channel_bw_per_scs_list =
      &f1_srs_carrier_list_item->uplink_channel_bw_per_scs_list;

  uint32_t scs_specific_carrier_list_length = uplink_channel_bw_per_scs_list->scs_specific_carrier_list_length;
  _EQ_CHECK_INT(f1_uplink_channel_bw_per_scs_list->scs_specific_carrier_list_length, scs_specific_carrier_list_length);
  for (int i = 0; i < scs_specific_carrier_list_length; i++) {
    f1ap_scs_specific_carrier_t *scs_specific_carrier = &uplink_channel_bw_per_scs_list->scs_specific_carrier[i];
    f1ap_scs_specific_carrier_t *f1_scs_specific_carrier = &f1_uplink_channel_bw_per_scs_list->scs_specific_carrier[i];
    // offset to carrier
    _EQ_CHECK_INT(f1_scs_specific_carrier->offset_to_carrier, scs_specific_carrier->offset_to_carrier);
    // subcarrier spacing
    _EQ_CHECK_INT(f1_scs_specific_carrier->subcarrier_spacing, scs_specific_carrier->subcarrier_spacing);
    // carrier bandwidth
    _EQ_CHECK_INT(f1_scs_specific_carrier->carrier_bandwidth, scs_specific_carrier->carrier_bandwidth);
  }

  // Active UL BWP
  const f1ap_active_ul_bwp_t *f1_active_ul_bwp = &f1_srs_carrier_list_item->active_ul_bwp;
  const f1ap_active_ul_bwp_t *active_ul_bwp = &srs_carrier_list_item->active_ul_bwp;

  // location and bandwidth
  _EQ_CHECK_INT(f1_active_ul_bwp->location_and_bandwidth, active_ul_bwp->location_and_bandwidth);
  // subcarrier spacing
  _EQ_CHECK_INT(f1_active_ul_bwp->subcarrier_spacing, active_ul_bwp->subcarrier_spacing);

  // cyclic prefix
  _EQ_CHECK_INT(f1_active_ul_bwp->cyclic_prefix, active_ul_bwp->cyclic_prefix);

  // Tx Direct Current Location
  _EQ_CHECK_INT(f1_active_ul_bwp->tx_direct_current_location, active_ul_bwp->tx_direct_current_location);

  // SRS Config
  const f1ap_srs_config_t *f1_sRSConfig = &f1_active_ul_bwp->srs_config;
  const f1ap_srs_config_t *sRSConfig = &active_ul_bwp->srs_config;
  _F1_CHECK_EXP(eq_srs_config(f1_sRSConfig, sRSConfig));
  return true;
}

static bool eq_srs_carrier_list(const f1ap_srs_carrier_list_t *srs_carrier_list_a,
                                const f1ap_srs_carrier_list_t *srs_carrier_list_b)
{
  uint32_t srs_carrier_list_len = srs_carrier_list_a->srs_carrier_list_length;
  _EQ_CHECK_INT(srs_carrier_list_b->srs_carrier_list_length, srs_carrier_list_len);
  for (int i = 0; i < srs_carrier_list_len; i++) {
    f1ap_srs_carrier_list_item_t *srs_carrier_list_item_a = &srs_carrier_list_a->srs_carrier_list_item[i];
    f1ap_srs_carrier_list_item_t *srs_carrier_list_item_b = &srs_carrier_list_b->srs_carrier_list_item[i];
    _F1_CHECK_EXP(eq_srs_carrier_list_item(srs_carrier_list_item_a, srs_carrier_list_item_b));
  }
  return true;
}

static void free_srs_carrier_list(f1ap_srs_carrier_list_t *srs_carrier_list)
{
  uint32_t srs_carrier_list_len = srs_carrier_list->srs_carrier_list_length;
  for (int i = 0; i < srs_carrier_list_len; i++) {
    f1ap_srs_carrier_list_item_t *srs_carrier_list_item = &srs_carrier_list->srs_carrier_list_item[i];
    free(srs_carrier_list_item->uplink_channel_bw_per_scs_list.scs_specific_carrier);

    f1ap_active_ul_bwp_t *active_ul_bwp = &srs_carrier_list_item->active_ul_bwp;
    f1ap_srs_config_t *sRSConfig = &active_ul_bwp->srs_config;
    if (sRSConfig->srs_resource_list) {
      f1ap_srs_resource_list_t *srs_resource_list = sRSConfig->srs_resource_list;
      free(srs_resource_list->srs_resource);
      free(sRSConfig->srs_resource_list);
    }
    if (sRSConfig->pos_srs_resource_list) {
      f1ap_pos_srs_resource_list_t *pos_srs_resource_list = sRSConfig->pos_srs_resource_list;
      free(pos_srs_resource_list->pos_srs_resource_item);
      free(sRSConfig->pos_srs_resource_list);
    }
    if (sRSConfig->srs_resource_set_list) {
      f1ap_srs_resource_set_list_t *srs_resource_set_list = sRSConfig->srs_resource_set_list;
      uint32_t srs_resource_set_list_length = srs_resource_set_list->srs_resource_set_list_length;
      for (int j = 0; j < srs_resource_set_list_length; j++) {
        f1ap_srs_resource_set_t *srs_resource_set = &srs_resource_set_list->srs_resource_set[j];
        free(srs_resource_set->srs_resource_id_list.srs_resource_id);
      }
      free(srs_resource_set_list->srs_resource_set);
      free(sRSConfig->srs_resource_set_list);
    }
    if (sRSConfig->pos_srs_resource_set_list) {
      f1ap_pos_srs_resource_set_list_t *pos_srs_resource_set_list = sRSConfig->pos_srs_resource_set_list;
      uint32_t pos_srs_resource_set_list_length = pos_srs_resource_set_list->pos_srs_resource_set_list_length;
      for (int j = 0; j < pos_srs_resource_set_list_length; j++) {
        f1ap_pos_srs_resource_set_item_t *pos_srs_resource_set = &pos_srs_resource_set_list->pos_srs_resource_set_item[j];
        free(pos_srs_resource_set->pos_srs_resource_id_list.srs_pos_resource_id);
      }
      free(pos_srs_resource_set_list->pos_srs_resource_set_item);
      free(sRSConfig->pos_srs_resource_set_list);
    }
  }
  free(srs_carrier_list->srs_carrier_list_item);
}

static F1AP_SRSType_t encode_f1ap_srstype(const f1ap_srs_type_t *srs_type)
{
  F1AP_SRSType_t f1_srstype = {0};
  switch (srs_type->present) {
    case F1AP_SRS_TYPE_PR_NOTHING:
      f1_srstype.present = F1AP_SRSType_PR_NOTHING;
      break;
    case F1AP_SRS_TYPE_PR_SEMIPERSISTENTSRS:
      f1_srstype.present = F1AP_SRSType_PR_semipersistentSRS;
      asn1cCalloc(f1_srstype.choice.semipersistentSRS, sp_srs);
      sp_srs->sRSResourceSetID = *srs_type->choice.srs_resource_set_id;
      break;
    case F1AP_SRS_TYPE_PR_APERIODICSRS:
      f1_srstype.present = F1AP_SRSType_PR_aperiodicSRS;
      asn1cCalloc(f1_srstype.choice.aperiodicSRS, ap_srs);
      ap_srs->aperiodic = *srs_type->choice.aperiodic;
      break;
    default:
      AssertFatal(false, "unknown SRS type %d\n", srs_type->present);
      break;
  }
  return f1_srstype;
}

static bool decode_f1ap_srstype(const F1AP_SRSType_t *srs_type, f1ap_srs_type_t *out)
{
  switch (srs_type->present) {
    case F1AP_SRSType_PR_NOTHING:
      out->present = F1AP_SRS_TYPE_PR_NOTHING;
      break;
    case F1AP_SRSType_PR_semipersistentSRS:
      out->present = F1AP_SRS_TYPE_PR_SEMIPERSISTENTSRS;
      out->choice.srs_resource_set_id = calloc_or_fail(1, sizeof(*out->choice.srs_resource_set_id));
      *out->choice.srs_resource_set_id = srs_type->choice.semipersistentSRS->sRSResourceSetID;
      break;
    case F1AP_SRSType_PR_aperiodicSRS:
      out->present = F1AP_SRS_TYPE_PR_APERIODICSRS;
      out->choice.aperiodic = calloc_or_fail(1, sizeof(*out->choice.aperiodic));
      *out->choice.aperiodic = srs_type->choice.aperiodicSRS->aperiodic;
      break;
    default:
      PRINT_ERROR("received illegal SRS type %d\n", srs_type->present);
      return false;
      break;
  }
  return true;
}

static F1AP_AbortTransmission_t encode_f1ap_abort_transmission(const f1ap_abort_transmission_t *abort_tx)
{
  F1AP_AbortTransmission_t f1_abort_tx = {0};
  switch (abort_tx->present) {
    case F1AP_ABORT_TRANSMISSION_PR_NOTHING:
      f1_abort_tx.present = F1AP_AbortTransmission_PR_NOTHING;
      break;
    case F1AP_ABORT_TRANSMISSION_PR_SRSRESOURCESETID:
      f1_abort_tx.present = F1AP_AbortTransmission_PR_sRSResourceSetID;
      f1_abort_tx.choice.sRSResourceSetID = abort_tx->choice.srs_resource_set_id;
      break;
    case F1AP_ABORT_TRANSMISSION_PR_RELEASEALL:
      f1_abort_tx.present = F1AP_AbortTransmission_PR_releaseALL;
      break;
    default:
      AssertFatal(false, "unknown Abort Transmission %d\n", abort_tx->present);
      break;
  }
  return f1_abort_tx;
}

static bool decode_f1ap_abort_transmission(const F1AP_AbortTransmission_t *in, f1ap_abort_transmission_t *out)
{
  switch (in->present) {
    case F1AP_AbortTransmission_PR_NOTHING:
      out->present = F1AP_ABORT_TRANSMISSION_PR_NOTHING;
      break;
    case F1AP_AbortTransmission_PR_sRSResourceSetID:
      out->present = F1AP_ABORT_TRANSMISSION_PR_SRSRESOURCESETID;
      out->choice.srs_resource_set_id = in->choice.sRSResourceSetID;
      break;
    case F1AP_AbortTransmission_PR_releaseALL:
      out->present = F1AP_ABORT_TRANSMISSION_PR_RELEASEALL;
      out->choice.release_all = true;
      break;
    default:
      AssertError(false, return false, "received illegal Abort Transmission %d\n", in->present);
      break;
  }
  return true;
}

static F1AP_TRPReferencePointType_t encode_reference_point_type(const f1ap_trp_reference_point_type_t *in)
{
  F1AP_TRPReferencePointType_t out = {0};
  switch (in->present) {
    case F1AP_TRP_REFERENCE_POINT_TYPE_PR_TRPPOSITION_RELATIVE_GEODETIC:
      out.present = F1AP_TRPReferencePointType_PR_tRPPositionRelativeGeodetic;
      asn1cCalloc(out.choice.tRPPositionRelativeGeodetic, f1_tRPPositionRelativeGeodetic);
      const f1ap_relative_geodetic_location_t *tRPPositionRelativeGeodetic = &in->choice.trp_position_relative_geodetic;
      f1_tRPPositionRelativeGeodetic->milli_Arc_SecondUnits = tRPPositionRelativeGeodetic->milli_arc_second_units;
      f1_tRPPositionRelativeGeodetic->heightUnits = tRPPositionRelativeGeodetic->height_units;
      f1_tRPPositionRelativeGeodetic->deltaLatitude = tRPPositionRelativeGeodetic->delta_latitude;
      f1_tRPPositionRelativeGeodetic->deltaLongitude = tRPPositionRelativeGeodetic->delta_longitude;
      f1_tRPPositionRelativeGeodetic->deltaHeight = tRPPositionRelativeGeodetic->delta_height;

      F1AP_LocationUncertainty_t *f1_locationUncertainty_g = (f1_tRPPositionRelativeGeodetic->locationUncertainty = calloc_or_fail(1, sizeof(*f1_tRPPositionRelativeGeodetic->locationUncertainty)));
      const f1ap_location_uncertainty_t *locationUncertainty_g = &tRPPositionRelativeGeodetic->location_uncertainty;
      f1_locationUncertainty_g->horizontalUncertainty = locationUncertainty_g->horizontal_uncertainty;
      f1_locationUncertainty_g->horizontalConfidence = locationUncertainty_g->horizontal_confidence;
      f1_locationUncertainty_g->verticalUncertainty = locationUncertainty_g->vertical_uncertainty;
      f1_locationUncertainty_g->verticalConfidence = locationUncertainty_g->vertical_confidence;
      break;
    case F1AP_TRP_REFERENCE_POINT_TYPE_PR_TRPPOSITION_RELATIVE_CARTESIAN:
      out.present = F1AP_TRPReferencePointType_PR_tRPPositionRelativeCartesian;
      asn1cCalloc(out.choice.tRPPositionRelativeCartesian, f1_tRPPositionRelativeCartesian);
      const f1ap_relative_cartesian_location_t *tRPPositionRelativeCartesian = &in->choice.trp_position_relative_cartesian;
      f1_tRPPositionRelativeCartesian->xYZunit = tRPPositionRelativeCartesian->xyz_unit;
      f1_tRPPositionRelativeCartesian->xvalue = tRPPositionRelativeCartesian->xvalue;
      f1_tRPPositionRelativeCartesian->yvalue = tRPPositionRelativeCartesian->yvalue;
      f1_tRPPositionRelativeCartesian->zvalue = tRPPositionRelativeCartesian->zvalue;

      F1AP_LocationUncertainty_t *f1_locationUncertainty_c = (f1_tRPPositionRelativeCartesian->locationUncertainty = calloc_or_fail(1, sizeof(*f1_tRPPositionRelativeCartesian->locationUncertainty)));
      const f1ap_location_uncertainty_t *locationUncertainty_c = &tRPPositionRelativeCartesian->location_uncertainty;
      f1_locationUncertainty_c->horizontalUncertainty = locationUncertainty_c->horizontal_uncertainty;
      f1_locationUncertainty_c->horizontalConfidence = locationUncertainty_c->horizontal_confidence;
      f1_locationUncertainty_c->verticalUncertainty = locationUncertainty_c->vertical_uncertainty;
      f1_locationUncertainty_c->verticalConfidence = locationUncertainty_c->vertical_confidence;
      break;
    default:
      AssertFatal(false, "illegal trp reference point type entry %d\n", in->present);
      break;
  }
  return out;
}

static F1AP_NGRANHighAccuracyAccessPointPosition_t encode_trp_ha_pos(const f1ap_ngran_high_accuracy_access_point_position_t *in)
{
  F1AP_NGRANHighAccuracyAccessPointPosition_t out = {0};
  out.latitude = in->latitude;
  out.longitude = in->longitude;
  out.altitude = in->altitude;
  out.uncertaintySemi_major = in->uncertainty_semi_major;
  out.uncertaintySemi_minor = in->uncertainty_semi_minor;
  out.orientationOfMajorAxis = in->orientation_of_major_axis;
  out.horizontalConfidence = in->horizontal_confidence;
  out.uncertaintyAltitude = in->uncertainty_altitude;
  out.verticalConfidence = in->vertical_confidence;
  return out;
}

static F1AP_GeographicalCoordinates_t encode_geographical_coordinates(const f1ap_geographical_coordinates_t *in)
{
  F1AP_GeographicalCoordinates_t out = {0};
  F1AP_TRPPositionDefinitionType_t *f1_trp_pos_def_type = (out.tRPPositionDefinitionType = calloc_or_fail(1, sizeof(*out.tRPPositionDefinitionType)));
  const f1ap_trp_position_definition_type_t *trp_pos_def_type = &in->trp_position_definition_type;
  switch (trp_pos_def_type->present) {
    case F1AP_TRP_POSITION_DEFINITION_TYPE_PR_NOTHING:
      f1_trp_pos_def_type->present = F1AP_TRPPositionDefinitionType_PR_NOTHING;
      break;
    case F1AP_TRP_POSITION_DEFINITION_TYPE_PR_DIRECT:
      f1_trp_pos_def_type->present = F1AP_TRPPositionDefinitionType_PR_direct;
      asn1cCalloc(f1_trp_pos_def_type->choice.direct, f1_direct);
      const f1ap_trp_position_direct_t *direct = &trp_pos_def_type->choice.direct;

      if (direct->accuracy.present == F1AP_TRP_POSITION_DIRECT_ACCURACY_PR_TRPPOSITION) {
        if (!f1_direct->accuracy)
          f1_direct->accuracy = calloc_or_fail(1, sizeof(*f1_direct->accuracy));
        f1_direct->accuracy->present = F1AP_TRPPositionDirectAccuracy_PR_tRPPosition;
        const f1ap_access_point_position_t *trp_pos = &direct->accuracy.choice.trp_position;
        F1AP_AccessPointPosition_t *f1_trp_pos = f1_direct->accuracy->choice.tRPPosition;
        f1_trp_pos->latitudeSign = trp_pos->latitude_sign;
        f1_trp_pos->latitude = trp_pos->latitude;
        f1_trp_pos->longitude = trp_pos->longitude;
        f1_trp_pos->directionOfAltitude = trp_pos->direction_of_altitude;
        f1_trp_pos->altitude = trp_pos->altitude;
        f1_trp_pos->uncertaintySemi_major = trp_pos->uncertainty_semi_major;
        f1_trp_pos->uncertaintySemi_minor = trp_pos->uncertainty_semi_minor;
        f1_trp_pos->orientationOfMajorAxis = trp_pos->orientation_of_major_axis;
        f1_trp_pos->uncertaintyAltitude = trp_pos->uncertainty_altitude;
        f1_trp_pos->confidence = trp_pos->confidence;
      } else if (direct->accuracy.present == F1AP_TRP_POSITION_DIRECT_ACCURACY_PR_TRPHAPOSITION) {
        if (!f1_direct->accuracy)
          f1_direct->accuracy = calloc_or_fail(1, sizeof(*f1_direct->accuracy));
        f1_direct->accuracy->present = F1AP_TRPPositionDirectAccuracy_PR_tRPHAposition;
        const f1ap_ngran_high_accuracy_access_point_position_t *trp_ha_pos = &direct->accuracy.choice.trp_HAposition;
        F1AP_NGRANHighAccuracyAccessPointPosition_t *f1_trp_ha_pos = f1_direct->accuracy->choice.tRPHAposition;
        *f1_trp_ha_pos = encode_trp_ha_pos(trp_ha_pos);
      } else {
        AssertFatal(false, "illegal direct accuracy entry %d\n", direct->accuracy.present);
      }
      break;
    case F1AP_TRP_POSITION_DEFINITION_TYPE_PR_REFERENCED:
      f1_trp_pos_def_type->present = F1AP_TRPPositionDefinitionType_PR_referenced;
      asn1cCalloc(f1_trp_pos_def_type->choice.referenced, f1_referenced);
      const f1ap_trp_position_referenced_t *referenced = &trp_pos_def_type->choice.referenced;
      F1AP_ReferencePoint_t *f1_referencePoint = (f1_referenced->referencePoint = calloc_or_fail(1, sizeof(*f1_referenced->referencePoint)));
      const f1ap_reference_point_t *referencePoint = &referenced->reference_point;

      if (referencePoint->present == F1AP_REFERENCE_POINT_PR_COORDINATEID) {
        f1_referencePoint->present = F1AP_ReferencePoint_PR_coordinateID;
        f1_referencePoint->choice.coordinateID = referencePoint->choice.coordinate_id;
      } else if (referencePoint->present == F1AP_REFERENCE_POINT_PR_REFERENCEPOINTCOORDINATE) {
        f1_referencePoint->present = F1AP_ReferencePoint_PR_referencePointCoordinate;
        const f1ap_access_point_position_t *referencePointCoordinate = &referencePoint->choice.reference_point_coordinate;
        asn1cCalloc(f1_referencePoint->choice.referencePointCoordinate, f1_referencePointCoordinate);
        f1_referencePointCoordinate->latitudeSign = referencePointCoordinate->latitude_sign;
        f1_referencePointCoordinate->latitude = referencePointCoordinate->latitude;
        f1_referencePointCoordinate->longitude = referencePointCoordinate->longitude;
        f1_referencePointCoordinate->directionOfAltitude = referencePointCoordinate->direction_of_altitude;
        f1_referencePointCoordinate->altitude = referencePointCoordinate->altitude;
        f1_referencePointCoordinate->uncertaintySemi_major = referencePointCoordinate->uncertainty_semi_major;
        f1_referencePointCoordinate->uncertaintySemi_minor = referencePointCoordinate->uncertainty_semi_minor;
        f1_referencePointCoordinate->orientationOfMajorAxis = referencePointCoordinate->orientation_of_major_axis;
        f1_referencePointCoordinate->uncertaintyAltitude = referencePointCoordinate->uncertainty_altitude;
        f1_referencePointCoordinate->confidence = referencePointCoordinate->confidence;
      } else if (referencePoint->present == F1AP_REFERENCE_POINT_PR_REFERENCEPOINTCOORDINATEHA) {
        f1_referencePoint->present = F1AP_ReferencePoint_PR_referencePointCoordinateHA;
        const f1ap_ngran_high_accuracy_access_point_position_t *referencePointCoordinateHA =
            &referencePoint->choice.reference_point_coordinateHA;
        asn1cCalloc(f1_referencePoint->choice.referencePointCoordinateHA, f1_referencePointCoordinateHA);
        *f1_referencePointCoordinateHA = encode_trp_ha_pos(referencePointCoordinateHA);
      } else {
        AssertFatal(false, "illegal reference point entry %d\n", referencePoint->present);
      }

      if (!f1_referenced->referencePointType)
        f1_referenced->referencePointType = calloc_or_fail(1, sizeof(*f1_referenced->referencePointType));
      F1AP_TRPReferencePointType_t *f1_referencePointType = f1_referenced->referencePointType;
      const f1ap_trp_reference_point_type_t *referencePointType = &referenced->reference_point_type;
      *f1_referencePointType = encode_reference_point_type(referencePointType);
      break;
    default:
      AssertFatal(false, "illegal Geographical Coordinates entry\n");
      break;
  }
  return out;
}

static void decode_reference_point_type(const F1AP_TRPReferencePointType_t *in, f1ap_trp_reference_point_type_t *out)
{
  switch (in->present) {
    case F1AP_TRPReferencePointType_PR_tRPPositionRelativeGeodetic:
      out->present = F1AP_TRP_REFERENCE_POINT_TYPE_PR_TRPPOSITION_RELATIVE_GEODETIC;
      F1AP_RelativeGeodeticLocation_t *f1_tRPPositionRelativeGeodetic = in->choice.tRPPositionRelativeGeodetic;
      f1ap_relative_geodetic_location_t *tRPPositionRelativeGeodetic = &out->choice.trp_position_relative_geodetic;
      tRPPositionRelativeGeodetic->milli_arc_second_units = f1_tRPPositionRelativeGeodetic->milli_Arc_SecondUnits;
      tRPPositionRelativeGeodetic->height_units = f1_tRPPositionRelativeGeodetic->heightUnits;
      tRPPositionRelativeGeodetic->delta_latitude = f1_tRPPositionRelativeGeodetic->deltaLatitude;
      tRPPositionRelativeGeodetic->delta_longitude = f1_tRPPositionRelativeGeodetic->deltaLongitude;
      tRPPositionRelativeGeodetic->delta_height = f1_tRPPositionRelativeGeodetic->deltaHeight;

      F1AP_LocationUncertainty_t *f1_locationUncertainty_g = f1_tRPPositionRelativeGeodetic->locationUncertainty;
      f1ap_location_uncertainty_t *locationUncertainty_g = &tRPPositionRelativeGeodetic->location_uncertainty;
      locationUncertainty_g->horizontal_uncertainty = f1_locationUncertainty_g->horizontalUncertainty;
      locationUncertainty_g->horizontal_confidence = f1_locationUncertainty_g->horizontalConfidence;
      locationUncertainty_g->vertical_uncertainty = f1_locationUncertainty_g->verticalUncertainty;
      locationUncertainty_g->vertical_confidence = f1_locationUncertainty_g->verticalConfidence;
      break;
    case F1AP_TRPReferencePointType_PR_tRPPositionRelativeCartesian:
      out->present = F1AP_TRP_REFERENCE_POINT_TYPE_PR_TRPPOSITION_RELATIVE_CARTESIAN;
      F1AP_RelativeCartesianLocation_t *f1_tRPPositionRelativeCartesian = in->choice.tRPPositionRelativeCartesian;
      f1ap_relative_cartesian_location_t *tRPPositionRelativeCartesian = &out->choice.trp_position_relative_cartesian;
      tRPPositionRelativeCartesian->xyz_unit = f1_tRPPositionRelativeCartesian->xYZunit;
      tRPPositionRelativeCartesian->xvalue = f1_tRPPositionRelativeCartesian->xvalue;
      tRPPositionRelativeCartesian->yvalue = f1_tRPPositionRelativeCartesian->yvalue;
      tRPPositionRelativeCartesian->zvalue = f1_tRPPositionRelativeCartesian->zvalue;

      F1AP_LocationUncertainty_t *f1_locationUncertainty_c = f1_tRPPositionRelativeCartesian->locationUncertainty;
      f1ap_location_uncertainty_t *locationUncertainty_c = &tRPPositionRelativeCartesian->location_uncertainty;
      locationUncertainty_c->horizontal_uncertainty = f1_locationUncertainty_c->horizontalUncertainty;
      locationUncertainty_c->horizontal_confidence = f1_locationUncertainty_c->horizontalConfidence;
      locationUncertainty_c->vertical_uncertainty = f1_locationUncertainty_c->verticalUncertainty;
      locationUncertainty_c->vertical_confidence = f1_locationUncertainty_c->verticalConfidence;
      break;
    default:
      AssertFatal(false, "illegal trp reference point type entry %d\n", in->present);
      break;
  }
}

static void decode_trp_ha_pos(const F1AP_NGRANHighAccuracyAccessPointPosition_t *in,
                              f1ap_ngran_high_accuracy_access_point_position_t *out)
{
  out->latitude = in->latitude;
  out->longitude = in->longitude;
  out->altitude = in->altitude;
  out->uncertainty_semi_major = in->uncertaintySemi_major;
  out->uncertainty_semi_minor = in->uncertaintySemi_minor;
  out->orientation_of_major_axis = in->orientationOfMajorAxis;
  out->horizontal_confidence = in->horizontalConfidence;
  out->uncertainty_altitude = in->uncertaintyAltitude;
  out->vertical_confidence = in->verticalConfidence;
}

static void decode_geographical_coordinates(const F1AP_GeographicalCoordinates_t *in, f1ap_geographical_coordinates_t *out)
{
  const F1AP_TRPPositionDefinitionType_t *f1_trp_pos_def_type = in->tRPPositionDefinitionType;
  f1ap_trp_position_definition_type_t *trp_pos_def_type = &out->trp_position_definition_type;
  switch (f1_trp_pos_def_type->present) {
    case F1AP_TRPPositionDefinitionType_PR_NOTHING:
      trp_pos_def_type->present = F1AP_TRP_POSITION_DEFINITION_TYPE_PR_NOTHING;
      break;
    case F1AP_TRPPositionDefinitionType_PR_direct:
      trp_pos_def_type->present = F1AP_TRP_POSITION_DEFINITION_TYPE_PR_DIRECT;
      F1AP_TRPPositionDirect_t *f1_direct = f1_trp_pos_def_type->choice.direct;
      f1ap_trp_position_direct_t *direct = &trp_pos_def_type->choice.direct;

      if (f1_direct->accuracy->present == F1AP_TRPPositionDirectAccuracy_PR_tRPPosition) {
        direct->accuracy.present = F1AP_TRP_POSITION_DIRECT_ACCURACY_PR_TRPPOSITION;
        f1ap_access_point_position_t *trp_pos = &direct->accuracy.choice.trp_position;
        F1AP_AccessPointPosition_t *f1_trp_pos = f1_direct->accuracy->choice.tRPPosition;
        trp_pos->latitude_sign = f1_trp_pos->latitudeSign;
        trp_pos->latitude = f1_trp_pos->latitude;
        trp_pos->longitude = f1_trp_pos->longitude;
        trp_pos->direction_of_altitude = f1_trp_pos->directionOfAltitude;
        trp_pos->altitude = f1_trp_pos->altitude;
        trp_pos->uncertainty_semi_major = f1_trp_pos->uncertaintySemi_major;
        trp_pos->uncertainty_semi_minor = f1_trp_pos->uncertaintySemi_minor;
        trp_pos->orientation_of_major_axis = f1_trp_pos->orientationOfMajorAxis;
        trp_pos->uncertainty_altitude = f1_trp_pos->uncertaintyAltitude;
        trp_pos->confidence = f1_trp_pos->confidence;
      } else if (f1_direct->accuracy->present == F1AP_TRPPositionDirectAccuracy_PR_tRPHAposition) {
        direct->accuracy.present = F1AP_TRP_POSITION_DIRECT_ACCURACY_PR_TRPHAPOSITION;
        f1ap_ngran_high_accuracy_access_point_position_t *trp_ha_pos = &direct->accuracy.choice.trp_HAposition;
        F1AP_NGRANHighAccuracyAccessPointPosition_t *f1_trp_ha_pos = f1_direct->accuracy->choice.tRPHAposition;
        decode_trp_ha_pos(f1_trp_ha_pos, trp_ha_pos);
      } else {
        AssertFatal(false, "illegal direct accuracy entry %d\n", direct->accuracy.present);
      }
      break;
    case F1AP_TRPPositionDefinitionType_PR_referenced:
      trp_pos_def_type->present = F1AP_TRP_POSITION_DEFINITION_TYPE_PR_REFERENCED;
      F1AP_TRPPositionReferenced_t *f1_referenced = f1_trp_pos_def_type->choice.referenced;
      f1ap_trp_position_referenced_t *referenced = &trp_pos_def_type->choice.referenced;
      F1AP_ReferencePoint_t *f1_referencePoint = f1_referenced->referencePoint;
      f1ap_reference_point_t *referencePoint = &referenced->reference_point;

      if (f1_referencePoint->present == F1AP_ReferencePoint_PR_coordinateID) {
        referencePoint->present = F1AP_REFERENCE_POINT_PR_COORDINATEID;
        referencePoint->choice.coordinate_id = f1_referencePoint->choice.coordinateID;
      } else if (f1_referencePoint->present == F1AP_ReferencePoint_PR_referencePointCoordinate) {
        referencePoint->present = F1AP_REFERENCE_POINT_PR_REFERENCEPOINTCOORDINATE;
        f1ap_access_point_position_t *referencePointCoordinate = &referencePoint->choice.reference_point_coordinate;
        F1AP_AccessPointPosition_t *f1_referencePointCoordinate = f1_referencePoint->choice.referencePointCoordinate;
        referencePointCoordinate->latitude_sign = f1_referencePointCoordinate->latitudeSign;
        referencePointCoordinate->latitude = f1_referencePointCoordinate->latitude;
        referencePointCoordinate->longitude = f1_referencePointCoordinate->longitude;
        referencePointCoordinate->direction_of_altitude = f1_referencePointCoordinate->directionOfAltitude;
        referencePointCoordinate->altitude = f1_referencePointCoordinate->altitude;
        referencePointCoordinate->uncertainty_semi_major = f1_referencePointCoordinate->uncertaintySemi_major;
        referencePointCoordinate->uncertainty_semi_minor = f1_referencePointCoordinate->uncertaintySemi_minor;
        referencePointCoordinate->orientation_of_major_axis = f1_referencePointCoordinate->orientationOfMajorAxis;
        referencePointCoordinate->uncertainty_altitude = f1_referencePointCoordinate->uncertaintyAltitude;
        referencePointCoordinate->confidence = f1_referencePointCoordinate->confidence;
      } else if (f1_referencePoint->present == F1AP_ReferencePoint_PR_referencePointCoordinateHA) {
        referencePoint->present = F1AP_REFERENCE_POINT_PR_REFERENCEPOINTCOORDINATEHA;
        f1ap_ngran_high_accuracy_access_point_position_t *referencePointCoordinateHA =
            &referencePoint->choice.reference_point_coordinateHA;
        F1AP_NGRANHighAccuracyAccessPointPosition_t *f1_referencePointCoordinateHA =
            f1_referencePoint->choice.referencePointCoordinateHA;
        decode_trp_ha_pos(f1_referencePointCoordinateHA, referencePointCoordinateHA);
      } else {
        AssertFatal(false, "illegal reference point entry %d\n", referencePoint->present);
      }

      F1AP_TRPReferencePointType_t *f1_referencePointType = f1_referenced->referencePointType;
      f1ap_trp_reference_point_type_t *referencePointType = &referenced->reference_point_type;
      decode_reference_point_type(f1_referencePointType, referencePointType);
      break;
    default:
      AssertFatal(false, "illegal Geographical Coordinates entry\n");
      break;
  }
}

static f1ap_trp_reference_point_type_t cp_encode_reference_point_type(const f1ap_trp_reference_point_type_t *in)
{
  f1ap_trp_reference_point_type_t out = {0};
  switch (in->present) {
    case F1AP_TRP_REFERENCE_POINT_TYPE_PR_TRPPOSITION_RELATIVE_GEODETIC:
      out.present = F1AP_TRP_REFERENCE_POINT_TYPE_PR_TRPPOSITION_RELATIVE_GEODETIC;
      const f1ap_relative_geodetic_location_t *f1_tRPPositionRelativeGeodetic = &in->choice.trp_position_relative_geodetic;
      f1ap_relative_geodetic_location_t *tRPPositionRelativeGeodetic = &out.choice.trp_position_relative_geodetic;
      tRPPositionRelativeGeodetic->milli_arc_second_units = f1_tRPPositionRelativeGeodetic->milli_arc_second_units;
      tRPPositionRelativeGeodetic->height_units = f1_tRPPositionRelativeGeodetic->height_units;
      tRPPositionRelativeGeodetic->delta_latitude = f1_tRPPositionRelativeGeodetic->delta_latitude;
      tRPPositionRelativeGeodetic->delta_longitude = f1_tRPPositionRelativeGeodetic->delta_longitude;
      tRPPositionRelativeGeodetic->delta_height = f1_tRPPositionRelativeGeodetic->delta_height;

      const f1ap_location_uncertainty_t *f1_locationUncertainty_g = &f1_tRPPositionRelativeGeodetic->location_uncertainty;
      f1ap_location_uncertainty_t *locationUncertainty_g = &tRPPositionRelativeGeodetic->location_uncertainty;
      locationUncertainty_g->horizontal_uncertainty = f1_locationUncertainty_g->horizontal_uncertainty;
      locationUncertainty_g->horizontal_confidence = f1_locationUncertainty_g->horizontal_confidence;
      locationUncertainty_g->vertical_uncertainty = f1_locationUncertainty_g->vertical_uncertainty;
      locationUncertainty_g->vertical_confidence = f1_locationUncertainty_g->vertical_confidence;
      break;
    case F1AP_TRP_REFERENCE_POINT_TYPE_PR_TRPPOSITION_RELATIVE_CARTESIAN:
      out.present = F1AP_TRP_REFERENCE_POINT_TYPE_PR_TRPPOSITION_RELATIVE_CARTESIAN;
      const f1ap_relative_cartesian_location_t *f1_tRPPositionRelativeCartesian = &in->choice.trp_position_relative_cartesian;
      f1ap_relative_cartesian_location_t *tRPPositionRelativeCartesian = &out.choice.trp_position_relative_cartesian;
      tRPPositionRelativeCartesian->xyz_unit = f1_tRPPositionRelativeCartesian->xyz_unit;
      tRPPositionRelativeCartesian->xvalue = f1_tRPPositionRelativeCartesian->xvalue;
      tRPPositionRelativeCartesian->yvalue = f1_tRPPositionRelativeCartesian->yvalue;
      tRPPositionRelativeCartesian->zvalue = f1_tRPPositionRelativeCartesian->zvalue;

      const f1ap_location_uncertainty_t *f1_locationUncertainty_c = &f1_tRPPositionRelativeCartesian->location_uncertainty;
      f1ap_location_uncertainty_t *locationUncertainty_c = &tRPPositionRelativeCartesian->location_uncertainty;
      locationUncertainty_c->horizontal_uncertainty = f1_locationUncertainty_c->horizontal_uncertainty;
      locationUncertainty_c->horizontal_confidence = f1_locationUncertainty_c->horizontal_confidence;
      locationUncertainty_c->vertical_uncertainty = f1_locationUncertainty_c->vertical_uncertainty;
      locationUncertainty_c->vertical_confidence = f1_locationUncertainty_c->vertical_confidence;
      break;
    default:
      AssertFatal(false, "illegal trp reference point type entry %d\n", in->present);
      break;
  }
  return out;
}

static f1ap_ngran_high_accuracy_access_point_position_t cp_trp_ha_pos(const f1ap_ngran_high_accuracy_access_point_position_t *in)
{
  f1ap_ngran_high_accuracy_access_point_position_t out = {0};
  out.latitude = in->latitude;
  out.longitude = in->longitude;
  out.altitude = in->altitude;
  out.uncertainty_semi_major = in->uncertainty_semi_major;
  out.uncertainty_semi_minor = in->uncertainty_semi_minor;
  out.orientation_of_major_axis = in->orientation_of_major_axis;
  out.horizontal_confidence = in->horizontal_confidence;
  out.uncertainty_altitude = in->uncertainty_altitude;
  out.vertical_confidence = in->vertical_confidence;
  return out;
}

static f1ap_geographical_coordinates_t cp_geographical_coordinates(const f1ap_geographical_coordinates_t *in)
{
  f1ap_geographical_coordinates_t out = {0};
  const f1ap_trp_position_definition_type_t *f1_trp_pos_def_type = &in->trp_position_definition_type;
  f1ap_trp_position_definition_type_t *trp_pos_def_type = &out.trp_position_definition_type;
  switch (f1_trp_pos_def_type->present) {
    case F1AP_TRP_POSITION_DEFINITION_TYPE_PR_NOTHING:
      trp_pos_def_type->present = F1AP_TRP_POSITION_DEFINITION_TYPE_PR_NOTHING;
      break;
    case F1AP_TRP_POSITION_DEFINITION_TYPE_PR_DIRECT:
      trp_pos_def_type->present = F1AP_TRP_POSITION_DEFINITION_TYPE_PR_DIRECT;
      const f1ap_trp_position_direct_t *f1_direct = &f1_trp_pos_def_type->choice.direct;
      f1ap_trp_position_direct_t *direct = &trp_pos_def_type->choice.direct;

      if (f1_direct->accuracy.present == F1AP_TRP_POSITION_DIRECT_ACCURACY_PR_TRPPOSITION) {
        direct->accuracy.present = F1AP_TRP_POSITION_DIRECT_ACCURACY_PR_TRPPOSITION;
        f1ap_access_point_position_t *trp_pos = &direct->accuracy.choice.trp_position;
        const f1ap_access_point_position_t *f1_trp_pos = &f1_direct->accuracy.choice.trp_position;
        trp_pos->latitude_sign = f1_trp_pos->latitude_sign;
        trp_pos->latitude = f1_trp_pos->latitude;
        trp_pos->longitude = f1_trp_pos->longitude;
        trp_pos->direction_of_altitude = f1_trp_pos->direction_of_altitude;
        trp_pos->altitude = f1_trp_pos->altitude;
        trp_pos->uncertainty_semi_major = f1_trp_pos->uncertainty_semi_major;
        trp_pos->uncertainty_semi_minor = f1_trp_pos->uncertainty_semi_minor;
        trp_pos->orientation_of_major_axis = f1_trp_pos->orientation_of_major_axis;
        trp_pos->uncertainty_altitude = f1_trp_pos->uncertainty_altitude;
        trp_pos->confidence = f1_trp_pos->confidence;
      } else if (f1_direct->accuracy.present == F1AP_TRP_POSITION_DIRECT_ACCURACY_PR_TRPHAPOSITION) {
        direct->accuracy.present = F1AP_TRP_POSITION_DIRECT_ACCURACY_PR_TRPHAPOSITION;
        f1ap_ngran_high_accuracy_access_point_position_t *trp_ha_pos = &direct->accuracy.choice.trp_HAposition;
        const f1ap_ngran_high_accuracy_access_point_position_t *f1_trp_ha_pos = &f1_direct->accuracy.choice.trp_HAposition;
        *trp_ha_pos = cp_trp_ha_pos(f1_trp_ha_pos);
      } else {
        AssertFatal(false, "illegal direct accuracy entry %d\n", direct->accuracy.present);
      }
      break;
    case F1AP_TRP_POSITION_DEFINITION_TYPE_PR_REFERENCED:
      trp_pos_def_type->present = F1AP_TRP_POSITION_DEFINITION_TYPE_PR_REFERENCED;
      const f1ap_trp_position_referenced_t *f1_referenced = &f1_trp_pos_def_type->choice.referenced;
      f1ap_trp_position_referenced_t *referenced = &trp_pos_def_type->choice.referenced;
      const f1ap_reference_point_t *f1_referencePoint = &f1_referenced->reference_point;
      f1ap_reference_point_t *referencePoint = &referenced->reference_point;

      if (f1_referencePoint->present == F1AP_REFERENCE_POINT_PR_COORDINATEID) {
        referencePoint->present = F1AP_REFERENCE_POINT_PR_COORDINATEID;
        referencePoint->choice.coordinate_id = f1_referencePoint->choice.coordinate_id;
      } else if (f1_referencePoint->present == F1AP_REFERENCE_POINT_PR_REFERENCEPOINTCOORDINATE) {
        referencePoint->present = F1AP_REFERENCE_POINT_PR_REFERENCEPOINTCOORDINATE;
        f1ap_access_point_position_t *referencePointCoordinate = &referencePoint->choice.reference_point_coordinate;
        const f1ap_access_point_position_t *f1_referencePointCoordinate = &f1_referencePoint->choice.reference_point_coordinate;
        referencePointCoordinate->latitude_sign = f1_referencePointCoordinate->latitude_sign;
        referencePointCoordinate->latitude = f1_referencePointCoordinate->latitude;
        referencePointCoordinate->longitude = f1_referencePointCoordinate->longitude;
        referencePointCoordinate->direction_of_altitude = f1_referencePointCoordinate->direction_of_altitude;
        referencePointCoordinate->altitude = f1_referencePointCoordinate->altitude;
        referencePointCoordinate->uncertainty_semi_major = f1_referencePointCoordinate->uncertainty_semi_major;
        referencePointCoordinate->uncertainty_semi_minor = f1_referencePointCoordinate->uncertainty_semi_minor;
        referencePointCoordinate->orientation_of_major_axis = f1_referencePointCoordinate->orientation_of_major_axis;
        referencePointCoordinate->uncertainty_altitude = f1_referencePointCoordinate->uncertainty_altitude;
        referencePointCoordinate->confidence = f1_referencePointCoordinate->confidence;
      } else if (f1_referencePoint->present == F1AP_REFERENCE_POINT_PR_REFERENCEPOINTCOORDINATEHA) {
        referencePoint->present = F1AP_REFERENCE_POINT_PR_REFERENCEPOINTCOORDINATEHA;
        f1ap_ngran_high_accuracy_access_point_position_t *referencePointCoordinateHA =
            &referencePoint->choice.reference_point_coordinateHA;
        const f1ap_ngran_high_accuracy_access_point_position_t *f1_referencePointCoordinateHA =
            &f1_referencePoint->choice.reference_point_coordinateHA;
        *referencePointCoordinateHA = cp_trp_ha_pos(f1_referencePointCoordinateHA);
      } else {
        AssertFatal(false, "illegal reference point entry %d\n", referencePoint->present);
      }

      const f1ap_trp_reference_point_type_t *f1_referencePointType = &f1_referenced->reference_point_type;
      f1ap_trp_reference_point_type_t *referencePointType = &referenced->reference_point_type;
      *referencePointType = cp_encode_reference_point_type(f1_referencePointType);
      break;
    default:
      AssertFatal(false, "illegal Geographical Coordinates entry\n");
      break;
  }
  return out;
}

static bool eq_reference_point_type(const f1ap_trp_reference_point_type_t *f1_referencePointType,
                                    const f1ap_trp_reference_point_type_t *referencePointType)
{
  _EQ_CHECK_INT(f1_referencePointType->present, referencePointType->present);
  switch (f1_referencePointType->present) {
    case F1AP_TRP_REFERENCE_POINT_TYPE_PR_TRPPOSITION_RELATIVE_GEODETIC: {
      const f1ap_relative_geodetic_location_t *f1_tRPPositionRelativeGeodetic =
          &f1_referencePointType->choice.trp_position_relative_geodetic;
      const f1ap_relative_geodetic_location_t *tRPPositionRelativeGeodetic =
          &referencePointType->choice.trp_position_relative_geodetic;
      _EQ_CHECK_LONG(tRPPositionRelativeGeodetic->milli_arc_second_units, f1_tRPPositionRelativeGeodetic->milli_arc_second_units);
      _EQ_CHECK_LONG(tRPPositionRelativeGeodetic->height_units, f1_tRPPositionRelativeGeodetic->height_units);
      _EQ_CHECK_LONG(tRPPositionRelativeGeodetic->delta_latitude, f1_tRPPositionRelativeGeodetic->delta_latitude);
      _EQ_CHECK_LONG(tRPPositionRelativeGeodetic->delta_longitude, f1_tRPPositionRelativeGeodetic->delta_longitude);
      _EQ_CHECK_LONG(tRPPositionRelativeGeodetic->delta_height, f1_tRPPositionRelativeGeodetic->delta_height);

      const f1ap_location_uncertainty_t *f1_locationUncertainty_g = &f1_tRPPositionRelativeGeodetic->location_uncertainty;
      const f1ap_location_uncertainty_t *locationUncertainty_g = &tRPPositionRelativeGeodetic->location_uncertainty;
      _EQ_CHECK_LONG(locationUncertainty_g->horizontal_uncertainty, f1_locationUncertainty_g->horizontal_uncertainty);
      _EQ_CHECK_LONG(locationUncertainty_g->horizontal_confidence, f1_locationUncertainty_g->horizontal_confidence);
      _EQ_CHECK_LONG(locationUncertainty_g->vertical_uncertainty, f1_locationUncertainty_g->vertical_uncertainty);
      _EQ_CHECK_LONG(locationUncertainty_g->vertical_confidence, f1_locationUncertainty_g->vertical_confidence);
      break;
    }
    case F1AP_TRP_REFERENCE_POINT_TYPE_PR_TRPPOSITION_RELATIVE_CARTESIAN: {
      const f1ap_relative_cartesian_location_t *f1_tRPPositionRelativeCartesian =
          &f1_referencePointType->choice.trp_position_relative_cartesian;
      const f1ap_relative_cartesian_location_t *tRPPositionRelativeCartesian =
          &referencePointType->choice.trp_position_relative_cartesian;
      _EQ_CHECK_LONG(tRPPositionRelativeCartesian->xyz_unit, f1_tRPPositionRelativeCartesian->xyz_unit);
      _EQ_CHECK_LONG(tRPPositionRelativeCartesian->xvalue, f1_tRPPositionRelativeCartesian->xvalue);
      _EQ_CHECK_LONG(tRPPositionRelativeCartesian->yvalue, f1_tRPPositionRelativeCartesian->yvalue);
      _EQ_CHECK_LONG(tRPPositionRelativeCartesian->zvalue, f1_tRPPositionRelativeCartesian->zvalue);

      const f1ap_location_uncertainty_t *f1_locationUncertainty_c = &f1_tRPPositionRelativeCartesian->location_uncertainty;
      const f1ap_location_uncertainty_t *locationUncertainty_c = &tRPPositionRelativeCartesian->location_uncertainty;
      _EQ_CHECK_LONG(locationUncertainty_c->horizontal_uncertainty, f1_locationUncertainty_c->horizontal_uncertainty);
      _EQ_CHECK_LONG(locationUncertainty_c->horizontal_confidence, f1_locationUncertainty_c->horizontal_confidence);
      _EQ_CHECK_LONG(locationUncertainty_c->vertical_uncertainty, f1_locationUncertainty_c->vertical_uncertainty);
      _EQ_CHECK_LONG(locationUncertainty_c->vertical_confidence, f1_locationUncertainty_c->vertical_confidence);
      break;
    }
    default:
      AssertError(false, return false, "illegal trp reference point type entry %d\n", f1_referencePointType->present);
      break;
  }
  return true;
}

static bool eq_trp_ha_pos(const f1ap_ngran_high_accuracy_access_point_position_t *trp_ha_pos,
                          const f1ap_ngran_high_accuracy_access_point_position_t *f1_trp_ha_pos)
{
  _EQ_CHECK_LONG(trp_ha_pos->latitude, f1_trp_ha_pos->latitude);
  _EQ_CHECK_LONG(trp_ha_pos->longitude, f1_trp_ha_pos->longitude);
  _EQ_CHECK_LONG(trp_ha_pos->altitude, f1_trp_ha_pos->altitude);
  _EQ_CHECK_LONG(trp_ha_pos->uncertainty_semi_major, f1_trp_ha_pos->uncertainty_semi_major);
  _EQ_CHECK_LONG(trp_ha_pos->uncertainty_semi_minor, f1_trp_ha_pos->uncertainty_semi_minor);
  _EQ_CHECK_LONG(trp_ha_pos->orientation_of_major_axis, f1_trp_ha_pos->orientation_of_major_axis);
  _EQ_CHECK_LONG(trp_ha_pos->horizontal_confidence, f1_trp_ha_pos->horizontal_confidence);
  _EQ_CHECK_LONG(trp_ha_pos->uncertainty_altitude, f1_trp_ha_pos->uncertainty_altitude);
  _EQ_CHECK_LONG(trp_ha_pos->vertical_confidence, f1_trp_ha_pos->vertical_confidence);
  return true;
}

static bool eq_geographical_coordinates(const f1ap_geographical_coordinates_t *in, const f1ap_geographical_coordinates_t *out)
{
  const f1ap_trp_position_definition_type_t *f1_trp_pos_def_type = &in->trp_position_definition_type;
  const f1ap_trp_position_definition_type_t *trp_pos_def_type = &out->trp_position_definition_type;
  _EQ_CHECK_INT(f1_trp_pos_def_type->present, trp_pos_def_type->present);
  switch (f1_trp_pos_def_type->present) {
    case F1AP_TRP_POSITION_DEFINITION_TYPE_PR_NOTHING:
      // nothing to check
      break;
    case F1AP_TRP_POSITION_DEFINITION_TYPE_PR_DIRECT: {
      const f1ap_trp_position_direct_t *f1_direct = &f1_trp_pos_def_type->choice.direct;
      const f1ap_trp_position_direct_t *direct = &trp_pos_def_type->choice.direct;
      _EQ_CHECK_INT(f1_direct->accuracy.present, direct->accuracy.present);

      if (f1_direct->accuracy.present == F1AP_TRP_POSITION_DIRECT_ACCURACY_PR_TRPPOSITION) {
        const f1ap_access_point_position_t *trp_pos = &direct->accuracy.choice.trp_position;
        const f1ap_access_point_position_t *f1_trp_pos = &f1_direct->accuracy.choice.trp_position;
        _EQ_CHECK_LONG(trp_pos->latitude_sign, f1_trp_pos->latitude_sign);
        _EQ_CHECK_LONG(trp_pos->latitude, f1_trp_pos->latitude);
        _EQ_CHECK_LONG(trp_pos->longitude, f1_trp_pos->longitude);
        _EQ_CHECK_LONG(trp_pos->direction_of_altitude, f1_trp_pos->direction_of_altitude);
        _EQ_CHECK_LONG(trp_pos->altitude, f1_trp_pos->altitude);
        _EQ_CHECK_LONG(trp_pos->uncertainty_semi_major, f1_trp_pos->uncertainty_semi_major);
        _EQ_CHECK_LONG(trp_pos->uncertainty_semi_minor, f1_trp_pos->uncertainty_semi_minor);
        _EQ_CHECK_LONG(trp_pos->orientation_of_major_axis, f1_trp_pos->orientation_of_major_axis);
        _EQ_CHECK_LONG(trp_pos->uncertainty_altitude, f1_trp_pos->uncertainty_altitude);
        _EQ_CHECK_LONG(trp_pos->confidence, f1_trp_pos->confidence);
      } else if (f1_direct->accuracy.present == F1AP_TRP_POSITION_DIRECT_ACCURACY_PR_TRPHAPOSITION) {
        const f1ap_ngran_high_accuracy_access_point_position_t *trp_ha_pos = &direct->accuracy.choice.trp_HAposition;
        const f1ap_ngran_high_accuracy_access_point_position_t *f1_trp_ha_pos = &f1_direct->accuracy.choice.trp_HAposition;
        _F1_CHECK_EXP(eq_trp_ha_pos(trp_ha_pos, f1_trp_ha_pos));
      } else {
        AssertError(false, return false, "illegal direct accuracy entry %d\n", direct->accuracy.present);
      }
      break;
    }
    case F1AP_TRP_POSITION_DEFINITION_TYPE_PR_REFERENCED: {
      const f1ap_trp_position_referenced_t *f1_referenced = &f1_trp_pos_def_type->choice.referenced;
      const f1ap_trp_position_referenced_t *referenced = &trp_pos_def_type->choice.referenced;
      const f1ap_reference_point_t *f1_referencePoint = &f1_referenced->reference_point;
      const f1ap_reference_point_t *referencePoint = &referenced->reference_point;
      _EQ_CHECK_INT(f1_referencePoint->present, referencePoint->present);

      if (f1_referencePoint->present == F1AP_REFERENCE_POINT_PR_COORDINATEID) {
        _EQ_CHECK_LONG(referencePoint->choice.coordinate_id, f1_referencePoint->choice.coordinate_id);
      } else if (f1_referencePoint->present == F1AP_REFERENCE_POINT_PR_REFERENCEPOINTCOORDINATE) {
        const f1ap_access_point_position_t *referencePointCoordinate = &referencePoint->choice.reference_point_coordinate;
        const f1ap_access_point_position_t *f1_referencePointCoordinate = &f1_referencePoint->choice.reference_point_coordinate;
        _EQ_CHECK_LONG(referencePointCoordinate->latitude_sign, f1_referencePointCoordinate->latitude_sign);
        _EQ_CHECK_LONG(referencePointCoordinate->latitude, f1_referencePointCoordinate->latitude);
        _EQ_CHECK_LONG(referencePointCoordinate->longitude, f1_referencePointCoordinate->longitude);
        _EQ_CHECK_LONG(referencePointCoordinate->direction_of_altitude, f1_referencePointCoordinate->direction_of_altitude);
        _EQ_CHECK_LONG(referencePointCoordinate->altitude, f1_referencePointCoordinate->altitude);
        _EQ_CHECK_LONG(referencePointCoordinate->uncertainty_semi_major, f1_referencePointCoordinate->uncertainty_semi_major);
        _EQ_CHECK_LONG(referencePointCoordinate->uncertainty_semi_minor, f1_referencePointCoordinate->uncertainty_semi_minor);
        _EQ_CHECK_LONG(referencePointCoordinate->orientation_of_major_axis, f1_referencePointCoordinate->orientation_of_major_axis);
        _EQ_CHECK_LONG(referencePointCoordinate->uncertainty_altitude, f1_referencePointCoordinate->uncertainty_altitude);
        _EQ_CHECK_LONG(referencePointCoordinate->confidence, f1_referencePointCoordinate->confidence);
      } else if (f1_referencePoint->present == F1AP_REFERENCE_POINT_PR_REFERENCEPOINTCOORDINATEHA) {
        const f1ap_ngran_high_accuracy_access_point_position_t *referencePointCoordinateHA =
            &referencePoint->choice.reference_point_coordinateHA;
        const f1ap_ngran_high_accuracy_access_point_position_t *f1_referencePointCoordinateHA =
            &f1_referencePoint->choice.reference_point_coordinateHA;
        _F1_CHECK_EXP(eq_trp_ha_pos(referencePointCoordinateHA, f1_referencePointCoordinateHA));
      } else {
        AssertError(false, return false, "illegal reference point entry %d\n", referencePoint->present);
      }

      const f1ap_trp_reference_point_type_t *f1_referencePointType = &f1_referenced->reference_point_type;
      const f1ap_trp_reference_point_type_t *referencePointType = &referenced->reference_point_type;
      _F1_CHECK_EXP(eq_reference_point_type(f1_referencePointType, referencePointType));
      break;
    }
    default:
      AssertError(false, return false, "illegal Geographical Coordinates entry\n");
      break;
  }
  return true;
}

static F1AP_TRPInformationTypeResponseItem_t encode_trp_info_type_response_item(const f1ap_trp_information_type_response_item_t *in)
{
  F1AP_TRPInformationTypeResponseItem_t out = {0};
  switch (in->present) {
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_NOTHING:
      out.present = F1AP_TRPInformationTypeResponseItem_PR_NOTHING;
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_PCI_NR:
      out.present = F1AP_TRPInformationTypeResponseItem_PR_pCI_NR;
      out.choice.pCI_NR = in->choice.pci_nr;
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_NG_RAN_CGI:
      out.present = F1AP_TRPInformationTypeResponseItem_PR_nG_RAN_CGI;
      asn1cCalloc(out.choice.nG_RAN_CGI, nG_RAN_CGI);
      MCC_MNC_TO_PLMNID(in->choice.ng_ran_cgi.plmn.mcc,
                        in->choice.ng_ran_cgi.plmn.mnc,
                        in->choice.ng_ran_cgi.plmn.mnc_digit_length,
                        &(nG_RAN_CGI->pLMN_Identity));
      NR_CELL_ID_TO_BIT_STRING(in->choice.ng_ran_cgi.nr_cellid, &(nG_RAN_CGI->nRCellIdentity));
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_NRARFCN:
      out.present = F1AP_TRPInformationTypeResponseItem_PR_nRARFCN;
      out.choice.nRARFCN = in->choice.nr_arfcn;
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_PRSCONFIGURATION:
      out.present = F1AP_TRPInformationTypeResponseItem_PR_pRSConfiguration;
      AssertFatal(false, "TRP information type response item PRS configuration unsupported\n");
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_SSBINFORMATION:
      out.present = F1AP_TRPInformationTypeResponseItem_PR_sSBinformation;
      AssertFatal(false, "TRP information type response item SSB Information unsupported\n");
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_SFNINITIALISATIONTIME:
      out.present = F1AP_TRPInformationTypeResponseItem_PR_sFNInitialisationTime;
      AssertFatal(false, "TRP information type response item SFN Initialization Time unsupported\n");
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_SPATIALDIRECTIONINFORMATION:
      out.present = F1AP_TRPInformationTypeResponseItem_PR_spatialDirectionInformation;
      AssertFatal(false, "TRP information type response item Spatial Direction Information unsupported\n");
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_GEOGRAPHICALCOORDINATES:
      out.present = F1AP_TRPInformationTypeResponseItem_PR_geographicalCoordinates;
      asn1cCalloc(out.choice.geographicalCoordinates, f1_geo_coord);
      f1ap_geographical_coordinates_t geo_coord = in->choice.geographical_coordinates;
      *f1_geo_coord = encode_geographical_coordinates(&geo_coord);
      break;
    default:
      AssertFatal(false, "received illegal trp information type response item %d\n", in->present);
      break;
  }
  return out;
}

static void decode_trp_info_type_response_item(const F1AP_TRPInformationTypeResponseItem_t *in,
                                               f1ap_trp_information_type_response_item_t *out)
{
  switch (in->present) {
    case F1AP_TRPInformationTypeResponseItem_PR_NOTHING:
      out->present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_NOTHING;
      break;
    case F1AP_TRPInformationTypeResponseItem_PR_pCI_NR:
      out->present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_PCI_NR;
      out->choice.pci_nr = in->choice.pCI_NR;
      break;
    case F1AP_TRPInformationTypeResponseItem_PR_nG_RAN_CGI:
      out->present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_NG_RAN_CGI;
      // NR CGI (M)
      PLMNID_TO_MCC_MNC(&(in->choice.nG_RAN_CGI->pLMN_Identity),
                        out->choice.ng_ran_cgi.plmn.mcc,
                        out->choice.ng_ran_cgi.plmn.mnc,
                        out->choice.ng_ran_cgi.plmn.mnc_digit_length);
      // NR Cell Identity (M)
      BIT_STRING_TO_NR_CELL_IDENTITY(&in->choice.nG_RAN_CGI->nRCellIdentity, out->choice.ng_ran_cgi.nr_cellid);
      break;
    case F1AP_TRPInformationTypeResponseItem_PR_nRARFCN:
      out->present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_NRARFCN;
      out->choice.nr_arfcn = in->choice.nRARFCN;
      break;
    case F1AP_TRPInformationTypeResponseItem_PR_pRSConfiguration:
      out->present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_PRSCONFIGURATION;
      AssertFatal(false, "TRP information type response item PRS configuration unsupported\n");
      break;
    case F1AP_TRPInformationTypeResponseItem_PR_sSBinformation:
      out->present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_SSBINFORMATION;
      AssertFatal(false, "TRP information type response item SSB Information unsupported\n");
      break;
    case F1AP_TRPInformationTypeResponseItem_PR_sFNInitialisationTime:
      out->present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_SFNINITIALISATIONTIME;
      AssertFatal(false, "TRP information type response item SFN Initialization Time unsupported\n");
      break;
    case F1AP_TRPInformationTypeResponseItem_PR_spatialDirectionInformation:
      out->present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_SPATIALDIRECTIONINFORMATION;
      AssertFatal(false, "TRP information type response item Spatial Direction Information unsupported\n");
      break;
    case F1AP_TRPInformationTypeResponseItem_PR_geographicalCoordinates:
      out->present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_GEOGRAPHICALCOORDINATES;
      decode_geographical_coordinates(in->choice.geographicalCoordinates, &out->choice.geographical_coordinates);
      break;
    default:
      AssertFatal(false, "received illegal trp information type response item %d\n", in->present);
      break;
  }
}

static f1ap_trp_information_type_response_item_t cp_trp_info_type_response_item(const f1ap_trp_information_type_response_item_t *in)
{
  f1ap_trp_information_type_response_item_t out = {0};
  switch (in->present) {
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_NOTHING:
      out.present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_NOTHING;
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_PCI_NR:
      out.present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_PCI_NR;
      out.choice.pci_nr = in->choice.pci_nr;
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_NG_RAN_CGI:
      out.present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_NG_RAN_CGI;
      out.choice.ng_ran_cgi.plmn.mcc = in->choice.ng_ran_cgi.plmn.mcc;
      out.choice.ng_ran_cgi.plmn.mnc = in->choice.ng_ran_cgi.plmn.mnc;
      out.choice.ng_ran_cgi.plmn.mnc_digit_length = in->choice.ng_ran_cgi.plmn.mnc_digit_length;
      out.choice.ng_ran_cgi.nr_cellid = in->choice.ng_ran_cgi.nr_cellid;
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_NRARFCN:
      out.present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_NRARFCN;
      out.choice.nr_arfcn = in->choice.nr_arfcn;
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_PRSCONFIGURATION:
      out.present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_PRSCONFIGURATION;
      AssertFatal(false, "TRP information type response item PRS configuration unsupported\n");
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_SSBINFORMATION:
      out.present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_SSBINFORMATION;
      AssertFatal(false, "TRP information type response item SSB Information unsupported\n");
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_SFNINITIALISATIONTIME:
      out.present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_SFNINITIALISATIONTIME;
      AssertFatal(false, "TRP information type response item SFN Initialization Time unsupported\n");
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_SPATIALDIRECTIONINFORMATION:
      out.present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_SPATIALDIRECTIONINFORMATION;
      AssertFatal(false, "TRP information type response item Spatial Direction Information unsupported\n");
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_GEOGRAPHICALCOORDINATES:
      out.present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_GEOGRAPHICALCOORDINATES;
      out.choice.geographical_coordinates = cp_geographical_coordinates(&in->choice.geographical_coordinates);
      break;
    default:
      AssertFatal(false, "received illegal trp information type response item %d\n", in->present);
      break;
  }
  return out;
}

static bool eq_trp_info_type_response_item(const f1ap_trp_information_type_response_item_t *in,
                                           const f1ap_trp_information_type_response_item_t *out)
{
  _EQ_CHECK_INT(in->present, out->present);
  switch (in->present) {
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_NOTHING:
      // nothing to check
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_PCI_NR:
      _EQ_CHECK_INT(in->choice.pci_nr, out->choice.pci_nr);
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_NG_RAN_CGI:
      _EQ_CHECK_INT(in->choice.ng_ran_cgi.plmn.mcc, out->choice.ng_ran_cgi.plmn.mcc);
      _EQ_CHECK_INT(in->choice.ng_ran_cgi.plmn.mnc, out->choice.ng_ran_cgi.plmn.mnc);
      _EQ_CHECK_INT(in->choice.ng_ran_cgi.plmn.mnc_digit_length, out->choice.ng_ran_cgi.plmn.mnc_digit_length);
      _EQ_CHECK_LONG(in->choice.ng_ran_cgi.nr_cellid, out->choice.ng_ran_cgi.nr_cellid);
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_NRARFCN:
      _EQ_CHECK_INT(in->choice.nr_arfcn, out->choice.nr_arfcn);
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_PRSCONFIGURATION:
      PRINT_ERROR("TRP information type response item PRS configuration unsupported\n");
      return false;
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_SSBINFORMATION:
      PRINT_ERROR("TRP information type response item SSB Information unsupported\n");
      return false;
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_SFNINITIALISATIONTIME:
      PRINT_ERROR("TRP information type response item SFN Initialization Time unsupported\n");
      return false;
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_SPATIALDIRECTIONINFORMATION:
      PRINT_ERROR("TRP information type response item Spatial Direction Information unsupported\n");
      return false;
      break;
    case F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_GEOGRAPHICALCOORDINATES:
      _F1_CHECK_EXP(eq_geographical_coordinates(&in->choice.geographical_coordinates, &out->choice.geographical_coordinates));
      break;
    default:
      PRINT_ERROR("received illegal trp information type response item %d\n", in->present);
      return false;
      break;
  }
  return true;
}

static F1AP_PosMeasurementResult_t encode_positioning_measurement_result(const f1ap_pos_measurement_result_t *posMeasurementResult)
{
  F1AP_PosMeasurementResult_t f1_posMeasurementResult = {0};
  for (int i = 0; i < posMeasurementResult->pos_measurement_result_item_length; i++) {
    asn1cSequenceAdd(f1_posMeasurementResult.list, F1AP_PosMeasurementResultItem_t, f1_pos_measurement_result_item);
    f1ap_pos_measurement_result_item_t *pos_measurement_result_item = &posMeasurementResult->pos_measurement_result_item[i];

    // measuredResultsValue
    F1AP_MeasuredResultsValue_t *f1_measuredResultsValue = (f1_pos_measurement_result_item->measuredResultsValue = calloc_or_fail(1, sizeof(*f1_pos_measurement_result_item->measuredResultsValue)));
    f1ap_measured_results_value_t *measuredResultsValue = &pos_measurement_result_item->measured_results_value;

    switch (measuredResultsValue->present) {
      case F1AP_MEASURED_RESULTS_VALUE_PR_NOTHING:
        f1_measuredResultsValue->present = F1AP_MeasuredResultsValue_PR_NOTHING;
        break;
      // Angle of Arrival
      case F1AP_MEASURED_RESULTS_VALUE_PR_UL_ANGLEOFARRIVAL:
        f1_measuredResultsValue->present = F1AP_MeasuredResultsValue_PR_uL_AngleOfArrival;
        asn1cCalloc(f1_measuredResultsValue->choice.uL_AngleOfArrival, f1_uL_AngleOfArrival);
        f1ap_ul_aoa_t *uL_AngleOfArrival = &measuredResultsValue->choice.ul_angle_of_arrival;
        f1_uL_AngleOfArrival->azimuthAoA = uL_AngleOfArrival->azimuth_aoa;
        if (uL_AngleOfArrival->zenith_aoa) {
          asn1cCalloc(f1_uL_AngleOfArrival->zenithAoA, f1_zenithAoA);
          *f1_zenithAoA = *uL_AngleOfArrival->zenith_aoa;
        }
        if (uL_AngleOfArrival->lcs_to_gcs_translation_aoa) {
          asn1cCalloc(f1_uL_AngleOfArrival->lCS_to_GCS_TranslationAoA, f1_lCS_to_GCS_TranslationAoA);
          f1ap_lcs_to_gcs_translationaoa_t *lCS_to_GCS_TranslationAoA = uL_AngleOfArrival->lcs_to_gcs_translation_aoa;

          f1_lCS_to_GCS_TranslationAoA->alpha = lCS_to_GCS_TranslationAoA->alpha;
          f1_lCS_to_GCS_TranslationAoA->beta = lCS_to_GCS_TranslationAoA->beta;
          f1_lCS_to_GCS_TranslationAoA->gamma = lCS_to_GCS_TranslationAoA->gamma;
        }
        break;
      // UL SRS RSRP
      case F1AP_MEASURED_RESULTS_VALUE_PR_UL_SRS_RSRP:
        f1_measuredResultsValue->present = F1AP_MeasuredResultsValue_PR_uL_SRS_RSRP;
        f1_measuredResultsValue->choice.uL_SRS_RSRP = measuredResultsValue->choice.ul_srs_rsrp;
        break;
      // UL RTOA
      case F1AP_MEASURED_RESULTS_VALUE_PR_UL_RTOA:
        f1_measuredResultsValue->present = F1AP_MeasuredResultsValue_PR_uL_RTOA;
        asn1cCalloc(f1_measuredResultsValue->choice.uL_RTOA, f1_uL_RTOA);
        f1ap_ul_rtoa_measurement_t *uL_RTOA = &measuredResultsValue->choice.ul_rtoa;
        f1ap_ul_rtoa_measurement_item_t *ul_rtoa_meas_item = &uL_RTOA->ul_rtoa_measurement_item;
        F1AP_UL_RTOA_MeasurementItem_t *f1_ul_rtoa_meas_item = (f1_uL_RTOA->uL_RTOA_MeasurementItem = calloc_or_fail(1, sizeof(*f1_uL_RTOA->uL_RTOA_MeasurementItem)));

        switch (ul_rtoa_meas_item->present) {
          case F1AP_ULRTOAMEAS_PR_NOTHING:
            f1_ul_rtoa_meas_item->present = F1AP_UL_RTOA_MeasurementItem_PR_NOTHING;
            break;
          case F1AP_ULRTOAMEAS_PR_K0:
            f1_ul_rtoa_meas_item->present = F1AP_UL_RTOA_MeasurementItem_PR_k0;
            f1_ul_rtoa_meas_item->choice.k0 = ul_rtoa_meas_item->choice.k0;
            break;
          case F1AP_ULRTOAMEAS_PR_K1:
            f1_ul_rtoa_meas_item->present = F1AP_UL_RTOA_MeasurementItem_PR_k1;
            f1_ul_rtoa_meas_item->choice.k1 = ul_rtoa_meas_item->choice.k1;
            break;
          case F1AP_ULRTOAMEAS_PR_K2:
            f1_ul_rtoa_meas_item->present = F1AP_UL_RTOA_MeasurementItem_PR_k2;
            f1_ul_rtoa_meas_item->choice.k2 = ul_rtoa_meas_item->choice.k2;
            break;
          case F1AP_ULRTOAMEAS_PR_K3:
            f1_ul_rtoa_meas_item->present = F1AP_UL_RTOA_MeasurementItem_PR_k3;
            f1_ul_rtoa_meas_item->choice.k3 = ul_rtoa_meas_item->choice.k3;
            break;
          case F1AP_ULRTOAMEAS_PR_K4:
            f1_ul_rtoa_meas_item->present = F1AP_UL_RTOA_MeasurementItem_PR_k4;
            f1_ul_rtoa_meas_item->choice.k4 = ul_rtoa_meas_item->choice.k4;
            break;
          case F1AP_ULRTOAMEAS_PR_K5:
            f1_ul_rtoa_meas_item->present = F1AP_UL_RTOA_MeasurementItem_PR_k5;
            f1_ul_rtoa_meas_item->choice.k5 = ul_rtoa_meas_item->choice.k5;
            break;
          default:
            AssertFatal(false, "Illegal uL_RTOA_MeasurementItem %d\n", ul_rtoa_meas_item->present);
            break;
        }
        break;
      // gNB RX-TX Time Diff
      case F1AP_MEASURED_RESULTS_VALUE_PR_GNB_RXTXTIMEDIFF:
        f1_measuredResultsValue->present = F1AP_MeasuredResultsValue_PR_gNB_RxTxTimeDiff;
        asn1cCalloc(f1_measuredResultsValue->choice.gNB_RxTxTimeDiff, f1_gNB_RxTxTimeDiff);

        f1ap_gnb_rx_tx_time_diff_t *gNB_RxTxTimeDiff = &measuredResultsValue->choice.gnb_rx_tx_time_diff;
        f1ap_gnb_rx_tx_time_diff_meas_t *rx_tx_time_diff = &gNB_RxTxTimeDiff->rx_tx_time_diff;
        F1AP_GNBRxTxTimeDiffMeas_t *f1_rxTxTimeDiff = (f1_gNB_RxTxTimeDiff->rxTxTimeDiff = calloc_or_fail(1, sizeof(*f1_gNB_RxTxTimeDiff->rxTxTimeDiff)));

        switch (rx_tx_time_diff->present) {
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_NOTHING:
            f1_rxTxTimeDiff->present = F1AP_GNBRxTxTimeDiffMeas_PR_NOTHING;
            break;
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_K0:
            f1_rxTxTimeDiff->present = F1AP_GNBRxTxTimeDiffMeas_PR_k0;
            f1_rxTxTimeDiff->choice.k0 = rx_tx_time_diff->choice.k0;
            break;
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_K1:
            f1_rxTxTimeDiff->present = F1AP_GNBRxTxTimeDiffMeas_PR_k1;
            f1_rxTxTimeDiff->choice.k1 = rx_tx_time_diff->choice.k1;
            break;
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_K2:
            f1_rxTxTimeDiff->present = F1AP_GNBRxTxTimeDiffMeas_PR_k2;
            f1_rxTxTimeDiff->choice.k2 = rx_tx_time_diff->choice.k2;
            break;
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_K3:
            f1_rxTxTimeDiff->present = F1AP_GNBRxTxTimeDiffMeas_PR_k3;
            f1_rxTxTimeDiff->choice.k3 = rx_tx_time_diff->choice.k3;
            break;
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_K4:
            f1_rxTxTimeDiff->present = F1AP_GNBRxTxTimeDiffMeas_PR_k4;
            f1_rxTxTimeDiff->choice.k4 = rx_tx_time_diff->choice.k4;
            break;
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_K5:
            f1_rxTxTimeDiff->present = F1AP_GNBRxTxTimeDiffMeas_PR_k5;
            f1_rxTxTimeDiff->choice.k5 = rx_tx_time_diff->choice.k5;
            break;
          default:
            AssertFatal(false, "Illegal rxTxTimeDiff value %d\n", rx_tx_time_diff->present);
            break;
        }
        break;
      default:
        AssertFatal(false, "Illegal measuredResultsValue %d\n", measuredResultsValue->present);
        break;
    }

    // timeStamp
    F1AP_TimeStamp_t *f1_timeStamp = (f1_pos_measurement_result_item->timeStamp = calloc_or_fail(1, sizeof(*f1_pos_measurement_result_item->timeStamp)));
    f1ap_time_stamp_t *timeStamp = &pos_measurement_result_item->time_stamp;
    f1_timeStamp->systemFrameNumber = timeStamp->system_frame_number;
    F1AP_TimeStampSlotIndex_t *f1_slotIndex = (f1_timeStamp->slotIndex = calloc_or_fail(1, sizeof(*f1_timeStamp->slotIndex)));
    f1ap_time_stamp_slot_index_t *slot_index = &timeStamp->slot_index;
    switch (slot_index->present) {
      case F1AP_TIME_STAMP_SLOT_INDEX_PR_NOTHING:
        f1_slotIndex->present = F1AP_TimeStampSlotIndex_PR_NOTHING;
        break;
      case F1AP_TIME_STAMP_SLOT_INDEX_PR_SCS_15:
        f1_slotIndex->present = F1AP_TimeStampSlotIndex_PR_sCS_15;
        f1_slotIndex->choice.sCS_15 = slot_index->choice.scs_15;
        break;
      case F1AP_TIME_STAMP_SLOT_INDEX_PR_SCS_30:
        f1_slotIndex->present = F1AP_TimeStampSlotIndex_PR_sCS_30;
        f1_slotIndex->choice.sCS_30 = slot_index->choice.scs_30;
        break;
      case F1AP_TIME_STAMP_SLOT_INDEX_PR_SCS_60:
        f1_slotIndex->present = F1AP_TimeStampSlotIndex_PR_sCS_60;
        f1_slotIndex->choice.sCS_60 = slot_index->choice.scs_60;
        break;
      case F1AP_TIME_STAMP_SLOT_INDEX_PR_SCS_120:
        f1_slotIndex->present = F1AP_TimeStampSlotIndex_PR_sCS_120;
        f1_slotIndex->choice.sCS_120 = slot_index->choice.scs_120;
        break;
      default:
        AssertFatal(false, "Illegal slotIndex value %d\n", slot_index->present);
        break;
    }
  }
  return f1_posMeasurementResult;
}

static bool decode_positioning_measurement_result(F1AP_PosMeasurementResult_t *f1_posMeasurementResult,
                                                  f1ap_pos_measurement_result_t *posMeasurementResult)
{
  uint32_t pos_meas_result_length = f1_posMeasurementResult->list.count;
  posMeasurementResult->pos_measurement_result_item_length = pos_meas_result_length;
  posMeasurementResult->pos_measurement_result_item =
      calloc_or_fail(pos_meas_result_length, sizeof(*posMeasurementResult->pos_measurement_result_item));
  for (int i = 0; i < pos_meas_result_length; i++) {
    F1AP_PosMeasurementResultItem_t *f1_pos_measurement_result_item = f1_posMeasurementResult->list.array[i];
    f1ap_pos_measurement_result_item_t *pos_measurement_result_item = &posMeasurementResult->pos_measurement_result_item[i];

    // measuredResultsValue
    F1AP_MeasuredResultsValue_t *f1_measuredResultsValue = f1_pos_measurement_result_item->measuredResultsValue;
    f1ap_measured_results_value_t *measuredResultsValue = &pos_measurement_result_item->measured_results_value;

    switch (f1_measuredResultsValue->present) {
      case F1AP_MeasuredResultsValue_PR_NOTHING:
        measuredResultsValue->present = F1AP_MEASURED_RESULTS_VALUE_PR_NOTHING;
        break;
      // Angle of Arrival
      case F1AP_MeasuredResultsValue_PR_uL_AngleOfArrival:
        measuredResultsValue->present = F1AP_MEASURED_RESULTS_VALUE_PR_UL_ANGLEOFARRIVAL;

        F1AP_UL_AoA_t *f1_uL_AngleOfArrival = f1_measuredResultsValue->choice.uL_AngleOfArrival;
        f1ap_ul_aoa_t *uL_AngleOfArrival = &measuredResultsValue->choice.ul_angle_of_arrival;

        uL_AngleOfArrival->azimuth_aoa = f1_uL_AngleOfArrival->azimuthAoA;
        if (f1_uL_AngleOfArrival->zenithAoA) {
          uL_AngleOfArrival->zenith_aoa = calloc_or_fail(1, sizeof(*uL_AngleOfArrival->zenith_aoa));
          *uL_AngleOfArrival->zenith_aoa = *f1_uL_AngleOfArrival->zenithAoA;
        }
        if (f1_uL_AngleOfArrival->lCS_to_GCS_TranslationAoA) {
          uL_AngleOfArrival->lcs_to_gcs_translation_aoa = calloc_or_fail(1, sizeof(*uL_AngleOfArrival->lcs_to_gcs_translation_aoa));
          f1ap_lcs_to_gcs_translationaoa_t *lCS_to_GCS_TranslationAoA = uL_AngleOfArrival->lcs_to_gcs_translation_aoa;
          F1AP_LCS_to_GCS_TranslationAoA_t *f1_lCS_to_GCS_TranslationAoA = f1_uL_AngleOfArrival->lCS_to_GCS_TranslationAoA;

          lCS_to_GCS_TranslationAoA->alpha = f1_lCS_to_GCS_TranslationAoA->alpha;
          lCS_to_GCS_TranslationAoA->beta = f1_lCS_to_GCS_TranslationAoA->beta;
          lCS_to_GCS_TranslationAoA->gamma = f1_lCS_to_GCS_TranslationAoA->gamma;
        }
        break;
      // UL SRS RSRP
      case F1AP_MeasuredResultsValue_PR_uL_SRS_RSRP:
        measuredResultsValue->present = F1AP_MEASURED_RESULTS_VALUE_PR_UL_SRS_RSRP;
        measuredResultsValue->choice.ul_srs_rsrp = f1_measuredResultsValue->choice.uL_SRS_RSRP;
        break;
      // UL RTOA
      case F1AP_MeasuredResultsValue_PR_uL_RTOA:
        measuredResultsValue->present = F1AP_MEASURED_RESULTS_VALUE_PR_UL_RTOA;
        F1AP_UL_RTOA_Measurement_t *f1_uL_RTOA = f1_measuredResultsValue->choice.uL_RTOA;
        f1ap_ul_rtoa_measurement_t *uL_RTOA = &measuredResultsValue->choice.ul_rtoa;
        F1AP_UL_RTOA_MeasurementItem_t *f1_ul_rtoa_meas_item = f1_uL_RTOA->uL_RTOA_MeasurementItem;
        f1ap_ul_rtoa_measurement_item_t *ul_rtoa_meas_item = &uL_RTOA->ul_rtoa_measurement_item;

        switch (f1_ul_rtoa_meas_item->present) {
          case F1AP_UL_RTOA_MeasurementItem_PR_NOTHING:
            ul_rtoa_meas_item->present = F1AP_ULRTOAMEAS_PR_NOTHING;
            break;
          case F1AP_UL_RTOA_MeasurementItem_PR_k0:
            ul_rtoa_meas_item->present = F1AP_ULRTOAMEAS_PR_K0;
            ul_rtoa_meas_item->choice.k0 = f1_ul_rtoa_meas_item->choice.k0;
            break;
          case F1AP_UL_RTOA_MeasurementItem_PR_k1:
            ul_rtoa_meas_item->present = F1AP_ULRTOAMEAS_PR_K1;
            ul_rtoa_meas_item->choice.k1 = f1_ul_rtoa_meas_item->choice.k1;
            break;
          case F1AP_UL_RTOA_MeasurementItem_PR_k2:
            ul_rtoa_meas_item->present = F1AP_ULRTOAMEAS_PR_K2;
            ul_rtoa_meas_item->choice.k2 = f1_ul_rtoa_meas_item->choice.k2;
            break;
          case F1AP_UL_RTOA_MeasurementItem_PR_k3:
            ul_rtoa_meas_item->present = F1AP_ULRTOAMEAS_PR_K3;
            ul_rtoa_meas_item->choice.k3 = f1_ul_rtoa_meas_item->choice.k3;
            break;
          case F1AP_UL_RTOA_MeasurementItem_PR_k4:
            ul_rtoa_meas_item->present = F1AP_ULRTOAMEAS_PR_K4;
            ul_rtoa_meas_item->choice.k4 = f1_ul_rtoa_meas_item->choice.k4;
            break;
          case F1AP_UL_RTOA_MeasurementItem_PR_k5:
            ul_rtoa_meas_item->present = F1AP_ULRTOAMEAS_PR_K5;
            ul_rtoa_meas_item->choice.k5 = f1_ul_rtoa_meas_item->choice.k5;
            break;
          default:
            AssertError(false, return false, "Illegal uL_RTOA_MeasurementItem %d\n", f1_ul_rtoa_meas_item->present);
            break;
        }
        break;
      // gNB RX-TX Time Diff
      case F1AP_MeasuredResultsValue_PR_gNB_RxTxTimeDiff:
        measuredResultsValue->present = F1AP_MEASURED_RESULTS_VALUE_PR_GNB_RXTXTIMEDIFF;
        F1AP_GNB_RxTxTimeDiff_t *f1_gNB_RxTxTimeDiff = f1_measuredResultsValue->choice.gNB_RxTxTimeDiff;
        f1ap_gnb_rx_tx_time_diff_t *gNB_RxTxTimeDiff = &measuredResultsValue->choice.gnb_rx_tx_time_diff;
        F1AP_GNBRxTxTimeDiffMeas_t *f1_rxTxTimeDiff = f1_gNB_RxTxTimeDiff->rxTxTimeDiff;
        f1ap_gnb_rx_tx_time_diff_meas_t *rx_tx_time_diff = &gNB_RxTxTimeDiff->rx_tx_time_diff;

        switch (f1_rxTxTimeDiff->present) {
          case F1AP_GNBRxTxTimeDiffMeas_PR_NOTHING:
            rx_tx_time_diff->present = F1AP_GNBRXTXTIMEDIFFMEAS_PR_NOTHING;
            break;
          case F1AP_GNBRxTxTimeDiffMeas_PR_k0:
            rx_tx_time_diff->present = F1AP_GNBRXTXTIMEDIFFMEAS_PR_K0;
            rx_tx_time_diff->choice.k0 = f1_rxTxTimeDiff->choice.k0;
            break;
          case F1AP_GNBRxTxTimeDiffMeas_PR_k1:
            rx_tx_time_diff->present = F1AP_GNBRXTXTIMEDIFFMEAS_PR_K1;
            rx_tx_time_diff->choice.k1 = f1_rxTxTimeDiff->choice.k1;
            break;
          case F1AP_GNBRxTxTimeDiffMeas_PR_k2:
            rx_tx_time_diff->present = F1AP_GNBRXTXTIMEDIFFMEAS_PR_K2;
            rx_tx_time_diff->choice.k2 = f1_rxTxTimeDiff->choice.k2;
            break;
          case F1AP_GNBRxTxTimeDiffMeas_PR_k3:
            rx_tx_time_diff->present = F1AP_GNBRXTXTIMEDIFFMEAS_PR_K3;
            rx_tx_time_diff->choice.k3 = f1_rxTxTimeDiff->choice.k3;
            break;
          case F1AP_GNBRxTxTimeDiffMeas_PR_k4:
            rx_tx_time_diff->present = F1AP_GNBRXTXTIMEDIFFMEAS_PR_K4;
            rx_tx_time_diff->choice.k4 = f1_rxTxTimeDiff->choice.k4;
            break;
          case F1AP_GNBRxTxTimeDiffMeas_PR_k5:
            rx_tx_time_diff->present = F1AP_GNBRXTXTIMEDIFFMEAS_PR_K5;
            rx_tx_time_diff->choice.k5 = f1_rxTxTimeDiff->choice.k5;
            break;
          default:
            AssertError(false, return false, "Illegal rxTxTimeDiff value %d\n", f1_rxTxTimeDiff->present);
            break;
        }
        break;
      default:
        AssertError(false, return false, "Illegal measuredResultsValue %d\n", f1_measuredResultsValue->present);
        break;
    }

    // timeStamp
    F1AP_TimeStamp_t *f1_timeStamp = f1_pos_measurement_result_item->timeStamp;
    f1ap_time_stamp_t *timeStamp = &pos_measurement_result_item->time_stamp;
    timeStamp->system_frame_number = f1_timeStamp->systemFrameNumber;
    F1AP_TimeStampSlotIndex_t *f1_slotIndex = f1_timeStamp->slotIndex;
    f1ap_time_stamp_slot_index_t *slot_index = &timeStamp->slot_index;
    switch (f1_slotIndex->present) {
      case F1AP_TimeStampSlotIndex_PR_NOTHING:
        slot_index->present = F1AP_TIME_STAMP_SLOT_INDEX_PR_NOTHING;
        break;
      case F1AP_TimeStampSlotIndex_PR_sCS_15:
        slot_index->present = F1AP_TIME_STAMP_SLOT_INDEX_PR_SCS_15;
        slot_index->choice.scs_15 = f1_slotIndex->choice.sCS_15;
        break;
      case F1AP_TimeStampSlotIndex_PR_sCS_30:
        slot_index->present = F1AP_TIME_STAMP_SLOT_INDEX_PR_SCS_30;
        slot_index->choice.scs_30 = f1_slotIndex->choice.sCS_30;
        break;
      case F1AP_TimeStampSlotIndex_PR_sCS_60:
        slot_index->present = F1AP_TIME_STAMP_SLOT_INDEX_PR_SCS_60;
        slot_index->choice.scs_60 = f1_slotIndex->choice.sCS_60;
        break;
      case F1AP_TimeStampSlotIndex_PR_sCS_120:
        slot_index->present = F1AP_TIME_STAMP_SLOT_INDEX_PR_SCS_120;
        slot_index->choice.scs_120 = f1_slotIndex->choice.sCS_120;
        break;
      default:
        AssertError(false, return false, "Illegal slotIndex value %d\n", f1_slotIndex->present);
        break;
    }
  }
  return true;
}

static f1ap_pos_measurement_result_t cp_positioning_measurement_result(const f1ap_pos_measurement_result_t *in)
{
  f1ap_pos_measurement_result_t out = {0};
  uint32_t pos_meas_result_length = in->pos_measurement_result_item_length;
  out.pos_measurement_result_item_length = pos_meas_result_length;
  out.pos_measurement_result_item = calloc_or_fail(pos_meas_result_length, sizeof(*out.pos_measurement_result_item));
  for (int i = 0; i < pos_meas_result_length; i++) {
    f1ap_pos_measurement_result_item_t *f1_pos_measurement_result_item = &in->pos_measurement_result_item[i];
    f1ap_pos_measurement_result_item_t *pos_measurement_result_item = &out.pos_measurement_result_item[i];

    // measuredResultsValue
    f1ap_measured_results_value_t *f1_measuredResultsValue = &f1_pos_measurement_result_item->measured_results_value;
    f1ap_measured_results_value_t *measuredResultsValue = &pos_measurement_result_item->measured_results_value;

    measuredResultsValue->present = f1_measuredResultsValue->present;
    switch (f1_measuredResultsValue->present) {
      case F1AP_MEASURED_RESULTS_VALUE_PR_NOTHING:
        // nothing to copy
        break;
      // Angle of Arrival
      case F1AP_MEASURED_RESULTS_VALUE_PR_UL_ANGLEOFARRIVAL: {
        f1ap_ul_aoa_t *f1_uL_AngleOfArrival = &f1_measuredResultsValue->choice.ul_angle_of_arrival;
        f1ap_ul_aoa_t *uL_AngleOfArrival = &measuredResultsValue->choice.ul_angle_of_arrival;

        uL_AngleOfArrival->azimuth_aoa = f1_uL_AngleOfArrival->azimuth_aoa;
        if (f1_uL_AngleOfArrival->zenith_aoa) {
          uL_AngleOfArrival->zenith_aoa = calloc_or_fail(1, sizeof(*uL_AngleOfArrival->zenith_aoa));
          *uL_AngleOfArrival->zenith_aoa = *f1_uL_AngleOfArrival->zenith_aoa;
        }
        if (f1_uL_AngleOfArrival->lcs_to_gcs_translation_aoa) {
          uL_AngleOfArrival->lcs_to_gcs_translation_aoa = calloc_or_fail(1, sizeof(*uL_AngleOfArrival->lcs_to_gcs_translation_aoa));

          f1ap_lcs_to_gcs_translationaoa_t *lCS_to_GCS_TranslationAoA = uL_AngleOfArrival->lcs_to_gcs_translation_aoa;
          f1ap_lcs_to_gcs_translationaoa_t *f1_lCS_to_GCS_TranslationAoA = f1_uL_AngleOfArrival->lcs_to_gcs_translation_aoa;

          lCS_to_GCS_TranslationAoA->alpha = f1_lCS_to_GCS_TranslationAoA->alpha;
          lCS_to_GCS_TranslationAoA->beta = f1_lCS_to_GCS_TranslationAoA->beta;
          lCS_to_GCS_TranslationAoA->gamma = f1_lCS_to_GCS_TranslationAoA->gamma;
        }
        break;
      }
      // UL SRS RSRP
      case F1AP_MEASURED_RESULTS_VALUE_PR_UL_SRS_RSRP:
        measuredResultsValue->choice.ul_srs_rsrp = f1_measuredResultsValue->choice.ul_srs_rsrp;
        break;
      // UL RTOA
      case F1AP_MEASURED_RESULTS_VALUE_PR_UL_RTOA: {
        f1ap_ul_rtoa_measurement_t *f1_uL_RTOA = &f1_measuredResultsValue->choice.ul_rtoa;
        f1ap_ul_rtoa_measurement_t *uL_RTOA = &measuredResultsValue->choice.ul_rtoa;
        f1ap_ul_rtoa_measurement_item_t *f1_ul_rtoa_meas_item = &f1_uL_RTOA->ul_rtoa_measurement_item;
        f1ap_ul_rtoa_measurement_item_t *ul_rtoa_meas_item = &uL_RTOA->ul_rtoa_measurement_item;

        ul_rtoa_meas_item->present = f1_ul_rtoa_meas_item->present;
        switch (f1_ul_rtoa_meas_item->present) {
          case F1AP_ULRTOAMEAS_PR_NOTHING:
            break;
          case F1AP_ULRTOAMEAS_PR_K0:
            ul_rtoa_meas_item->choice.k0 = f1_ul_rtoa_meas_item->choice.k0;
            break;
          case F1AP_ULRTOAMEAS_PR_K1:
            ul_rtoa_meas_item->choice.k1 = f1_ul_rtoa_meas_item->choice.k1;
            break;
          case F1AP_ULRTOAMEAS_PR_K2:
            ul_rtoa_meas_item->choice.k2 = f1_ul_rtoa_meas_item->choice.k2;
            break;
          case F1AP_ULRTOAMEAS_PR_K3:
            ul_rtoa_meas_item->choice.k3 = f1_ul_rtoa_meas_item->choice.k3;
            break;
          case F1AP_ULRTOAMEAS_PR_K4:
            ul_rtoa_meas_item->choice.k4 = f1_ul_rtoa_meas_item->choice.k4;
            break;
          case F1AP_ULRTOAMEAS_PR_K5:
            ul_rtoa_meas_item->choice.k5 = f1_ul_rtoa_meas_item->choice.k5;
            break;
          default:
            AssertFatal(false, "Illegal uL_RTOA_MeasurementItem %d\n", f1_uL_RTOA->ul_rtoa_measurement_item.present);
            break;
        }
        break;
      }
      // gNB RX-TX Time Diff
      case F1AP_MEASURED_RESULTS_VALUE_PR_GNB_RXTXTIMEDIFF: {
        f1ap_gnb_rx_tx_time_diff_t *f1_gNB_RxTxTimeDiff = &f1_measuredResultsValue->choice.gnb_rx_tx_time_diff;
        f1ap_gnb_rx_tx_time_diff_t *gNB_RxTxTimeDiff = &measuredResultsValue->choice.gnb_rx_tx_time_diff;
        f1ap_gnb_rx_tx_time_diff_meas_t *f1_rx_tx_time_diff = &f1_gNB_RxTxTimeDiff->rx_tx_time_diff;
        f1ap_gnb_rx_tx_time_diff_meas_t *rx_tx_time_diff = &gNB_RxTxTimeDiff->rx_tx_time_diff;

        rx_tx_time_diff->present = f1_rx_tx_time_diff->present;
        switch (f1_rx_tx_time_diff->present) {
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_NOTHING:
            break;
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_K0:
            rx_tx_time_diff->choice.k0 = f1_rx_tx_time_diff->choice.k0;
            break;
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_K1:
            rx_tx_time_diff->choice.k1 = f1_rx_tx_time_diff->choice.k1;
            break;
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_K2:
            rx_tx_time_diff->choice.k2 = f1_rx_tx_time_diff->choice.k2;
            break;
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_K3:
            rx_tx_time_diff->choice.k3 = f1_rx_tx_time_diff->choice.k3;
            break;
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_K4:
            rx_tx_time_diff->choice.k4 = f1_rx_tx_time_diff->choice.k4;
            break;
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_K5:
            rx_tx_time_diff->choice.k5 = f1_rx_tx_time_diff->choice.k5;
            break;
          default:
            AssertFatal(false, "Illegal rxTxTimeDiff value %d\n", f1_rx_tx_time_diff->present);
            break;
        }
        break;
      }
      default:
        AssertFatal(false, "Illegal measuredResultsValue %d\n", f1_measuredResultsValue->present);
        break;
    }

    // timeStamp
    f1ap_time_stamp_t *f1_timeStamp = &f1_pos_measurement_result_item->time_stamp;
    f1ap_time_stamp_t *timeStamp = &pos_measurement_result_item->time_stamp;
    timeStamp->system_frame_number = f1_timeStamp->system_frame_number;
    f1ap_time_stamp_slot_index_t *f1_slot_index = &f1_timeStamp->slot_index;
    f1ap_time_stamp_slot_index_t *slot_index = &timeStamp->slot_index;

    slot_index->present = f1_slot_index->present;
    switch (f1_slot_index->present) {
      case F1AP_TIME_STAMP_SLOT_INDEX_PR_NOTHING:
        break;
      case F1AP_TIME_STAMP_SLOT_INDEX_PR_SCS_15:
        slot_index->choice.scs_15 = f1_slot_index->choice.scs_15;
        break;
      case F1AP_TIME_STAMP_SLOT_INDEX_PR_SCS_30:
        slot_index->choice.scs_30 = f1_slot_index->choice.scs_30;
        break;
      case F1AP_TIME_STAMP_SLOT_INDEX_PR_SCS_60:
        slot_index->choice.scs_60 = f1_slot_index->choice.scs_60;
        break;
      case F1AP_TIME_STAMP_SLOT_INDEX_PR_SCS_120:
        slot_index->choice.scs_120 = f1_slot_index->choice.scs_120;
        break;
      default:
        AssertFatal(false, "Illegal slotIndex value %d\n", f1_slot_index->present);
        break;
    }
  }
  return out;
}

static bool eq_positioning_measurement_result(const f1ap_pos_measurement_result_t *f1_posMeasurementResult,
                                              const f1ap_pos_measurement_result_t *posMeasurementResult)
{
  uint32_t pos_meas_result_length = f1_posMeasurementResult->pos_measurement_result_item_length;
  _EQ_CHECK_INT(posMeasurementResult->pos_measurement_result_item_length, pos_meas_result_length);
  for (int i = 0; i < pos_meas_result_length; i++) {
    f1ap_pos_measurement_result_item_t *f1_pos_measurement_result_item = &f1_posMeasurementResult->pos_measurement_result_item[i];
    f1ap_pos_measurement_result_item_t *pos_measurement_result_item = &posMeasurementResult->pos_measurement_result_item[i];

    // measuredResultsValue
    f1ap_measured_results_value_t *f1_measuredResultsValue = &f1_pos_measurement_result_item->measured_results_value;
    f1ap_measured_results_value_t *measuredResultsValue = &pos_measurement_result_item->measured_results_value;

    _EQ_CHECK_INT(measuredResultsValue->present, f1_measuredResultsValue->present);
    switch (measuredResultsValue->present) {
      case F1AP_MEASURED_RESULTS_VALUE_PR_NOTHING:
        // nothing to check
        break;
      // Angle of Arrival
      case F1AP_MEASURED_RESULTS_VALUE_PR_UL_ANGLEOFARRIVAL: {
        f1ap_ul_aoa_t *f1_uL_AngleOfArrival = &f1_measuredResultsValue->choice.ul_angle_of_arrival;
        f1ap_ul_aoa_t *uL_AngleOfArrival = &measuredResultsValue->choice.ul_angle_of_arrival;

        _EQ_CHECK_INT(uL_AngleOfArrival->azimuth_aoa, f1_uL_AngleOfArrival->azimuth_aoa);
        if (f1_uL_AngleOfArrival->zenith_aoa) {
          _EQ_CHECK_INT(*uL_AngleOfArrival->zenith_aoa, *f1_uL_AngleOfArrival->zenith_aoa);
        }
        if (f1_uL_AngleOfArrival->lcs_to_gcs_translation_aoa) {
          f1ap_lcs_to_gcs_translationaoa_t *lCS_to_GCS_TranslationAoA = uL_AngleOfArrival->lcs_to_gcs_translation_aoa;
          f1ap_lcs_to_gcs_translationaoa_t *f1_lCS_to_GCS_TranslationAoA = f1_uL_AngleOfArrival->lcs_to_gcs_translation_aoa;

          _EQ_CHECK_INT(lCS_to_GCS_TranslationAoA->alpha, f1_lCS_to_GCS_TranslationAoA->alpha);
          _EQ_CHECK_INT(lCS_to_GCS_TranslationAoA->beta, f1_lCS_to_GCS_TranslationAoA->beta);
          _EQ_CHECK_INT(lCS_to_GCS_TranslationAoA->gamma, f1_lCS_to_GCS_TranslationAoA->gamma);
        }
        break;
      }
      // UL SRS RSRP
      case F1AP_MEASURED_RESULTS_VALUE_PR_UL_SRS_RSRP:
        _EQ_CHECK_INT(measuredResultsValue->choice.ul_srs_rsrp, f1_measuredResultsValue->choice.ul_srs_rsrp);
        break;
      // UL RTOA
      case F1AP_MEASURED_RESULTS_VALUE_PR_UL_RTOA: {
        f1ap_ul_rtoa_measurement_t *f1_uL_RTOA = &f1_measuredResultsValue->choice.ul_rtoa;
        f1ap_ul_rtoa_measurement_t *uL_RTOA = &measuredResultsValue->choice.ul_rtoa;

        f1ap_ul_rtoa_measurement_item_t *f1_ul_rtoa_meas_item = &f1_uL_RTOA->ul_rtoa_measurement_item;
        f1ap_ul_rtoa_measurement_item_t *ul_rtoa_meas_item = &uL_RTOA->ul_rtoa_measurement_item;

        switch (f1_ul_rtoa_meas_item->present) {
          case F1AP_ULRTOAMEAS_PR_NOTHING:
            break;
          case F1AP_ULRTOAMEAS_PR_K0:
            _EQ_CHECK_INT(ul_rtoa_meas_item->choice.k0, f1_ul_rtoa_meas_item->choice.k0);
            break;
          case F1AP_ULRTOAMEAS_PR_K1:
            _EQ_CHECK_INT(ul_rtoa_meas_item->choice.k1, f1_ul_rtoa_meas_item->choice.k1);
            break;
          case F1AP_ULRTOAMEAS_PR_K2:
            _EQ_CHECK_INT(ul_rtoa_meas_item->choice.k2, f1_ul_rtoa_meas_item->choice.k2);
            break;
          case F1AP_ULRTOAMEAS_PR_K3:
            _EQ_CHECK_INT(ul_rtoa_meas_item->choice.k3, f1_ul_rtoa_meas_item->choice.k3);
            break;
          case F1AP_ULRTOAMEAS_PR_K4:
            _EQ_CHECK_INT(ul_rtoa_meas_item->choice.k4, f1_ul_rtoa_meas_item->choice.k4);
            break;
          case F1AP_ULRTOAMEAS_PR_K5:
            _EQ_CHECK_INT(ul_rtoa_meas_item->choice.k5, f1_ul_rtoa_meas_item->choice.k5);
            break;
          default:
            AssertError(false, return false, "Illegal uL_RTOA_MeasurementItem %d\n", f1_ul_rtoa_meas_item->present);
            break;
        }
        break;
      }
      // gNB RX-TX Time Diff
      case F1AP_MEASURED_RESULTS_VALUE_PR_GNB_RXTXTIMEDIFF: {
        f1ap_gnb_rx_tx_time_diff_t *f1_gNB_RxTxTimeDiff = &f1_measuredResultsValue->choice.gnb_rx_tx_time_diff;
        f1ap_gnb_rx_tx_time_diff_t *gNB_RxTxTimeDiff = &measuredResultsValue->choice.gnb_rx_tx_time_diff;
        f1ap_gnb_rx_tx_time_diff_meas_t *f1_rx_tx_time_diff = &f1_gNB_RxTxTimeDiff->rx_tx_time_diff;
        f1ap_gnb_rx_tx_time_diff_meas_t *rx_tx_time_diff = &gNB_RxTxTimeDiff->rx_tx_time_diff;
        _EQ_CHECK_INT(f1_rx_tx_time_diff->present, rx_tx_time_diff->present);
        switch (f1_rx_tx_time_diff->present) {
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_NOTHING:
            break;
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_K0:
            _EQ_CHECK_INT(rx_tx_time_diff->choice.k0, f1_rx_tx_time_diff->choice.k0);
            break;
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_K1:
            _EQ_CHECK_INT(rx_tx_time_diff->choice.k1, f1_rx_tx_time_diff->choice.k1);
            break;
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_K2:
            _EQ_CHECK_INT(rx_tx_time_diff->choice.k2, f1_rx_tx_time_diff->choice.k2);
            break;
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_K3:
            _EQ_CHECK_INT(rx_tx_time_diff->choice.k3, f1_rx_tx_time_diff->choice.k3);
            break;
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_K4:
            _EQ_CHECK_INT(rx_tx_time_diff->choice.k4, f1_rx_tx_time_diff->choice.k4);
            break;
          case F1AP_GNBRXTXTIMEDIFFMEAS_PR_K5:
            _EQ_CHECK_INT(rx_tx_time_diff->choice.k5, f1_rx_tx_time_diff->choice.k5);
            break;
          default:
            AssertError(false, return false, "Illegal rxTxTimeDiff value %d\n", f1_rx_tx_time_diff->present);
            break;
        }
        break;
      }
      default:
        AssertError(false, return false, "Illegal measuredResultsValue %d\n", f1_measuredResultsValue->present);
        break;
    }

    // timeStamp
    f1ap_time_stamp_t *f1_timeStamp = &f1_pos_measurement_result_item->time_stamp;
    f1ap_time_stamp_t *timeStamp = &pos_measurement_result_item->time_stamp;
    _EQ_CHECK_INT(timeStamp->system_frame_number, f1_timeStamp->system_frame_number);
    f1ap_time_stamp_slot_index_t *f1_slot_index = &f1_timeStamp->slot_index;
    f1ap_time_stamp_slot_index_t *slot_index = &timeStamp->slot_index;

    switch (f1_slot_index->present) {
      case F1AP_TIME_STAMP_SLOT_INDEX_PR_NOTHING:
        break;
      case F1AP_TIME_STAMP_SLOT_INDEX_PR_SCS_15:
        _EQ_CHECK_INT(slot_index->choice.scs_15, f1_slot_index->choice.scs_15);
        break;
      case F1AP_TIME_STAMP_SLOT_INDEX_PR_SCS_30:
        _EQ_CHECK_INT(slot_index->choice.scs_30, f1_slot_index->choice.scs_30);
        break;
      case F1AP_TIME_STAMP_SLOT_INDEX_PR_SCS_60:
        _EQ_CHECK_INT(slot_index->choice.scs_60, f1_slot_index->choice.scs_60);
        break;
      case F1AP_TIME_STAMP_SLOT_INDEX_PR_SCS_120:
        _EQ_CHECK_INT(slot_index->choice.scs_120, f1_slot_index->choice.scs_120);
        break;
      default:
        AssertError(false, return false, "Illegal slotIndex value %d\n", f1_slot_index->present);
        break;
    }
  }
  return true;
}

/**
 * @brief Encode F1 positioning information request to ASN.1
 */
F1AP_F1AP_PDU_t *encode_positioning_information_req(const f1ap_positioning_information_req_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_PositioningInformationExchange;
  tmp->criticality = F1AP_Criticality_reject;
  tmp->value.present = F1AP_InitiatingMessage__value_PR_PositioningInformationRequest;
  F1AP_PositioningInformationRequest_t *out = (tmp->value.choice.PositioningInformationRequest = calloc_or_fail(1, sizeof(*tmp->value.choice.PositioningInformationRequest)));

  /* mandatory : GNB_CU_UE_F1AP_ID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningInformationRequestIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_PositioningInformationRequestIEs__value_PR_GNB_CU_UE_F1AP_ID;
  asn1cCallocOne(ie1->value.choice.GNB_CU_UE_F1AP_ID, msg->gNB_CU_ue_id);

  /* mandatory : GNB_DU_UE_F1AP_ID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningInformationRequestIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_PositioningInformationRequestIEs__value_PR_GNB_DU_UE_F1AP_ID;
  asn1cCallocOne(ie2->value.choice.GNB_DU_UE_F1AP_ID, msg->gNB_DU_ue_id);

  return pdu;
}

/**
 * @brief Decode F1 positioning information request
 */
bool decode_positioning_information_req(const F1AP_F1AP_PDU_t *pdu, f1ap_positioning_information_req_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_PositioningInformationRequest_t *in = pdu->choice.initiatingMessage->value.choice.PositioningInformationRequest;
  F1AP_PositioningInformationRequestIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_PositioningInformationRequestIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningInformationRequestIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID,
                   true);

  for (int i = 0; i < in->protocolIEs->list.count; ++i) {
    ie = in->protocolIEs->list.array[i];
    AssertError(ie != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningInformationRequestIEs__value_PR_GNB_CU_UE_F1AP_ID);
        out->gNB_CU_ue_id = *ie->value.choice.GNB_CU_UE_F1AP_ID;
        break;
      case F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningInformationRequestIEs__value_PR_GNB_DU_UE_F1AP_ID);
        out->gNB_DU_ue_id = *ie->value.choice.GNB_DU_UE_F1AP_ID;
        break;
      case F1AP_ProtocolIE_ID_id_RequestedSRSTransmissionCharacteristics:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld not handled, skipping\n", ie->id);
        break;
      default:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld unknown, skipping\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1 positioning information request deep copy
 */
f1ap_positioning_information_req_t cp_positioning_information_req(const f1ap_positioning_information_req_t *orig)
{
  /* copy all mandatory fields that are not dynamic memory */
  f1ap_positioning_information_req_t cp = {
      .gNB_CU_ue_id = orig->gNB_CU_ue_id,
      .gNB_DU_ue_id = orig->gNB_DU_ue_id,
  };

  return cp;
}

/**
 * @brief F1 positioning information request equality check
 */
bool eq_positioning_information_req(const f1ap_positioning_information_req_t *a, const f1ap_positioning_information_req_t *b)
{
  _EQ_CHECK_INT(a->gNB_CU_ue_id, b->gNB_CU_ue_id);
  _EQ_CHECK_INT(a->gNB_DU_ue_id, b->gNB_DU_ue_id);

  return true;
}

/**
 * @brief Free Allocated F1 positioning information request
 */
void free_positioning_information_req(f1ap_positioning_information_req_t *msg)
{
  // nothing to free
  UNUSED(msg);
}

/**
 * @brief Encode F1 positioning information response to ASN.1
 */
F1AP_F1AP_PDU_t *encode_positioning_information_resp(const f1ap_positioning_information_resp_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_successfulOutcome;
  asn1cCalloc(pdu->choice.successfulOutcome, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_PositioningInformationExchange;
  tmp->criticality = F1AP_Criticality_reject;
  tmp->value.present = F1AP_SuccessfulOutcome__value_PR_PositioningInformationResponse;
  F1AP_PositioningInformationResponse_t *out = (tmp->value.choice.PositioningInformationResponse = calloc_or_fail(1, sizeof(*tmp->value.choice.PositioningInformationResponse)));

  /* mandatory : GNB_CU_UE_F1AP_ID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningInformationResponseIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_PositioningInformationResponseIEs__value_PR_GNB_CU_UE_F1AP_ID;
  asn1cCallocOne(ie1->value.choice.GNB_CU_UE_F1AP_ID, msg->gNB_CU_ue_id);

  /* mandatory : GNB_DU_UE_F1AP_ID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningInformationResponseIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_PositioningInformationResponseIEs__value_PR_GNB_DU_UE_F1AP_ID;
  asn1cCallocOne(ie2->value.choice.GNB_DU_UE_F1AP_ID, msg->gNB_DU_ue_id);

  /* optional : SRSConfiguration */
  if (msg->srs_configuration) {
    asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningInformationResponseIEs_t, ie3);
    ie3->id = F1AP_ProtocolIE_ID_id_SRSConfiguration;
    ie3->criticality = F1AP_Criticality_ignore;
    ie3->value.present = F1AP_PositioningInformationResponseIEs__value_PR_SRSConfiguration;
    f1ap_srs_carrier_list_t *srs_carrier_list = &msg->srs_configuration->srs_carrier_list;
    if (!ie3->value.choice.SRSConfiguration)
      ie3->value.choice.SRSConfiguration = calloc_or_fail(1, sizeof(*ie3->value.choice.SRSConfiguration));
    asn1cCallocOne(ie3->value.choice.SRSConfiguration->sRSCarrier_List, encode_srs_carrier_list(srs_carrier_list));
  }

  return pdu;
}

/**
 * @brief Decode F1 positioning information response
 */
bool decode_positioning_information_resp(const F1AP_F1AP_PDU_t *pdu, f1ap_positioning_information_resp_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_PositioningInformationResponse_t *in = pdu->choice.successfulOutcome->value.choice.PositioningInformationResponse;
  F1AP_PositioningInformationResponseIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_PositioningInformationResponseIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningInformationResponseIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningInformationResponseIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_SRSConfiguration,
                   false);

  for (int i = 0; i < in->protocolIEs->list.count; ++i) {
    ie = in->protocolIEs->list.array[i];
    AssertError(ie != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningInformationResponseIEs__value_PR_GNB_CU_UE_F1AP_ID);
        out->gNB_CU_ue_id = *ie->value.choice.GNB_CU_UE_F1AP_ID;
        break;
      case F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningInformationResponseIEs__value_PR_GNB_DU_UE_F1AP_ID);
        out->gNB_DU_ue_id = *ie->value.choice.GNB_DU_UE_F1AP_ID;
        break;
      case F1AP_ProtocolIE_ID_id_SRSConfiguration:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningInformationResponseIEs__value_PR_SRSConfiguration);
        F1AP_SRSCarrier_List_t *f1_sRSCarrier_List = ie->value.choice.SRSConfiguration->sRSCarrier_List;
        out->srs_configuration = calloc_or_fail(1, sizeof(*out->srs_configuration));
        f1ap_srs_carrier_list_t *srs_carrier_list = &out->srs_configuration->srs_carrier_list;
        decode_srs_carrier_list(srs_carrier_list, f1_sRSCarrier_List);
        break;
      case F1AP_ProtocolIE_ID_id_SFNInitialisationTime:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld not handled, skipping\n", ie->id);
        break;
      case F1AP_ProtocolIE_ID_id_CriticalityDiagnostics:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld not handled, skipping\n", ie->id);
        break;
      default:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld unknown, skipping\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1 positioning information response deep copy
 */
f1ap_positioning_information_resp_t cp_positioning_information_resp(const f1ap_positioning_information_resp_t *orig)
{
  /* copy all mandatory fields that are not dynamic memory */
  f1ap_positioning_information_resp_t cp = {
      .gNB_CU_ue_id = orig->gNB_CU_ue_id,
      .gNB_DU_ue_id = orig->gNB_DU_ue_id,
  };

  /* optional: SRS Configuration */
  if (orig->srs_configuration) {
    cp.srs_configuration = calloc_or_fail(1, sizeof(*cp.srs_configuration));
    f1ap_srs_carrier_list_t *srs_carrier_list_cp = &cp.srs_configuration->srs_carrier_list;
    f1ap_srs_carrier_list_t *srs_carrier_list = &orig->srs_configuration->srs_carrier_list;
    *srs_carrier_list_cp = cp_srs_carrier_list(srs_carrier_list);
  }

  return cp;
}

/**
 * @brief F1 positioning information response equality check
 */
bool eq_positioning_information_resp(const f1ap_positioning_information_resp_t *a, const f1ap_positioning_information_resp_t *b)
{
  _EQ_CHECK_INT(a->gNB_CU_ue_id, b->gNB_CU_ue_id);
  _EQ_CHECK_INT(a->gNB_DU_ue_id, b->gNB_DU_ue_id);

  /* optional: SRS Configuration (O) */
  if ((a->srs_configuration == NULL) != (b->srs_configuration == NULL)) {
    return false;
  }
  if (a->srs_configuration) {
    f1ap_srs_carrier_list_t *srs_carrier_list_a = &a->srs_configuration->srs_carrier_list;
    f1ap_srs_carrier_list_t *srs_carrier_list_b = &b->srs_configuration->srs_carrier_list;
    _F1_CHECK_EXP(eq_srs_carrier_list(srs_carrier_list_a, srs_carrier_list_b));
  }

  return true;
}

/**
 * @brief Free Allocated F1 positioning information response
 */
void free_positioning_information_resp(f1ap_positioning_information_resp_t *msg)
{
  /* SRS Configuration (O) */
  if (msg->srs_configuration) {
    f1ap_srs_carrier_list_t *srs_carrier_list = &msg->srs_configuration->srs_carrier_list;
    free_srs_carrier_list(srs_carrier_list);
    free(msg->srs_configuration);
  }
}

/**
 * @brief Encode F1 positioning information failure to ASN.1
 */
F1AP_F1AP_PDU_t *encode_positioning_information_failure(const f1ap_positioning_information_failure_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_unsuccessfulOutcome;
  asn1cCalloc(pdu->choice.unsuccessfulOutcome, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_PositioningInformationExchange;
  tmp->criticality = F1AP_Criticality_reject;
  tmp->value.present = F1AP_UnsuccessfulOutcome__value_PR_PositioningInformationFailure;
  F1AP_PositioningInformationFailure_t *out = (tmp->value.choice.PositioningInformationFailure = calloc_or_fail(1, sizeof(*tmp->value.choice.PositioningInformationFailure)));

  /* mandatory : GNB_CU_UE_F1AP_ID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningInformationFailureIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_PositioningInformationFailureIEs__value_PR_GNB_CU_UE_F1AP_ID;
  asn1cCallocOne(ie1->value.choice.GNB_CU_UE_F1AP_ID, msg->gNB_CU_ue_id);

  /* mandatory : GNB_DU_UE_F1AP_ID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningInformationFailureIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_PositioningInformationFailureIEs__value_PR_GNB_DU_UE_F1AP_ID;
  asn1cCallocOne(ie2->value.choice.GNB_DU_UE_F1AP_ID, msg->gNB_DU_ue_id);

  /* mandatory : Cause */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningInformationFailureIEs_t, ie3);
  ie3->id = F1AP_ProtocolIE_ID_id_Cause;
  ie3->criticality = F1AP_Criticality_ignore;
  ie3->value.present = F1AP_PositioningInformationFailureIEs__value_PR_Cause;
  asn1cCallocOne(ie3->value.choice.Cause, encode_f1ap_cause(msg->cause, msg->cause_value));

  return pdu;
}

/**
 * @brief Decode F1 positioning information failure
 */
bool decode_positioning_information_failure(const F1AP_F1AP_PDU_t *pdu, f1ap_positioning_information_failure_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_PositioningInformationFailure_t *in = pdu->choice.unsuccessfulOutcome->value.choice.PositioningInformationFailure;
  F1AP_PositioningInformationFailureIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_PositioningInformationFailureIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningInformationFailureIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningInformationFailureIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_Cause, true);

  for (int i = 0; i < in->protocolIEs->list.count; ++i) {
    ie = in->protocolIEs->list.array[i];
    AssertError(ie != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningInformationFailureIEs__value_PR_GNB_CU_UE_F1AP_ID);
        out->gNB_CU_ue_id = *ie->value.choice.GNB_CU_UE_F1AP_ID;
        break;
      case F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningInformationFailureIEs__value_PR_GNB_DU_UE_F1AP_ID);
        out->gNB_DU_ue_id = *ie->value.choice.GNB_DU_UE_F1AP_ID;
        break;
      case F1AP_ProtocolIE_ID_id_Cause:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningInformationFailureIEs__value_PR_Cause);
        _F1_CHECK_EXP(decode_f1ap_cause(*ie->value.choice.Cause, &out->cause, &out->cause_value));
        break;
      case F1AP_ProtocolIE_ID_id_CriticalityDiagnostics:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld not handled, skipping\n", ie->id);
        break;
      default:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld unknown, skipping\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1 positioning information failure deep copy
 */
f1ap_positioning_information_failure_t cp_positioning_information_failure(const f1ap_positioning_information_failure_t *orig)
{
  /* copy all mandatory fields that are not dynamic memory */
  f1ap_positioning_information_failure_t cp = {
      .gNB_CU_ue_id = orig->gNB_CU_ue_id,
      .gNB_DU_ue_id = orig->gNB_DU_ue_id,
      .cause = orig->cause,
      .cause_value = orig->cause_value,
  };

  return cp;
}

/**
 * @brief F1 positioning information failure equality check
 */
bool eq_positioning_information_failure(const f1ap_positioning_information_failure_t *a,
                                        const f1ap_positioning_information_failure_t *b)
{
  _EQ_CHECK_INT(a->gNB_CU_ue_id, b->gNB_CU_ue_id);
  _EQ_CHECK_INT(a->gNB_DU_ue_id, b->gNB_DU_ue_id);
  _EQ_CHECK_INT(a->cause, b->cause);
  _EQ_CHECK_LONG(a->cause_value, b->cause_value);

  return true;
}

/**
 * @brief Free Allocated F1 positioning information failure
 */
void free_positioning_information_failure(f1ap_positioning_information_failure_t *msg)
{
  // nothing to free
  UNUSED(msg);
}

/**
 * @brief Encode F1 positioning activation request to ASN.1
 */
F1AP_F1AP_PDU_t *encode_positioning_activation_req(const f1ap_positioning_activation_req_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_PositioningActivation;
  tmp->criticality = F1AP_Criticality_reject;
  tmp->value.present = F1AP_InitiatingMessage__value_PR_PositioningActivationRequest;
  F1AP_PositioningActivationRequest_t *out = (tmp->value.choice.PositioningActivationRequest = calloc_or_fail(1, sizeof(*tmp->value.choice.PositioningActivationRequest)));

  /* mandatory : GNB_CU_UE_F1AP_ID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningActivationRequestIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_PositioningActivationRequestIEs__value_PR_GNB_CU_UE_F1AP_ID;
  asn1cCallocOne(ie1->value.choice.GNB_CU_UE_F1AP_ID, msg->gNB_CU_ue_id);

  /* mandatory : GNB_DU_UE_F1AP_ID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningActivationRequestIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_PositioningActivationRequestIEs__value_PR_GNB_DU_UE_F1AP_ID;
  asn1cCallocOne(ie2->value.choice.GNB_DU_UE_F1AP_ID, msg->gNB_DU_ue_id);

  /* mandatory : SRS type */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningActivationRequestIEs_t, ie3);
  ie3->id = F1AP_ProtocolIE_ID_id_SRSType;
  ie3->criticality = F1AP_Criticality_reject;
  ie3->value.present = F1AP_PositioningActivationRequestIEs__value_PR_SRSType;
  asn1cCallocOne(ie3->value.choice.SRSType, encode_f1ap_srstype(&msg->srs_type));

  return pdu;
}

/**
 * @brief Decode F1 positioning activation request
 */
bool decode_positioning_activation_req(const F1AP_F1AP_PDU_t *pdu, f1ap_positioning_activation_req_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_PositioningActivationRequest_t *in = pdu->choice.initiatingMessage->value.choice.PositioningActivationRequest;
  F1AP_PositioningActivationRequestIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_PositioningActivationRequestIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningActivationRequestIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningActivationRequestIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_SRSType, true);

  for (int i = 0; i < in->protocolIEs->list.count; ++i) {
    ie = in->protocolIEs->list.array[i];
    AssertError(ie != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningActivationRequestIEs__value_PR_GNB_CU_UE_F1AP_ID);
        out->gNB_CU_ue_id = *ie->value.choice.GNB_CU_UE_F1AP_ID;
        break;
      case F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningActivationRequestIEs__value_PR_GNB_DU_UE_F1AP_ID);
        out->gNB_DU_ue_id = *ie->value.choice.GNB_DU_UE_F1AP_ID;
        break;
      case F1AP_ProtocolIE_ID_id_SRSType:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningActivationRequestIEs__value_PR_SRSType);
        _F1_CHECK_EXP(decode_f1ap_srstype(ie->value.choice.SRSType, &out->srs_type));
        break;
      case F1AP_ProtocolIE_ID_id_ActivationTime:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld not handled, skipping\n", ie->id);
        break;
      default:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld unknown, skipping\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1 positioning activation request deep copy
 */
f1ap_positioning_activation_req_t cp_positioning_activation_req(const f1ap_positioning_activation_req_t *orig)
{
  /* copy all mandatory fields that are not dynamic memory */
  f1ap_positioning_activation_req_t cp = {
      .gNB_CU_ue_id = orig->gNB_CU_ue_id,
      .gNB_DU_ue_id = orig->gNB_DU_ue_id,
      .srs_type.present = orig->srs_type.present,
  };

  switch (orig->srs_type.present) {
    case F1AP_SRS_TYPE_PR_NOTHING:
      // nothing to copy
      break;
    case F1AP_SRS_TYPE_PR_SEMIPERSISTENTSRS:
      cp.srs_type.choice.srs_resource_set_id = calloc_or_fail(1, sizeof(*cp.srs_type.choice.srs_resource_set_id));
      *cp.srs_type.choice.srs_resource_set_id = *orig->srs_type.choice.srs_resource_set_id;
      break;
    case F1AP_SRS_TYPE_PR_APERIODICSRS:
      cp.srs_type.choice.aperiodic = calloc_or_fail(1, sizeof(*cp.srs_type.choice.aperiodic));
      *cp.srs_type.choice.aperiodic = *orig->srs_type.choice.aperiodic;
      break;
    default:
      PRINT_ERROR("received illegal SRS type %d\n", orig->srs_type.present);
      break;
  }

  return cp;
}

/**
 * @brief F1 positioning activation request equality check
 */
bool eq_positioning_activation_req(const f1ap_positioning_activation_req_t *a, const f1ap_positioning_activation_req_t *b)
{
  _EQ_CHECK_INT(a->gNB_CU_ue_id, b->gNB_CU_ue_id);
  _EQ_CHECK_INT(a->gNB_DU_ue_id, b->gNB_DU_ue_id);
  _EQ_CHECK_INT(a->srs_type.present, b->srs_type.present);

  const f1ap_srs_type_t *srs_type_a = &a->srs_type;
  const f1ap_srs_type_t *srs_type_b = &b->srs_type;
  switch (srs_type_a->present) {
    case F1AP_SRS_TYPE_PR_NOTHING:
      // nothing to check
      break;
    case F1AP_SRS_TYPE_PR_SEMIPERSISTENTSRS:
      _EQ_CHECK_INT(*srs_type_a->choice.srs_resource_set_id, *srs_type_b->choice.srs_resource_set_id);
      break;
    case F1AP_SRS_TYPE_PR_APERIODICSRS:
      _EQ_CHECK_INT(*srs_type_a->choice.aperiodic, *srs_type_b->choice.aperiodic);
      break;
    default:
      PRINT_ERROR("received illegal SRS type %d\n", srs_type_a->present);
      break;
  }

  return true;
}

/**
 * @brief Free Allocated F1 activation request
 */
void free_positioning_activation_req(f1ap_positioning_activation_req_t *msg)
{
  DevAssert(msg != NULL);

  switch (msg->srs_type.present) {
    case F1AP_SRS_TYPE_PR_NOTHING:
      // nothing to free
      return;
    case F1AP_SRS_TYPE_PR_SEMIPERSISTENTSRS:
      free(msg->srs_type.choice.srs_resource_set_id);
      break;
    case F1AP_SRS_TYPE_PR_APERIODICSRS:
      free(msg->srs_type.choice.aperiodic);
      break;
    default:
      PRINT_ERROR("received illegal SRS type %d\n", msg->srs_type.present);
      break;
  }
}

/**
 * @brief Encode F1 positioning activation response to ASN.1
 */
F1AP_F1AP_PDU_t *encode_positioning_activation_resp(const f1ap_positioning_activation_resp_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_successfulOutcome;
  asn1cCalloc(pdu->choice.successfulOutcome, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_PositioningActivation;
  tmp->criticality = F1AP_Criticality_reject;
  tmp->value.present = F1AP_SuccessfulOutcome__value_PR_PositioningActivationResponse;
  F1AP_PositioningActivationResponse_t *out = (tmp->value.choice.PositioningActivationResponse = calloc_or_fail(1, sizeof(*tmp->value.choice.PositioningActivationResponse)));

  /* mandatory : GNB_CU_UE_F1AP_ID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningActivationResponseIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_PositioningActivationResponseIEs__value_PR_GNB_CU_UE_F1AP_ID;
  asn1cCallocOne(ie1->value.choice.GNB_CU_UE_F1AP_ID, msg->gNB_CU_ue_id);

  /* mandatory : GNB_DU_UE_F1AP_ID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningActivationResponseIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_PositioningActivationResponseIEs__value_PR_GNB_DU_UE_F1AP_ID;
  asn1cCallocOne(ie2->value.choice.GNB_DU_UE_F1AP_ID, msg->gNB_DU_ue_id);

  return pdu;
}

/**
 * @brief Decode F1 positioning activation response
 */
bool decode_positioning_activation_resp(const F1AP_F1AP_PDU_t *pdu, f1ap_positioning_activation_resp_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_PositioningActivationResponse_t *in = pdu->choice.successfulOutcome->value.choice.PositioningActivationResponse;
  F1AP_PositioningActivationResponseIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_PositioningActivationResponseIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningActivationResponseIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID,
                   true);

  for (int i = 0; i < in->protocolIEs->list.count; ++i) {
    ie = in->protocolIEs->list.array[i];
    AssertError(ie != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningActivationResponseIEs__value_PR_GNB_CU_UE_F1AP_ID);
        out->gNB_CU_ue_id = *ie->value.choice.GNB_CU_UE_F1AP_ID;
        break;
      case F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningActivationResponseIEs__value_PR_GNB_DU_UE_F1AP_ID);
        out->gNB_DU_ue_id = *ie->value.choice.GNB_DU_UE_F1AP_ID;
        break;
      case F1AP_ProtocolIE_ID_id_SystemFrameNumber:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld not handled, skipping\n", ie->id);
        break;
      case F1AP_ProtocolIE_ID_id_SlotNumber:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld not handled, skipping\n", ie->id);
        break;
      case F1AP_ProtocolIE_ID_id_CriticalityDiagnostics:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld not handled, skipping\n", ie->id);
        break;
      default:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld unknown, skipping\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1 positioning activation response deep copy
 */
f1ap_positioning_activation_resp_t cp_positioning_activation_resp(const f1ap_positioning_activation_resp_t *orig)
{
  /* copy all mandatory fields that are not dynamic memory */
  f1ap_positioning_activation_resp_t cp = {
      .gNB_CU_ue_id = orig->gNB_CU_ue_id,
      .gNB_DU_ue_id = orig->gNB_DU_ue_id,
  };

  return cp;
}

/**
 * @brief F1 positioning activation response equality check
 */
bool eq_positioning_activation_resp(const f1ap_positioning_activation_resp_t *a, const f1ap_positioning_activation_resp_t *b)
{
  _EQ_CHECK_INT(a->gNB_CU_ue_id, b->gNB_CU_ue_id);
  _EQ_CHECK_INT(a->gNB_DU_ue_id, b->gNB_DU_ue_id);

  return true;
}

/**
 * @brief Free Allocated F1 positioning activation response
 */
void free_positioning_activation_resp(f1ap_positioning_activation_resp_t *msg)
{
  // nothing to free
  UNUSED(msg);
}

/**
 * @brief Encode F1 positioning activation failure to ASN.1
 */
F1AP_F1AP_PDU_t *encode_positioning_activation_failure(const f1ap_positioning_activation_failure_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_unsuccessfulOutcome;
  asn1cCalloc(pdu->choice.unsuccessfulOutcome, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_PositioningActivation;
  tmp->criticality = F1AP_Criticality_reject;
  tmp->value.present = F1AP_UnsuccessfulOutcome__value_PR_PositioningActivationFailure;
  F1AP_PositioningActivationFailure_t *out = (tmp->value.choice.PositioningActivationFailure = calloc_or_fail(1, sizeof(*tmp->value.choice.PositioningActivationFailure)));

  /* mandatory : GNB_CU_UE_F1AP_ID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningActivationFailureIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_PositioningActivationFailureIEs__value_PR_GNB_CU_UE_F1AP_ID;
  asn1cCallocOne(ie1->value.choice.GNB_CU_UE_F1AP_ID, msg->gNB_CU_ue_id);

  /* mandatory : GNB_DU_UE_F1AP_ID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningActivationFailureIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_PositioningActivationFailureIEs__value_PR_GNB_DU_UE_F1AP_ID;
  asn1cCallocOne(ie2->value.choice.GNB_DU_UE_F1AP_ID, msg->gNB_DU_ue_id);

  /* mandatory : Cause */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningActivationFailureIEs_t, ie3);
  ie3->id = F1AP_ProtocolIE_ID_id_Cause;
  ie3->criticality = F1AP_Criticality_ignore;
  ie3->value.present = F1AP_PositioningActivationFailureIEs__value_PR_Cause;
  asn1cCallocOne(ie3->value.choice.Cause, encode_f1ap_cause(msg->cause, msg->cause_value));

  return pdu;
}

/**
 * @brief Decode F1 positioning activation failure
 */
bool decode_positioning_activation_failure(const F1AP_F1AP_PDU_t *pdu, f1ap_positioning_activation_failure_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_PositioningActivationFailure_t *in = pdu->choice.unsuccessfulOutcome->value.choice.PositioningActivationFailure;
  F1AP_PositioningActivationFailureIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_PositioningActivationFailureIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningActivationFailureIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningActivationFailureIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_Cause, true);

  for (int i = 0; i < in->protocolIEs->list.count; ++i) {
    ie = in->protocolIEs->list.array[i];
    AssertError(ie != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningActivationFailureIEs__value_PR_GNB_CU_UE_F1AP_ID);
        out->gNB_CU_ue_id = *ie->value.choice.GNB_CU_UE_F1AP_ID;
        break;
      case F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningActivationFailureIEs__value_PR_GNB_DU_UE_F1AP_ID);
        out->gNB_DU_ue_id = *ie->value.choice.GNB_DU_UE_F1AP_ID;
        break;
      case F1AP_ProtocolIE_ID_id_Cause:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningActivationFailureIEs__value_PR_Cause);
        _F1_CHECK_EXP(decode_f1ap_cause(*ie->value.choice.Cause, &out->cause, &out->cause_value));
        break;
      case F1AP_ProtocolIE_ID_id_CriticalityDiagnostics:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld not handled, skipping\n", ie->id);
        break;
      default:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld unknown, skipping\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1 positioning activation failure deep copy
 */
f1ap_positioning_activation_failure_t cp_positioning_activation_failure(const f1ap_positioning_activation_failure_t *orig)
{
  /* copy all mandatory fields that are not dynamic memory */
  f1ap_positioning_activation_failure_t cp = {
      .gNB_CU_ue_id = orig->gNB_CU_ue_id,
      .gNB_DU_ue_id = orig->gNB_DU_ue_id,
      .cause = orig->cause,
      .cause_value = orig->cause_value,
  };

  return cp;
}

/**
 * @brief F1 positioning activation failure equality check
 */
bool eq_positioning_activation_failure(const f1ap_positioning_activation_failure_t *a,
                                       const f1ap_positioning_activation_failure_t *b)
{
  _EQ_CHECK_INT(a->gNB_CU_ue_id, b->gNB_CU_ue_id);
  _EQ_CHECK_INT(a->gNB_DU_ue_id, b->gNB_DU_ue_id);
  _EQ_CHECK_INT(a->cause, b->cause);
  _EQ_CHECK_LONG(a->cause_value, b->cause_value);

  return true;
}

/**
 * @brief Free Allocated F1 positioning activation failure
 */
void free_positioning_activation_failure(f1ap_positioning_activation_failure_t *msg)
{
  // nothing to free
  UNUSED(msg);
}

/**
 * @brief Encode F1 positioning deactivation to ASN.1
 */
F1AP_F1AP_PDU_t *encode_positioning_deactivation(const f1ap_positioning_deactivation_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_PositioningDeactivation;
  tmp->criticality = F1AP_Criticality_ignore;
  tmp->value.present = F1AP_InitiatingMessage__value_PR_PositioningDeactivation;
  F1AP_PositioningDeactivation_t *out = (tmp->value.choice.PositioningDeactivation = calloc_or_fail(1, sizeof(*tmp->value.choice.PositioningDeactivation)));

  /* mandatory : GNB_CU_UE_F1AP_ID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningDeactivationIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_PositioningDeactivationIEs__value_PR_GNB_CU_UE_F1AP_ID;
  asn1cCallocOne(ie1->value.choice.GNB_CU_UE_F1AP_ID, msg->gNB_CU_ue_id);

  /* mandatory : GNB_DU_UE_F1AP_ID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningDeactivationIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_PositioningDeactivationIEs__value_PR_GNB_DU_UE_F1AP_ID;
  asn1cCallocOne(ie2->value.choice.GNB_DU_UE_F1AP_ID, msg->gNB_DU_ue_id);

  /* mandatory : CHOICE ABORT TRANSMISSION */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningDeactivationIEs_t, ie3);
  ie3->id = F1AP_ProtocolIE_ID_id_AbortTransmission;
  ie3->criticality = F1AP_Criticality_ignore;
  ie3->value.present = F1AP_PositioningDeactivationIEs__value_PR_AbortTransmission;
  asn1cCallocOne(ie3->value.choice.AbortTransmission, encode_f1ap_abort_transmission(&msg->abort_transmission));

  return pdu;
}

/**
 * @brief Decode F1 positioning deactivation
 */
bool decode_positioning_deactivation(const F1AP_F1AP_PDU_t *pdu, f1ap_positioning_deactivation_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_PositioningDeactivation_t *in = pdu->choice.initiatingMessage->value.choice.PositioningDeactivation;
  F1AP_PositioningDeactivationIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_PositioningDeactivationIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID, true);
  F1AP_LIB_FIND_IE(F1AP_PositioningDeactivationIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID, true);
  F1AP_LIB_FIND_IE(F1AP_PositioningDeactivationIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_AbortTransmission, true);

  for (int i = 0; i < in->protocolIEs->list.count; ++i) {
    ie = in->protocolIEs->list.array[i];
    AssertError(ie != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningDeactivationIEs__value_PR_GNB_CU_UE_F1AP_ID);
        out->gNB_CU_ue_id = *ie->value.choice.GNB_CU_UE_F1AP_ID;
        break;
      case F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningDeactivationIEs__value_PR_GNB_DU_UE_F1AP_ID);
        out->gNB_DU_ue_id = *ie->value.choice.GNB_DU_UE_F1AP_ID;
        break;
      case F1AP_ProtocolIE_ID_id_AbortTransmission:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningDeactivationIEs__value_PR_AbortTransmission);
        _F1_CHECK_EXP(decode_f1ap_abort_transmission(ie->value.choice.AbortTransmission, &out->abort_transmission));
        break;
      default:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld unknown, skipping\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1 positioning deactivation deep copy
 */
f1ap_positioning_deactivation_t cp_positioning_deactivation(const f1ap_positioning_deactivation_t *orig)
{
  /* copy all mandatory fields that are not dynamic memory */
  f1ap_positioning_deactivation_t cp = {
      .gNB_CU_ue_id = orig->gNB_CU_ue_id,
      .gNB_DU_ue_id = orig->gNB_DU_ue_id,
      .abort_transmission.present = orig->abort_transmission.present,
  };
  // do this properly abort mission
  switch (orig->abort_transmission.present) {
    case F1AP_ABORT_TRANSMISSION_PR_NOTHING:
      // nothing to copy
      break;
    case F1AP_ABORT_TRANSMISSION_PR_SRSRESOURCESETID:
      cp.abort_transmission.choice.srs_resource_set_id = orig->abort_transmission.choice.srs_resource_set_id;
      break;
    case F1AP_ABORT_TRANSMISSION_PR_RELEASEALL:
      cp.abort_transmission.choice.release_all = orig->abort_transmission.choice.release_all;
      break;
    default:
      PRINT_ERROR("received illegal Abort Transmission value %d\n", orig->abort_transmission.present);
      break;
  }

  return cp;
}

/**
 * @brief F1 positioning deactivation equality check
 */
bool eq_positioning_deactivation(const f1ap_positioning_deactivation_t *a, const f1ap_positioning_deactivation_t *b)
{
  _EQ_CHECK_INT(a->gNB_CU_ue_id, b->gNB_CU_ue_id);
  _EQ_CHECK_INT(a->gNB_DU_ue_id, b->gNB_DU_ue_id);
  _EQ_CHECK_INT(a->abort_transmission.present, b->abort_transmission.present);
  switch (a->abort_transmission.present) {
    case F1AP_ABORT_TRANSMISSION_PR_NOTHING:
      // nothing to check
      break;
    case F1AP_ABORT_TRANSMISSION_PR_SRSRESOURCESETID:
      _EQ_CHECK_INT(a->abort_transmission.choice.srs_resource_set_id, b->abort_transmission.choice.srs_resource_set_id);
      break;
    case F1AP_ABORT_TRANSMISSION_PR_RELEASEALL:
      _EQ_CHECK_INT(a->abort_transmission.choice.release_all, b->abort_transmission.choice.release_all);
      break;
    default:
      PRINT_ERROR("received illegal Abort Transmission value %d\n", a->abort_transmission.present);
      break;
  }

  return true;
}

/**
 * @brief Free Allocated F1 positioning activation failure
 */
void free_positioning_deactivation(f1ap_positioning_deactivation_t *msg)
{
  // nothing to free
  UNUSED(msg);
}

/**
 * @brief Encode F1 positioning information update to ASN.1
 */
F1AP_F1AP_PDU_t *encode_positioning_information_update(const f1ap_positioning_information_update_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_PositioningInformationUpdate;
  tmp->criticality = F1AP_Criticality_ignore;
  tmp->value.present = F1AP_InitiatingMessage__value_PR_PositioningInformationUpdate;
  F1AP_PositioningInformationUpdate_t *out = (tmp->value.choice.PositioningInformationUpdate = calloc_or_fail(1, sizeof(*tmp->value.choice.PositioningInformationUpdate)));

  /* mandatory : GNB_CU_UE_F1AP_ID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningInformationUpdateIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_PositioningInformationUpdateIEs__value_PR_GNB_CU_UE_F1AP_ID;
  asn1cCallocOne(ie1->value.choice.GNB_CU_UE_F1AP_ID, msg->gNB_CU_ue_id);

  /* mandatory : GNB_DU_UE_F1AP_ID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningInformationUpdateIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_PositioningInformationUpdateIEs__value_PR_GNB_DU_UE_F1AP_ID;
  asn1cCallocOne(ie2->value.choice.GNB_DU_UE_F1AP_ID, msg->gNB_DU_ue_id);

  /* optional : SRSConfiguration */
  if (msg->srs_configuration) {
    asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningInformationUpdateIEs_t, ie3);
    ie3->id = F1AP_ProtocolIE_ID_id_SRSConfiguration;
    ie3->criticality = F1AP_Criticality_ignore;
    ie3->value.present = F1AP_PositioningInformationUpdateIEs__value_PR_SRSConfiguration;
    f1ap_srs_carrier_list_t *srs_carrier_list = &msg->srs_configuration->srs_carrier_list;
    if (!ie3->value.choice.SRSConfiguration)
      ie3->value.choice.SRSConfiguration = calloc_or_fail(1, sizeof(*ie3->value.choice.SRSConfiguration));
    asn1cCallocOne(ie3->value.choice.SRSConfiguration->sRSCarrier_List, encode_srs_carrier_list(srs_carrier_list));
  }

  return pdu;
}

/**
 * @brief Decode F1 positioning information update
 */
bool decode_positioning_information_update(const F1AP_F1AP_PDU_t *pdu, f1ap_positioning_information_update_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_PositioningInformationUpdate_t *in = pdu->choice.initiatingMessage->value.choice.PositioningInformationUpdate;
  F1AP_PositioningInformationUpdateIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_PositioningInformationUpdateIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningInformationUpdateIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningInformationUpdateIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_SRSConfiguration,
                   false);

  for (int i = 0; i < in->protocolIEs->list.count; ++i) {
    ie = in->protocolIEs->list.array[i];
    AssertError(ie != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningInformationUpdateIEs__value_PR_GNB_CU_UE_F1AP_ID);
        out->gNB_CU_ue_id = *ie->value.choice.GNB_CU_UE_F1AP_ID;
        break;
      case F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningInformationUpdateIEs__value_PR_GNB_DU_UE_F1AP_ID);
        out->gNB_DU_ue_id = *ie->value.choice.GNB_DU_UE_F1AP_ID;
        break;
      case F1AP_ProtocolIE_ID_id_SRSConfiguration:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningInformationUpdateIEs__value_PR_SRSConfiguration);
        F1AP_SRSCarrier_List_t *f1_sRSCarrier_List = ie->value.choice.SRSConfiguration->sRSCarrier_List;
        out->srs_configuration = calloc_or_fail(1, sizeof(*out->srs_configuration));
        f1ap_srs_carrier_list_t *srs_carrier_list = &out->srs_configuration->srs_carrier_list;
        decode_srs_carrier_list(srs_carrier_list, f1_sRSCarrier_List);
        break;
      case F1AP_ProtocolIE_ID_id_SFNInitialisationTime:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld not handled, skipping\n", ie->id);
        break;
      default:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld unknown, skipping\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1 positioning information update deep copy
 */
f1ap_positioning_information_update_t cp_positioning_information_update(const f1ap_positioning_information_update_t *orig)
{
  /* copy all mandatory fields that are not dynamic memory */
  f1ap_positioning_information_update_t cp = {
      .gNB_CU_ue_id = orig->gNB_CU_ue_id,
      .gNB_DU_ue_id = orig->gNB_DU_ue_id,
  };

  /* optional: SRS Configuration */
  if (orig->srs_configuration) {
    cp.srs_configuration = calloc_or_fail(1, sizeof(*cp.srs_configuration));
    f1ap_srs_carrier_list_t *srs_carrier_list_cp = &cp.srs_configuration->srs_carrier_list;
    f1ap_srs_carrier_list_t *srs_carrier_list = &orig->srs_configuration->srs_carrier_list;
    *srs_carrier_list_cp = cp_srs_carrier_list(srs_carrier_list);
  }

  return cp;
}

/**
 * @brief F1 positioning information update equality check
 */
bool eq_positioning_information_update(const f1ap_positioning_information_update_t *a,
                                       const f1ap_positioning_information_update_t *b)
{
  _EQ_CHECK_INT(a->gNB_CU_ue_id, b->gNB_CU_ue_id);
  _EQ_CHECK_INT(a->gNB_DU_ue_id, b->gNB_DU_ue_id);

  /* optional: SRS Configuration */
  if ((a->srs_configuration == NULL) != (b->srs_configuration == NULL)) {
    return false;
  }
  if (a->srs_configuration) {
    f1ap_srs_carrier_list_t *srs_carrier_list_a = &a->srs_configuration->srs_carrier_list;
    f1ap_srs_carrier_list_t *srs_carrier_list_b = &b->srs_configuration->srs_carrier_list;
    _F1_CHECK_EXP(eq_srs_carrier_list(srs_carrier_list_a, srs_carrier_list_b));
  }

  return true;
}

/**
 * @brief Free Allocated F1 positioning information update
 */
void free_positioning_information_update(f1ap_positioning_information_update_t *msg)
{
  /* optional: SRS Configuration */
  if (msg->srs_configuration) {
    f1ap_srs_carrier_list_t *srs_carrier_list = &msg->srs_configuration->srs_carrier_list;
    free_srs_carrier_list(srs_carrier_list);
    free(msg->srs_configuration);
  }
}

/**
 * @brief Encode F1 TRP information request to ASN.1
 */
F1AP_F1AP_PDU_t *encode_trp_information_req(const f1ap_trp_information_req_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_TRPInformationExchange;
  tmp->criticality = F1AP_Criticality_reject;
  tmp->value.present = F1AP_InitiatingMessage__value_PR_TRPInformationRequest;
  F1AP_TRPInformationRequest_t *out = (tmp->value.choice.TRPInformationRequest = calloc_or_fail(1, sizeof(*tmp->value.choice.TRPInformationRequest)));

  /* mandatory : TransactionID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_TRPInformationRequestIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_TransactionID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_TRPInformationRequestIEs__value_PR_TransactionID;
  asn1cCallocOne(ie1->value.choice.TransactionID, msg->transaction_id);

  /* TRPList */
  if (msg->has_trp_list) {
    asn1cSequenceAdd(out->protocolIEs->list, F1AP_TRPInformationRequestIEs_t, ie2);
    ie2->id = F1AP_ProtocolIE_ID_id_TRPList;
    ie2->criticality = F1AP_Criticality_ignore;
    ie2->value.present = F1AP_TRPInformationRequestIEs__value_PR_TRPList;
    for (int i = 0; i < msg->trp_list.trp_list_length; i++) {
      // mandatory : TRP List Item (M)
      if (!ie2->value.choice.TRPList)
        ie2->value.choice.TRPList = calloc_or_fail(1, sizeof(*ie2->value.choice.TRPList));
      asn1cSequenceAdd(ie2->value.choice.TRPList->list, F1AP_TRPListItem_t, trplistitem);
      trplistitem->tRPID = msg->trp_list.trp_list_item[i].trp_id;
    }
  }

  /* mandatory : TRPInformationTypeList */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_TRPInformationRequestIEs_t, ie3);
  ie3->id = F1AP_ProtocolIE_ID_id_TRPInformationTypeListTRPReq;
  ie3->criticality = F1AP_Criticality_reject;
  ie3->value.present = F1AP_TRPInformationRequestIEs__value_PR_TRPInformationTypeListTRPReq;
  for (int i = 0; i < msg->trp_information_type_list.trp_information_type_list_length; i++) {
    // TRPInformationTypeItem (M)
    if (!ie3->value.choice.TRPInformationTypeListTRPReq)
      ie3->value.choice.TRPInformationTypeListTRPReq = calloc_or_fail(1, sizeof(*ie3->value.choice.TRPInformationTypeListTRPReq));
    asn1cSequenceAdd(ie3->value.choice.TRPInformationTypeListTRPReq->list, F1AP_TRPInformationTypeItemTRPReq_t, trpinfotypeitem);
    trpinfotypeitem->id = F1AP_ProtocolIE_ID_id_TRPInformationTypeItem;
    trpinfotypeitem->criticality = F1AP_Criticality_reject;
    trpinfotypeitem->value.present = F1AP_TRPInformationTypeItemTRPReq__value_PR_TRPInformationTypeItem;
    asn1cCallocOne(trpinfotypeitem->value.choice.TRPInformationTypeItem, msg->trp_information_type_list.trp_information_type_item[i]);
  }

  return pdu;
}

/**
 * @brief Decode F1 TRP information request
 */
bool decode_trp_information_req(const F1AP_F1AP_PDU_t *pdu, f1ap_trp_information_req_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_TRPInformationRequest_t *in = pdu->choice.initiatingMessage->value.choice.TRPInformationRequest;
  F1AP_TRPInformationRequestIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_TRPInformationRequestIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_TransactionID, true);
  F1AP_LIB_FIND_IE(F1AP_TRPInformationRequestIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_TRPInformationTypeListTRPReq,
                   true);

  for (int i = 0; i < in->protocolIEs->list.count; ++i) {
    ie = in->protocolIEs->list.array[i];
    AssertError(ie != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_TransactionID:
        _EQ_CHECK_INT(ie->value.present, F1AP_TRPInformationRequestIEs__value_PR_TransactionID);
        out->transaction_id = *ie->value.choice.TransactionID;
        break;
      case F1AP_ProtocolIE_ID_id_TRPList:
        _EQ_CHECK_INT(ie->value.present, F1AP_TRPInformationRequestIEs__value_PR_TRPList);
        out->has_trp_list = true;
        uint32_t trp_list_length = ie->value.choice.TRPList->list.count;
        AssertError(trp_list_length > 0, return false, "at least 1 TRP must be present");
        out->trp_list.trp_list_length = trp_list_length;
        out->trp_list.trp_list_item = calloc_or_fail(trp_list_length, sizeof(*out->trp_list.trp_list_item));
        F1AP_TRPList_t *TRPList = ie->value.choice.TRPList;
        for (int i = 0; i < trp_list_length; i++) {
          out->trp_list.trp_list_item[i].trp_id = TRPList->list.array[i]->tRPID;
        }
        break;
      case F1AP_ProtocolIE_ID_id_TRPInformationTypeListTRPReq:
        _EQ_CHECK_INT(ie->value.present, F1AP_TRPInformationRequestIEs__value_PR_TRPInformationTypeListTRPReq);
        uint8_t trp_info_type_list_length = ie->value.choice.TRPInformationTypeListTRPReq->list.count;
        AssertError(trp_info_type_list_length > 0, return false, "at least 1 TRP Information Type must be present");
        f1ap_trp_information_type_list_t *trp_information_type_list = &out->trp_information_type_list;
        trp_information_type_list->trp_information_type_list_length = trp_info_type_list_length;
        trp_information_type_list->trp_information_type_item =
            calloc_or_fail(trp_info_type_list_length, sizeof(*trp_information_type_list->trp_information_type_item));
        for (int i = 0; i < trp_info_type_list_length; i++) {
          F1AP_TRPInformationTypeItemTRPReq_t *f1_trpinfotypeitem =
              (F1AP_TRPInformationTypeItemTRPReq_t *)ie->value.choice.TRPInformationTypeListTRPReq->list.array[i];
          _EQ_CHECK_LONG(f1_trpinfotypeitem->id, F1AP_ProtocolIE_ID_id_TRPInformationTypeItem);
          _EQ_CHECK_INT(f1_trpinfotypeitem->value.present, F1AP_TRPInformationTypeItemTRPReq__value_PR_TRPInformationTypeItem);
          trp_information_type_list->trp_information_type_item[i] = *f1_trpinfotypeitem->value.choice.TRPInformationTypeItem;
        }
        break;
      default:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld unknown, skipping\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1 TRP information request deep copy
 */
f1ap_trp_information_req_t cp_trp_information_req(const f1ap_trp_information_req_t *orig)
{
  /* copy all mandatory fields that are not dynamic memory */
  f1ap_trp_information_req_t cp = {
      .transaction_id = orig->transaction_id,
      .has_trp_list = orig->has_trp_list,
  };

  if (orig->has_trp_list) {
    uint32_t trp_list_length = orig->trp_list.trp_list_length;
    cp.trp_list.trp_list_length = trp_list_length;
    cp.trp_list.trp_list_item = calloc_or_fail(trp_list_length, sizeof(*cp.trp_list.trp_list_item));
    for (int i = 0; i < trp_list_length; i++) {
      cp.trp_list.trp_list_item[i].trp_id = orig->trp_list.trp_list_item[i].trp_id;
    }
  }

  const f1ap_trp_information_type_list_t *trp_information_type_list = &orig->trp_information_type_list;
  f1ap_trp_information_type_list_t *trp_information_type_list_cp = &cp.trp_information_type_list;
  uint8_t trp_info_type_list_length = trp_information_type_list->trp_information_type_list_length;
  trp_information_type_list_cp->trp_information_type_list_length = trp_info_type_list_length;
  trp_information_type_list_cp->trp_information_type_item =
      calloc_or_fail(trp_info_type_list_length, sizeof(*trp_information_type_list_cp->trp_information_type_item));
  for (int i = 0; i < trp_info_type_list_length; i++) {
    trp_information_type_list_cp->trp_information_type_item[i] = trp_information_type_list->trp_information_type_item[i];
  }

  return cp;
}

/**
 * @brief F1 TRP information request equality check
 */
bool eq_trp_information_req(const f1ap_trp_information_req_t *a, const f1ap_trp_information_req_t *b)
{
  _EQ_CHECK_INT(a->transaction_id, b->transaction_id);
  _EQ_CHECK_INT(a->has_trp_list, b->has_trp_list);
  if (a->has_trp_list) {
    _EQ_CHECK_INT(a->trp_list.trp_list_length, b->trp_list.trp_list_length);
    for (int i = 0; i < a->trp_list.trp_list_length; i++) {
      _EQ_CHECK_INT(a->trp_list.trp_list_item[i].trp_id, b->trp_list.trp_list_item[i].trp_id);
    }
  }

  const f1ap_trp_information_type_list_t *trp_information_type_list_a = &a->trp_information_type_list;
  const f1ap_trp_information_type_list_t *trp_information_type_list_b = &b->trp_information_type_list;
  _EQ_CHECK_INT(trp_information_type_list_a->trp_information_type_list_length,
                trp_information_type_list_b->trp_information_type_list_length);
  for (int i = 0; i < trp_information_type_list_a->trp_information_type_list_length; i++) {
    _EQ_CHECK_INT(trp_information_type_list_a->trp_information_type_item[i],
                  trp_information_type_list_b->trp_information_type_item[i]);
  }

  return true;
}

/**
 * @brief Free Allocated F1 TRP information request
 */
void free_trp_information_req(f1ap_trp_information_req_t *msg)
{
  if (msg->has_trp_list) {
    free(msg->trp_list.trp_list_item);
  }
  free(msg->trp_information_type_list.trp_information_type_item);
}

/**
 * @brief Encode F1 TRP information response to ASN.1
 */
F1AP_F1AP_PDU_t *encode_trp_information_resp(const f1ap_trp_information_resp_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_successfulOutcome;
  asn1cCalloc(pdu->choice.successfulOutcome, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_TRPInformationExchange;
  tmp->criticality = F1AP_Criticality_reject;
  tmp->value.present = F1AP_SuccessfulOutcome__value_PR_TRPInformationResponse;
  F1AP_TRPInformationResponse_t *out = (tmp->value.choice.TRPInformationResponse = calloc_or_fail(1, sizeof(*tmp->value.choice.TRPInformationResponse)));

  /* mandatory : TransactionID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_TRPInformationResponseIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_TransactionID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_TRPInformationResponseIEs__value_PR_TransactionID;
  asn1cCallocOne(ie1->value.choice.TransactionID, msg->transaction_id);

  /* mandatory : TRPInformationList */
  // Supported : NR PCI, NG-RAN CGI, NR ARFCN, Geogarphical Coordinates
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_TRPInformationResponseIEs_t, ie3);
  ie3->id = F1AP_ProtocolIE_ID_id_TRPInformationListTRPResp;
  ie3->criticality = F1AP_Criticality_ignore;
  ie3->value.present = F1AP_TRPInformationResponseIEs__value_PR_TRPInformationListTRPResp;
  uint32_t trp_info_item_len = msg->trp_information_list.trp_information_item_length;
  for (int i = 0; i < trp_info_item_len; i++) {
    // TRPInformationItem (M)
    if (!ie3->value.choice.TRPInformationListTRPResp)
      ie3->value.choice.TRPInformationListTRPResp = calloc_or_fail(1, sizeof(*ie3->value.choice.TRPInformationListTRPResp));
    asn1cSequenceAdd(ie3->value.choice.TRPInformationListTRPResp->list, F1AP_TRPInformationItemTRPResp_t, trpinfolist);
    trpinfolist->id = F1AP_ProtocolIE_ID_id_TRPInformationItem;
    trpinfolist->criticality = F1AP_Criticality_ignore;
    trpinfolist->value.present = F1AP_TRPInformationItemTRPResp__value_PR_TRPInformationItem;
    if (!trpinfolist->value.choice.TRPInformationItem)
      trpinfolist->value.choice.TRPInformationItem = calloc_or_fail(1, sizeof(*trpinfolist->value.choice.TRPInformationItem));
    F1AP_TRPInformationItem_t *trp_info_item = trpinfolist->value.choice.TRPInformationItem;
    F1AP_TRPInformation_t *trp_info = (trp_info_item->tRPInformation = calloc_or_fail(1, sizeof(*trp_info_item->tRPInformation)));
    f1ap_trp_information_t *tRPInformation = &msg->trp_information_list.trp_information_item[i];
    trp_info->tRPID = tRPInformation->trp_id;

    f1ap_trp_information_type_response_list_t *tRPInformationTypeResponseList = &tRPInformation->trp_information_type_response_list;
    uint8_t trp_item_len = tRPInformationTypeResponseList->trp_information_type_response_item_length;

    trp_info->tRPInformationTypeResponseList = calloc_or_fail(1, sizeof(*trp_info->tRPInformationTypeResponseList));
    for (int j = 0; j < trp_item_len; j++) {
      asn1cSequenceAdd(trp_info->tRPInformationTypeResponseList->list, F1AP_TRPInformationTypeResponseItem_t, trp_resp_item);
      *trp_resp_item = encode_trp_info_type_response_item(&tRPInformationTypeResponseList->trp_information_type_response_item[j]);
    }
  }

  return pdu;
}

/**
 * @brief Decode F1 TRP information response
 */
bool decode_trp_information_resp(const F1AP_F1AP_PDU_t *pdu, f1ap_trp_information_resp_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_TRPInformationResponse_t *in = pdu->choice.successfulOutcome->value.choice.TRPInformationResponse;
  F1AP_TRPInformationResponseIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_TRPInformationResponseIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_TransactionID, true);
  F1AP_LIB_FIND_IE(F1AP_TRPInformationResponseIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_TRPInformationListTRPResp,
                   true);

  for (int i = 0; i < in->protocolIEs->list.count; ++i) {
    ie = in->protocolIEs->list.array[i];
    AssertError(ie != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_TransactionID:
        _EQ_CHECK_INT(ie->value.present, F1AP_TRPInformationResponseIEs__value_PR_TransactionID);
        out->transaction_id = *ie->value.choice.TransactionID;
        break;
      case F1AP_ProtocolIE_ID_id_TRPInformationListTRPResp:
        _EQ_CHECK_INT(ie->value.present, F1AP_TRPInformationResponseIEs__value_PR_TRPInformationListTRPResp);
        uint32_t trp_info_item_length = ie->value.choice.TRPInformationListTRPResp->list.count;
        AssertError(trp_info_item_length > 0, return false, "at least 1 TRP Information Item must be present");
        out->trp_information_list.trp_information_item_length = trp_info_item_length;
        f1ap_trp_information_t *trp_info_item = calloc_or_fail(trp_info_item_length, sizeof(*trp_info_item));
        out->trp_information_list.trp_information_item = trp_info_item;
        for (int i = 0; i < trp_info_item_length; i++) {
          F1AP_TRPInformationItemTRPResp_t *f1_trpinfolist =
              (F1AP_TRPInformationItemTRPResp_t *)ie->value.choice.TRPInformationListTRPResp->list.array[i];
          _EQ_CHECK_LONG(f1_trpinfolist->id, F1AP_ProtocolIE_ID_id_TRPInformationItem);
          _EQ_CHECK_INT(f1_trpinfolist->value.present, F1AP_TRPInformationItemTRPResp__value_PR_TRPInformationItem);
          F1AP_TRPInformationItem_t *f1_trp_info_item = f1_trpinfolist->value.choice.TRPInformationItem;
          F1AP_TRPInformation_t *f1_trp_info = f1_trp_info_item->tRPInformation;
          trp_info_item[i].trp_id = f1_trp_info->tRPID;

          F1AP_TRPInformationTypeResponseList_t *f1_trp_info_type_resp_list =
              (F1AP_TRPInformationTypeResponseList_t *)f1_trp_info->tRPInformationTypeResponseList;

          uint8_t trp_info_type_resp_item_len = f1_trp_info_type_resp_list->list.count;
          AssertError(trp_info_type_resp_item_len > 0,
                      return false,
                      "at least 1 TRP Information type response Item must be present");
          f1ap_trp_information_type_response_list_t *trp_info_resp_list = &trp_info_item[i].trp_information_type_response_list;
          trp_info_resp_list->trp_information_type_response_item_length = trp_info_type_resp_item_len;
          trp_info_resp_list->trp_information_type_response_item =
              calloc_or_fail(trp_info_type_resp_item_len, sizeof(*trp_info_resp_list->trp_information_type_response_item));
          for (int j = 0; j < trp_info_type_resp_item_len; j++) {
            f1ap_trp_information_type_response_item_t *trp_info_type_resp_item =
                &trp_info_resp_list->trp_information_type_response_item[j];
            decode_trp_info_type_response_item(f1_trp_info_type_resp_list->list.array[j], trp_info_type_resp_item);
          }
        }
        break;
      case F1AP_ProtocolIE_ID_id_CriticalityDiagnostics:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld not handled, skipping\n", ie->id);
        break;
      default:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld unknown, skipping\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1 TRP information response deep copy
 */
f1ap_trp_information_resp_t cp_trp_information_resp(const f1ap_trp_information_resp_t *orig)
{
  /* copy all mandatory fields that are not dynamic memory */
  f1ap_trp_information_resp_t cp = {
      .transaction_id = orig->transaction_id,
  };

  uint32_t trp_info_item_length = orig->trp_information_list.trp_information_item_length;
  AssertFatal(trp_info_item_length > 0, "at least 1 TRP Information Item must be present\n");
  cp.trp_information_list.trp_information_item_length = trp_info_item_length;
  cp.trp_information_list.trp_information_item =
      calloc_or_fail(trp_info_item_length, sizeof(*cp.trp_information_list.trp_information_item));

  f1ap_trp_information_t *trp_information_item_cp = cp.trp_information_list.trp_information_item;
  f1ap_trp_information_t *trp_information_item_orig = orig->trp_information_list.trp_information_item;

  for (int i = 0; i < trp_info_item_length; i++) {
    uint8_t trp_info_type_resp_item_len =
        trp_information_item_orig[i].trp_information_type_response_list.trp_information_type_response_item_length;
    AssertFatal(trp_info_type_resp_item_len > 0, "at least 1 TRP Information type response Item must be present\n");

    trp_information_item_cp[i].trp_id = trp_information_item_orig[i].trp_id;

    f1ap_trp_information_type_response_list_t *trp_info_type_resp_list_cp =
        &trp_information_item_cp[i].trp_information_type_response_list;
    f1ap_trp_information_type_response_list_t *trp_info_type_resp_list_orig =
        &trp_information_item_orig[i].trp_information_type_response_list;
    trp_info_type_resp_list_cp->trp_information_type_response_item_length = trp_info_type_resp_item_len;
    trp_info_type_resp_list_cp->trp_information_type_response_item =
        calloc_or_fail(trp_info_type_resp_item_len, sizeof(*trp_info_type_resp_list_cp->trp_information_type_response_item));
    for (int j = 0; j < trp_info_type_resp_item_len; j++) {
      trp_info_type_resp_list_cp->trp_information_type_response_item[j] =
          cp_trp_info_type_response_item(&trp_info_type_resp_list_orig->trp_information_type_response_item[j]);
    }
  }

  return cp;
}

/**
 * @brief F1 TRP information response equality check
 */
bool eq_trp_information_resp(const f1ap_trp_information_resp_t *a, const f1ap_trp_information_resp_t *b)
{
  _EQ_CHECK_INT(a->transaction_id, b->transaction_id);

  _EQ_CHECK_INT(a->trp_information_list.trp_information_item_length, b->trp_information_list.trp_information_item_length);
  uint32_t trp_info_item_length = a->trp_information_list.trp_information_item_length;
  AssertError(trp_info_item_length > 0, return false, "at least 1 TRP Information Item must be present\n");

  f1ap_trp_information_t *trp_information_item_a = a->trp_information_list.trp_information_item;
  f1ap_trp_information_t *trp_information_item_b = b->trp_information_list.trp_information_item;

  for (int i = 0; i < trp_info_item_length; i++) {
    f1ap_trp_information_type_response_list_t *trp_info_type_resp_list_a =
        &trp_information_item_a[i].trp_information_type_response_list;
    f1ap_trp_information_type_response_list_t *trp_info_type_resp_list_b =
        &trp_information_item_b[i].trp_information_type_response_list;
    _EQ_CHECK_INT(trp_info_type_resp_list_a->trp_information_type_response_item_length,
                  trp_info_type_resp_list_b->trp_information_type_response_item_length);
    uint8_t trp_info_type_resp_item_len = trp_info_type_resp_list_a->trp_information_type_response_item_length;
    AssertError(trp_info_type_resp_item_len > 0, return false, "at least 1 TRP Information type response Item must be present\n");
    _EQ_CHECK_INT(trp_information_item_a[i].trp_id, trp_information_item_b[i].trp_id);
    for (int j = 0; j < trp_info_type_resp_item_len; j++) {
      _F1_CHECK_EXP(eq_trp_info_type_response_item(&trp_info_type_resp_list_a->trp_information_type_response_item[j],
                                                   &trp_info_type_resp_list_b->trp_information_type_response_item[j]));
    }
  }

  return true;
}

/**
 * @brief Free Allocated F1 TRP information response
 */
void free_trp_information_resp(f1ap_trp_information_resp_t *msg)
{
  f1ap_trp_information_list_t *trp_information_list = &msg->trp_information_list;
  uint32_t trp_info_item_length = trp_information_list->trp_information_item_length;
  for (int i = 0; i < trp_info_item_length; i++) {
    f1ap_trp_information_t *trp_information_item = &trp_information_list->trp_information_item[i];
    f1ap_trp_information_type_response_list_t *trp_info_type_resp_list = &trp_information_item->trp_information_type_response_list;
    free(trp_info_type_resp_list->trp_information_type_response_item);
  }
  free(msg->trp_information_list.trp_information_item);
}

/**
 * @brief Encode F1 TRP information failure to ASN.1
 */
F1AP_F1AP_PDU_t *encode_trp_information_failure(const f1ap_trp_information_failure_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_unsuccessfulOutcome;
  asn1cCalloc(pdu->choice.unsuccessfulOutcome, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_TRPInformationExchange;
  tmp->criticality = F1AP_Criticality_reject;
  tmp->value.present = F1AP_UnsuccessfulOutcome__value_PR_TRPInformationFailure;
  F1AP_TRPInformationFailure_t *out = (tmp->value.choice.TRPInformationFailure = calloc_or_fail(1, sizeof(*tmp->value.choice.TRPInformationFailure)));

  /* mandatory : TransactionID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_TRPInformationFailureIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_TransactionID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_TRPInformationFailureIEs__value_PR_TransactionID;
  asn1cCallocOne(ie1->value.choice.TransactionID, msg->transaction_id);

  /* mandatory : Cause */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_TRPInformationFailureIEs_t, ie3);
  ie3->id = F1AP_ProtocolIE_ID_id_Cause;
  ie3->criticality = F1AP_Criticality_ignore;
  ie3->value.present = F1AP_TRPInformationFailureIEs__value_PR_Cause;
  asn1cCallocOne(ie3->value.choice.Cause, encode_f1ap_cause(msg->cause, msg->cause_value));

  return pdu;
}

/**
 * @brief Decode F1 TRP information failure
 */
bool decode_trp_information_failure(const F1AP_F1AP_PDU_t *pdu, f1ap_trp_information_failure_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_TRPInformationFailure_t *in = pdu->choice.unsuccessfulOutcome->value.choice.TRPInformationFailure;
  F1AP_TRPInformationFailureIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_TRPInformationFailureIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_TransactionID, true);
  F1AP_LIB_FIND_IE(F1AP_TRPInformationFailureIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_Cause, true);

  for (int i = 0; i < in->protocolIEs->list.count; ++i) {
    ie = in->protocolIEs->list.array[i];
    AssertError(ie != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_TransactionID:
        _EQ_CHECK_INT(ie->value.present, F1AP_TRPInformationFailureIEs__value_PR_TransactionID);
        out->transaction_id = *ie->value.choice.TransactionID;
        break;
      case F1AP_ProtocolIE_ID_id_Cause:
        _EQ_CHECK_INT(ie->value.present, F1AP_TRPInformationFailureIEs__value_PR_Cause);
        _F1_CHECK_EXP(decode_f1ap_cause(*ie->value.choice.Cause, &out->cause, &out->cause_value));
        break;
      case F1AP_ProtocolIE_ID_id_CriticalityDiagnostics:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld not handled, skipping\n", ie->id);
        break;
      default:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld unknown, skipping\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1 trp information failure deep copy
 */
f1ap_trp_information_failure_t cp_trp_information_failure(const f1ap_trp_information_failure_t *orig)
{
  /* copy all mandatory fields that are not dynamic memory */
  f1ap_trp_information_failure_t cp = {
      .transaction_id = orig->transaction_id,
      .cause = orig->cause,
      .cause_value = orig->cause_value,
  };

  return cp;
}

/**
 * @brief F1 trp information failure equality check
 */
bool eq_trp_information_failure(const f1ap_trp_information_failure_t *a, const f1ap_trp_information_failure_t *b)
{
  _EQ_CHECK_INT(a->transaction_id, b->transaction_id);
  _EQ_CHECK_INT(a->cause, b->cause);
  _EQ_CHECK_LONG(a->cause_value, b->cause_value);

  return true;
}

/**
 * @brief Free Allocated F1 trp information failure
 */
void free_trp_information_failure(f1ap_trp_information_failure_t *msg)
{
  // nothing to free
  UNUSED(msg);
}

/**
 * @brief Encode F1 positioning measurement request to ASN.1
 */
F1AP_F1AP_PDU_t *encode_positioning_measurement_req(const f1ap_positioning_measurement_req_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_PositioningMeasurementExchange;
  tmp->criticality = F1AP_Criticality_reject;
  tmp->value.present = F1AP_InitiatingMessage__value_PR_PositioningMeasurementRequest;
  F1AP_PositioningMeasurementRequest_t *out = (tmp->value.choice.PositioningMeasurementRequest = calloc_or_fail(1, sizeof(*tmp->value.choice.PositioningMeasurementRequest)));

  /* mandatory : TransactionID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementRequestIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_TransactionID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_PositioningMeasurementRequestIEs__value_PR_TransactionID;
  asn1cCallocOne(ie1->value.choice.TransactionID, msg->transaction_id);

  /* mandatory : LMF_MeasurementID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementRequestIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_LMF_MeasurementID;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_PositioningMeasurementRequestIEs__value_PR_LMF_MeasurementID;
  asn1cCallocOne(ie2->value.choice.LMF_MeasurementID, msg->lmf_measurement_id);

  /* mandatory : RAN_MeasurementID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementRequestIEs_t, ie3);
  ie3->id = F1AP_ProtocolIE_ID_id_RAN_MeasurementID;
  ie3->criticality = F1AP_Criticality_reject;
  ie3->value.present = F1AP_PositioningMeasurementRequestIEs__value_PR_RAN_MeasurementID;
  asn1cCallocOne(ie3->value.choice.RAN_MeasurementID, msg->ran_measurement_id);

  /* TRP Measurement Request List */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementRequestIEs_t, ie4);
  ie4->id = F1AP_ProtocolIE_ID_id_TRP_MeasurementRequestList;
  ie4->criticality = F1AP_Criticality_reject;
  ie4->value.present = F1AP_PositioningMeasurementRequestIEs__value_PR_TRP_MeasurementRequestList;
  F1AP_TRP_MeasurementRequestList_t *f1_trp_meas_req_list = (ie4->value.choice.TRP_MeasurementRequestList = calloc_or_fail(1, sizeof(*ie4->value.choice.TRP_MeasurementRequestList)));
  const f1ap_trp_measurement_request_list_t *trp_meas_req_list = &msg->trp_measurement_request_list;
  for (int i = 0; i < trp_meas_req_list->trp_measurement_request_list_length; i++) {
    // mandatory : TRP List Item
    asn1cSequenceAdd(f1_trp_meas_req_list->list, F1AP_TRP_MeasurementRequestItem_t, trp_meas_req_item);
    trp_meas_req_item->tRPID = trp_meas_req_list->trp_measurement_request_item[i].tRPID;
  }

  /* mandatory : PosReportCharacteristics */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementRequestIEs_t, ie5);
  ie5->id = F1AP_ProtocolIE_ID_id_PosReportCharacteristics;
  ie5->criticality = F1AP_Criticality_reject;
  ie5->value.present = F1AP_PositioningMeasurementRequestIEs__value_PR_PosReportCharacteristics;
  asn1cCallocOne(ie5->value.choice.PosReportCharacteristics, msg->pos_report_characteristics);

  /* C : if ReportCharacteristicsPeriodic */
  if (msg->pos_report_characteristics == F1AP_POSREPORTCHARACTERISTICS_PERIODIC) {
    asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementRequestIEs_t, ie6);
    ie6->id = F1AP_ProtocolIE_ID_id_PosMeasurementPeriodicity;
    ie6->criticality = F1AP_Criticality_reject;
    ie6->value.present = F1AP_PositioningMeasurementRequestIEs__value_PR_MeasurementPeriodicity;
    asn1cCallocOne(ie6->value.choice.MeasurementPeriodicity, msg->measurement_periodicity);
  }

  /* mandatory : Positioning Measurement Periodicity */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementRequestIEs_t, ie7);
  ie7->id = F1AP_ProtocolIE_ID_id_PosMeasurementQuantities;
  ie7->criticality = F1AP_Criticality_reject;
  ie7->value.present = F1AP_PositioningMeasurementRequestIEs__value_PR_PosMeasurementQuantities;
  F1AP_PosMeasurementQuantities_t *f1_pos_meas_quantities = (ie7->value.choice.PosMeasurementQuantities = calloc_or_fail(1, sizeof(*ie7->value.choice.PosMeasurementQuantities)));
  const f1ap_pos_measurement_quantities_t *pos_meas_quantities = &msg->pos_measurement_quantities;
  for (int i = 0; i < pos_meas_quantities->pos_measurement_quantities_length; i++) {
    // mandatory: PosMeasurementQuantitiesItem
    asn1cSequenceAdd(f1_pos_meas_quantities->list, F1AP_PosMeasurementQuantities_Item_t, pos_meas_quantity_item);
    pos_meas_quantity_item->posMeasurementType = pos_meas_quantities->pos_measurement_quantities_item[i].pos_measurement_type;
  }

  /* optional: SRS Configuration */
  if (msg->srs_configuration) {
    asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementRequestIEs_t, ie8);
    ie8->id = F1AP_ProtocolIE_ID_id_SRSConfiguration;
    ie8->criticality = F1AP_Criticality_ignore;
    ie8->value.present = F1AP_PositioningMeasurementRequestIEs__value_PR_SRSConfiguration;
    f1ap_srs_carrier_list_t *srs_carrier_list = &msg->srs_configuration->srs_carrier_list;
    if (!ie8->value.choice.SRSConfiguration)
      ie8->value.choice.SRSConfiguration = calloc_or_fail(1, sizeof(*ie8->value.choice.SRSConfiguration));
    asn1cCallocOne(ie8->value.choice.SRSConfiguration->sRSCarrier_List, encode_srs_carrier_list(srs_carrier_list));
  }

  return pdu;
}

/**
 * @brief Decode F1 Positioning Measurement request
 */
bool decode_positioning_measurement_req(const F1AP_F1AP_PDU_t *pdu, f1ap_positioning_measurement_req_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_PositioningMeasurementRequest_t *in = pdu->choice.initiatingMessage->value.choice.PositioningMeasurementRequest;
  F1AP_PositioningMeasurementRequestIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementRequestIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_TransactionID, true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementRequestIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_LMF_MeasurementID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementRequestIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_RAN_MeasurementID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementRequestIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_TRP_MeasurementRequestList,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementRequestIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_PosReportCharacteristics,
                   true);
  F1AP_PosReportCharacteristics_t *report_char = ie->value.choice.PosReportCharacteristics;
  if (*report_char == F1AP_PosReportCharacteristics_periodic) {
    F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementRequestIEs_t,
                     ie,
                     &in->protocolIEs->list,
                     F1AP_ProtocolIE_ID_id_PosMeasurementPeriodicity,
                     true);
  }
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementRequestIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_PosMeasurementQuantities,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementRequestIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_SRSConfiguration,
                   false);

  for (int i = 0; i < in->protocolIEs->list.count; ++i) {
    ie = in->protocolIEs->list.array[i];
    AssertError(ie != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_TransactionID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementRequestIEs__value_PR_TransactionID);
        out->transaction_id = *ie->value.choice.TransactionID;
        break;
      case F1AP_ProtocolIE_ID_id_LMF_MeasurementID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementRequestIEs__value_PR_LMF_MeasurementID);
        out->lmf_measurement_id = *ie->value.choice.LMF_MeasurementID;
        break;
      case F1AP_ProtocolIE_ID_id_RAN_MeasurementID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementRequestIEs__value_PR_RAN_MeasurementID);
        out->ran_measurement_id = *ie->value.choice.RAN_MeasurementID;
        break;
      case F1AP_ProtocolIE_ID_id_TRP_MeasurementRequestList:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementRequestIEs__value_PR_TRP_MeasurementRequestList);
        F1AP_TRP_MeasurementRequestList_t *f1_trp_meas_req_list = ie->value.choice.TRP_MeasurementRequestList;
        f1ap_trp_measurement_request_list_t *trp_meas_req_list = &out->trp_measurement_request_list;
        uint32_t trp_meas_req_list_len = f1_trp_meas_req_list->list.count;
        AssertError(trp_meas_req_list_len > 0, return false, "at least 1 TRP must be present");
        trp_meas_req_list->trp_measurement_request_list_length = trp_meas_req_list_len;
        trp_meas_req_list->trp_measurement_request_item =
            calloc_or_fail(trp_meas_req_list_len, sizeof(*trp_meas_req_list->trp_measurement_request_item));
        for (int i = 0; i < trp_meas_req_list_len; i++) {
          trp_meas_req_list->trp_measurement_request_item[i].tRPID = f1_trp_meas_req_list->list.array[i]->tRPID;
        }
        break;
      case F1AP_ProtocolIE_ID_id_PosReportCharacteristics:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementRequestIEs__value_PR_PosReportCharacteristics);
        out->pos_report_characteristics = *ie->value.choice.PosReportCharacteristics;
        break;
      case F1AP_ProtocolIE_ID_id_PosMeasurementPeriodicity:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementRequestIEs__value_PR_MeasurementPeriodicity);
        out->measurement_periodicity = *ie->value.choice.MeasurementPeriodicity;
        break;
      case F1AP_ProtocolIE_ID_id_PosMeasurementQuantities:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementRequestIEs__value_PR_PosMeasurementQuantities);
        F1AP_PosMeasurementQuantities_t *f1_pos_meas_quantities = ie->value.choice.PosMeasurementQuantities;
        f1ap_pos_measurement_quantities_t *pos_meas_quantities = &out->pos_measurement_quantities;
        uint32_t pos_meas_quantities_len = f1_pos_meas_quantities->list.count;
        AssertError(pos_meas_quantities_len > 0, return false, "at least 1 position measurement must be present");
        pos_meas_quantities->pos_measurement_quantities_length = pos_meas_quantities_len;
        pos_meas_quantities->pos_measurement_quantities_item =
            calloc_or_fail(pos_meas_quantities_len, sizeof(*pos_meas_quantities->pos_measurement_quantities_item));
        for (int i = 0; i < pos_meas_quantities_len; i++) {
          f1ap_pos_measurement_quantities_item_t *pos_meas_quantities_item =
              &pos_meas_quantities->pos_measurement_quantities_item[i];
          F1AP_PosMeasurementQuantities_Item_t *f1_PosMeasurementQuantities_Item = f1_pos_meas_quantities->list.array[i];
          pos_meas_quantities_item->pos_measurement_type = f1_PosMeasurementQuantities_Item->posMeasurementType;
        }
        break;
      case F1AP_ProtocolIE_ID_id_SFNInitialisationTime:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld not supported, skipping\n", ie->id);
        break;
      case F1AP_ProtocolIE_ID_id_SRSConfiguration:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementRequestIEs__value_PR_SRSConfiguration);
        F1AP_SRSCarrier_List_t *f1_sRSCarrier_List = ie->value.choice.SRSConfiguration->sRSCarrier_List;
        out->srs_configuration = calloc_or_fail(1, sizeof(*out->srs_configuration));
        f1ap_srs_carrier_list_t *srs_carrier_list = &out->srs_configuration->srs_carrier_list;
        decode_srs_carrier_list(srs_carrier_list, f1_sRSCarrier_List);
        break;
      case F1AP_ProtocolIE_ID_id_MeasurementBeamInfoRequest:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld not supported, skipping\n", ie->id);
        break;
      case F1AP_ProtocolIE_ID_id_SystemFrameNumber:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld not supported, skipping\n", ie->id);
        break;
      case F1AP_ProtocolIE_ID_id_SlotNumber:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld not supported, skipping\n", ie->id);
        break;
      default:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld unknown, skipping\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1 Positioning Measurement request deep copy
 */
f1ap_positioning_measurement_req_t cp_positioning_measurement_req(const f1ap_positioning_measurement_req_t *orig)
{
  // copy all mandatory fields that are not dynamic memory
  f1ap_positioning_measurement_req_t cp = {
      .transaction_id = orig->transaction_id,
      .lmf_measurement_id = orig->lmf_measurement_id,
      .ran_measurement_id = orig->ran_measurement_id,
  };

  f1ap_trp_measurement_request_list_t *trp_meas_req_list_cp = &cp.trp_measurement_request_list;
  const f1ap_trp_measurement_request_list_t *trp_meas_req_list_orig = &orig->trp_measurement_request_list;
  uint32_t trp_meas_req_list_len = trp_meas_req_list_orig->trp_measurement_request_list_length;
  AssertFatal(trp_meas_req_list_len > 0, "at least 1 TRP must be present");
  trp_meas_req_list_cp->trp_measurement_request_list_length = trp_meas_req_list_len;
  trp_meas_req_list_cp->trp_measurement_request_item =
      calloc_or_fail(trp_meas_req_list_len, sizeof(*trp_meas_req_list_cp->trp_measurement_request_item));
  for (int i = 0; i < trp_meas_req_list_len; i++) {
    trp_meas_req_list_cp->trp_measurement_request_item[i].tRPID = trp_meas_req_list_orig->trp_measurement_request_item[i].tRPID;
  }

  cp.pos_report_characteristics = orig->pos_report_characteristics;
  if (orig->pos_report_characteristics == F1AP_POSREPORTCHARACTERISTICS_PERIODIC) {
    cp.measurement_periodicity = orig->measurement_periodicity;
  }

  f1ap_pos_measurement_quantities_t *q_out = &cp.pos_measurement_quantities;
  const f1ap_pos_measurement_quantities_t *q_in = &orig->pos_measurement_quantities;
  uint32_t q_len = q_in->pos_measurement_quantities_length;
  AssertFatal(q_len > 0, "at least 1 position measurement must be present");
  q_out->pos_measurement_quantities_length = q_len;
  q_out->pos_measurement_quantities_item = calloc_or_fail(q_len, sizeof(*q_out->pos_measurement_quantities_item));
  for (int i = 0; i < q_len; i++) {
    f1ap_pos_measurement_quantities_item_t *q_item_in = &q_in->pos_measurement_quantities_item[i];
    f1ap_pos_measurement_quantities_item_t *q_item_out = &q_out->pos_measurement_quantities_item[i];
    q_item_out->pos_measurement_type = q_item_in->pos_measurement_type;
  }

  /* optional: SRS Configuration */
  if (orig->srs_configuration) {
    cp.srs_configuration = calloc_or_fail(1, sizeof(*cp.srs_configuration));
    f1ap_srs_carrier_list_t *srs_carrier_list_cp = &cp.srs_configuration->srs_carrier_list;
    f1ap_srs_carrier_list_t *srs_carrier_list = &orig->srs_configuration->srs_carrier_list;
    *srs_carrier_list_cp = cp_srs_carrier_list(srs_carrier_list);
  }

  return cp;
}

/**
 * @brief F1 Positioning Measurement request equality check
 */
bool eq_positioning_measurement_req(const f1ap_positioning_measurement_req_t *a, const f1ap_positioning_measurement_req_t *b)
{
  _EQ_CHECK_INT(a->transaction_id, b->transaction_id);
  _EQ_CHECK_INT(a->lmf_measurement_id, b->lmf_measurement_id);
  _EQ_CHECK_INT(a->ran_measurement_id, b->ran_measurement_id);

  const f1ap_trp_measurement_request_list_t *trp_meas_req_list_a = &a->trp_measurement_request_list;
  const f1ap_trp_measurement_request_list_t *trp_meas_req_list_b = &b->trp_measurement_request_list;
  uint32_t trp_meas_req_list_len = trp_meas_req_list_a->trp_measurement_request_list_length;
  AssertFatal(trp_meas_req_list_len > 0, "at least 1 TRP must be present");
  _EQ_CHECK_INT(trp_meas_req_list_b->trp_measurement_request_list_length, trp_meas_req_list_len);
  for (int i = 0; i < trp_meas_req_list_len; i++) {
    _EQ_CHECK_INT(trp_meas_req_list_a->trp_measurement_request_item[i].tRPID,
                  trp_meas_req_list_b->trp_measurement_request_item[i].tRPID);
  }

  _EQ_CHECK_INT(a->pos_report_characteristics, b->pos_report_characteristics);
  if (a->pos_report_characteristics == F1AP_POSREPORTCHARACTERISTICS_PERIODIC) {
    _EQ_CHECK_INT(a->measurement_periodicity, b->measurement_periodicity);
  }

  const f1ap_pos_measurement_quantities_t *pos_meas_quantities_a = &a->pos_measurement_quantities;
  const f1ap_pos_measurement_quantities_t *pos_meas_quantities_b = &b->pos_measurement_quantities;
  uint32_t pos_meas_quantities_len = pos_meas_quantities_a->pos_measurement_quantities_length;
  AssertFatal(pos_meas_quantities_len > 0, "at least 1 position measurement must be present");
  _EQ_CHECK_INT(pos_meas_quantities_b->pos_measurement_quantities_length, pos_meas_quantities_len);
  for (int i = 0; i < pos_meas_quantities_len; i++) {
    f1ap_pos_measurement_quantities_item_t *a_pos_meas_quantities_item = &pos_meas_quantities_a->pos_measurement_quantities_item[i];
    f1ap_pos_measurement_quantities_item_t *b_pos_meas_quantities_item = &pos_meas_quantities_b->pos_measurement_quantities_item[i];
    _EQ_CHECK_INT(a_pos_meas_quantities_item->pos_measurement_type, b_pos_meas_quantities_item->pos_measurement_type);
  }

  /* optional: SRS Configuration */
  if ((a->srs_configuration == NULL) != (b->srs_configuration == NULL)) {
    return false;
  }
  if (a->srs_configuration) {
    f1ap_srs_carrier_list_t *srs_carrier_list_a = &a->srs_configuration->srs_carrier_list;
    f1ap_srs_carrier_list_t *srs_carrier_list_b = &b->srs_configuration->srs_carrier_list;
    _F1_CHECK_EXP(eq_srs_carrier_list(srs_carrier_list_a, srs_carrier_list_b));
  }

  return true;
}

/**
 * @brief Free Allocated F1 Positioning Measurement request
 */
void free_positioning_measurement_req(f1ap_positioning_measurement_req_t *msg)
{
  free(msg->trp_measurement_request_list.trp_measurement_request_item);
  free(msg->pos_measurement_quantities.pos_measurement_quantities_item);

  /* optional: SRS Configuration */
  if (msg->srs_configuration) {
    f1ap_srs_carrier_list_t *srs_carrier_list = &msg->srs_configuration->srs_carrier_list;
    free_srs_carrier_list(srs_carrier_list);
    free(msg->srs_configuration);
  }
}

/**
 * @brief Encode F1 positioning measurement response to ASN.1
 */
F1AP_F1AP_PDU_t *encode_positioning_measurement_resp(const f1ap_positioning_measurement_resp_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_successfulOutcome;
  asn1cCalloc(pdu->choice.successfulOutcome, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_PositioningMeasurementExchange;
  tmp->criticality = F1AP_Criticality_reject;
  tmp->value.present = F1AP_SuccessfulOutcome__value_PR_PositioningMeasurementResponse;
  F1AP_PositioningMeasurementResponse_t *out = (tmp->value.choice.PositioningMeasurementResponse = calloc_or_fail(1, sizeof(*tmp->value.choice.PositioningMeasurementResponse)));

  /* mandatory : TransactionID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementResponseIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_TransactionID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_PositioningMeasurementResponseIEs__value_PR_TransactionID;
  asn1cCallocOne(ie1->value.choice.TransactionID, msg->transaction_id);

  /* mandatory : LMF_MeasurementID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementResponseIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_LMF_MeasurementID;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_PositioningMeasurementResponseIEs__value_PR_LMF_MeasurementID;
  asn1cCallocOne(ie2->value.choice.LMF_MeasurementID, msg->lmf_measurement_id);

  /* mandatory : RAN_MeasurementID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementResponseIEs_t, ie3);
  ie3->id = F1AP_ProtocolIE_ID_id_RAN_MeasurementID;
  ie3->criticality = F1AP_Criticality_reject;
  ie3->value.present = F1AP_PositioningMeasurementResponseIEs__value_PR_RAN_MeasurementID;
  asn1cCallocOne(ie3->value.choice.RAN_MeasurementID, msg->ran_measurement_id);

  /* Positioning Measurement Result List */
  if (msg->pos_measurement_result_list) {
    asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementResponseIEs_t, ie4);
    ie4->id = F1AP_ProtocolIE_ID_id_PosMeasurementResultList;
    ie4->criticality = F1AP_Criticality_reject;
    ie4->value.present = F1AP_PositioningMeasurementResponseIEs__value_PR_PosMeasurementResultList;
    const f1ap_pos_measurement_result_list_t *in_list = msg->pos_measurement_result_list;
    for (int i = 0; i < in_list->pos_measurement_result_list_length; i++) {
      // mandatory : Positioning Measurement Result List Item
      if (!ie4->value.choice.PosMeasurementResultList)
        ie4->value.choice.PosMeasurementResultList = calloc_or_fail(1, sizeof(*ie4->value.choice.PosMeasurementResultList));
      asn1cSequenceAdd(ie4->value.choice.PosMeasurementResultList->list, F1AP_PosMeasurementResultList_Item_t, out_item);
      f1ap_pos_measurement_result_list_item_t *in_item = &in_list->pos_measurement_result_list_item[i];
      if (!out_item->posMeasurementResult)
        out_item->posMeasurementResult = calloc_or_fail(1, sizeof(*out_item->posMeasurementResult));
      F1AP_PosMeasurementResult_t *out_result = out_item->posMeasurementResult;
      f1ap_pos_measurement_result_t *in_result = &in_item->pos_measurement_result;
      *out_result = encode_positioning_measurement_result(in_result);
      out_item->tRPID = in_item->trp_id;
    }
  }

  return pdu;
}

/**
 * @brief Decode F1 Positioning Measurement response
 */
bool decode_positioning_measurement_resp(const F1AP_F1AP_PDU_t *pdu, f1ap_positioning_measurement_resp_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_PositioningMeasurementResponse_t *in = pdu->choice.successfulOutcome->value.choice.PositioningMeasurementResponse;
  F1AP_PositioningMeasurementResponseIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementResponseIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_TransactionID, true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementResponseIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_LMF_MeasurementID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementResponseIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_RAN_MeasurementID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementResponseIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_PosMeasurementResultList,
                   false);

  for (int i = 0; i < in->protocolIEs->list.count; ++i) {
    ie = in->protocolIEs->list.array[i];
    AssertError(ie != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_TransactionID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementResponseIEs__value_PR_TransactionID);
        out->transaction_id = *ie->value.choice.TransactionID;
        break;
      case F1AP_ProtocolIE_ID_id_LMF_MeasurementID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementResponseIEs__value_PR_LMF_MeasurementID);
        out->lmf_measurement_id = *ie->value.choice.LMF_MeasurementID;
        break;
      case F1AP_ProtocolIE_ID_id_RAN_MeasurementID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementResponseIEs__value_PR_RAN_MeasurementID);
        out->ran_measurement_id = *ie->value.choice.RAN_MeasurementID;
        break;
      case F1AP_ProtocolIE_ID_id_PosMeasurementResultList:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementResponseIEs__value_PR_PosMeasurementResultList);
        F1AP_PosMeasurementResultList_t *in_list = ie->value.choice.PosMeasurementResultList;
        uint32_t pos_meas_result_list_len = in_list->list.count;
        out->pos_measurement_result_list = calloc_or_fail(1, sizeof(*out->pos_measurement_result_list));
        f1ap_pos_measurement_result_list_t *out_list = out->pos_measurement_result_list;
        out_list->pos_measurement_result_list_length = pos_meas_result_list_len;
        out_list->pos_measurement_result_list_item =
            calloc_or_fail(pos_meas_result_list_len, sizeof(*out_list->pos_measurement_result_list_item));
        for (int i = 0; i < pos_meas_result_list_len; i++) {
          F1AP_PosMeasurementResultList_Item_t *in_item = in_list->list.array[i];
          f1ap_pos_measurement_result_list_item_t *out_item = &out_list->pos_measurement_result_list_item[i];
          _F1_CHECK_EXP(decode_positioning_measurement_result(in_item->posMeasurementResult, &out_item->pos_measurement_result));
          out_item->trp_id = in_item->tRPID;
        }
        break;
      case F1AP_ProtocolIE_ID_id_CriticalityDiagnostics:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld not handled, skipping\n", ie->id);
        break;
      default:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld unknown, skipping\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1 Positioning Measurement response deep copy
 */
f1ap_positioning_measurement_resp_t cp_positioning_measurement_resp(const f1ap_positioning_measurement_resp_t *orig)
{
  // copy all mandatory fields that are not dynamic memory
  f1ap_positioning_measurement_resp_t cp = {
      .transaction_id = orig->transaction_id,
      .lmf_measurement_id = orig->lmf_measurement_id,
      .ran_measurement_id = orig->ran_measurement_id,
  };

  if (orig->pos_measurement_result_list) {
    cp.pos_measurement_result_list = calloc_or_fail(1, sizeof(*cp.pos_measurement_result_list));
    f1ap_pos_measurement_result_list_t *list_orig = orig->pos_measurement_result_list;
    f1ap_pos_measurement_result_list_t *list_cp = cp.pos_measurement_result_list;
    uint32_t pos_meas_result_list_len = list_orig->pos_measurement_result_list_length;
    list_cp->pos_measurement_result_list_length = pos_meas_result_list_len;
    list_cp->pos_measurement_result_list_item =
        calloc_or_fail(pos_meas_result_list_len, sizeof(*list_cp->pos_measurement_result_list_item));
    for (int i = 0; i < pos_meas_result_list_len; i++) {
      f1ap_pos_measurement_result_list_item_t *item_cp = &list_cp->pos_measurement_result_list_item[i];
      f1ap_pos_measurement_result_list_item_t *item_orig = &list_orig->pos_measurement_result_list_item[i];
      item_cp->pos_measurement_result = cp_positioning_measurement_result(&item_orig->pos_measurement_result);
      item_cp->trp_id = item_orig->trp_id;
    }
  }

  return cp;
}

/**
 * @brief F1 Positioning Measurement response equality check
 */
bool eq_positioning_measurement_resp(const f1ap_positioning_measurement_resp_t *a, const f1ap_positioning_measurement_resp_t *b)
{
  _EQ_CHECK_INT(a->transaction_id, b->transaction_id);
  _EQ_CHECK_INT(a->lmf_measurement_id, b->lmf_measurement_id);
  _EQ_CHECK_INT(a->ran_measurement_id, b->ran_measurement_id);

  if ((a->pos_measurement_result_list == NULL) != (b->pos_measurement_result_list == NULL)) {
    return false;
  }
  if (a->pos_measurement_result_list) {
    f1ap_pos_measurement_result_list_t *list_a = a->pos_measurement_result_list;
    f1ap_pos_measurement_result_list_t *list_b = b->pos_measurement_result_list;
    uint32_t pos_meas_result_list_len = list_a->pos_measurement_result_list_length;
    _EQ_CHECK_INT(list_b->pos_measurement_result_list_length, pos_meas_result_list_len);
    for (int i = 0; i < pos_meas_result_list_len; i++) {
      f1ap_pos_measurement_result_list_item_t *item_a = &list_a->pos_measurement_result_list_item[i];
      f1ap_pos_measurement_result_list_item_t *item_b = &list_b->pos_measurement_result_list_item[i];
      _F1_CHECK_EXP(eq_positioning_measurement_result(&item_a->pos_measurement_result, &item_b->pos_measurement_result));
      _EQ_CHECK_INT(item_a->trp_id, item_b->trp_id);
    }
  }

  return true;
}

/**
 * @brief Free Allocated F1 Positioning Measurement response
 */
void free_positioning_measurement_resp(f1ap_positioning_measurement_resp_t *msg)
{
  if (msg->pos_measurement_result_list) {
    f1ap_pos_measurement_result_list_t *list = msg->pos_measurement_result_list;
    uint32_t pos_meas_result_list_len = list->pos_measurement_result_list_length;
    for (int i = 0; i < pos_meas_result_list_len; i++) {
      f1ap_pos_measurement_result_t *posMeasurementResult = &list->pos_measurement_result_list_item[i].pos_measurement_result;
      uint32_t pos_meas_result_length = posMeasurementResult->pos_measurement_result_item_length;
      for (int j = 0; j < pos_meas_result_length; j++) {
        f1ap_pos_measurement_result_item_t *item = &posMeasurementResult->pos_measurement_result_item[j];
        f1ap_measured_results_value_t *measuredResultsValue = &item->measured_results_value;
        if (measuredResultsValue->present == F1AP_MEASURED_RESULTS_VALUE_PR_UL_ANGLEOFARRIVAL) {
          if (measuredResultsValue->choice.ul_angle_of_arrival.zenith_aoa) {
            free(measuredResultsValue->choice.ul_angle_of_arrival.zenith_aoa);
          }
          if (measuredResultsValue->choice.ul_angle_of_arrival.lcs_to_gcs_translation_aoa) {
            free(measuredResultsValue->choice.ul_angle_of_arrival.lcs_to_gcs_translation_aoa);
          }
        }
      }
      free(posMeasurementResult->pos_measurement_result_item);
    }
    free(msg->pos_measurement_result_list->pos_measurement_result_list_item);
    free(msg->pos_measurement_result_list);
  }
}

/**
 * @brief Encode F1 positioning measurement failure to ASN.1
 */
F1AP_F1AP_PDU_t *encode_positioning_measurement_failure(const f1ap_positioning_measurement_failure_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_unsuccessfulOutcome;
  asn1cCalloc(pdu->choice.unsuccessfulOutcome, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_PositioningMeasurementExchange;
  tmp->criticality = F1AP_Criticality_reject;
  tmp->value.present = F1AP_UnsuccessfulOutcome__value_PR_PositioningMeasurementFailure;
  F1AP_PositioningMeasurementFailure_t *out = (tmp->value.choice.PositioningMeasurementFailure = calloc_or_fail(1, sizeof(*tmp->value.choice.PositioningMeasurementFailure)));

  /* mandatory : TransactionID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementFailureIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_TransactionID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_PositioningMeasurementFailureIEs__value_PR_TransactionID;
  asn1cCallocOne(ie1->value.choice.TransactionID, msg->transaction_id);

  /* mandatory : LMF_MeasurementID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementFailureIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_LMF_MeasurementID;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_PositioningMeasurementFailureIEs__value_PR_LMF_MeasurementID;
  asn1cCallocOne(ie2->value.choice.LMF_MeasurementID, msg->lmf_measurement_id);

  /* mandatory : RAN_MeasurementID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementFailureIEs_t, ie3);
  ie3->id = F1AP_ProtocolIE_ID_id_RAN_MeasurementID;
  ie3->criticality = F1AP_Criticality_reject;
  ie3->value.present = F1AP_PositioningMeasurementFailureIEs__value_PR_RAN_MeasurementID;
  asn1cCallocOne(ie3->value.choice.RAN_MeasurementID, msg->ran_measurement_id);

  /* mandatory : Cause */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementFailureIEs_t, ie4);
  ie4->id = F1AP_ProtocolIE_ID_id_Cause;
  ie4->criticality = F1AP_Criticality_ignore;
  ie4->value.present = F1AP_PositioningMeasurementFailureIEs__value_PR_Cause;
  asn1cCallocOne(ie4->value.choice.Cause, encode_f1ap_cause(msg->cause, msg->cause_value));

  return pdu;
}

/**
 * @brief Decode F1 positioning measurement failure
 */
bool decode_positioning_measurement_failure(const F1AP_F1AP_PDU_t *pdu, f1ap_positioning_measurement_failure_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_PositioningMeasurementFailure_t *in = pdu->choice.unsuccessfulOutcome->value.choice.PositioningMeasurementFailure;
  F1AP_PositioningMeasurementFailureIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementFailureIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_TransactionID, true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementFailureIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_LMF_MeasurementID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementFailureIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_RAN_MeasurementID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementFailureIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_Cause, true);

  for (int i = 0; i < in->protocolIEs->list.count; ++i) {
    ie = in->protocolIEs->list.array[i];
    AssertError(ie != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_TransactionID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementFailureIEs__value_PR_TransactionID);
        out->transaction_id = *ie->value.choice.TransactionID;
        break;
      case F1AP_ProtocolIE_ID_id_LMF_MeasurementID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementFailureIEs__value_PR_LMF_MeasurementID);
        out->lmf_measurement_id = *ie->value.choice.LMF_MeasurementID;
        break;
      case F1AP_ProtocolIE_ID_id_RAN_MeasurementID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementFailureIEs__value_PR_RAN_MeasurementID);
        out->ran_measurement_id = *ie->value.choice.RAN_MeasurementID;
        break;
      case F1AP_ProtocolIE_ID_id_Cause:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementFailureIEs__value_PR_Cause);
        _F1_CHECK_EXP(decode_f1ap_cause(*ie->value.choice.Cause, &out->cause, &out->cause_value));
        break;
      case F1AP_ProtocolIE_ID_id_CriticalityDiagnostics:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld not handled, skipping\n", ie->id);
        break;
      default:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld unknown, skipping\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1 positioning measurement failure deep copy
 */
f1ap_positioning_measurement_failure_t cp_positioning_measurement_failure(const f1ap_positioning_measurement_failure_t *orig)
{
  /* copy all mandatory fields that are not dynamic memory */
  f1ap_positioning_measurement_failure_t cp = {
      .transaction_id = orig->transaction_id,
      .lmf_measurement_id = orig->lmf_measurement_id,
      .ran_measurement_id = orig->ran_measurement_id,
      .cause = orig->cause,
      .cause_value = orig->cause_value,
  };

  return cp;
}

/**
 * @brief F1 positioning measurement failure equality check
 */
bool eq_positioning_measurement_failure(const f1ap_positioning_measurement_failure_t *a,
                                        const f1ap_positioning_measurement_failure_t *b)
{
  _EQ_CHECK_INT(a->transaction_id, b->transaction_id);
  _EQ_CHECK_INT(a->lmf_measurement_id, b->lmf_measurement_id);
  _EQ_CHECK_INT(a->ran_measurement_id, b->ran_measurement_id);
  _EQ_CHECK_INT(a->cause, b->cause);
  _EQ_CHECK_LONG(a->cause_value, b->cause_value);

  return true;
}

/**
 * @brief Free Allocated F1 positioning measurement failure
 */
void free_positioning_measurement_failure(f1ap_positioning_measurement_failure_t *msg)
{
  // nothing to free
  UNUSED(msg);
}

/**
 * @brief Encode F1 positioning measurement report to ASN.1
 */
F1AP_F1AP_PDU_t *encode_positioning_measurement_report(const f1ap_positioning_measurement_report_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_PositioningMeasurementReport;
  tmp->criticality = F1AP_Criticality_ignore;
  tmp->value.present = F1AP_InitiatingMessage__value_PR_PositioningMeasurementReport;
  F1AP_PositioningMeasurementReport_t *out = (tmp->value.choice.PositioningMeasurementReport = calloc_or_fail(1, sizeof(*tmp->value.choice.PositioningMeasurementReport)));

  /* mandatory : TransactionID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementReportIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_TransactionID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_PositioningMeasurementReportIEs__value_PR_TransactionID;
  asn1cCallocOne(ie1->value.choice.TransactionID, msg->transaction_id);

  /* mandatory : LMF_MeasurementID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementReportIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_LMF_MeasurementID;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_PositioningMeasurementReportIEs__value_PR_LMF_MeasurementID;
  asn1cCallocOne(ie2->value.choice.LMF_MeasurementID, msg->lmf_measurement_id);

  /* mandatory : RAN_MeasurementID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementReportIEs_t, ie3);
  ie3->id = F1AP_ProtocolIE_ID_id_RAN_MeasurementID;
  ie3->criticality = F1AP_Criticality_reject;
  ie3->value.present = F1AP_PositioningMeasurementReportIEs__value_PR_RAN_MeasurementID;
  asn1cCallocOne(ie3->value.choice.RAN_MeasurementID, msg->ran_measurement_id);

  /* Positioning Measurement Result List */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementReportIEs_t, ie4);
  ie4->id = F1AP_ProtocolIE_ID_id_PosMeasurementResultList;
  ie4->criticality = F1AP_Criticality_reject;
  ie4->value.present = F1AP_PositioningMeasurementReportIEs__value_PR_PosMeasurementResultList;
  const f1ap_pos_measurement_result_list_t *pos_meas_res_list = msg->pos_measurement_result_list;
  for (int i = 0; i < pos_meas_res_list->pos_measurement_result_list_length; i++) {
    // mandatory : Positioning Measurement Result List Item
    if (!ie4->value.choice.PosMeasurementResultList)
      ie4->value.choice.PosMeasurementResultList = calloc_or_fail(1, sizeof(*ie4->value.choice.PosMeasurementResultList));
    asn1cSequenceAdd(ie4->value.choice.PosMeasurementResultList->list,
                     F1AP_PosMeasurementResultList_Item_t,
                     f1_pos_meas_result_list_item);
    f1ap_pos_measurement_result_list_item_t *pos_meas_res_list_item = &pos_meas_res_list->pos_measurement_result_list_item[i];
    if (!f1_pos_meas_result_list_item->posMeasurementResult)
      f1_pos_meas_result_list_item->posMeasurementResult = calloc_or_fail(1, sizeof(*f1_pos_meas_result_list_item->posMeasurementResult));
    F1AP_PosMeasurementResult_t *f1_posMeasurementResult = f1_pos_meas_result_list_item->posMeasurementResult;
    f1ap_pos_measurement_result_t *pos_measurement_result = &pos_meas_res_list_item->pos_measurement_result;
    *f1_posMeasurementResult = encode_positioning_measurement_result(pos_measurement_result);
    f1_pos_meas_result_list_item->tRPID = pos_meas_res_list_item->trp_id;
  }

  return pdu;
}

/**
 * @brief Decode F1 Positioning Measurement report
 */
bool decode_positioning_measurement_report(const F1AP_F1AP_PDU_t *pdu, f1ap_positioning_measurement_report_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_PositioningMeasurementReport_t *in = pdu->choice.initiatingMessage->value.choice.PositioningMeasurementReport;
  F1AP_PositioningMeasurementReportIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementReportIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_TransactionID, true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementReportIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_LMF_MeasurementID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementReportIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_RAN_MeasurementID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementReportIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_PosMeasurementResultList,
                   true);

  for (int i = 0; i < in->protocolIEs->list.count; ++i) {
    ie = in->protocolIEs->list.array[i];
    AssertError(ie != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_TransactionID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementReportIEs__value_PR_TransactionID);
        out->transaction_id = *ie->value.choice.TransactionID;
        break;
      case F1AP_ProtocolIE_ID_id_LMF_MeasurementID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementReportIEs__value_PR_LMF_MeasurementID);
        out->lmf_measurement_id = *ie->value.choice.LMF_MeasurementID;
        break;
      case F1AP_ProtocolIE_ID_id_RAN_MeasurementID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementReportIEs__value_PR_RAN_MeasurementID);
        out->ran_measurement_id = *ie->value.choice.RAN_MeasurementID;
        break;
      case F1AP_ProtocolIE_ID_id_PosMeasurementResultList:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementReportIEs__value_PR_PosMeasurementResultList);
        F1AP_PosMeasurementResultList_t *f1_PosMeasurementResultList = ie->value.choice.PosMeasurementResultList;
        uint32_t pos_meas_result_list_len = f1_PosMeasurementResultList->list.count;
        out->pos_measurement_result_list = calloc_or_fail(1, sizeof(*out->pos_measurement_result_list));
        f1ap_pos_measurement_result_list_t *pos_meas_res_list = out->pos_measurement_result_list;
        pos_meas_res_list->pos_measurement_result_list_length = pos_meas_result_list_len;
        pos_meas_res_list->pos_measurement_result_list_item =
            calloc_or_fail(pos_meas_result_list_len, sizeof(*pos_meas_res_list->pos_measurement_result_list_item));
        for (int i = 0; i < pos_meas_result_list_len; i++) {
          F1AP_PosMeasurementResultList_Item_t *f1_pos_meas_result_list_item = f1_PosMeasurementResultList->list.array[i];
          f1ap_pos_measurement_result_list_item_t *pos_meas_result_list_item =
              &pos_meas_res_list->pos_measurement_result_list_item[i];
          _F1_CHECK_EXP(decode_positioning_measurement_result(f1_pos_meas_result_list_item->posMeasurementResult,
                                                              &pos_meas_result_list_item->pos_measurement_result));
          pos_meas_result_list_item->trp_id = f1_pos_meas_result_list_item->tRPID;
        }
        break;
      default:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld unknown, skipping\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1 Positioning Measurement report deep copy
 */
f1ap_positioning_measurement_report_t cp_positioning_measurement_report(const f1ap_positioning_measurement_report_t *orig)
{
  // copy all mandatory fields that are not dynamic memory
  f1ap_positioning_measurement_report_t cp = {
      .transaction_id = orig->transaction_id,
      .lmf_measurement_id = orig->lmf_measurement_id,
      .ran_measurement_id = orig->ran_measurement_id,
  };

  cp.pos_measurement_result_list = calloc_or_fail(1, sizeof(*cp.pos_measurement_result_list));
  f1ap_pos_measurement_result_list_t *list_orig = orig->pos_measurement_result_list;
  f1ap_pos_measurement_result_list_t *list_cp = cp.pos_measurement_result_list;
  uint32_t pos_meas_result_list_len = list_orig->pos_measurement_result_list_length;
  list_cp->pos_measurement_result_list_length = pos_meas_result_list_len;
  list_cp->pos_measurement_result_list_item =
      calloc_or_fail(pos_meas_result_list_len, sizeof(*list_cp->pos_measurement_result_list_item));
  for (int i = 0; i < pos_meas_result_list_len; i++) {
    f1ap_pos_measurement_result_list_item_t *item_cp = &list_cp->pos_measurement_result_list_item[i];
    f1ap_pos_measurement_result_list_item_t *item_orig = &list_orig->pos_measurement_result_list_item[i];
    item_cp->pos_measurement_result = cp_positioning_measurement_result(&item_orig->pos_measurement_result);
    item_cp->trp_id = item_orig->trp_id;
  }

  return cp;
}

/**
 * @brief F1 Positioning Measurement report equality check
 */
bool eq_positioning_measurement_report(const f1ap_positioning_measurement_report_t *a,
                                       const f1ap_positioning_measurement_report_t *b)
{
  _EQ_CHECK_INT(a->transaction_id, b->transaction_id);
  _EQ_CHECK_INT(a->lmf_measurement_id, b->lmf_measurement_id);
  _EQ_CHECK_INT(a->ran_measurement_id, b->ran_measurement_id);

  f1ap_pos_measurement_result_list_t *list_a = a->pos_measurement_result_list;
  f1ap_pos_measurement_result_list_t *list_b = b->pos_measurement_result_list;
  uint32_t pos_meas_result_list_len = list_a->pos_measurement_result_list_length;
  _EQ_CHECK_INT(list_b->pos_measurement_result_list_length, pos_meas_result_list_len);
  for (int i = 0; i < pos_meas_result_list_len; i++) {
    f1ap_pos_measurement_result_list_item_t *item_a = &list_a->pos_measurement_result_list_item[i];
    f1ap_pos_measurement_result_list_item_t *item_b = &list_b->pos_measurement_result_list_item[i];
    _F1_CHECK_EXP(eq_positioning_measurement_result(&item_a->pos_measurement_result, &item_b->pos_measurement_result));
    _EQ_CHECK_INT(item_a->trp_id, item_b->trp_id);
  }

  return true;
}

/**
 * @brief Free Allocated F1 Positioning Measurement report
 */
void free_positioning_measurement_report(f1ap_positioning_measurement_report_t *msg)
{
  f1ap_pos_measurement_result_list_t *list = msg->pos_measurement_result_list;
  uint32_t pos_meas_result_list_len = list->pos_measurement_result_list_length;
  for (int i = 0; i < pos_meas_result_list_len; i++) {
    f1ap_pos_measurement_result_t *posMeasurementResult = &list->pos_measurement_result_list_item[i].pos_measurement_result;
    uint32_t pos_meas_result_length = posMeasurementResult->pos_measurement_result_item_length;
    for (int j = 0; j < pos_meas_result_length; j++) {
      f1ap_pos_measurement_result_item_t *item = &posMeasurementResult->pos_measurement_result_item[j];
      f1ap_measured_results_value_t *measuredResultsValue = &item->measured_results_value;
      if (measuredResultsValue->present == F1AP_MEASURED_RESULTS_VALUE_PR_UL_ANGLEOFARRIVAL) {
        if (measuredResultsValue->choice.ul_angle_of_arrival.zenith_aoa) {
          free(measuredResultsValue->choice.ul_angle_of_arrival.zenith_aoa);
        }
        if (measuredResultsValue->choice.ul_angle_of_arrival.lcs_to_gcs_translation_aoa) {
          free(measuredResultsValue->choice.ul_angle_of_arrival.lcs_to_gcs_translation_aoa);
        }
      }
    }
    free(posMeasurementResult->pos_measurement_result_item);
  }
  free(msg->pos_measurement_result_list->pos_measurement_result_list_item);
  free(msg->pos_measurement_result_list);
}

/**
 * @brief Encode F1 positioning measurement abort to ASN.1
 */
F1AP_F1AP_PDU_t *encode_positioning_measurement_abort(const f1ap_positioning_measurement_abort_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_PositioningMeasurementAbort;
  tmp->criticality = F1AP_Criticality_ignore;
  tmp->value.present = F1AP_InitiatingMessage__value_PR_PositioningMeasurementAbort;
  F1AP_PositioningMeasurementAbort_t *out = (tmp->value.choice.PositioningMeasurementAbort = calloc_or_fail(1, sizeof(*tmp->value.choice.PositioningMeasurementAbort)));

  /* mandatory : TransactionID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementAbortIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_TransactionID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_PositioningMeasurementAbortIEs__value_PR_TransactionID;
  asn1cCallocOne(ie1->value.choice.TransactionID, msg->transaction_id);

  /* mandatory : LMF_MeasurementID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementAbortIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_LMF_MeasurementID;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_PositioningMeasurementAbortIEs__value_PR_LMF_MeasurementID;
  asn1cCallocOne(ie2->value.choice.LMF_MeasurementID, msg->lmf_measurement_id);

  /* mandatory : RAN_MeasurementID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementAbortIEs_t, ie3);
  ie3->id = F1AP_ProtocolIE_ID_id_RAN_MeasurementID;
  ie3->criticality = F1AP_Criticality_reject;
  ie3->value.present = F1AP_PositioningMeasurementAbortIEs__value_PR_RAN_MeasurementID;
  asn1cCallocOne(ie3->value.choice.RAN_MeasurementID, msg->ran_measurement_id);

  return pdu;
}

/**
 * @brief Decode F1 positioning measurement abort
 */
bool decode_positioning_measurement_abort(const F1AP_F1AP_PDU_t *pdu, f1ap_positioning_measurement_abort_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_PositioningMeasurementAbort_t *in = pdu->choice.initiatingMessage->value.choice.PositioningMeasurementAbort;
  F1AP_PositioningMeasurementAbortIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementAbortIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_TransactionID, true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementAbortIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_LMF_MeasurementID, true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementAbortIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_RAN_MeasurementID, true);

  for (int i = 0; i < in->protocolIEs->list.count; ++i) {
    ie = in->protocolIEs->list.array[i];
    AssertError(ie != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_TransactionID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementAbortIEs__value_PR_TransactionID);
        out->transaction_id = *ie->value.choice.TransactionID;
        break;
      case F1AP_ProtocolIE_ID_id_LMF_MeasurementID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementAbortIEs__value_PR_LMF_MeasurementID);
        out->lmf_measurement_id = *ie->value.choice.LMF_MeasurementID;
        break;
      case F1AP_ProtocolIE_ID_id_RAN_MeasurementID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementAbortIEs__value_PR_RAN_MeasurementID);
        out->ran_measurement_id = *ie->value.choice.RAN_MeasurementID;
        break;
      default:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld unknown, skipping\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1 positioning measurement abort deep copy
 */
f1ap_positioning_measurement_abort_t cp_positioning_measurement_abort(const f1ap_positioning_measurement_abort_t *orig)
{
  /* copy all mandatory fields that are not dynamic memory */
  f1ap_positioning_measurement_abort_t cp = {
      .transaction_id = orig->transaction_id,
      .lmf_measurement_id = orig->lmf_measurement_id,
      .ran_measurement_id = orig->ran_measurement_id,
  };

  return cp;
}

/**
 * @brief F1 positioning measurement abort equality check
 */
bool eq_positioning_measurement_abort(const f1ap_positioning_measurement_abort_t *a, const f1ap_positioning_measurement_abort_t *b)
{
  _EQ_CHECK_INT(a->transaction_id, b->transaction_id);
  _EQ_CHECK_INT(a->lmf_measurement_id, b->lmf_measurement_id);
  _EQ_CHECK_INT(a->ran_measurement_id, b->ran_measurement_id);

  return true;
}

/**
 * @brief Free Allocated F1 positioning measurement abort
 */
void free_positioning_measurement_abort(f1ap_positioning_measurement_abort_t *msg)
{
  // nothing to free
  UNUSED(msg);
}

/**
 * @brief Encode F1 positioning measurement failure indication to ASN.1
 */
F1AP_F1AP_PDU_t *encode_positioning_measurement_failure_indication(const f1ap_positioning_measurement_failure_indication_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_PositioningMeasurementFailureIndication;
  tmp->criticality = F1AP_Criticality_ignore;
  tmp->value.present = F1AP_InitiatingMessage__value_PR_PositioningMeasurementFailureIndication;
  F1AP_PositioningMeasurementFailureIndication_t *out = (tmp->value.choice.PositioningMeasurementFailureIndication = calloc_or_fail(1, sizeof(*tmp->value.choice.PositioningMeasurementFailureIndication)));

  /* mandatory : TransactionID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementFailureIndicationIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_TransactionID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_PositioningMeasurementFailureIndicationIEs__value_PR_TransactionID;
  asn1cCallocOne(ie1->value.choice.TransactionID, msg->transaction_id);

  /* mandatory : LMF_MeasurementID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementFailureIndicationIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_LMF_MeasurementID;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_PositioningMeasurementFailureIndicationIEs__value_PR_LMF_MeasurementID;
  asn1cCallocOne(ie2->value.choice.LMF_MeasurementID, msg->lmf_measurement_id);

  /* mandatory : RAN_MeasurementID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementFailureIndicationIEs_t, ie3);
  ie3->id = F1AP_ProtocolIE_ID_id_RAN_MeasurementID;
  ie3->criticality = F1AP_Criticality_reject;
  ie3->value.present = F1AP_PositioningMeasurementFailureIndicationIEs__value_PR_RAN_MeasurementID;
  asn1cCallocOne(ie3->value.choice.RAN_MeasurementID, msg->ran_measurement_id);

  /* mandatory : Cause */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementFailureIndicationIEs_t, ie4);
  ie4->id = F1AP_ProtocolIE_ID_id_Cause;
  ie4->criticality = F1AP_Criticality_ignore;
  ie4->value.present = F1AP_PositioningMeasurementFailureIndicationIEs__value_PR_Cause;
  asn1cCallocOne(ie4->value.choice.Cause, encode_f1ap_cause(msg->cause, msg->cause_value));

  return pdu;
}

/**
 * @brief Decode F1 positioning measurement failure indication
 */
bool decode_positioning_measurement_failure_indication(const F1AP_F1AP_PDU_t *pdu,
                                                       f1ap_positioning_measurement_failure_indication_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_PositioningMeasurementFailureIndication_t *in =
      pdu->choice.initiatingMessage->value.choice.PositioningMeasurementFailureIndication;
  F1AP_PositioningMeasurementFailureIndicationIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementFailureIndicationIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_TransactionID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementFailureIndicationIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_LMF_MeasurementID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementFailureIndicationIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_RAN_MeasurementID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementFailureIndicationIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_Cause, true);

  for (int i = 0; i < in->protocolIEs->list.count; ++i) {
    ie = in->protocolIEs->list.array[i];
    AssertError(ie != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_TransactionID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementFailureIndicationIEs__value_PR_TransactionID);
        out->transaction_id = *ie->value.choice.TransactionID;
        break;
      case F1AP_ProtocolIE_ID_id_LMF_MeasurementID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementFailureIndicationIEs__value_PR_LMF_MeasurementID);
        out->lmf_measurement_id = *ie->value.choice.LMF_MeasurementID;
        break;
      case F1AP_ProtocolIE_ID_id_RAN_MeasurementID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementFailureIndicationIEs__value_PR_RAN_MeasurementID);
        out->ran_measurement_id = *ie->value.choice.RAN_MeasurementID;
        break;
      case F1AP_ProtocolIE_ID_id_Cause:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementFailureIndicationIEs__value_PR_Cause);
        _F1_CHECK_EXP(decode_f1ap_cause(*ie->value.choice.Cause, &out->cause, &out->cause_value));
        break;
      default:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld unknown, skipping\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1 positioning measurement failure indication deep copy
 */
f1ap_positioning_measurement_failure_indication_t cp_positioning_measurement_failure_indication(
    const f1ap_positioning_measurement_failure_indication_t *orig)
{
  /* copy all mandatory fields that are not dynamic memory */
  f1ap_positioning_measurement_failure_indication_t cp = {
      .transaction_id = orig->transaction_id,
      .lmf_measurement_id = orig->lmf_measurement_id,
      .ran_measurement_id = orig->ran_measurement_id,
      .cause = orig->cause,
      .cause_value = orig->cause_value,
  };

  return cp;
}

/**
 * @brief F1 positioning measurement failure indication equality check
 */
bool eq_positioning_measurement_failure_indication(const f1ap_positioning_measurement_failure_indication_t *a,
                                                   const f1ap_positioning_measurement_failure_indication_t *b)
{
  _EQ_CHECK_INT(a->transaction_id, b->transaction_id);
  _EQ_CHECK_INT(a->lmf_measurement_id, b->lmf_measurement_id);
  _EQ_CHECK_INT(a->ran_measurement_id, b->ran_measurement_id);
  _EQ_CHECK_INT(a->cause, b->cause);
  _EQ_CHECK_LONG(a->cause_value, b->cause_value);

  return true;
}

/**
 * @brief Free Allocated F1 positioning measurement failure indication
 */
void free_positioning_measurement_failure_indication(f1ap_positioning_measurement_failure_indication_t *msg)
{
  // nothing to free
  UNUSED(msg);
}

/**
 * @brief Encode F1 positioning measurement update to ASN.1
 */
F1AP_F1AP_PDU_t *encode_positioning_measurement_update(const f1ap_positioning_measurement_update_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_PositioningMeasurementUpdate;
  tmp->criticality = F1AP_Criticality_ignore;
  tmp->value.present = F1AP_InitiatingMessage__value_PR_PositioningMeasurementUpdate;
  F1AP_PositioningMeasurementUpdate_t *out = (tmp->value.choice.PositioningMeasurementUpdate = calloc_or_fail(1, sizeof(*tmp->value.choice.PositioningMeasurementUpdate)));

  /* mandatory : TransactionID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementUpdateIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_TransactionID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_PositioningMeasurementUpdateIEs__value_PR_TransactionID;
  asn1cCallocOne(ie1->value.choice.TransactionID, msg->transaction_id);

  /* mandatory : LMF_MeasurementID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementUpdateIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_LMF_MeasurementID;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_PositioningMeasurementUpdateIEs__value_PR_LMF_MeasurementID;
  asn1cCallocOne(ie2->value.choice.LMF_MeasurementID, msg->lmf_measurement_id);

  /* mandatory : RAN_MeasurementID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementUpdateIEs_t, ie3);
  ie3->id = F1AP_ProtocolIE_ID_id_RAN_MeasurementID;
  ie3->criticality = F1AP_Criticality_reject;
  ie3->value.present = F1AP_PositioningMeasurementUpdateIEs__value_PR_RAN_MeasurementID;
  asn1cCallocOne(ie3->value.choice.RAN_MeasurementID, msg->ran_measurement_id);

  /* optional: SRS Configuration */
  if (msg->srs_configuration) {
    asn1cSequenceAdd(out->protocolIEs->list, F1AP_PositioningMeasurementUpdateIEs_t, ie4);
    ie4->id = F1AP_ProtocolIE_ID_id_SRSConfiguration;
    ie4->criticality = F1AP_Criticality_ignore;
    ie4->value.present = F1AP_PositioningMeasurementUpdateIEs__value_PR_SRSConfiguration;
    f1ap_srs_carrier_list_t *srs_carrier_list = &msg->srs_configuration->srs_carrier_list;
    if (!ie4->value.choice.SRSConfiguration)
      ie4->value.choice.SRSConfiguration = calloc_or_fail(1, sizeof(*ie4->value.choice.SRSConfiguration));
    asn1cCallocOne(ie4->value.choice.SRSConfiguration->sRSCarrier_List, encode_srs_carrier_list(srs_carrier_list));
  }

  return pdu;
}

/**
 * @brief Decode F1 Positioning Measurement Update
 */
bool decode_positioning_measurement_update(const F1AP_F1AP_PDU_t *pdu, f1ap_positioning_measurement_update_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_PositioningMeasurementUpdate_t *in = pdu->choice.initiatingMessage->value.choice.PositioningMeasurementUpdate;
  F1AP_PositioningMeasurementUpdateIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementUpdateIEs_t, ie, &in->protocolIEs->list, F1AP_ProtocolIE_ID_id_TransactionID, true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementUpdateIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_LMF_MeasurementID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementUpdateIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_RAN_MeasurementID,
                   true);
  F1AP_LIB_FIND_IE(F1AP_PositioningMeasurementUpdateIEs_t,
                   ie,
                   &in->protocolIEs->list,
                   F1AP_ProtocolIE_ID_id_SRSConfiguration,
                   false);

  for (int i = 0; i < in->protocolIEs->list.count; ++i) {
    ie = in->protocolIEs->list.array[i];
    AssertError(ie != NULL, return false, "in->protocolIEs.list.array[i] is NULL");
    switch (ie->id) {
      case F1AP_ProtocolIE_ID_id_TransactionID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementUpdateIEs__value_PR_TransactionID);
        out->transaction_id = *ie->value.choice.TransactionID;
        break;
      case F1AP_ProtocolIE_ID_id_LMF_MeasurementID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementUpdateIEs__value_PR_LMF_MeasurementID);
        out->lmf_measurement_id = *ie->value.choice.LMF_MeasurementID;
        break;
      case F1AP_ProtocolIE_ID_id_RAN_MeasurementID:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementUpdateIEs__value_PR_RAN_MeasurementID);
        out->ran_measurement_id = *ie->value.choice.RAN_MeasurementID;
        break;
      case F1AP_ProtocolIE_ID_id_SRSConfiguration:
        _EQ_CHECK_INT(ie->value.present, F1AP_PositioningMeasurementUpdateIEs__value_PR_SRSConfiguration);
        F1AP_SRSCarrier_List_t *f1_sRSCarrier_List = ie->value.choice.SRSConfiguration->sRSCarrier_List;
        out->srs_configuration = calloc_or_fail(1, sizeof(*out->srs_configuration));
        f1ap_srs_carrier_list_t *srs_carrier_list = &out->srs_configuration->srs_carrier_list;
        decode_srs_carrier_list(srs_carrier_list, f1_sRSCarrier_List);
        break;
      default:
        PRINT_ERROR("F1AP_ProtocolIE_ID_id %ld unknown, skipping\n", ie->id);
        break;
    }
  }

  return true;
}

/**
 * @brief F1 Positioning Measurement Update deep copy
 */
f1ap_positioning_measurement_update_t cp_positioning_measurement_update(const f1ap_positioning_measurement_update_t *orig)
{
  // copy all mandatory fields that are not dynamic memory
  f1ap_positioning_measurement_update_t cp = {
      .transaction_id = orig->transaction_id,
      .lmf_measurement_id = orig->lmf_measurement_id,
      .ran_measurement_id = orig->ran_measurement_id,
  };

  /* optional: SRS Configuration */
  if (orig->srs_configuration) {
    cp.srs_configuration = calloc_or_fail(1, sizeof(*cp.srs_configuration));
    f1ap_srs_carrier_list_t *srs_carrier_list_cp = &cp.srs_configuration->srs_carrier_list;
    f1ap_srs_carrier_list_t *srs_carrier_list = &orig->srs_configuration->srs_carrier_list;
    *srs_carrier_list_cp = cp_srs_carrier_list(srs_carrier_list);
  }

  return cp;
}

/**
 * @brief F1 Positioning Measurement Update equality check
 */
bool eq_positioning_measurement_update(const f1ap_positioning_measurement_update_t *a,
                                       const f1ap_positioning_measurement_update_t *b)
{
  _EQ_CHECK_INT(a->transaction_id, b->transaction_id);
  _EQ_CHECK_INT(a->lmf_measurement_id, b->lmf_measurement_id);
  _EQ_CHECK_INT(a->ran_measurement_id, b->ran_measurement_id);

  /* optional: SRS Configuration */
  if ((a->srs_configuration == NULL) != (b->srs_configuration == NULL)) {
    return false;
  }
  if (a->srs_configuration) {
    f1ap_srs_carrier_list_t *srs_carrier_list_a = &a->srs_configuration->srs_carrier_list;
    f1ap_srs_carrier_list_t *srs_carrier_list_b = &b->srs_configuration->srs_carrier_list;
    _F1_CHECK_EXP(eq_srs_carrier_list(srs_carrier_list_a, srs_carrier_list_b));
  }

  return true;
}

/**
 * @brief Free Allocated F1 Positioning Measurement Update
 */
void free_positioning_measurement_update(f1ap_positioning_measurement_update_t *msg)
{
  /* SRS Configuration */
  if (msg->srs_configuration) {
    f1ap_srs_carrier_list_t *srs_carrier_list = &msg->srs_configuration->srs_carrier_list;
    free_srs_carrier_list(srs_carrier_list);
    free(msg->srs_configuration);
  }
}
