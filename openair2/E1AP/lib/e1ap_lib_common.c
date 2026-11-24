/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "e1ap_lib_common.h"
#include "e1ap_lib_includes.h"
#include "e1ap_messages_types.h"
#include "common/utils/assertions.h"
#include "common/utils/utils.h"
#include "E1AP_SNSSAI.h"

E1AP_Cause_t e1_encode_cause_ie(const e1ap_cause_t *cause)
{
  E1AP_Cause_t ie = {0};
  switch (cause->type) {
    case E1AP_CAUSE_RADIO_NETWORK:
      ie.present = E1AP_Cause_PR_radioNetwork;
      ie.choice.radioNetwork = cause->value;
      break;
    case E1AP_CAUSE_TRANSPORT:
      ie.present = E1AP_Cause_PR_transport;
      ie.choice.transport = cause->value;
      break;
    case E1AP_CAUSE_PROTOCOL:
      ie.present = E1AP_Cause_PR_protocol;
      ie.choice.protocol = cause->value;
      break;
    case E1AP_CAUSE_MISC:
      ie.present = E1AP_Cause_PR_misc;
      ie.choice.misc = cause->value;
      break;
    case E1AP_CAUSE_NOTHING:
    default:
      AssertFatal(false, "unknown cause value %d\n", cause->type);
      break;
  }
  return ie;
}

bool e1_decode_cause_ie(const E1AP_Cause_t *ie, e1ap_cause_t *out)
{
  *out = (e1ap_cause_t){0};
  switch (ie->present) {
    case E1AP_Cause_PR_radioNetwork:
      out->value = ie->choice.radioNetwork;
      out->type = E1AP_CAUSE_RADIO_NETWORK;
      break;
    case E1AP_Cause_PR_transport:
      out->value = ie->choice.transport;
      out->type = E1AP_CAUSE_TRANSPORT;
      break;
    case E1AP_Cause_PR_protocol:
      out->value = ie->choice.protocol;
      out->type = E1AP_CAUSE_PROTOCOL;
      break;
    case E1AP_Cause_PR_misc:
      out->value = ie->choice.misc;
      out->type = E1AP_CAUSE_MISC;
      break;
    case E1AP_Cause_PR_NOTHING:
    case E1AP_Cause_PR_choice_extension:
    default:
      PRINT_ERROR("received illegal E1AP cause %d\n", ie->present);
      return false;
  }
  return true;
}

/** @brief S-NSSAI Encoding (3GPP TS 38.463 9.3.1.9) */
E1AP_SNSSAI_t e1_encode_snssai(const nssai_t *in)
{
  E1AP_SNSSAI_t out = {0};
  INT8_TO_OCTET_STRING(in->sst, &out.sST);
  if (in->sd != 0xffffff) {
    out.sD = malloc_or_fail(sizeof(*out.sD));
    INT24_TO_OCTET_STRING(in->sd, out.sD);
  }
  return out;
}

/** @brief S-NSSAI Decoding (3GPP TS 38.463 9.3.1.9) */
bool e1_decode_snssai(const E1AP_SNSSAI_t *in, e1ap_nssai_t *out)
{
  // SST (M)
  OCTET_STRING_TO_INT8(&in->sST, out->sst);
  out->sd = 0xffffff;
  // SD (O)
  if (in->sD != NULL)
    OCTET_STRING_TO_INT24(in->sD, out->sd);
  return true;
}
