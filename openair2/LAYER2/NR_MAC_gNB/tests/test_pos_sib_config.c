/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */

#include "NR_MAC_gNB/mac_proto.h"
#include "NR_MAC_gNB/nr_radio_config.h"
#include "common/utils/oai_asn1.h"
#include "uper_decoder.h"
#include "uper_encoder.h"

static NR_BCCH_DL_SCH_Message_t *make_sib1(void)
{
  NR_BCCH_DL_SCH_Message_t *message = calloc_or_fail(1, sizeof(*message));
  message->message.present = NR_BCCH_DL_SCH_MessageType_PR_c1;
  message->message.choice.c1 = calloc_or_fail(1, sizeof(*message->message.choice.c1));
  message->message.choice.c1->present = NR_BCCH_DL_SCH_MessageType__c1_PR_systemInformationBlockType1;
  NR_SIB1_t *sib1 = calloc_or_fail(1, sizeof(*sib1));
  message->message.choice.c1->choice.systemInformationBlockType1 = sib1;
  NR_PLMN_IdentityInfo_t *info = calloc_or_fail(1, sizeof(*info));
  info->cellIdentity.buf = calloc_or_fail(5, 1);
  info->cellIdentity.size = 5;
  info->cellIdentity.bits_unused = 4;
  NR_PLMN_Identity_t *plmn = calloc_or_fail(1, sizeof(*plmn));
  for (int i = 0; i < 2; i++) {
    long *digit;
    asn1cCallocOne(digit, 1);
    asn1cSeqAdd(&plmn->mnc.list, digit);
  }
  asn1cSeqAdd(&info->plmn_IdentityList.list, plmn);
  asn1cSeqAdd(&sib1->cellAccessRelatedInfo.plmn_IdentityInfoList.list, info);
  return message;
}

static void test_other_si_update(uint16_t pos_period, bool expected_active)
{
  nr_cell_sched_t *cell = calloc_or_fail(1, sizeof(*cell));
  NR_COMMON_channels_t *cc = &cell->common_channels;
  cc->sib1 = make_sib1();
  cc->pos_sib_bcch_length = 1;
  cc->pos_sib_periodicity = pos_period;

  NR_SIB2_t sib2 = {0};
  sib2.intraFreqCellReselectionInfo.q_RxLevMin = -65;
  uint8_t buffer[128];
  asn_enc_rval_t enc = uper_encode_to_buffer(&asn_DEF_NR_SIB2, NULL, &sib2, buffer, sizeof(buffer));
  AssertFatal(enc.encoded > 0, "cannot encode test SIB2\n");
  const f1ap_sib_msg_t cu_sib = {.SI_type = NR_SIB_2, .SI_container = {.buf = buffer, .len = (enc.encoded + 7) / 8}};
  AssertFatal(nr_mac_configure_other_sib(cell, 1, &cu_sib), "ordinary SI update must be reported independently of PosSI\n");
  AssertFatal(cc->pos_sib_active == expected_active, "unexpected PosSI activation state\n");
  AssertFatal(cc->other_sib_bcch_length[0] > 0, "ordinary SI payload missing\n");

  NR_BCCH_DL_SCH_Message_t *decoded = NULL;
  asn_dec_rval_t dec =
      uper_decode_complete(NULL, &asn_DEF_NR_BCCH_DL_SCH_Message, (void **)&decoded, cc->sib1_bcch_pdu, cc->sib1_bcch_length);
  AssertFatal(dec.code == RC_OK, "updated SIB1 cannot be decoded\n");
  NR_SIB1_t *sib1 = decoded->message.choice.c1->choice.systemInformationBlockType1;
  AssertFatal(sib1->si_SchedulingInfo && sib1->si_SchedulingInfo->schedulingInfoList.list.count == 1,
              "ordinary SI schedule missing from broadcast SIB1\n");
  const bool has_pos_schedule = sib1->nonCriticalExtension && sib1->nonCriticalExtension->posSI_SchedulingInfo_r16;
  AssertFatal(has_pos_schedule == expected_active, "broadcast PosSI schedule disagrees with activation state\n");
  ASN_STRUCT_FREE(asn_DEF_NR_BCCH_DL_SCH_Message, decoded);
  free_SIB1_NR(cc->sib1);
  free(cell);
}

int main(void)
{
  logInit();
  test_other_si_update(16, true);
  test_other_si_update(7, false);
  logTerm();
  return 0;
}
