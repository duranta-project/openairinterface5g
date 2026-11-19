/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
#include "openair1/PHY/CODING/nrLDPC_defs.h"
int LDPCencoder(unsigned char **inputArray, unsigned char *outputArray, encoder_implemparams_t *impp);
uint32_t **LDPCencoder32(uint8_t **input, encoder_implemparams_t *impp);
