#ifndef _CURL_HTTP_CLIENT_H_
#define _CURL_HTTP_CLIENT_H_

#include "cJSON.h"
#include <stdint.h>

cJSON *sendHttpRequestAndParseJSON(const char *url, const char *method, const char *msgbody, uint32_t msgbody_len, int *httpCode);

#endif