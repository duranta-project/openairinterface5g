/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "spectrum_dec.h"
#include "../../e3_log.h"

#include <stdlib.h>
#include <string.h>

#include "common/utils/LOG/log.h"

/* Control decode in the wire format selected at runtime from the config file
 * (the same selection spectrum_enc.c uses, via e3_get_encoding()). The agent
 * serves a single encoding per run; a control arriving in the other format is
 * a deployment mismatch and fails to decode. */
#include "../../e3_agent.h" /* e3_get_encoding() */
#include "Spectrum-DAppControlData.h"
#include "Spectrum-DAppControlPayload.h"
#include "Spectrum-PRBBlacklistControl.h"
#include "Spectrum-SensingPolicyControl.h"
#include <json-c/json.h>
#ifdef E3_SM_HAVE_PROTOBUF
#include "e3sm_spectrum.pb-c.h"
#endif

/* 14-bit symbol bitmap upper bound (Spectrum-SensingPolicyControl INTEGER
 * (0..16383)) and per-slot vector upper bound (maxSlotsFrame=160, numerology 4 worst
 * case; numerology 1 uses 20). */
#define SPECTRUM_SENSING_MASK_MAX 0x3FFFu
#define SPECTRUM_SENSING_MAX_SLOTS 160u
/* PRB-block list bounds (Spectrum-PRBBlacklistControl): blacklistedPRBs
 * SEQUENCE (SIZE(0..273)) OF INTEGER (0..272) — the absolute PRB index space;
 * the dispatcher applies the MAX_BWP_SIZE clamp separately when stamping. */
#define SPECTRUM_MAX_PRB_COUNT 273u
#define SPECTRUM_MAX_PRB_INDEX 272u

/* Convert a decoded ASN.1 PRBBlacklistControl into a newly allocated
 * spectrum_prb_control_t (blocked-PRB list + sampling threshold + validity),
 * validating list count and per-PRB range. Returns NULL on alloc failure or
 * out-of-range value. asn_control stays owned by the caller's envelope
 * (ASN_STRUCT_FREE on the envelope covers it, on every path). */
static spectrum_prb_control_t *spectrum_extract_prb_control(const Spectrum_PRBBlacklistControl_t *asn_control)
{
  spectrum_prb_control_t *prb_control = calloc(1, sizeof(*prb_control));
  if (!prb_control) {
    SPEC_LOG_E("Failed to malloc structure\n");
    return NULL;
  }

  const int n = asn_control->blacklistedPRBs.list.count;
  if (n < 0 || (uint32_t)n > SPECTRUM_MAX_PRB_COUNT) {
    SPEC_LOG_E("Invalid PRB list count: %d\n", n);
    free(prb_control);
    return NULL;
  }

  prb_control->prb_count = (uint32_t)n;

  if (prb_control->prb_count > 0) {
    prb_control->blacklisted_prbs = malloc((size_t)prb_control->prb_count * sizeof(uint16_t));
    if (!prb_control->blacklisted_prbs) {
      SPEC_LOG_E("Failed to malloc PRB list\n");
      free(prb_control);
      return NULL;
    }

    for (int i = 0; i < n; ++i) {
      if (asn_control->blacklistedPRBs.list.array[i] == NULL) {
        prb_control->blacklisted_prbs[i] = 0;
        continue;
      }

      long v = *(asn_control->blacklistedPRBs.list.array[i]);

      if (v < 0 || (unsigned long)v > SPECTRUM_MAX_PRB_INDEX) {
        SPEC_LOG_E("Invalid PRB value at index %d: %ld\n", i, v);
        free(prb_control->blacklisted_prbs);
        free(prb_control);
        return NULL;
      }

      prb_control->blacklisted_prbs[i] = (uint16_t)v;
    }
  }

  // Extract optional sampling threshold
  if (asn_control->samplingThreshold) {
    prb_control->sampling_threshold = *(asn_control->samplingThreshold);
  }
  // Extract optional validity period
  if (asn_control->validityPeriod) {
    prb_control->validity_period = *(asn_control->validityPeriod);
  }

  SPEC_LOG_D("Decoded PRB-block control: %u PRBs, sampling threshold %u, validity %u seconds\n",
             prb_control->prb_count,
             prb_control->sampling_threshold,
             prb_control->validity_period);

  return prb_control;
}

