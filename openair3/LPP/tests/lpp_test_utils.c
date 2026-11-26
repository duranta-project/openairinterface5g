/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "lpp_test_utils.h"
#include <string.h>
#include "aper_decoder.h"
#include "aper_encoder.h"
#include "constraints.h"
#include "per_decoder.h"
#include "per_encoder.h"

static void check_lpp_constraints(const LPP_LPP_Message_t *message)
{
  char errbuf[1024];
  size_t errlen = sizeof(errbuf);
  int ret = asn_check_constraints(&asn_DEF_LPP_LPP_Message, message, errbuf, &errlen);
  AssertFatal(ret == 0, "asn_check_constraints() failed: %s\n", errbuf);
}

LPP_LPP_Message_t *encode_decode_lpp_message(const LPP_LPP_Message_t *message, enum lpp_codec codec)
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

  AssertFatal(enc.encoded > 0, "%s encode failed\n", codec_name);

  LPP_LPP_Message_t *decoded = NULL;
  asn_codec_ctx_t ctx = {.max_stack_size = 100 * 1000};
  const size_t encoded_bytes = (enc.encoded + 7) / 8;
  asn_dec_rval_t dec = {0};

  if (codec == LPP_CODEC_APER)
    dec = aper_decode(&ctx, &asn_DEF_LPP_LPP_Message, (void **)&decoded, buffer, encoded_bytes, 0, 0);
  else
    dec = uper_decode_complete(&ctx, &asn_DEF_LPP_LPP_Message, (void **)&decoded, buffer, encoded_bytes);

  AssertFatal(dec.code == RC_OK, "%s decode failed with code %d\n", codec_name, dec.code);

  check_lpp_constraints(decoded);
  return decoded;
}

bool eq_lpp_message(const LPP_LPP_Message_t *a, const LPP_LPP_Message_t *b)
{
  void *a_buffer = NULL;
  void *b_buffer = NULL;
  ssize_t a_size = uper_encode_to_new_buffer(&asn_DEF_LPP_LPP_Message, NULL, a, &a_buffer);
  ssize_t b_size = uper_encode_to_new_buffer(&asn_DEF_LPP_LPP_Message, NULL, b, &b_buffer);
  bool equal = a_size > 0 && a_size == b_size && memcmp(a_buffer, b_buffer, a_size) == 0;
  free(a_buffer);
  free(b_buffer);
  return equal;
}

void free_lpp_message(LPP_LPP_Message_t *message)
{
  ASN_STRUCT_FREE(asn_DEF_LPP_LPP_Message, message);
}
