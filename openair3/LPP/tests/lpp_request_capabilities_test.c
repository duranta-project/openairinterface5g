/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

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

  uint8_t buffer[1024] = {0};
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

static LPP_LPP_Message_t *create_request_capabilities_message(void)
{
  LPP_LPP_Message_t *message = checked_calloc(1, sizeof(*message));

  message->transactionID = checked_calloc(1, sizeof(*message->transactionID));
  message->transactionID->initiator = LPP_Initiator_locationServer;
  message->transactionID->transactionNumber = 1;
  message->endTransaction = false;

  message->lpp_MessageBody = checked_calloc(1, sizeof(*message->lpp_MessageBody));
  message->lpp_MessageBody->present = LPP_LPP_MessageBody_PR_c1;
  message->lpp_MessageBody->choice.c1 = checked_calloc(1, sizeof(*message->lpp_MessageBody->choice.c1));
  message->lpp_MessageBody->choice.c1->present = LPP_LPP_MessageBody__c1_PR_requestCapabilities;

  LPP_RequestCapabilities_t *request = checked_calloc(1, sizeof(*request));
  message->lpp_MessageBody->choice.c1->choice.requestCapabilities = request;

  request->criticalExtensions.present = LPP_RequestCapabilities__criticalExtensions_PR_c1;
  request->criticalExtensions.choice.c1 = checked_calloc(1, sizeof(*request->criticalExtensions.choice.c1));
  request->criticalExtensions.choice.c1->present =
      LPP_RequestCapabilities__criticalExtensions__c1_PR_requestCapabilities_r9;

  LPP_RequestCapabilities_r9_IEs_t *request_r9 = checked_calloc(1, sizeof(*request_r9));
  request->criticalExtensions.choice.c1->choice.requestCapabilities_r9 = request_r9;

  request_r9->a_gnss_RequestCapabilities = checked_calloc(1, sizeof(*request_r9->a_gnss_RequestCapabilities));
  request_r9->a_gnss_RequestCapabilities->gnss_SupportListReq = true;
  request_r9->a_gnss_RequestCapabilities->assistanceDataSupportListReq = true;
  request_r9->a_gnss_RequestCapabilities->locationVelocityTypesReq = true;

  return message;
}

static void assert_request_capabilities_message(const LPP_LPP_Message_t *message)
{
  if (message->transactionID == NULL)
    fail("decoded message has no transactionID");
  if (message->transactionID->initiator != LPP_Initiator_locationServer)
    fail("decoded initiator mismatch");
  if (message->transactionID->transactionNumber != 1)
    fail("decoded transactionNumber mismatch");
  if (message->endTransaction != false)
    fail("decoded endTransaction mismatch");

  if (message->lpp_MessageBody == NULL || message->lpp_MessageBody->present != LPP_LPP_MessageBody_PR_c1)
    fail("decoded message body is not c1");

  const struct LPP_LPP_MessageBody__c1 *body_c1 = message->lpp_MessageBody->choice.c1;
  if (body_c1 == NULL || body_c1->present != LPP_LPP_MessageBody__c1_PR_requestCapabilities)
    fail("decoded c1 body is not requestCapabilities");

  const LPP_RequestCapabilities_t *request = body_c1->choice.requestCapabilities;
  if (request == NULL || request->criticalExtensions.present != LPP_RequestCapabilities__criticalExtensions_PR_c1)
    fail("decoded requestCapabilities has no c1 criticalExtensions");

  const struct LPP_RequestCapabilities__criticalExtensions__c1 *request_c1 = request->criticalExtensions.choice.c1;
  if (request_c1 == NULL
      || request_c1->present != LPP_RequestCapabilities__criticalExtensions__c1_PR_requestCapabilities_r9)
    fail("decoded criticalExtensions c1 is not requestCapabilities-r9");

  const LPP_RequestCapabilities_r9_IEs_t *request_r9 = request_c1->choice.requestCapabilities_r9;
  if (request_r9 == NULL || request_r9->a_gnss_RequestCapabilities == NULL)
    fail("decoded requestCapabilities-r9 has no A-GNSS request capabilities");

  const LPP_A_GNSS_RequestCapabilities_t *agnss = request_r9->a_gnss_RequestCapabilities;
  if (!agnss->gnss_SupportListReq || !agnss->assistanceDataSupportListReq || !agnss->locationVelocityTypesReq)
    fail("decoded A-GNSS request capability flags mismatch");
}

int main(void)
{
  LPP_LPP_Message_t *message = create_request_capabilities_message();

  LPP_LPP_Message_t *aper_decoded = encode_decode_lpp_message(message, LPP_CODEC_APER);
  assert_request_capabilities_message(aper_decoded);
  ASN_STRUCT_FREE(asn_DEF_LPP_LPP_Message, aper_decoded);

  LPP_LPP_Message_t *uper_decoded = encode_decode_lpp_message(message, LPP_CODEC_UPER);
  assert_request_capabilities_message(uper_decoded);

  xer_fprint(stdout, &asn_DEF_LPP_LPP_Message, uper_decoded);

  ASN_STRUCT_FREE(asn_DEF_LPP_LPP_Message, uper_decoded);
  ASN_STRUCT_FREE(asn_DEF_LPP_LPP_Message, message);

  return EXIT_SUCCESS;
}