/* Convert a decoded ASN.1 SensingPolicyControl into a newly allocated
 * spectrum_sensing_policy_t (per-slot mask vector + deactivate + validity),
 * validating slot count and per-slot mask range. NULL on alloc/range failure.
 * asn_policy stays owned by the caller's envelope. */
static spectrum_sensing_policy_t *spectrum_extract_sensing_policy(const Spectrum_SensingPolicyControl_t *asn_policy)
{
  const int n = asn_policy->maskPerSlot.list.count;
  if (n < 1 || (uint32_t)n > SPECTRUM_SENSING_MAX_SLOTS) {
    SPEC_LOG_E("Invalid sensing-policy mask length: %d (max %u)\n", n, SPECTRUM_SENSING_MAX_SLOTS);
    return NULL;
  }

  spectrum_sensing_policy_t *policy = calloc(1, sizeof(*policy));
  if (!policy) {
    SPEC_LOG_E("Failed to malloc sensing_policy struct\n");
    return NULL;
  }
  policy->mask_per_slot = malloc((size_t)n * sizeof(uint16_t));
  if (!policy->mask_per_slot) {
    SPEC_LOG_E("Failed to malloc sensing-policy mask\n");
    free(policy);
    return NULL;
  }
  policy->n_slots = (uint32_t)n;

  for (int i = 0; i < n; ++i) {
    long v = 0;
    if (asn_policy->maskPerSlot.list.array[i] != NULL) {
      v = *(asn_policy->maskPerSlot.list.array[i]);
    }
    if (v < 0 || (unsigned long)v > SPECTRUM_SENSING_MASK_MAX) {
      SPEC_LOG_E("Invalid sensing-policy mask[%d]=0x%lx (max 0x%x)\n", i, v, SPECTRUM_SENSING_MASK_MAX);
      free(policy->mask_per_slot);
      free(policy);
      return NULL;
    }
    policy->mask_per_slot[i] = (uint16_t)v;
  }

  policy->deactivate = (asn_policy->deactivate != 0);
  if (asn_policy->validityPeriod) {
    policy->validity_period = (uint32_t) * (asn_policy->validityPeriod);
  }

  SPEC_LOG_D("Decoded sensingPolicy: n_slots=%u deactivate=%d validity=%u\n",
             policy->n_slots,
             (int)policy->deactivate,
             policy->validity_period);
  return policy;
}

/* Parse a JSON byte buffer (not necessarily NUL-terminated) into a
 * json_object*. Caller frees with json_object_put. */
static json_object *spectrum_json_parse(const uint8_t *data, size_t len)
{
  if (!data || len == 0) {
    return NULL;
  }
  json_tokener *tok = json_tokener_new();
  if (!tok) {
    return NULL;
  }
  json_object *root = json_tokener_parse_ex(tok, (const char *)data, (int)len);
  enum json_tokener_error err = json_tokener_get_error(tok);
  json_tokener_free(tok);
  if (!root || err != json_tokener_success) {
    SPEC_LOG_E("JSON parse failed: %s\n", json_tokener_error_desc(err));
    if (root) {
      json_object_put(root);
    }
    return NULL;
  }
  return root;
}

/* Validate the Spectrum-DAppControlData envelope and return the inner payload
 * object matching `expected_payload_key`. Returns a borrowed reference (no
 * extra refcount); the caller must keep `root` alive. */
