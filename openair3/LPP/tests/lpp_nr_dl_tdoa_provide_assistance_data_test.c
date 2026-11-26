/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "lpp_test_utils.h"
#include "common/utils/oai_asn1.h"
#include "openair3/UTILS/conversions.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "LPP_DL-PRS-ID-Info-r16.h"
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
#include "LPP_NR-DL-PRS-Info-r16.h"
#include "LPP_NR-DL-PRS-PositioningFrequencyLayer-r16.h"
#include "LPP_NR-DL-PRS-Resource-r16.h"
#include "LPP_NR-DL-PRS-ResourceSet-r16.h"
#include "LPP_NR-DL-PRS-SFN0-Offset-r16.h"
#include "LPP_NR-DL-TDOA-ProvideAssistanceData-r16.h"
#include "LPP_NR-SSB-Config-r16.h"
#include "LPP_NR-SelectedDL-PRS-IndexList-r16.h"
#include "LPP_NR-SelectedDL-PRS-IndexPerTRP-r16.h"
#include "LPP_NR-SelectedDL-PRS-PerFreq-r16.h"
#include "LPP_ProvideAssistanceData-r9-IEs.h"
#include "LPP_ProvideAssistanceData.h"
#include "xer_encoder.h"

typedef struct trp_fixture_s {
  long dl_prs_id;
  long phys_cell_id;
  uint64_t nr_cell_identity;
  long arfcn;
  long sfn_offset;
  long subframe_offset;
  long expected_rstd;
  long expected_rstd_uncertainty;
  long sequence_id;
  long comb_offset;
  long slot_offset;
  long symbol_offset;
  long ssb_index;
  long half_frame_index;
  long sfn_ssb_offset;
} trp_fixture_t;

static const trp_fixture_t trps[] = {
    {10, 123, 0x001234560, 633984, 17, 3, 0, 12, 801, 0, 0, 2, 8, 0, 1},
    {11, 321, 0x001234561, 633984, 41, 7, 148, 22, 1462, 1, 8, 4, 12, 1, 4},
    {12, 654, 0x001234562, 633984, 77, 1, -226, 35, 2047, 2, 16, 6, 20, 0, 9},
    {13, 777, 0x001234563, 633984, 103, 5, 379, 18, 3001, 3, 24, 8, 28, 1, 12},
};

static long *new_long(long value)
{
  long *ptr = calloc_or_fail(1, sizeof(*ptr));
  *ptr = value;
  return ptr;
}

static void set_ssb_long_bitmap(BIT_STRING_t *bit_string, long ssb_index)
{
  bit_string->buf = calloc_or_fail(8, sizeof(*bit_string->buf));
  bit_string->size = 8;
  bit_string->bits_unused = 0;
  bit_string->buf[ssb_index / 8] = (uint8_t)(0x80u >> (ssb_index % 8));
}

static LPP_NCGI_r15_t *create_ncgi(uint64_t nr_cell_identity)
{
  LPP_NCGI_r15_t *ncgi = calloc_or_fail(1, sizeof(*ncgi));

  asn1cSeqAdd(&ncgi->mcc_r15.list, new_long(2));
  asn1cSeqAdd(&ncgi->mcc_r15.list, new_long(0));
  asn1cSeqAdd(&ncgi->mcc_r15.list, new_long(8));

  asn1cSeqAdd(&ncgi->mnc_r15.list, new_long(9));
  asn1cSeqAdd(&ncgi->mnc_r15.list, new_long(5));

  NR_CELL_ID_TO_BIT_STRING(nr_cell_identity, &ncgi->nr_cellidentity_r15);
  return ncgi;
}

static LPP_DL_PRS_QCL_Info_r16_t *create_qcl_info(const trp_fixture_t *fixture)
{
  LPP_DL_PRS_QCL_Info_r16_t *qcl = calloc_or_fail(1, sizeof(*qcl));

  qcl->present = LPP_DL_PRS_QCL_Info_r16_PR_ssb_r16;
  qcl->choice.ssb_r16 = calloc_or_fail(1, sizeof(*qcl->choice.ssb_r16));
  qcl->choice.ssb_r16->pci_r16 = fixture->phys_cell_id;
  qcl->choice.ssb_r16->ssb_Index_r16 = fixture->ssb_index;
  qcl->choice.ssb_r16->rs_Type_r16 = LPP_DL_PRS_QCL_Info_r16__ssb_r16__rs_Type_r16_typeD;

  return qcl;
}

