/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

/* return 0 if not enough bytes or wrong format (AID value not checked,
 * only the presence of it)
 * on return, *aid_size contains the length of the AID (in bytes)
 */
unsigned char *aid_from_app_template(unsigned char *b, int size, int *aid_size)
{
  /* 102.221 Table 13.2 */
  if (size < 4) return 0;
  int app_template_length = b[1];
  int aid_length = b[3];
  if (b[0] != 0x61
      || app_template_length + 2 > size
      || b[2] != 0x4f
      || aid_length + 4 > size)
     return 0;
  *aid_size = aid_length;
  return b + 4;
}

bool expected_status(int resp_size, unsigned char *resp, unsigned char sw1, unsigned char sw2)
{
  if (resp_size < 2) return false;
  return resp[resp_size - 2] == sw1 && resp[resp_size - 1] == sw2;
}

void decode_imsi(unsigned char *in, char *out)
{
  /* format of IMSI is defined in 24.008 Figure 10.5.4 */
  int type = in[0] & 0x0f;
  out[0] = '0' + (in[0] >> 4);
  for (int i = 1; i < 15; i += 2) {
    out[i] = '0' + (in[(i + 1) / 2] & 0x0f);
    out[i + 1] = '0' + (in[(i + 1) / 2] >> 4);
  }
  out[15] = 0;
  debug("IMSI %s type %2.2x\n", out, type);
  /* accept only IMSI with odd number of digits (24.008 Table 10.5.4) */
  if (type != 9) {
    error("unsupported mobile identity type\n");
    exit(1);
  }
}

void init_byte_reader(byte_reader_t *r, unsigned char *buf, int size)
{
  r->b = buf;
  r->size = size;
  r->pos = 0;
}

bool byte_reader_get_bytes(byte_reader_t *r, unsigned char *buf, int count)
{
  if (r->pos + count > r->size) return false;
  memcpy(buf, r->b + r->pos, count);
  r->pos += count;
  return true;
}