static json_object *spectrum_json_envelope_extract(json_object *root, const char *expected_type, const char *expected_payload_key)
{
  /* "controlType" is a codec-level convenience key, not a schema field: the
   * payload key below is the real discriminator. When present it must be a
   * string matching the expected variant; when absent it is simply omitted. */
  json_object *jtype = NULL;
  if (json_object_object_get_ex(root, "controlType", &jtype)) {
    const char *type_str = json_object_is_type(jtype, json_type_string) ? json_object_get_string(jtype) : NULL;
    if (!type_str || strcmp(type_str, expected_type) != 0) {
      SPEC_LOG_E("JSON envelope controlType=%s, expected %s\n", type_str ? type_str : "(non-string)", expected_type);
      return NULL;
    }
  }
  json_object *jpayload = NULL;
  if (!json_object_object_get_ex(root, "controlPayload", &jpayload) || !json_object_is_type(jpayload, json_type_object)) {
    SPEC_LOG_E("JSON envelope missing controlPayload object\n");
    return NULL;
  }
  json_object *jinner = NULL;
  if (!json_object_object_get_ex(jpayload, expected_payload_key, &jinner) || !json_object_is_type(jinner, json_type_object)) {
    SPEC_LOG_E("JSON envelope missing %s object\n", expected_payload_key);
    return NULL;
  }
  return jinner;
}

/* Decode the required JSON integer array `inner[key]` into a freshly allocated
 * uint16_t vector (the sensingPolicy maskPerSlot). Returns the element count
 * (>=0) and stores the heap pointer in *out (NULL for a zero-length array);
 * returns -1 on failure with *out left NULL. The count is clamped to max_cnt
 * and every element range-checked against [0, max_val] before the cast. */
static int spectrum_json_u16_array(json_object *inner, const char *key, size_t max_cnt, long max_val, uint16_t **out)
{
  *out = NULL;

  json_object *jarr = NULL;
  if (!json_object_object_get_ex(inner, key, &jarr) || !json_object_is_type(jarr, json_type_array)) {
    SPEC_LOG_E("JSON missing %s array\n", key);
    return -1;
  }
  size_t n = json_object_array_length(jarr);
  if (n > max_cnt) {
    SPEC_LOG_E("JSON %s length %zu exceeds %zu\n", key, n, max_cnt);
    return -1;
  }
  if (n == 0) {
    return 0;
  }

  uint16_t *vec = malloc(n * sizeof(uint16_t));
  if (!vec) {
    return -1;
  }
  for (size_t i = 0; i < n; ++i) {
    int64_t v = json_object_get_int64(json_object_array_get_idx(jarr, i));
    if (v < 0 || v > (int64_t)max_val) {
      SPEC_LOG_E("JSON %s[%zu]=%lld out of range\n", key, i, (long long)v);
      free(vec);
      return -1;
    }
    vec[i] = (uint16_t)v;
  }
  *out = vec;
  return (int)n;
}

/* APER-decode the DAppControlData envelope, verify it carries a
 * prbBlacklistControl variant, and extract it. Returns NULL on decode failure
 * or unexpected variant; the ASN.1 envelope is always freed before return. */
static spectrum_prb_control_t *spectrum_decode_prb_control_asn1(uint8_t *encoded_data, size_t encoded_size)
{
  Spectrum_DAppControlData_t *envelope = NULL;
  asn_dec_rval_t dec_ret = aper_decode(0, &asn_DEF_Spectrum_DAppControlData, (void **)&envelope, encoded_data, encoded_size, 0, 0);

  if (dec_ret.code != RC_OK || !envelope) {
    SPEC_LOG_E("Failed to decode dApp control envelope\n");
    if (envelope) {
      ASN_STRUCT_FREE(asn_DEF_Spectrum_DAppControlData, envelope);
    }
    return NULL;
  }

  if (envelope->controlPayload.present != Spectrum_DAppControlPayload_PR_prbBlacklistControl) {
    SPEC_LOG_E("Unexpected dApp control variant (payload_present=%d)\n", (int)envelope->controlPayload.present);
    ASN_STRUCT_FREE(asn_DEF_Spectrum_DAppControlData, envelope);
    return NULL;
  }

  Spectrum_PRBBlacklistControl_t *asn_control = envelope->controlPayload.choice.prbBlacklistControl;
  if (!asn_control) {
    SPEC_LOG_E("dApp control envelope has null prbBlacklistControl payload\n");
    ASN_STRUCT_FREE(asn_DEF_Spectrum_DAppControlData, envelope);
    return NULL;
  }

  spectrum_prb_control_t *prb_control = spectrum_extract_prb_control(asn_control);
  ASN_STRUCT_FREE(asn_DEF_Spectrum_DAppControlData, envelope);
  return prb_control;
}