static LPP_NR_DL_PRS_Resource_r16_t *create_prs_resource(const trp_fixture_t *fixture)
{
  LPP_NR_DL_PRS_Resource_r16_t *resource = calloc_or_fail(1, sizeof(*resource));

  resource->nr_DL_PRS_ResourceID_r16 = 0;
  resource->dl_PRS_SequenceID_r16 = fixture->sequence_id;
  resource->dl_PRS_CombSizeN_AndReOffset_r16.present =
      LPP_NR_DL_PRS_Resource_r16__dl_PRS_CombSizeN_AndReOffset_r16_PR_n4_r16;
  resource->dl_PRS_CombSizeN_AndReOffset_r16.choice.n4_r16 = fixture->comb_offset;
  resource->dl_PRS_ResourceSlotOffset_r16 = fixture->slot_offset;
  resource->dl_PRS_ResourceSymbolOffset_r16 = fixture->symbol_offset;
  resource->dl_PRS_QCL_Info_r16 = create_qcl_info(fixture);

  return resource;
}

static LPP_NR_DL_PRS_ResourceSet_r16_t *create_prs_resource_set(const trp_fixture_t *fixture)
{
  LPP_NR_DL_PRS_ResourceSet_r16_t *resource_set = calloc_or_fail(1, sizeof(*resource_set));

  resource_set->nr_DL_PRS_ResourceSetID_r16 = 0;
  resource_set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16.present =
      LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16_PR_scs30_r16;
  resource_set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16.choice.scs30_r16 =
      calloc_or_fail(1, sizeof(*resource_set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16.choice.scs30_r16));
  resource_set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16.choice.scs30_r16->present =
      LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR_n16_r16;
  resource_set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16.choice.scs30_r16->choice.n16_r16 = 3;
  resource_set->dl_PRS_ResourceRepetitionFactor_r16 =
      new_long(LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_ResourceRepetitionFactor_r16_n2);
  resource_set->dl_PRS_ResourceTimeGap_r16 =
      new_long(LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_ResourceTimeGap_r16_s1);
  resource_set->dl_PRS_NumSymbols_r16 = LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_NumSymbols_r16_n4;
  resource_set->dl_PRS_ResourcePower_r16 = -10;

  asn1cSeqAdd(&resource_set->dl_PRS_ResourceList_r16.list, create_prs_resource(fixture));
  return resource_set;
}

static LPP_NR_DL_PRS_AssistanceDataPerTRP_r16_t *create_trp_assistance_data(const trp_fixture_t *fixture)
{
  LPP_NR_DL_PRS_AssistanceDataPerTRP_r16_t *trp = calloc_or_fail(1, sizeof(*trp));

  trp->dl_PRS_ID_r16 = fixture->dl_prs_id;
  trp->nr_PhysCellID_r16 = new_long(fixture->phys_cell_id);
  trp->nr_CellGlobalID_r16 = create_ncgi(fixture->nr_cell_identity);
  trp->nr_ARFCN_r16 = new_long(fixture->arfcn);
  trp->nr_DL_PRS_SFN0_Offset_r16.sfn_Offset_r16 = fixture->sfn_offset;
  trp->nr_DL_PRS_SFN0_Offset_r16.integerSubframeOffset_r16 = fixture->subframe_offset;
  trp->nr_DL_PRS_ExpectedRSTD_r16 = fixture->expected_rstd;
  trp->nr_DL_PRS_ExpectedRSTD_Uncertainty_r16 = fixture->expected_rstd_uncertainty;

  asn1cSeqAdd(&trp->nr_DL_PRS_Info_r16.nr_DL_PRS_ResourceSetList_r16.list,
                       create_prs_resource_set(fixture));
  return trp;
}

static LPP_NR_DL_PRS_AssistanceDataPerFreq_r16_t *create_frequency_layer(void)
{
  LPP_NR_DL_PRS_AssistanceDataPerFreq_r16_t *frequency = calloc_or_fail(1, sizeof(*frequency));
  LPP_NR_DL_PRS_PositioningFrequencyLayer_r16_t *layer = &frequency->nr_DL_PRS_PositioningFrequencyLayer_r16;

  layer->dl_PRS_SubcarrierSpacing_r16 =
      LPP_NR_DL_PRS_PositioningFrequencyLayer_r16__dl_PRS_SubcarrierSpacing_r16_kHz30;
  layer->dl_PRS_ResourceBandwidth_r16 = 52;
  layer->dl_PRS_StartPRB_r16 = 24;
  layer->dl_PRS_PointA_r16 = 633984;
  layer->dl_PRS_CombSizeN_r16 = LPP_NR_DL_PRS_PositioningFrequencyLayer_r16__dl_PRS_CombSizeN_r16_n4;
  layer->dl_PRS_CyclicPrefix_r16 = LPP_NR_DL_PRS_PositioningFrequencyLayer_r16__dl_PRS_CyclicPrefix_r16_normal;

  for (size_t i = 0; i < sizeof(trps) / sizeof(trps[0]); ++i)
    asn1cSeqAdd(&frequency->nr_DL_PRS_AssistanceDataPerFreq_r16.list, create_trp_assistance_data(&trps[i]));

  return frequency;
}

