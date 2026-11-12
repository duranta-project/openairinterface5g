/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
#ifndef LDPC_SIMTEST_NRLDPC_EXTERN_H
#define LDPC_SIMTEST_NRLDPC_EXTERN_H
#include "openair1/PHY/CODING/nrLDPC_defs.h"
int LDPCencoder(unsigned char **inputArray, unsigned char *outputArray, encoder_implemparams_t *impp);
uint32_t **LDPCencoder32(uint8_t **input, encoder_implemparams_t *impp);
#endif
