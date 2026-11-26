/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "openair2/RRC/NR/lpp_prs.h"
#include "openair2/RRC/NR_UE/lpp_prs_ue.h"
#include "f1ap_positioning.h"
#include "F1AP_F1AP-PDU.h"
#include "LPP_AssistanceDataSIBelement-r15.h"
#include "LPP_NR-DL-PRS-AssistanceData-r16.h"
#include "LPP_NR-DL-PRS-AssistanceDataPerFreq-r16.h"
#include "LPP_NR-DL-PRS-AssistanceDataPerTRP-r16.h"
#include "LPP_NCGI-r15.h"
#include "LPP_NR-DL-PRS-ResourceSet-r16.h"
#include "LPP_NR-DL-PRS-Resource-r16.h"
#include "LPP_DL-PRS-MutingOption1-r16.h"
#include "aper_encoder.h"
#include "aper_decoder.h"
#include "uper_decoder.h"

static void test_periods(f1ap_prs_configuration_t *prs, const f1ap_ng_ran_cgi_t *cgi)
{
  const uint16_t periods[] = {4, 5, 8, 10, 16, 20, 32, 40, 64, 80, 160, 320, 640, 1280, 2560, 5120, 10240};
  f1ap_prs_resource_set_t *set = &prs->prs_resource_set[0];
  const f1ap_prs_resource_set_t saved = *set;
  for (int scs = 0; scs < 4; ++scs) {
    set->subcarrier_spacing = scs;
    for (size_t i = 0; i < sizeofArray(periods); ++i) {
      set->resource_set_periodicity = periods[i] << scs;
      for (int last_slot = 0; last_slot < 2; ++last_slot) {
        set->resource_set_slot_offset = last_slot ? set->resource_set_periodicity - 1 : 0;
        uint8_t *segment = NULL;
        size_t segment_len = 0;
        bool encoded = lpp_prs_from_f1(prs, cgi, 42, &segment, &segment_len);
        if (set->resource_set_periodicity > UINT16_MAX) {
          AssertFatal(!encoded, "accepted a period exceeding the MAC representation\n");
          continue;
        }
        AssertFatal(encoded, "could not encode SCS %d period %u\n", scs, set->resource_set_periodicity);
        LPP_AssistanceDataSIBelement_r15_t *sib = NULL;
        asn_dec_rval_t dec = uper_decode_complete(NULL, &asn_DEF_LPP_AssistanceDataSIBelement_r15,
                                                (void **)&sib, segment, segment_len);
        AssertFatal(dec.code == RC_OK, "SIB element decode failed\n");
        LPP_NR_DL_PRS_AssistanceData_r16_t *assistance = NULL;
        dec = uper_decode_complete(NULL, &asn_DEF_LPP_NR_DL_PRS_AssistanceData_r16, (void **)&assistance,
                                   sib->assistanceDataElement_r15.buf, sib->assistanceDataElement_r15.size);
        AssertFatal(dec.code == RC_OK, "PRS assistance decode failed\n");
        const LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16_t *periodicity =
            &assistance->nr_DL_PRS_AssistanceDataList_r16.list.array[0]
                 ->nr_DL_PRS_AssistanceDataPerFreq_r16.list.array[0]
                 ->nr_DL_PRS_Info_r16.nr_DL_PRS_ResourceSetList_r16.list.array[0]
                 ->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16;
        AssertFatal(periodicity->present == scs + 1, "incorrect ASN.1 SCS alternative\n");
        int alternative = 0;
        switch (scs) {
          case 0: alternative = periodicity->choice.scs15_r16->present; break;
          case 1: alternative = periodicity->choice.scs30_r16->present; break;
          case 2: alternative = periodicity->choice.scs60_r16->present; break;
          case 3: alternative = periodicity->choice.scs120_r16->present; break;
        }
        AssertFatal(alternative == i + 1, "incorrect ASN.1 period alternative\n");
        nr_ue_prs_configuration_t configuration;
        AssertFatal(lpp_nr_prs_assistance_to_configuration(assistance, NR_PRS_SOURCE_POS_SIB, &configuration),
                    "UE rejected SCS %d period %u\n", scs, set->resource_set_periodicity);
        const nr_ue_prs_target_config_t *target = &configuration.targets[0];
        AssertFatal(target->period == set->resource_set_periodicity && target->offset == set->resource_set_slot_offset,
                    "PRS period or offset changed during round trip\n");
        AssertFatal(target->resources[0].pdu.subcarrier_spacing == scs, "PRS SCS changed during round trip\n");
        ASN_STRUCT_FREE(asn_DEF_LPP_NR_DL_PRS_AssistanceData_r16, assistance);
        ASN_STRUCT_FREE(asn_DEF_LPP_AssistanceDataSIBelement_r15, sib);
        free(segment);
      }
    }
    uint8_t *segment = NULL;
    size_t segment_len = 0;
    set->resource_set_periodicity = 7 << scs;
    set->resource_set_slot_offset = 0;
    AssertFatal(!lpp_prs_from_f1(prs, cgi, 42, &segment, &segment_len), "accepted an invalid PRS period\n");
  }
  *set = saved;
}

