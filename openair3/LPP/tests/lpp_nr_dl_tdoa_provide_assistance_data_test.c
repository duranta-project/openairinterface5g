/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

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
#include "aper_decoder.h"
#include "aper_encoder.h"
#include "constraints.h"
#include "per_decoder.h"
#include "per_encoder.h"
#include "xer_encoder.h"

enum lpp_codec {
  LPP_CODEC_APER,
  LPP_CODEC_UPER,
};

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

static void fail(const char *message)
{
  fprintf(stderr, "%s\n", message);
  exit(EXIT_FAILURE);
}

static void *checked_calloc(size_t nmemb, size_t size)
{
  void *ptr = calloc(nmemb, size);
  if (ptr == NULL)
    fail("calloc() failed");
  return ptr;
}

static long *new_long(long value)
{
  long *ptr = checked_calloc(1, sizeof(*ptr));
  *ptr = value;
  return ptr;
}

static void checked_sequence_add(void *list, void *element)
{
  if (ASN_SEQUENCE_ADD(list, element) != 0)
    fail("ASN_SEQUENCE_ADD() failed");
}

static void set_bit_string36(BIT_STRING_t *bit_string, uint64_t value)
{
  bit_string->buf = checked_calloc(5, sizeof(*bit_string->buf));
  bit_string->size = 5;
  bit_string->bits_unused = 4;
  bit_string->buf[0] = (uint8_t)((value >> 28) & 0xff);
  bit_string->buf[1] = (uint8_t)((value >> 20) & 0xff);
  bit_string->buf[2] = (uint8_t)((value >> 12) & 0xff);
  bit_string->buf[3] = (uint8_t)((value >> 4) & 0xff);
  bit_string->buf[4] = (uint8_t)((value & 0x0f) << 4);
}

static void set_ssb_long_bitmap(BIT_STRING_t *bit_string, long ssb_index)
{
  bit_string->buf = checked_calloc(8, sizeof(*bit_string->buf));
  bit_string->size = 8;
  bit_string->bits_unused = 0;
  bit_string->buf[ssb_index / 8] = (uint8_t)(0x80u >> (ssb_index % 8));
}

static LPP_NCGI_r15_t *create_ncgi(uint64_t nr_cell_identity)
{
  LPP_NCGI_r15_t *ncgi = checked_calloc(1, sizeof(*ncgi));

  checked_sequence_add(&ncgi->mcc_r15.list, new_long(2));
  checked_sequence_add(&ncgi->mcc_r15.list, new_long(0));
  checked_sequence_add(&ncgi->mcc_r15.list, new_long(8));

  checked_sequence_add(&ncgi->mnc_r15.list, new_long(9));
  checked_sequence_add(&ncgi->mnc_r15.list, new_long(5));

  set_bit_string36(&ncgi->nr_cellidentity_r15, nr_cell_identity);
  return ncgi;
}

static LPP_DL_PRS_QCL_Info_r16_t *create_qcl_info(const trp_fixture_t *fixture)
{
  LPP_DL_PRS_QCL_Info_r16_t *qcl = checked_calloc(1, sizeof(*qcl));

  qcl->present = LPP_DL_PRS_QCL_Info_r16_PR_ssb_r16;
  qcl->choice.ssb_r16 = checked_calloc(1, sizeof(*qcl->choice.ssb_r16));
  qcl->choice.ssb_r16->pci_r16 = fixture->phys_cell_id;
  qcl->choice.ssb_r16->ssb_Index_r16 = fixture->ssb_index;
  qcl->choice.ssb_r16->rs_Type_r16 = LPP_DL_PRS_QCL_Info_r16__ssb_r16__rs_Type_r16_typeD;

  return qcl;
}

