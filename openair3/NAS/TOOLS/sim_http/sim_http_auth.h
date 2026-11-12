/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#ifndef SIM_HTTP_AUTH_H
#define SIM_HTTP_AUTH_H

#include <stddef.h>
#include <stdint.h>
#include "openair3/UICC/usim_interface.h"

#define SIM_HTTP_IMSI_MAX_LEN 15

enum sim_http_aka_result {
  SIM_HTTP_AKA_FAIL = -1,
  SIM_HTTP_AKA_OK = 0,
  SIM_HTTP_AKA_AUTS = 1,
};

/** GET IMSI from SIM HTTP service; updates uicc->imsiStr on success. */
int sim_http_fetch_imsi(uicc_t *uicc);

/** POST RAND/AUTN; fills ck/ik/res or auts_out. Does not log ck/ik. */
int sim_http_run_aka(const char *url,
                     const uint8_t rand[16],
                     const uint8_t autn[16],
                     uint8_t ck[16],
                     uint8_t ik[16],
                     uint8_t res[8],
                     uint8_t *auts_out,
                     size_t auts_out_bytes);

#endif