int main(void)
{
  logInit();
  f1ap_prs_resource_t resources[] = {
      {.prs_resource_id = 2, .sequence_id = 43, .re_offset = 1, .resource_slot_offset = 3, .resource_symbol_offset = 2},
      {.prs_resource_id = 7, .sequence_id = 71, .re_offset = 3, .resource_slot_offset = 5, .resource_symbol_offset = 2},
  };
  f1ap_prs_resource_set_t set = {.prs_resource_set_id = 3, .subcarrier_spacing = 1, .num_rb = 104,
      .start_prb = 12, .point_a = 640008, .comb_size = 4, .resource_set_periodicity = 640,
      .resource_set_slot_offset = 17, .resource_repetition_factor = 2, .resource_time_gap = 4,
      .resource_number_of_symbols = 4, .muting1_length = 4, .muting1_bits = {0xa0}, .muting1_repetition = 2,
      .prs_resource_transmit_power = -10, .prs_resource_list_length = 2, .prs_resource = resources};
  resources[1].has_qcl_prs = true;
  resources[1].qcl_prs_set_id = set.prs_resource_set_id;
  resources[1].has_qcl_prs_resource_id = true;
  resources[1].qcl_prs_resource_id = resources[0].prs_resource_id;
  f1ap_trp_information_type_response_item_t item = {.present = F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_PRSCONFIGURATION,
      .choice.prs_configuration = {.prs_resource_set_list_length = 1, .prs_resource_set = &set}};
  f1ap_trp_information_t trp = {.trp_id = 12, .trp_information_type_response_list = {
      .trp_information_type_response_item_length = 1, .trp_information_type_response_item = &item}};
  f1ap_trp_information_resp_t original = {.transaction_id = 17, .trp_information_list = {
      .trp_information_item_length = 1, .trp_information_item = &trp}};
  F1AP_F1AP_PDU_t *pdu = encode_trp_information_resp(&original);
  uint8_t *wire = NULL;
  ssize_t wire_len = aper_encode_to_new_buffer(&asn_DEF_F1AP_F1AP_PDU, NULL, pdu, (void **)&wire);
  AssertFatal(wire_len > 0, "F1 encoding failed\n");
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  pdu = NULL;
  asn_dec_rval_t dec = aper_decode_complete(NULL, &asn_DEF_F1AP_F1AP_PDU, (void **)&pdu, wire, wire_len);
  AssertFatal(dec.code == RC_OK, "F1 decoding failed\n");
  free(wire);
  f1ap_trp_information_resp_t response = {0};
  AssertFatal(decode_trp_information_resp(pdu, &response), "TRP response decoding failed\n");
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  f1ap_prs_configuration_t *prs = &response.trp_information_list.trp_information_item[0]
                                        .trp_information_type_response_list.trp_information_type_response_item[0]
                                        .choice.prs_configuration;
  f1ap_ng_ran_cgi_t cgi = {.plmn = {.mcc = 208, .mnc = 95, .mnc_digit_length = 2}, .nr_cellid = 0xf12345678};
  test_periods(prs, &cgi);
  uint8_t *segment = NULL;
  size_t segment_len = 0;
  AssertFatal(lpp_prs_from_f1(prs, &cgi, 42, &segment, &segment_len), "F1 to LPP conversion failed\n");
  LPP_AssistanceDataSIBelement_r15_t *sib = NULL;
  dec = uper_decode_complete(NULL, &asn_DEF_LPP_AssistanceDataSIBelement_r15, (void **)&sib, segment, segment_len);
  AssertFatal(dec.code == RC_OK, "LPP SIB element decode failed\n");
  LPP_NR_DL_PRS_AssistanceData_r16_t *assistance = NULL;
  dec = uper_decode_complete(NULL, &asn_DEF_LPP_NR_DL_PRS_AssistanceData_r16, (void **)&assistance,
                             sib->assistanceDataElement_r15.buf, sib->assistanceDataElement_r15.size);
  AssertFatal(dec.code == RC_OK, "LPP PRS assistance decode failed\n");
  LPP_NR_DL_PRS_AssistanceDataPerFreq_r16_t *freq = assistance->nr_DL_PRS_AssistanceDataList_r16.list.array[0];
  LPP_NR_DL_PRS_AssistanceDataPerTRP_r16_t *decoded_trp = freq->nr_DL_PRS_AssistanceDataPerFreq_r16.list.array[0];
  const uint8_t cell_id[] = {0xf1, 0x23, 0x45, 0x67, 0x80};
  const BIT_STRING_t *decoded_cell_id = &decoded_trp->nr_CellGlobalID_r16->nr_cellidentity_r15;
  AssertFatal(decoded_cell_id->size == sizeof(cell_id) && decoded_cell_id->bits_unused == 4 &&
                  memcmp(decoded_cell_id->buf, cell_id, sizeof(cell_id)) == 0,
              "NR cell identity changed during conversion\n");
  LPP_NR_DL_PRS_ResourceSet_r16_t *decoded_set = decoded_trp->nr_DL_PRS_Info_r16.nr_DL_PRS_ResourceSetList_r16.list.array[0];
  AssertFatal(freq->nr_DL_PRS_PositioningFrequencyLayer_r16.dl_PRS_PointA_r16 == set.point_a &&
                  freq->nr_DL_PRS_PositioningFrequencyLayer_r16.dl_PRS_ResourceBandwidth_r16 == (set.num_rb - 20) / 4 &&
                  decoded_set->nr_DL_PRS_ResourceSetID_r16 == set.prs_resource_set_id &&
                  decoded_set->dl_PRS_ResourcePower_r16 == set.prs_resource_transmit_power &&
                  decoded_set->dl_PRS_ResourceList_r16.list.count == 2 &&
                  decoded_set->dl_PRS_ResourceList_r16.list.array[1]->nr_DL_PRS_ResourceID_r16 == resources[1].prs_resource_id &&
                  decoded_set->dl_PRS_ResourceList_r16.list.array[1]->dl_PRS_SequenceID_r16 == resources[1].sequence_id &&
                  decoded_set->dl_PRS_ResourceList_r16.list.array[1]->dl_PRS_QCL_Info_r16 != NULL &&
                  decoded_set->dl_PRS_MutingOption1_r16 != NULL, "LPP fields differ from decoded F1 response\n");
  AssertFatal(decoded_set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16.present ==
                  LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16_PR_scs30_r16 &&
                  decoded_set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16.choice.scs30_r16->present ==
                      LPP_NR_DL_PRS_Periodicity_and_ResourceSetSlotOffset_r16__scs30_r16_PR_n640_r16 &&
                  decoded_set->dl_PRS_Periodicity_and_ResourceSetSlotOffset_r16.choice.scs30_r16->choice.n640_r16 == 17 &&
                  decoded_set->dl_PRS_MutingOption1_r16->nr_option1_muting_r16.present == LPP_NR_MutingPattern_r16_PR_po4_r16 &&
                  decoded_set->dl_PRS_MutingOption1_r16->nr_option1_muting_r16.choice.po4_r16.buf[0] == 0xa0,
              "period, offset or muting differs from F1\n");
  nr_ue_prs_configuration_t configuration;
  AssertFatal(lpp_nr_prs_assistance_to_configuration(assistance, NR_PRS_SOURCE_POS_SIB, &configuration),
              "UE LPP assistance conversion failed\n");
  DevAssert(configuration.num_targets == 1 && configuration.source == NR_PRS_SOURCE_POS_SIB);
  const nr_ue_prs_target_config_t *target = &configuration.targets[0];
  DevAssert(target->period == 640 && target->offset == 17 && target->repetition == 2 && target->time_gap == 4);
  DevAssert(target->num_resources == 2 && target->resource_set_id == 3 && target->point_a_arfcn == 640008);
  DevAssert(target->muting_pattern1_length == 4 && target->muting_bit_repetition == 2);
  DevAssert(target->muting_pattern1[0] == 1 && target->muting_pattern1[1] == 0 &&
            target->muting_pattern1[2] == 1 && target->muting_pattern1[3] == 0);
  for (int i = 0; i < target->num_resources; i++) {
    const nr_ue_prs_resource_t *resource = &target->resources[i];
    const fapi_nr_dl_config_prs_pdu_rel16_t *pdu = &resource->pdu;
    DevAssert(resource->slot_offset == resources[i].resource_slot_offset);
    DevAssert(pdu->nid_prs == resources[i].sequence_id && pdu->comb_offset == resources[i].re_offset);
    DevAssert(pdu->num_symbols == 4 && pdu->first_symbol == 2 && pdu->num_rbs == 104 && pdu->start_rb == 12);
    DevAssert(pdu->subcarrier_spacing == 1 && pdu->cyclic_prefix == 0 && pdu->comb_size == 4);
  }
  decoded_set->dl_PRS_ResourceList_r16.list.array[1]->dl_PRS_ResourceSymbolOffset_r16 = 13;
  AssertFatal(!lpp_nr_prs_assistance_to_configuration(assistance, NR_PRS_SOURCE_POS_SIB, &configuration),
              "UE accepted a PRS resource extending beyond the slot\n");
  ASN_STRUCT_FREE(asn_DEF_LPP_NR_DL_PRS_AssistanceData_r16, assistance);
  ASN_STRUCT_FREE(asn_DEF_LPP_AssistanceDataSIBelement_r15, sib);
  free(segment);
  prs->prs_resource_set[0].prs_resource[0].has_qcl_ssb = true;
  AssertFatal(!lpp_prs_from_f1(prs, &cgi, 42, &segment, &segment_len), "QCL without an LPP type accepted\n");
  prs->prs_resource_set[0].prs_resource[0].has_qcl_ssb = false;
  prs->prs_resource_set[0].prs_resource[1].prs_resource_id = prs->prs_resource_set[0].prs_resource[0].prs_resource_id;
  AssertFatal(!lpp_prs_from_f1(prs, &cgi, 42, &segment, &segment_len), "duplicate PRS resource ID accepted\n");
  prs->prs_resource_set[0].prs_resource[1].prs_resource_id = resources[1].prs_resource_id;
  prs->prs_resource_set[0].resource_set_slot_offset = prs->prs_resource_set[0].resource_set_periodicity;
  AssertFatal(!lpp_prs_from_f1(prs, &cgi, 42, &segment, &segment_len), "invalid PRS period offset accepted\n");
  prs->prs_resource_set[0].resource_set_slot_offset = set.resource_set_slot_offset;
  prs->prs_resource_set[0].muting1_length = 33;
  AssertFatal(!lpp_prs_from_f1(prs, &cgi, 42, &segment, &segment_len), "oversized muting pattern accepted\n");
  free_trp_information_resp(&response);
  logTerm();
  return 0;
}
