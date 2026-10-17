/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
/* Invokes the actual production decoder. */
#include "ntn_assistance.h"
#include <assert.h>
#include <stdbool.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char update[] =
    "{\"version\":1,\"type\":\"update\",\"request\":\"17\","
    "\"cell\":{\"plmn\":\"00101\",\"nci\":\"68719476735\"},"
    "\"session\":\"3\",\"sequence\":\"9007199254740993\",\"subject\":{\"kind\":\"serving\"},"
    "\"epoch\":{\"kind\":\"cell\",\"generation\":\"9\",\"subframe\":\"10241\"},"
    "\"ephemeris\":{\"kind\":\"ecef\",\"position_m\":[1300000,-2600000,3900000],\"velocity_mps\":[60,-120,180]},"
    "\"ta\":{\"common_us\":4.072,\"drift_us_per_s\":-0.2,\"drift_variant_us_per_s2\":0.0002},"
    "\"ul_sync_validity_s\":120}";

static ntn_assistance_request_t parse(const char *text)
{
  ntn_assistance_request_t out;
  const char *error = ntn_assistance_parse(text, strlen(text), &out);
  if (error)
    fprintf(stderr, "Unexpected rejection: %s\n%s\n", error, text);
  assert(!error);
  return out;
}

static void rejected(const void *data, size_t length)
{
  ntn_assistance_request_t out, original;
  memset(&out, 0xa5, sizeof(out));
  memcpy(&original, &out, sizeof(original));
  assert(ntn_assistance_parse(data, length, &out));
  assert(!memcmp(&out, &original, sizeof(out)));
}

static void check_json(json_t *root, bool accepted)
{
  char *text = json_dumps(root, JSON_COMPACT);
  assert(text);
  if (accepted)
    parse(text);
  else
    rejected(text, strlen(text));
  free(text);
}

int main(void)
{
  ntn_assistance_request_t result = parse(update);
  assert(result.kind == NTN_ASSISTANCE_UPDATE && result.request == 17);
  assert(result.sequence == UINT64_C(9007199254740993));
  assert(result.cell.nci == (UINT64_C(1) << 36) - 1 && !strcmp(result.cell.plmn, "00101"));
  assert(result.state.epoch.subframe == 10241 && result.state.epoch.generation == 9);
  assert(result.state.position[0] == 1000000 && result.state.position[1] == -2000000 && result.state.position[2] == 3000000);
  assert(result.state.velocity[0] == 1000 && result.state.velocity[1] == -2000 && result.state.velocity[2] == 3000);
  assert(result.state.ta_common == 1000 && result.state.ta_drift == -1000 && result.state.ta_drift_variant == 10);
  assert(result.state.validity_index == 12 && ntn_assistance_validity_seconds(12) == 120);
  assert(ntn_assistance_validity_seconds(16) == 0);
  json_error_t error;
  json_t *root = json_loads(update, 0, &error);
  assert(root);
  const char *invalid[] = {"", "01", "-1", "+1", " 1", "1 ", "0", "1e2", "18446744073709551616"};
  for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
    json_object_set_new(root, "sequence", json_string(invalid[i]));
    check_json(root, false);
  }
  json_object_set_new(root, "sequence", json_integer(1));
  check_json(root, false);
  json_object_set_new(root, "sequence", json_string("18446744073709551615"));
  check_json(root, true);
  json_object_set_new(root, "unrecognized", json_true());
  check_json(root, false);
  json_object_del(root, "unrecognized");
  json_t *ephemeris = json_object_get(root, "ephemeris");
  json_t *position = json_object_get(ephemeris, "position_m");
  const double edge[] = {43620760.3, 43620760.4, -43620761.6, -43620761.7};
  for (unsigned int i = 0; i < 4; ++i) {
    json_array_set_new(position, 0, json_real(edge[i]));
    check_json(root, (i % 2) == 0);
  }
  json_array_set_new(position, 0, json_integer(0));
  json_object_set_new(ephemeris, "kind", json_string("teme"));
  check_json(root, false);
  json_object_set_new(ephemeris, "kind", json_string("ecef"));
  json_t *ta = json_object_get(root, "ta");
  json_object_set_new(ta, "drift_variant_us_per_s2", json_real(-0.000001));
  check_json(root, false);
  json_object_set_new(ta, "drift_variant_us_per_s2", json_integer(0));
  json_object_set_new(root, "ul_sync_validity_s", json_integer(61));
  check_json(root, false);
  json_object_set_new(root, "ul_sync_validity_s", json_integer(900));
  check_json(root, true);
  json_decref(root);
  const char hello[] = "{\"version\":1,\"type\":\"hello\",\"request\":\"0\",\"cell\":{\"plmn\":\"001001\",\"nci\":\"1\"}}";
  assert(parse(hello).kind == NTN_ASSISTANCE_HELLO);
  const char time[] = "{\"version\":1,\"type\":\"time\",\"request\":\"1\",\"cell\":{\"plmn\":\"00101\",\"nci\":\"1\"},\"session\":\"1\"}";
  assert(parse(time).kind == NTN_ASSISTANCE_TIME);
  const char status[] = "{\"version\":1,\"type\":\"status\",\"request\":\"2\",\"cell\":{\"plmn\":\"00101\",\"nci\":\"1\"},\"session\":\"1\"}";
  assert(parse(status).kind == NTN_ASSISTANCE_STATUS);
  const char duplicate[] = "{\"version\":1,\"version\":1}";
  rejected(duplicate, sizeof(duplicate) - 1);
  rejected("{\"version\":1e400}", strlen("{\"version\":1e400}"));
  rejected("[]", 2);
  rejected("{} {}", 5);
  rejected(hello, sizeof(hello));
  rejected(NULL, 1);
  char oversized[NTN_ASSISTANCE_MAX_DATAGRAM + 1] = {0};
  rejected(oversized, sizeof(oversized));
  puts("PASS: production codec, quantization, strict request families and rejection atomicity");
  return 0;
}