static LPP_NR_DL_PRS_Resource_r16_t *create_prs_resource(const trp_fixture_t *fixture)
{
  LPP_NR_DL_PRS_Resource_r16_t *resource = checked_calloc(1, sizeof(*resource));

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
  LPP_NR_DL_PRS_ResourceSet_r16_t *resource_set = checked_calloc(1, sizeof(*resource_set));

  resource_set->nr_DL_PRS_ResourceSetID_r16 = 0;
  resource_set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16.present =
      LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16_PR_scs30_r16;
  resource_set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16.choice.scs30_r16 =
      checked_calloc(1, sizeof(*resource_set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16.choice.scs30_r16));
  resource_set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16.choice.scs30_r16->present =
      LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR_n16_r16;
  resource_set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16.choice.scs30_r16->choice.n16_r16 = 3;
  resource_set->dl_PRS_ResourceRepetitionFactor_r16 =
      new_long(LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_ResourceRepetitionFactor_r16_n2);
  resource_set->dl_PRS_ResourceTimeGap_r16 =
      new_long(LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_ResourceTimeGap_r16_s1);
  resource_set->dl_PRS_NumSymbols_r16 = LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_NumSymbols_r16_n4;
  resource_set->dl_PRS_ResourcePower_r16 = -10;

  checked_sequence_add(&resource_set->dl_PRS_ResourceList_r16.list, create_prs_resource(fixture));
  return resource_set;
}

static LPP_NR_DL_PRS_AssistanceDataPerTRP_r16_t *create_trp_assistance_data(const trp_fixture_t *fixture)
{
  LPP_NR_DL_PRS_AssistanceDataPerTRP_r16_t *trp = checked_calloc(1, sizeof(*trp));

  trp->dl_PRS_ID_r16 = fixture->dl_prs_id;
  trp->nr_PhysCellID_r16 = new_long(fixture->phys_cell_id);
  trp->nr_CellGlobalID_r16 = create_ncgi(fixture->nr_cell_identity);
  trp->nr_ARFCN_r16 = new_long(fixture->arfcn);
  trp->nr_DL_PRS_SFN0_Offset_r16.sfn_Offset_r16 = fixture->sfn_offset;
  trp->nr_DL_PRS_SFN0_Offset_r16.integerSubframeOffset_r16 = fixture->subframe_offset;
  trp->nr_DL_PRS_ExpectedRSTD_r16 = fixture->expected_rstd;
  trp->nr_DL_PRS_ExpectedRSTD_Uncertainty_r16 = fixture->expected_rstd_uncertainty;

  checked_sequence_add(&trp->nr_DL_PRS_Info_r16.nr_DL_PRS_ResourceSetList_r16.list,
                       create_prs_resource_set(fixture));
  return trp;
}

static LPP_NR_DL_PRS_AssistanceDataPerFreq_r16_t *create_frequency_layer(void)
{
  LPP_NR_DL_PRS_AssistanceDataPerFreq_r16_t *frequency = checked_calloc(1, sizeof(*frequency));
  LPP_NR_DL_PRS_PositioningFrequencyLayer_r16_t *layer = &frequency->nr_DL_PRS_PositioningFrequencyLayer_r16;

  layer->dl_PRS_SubcarrierSpacing_r16 =
      LPP_NR_DL_PRS_PositioningFrequencyLayer_r16__dl_PRS_SubcarrierSpacing_r16_kHz30;
  layer->dl_PRS_ResourceBandwidth_r16 = 52;
  layer->dl_PRS_StartPRB_r16 = 24;
  layer->dl_PRS_PointA_r16 = 633984;
  layer->dl_PRS_CombSizeN_r16 = LPP_NR_DL_PRS_PositioningFrequencyLayer_r16__dl_PRS_CombSizeN_r16_n4;
  layer->dl_PRS_CyclicPrefix_r16 = LPP_NR_DL_PRS_PositioningFrequencyLayer_r16__dl_PRS_CyclicPrefix_r16_normal;

  for (size_t i = 0; i < sizeof(trps) / sizeof(trps[0]); ++i)
    checked_sequence_add(&frequency->nr_DL_PRS_AssistanceDataPerFreq_r16.list, create_trp_assistance_data(&trps[i]));

  return frequency;
}