static LPP_NR_SSB_Config_r16_t *create_ssb_config(const trp_fixture_t *fixture)
{
  LPP_NR_SSB_Config_r16_t *ssb = calloc_or_fail(1, sizeof(*ssb));

  ssb->nr_PhysCellID_r16 = fixture->phys_cell_id;
  ssb->nr_ARFCN_r16 = fixture->arfcn;
  ssb->ss_PBCH_BlockPower_r16 = -20;
  ssb->halfFrameIndex_r16 = fixture->half_frame_index;
  ssb->ssb_periodicity_r16 = LPP_NR_SSB_Config_r16__ssb_periodicity_r16_ms20;
  ssb->ssb_PositionsInBurst_r16 = calloc_or_fail(1, sizeof(*ssb->ssb_PositionsInBurst_r16));
  ssb->ssb_PositionsInBurst_r16->present = LPP_NR_SSB_Config_r16__ssb_PositionsInBurst_r16_PR_longBitmap_r16;
  set_ssb_long_bitmap(&ssb->ssb_PositionsInBurst_r16->choice.longBitmap_r16, fixture->ssb_index);
  ssb->ssb_SubcarrierSpacing_r16 = LPP_NR_SSB_Config_r16__ssb_SubcarrierSpacing_r16_kHz30;
  ssb->sfn_SSB_Offset_r16 = fixture->sfn_ssb_offset;

  return ssb;
}

static LPP_NR_DL_PRS_AssistanceData_r16_t *create_prs_assistance_data(void)
{
  LPP_NR_DL_PRS_AssistanceData_r16_t *assistance = calloc_or_fail(1, sizeof(*assistance));

  assistance->nr_DL_PRS_ReferenceInfo_r16.dl_PRS_ID_r16 = trps[0].dl_prs_id;
  assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceID_List_r16 =
      calloc_or_fail(1, sizeof(*assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceID_List_r16));
  asn1cSeqAdd(&assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceID_List_r16->list, new_long(0));
  assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceSetID_r16 = new_long(0);

  asn1cSeqAdd(&assistance->nr_DL_PRS_AssistanceDataList_r16.list, create_frequency_layer());

  assistance->nr_SSB_Config_r16 = calloc_or_fail(1, sizeof(*assistance->nr_SSB_Config_r16));
  for (size_t i = 0; i < sizeof(trps) / sizeof(trps[0]); ++i)
    asn1cSeqAdd(&assistance->nr_SSB_Config_r16->list, create_ssb_config(&trps[i]));

  return assistance;
}

static LPP_DL_SelectedPRS_ResourceSetIndex_r16_t *create_selected_resource_set_index(void)
{
  LPP_DL_SelectedPRS_ResourceSetIndex_r16_t *resource_set = calloc_or_fail(1, sizeof(*resource_set));
  LPP_DL_SelectedPRS_ResourceIndex_r16_t *resource = calloc_or_fail(1, sizeof(*resource));

  resource_set->nr_DL_SelectedPRS_ResourceSetIndex_r16 = 0;
  resource_set->dl_SelectedPRS_ResourceIndexList_r16 =
      calloc_or_fail(1, sizeof(*resource_set->dl_SelectedPRS_ResourceIndexList_r16));

  resource->nr_DL_SelectedPRS_ResourceIdIndex_r16 = 0;
  asn1cSeqAdd(&resource_set->dl_SelectedPRS_ResourceIndexList_r16->list, resource);

  return resource_set;
}

static LPP_NR_SelectedDL_PRS_IndexPerTRP_r16_t *create_selected_trp(long trp_index)
{
  LPP_NR_SelectedDL_PRS_IndexPerTRP_r16_t *selected_trp = calloc_or_fail(1, sizeof(*selected_trp));

  selected_trp->nr_SelectedTRP_Index_r16 = trp_index;
  selected_trp->dl_SelectedPRS_ResourceSetIndexList_r16 =
      calloc_or_fail(1, sizeof(*selected_trp->dl_SelectedPRS_ResourceSetIndexList_r16));
  asn1cSeqAdd(&selected_trp->dl_SelectedPRS_ResourceSetIndexList_r16->list,
                       create_selected_resource_set_index());

  return selected_trp;
}

