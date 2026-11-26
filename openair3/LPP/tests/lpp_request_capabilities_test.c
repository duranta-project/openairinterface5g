/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "lpp_test_utils.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "LPP_A-GNSS-RequestCapabilities.h"
#include "LPP_LPP-Message.h"
#include "LPP_LPP-MessageBody.h"
#include "LPP_LPP-TransactionID.h"
#include "LPP_RequestCapabilities-r9-IEs.h"
#include "LPP_RequestCapabilities.h"
#include "xer_encoder.h"

static LPP_LPP_Message_t *create_request_capabilities_message(void)
{
  LPP_LPP_Message_t *message = calloc_or_fail(1, sizeof(*message));

  message->transactionID = calloc_or_fail(1, sizeof(*message->transactionID));
  message->transactionID->initiator = LPP_Initiator_locationServer;
  message->transactionID->transactionNumber = 1;
  message->endTransaction = false;

  message->lpp_MessageBody = calloc_or_fail(1, sizeof(*message->lpp_MessageBody));
  message->lpp_MessageBody->present = LPP_LPP_MessageBody_PR_c1;
  message->lpp_MessageBody->choice.c1 = calloc_or_fail(1, sizeof(*message->lpp_MessageBody->choice.c1));
  message->lpp_MessageBody->choice.c1->present = LPP_LPP_MessageBody__c1_PR_requestCapabilities;

  LPP_RequestCapabilities_t *request = calloc_or_fail(1, sizeof(*request));
  message->lpp_MessageBody->choice.c1->choice.requestCapabilities = request;

  request->criticalExtensions.present = LPP_RequestCapabilities__criticalExtensions_PR_c1;
  request->criticalExtensions.choice.c1 = calloc_or_fail(1, sizeof(*request->criticalExtensions.choice.c1));
  request->criticalExtensions.choice.c1->present =
      LPP_RequestCapabilities__criticalExtensions__c1_PR_requestCapabilities_r9;

  LPP_RequestCapabilities_r9_IEs_t *request_r9 = calloc_or_fail(1, sizeof(*request_r9));
  request->criticalExtensions.choice.c1->choice.requestCapabilities_r9 = request_r9;

  request_r9->a_gnss_RequestCapabilities = calloc_or_fail(1, sizeof(*request_r9->a_gnss_RequestCapabilities));
  request_r9->a_gnss_RequestCapabilities->gnss_SupportListReq = true;
  request_r9->a_gnss_RequestCapabilities->assistanceDataSupportListReq = true;
  request_r9->a_gnss_RequestCapabilities->locationVelocityTypesReq = true;

  return message;
}

int main(void)
{
  LPP_LPP_Message_t *message = create_request_capabilities_message();

  LPP_LPP_Message_t *aper_decoded = encode_decode_lpp_message(message, LPP_CODEC_APER);
  AssertFatal(eq_lpp_message(message, aper_decoded), "decoded LPP message differs\n");
  free_lpp_message(aper_decoded);

  LPP_LPP_Message_t *uper_decoded = encode_decode_lpp_message(message, LPP_CODEC_UPER);
  AssertFatal(eq_lpp_message(message, uper_decoded), "decoded LPP message differs\n");

  ++uper_decoded->transactionID->transactionNumber;
  AssertFatal(!eq_lpp_message(message, uper_decoded), "LPP comparison missed a changed transaction number\n");
  --uper_decoded->transactionID->transactionNumber;

  xer_fprint(stdout, &asn_DEF_LPP_LPP_Message, uper_decoded);

  free_lpp_message(uper_decoded);
  free_lpp_message(message);
  puts("LPP RequestCapabilities round trips succeeded");
  return EXIT_SUCCESS;
}