/* Parse the JSON DAppControlData envelope, extract its prbBlacklistControl
 * object, and convert it into a newly allocated spectrum_prb_control_t. NULL on
 * parse failure, missing/mistyped blacklistedPRBs, or out-of-range PRB value. */
static spectrum_prb_control_t *spectrum_decode_prb_control_json(uint8_t *encoded_data, size_t encoded_size)
{
  /* JSON envelope shape ("controlType" is optional, codec-added):
   *   { "controlType": "prbBlacklist",
   *     "controlPayload": { "prbBlacklistControl": { ... } } }
   */
  json_object *root = spectrum_json_parse(encoded_data, encoded_size);
  if (!root) {
    return NULL;
  }

  json_object *inner = spectrum_json_envelope_extract(root, "prbBlacklist", "prbBlacklistControl");
  if (!inner) {
    json_object_put(root);
    return NULL;
  }

  spectrum_prb_control_t *prb_control = calloc(1, sizeof(*prb_control));
  if (!prb_control) {
    json_object_put(root);
    return NULL;
  }

  /* blacklistedPRBs is REQUIRED: an explicit empty array is the documented
   * clear-all, but a missing or mistyped key is a malformed control and is
   * NACKed (decode failure) instead of silently clearing the block mask. */
  int n = spectrum_json_u16_array(inner,
                                  "blacklistedPRBs",
                                  SPECTRUM_MAX_PRB_COUNT,
                                  SPECTRUM_MAX_PRB_INDEX,
                                  &prb_control->blacklisted_prbs);
  if (n < 0) {
    free(prb_control);
    json_object_put(root);
    return NULL;
  }
  prb_control->prb_count = (uint32_t)n;

  json_object *jth = NULL;
  if (json_object_object_get_ex(inner, "samplingThreshold", &jth)) {
    prb_control->sampling_threshold = (uint32_t)json_object_get_int(jth);
  }
  json_object *jvp = NULL;
  if (json_object_object_get_ex(inner, "validityPeriod", &jvp)) {
    prb_control->validity_period = (uint32_t)json_object_get_int(jvp);
  }

  SPEC_LOG_D("Decoded PRB-block (JSON): %u PRBs, sampling threshold %u, validity %u s\n",
             prb_control->prb_count,
             prb_control->sampling_threshold,
             prb_control->validity_period);

  json_object_put(root);
  return prb_control;
}

