/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**
 * @file test_l1_kpm_enc.c
 * @brief Round-trip tests for the L1-KPM indication and RAN-function encoders.
 *
 * One slot snapshot is encoded and decoded back on every wire format compiled
 * in, and every field is compared against what went in. The encoders pick the
 * format from e3_get_encoding(), which is stubbed here so a single binary can
 * drive all of them.
 */

#include "l1_kpm_enc.h"

#include "common/utils/LOG/log.h"
#include "config/e3_config.h" /* E3_ENCODING_* */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "L1KPM-Indication.h"
#include "L1KPM-RanFunctionData.h"
#include "L1KPM-ShmRef.h"
#include "aper_decoder.h"
#ifdef E3_SM_HAVE_JSON
#include <json-c/json.h>
#endif
#ifdef E3_SM_HAVE_PROTOBUF
#include "e3sm_l1_kpm.pb-c.h"
#endif

/* The encoders read the active format from the agent; drive it from the test. */
static int g_encoding = E3_ENCODING_ASN1;
int e3_get_encoding(void)
{
  return g_encoding;
}

/* Required by the logging and config code we link against; the other
 * standalone tests do the same (see common/utils/tests/test_bits.c and
 * common/utils/time_manager/tests/test_auto.c). Defining them here rather than
 * linking minimal_lib keeps the link free of static-archive ordering. */
struct configmodule_interface_s *uniqCfg = NULL;

void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  abort();
}

/* ---- harness ---- */

static int g_failures = 0;

static void check(const char *name, int ok)
{
  if (ok) {
    printf("PASS [%s]\n", name);
  } else {
    fprintf(stderr, "FAIL [%s]\n", name);
    g_failures++;
  }
}

static void check_u64(const char *name, uint64_t got, uint64_t want)
{
  if (got == want) {
    printf("PASS [%s]\n", name);
  } else {
    fprintf(stderr, "FAIL [%s]: got %" PRIu64 ", want %" PRIu64 "\n", name, got, want);
    g_failures++;
  }
}

/* Distinctive values, so a field swapped for its neighbour is visible. */
static e3_ran_buffers_slot_info_t make_slot(void)
{
  e3_ran_buffers_slot_info_t s;
  memset(&s, 0, sizeof(s));
  s.fh_buffer_index = 1;
  s.fh_write_index = 37;
  s.timestamp_ns = 1723052400123456789ull;
  s.sfn = 613;
  s.slot = 8;
  s.cell_id = 42;
  s.n_rx_ant = 4;
  s.valid_symbol_mask = 0x3F00;
  s.valid = true;
  return s;
}

/* ---- indication round trips ---- */

static void test_asn1_indication(void)
{
  const e3_ran_buffers_slot_info_t in = make_slot();
  uint8_t buf[256];

  g_encoding = E3_ENCODING_ASN1;
  const int n = l1_kpm_enc_indication(&in, buf, sizeof(buf));
  check("asn1: encode returns bytes", n > 0);
  if (n <= 0)
    return;

  L1KPM_Indication_t *pdu = NULL;
  const asn_dec_rval_t rv = aper_decode(NULL, &asn_DEF_L1KPM_Indication, (void **)&pdu, buf, (size_t)n, 0, 0);
  check("asn1: decode ok", rv.code == RC_OK && pdu != NULL);
  if (rv.code != RC_OK || !pdu)
    return;

  check_u64("asn1: timestamp", (uint64_t)pdu->timestamp, in.timestamp_ns);
  check_u64("asn1: sfn", (uint64_t)pdu->sfn, in.sfn);
  check_u64("asn1: slot", (uint64_t)pdu->slot, in.slot);
  check("asn1: cellId present", pdu->cellId != NULL);
  check("asn1: nRxAnt present", pdu->nRxAnt != NULL);
  check("asn1: validSymbolMask present", pdu->validSymbolMask != NULL);
  if (pdu->cellId)
    check_u64("asn1: cellId", (uint64_t)*pdu->cellId, in.cell_id);
  if (pdu->nRxAnt)
    check_u64("asn1: nRxAnt", (uint64_t)*pdu->nRxAnt, in.n_rx_ant);
  if (pdu->validSymbolMask)
    check_u64("asn1: validSymbolMask", (uint64_t)*pdu->validSymbolMask, in.valid_symbol_mask);

  check("asn1: iqSamplesRef present", pdu->iqSamplesRef != NULL);
  if (pdu->iqSamplesRef) {
    check_u64("asn1: fhBufferIndex", (uint64_t)pdu->iqSamplesRef->fhBufferIndex, in.fh_buffer_index);
    check_u64("asn1: fhWriteIndex", (uint64_t)pdu->iqSamplesRef->fhWriteIndex, in.fh_write_index);
    const OCTET_STRING_t *nm = &pdu->iqSamplesRef->shmName;
    check("asn1: shmName", nm->size == (int)strlen(E3_RB_SHM_NAME) && memcmp(nm->buf, E3_RB_SHM_NAME, (size_t)nm->size) == 0);
  }

  ASN_STRUCT_FREE(asn_DEF_L1KPM_Indication, pdu);
}