static LPP_NR_SSB_Config_r16_t *create_ssb_config(const trp_fixture_t *fixture)
{
  LPP_NR_SSB_Config_r16_t *ssb = checked_calloc(1, sizeof(*ssb));

  ssb->nr_PhysCellID_r16 = fixture->phys_cell_id;
  ssb->nr_ARFCN_r16 = fixture->arfcn;
  ssb->ss_PBCH_BlockPower_r16 = -20;
  ssb->halfFrameIndex_r16 = fixture->half_frame_index;
  ssb->ssb_periodicity_r16 = LPP_NR_SSB_Config_r16__ssb_periodicity_r16_ms20;
  ssb->ssb_PositionsInBurst_r16 = checked_calloc(1, sizeof(*ssb->ssb_PositionsInBurst_r16));
  ssb->ssb_PositionsInBurst_r16->present = LPP_NR_SSB_Config_r16__ssb_PositionsInBurst_r16_PR_longBitmap_r16;
  set_ssb_long_bitmap(&ssb->ssb_PositionsInBurst_r16->choice.longBitmap_r16, fixture->ssb_index);
  ssb->ssb_SubcarrierSpacing_r16 = LPP_NR_SSB_Config_r16__ssb_SubcarrierSpacing_r16_kHz30;
  ssb->sfn_SSB_Offset_r16 = fixture->sfn_ssb_offset;

  return ssb;
}

static LPP_NR_DL_PRS_AssistanceData_r16_t *create_prs_assistance_data(void)
{
  LPP_NR_DL_PRS_AssistanceData_r16_t *assistance = checked_calloc(1, sizeof(*assistance));

  assistance->nr_DL_PRS_ReferenceInfo_r16.dl_PRS_ID_r16 = trps[0].dl_prs_id;
  assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceID_List_r16 =
      checked_calloc(1, sizeof(*assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceID_List_r16));
  checked_sequence_add(&assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceID_List_r16->list, new_long(0));
  assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceSetID_r16 = new_long(0);

  checked_sequence_add(&assistance->nr_DL_PRS_AssistanceDataList_r16.list, create_frequency_layer());

  assistance->nr_SSB_Config_r16 = checked_calloc(1, sizeof(*assistance->nr_SSB_Config_r16));
  for (size_t i = 0; i < sizeof(trps) / sizeof(trps[0]); ++i)
    checked_sequence_add(&assistance->nr_SSB_Config_r16->list, create_ssb_config(&trps[i]));

  return assistance;
}

static LPP_DL_SelectedPRS_ResourceSetIndex_r16_t *create_selected_resource_set_index(void)
{
  LPP_DL_SelectedPRS_ResourceSetIndex_r16_t *resource_set = checked_calloc(1, sizeof(*resource_set));
  LPP_DL_SelectedPRS_ResourceIndex_r16_t *resource = checked_calloc(1, sizeof(*resource));

  resource_set->nr_DL_SelectedPRS_ResourceSetIndex_r16 = 0;
  resource_set->dl_SelectedPRS_ResourceIndexList_r16 =
      checked_calloc(1, sizeof(*resource_set->dl_SelectedPRS_ResourceIndexList_r16));

  resource->nr_DL_SelectedPRS_ResourceIdIndex_r16 = 0;
  checked_sequence_add(&resource_set->dl_SelectedPRS_ResourceIndexList_r16->list, resource);

  return resource_set;
}

static LPP_NR_SelectedDL_PRS_IndexPerTRP_r16_t *create_selected_trp(long trp_index)
{
  LPP_NR_SelectedDL_PRS_IndexPerTRP_r16_t *selected_trp = checked_calloc(1, sizeof(*selected_trp));

  selected_trp->nr_SelectedTRP_Index_r16 = trp_index;
  selected_trp->dl_SelectedPRS_ResourceSetIndexList_r16 =
      checked_calloc(1, sizeof(*selected_trp->dl_SelectedPRS_ResourceSetIndexList_r16));
  checked_sequence_add(&selected_trp->dl_SelectedPRS_ResourceSetIndexList_r16->list,
                       create_selected_resource_set_index());

  return selected_trp;
}