#ifdef E3_SM_HAVE_PROTOBUF
/* protobuf-c decode of the DAppControlData envelope's prbBlacklistControl. */
static spectrum_prb_control_t *spectrum_decode_prb_control_protobuf(uint8_t *encoded_data, size_t encoded_size)
{
  E3sm__Spectrum__V1__SpectrumDAppControlData *env =
      e3sm__spectrum__v1__spectrum_dapp_control_data__unpack(NULL, encoded_size, encoded_data);
  if (!env) {
    SPEC_LOG_E("Failed to decode dApp control envelope (protobuf)\n");
    return NULL;
  }
  if (env->control_payload_case != E3SM__SPECTRUM__V1__SPECTRUM_DAPP_CONTROL_DATA__CONTROL_PAYLOAD_PRB_BLACKLIST_CONTROL
      || !env->prb_blacklist_control) {
    SPEC_LOG_E("Unexpected dApp control variant (protobuf, case=%d)\n", (int)env->control_payload_case);
    e3sm__spectrum__v1__spectrum_dapp_control_data__free_unpacked(env, NULL);
    return NULL;
  }
  const E3sm__Spectrum__V1__SpectrumPRBBlacklistControl *pc = env->prb_blacklist_control;

  if (pc->n_blacklisted_prbs > SPECTRUM_MAX_PRB_COUNT) {
    SPEC_LOG_E("Invalid PRB list count: %zu\n", pc->n_blacklisted_prbs);
    e3sm__spectrum__v1__spectrum_dapp_control_data__free_unpacked(env, NULL);
    return NULL;
  }

  spectrum_prb_control_t *prb_control = calloc(1, sizeof(*prb_control));
  if (!prb_control) {
    e3sm__spectrum__v1__spectrum_dapp_control_data__free_unpacked(env, NULL);
    return NULL;
  }
  prb_control->prb_count = (uint32_t)pc->n_blacklisted_prbs;
  if (prb_control->prb_count > 0) {
    prb_control->blacklisted_prbs = malloc((size_t)prb_control->prb_count * sizeof(uint16_t));
    if (!prb_control->blacklisted_prbs) {
      free(prb_control);
      e3sm__spectrum__v1__spectrum_dapp_control_data__free_unpacked(env, NULL);
      return NULL;
    }
    for (size_t i = 0; i < pc->n_blacklisted_prbs; ++i) {
      uint32_t v = pc->blacklisted_prbs[i];
      if (v > SPECTRUM_MAX_PRB_INDEX) {
        SPEC_LOG_E("Invalid PRB value at index %zu: %u\n", i, v);
        free(prb_control->blacklisted_prbs);
        free(prb_control);
        e3sm__spectrum__v1__spectrum_dapp_control_data__free_unpacked(env, NULL);
        return NULL;
      }
      prb_control->blacklisted_prbs[i] = (uint16_t)v;
    }
  }
  if (pc->has_sampling_threshold) {
    prb_control->sampling_threshold = pc->sampling_threshold;
  }
  if (pc->has_validity_period) {
    prb_control->validity_period = pc->validity_period;
  }

  SPEC_LOG_D("Decoded PRB-block (protobuf): %u PRBs, sampling threshold %u, validity %u s\n",
             prb_control->prb_count,
             prb_control->sampling_threshold,
             prb_control->validity_period);

  e3sm__spectrum__v1__spectrum_dapp_control_data__free_unpacked(env, NULL);
  return prb_control;
}
#endif

spectrum_prb_control_t *spectrum_decode_prb_control(uint8_t *encoded_data, size_t encoded_size)
{
  if (!encoded_data || encoded_size == 0) {
    SPEC_LOG_E("tried to decode an empty buffer\n");
    return NULL;
  }
  if (e3_get_encoding() == E3_ENCODING_ASN1) {
    return spectrum_decode_prb_control_asn1(encoded_data, encoded_size);
  }
  if (e3_get_encoding() == E3_ENCODING_PROTOBUF) {
#ifdef E3_SM_HAVE_PROTOBUF
    return spectrum_decode_prb_control_protobuf(encoded_data, encoded_size);
#else
    SPEC_LOG_E("protobuf encoding not compiled in\n");
    return NULL;
#endif
  }
  return spectrum_decode_prb_control_json(encoded_data, encoded_size);
}

/* APER-decode the DAppControlData envelope, verify it carries a sensingPolicy
 * variant, and extract its SensingPolicyControl. Returns NULL on decode failure
 * or unexpected variant; the ASN.1 envelope is always freed before return. */
static spectrum_sensing_policy_t *spectrum_decode_sensing_policy_asn1(uint8_t *encoded_data, size_t encoded_size)
{
  Spectrum_DAppControlData_t *envelope = NULL;
  asn_dec_rval_t dec_ret = aper_decode(0, &asn_DEF_Spectrum_DAppControlData, (void **)&envelope, encoded_data, encoded_size, 0, 0);
  if (dec_ret.code != RC_OK || !envelope) {
    SPEC_LOG_E("Failed to decode sensing-policy envelope\n");
    if (envelope) {
      ASN_STRUCT_FREE(asn_DEF_Spectrum_DAppControlData, envelope);
    }
    return NULL;
  }
  if (envelope->controlPayload.present != Spectrum_DAppControlPayload_PR_sensingPolicyControl) {
    SPEC_LOG_E("Unexpected envelope variant for sensingPolicy (pr=%d)\n", (int)envelope->controlPayload.present);
    ASN_STRUCT_FREE(asn_DEF_Spectrum_DAppControlData, envelope);
    return NULL;
  }
  Spectrum_SensingPolicyControl_t *asn_policy = envelope->controlPayload.choice.sensingPolicyControl;
  if (!asn_policy) {
    SPEC_LOG_E("Envelope has null sensingPolicyControl payload\n");
    ASN_STRUCT_FREE(asn_DEF_Spectrum_DAppControlData, envelope);
    return NULL;
  }
  spectrum_sensing_policy_t *policy = spectrum_extract_sensing_policy(asn_policy);
  ASN_STRUCT_FREE(asn_DEF_Spectrum_DAppControlData, envelope);
  return policy;
}

