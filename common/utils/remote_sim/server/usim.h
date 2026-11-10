/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _USIM_H_
#define _USIM_H_

/* implement functions found in 3GPP TS 31.102 */

#include "def.h"

void open_usim(card_t *card);
void init_usim(card_t *card, char *pin);
bool get_imsi(card_t *card, char *imsi);
authenticate_result_t authenticate(card_t * card, unsigned char *rand, unsigned char *autn);

#endif /* _USIM_H_ */
