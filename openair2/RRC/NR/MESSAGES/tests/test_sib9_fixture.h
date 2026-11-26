/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <string.h>
#include "common/ran_context.h"
#include "common/utils/LOG/log.h"
#include "common/utils/oai_asn1.h"
#include "openair2/RRC/NR/nr_sib9.h"
#include "openair2/RRC/NR/MESSAGES/asn1_msg.h"
#include "openair2/LAYER2/NR_MAC_gNB/mac_proto.h"
#include "openair2/LAYER2/NR_MAC_gNB/nr_radio_config.h"
#include "openair2/F1AP/lib/f1ap_interface_management.h"
#include "F1AP_F1AP-PDU.h"
#include "NR_SIB1.h"
#include "NR_SIB-TypeInfo.h"
#include "NR_SI-SchedulingInfo.h"
#include "NR_PLMN-IdentityInfo.h"
#include "NR_PLMN-Identity.h"
#include "NR_SystemInformation.h"
#include "NR_SIB19-r17.h"
#include "NR_NTN-Config-r17.h"
#include "NR_EpochTime-r17.h"
#include "aper_encoder.h"
#include "aper_decoder.h"
#include "uper_decoder.h"
#include "uper_encoder.h"

static NR_BCCH_DL_SCH_Message_t *make_sib1(uint64_t nci)
{
  NR_BCCH_DL_SCH_Message_t *msg = calloc_or_fail(1, sizeof(*msg));
  msg->message.present = NR_BCCH_DL_SCH_MessageType_PR_c1;
  msg->message.choice.c1 = calloc_or_fail(1, sizeof(*msg->message.choice.c1));
  msg->message.choice.c1->present = NR_BCCH_DL_SCH_MessageType__c1_PR_systemInformationBlockType1;
  NR_SIB1_t *sib1 = calloc_or_fail(1, sizeof(*sib1));
  msg->message.choice.c1->choice.systemInformationBlockType1 = sib1;
  NR_PLMN_IdentityInfo_t *identity = calloc_or_fail(1, sizeof(*identity));
  NR_PLMN_Identity_t *plmn = calloc_or_fail(1, sizeof(*plmn));
  plmn->mcc = calloc_or_fail(1, sizeof(*plmn->mcc));
  for (int i = 0; i < 3; ++i) {
    long *digit = calloc_or_fail(1, sizeof(*digit));
    *digit = i == 2 ? 1 : 0;
    asn1cSeqAdd(&plmn->mcc->list, digit);
  }
  for (int i = 0; i < 2; ++i) {
    long *digit = calloc_or_fail(1, sizeof(*digit));
    *digit = i;
    asn1cSeqAdd(&plmn->mnc.list, digit);
  }
  asn1cSeqAdd(&identity->plmn_IdentityList.list, plmn);
  identity->cellIdentity.buf = calloc_or_fail(5, 1);
  identity->cellIdentity.size = 5;
  identity->cellIdentity.bits_unused = 4;
  for (int i = 0; i < 5; ++i)
    identity->cellIdentity.buf[i] = (nci << 4) >> (8 * (4 - i));
  identity->cellReservedForOperatorUse = NR_PLMN_IdentityInfo__cellReservedForOperatorUse_notReserved;
  asn1cSeqAdd(&sib1->cellAccessRelatedInfo.plmn_IdentityInfoList.list, identity);
  return msg;
}

static NR_BCCH_DL_SCH_Message_t *decode_bcch(const uint8_t *buf, size_t len)
{
  NR_BCCH_DL_SCH_Message_t *decoded = NULL;
  asn_dec_rval_t dec = uper_decode_complete(NULL, &asn_DEF_NR_BCCH_DL_SCH_Message, (void **)&decoded, buf, len);
  AssertFatal(dec.code == RC_OK && decoded && decoded->message.present == NR_BCCH_DL_SCH_MessageType_PR_c1, "BCCH decoding\n");
  return decoded;
}

static uint64_t read_utc(const uint8_t *pdu, int length)
{
  NR_BCCH_DL_SCH_Message_t *decoded = decode_bcch(pdu, length);
  NR_SystemInformation_IEs_t *si =
      decoded->message.choice.c1->choice.systemInformation->criticalExtensions.choice.systemInformation;
  AssertFatal(si->sib_TypeAndInfo.list.count == 1
                  && si->sib_TypeAndInfo.list.array[0]->present == NR_SystemInformation_IEs__sib_TypeAndInfo__Member_PR_sib9,
              "dedicated SIB9 SI\n");
  uint64_t utc;
  AssertFatal(!asn_INTEGER2uint64(&si->sib_TypeAndInfo.list.array[0]->choice.sib9->timeInfo->timeInfoUTC, &utc), "UTC\n");
  ASN_STRUCT_FREE(asn_DEF_NR_BCCH_DL_SCH_Message, decoded);
  return utc;
}