/* Parse the JSON DAppControlData envelope, extract its sensingPolicyControl
 * object, and convert it into a newly allocated spectrum_sensing_policy_t.
 * Returns NULL on parse failure, missing/mistyped maskPerSlot, or out-of-range mask value. */
static spectrum_sensing_policy_t *spectrum_decode_sensing_policy_json(uint8_t *encoded_data, size_t encoded_size)
{
  /* JSON envelope shape ("controlType" is optional, codec-added):
   *   { "controlType": "sensingPolicy",
   *     "controlPayload": { "sensingPolicyControl": {
   *         "maskPerSlot": [..], "deactivate": false, "validityPeriod": .. } } }
   */
  json_object *root = spectrum_json_parse(encoded_data, encoded_size);
  if (!root) {
    return NULL;
  }
  json_object *inner = spectrum_json_envelope_extract(root, "sensingPolicy", "sensingPolicyControl");
  if (!inner) {
    json_object_put(root);
    return NULL;
  }

  /* maskPerSlot must carry at least one slot; an empty vector is a clean NACK
   * (deactivate=false with no mask is meaningless). */
  uint16_t *mask = NULL;
  int n = spectrum_json_u16_array(inner, "maskPerSlot", SPECTRUM_SENSING_MAX_SLOTS, SPECTRUM_SENSING_MASK_MAX, &mask);
  if (n <= 0) {
    if (n == 0) {
      SPEC_LOG_E("sensingPolicy JSON maskPerSlot empty (need [1, %u])\n", SPECTRUM_SENSING_MAX_SLOTS);
    }
    json_object_put(root);
    return NULL;
  }

  spectrum_sensing_policy_t *policy = calloc(1, sizeof(*policy));
  if (!policy) {
    free(mask);
    json_object_put(root);
    return NULL;
  }
  policy->mask_per_slot = mask;
  policy->n_slots = (uint32_t)n;

  json_object *jdeact = NULL;
  if (json_object_object_get_ex(inner, "deactivate", &jdeact)) {
    policy->deactivate = json_object_get_boolean(jdeact) ? true : false;
  }

  json_object *jvp = NULL;
  if (json_object_object_get_ex(inner, "validityPeriod", &jvp)) {
    policy->validity_period = (uint32_t)json_object_get_int(jvp);
  }

  SPEC_LOG_D("Decoded sensingPolicy (JSON): n_slots=%u deactivate=%d validity=%u\n",
             policy->n_slots,
             (int)policy->deactivate,
             policy->validity_period);

  json_object_put(root);
  return policy;
}

