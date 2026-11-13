/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _TPDU_APDU_H_
#define _TPDU_APDU_H_

/* implement TPDU / APDU processing */

#include "def.h"

int send_apdu(card_t *card, unsigned char *apdu, int length,
              unsigned char *resp, int resp_size);

#endif /* _TPDU_APDU_H_ */