#ifdef E3_SM_HAVE_JSON
static uint64_t json_u64(struct json_object *o, const char *key, int *found)
{
  struct json_object *v = NULL;
  *found = json_object_object_get_ex(o, key, &v);
  return *found ? json_object_get_uint64(v) : 0;
}

static void test_json_indication(void)
{
  const e3_ran_buffers_slot_info_t in = make_slot();
  uint8_t buf[512];

  g_encoding = E3_ENCODING_JSON;
  const int n = l1_kpm_enc_indication(&in, buf, sizeof(buf));
  check("json: encode returns bytes", n > 0);
  if (n <= 0)
    return;

  /* The encoder writes exactly n bytes and does not NUL-terminate. */
  char *text = malloc((size_t)n + 1);
  memcpy(text, buf, (size_t)n);
  text[n] = '\0';

  struct json_object *root = json_tokener_parse(text);
  check("json: parses", root != NULL);
  if (root) {
    int found = 0;
    check_u64("json: timestamp", json_u64(root, "timestamp", &found), in.timestamp_ns);
    check_u64("json: sfn", json_u64(root, "sfn", &found), in.sfn);
    check_u64("json: slot", json_u64(root, "slot", &found), in.slot);
    check_u64("json: cell_id", json_u64(root, "cell_id", &found), in.cell_id);
    check_u64("json: n_rx_ant", json_u64(root, "n_rx_ant", &found), in.n_rx_ant);
    check_u64("json: valid_symbol_mask", json_u64(root, "valid_symbol_mask", &found), in.valid_symbol_mask);

    struct json_object *iq = NULL;
    check("json: iq_samples present", json_object_object_get_ex(root, "iq_samples", &iq) && iq != NULL);
    if (iq) {
      check_u64("json: fh_buffer_index", json_u64(iq, "fh_buffer_index", &found), in.fh_buffer_index);
      check_u64("json: fh_write_index", json_u64(iq, "fh_write_index", &found), in.fh_write_index);
      struct json_object *nm = NULL;
      check(
          "json: shm_name unescaped",
          json_object_object_get_ex(iq, "shm_name", &nm) && nm != NULL && strcmp(json_object_get_string(nm), E3_RB_SHM_NAME) == 0);
    }
    json_object_put(root);
  }
  free(text);
}
#endif

