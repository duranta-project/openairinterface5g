/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _PCSC_H_
#define _PCSC_H_

#include "def.h"

void init_card_pcsc(card_t *card, int pcsc_card_id);
void close_card_pcsc(card_t *card);
void reset_card_pcsc(card_t *card);
int send_apdu_pcsc(card_t *card, unsigned char *apdu, int length,
                   unsigned char *resp, int resp_size);

#endif /* _PCSC_H_ */