static LPP_NR_SelectedDL_PRS_IndexList_r16_t *create_selected_prs_index_list(void)
{
  LPP_NR_SelectedDL_PRS_IndexList_r16_t *index_list = checked_calloc(1, sizeof(*index_list));
  LPP_NR_SelectedDL_PRS_PerFreq_r16_t *per_frequency = checked_calloc(1, sizeof(*per_frequency));

  per_frequency->nr_SelectedDL_PRS_FrequencyLayerIndex_r16 = 0;
  per_frequency->nr_SelectedDL_PRS_IndexListPerFreq_r16 =
      checked_calloc(1, sizeof(*per_frequency->nr_SelectedDL_PRS_IndexListPerFreq_r16));

  for (size_t i = 0; i < sizeof(trps) / sizeof(trps[0]); ++i)
    checked_sequence_add(&per_frequency->nr_SelectedDL_PRS_IndexListPerFreq_r16->list, create_selected_trp((long)i));

  checked_sequence_add(&index_list->list, per_frequency);
  return index_list;
}

static LPP_NR_DL_TDOA_ProvideAssistanceData_r16_t *create_nr_dl_tdoa_provide_assistance_data(void)
{
  LPP_NR_DL_TDOA_ProvideAssistanceData_r16_t *tdoa = checked_calloc(1, sizeof(*tdoa));

  tdoa->nr_DL_PRS_AssistanceData_r16 = create_prs_assistance_data();
  tdoa->nr_SelectedDL_PRS_IndexList_r16 = create_selected_prs_index_list();

  return tdoa;
}

static LPP_LPP_Message_t *create_provide_assistance_data_message(void)
{
  LPP_LPP_Message_t *message = checked_calloc(1, sizeof(*message));

  message->transactionID = checked_calloc(1, sizeof(*message->transactionID));
  message->transactionID->initiator = LPP_Initiator_locationServer;
  message->transactionID->transactionNumber = 2;
  message->endTransaction = false;

  message->lpp_MessageBody = checked_calloc(1, sizeof(*message->lpp_MessageBody));
  message->lpp_MessageBody->present = LPP_LPP_MessageBody_PR_c1;
  message->lpp_MessageBody->choice.c1 = checked_calloc(1, sizeof(*message->lpp_MessageBody->choice.c1));
  message->lpp_MessageBody->choice.c1->present = LPP_LPP_MessageBody__c1_PR_provideAssistanceData;

  LPP_ProvideAssistanceData_t *provide = checked_calloc(1, sizeof(*provide));
  message->lpp_MessageBody->choice.c1->choice.provideAssistanceData = provide;

  provide->criticalExtensions.present = LPP_ProvideAssistanceData__criticalExtensions_PR_c1;
  provide->criticalExtensions.choice.c1 = checked_calloc(1, sizeof(*provide->criticalExtensions.choice.c1));
  provide->criticalExtensions.choice.c1->present =
      LPP_ProvideAssistanceData__criticalExtensions__c1_PR_provideAssistanceData_r9;

  LPP_ProvideAssistanceData_r9_IEs_t *provide_r9 = checked_calloc(1, sizeof(*provide_r9));
  provide->criticalExtensions.choice.c1->choice.provideAssistanceData_r9 = provide_r9;
  provide_r9->ext2 = checked_calloc(1, sizeof(*provide_r9->ext2));
  provide_r9->ext2->nr_DL_TDOA_ProvideAssistanceData_r16 = create_nr_dl_tdoa_provide_assistance_data();

  return message;
}

static void check_lpp_constraints(const LPP_LPP_Message_t *message)
{
  char errbuf[1024];
  size_t errlen = sizeof(errbuf);
  int ret = asn_check_constraints(&asn_DEF_LPP_LPP_Message, message, errbuf, &errlen);
  if (ret != 0) {
    fprintf(stderr, "asn_check_constraints() failed: %s\n", errbuf);
    exit(EXIT_FAILURE);
  }
}

