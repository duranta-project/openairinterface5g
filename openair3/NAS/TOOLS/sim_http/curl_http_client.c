/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include "curl_http_client.h"
#include "common/utils/LOG/log.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define CURL_TIMEOUT_MS 3000
#define SIM_HTTP_RESPONSE_MAX (64 * 1024)
#define SIM_HTTP_RESPONSE_INITIAL 512

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} http_response_buf_t;

static pthread_once_t curl_global_once = PTHREAD_ONCE_INIT;

static void curl_global_init_once(void)
{
  if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
    LOG_E(SIM, "curl_global_init failed\n");
  }
}

static void ensure_curl_global(void)
{
  pthread_once(&curl_global_once, curl_global_init_once);
}

static int http_response_grow(http_response_buf_t *buf, size_t add_len)
{
  size_t need = buf->len + add_len + 1;
  if (need > SIM_HTTP_RESPONSE_MAX) {
    return -1;
  }
  if (need <= buf->cap) {
    return 0;
  }
  size_t new_cap = buf->cap ? buf->cap : SIM_HTTP_RESPONSE_INITIAL;
  while (new_cap < need) {
    if (new_cap >= SIM_HTTP_RESPONSE_MAX) {
      return -1;
    }
    new_cap *= 2;
  }
  if (new_cap > SIM_HTTP_RESPONSE_MAX) {
    new_cap = SIM_HTTP_RESPONSE_MAX;
  }
  char *next = realloc(buf->data, new_cap);
  if (!next) {
    return -1;
  }
  buf->data = next;
  buf->cap = new_cap;
  return 0;
}

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  http_response_buf_t *buf = (http_response_buf_t *)userp;

  if (http_response_grow(buf, realsize) != 0) {
    LOG_W(SIM, "SIM HTTP response exceeds %u bytes, truncating transfer\n", SIM_HTTP_RESPONSE_MAX);
    return 0;
  }
  memcpy(buf->data + buf->len, contents, realsize);
  buf->len += realsize;
  buf->data[buf->len] = '\0';
  return realsize;
}

static void http_response_buf_free(http_response_buf_t *buf)
{
  free(buf->data);
  buf->data = NULL;
  buf->len = 0;
  buf->cap = 0;
}

cJSON *sendHttpRequestAndParseJSON(const char *url, const char *method, const char *msgbody, uint32_t msgbody_len, int *httpCode)
{
  CURL *curl;
  CURLcode res;
  char errorBuffer[CURL_ERROR_SIZE];
  http_response_buf_t response = {0};
  struct curl_slist *headers = NULL;
  cJSON *parsed = NULL;

  if (!url || !method || !httpCode) {
    return NULL;
  }

  ensure_curl_global();
  curl = curl_easy_init();
  if (!curl) {
    LOG_E(SIM, "Failed to initialize libcurl\n");
    return NULL;
  }

  if ((strcmp(method, "POST") == 0) || (strcmp(method, "PUT") == 0) || (strcmp(method, "PATCH") == 0)) {
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  if (strcmp(method, "POST") == 0) {
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
  } else if (strcmp(method, "PUT") == 0) {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
  } else if (strcmp(method, "DELETE") == 0) {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
  } else if (strcmp(method, "PATCH") == 0) {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
  } else {
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  }

  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)CURL_TIMEOUT_MS);
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);

  if ((strcmp(method, "POST") == 0) || (strcmp(method, "PUT") == 0) || (strcmp(method, "PATCH") == 0)) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)msgbody_len);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, msgbody ? msgbody : "");
  }

  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    LOG_E(SIM, "curl_easy_perform failed: %s\n", errorBuffer[0] ? errorBuffer : curl_easy_strerror(res));
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    http_response_buf_free(&response);
    return NULL;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, httpCode);
  if (*httpCode < 200 || *httpCode >= 300) {
    LOG_W(SIM, "SIM HTTP status %d for %s %s\n", *httpCode, method, url);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    http_response_buf_free(&response);
    return NULL;
  }

  if (!response.data || response.len == 0) {
    LOG_W(SIM, "SIM HTTP empty response body\n");
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    http_response_buf_free(&response);
    return NULL;
  }

  LOG_D(SIM, "SIM HTTP response (%zu bytes)\n", response.len);
  parsed = cJSON_Parse(response.data);
  if (!parsed) {
    LOG_E(SIM, "SIM HTTP JSON parse failed\n");
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  http_response_buf_free(&response);
  return parsed;
}