#ifdef E3_SM_HAVE_PROTOBUF
static void test_protobuf_indication(void)
{
  const e3_ran_buffers_slot_info_t in = make_slot();
  uint8_t buf[512];

  g_encoding = E3_ENCODING_PROTOBUF;
  const int n = l1_kpm_enc_indication(&in, buf, sizeof(buf));
  check("protobuf: encode returns bytes", n > 0);
  if (n <= 0)
    return;

  E3sm__L1kpm__V1__L1KPMIndication *pdu = e3sm__l1kpm__v1__l1_kpmindication__unpack(NULL, (size_t)n, buf);
  check("protobuf: unpack ok", pdu != NULL);
  if (!pdu)
    return;

  check("protobuf: all fields present",
        pdu->has_timestamp && pdu->has_sfn && pdu->has_slot && pdu->has_cell_id && pdu->has_n_rx_ant && pdu->has_valid_symbol_mask);
  check_u64("protobuf: timestamp", (uint64_t)pdu->timestamp, in.timestamp_ns);
  check_u64("protobuf: sfn", pdu->sfn, in.sfn);
  check_u64("protobuf: slot", pdu->slot, in.slot);
  check_u64("protobuf: cell_id", pdu->cell_id, in.cell_id);
  check_u64("protobuf: n_rx_ant", pdu->n_rx_ant, in.n_rx_ant);
  check_u64("protobuf: valid_symbol_mask", pdu->valid_symbol_mask, in.valid_symbol_mask);
  check("protobuf: iq_samples_ref present", pdu->iq_samples_ref != NULL);
  if (pdu->iq_samples_ref) {
    check_u64("protobuf: fh_buffer_index", pdu->iq_samples_ref->fh_buffer_index, in.fh_buffer_index);
    check_u64("protobuf: fh_write_index", pdu->iq_samples_ref->fh_write_index, in.fh_write_index);
    check("protobuf: shm_name",
          pdu->iq_samples_ref->shm_name.len == strlen(E3_RB_SHM_NAME)
              && memcmp(pdu->iq_samples_ref->shm_name.data, E3_RB_SHM_NAME, pdu->iq_samples_ref->shm_name.len) == 0);
  }

  e3sm__l1kpm__v1__l1_kpmindication__free_unpacked(pdu, NULL);
}
#endif

/* ---- RAN-function data round trip ---- */

static void test_asn1_ran_function_data(void)
{
  g_encoding = E3_ENCODING_ASN1;
  uint8_t *enc = NULL;
  size_t enc_len = 0;
  const int rc = l1_kpm_enc_ran_function_data("L1-KPM", 1, "L1-KPM SM: post-FFT IQ telemetry.", &enc, &enc_len);
  check("rfd asn1: encode ok", rc == 0);
  /* E3AP requires a non-empty ranFunctionData per advertised RAN function. */
  check("rfd asn1: non-empty", enc != NULL && enc_len > 0);
  if (rc != 0 || !enc) {
    free(enc);
    return;
  }

  L1KPM_RanFunctionData_t *pdu = NULL;
  const asn_dec_rval_t rv = aper_decode(NULL, &asn_DEF_L1KPM_RanFunctionData, (void **)&pdu, enc, enc_len, 0, 0);
  check("rfd asn1: decode ok", rv.code == RC_OK && pdu != NULL);
  if (rv.code == RC_OK && pdu) {
    check("rfd asn1: name", pdu->name.size == 6 && memcmp(pdu->name.buf, "L1-KPM", 6) == 0);
    check_u64("rfd asn1: version", (uint64_t)pdu->version, 1);
    check("rfd asn1: description non-empty", pdu->description.size > 0);
    ASN_STRUCT_FREE(asn_DEF_L1KPM_RanFunctionData, pdu);
  }
  free(enc);
}

/* ---- failure paths ---- */

static void test_rejects_bad_input(void)
{
  const e3_ran_buffers_slot_info_t in = make_slot();
  uint8_t buf[256];

  g_encoding = E3_ENCODING_ASN1;
  check("reject: NULL slot", l1_kpm_enc_indication(NULL, buf, sizeof(buf)) == -1);
  check("reject: NULL buffer", l1_kpm_enc_indication(&in, NULL, sizeof(buf)) == -1);
  check("reject: zero-size buffer", l1_kpm_enc_indication(&in, buf, 0) == -1);
  /* Too small to hold the PDU: must fail rather than write past the end. */
  check("reject: undersized buffer", l1_kpm_enc_indication(&in, buf, 2) == -1);

  uint8_t *enc = NULL;
  size_t enc_len = 0;
  check("reject: rfd NULL name", l1_kpm_enc_ran_function_data(NULL, 1, "d", &enc, &enc_len) == -1);
  check("reject: rfd NULL out", l1_kpm_enc_ran_function_data("n", 1, "d", NULL, &enc_len) == -1);
}

int main(void)
{
  /* The encoders log on their failure paths; LOG_E dereferences state that
   * only logInit() sets up. */
  logInit();

  test_asn1_indication();
#ifdef E3_SM_HAVE_JSON
  test_json_indication();
#endif
#ifdef E3_SM_HAVE_PROTOBUF
  test_protobuf_indication();
#endif
  test_asn1_ran_function_data();
  test_rejects_bad_input();

  if (g_failures) {
    fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
  }
  printf("all checks passed\n");
  return 0;
}
