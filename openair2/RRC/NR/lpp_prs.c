/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "lpp_prs.h"

#include <limits.h>
#include "NR_BCCH-DL-SCH-Message.h"
#include "common/utils/oai_asn1.h"
#include "common/utils/utils.h"

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
