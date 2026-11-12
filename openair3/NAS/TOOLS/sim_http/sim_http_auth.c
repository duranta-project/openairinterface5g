/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include "sim_http_auth.h"
#include "curl_http_client.h"
#include "openair3/UICC/usim_interface.h"
#include "common/utils/LOG/log.h"
#include "utils.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int imsi_string_valid(const char *s)
{
  size_t len;

  if (!s) {
    return 0;
  }
  len = strlen(s);
  if (len < 5 || len > SIM_HTTP_IMSI_MAX_LEN) {
    return 0;
  }
  for (size_t i = 0; i < len; i++) {
    if (!isdigit((unsigned char)s[i])) {
      return 0;
    }
  }
  return 1;
}

static void bytes_to_hex_lower(const uint8_t *array, size_t length, char *hex_string)
{
  size_t j = 0;

  for (size_t i = 0; i < length; i++) {
    sprintf(&hex_string[j], "%02x", array[i]);
    j += 2;
  }
  hex_string[j] = '\0';
}

int sim_http_fetch_imsi(uicc_t *uicc)
{
  int http_code = 0;
  cJSON *json_response;

  if (!uicc || !uicc_auth_uses_http(uicc)) {
    LOG_E(SIM, "sim_http_fetch_imsi: uicc_auth_mode is not http\n");
    return -1;
  }
  if (!uicc->uiccAuthUrlStr || uicc->uiccAuthUrlStr[0] == '\0') {
    LOG_E(SIM, "sim_http_fetch_imsi: missing uicc_auth_url\n");
    return -1;
  }

  json_response = sendHttpRequestAndParseJSON(uicc->uiccAuthUrlStr, "GET", NULL, 0, &http_code);
  if (!json_response) {
    LOG_W(SIM, "GET IMSI HTTP request failed (code %d)\n", http_code);
    return -1;
  }

  cJSON *imsi = cJSON_GetObjectItem(json_response, "imsi");
  if (!imsi || !cJSON_IsString(imsi) || !imsi_string_valid(imsi->valuestring)) {
    LOG_W(SIM, "GET IMSI: invalid or missing imsi field\n");
    cJSON_Delete(json_response);
    return -1;
  }

  char *new_imsi = strdup(imsi->valuestring);
  if (!new_imsi) {
    cJSON_Delete(json_response);
    return -1;
  }

  free(uicc->imsiStr);
  uicc->imsiStr = new_imsi;
  LOG_I(SIM, "Update UICC IMSI from SIM HTTP: %s\n", uicc->imsiStr);
  cJSON_Delete(json_response);
  return 0;
}

int sim_http_run_aka(const char *url,
                     const uint8_t rand[16],
                     const uint8_t autn[16],
                     uint8_t ck[16],
                     uint8_t ik[16],
                     uint8_t res[8],
                     uint8_t *auts_out,
                     size_t auts_out_bytes)
{
  char rand_str[33];
  char autn_str[33];
  char *body = NULL;
  int http_code = 0;
  int rc = SIM_HTTP_AKA_FAIL;
  cJSON *body_json = NULL;
  cJSON *json_response = NULL;

  if (!url || !rand || !autn || !ck || !ik || !res) {
    return SIM_HTTP_AKA_FAIL;
  }

  bytes_to_hex_lower(rand, 16, rand_str);
  bytes_to_hex_lower(autn, 16, autn_str);

  body_json = cJSON_CreateObject();
  if (!body_json) {
    return SIM_HTTP_AKA_FAIL;
  }
  cJSON_AddStringToObject(body_json, "rand", rand_str);
  cJSON_AddStringToObject(body_json, "autn", autn_str);
  body = cJSON_PrintUnformatted(body_json);
  if (!body) {
    cJSON_Delete(body_json);
    return SIM_HTTP_AKA_FAIL;
  }

  json_response = sendHttpRequestAndParseJSON(url, "POST", body, (uint32_t)strlen(body), &http_code);
  if (!json_response) {
    LOG_W(SIM, "SIM HTTP AKA POST failed (http %d)\n", http_code);
    goto out;
  }

  cJSON *auts = cJSON_GetObjectItem(json_response, "auts");
  if (auts && cJSON_IsString(auts) && auts->valuestring) {
    if (!auts_out || auts_out_bytes < 14 || hex_string_to_hex_value(auts_out, auts->valuestring, 14) < 0) {
      LOG_W(SIM, "SIM HTTP AKA: invalid AUTS\n");
      goto out;
    }
    LOG_I(SIM, "SIM HTTP AKA: received AUTS (SQN sync)\n");
    rc = SIM_HTTP_AKA_AUTS;
    goto out;
  }

  cJSON *ck_str = cJSON_GetObjectItem(json_response, "ck");
  cJSON *ik_str = cJSON_GetObjectItem(json_response, "ik");
  cJSON *res_str = cJSON_GetObjectItem(json_response, "resTemp");

  if (!ck_str || !cJSON_IsString(ck_str) || !ik_str || !cJSON_IsString(ik_str) || !res_str || !cJSON_IsString(res_str)) {
    LOG_W(SIM, "SIM HTTP AKA: missing ck/ik/resTemp in response\n");
    goto out;
  }

  if (hex_string_to_hex_value(ck, ck_str->valuestring, 16) < 0 || hex_string_to_hex_value(ik, ik_str->valuestring, 16) < 0
      || hex_string_to_hex_value(res, res_str->valuestring, 8) < 0) {
    LOG_W(SIM, "SIM HTTP AKA: invalid hex in ck/ik/resTemp\n");
    goto out;
  }

  LOG_D(SIM, "SIM HTTP AKA: authentication vectors received (ck/ik not logged at info level)\n");
  LOG_I(SIM, "SIM HTTP AKA: success\n");
  rc = SIM_HTTP_AKA_OK;

out:
  if (json_response) {
    cJSON_Delete(json_response);
  }
  free(body);
  cJSON_Delete(body_json);
  return rc;
}
