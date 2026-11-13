/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdbool.h>

unsigned char *aid_from_app_template(unsigned char *b, int size, int *aid_size);
bool expected_status(int resp_size, unsigned char *resp, unsigned char sw1, unsigned char sw2);
void decode_imsi(unsigned char *in, char *out);

typedef struct {
  unsigned char *b;
  int size;
  int pos;
} byte_reader_t;

void init_byte_reader(byte_reader_t *r, unsigned char *buf, int size);
bool byte_reader_get_bytes(byte_reader_t *r, unsigned char *buf, int count);

#endif /* _UTIL_H_ */