#ifdef E3_SM_HAVE_PROTOBUF
/* protobuf-c decode of the DAppControlData envelope's sensingPolicyControl. */
static spectrum_sensing_policy_t *spectrum_decode_sensing_policy_protobuf(uint8_t *encoded_data, size_t encoded_size)
{
  E3sm__Spectrum__V1__SpectrumDAppControlData *env =
      e3sm__spectrum__v1__spectrum_dapp_control_data__unpack(NULL, encoded_size, encoded_data);
  if (!env) {
    SPEC_LOG_E("Failed to decode sensing-policy envelope (protobuf)\n");
    return NULL;
  }
  if (env->control_payload_case != E3SM__SPECTRUM__V1__SPECTRUM_DAPP_CONTROL_DATA__CONTROL_PAYLOAD_SENSING_POLICY_CONTROL
      || !env->sensing_policy_control) {
    SPEC_LOG_E("Unexpected envelope variant for sensingPolicy (protobuf, case=%d)\n", (int)env->control_payload_case);
    e3sm__spectrum__v1__spectrum_dapp_control_data__free_unpacked(env, NULL);
    return NULL;
  }
  const E3sm__Spectrum__V1__SpectrumSensingPolicyControl *sp = env->sensing_policy_control;
  if (sp->n_mask_per_slot < 1 || sp->n_mask_per_slot > SPECTRUM_SENSING_MAX_SLOTS) {
    SPEC_LOG_E("Invalid sensing-policy mask length: %zu (max %u)\n", sp->n_mask_per_slot, SPECTRUM_SENSING_MAX_SLOTS);
    e3sm__spectrum__v1__spectrum_dapp_control_data__free_unpacked(env, NULL);
    return NULL;
  }

  spectrum_sensing_policy_t *policy = calloc(1, sizeof(*policy));
  if (!policy) {
    e3sm__spectrum__v1__spectrum_dapp_control_data__free_unpacked(env, NULL);
    return NULL;
  }
  policy->mask_per_slot = malloc(sp->n_mask_per_slot * sizeof(uint16_t));
  if (!policy->mask_per_slot) {
    free(policy);
    e3sm__spectrum__v1__spectrum_dapp_control_data__free_unpacked(env, NULL);
    return NULL;
  }
  policy->n_slots = (uint32_t)sp->n_mask_per_slot;
  for (size_t i = 0; i < sp->n_mask_per_slot; ++i) {
    uint32_t v = sp->mask_per_slot[i];
    if (v > SPECTRUM_SENSING_MASK_MAX) {
      SPEC_LOG_E("Invalid sensing-policy mask[%zu]=0x%x (max 0x%x)\n", i, v, SPECTRUM_SENSING_MASK_MAX);
      free(policy->mask_per_slot);
      free(policy);
      e3sm__spectrum__v1__spectrum_dapp_control_data__free_unpacked(env, NULL);
      return NULL;
    }
    policy->mask_per_slot[i] = (uint16_t)v;
  }
  if (sp->has_deactivate) {
    policy->deactivate = sp->deactivate ? true : false;
  }
  if (sp->has_validity_period) {
    policy->validity_period = sp->validity_period;
  }

  SPEC_LOG_D("Decoded sensingPolicy (protobuf): n_slots=%u deactivate=%d validity=%u\n",
             policy->n_slots,
             (int)policy->deactivate,
             policy->validity_period);

  e3sm__spectrum__v1__spectrum_dapp_control_data__free_unpacked(env, NULL);
  return policy;
}
#endif

spectrum_sensing_policy_t *spectrum_decode_sensing_policy(uint8_t *encoded_data, size_t encoded_size)
{
  if (!encoded_data || encoded_size == 0) {
    SPEC_LOG_E("tried to decode an empty sensing-policy buffer\n");
    return NULL;
  }
  if (e3_get_encoding() == E3_ENCODING_ASN1) {
    return spectrum_decode_sensing_policy_asn1(encoded_data, encoded_size);
  }
  if (e3_get_encoding() == E3_ENCODING_PROTOBUF) {
#ifdef E3_SM_HAVE_PROTOBUF
    return spectrum_decode_sensing_policy_protobuf(encoded_data, encoded_size);
#else
    SPEC_LOG_E("protobuf encoding not compiled in\n");
    return NULL;
#endif
  }
  return spectrum_decode_sensing_policy_json(encoded_data, encoded_size);
}

void spectrum_free_decoded_control(spectrum_prb_control_t *prb_control)
{
  if (!prb_control) {
    return;
  }
  // Free the dynamically allocated blacklisted_prbs array
  free(prb_control->blacklisted_prbs);
  free(prb_control);
}

void spectrum_free_sensing_policy(spectrum_sensing_policy_t *policy)
{
  if (!policy) {
    return;
  }
  free(policy->mask_per_slot);
  free(policy);
}
