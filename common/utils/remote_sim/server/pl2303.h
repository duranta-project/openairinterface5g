/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _PL2303_H_
#define _PL2303_H_

/* interface with a PL2303-based SIM card reader, as the one sold
 * by Laurent Thomas' Open Cells company (the one that was used
 * while writing this software has USB ID 067b:2303)
 */

#include "def.h"

void init_card(card_t *card, char *card_filename);
void close_card(card_t *card);
void reset_card(card_t *card);

unsigned char get_byte(card_t *card, int timeout_ms);
void put_bytes(card_t *card, unsigned char *bytes, int count);

#endif /* _PL2303_H_ */