static LPP_NR_SelectedDL_PRS_IndexList_r16_t *create_selected_prs_index_list(void)
{
  LPP_NR_SelectedDL_PRS_IndexList_r16_t *index_list = calloc_or_fail(1, sizeof(*index_list));
  LPP_NR_SelectedDL_PRS_PerFreq_r16_t *per_frequency = calloc_or_fail(1, sizeof(*per_frequency));

  per_frequency->nr_SelectedDL_PRS_FrequencyLayerIndex_r16 = 0;
  per_frequency->nr_SelectedDL_PRS_IndexListPerFreq_r16 =
      calloc_or_fail(1, sizeof(*per_frequency->nr_SelectedDL_PRS_IndexListPerFreq_r16));

  for (size_t i = 0; i < sizeof(trps) / sizeof(trps[0]); ++i)
    asn1cSeqAdd(&per_frequency->nr_SelectedDL_PRS_IndexListPerFreq_r16->list, create_selected_trp((long)i));

  asn1cSeqAdd(&index_list->list, per_frequency);
  return index_list;
}

static LPP_NR_DL_TDOA_ProvideAssistanceData_r16_t *create_nr_dl_tdoa_provide_assistance_data(void)
{
  LPP_NR_DL_TDOA_ProvideAssistanceData_r16_t *tdoa = calloc_or_fail(1, sizeof(*tdoa));

  tdoa->nr_DL_PRS_AssistanceData_r16 = create_prs_assistance_data();
  tdoa->nr_SelectedDL_PRS_IndexList_r16 = create_selected_prs_index_list();

  return tdoa;
}

static LPP_LPP_Message_t *create_provide_assistance_data_message(void)
{
  LPP_LPP_Message_t *message = calloc_or_fail(1, sizeof(*message));

  message->transactionID = calloc_or_fail(1, sizeof(*message->transactionID));
  message->transactionID->initiator = LPP_Initiator_locationServer;
  message->transactionID->transactionNumber = 2;
  message->endTransaction = false;

  message->lpp_MessageBody = calloc_or_fail(1, sizeof(*message->lpp_MessageBody));
  message->lpp_MessageBody->present = LPP_LPP_MessageBody_PR_c1;
  message->lpp_MessageBody->choice.c1 = calloc_or_fail(1, sizeof(*message->lpp_MessageBody->choice.c1));
  message->lpp_MessageBody->choice.c1->present = LPP_LPP_MessageBody__c1_PR_provideAssistanceData;

  LPP_ProvideAssistanceData_t *provide = calloc_or_fail(1, sizeof(*provide));
  message->lpp_MessageBody->choice.c1->choice.provideAssistanceData = provide;

  provide->criticalExtensions.present = LPP_ProvideAssistanceData__criticalExtensions_PR_c1;
  provide->criticalExtensions.choice.c1 = calloc_or_fail(1, sizeof(*provide->criticalExtensions.choice.c1));
  provide->criticalExtensions.choice.c1->present =
      LPP_ProvideAssistanceData__criticalExtensions__c1_PR_provideAssistanceData_r9;

  LPP_ProvideAssistanceData_r9_IEs_t *provide_r9 = calloc_or_fail(1, sizeof(*provide_r9));
  provide->criticalExtensions.choice.c1->choice.provideAssistanceData_r9 = provide_r9;
  provide_r9->ext2 = calloc_or_fail(1, sizeof(*provide_r9->ext2));
  provide_r9->ext2->nr_DL_TDOA_ProvideAssistanceData_r16 = create_nr_dl_tdoa_provide_assistance_data();

  return message;
}

int main(void)
{
  LPP_LPP_Message_t *message = create_provide_assistance_data_message();

  LPP_LPP_Message_t *aper_decoded = encode_decode_lpp_message(message, LPP_CODEC_APER);
  AssertFatal(eq_lpp_message(message, aper_decoded), "decoded LPP message differs\n");
  free_lpp_message(aper_decoded);

  LPP_LPP_Message_t *uper_decoded = encode_decode_lpp_message(message, LPP_CODEC_UPER);
  AssertFatal(eq_lpp_message(message, uper_decoded), "decoded LPP message differs\n");

  xer_fprint(stdout, &asn_DEF_LPP_LPP_Message, uper_decoded);

  free_lpp_message(uper_decoded);
  free_lpp_message(message);

  return EXIT_SUCCESS;
}
