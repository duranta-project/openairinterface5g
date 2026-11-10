/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <pthread.h>
#include "nfapi/oai_integration/vendor_ext.h"

static const char *const nfapi_str_mode[] = {
    "MONOLITHIC",
    "PNF",
    "VNF",
    "AERIAL",
    "UE_STUB_PNF",
    "UE_STUB_OFFNET",
    "STANDALONE_PNF",
    "<UNKNOWN NFAPI MODE>"
};

typedef struct {
  nfapi_mode_t nfapi_mode;
} nfapi_params_t;

static nfapi_params_t nfapi_params = {0};

const char *nfapi_get_strmode(void) {
  if (nfapi_params.nfapi_mode > NFAPI_MODE_UNKNOWN)
    return nfapi_str_mode[NFAPI_MODE_UNKNOWN];

  return nfapi_str_mode[nfapi_params.nfapi_mode];
}

nfapi_mode_t nfapi_getmode(void) {
  return nfapi_params.nfapi_mode;
}

void nfapi_setmode(nfapi_mode_t nfapi_mode) {
  nfapi_params.nfapi_mode = nfapi_mode;
}