static LPP_LPP_Message_t *encode_decode_lpp_message(const LPP_LPP_Message_t *message, enum lpp_codec codec)
{
  check_lpp_constraints(message);

  uint8_t buffer[8192] = {0};
  asn_enc_rval_t enc = {0};
  const char *codec_name = NULL;

  if (codec == LPP_CODEC_APER) {
    codec_name = "APER";
    enc = aper_encode_to_buffer(&asn_DEF_LPP_LPP_Message, NULL, message, buffer, sizeof(buffer));
  } else {
    codec_name = "UPER";
    enc = uper_encode_to_buffer(&asn_DEF_LPP_LPP_Message, NULL, message, buffer, sizeof(buffer));
  }

  if (enc.encoded <= 0) {
    fprintf(stderr, "%s encode failed\n", codec_name);
    exit(EXIT_FAILURE);
  }

  LPP_LPP_Message_t *decoded = NULL;
  asn_codec_ctx_t ctx = {.max_stack_size = 100 * 1000};
  const size_t encoded_bytes = (enc.encoded + 7) / 8;
  asn_dec_rval_t dec = {0};

  if (codec == LPP_CODEC_APER)
    dec = aper_decode(&ctx, &asn_DEF_LPP_LPP_Message, (void **)&decoded, buffer, encoded_bytes, 0, 0);
  else
    dec = uper_decode_complete(&ctx, &asn_DEF_LPP_LPP_Message, (void **)&decoded, buffer, encoded_bytes);

  if (dec.code != RC_OK) {
    fprintf(stderr, "%s decode failed with code %d\n", codec_name, dec.code);
    ASN_STRUCT_FREE(asn_DEF_LPP_LPP_Message, decoded);
    exit(EXIT_FAILURE);
  }

  check_lpp_constraints(decoded);
  return decoded;
}

static const LPP_NR_DL_TDOA_ProvideAssistanceData_r16_t *get_tdoa_assistance(const LPP_LPP_Message_t *message)
{
  if (message->transactionID == NULL)
    fail("decoded message has no transactionID");
  if (message->transactionID->initiator != LPP_Initiator_locationServer)
    fail("decoded initiator mismatch");
  if (message->transactionID->transactionNumber != 2)
    fail("decoded transactionNumber mismatch");
  if (message->endTransaction != false)
    fail("decoded endTransaction mismatch");

  if (message->lpp_MessageBody == NULL || message->lpp_MessageBody->present != LPP_LPP_MessageBody_PR_c1)
    fail("decoded message body is not c1");

  const struct LPP_LPP_MessageBody__c1 *body_c1 = message->lpp_MessageBody->choice.c1;
  if (body_c1 == NULL || body_c1->present != LPP_LPP_MessageBody__c1_PR_provideAssistanceData)
    fail("decoded c1 body is not provideAssistanceData");

  const LPP_ProvideAssistanceData_t *provide = body_c1->choice.provideAssistanceData;
  if (provide == NULL || provide->criticalExtensions.present != LPP_ProvideAssistanceData__criticalExtensions_PR_c1)
    fail("decoded provideAssistanceData has no c1 criticalExtensions");

  const struct LPP_ProvideAssistanceData__criticalExtensions__c1 *provide_c1 =
      provide->criticalExtensions.choice.c1;
  if (provide_c1 == NULL
      || provide_c1->present != LPP_ProvideAssistanceData__criticalExtensions__c1_PR_provideAssistanceData_r9)
    fail("decoded criticalExtensions c1 is not provideAssistanceData-r9");

  const LPP_ProvideAssistanceData_r9_IEs_t *provide_r9 = provide_c1->choice.provideAssistanceData_r9;
  if (provide_r9 == NULL || provide_r9->ext2 == NULL || provide_r9->ext2->nr_DL_TDOA_ProvideAssistanceData_r16 == NULL)
    fail("decoded provideAssistanceData-r9 has no NR-DL-TDOA assistance data");

  return provide_r9->ext2->nr_DL_TDOA_ProvideAssistanceData_r16;
}

