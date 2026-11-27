/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _UICC_H_
#define _UICC_H_

/* implement functions found in ISO 7816-4 and ETSI TS 102.221 */

#include "def.h"

void select_file(card_t *card, unsigned char name_1, unsigned char name_2);
int select_app(card_t *card, unsigned char *name, int name_length,
               unsigned char *aid_fcp, int aid_fcp_size);
int read_record(card_t *card, int rec, unsigned char *out, int out_size);
int read_binary(card_t *card, unsigned char *out, int out_size);
void status(card_t *card);
int pin_count(card_t *card);
bool verify_pin(card_t *card, char *pin);

#endif /* _UICC_H_ */