static void assert_prs_resource(const LPP_NR_DL_PRS_Resource_r16_t *resource, const trp_fixture_t *fixture)
{
  if (resource == NULL)
    fail("decoded PRS resource is missing");
  if (resource->nr_DL_PRS_ResourceID_r16 != 0)
    fail("decoded PRS resource ID mismatch");
  if (resource->dl_PRS_SequenceID_r16 != fixture->sequence_id)
    fail("decoded PRS sequence ID mismatch");
  if (resource->dl_PRS_CombSizeN_AndReOffset_r16.present
      != LPP_NR_DL_PRS_Resource_r16__dl_PRS_CombSizeN_AndReOffset_r16_PR_n4_r16)
    fail("decoded PRS comb size mismatch");
  if (resource->dl_PRS_CombSizeN_AndReOffset_r16.choice.n4_r16 != fixture->comb_offset)
    fail("decoded PRS comb offset mismatch");
  if (resource->dl_PRS_ResourceSlotOffset_r16 != fixture->slot_offset)
    fail("decoded PRS slot offset mismatch");
  if (resource->dl_PRS_ResourceSymbolOffset_r16 != fixture->symbol_offset)
    fail("decoded PRS symbol offset mismatch");

  const LPP_DL_PRS_QCL_Info_r16_t *qcl = resource->dl_PRS_QCL_Info_r16;
  if (qcl == NULL || qcl->present != LPP_DL_PRS_QCL_Info_r16_PR_ssb_r16 || qcl->choice.ssb_r16 == NULL)
    fail("decoded PRS QCL info mismatch");
  if (qcl->choice.ssb_r16->pci_r16 != fixture->phys_cell_id || qcl->choice.ssb_r16->ssb_Index_r16 != fixture->ssb_index
      || qcl->choice.ssb_r16->rs_Type_r16 != LPP_DL_PRS_QCL_Info_r16__ssb_r16__rs_Type_r16_typeD)
    fail("decoded PRS QCL SSB content mismatch");
}

static void assert_trp(const LPP_NR_DL_PRS_AssistanceDataPerTRP_r16_t *trp, const trp_fixture_t *fixture)
{
  if (trp == NULL)
    fail("decoded TRP assistance data is missing");
  if (trp->dl_PRS_ID_r16 != fixture->dl_prs_id)
    fail("decoded TRP DL-PRS ID mismatch");
  if (trp->nr_PhysCellID_r16 == NULL || *trp->nr_PhysCellID_r16 != fixture->phys_cell_id)
    fail("decoded TRP physical cell ID mismatch");
  if (trp->nr_CellGlobalID_r16 == NULL || trp->nr_CellGlobalID_r16->nr_cellidentity_r15.size != 5)
    fail("decoded TRP NCGI mismatch");
  if (trp->nr_ARFCN_r16 == NULL || *trp->nr_ARFCN_r16 != fixture->arfcn)
    fail("decoded TRP ARFCN mismatch");
  if (trp->nr_DL_PRS_SFN0_Offset_r16.sfn_Offset_r16 != fixture->sfn_offset
      || trp->nr_DL_PRS_SFN0_Offset_r16.integerSubframeOffset_r16 != fixture->subframe_offset)
    fail("decoded TRP SFN0 offset mismatch");
  if (trp->nr_DL_PRS_ExpectedRSTD_r16 != fixture->expected_rstd
      || trp->nr_DL_PRS_ExpectedRSTD_Uncertainty_r16 != fixture->expected_rstd_uncertainty)
    fail("decoded TRP expected RSTD mismatch");

  const struct LPP_NR_DL_PRS_Info_r16__nr_DL_PRS_ResourceSetList_r16 *set_list =
      &trp->nr_DL_PRS_Info_r16.nr_DL_PRS_ResourceSetList_r16;
  if (set_list->list.count != 1)
    fail("decoded TRP resource set list count mismatch");

  const LPP_NR_DL_PRS_ResourceSet_r16_t *resource_set = set_list->list.array[0];
  if (resource_set == NULL || resource_set->nr_DL_PRS_ResourceSetID_r16 != 0)
    fail("decoded PRS resource set mismatch");
  if (resource_set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16.present
      != LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16_PR_scs30_r16)
    fail("decoded PRS periodicity SCS mismatch");
  if (resource_set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16.choice.scs30_r16 == NULL
      || resource_set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16.choice.scs30_r16->present
             != LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR_n16_r16
      || resource_set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16.choice.scs30_r16->choice.n16_r16 != 3)
    fail("decoded PRS periodicity slot offset mismatch");
  if (resource_set->dl_PRS_ResourceRepetitionFactor_r16 == NULL
      || *resource_set->dl_PRS_ResourceRepetitionFactor_r16
             != LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_ResourceRepetitionFactor_r16_n2)
    fail("decoded PRS repetition factor mismatch");
  if (resource_set->dl_PRS_ResourceTimeGap_r16 == NULL
      || *resource_set->dl_PRS_ResourceTimeGap_r16
             != LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_ResourceTimeGap_r16_s1)
    fail("decoded PRS time gap mismatch");
  if (resource_set->dl_PRS_NumSymbols_r16 != LPP_NR_DL_PRS_ResourceSet_r16__dl_PRS_NumSymbols_r16_n4)
    fail("decoded PRS number of symbols mismatch");
  if (resource_set->dl_PRS_ResourcePower_r16 != -10)
    fail("decoded PRS resource power mismatch");
  if (resource_set->dl_PRS_ResourceList_r16.list.count != 1)
    fail("decoded PRS resource list count mismatch");

  assert_prs_resource(resource_set->dl_PRS_ResourceList_r16.list.array[0], fixture);
}

static void assert_provide_assistance_data_message(const LPP_LPP_Message_t *message)
{
  const LPP_NR_DL_TDOA_ProvideAssistanceData_r16_t *tdoa = get_tdoa_assistance(message);

  if (tdoa->nr_DL_PRS_AssistanceData_r16 == NULL)
    fail("decoded NR-DL-TDOA has no PRS assistance data");
  if (tdoa->nr_SelectedDL_PRS_IndexList_r16 == NULL)
    fail("decoded NR-DL-TDOA has no selected PRS index list");

  const LPP_NR_DL_PRS_AssistanceData_r16_t *assistance = tdoa->nr_DL_PRS_AssistanceData_r16;
  if (assistance->nr_DL_PRS_ReferenceInfo_r16.dl_PRS_ID_r16 != trps[0].dl_prs_id)
    fail("decoded reference DL-PRS ID mismatch");
  if (assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceID_List_r16 == NULL
      || assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceID_List_r16->list.count != 1
      || *assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceID_List_r16->list.array[0] != 0)
    fail("decoded reference resource ID list mismatch");
  if (assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceSetID_r16 == NULL
      || *assistance->nr_DL_PRS_ReferenceInfo_r16.nr_DL_PRS_ResourceSetID_r16 != 0)
    fail("decoded reference resource set ID mismatch");
  if (assistance->nr_DL_PRS_AssistanceDataList_r16.list.count != 1)
    fail("decoded frequency layer list count mismatch");
  if (assistance->nr_SSB_Config_r16 == NULL || assistance->nr_SSB_Config_r16->list.count != 4)
    fail("decoded SSB config list count mismatch");

  const LPP_NR_DL_PRS_AssistanceDataPerFreq_r16_t *frequency =
      assistance->nr_DL_PRS_AssistanceDataList_r16.list.array[0];
  const LPP_NR_DL_PRS_PositioningFrequencyLayer_r16_t *layer = &frequency->nr_DL_PRS_PositioningFrequencyLayer_r16;
  if (layer->dl_PRS_SubcarrierSpacing_r16
          != LPP_NR_DL_PRS_PositioningFrequencyLayer_r16__dl_PRS_SubcarrierSpacing_r16_kHz30
      || layer->dl_PRS_ResourceBandwidth_r16 != 52 || layer->dl_PRS_StartPRB_r16 != 24
      || layer->dl_PRS_PointA_r16 != 633984
      || layer->dl_PRS_CombSizeN_r16 != LPP_NR_DL_PRS_PositioningFrequencyLayer_r16__dl_PRS_CombSizeN_r16_n4
      || layer->dl_PRS_CyclicPrefix_r16
             != LPP_NR_DL_PRS_PositioningFrequencyLayer_r16__dl_PRS_CyclicPrefix_r16_normal)
    fail("decoded positioning frequency layer mismatch");
  if (frequency->nr_DL_PRS_AssistanceDataPerFreq_r16.list.count != 4)
    fail("decoded TRP list count mismatch");

  for (size_t i = 0; i < sizeof(trps) / sizeof(trps[0]); ++i) {
    assert_trp(frequency->nr_DL_PRS_AssistanceDataPerFreq_r16.list.array[i], &trps[i]);
    const LPP_NR_SSB_Config_r16_t *ssb = assistance->nr_SSB_Config_r16->list.array[i];
    if (ssb == NULL || ssb->nr_PhysCellID_r16 != trps[i].phys_cell_id || ssb->nr_ARFCN_r16 != trps[i].arfcn
        || ssb->ssb_PositionsInBurst_r16 == NULL
        || ssb->ssb_PositionsInBurst_r16->present != LPP_NR_SSB_Config_r16__ssb_PositionsInBurst_r16_PR_longBitmap_r16)
      fail("decoded SSB config mismatch");
  }

  if (tdoa->nr_SelectedDL_PRS_IndexList_r16->list.count != 1)
    fail("decoded selected PRS frequency list count mismatch");
  const LPP_NR_SelectedDL_PRS_PerFreq_r16_t *selected_frequency = tdoa->nr_SelectedDL_PRS_IndexList_r16->list.array[0];
  if (selected_frequency == NULL || selected_frequency->nr_SelectedDL_PRS_FrequencyLayerIndex_r16 != 0
      || selected_frequency->nr_SelectedDL_PRS_IndexListPerFreq_r16 == NULL
      || selected_frequency->nr_SelectedDL_PRS_IndexListPerFreq_r16->list.count != 4)
    fail("decoded selected PRS frequency mismatch");

  for (int i = 0; i < selected_frequency->nr_SelectedDL_PRS_IndexListPerFreq_r16->list.count; ++i) {
    const LPP_NR_SelectedDL_PRS_IndexPerTRP_r16_t *selected_trp =
        selected_frequency->nr_SelectedDL_PRS_IndexListPerFreq_r16->list.array[i];
    if (selected_trp == NULL || selected_trp->nr_SelectedTRP_Index_r16 != i
        || selected_trp->dl_SelectedPRS_ResourceSetIndexList_r16 == NULL
        || selected_trp->dl_SelectedPRS_ResourceSetIndexList_r16->list.count != 1)
      fail("decoded selected PRS TRP mismatch");
  }
}

int main(void)
{
  LPP_LPP_Message_t *message = create_provide_assistance_data_message();

  LPP_LPP_Message_t *aper_decoded = encode_decode_lpp_message(message, LPP_CODEC_APER);
  assert_provide_assistance_data_message(aper_decoded);
  ASN_STRUCT_FREE(asn_DEF_LPP_LPP_Message, aper_decoded);

  LPP_LPP_Message_t *uper_decoded = encode_decode_lpp_message(message, LPP_CODEC_UPER);
  assert_provide_assistance_data_message(uper_decoded);

  xer_fprint(stdout, &asn_DEF_LPP_LPP_Message, uper_decoded);

  ASN_STRUCT_FREE(asn_DEF_LPP_LPP_Message, uper_decoded);
  ASN_STRUCT_FREE(asn_DEF_LPP_LPP_Message, message);

  return EXIT_SUCCESS;
}
