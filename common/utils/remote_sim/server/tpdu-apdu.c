/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "tpdu-apdu.h"

#include <stdio.h>
#include <stdlib.h>

#include "pl2303.h"
#include "log.h"

int send_apdu(card_t *card, unsigned char *apdu, int length,
              unsigned char *resp, int resp_size)
{
  int out_pos = 0;
  int warn_byte = -1;     /* warning condition, if one is received through 62 xx status */
  int warn_type = 0;      /* 62 or 63, what the card sends */

#define PUT(x) do { \
  if (out_pos == resp_size) { error("send_apdu: resp_size too small\n"); exit(1); } \
  resp[out_pos] = (x); \
  out_pos++; \
} while (0)

  debug("send apdu");
  for (int i = 0; i < length; i++) debug(" %2.2x", apdu[i]);
  debug("\n");
  /* get the case (ISO 7816-3 Table 13) (we don't do extended, only short) */
  int tpdu_case;

  if (length < 4) { error("bad APDU\n"); exit(1); }

  if (length == 4) tpdu_case = 1;
  else if (length == 5) tpdu_case = 2;
  else {
    int c5 = apdu[4];
    if (c5 == 0) { error("unsupported APDU, C(5) = 0 not supported (the is E, we do only S)\n"); exit(1); }
    if (length == 5 + c5) tpdu_case = 3;
    else if (length == 6 + c5) tpdu_case = 4;
    else { error("bad APDU\n"); exit(1); }
  }

  debug("TPDU case %d\n", tpdu_case);

  int Le = 0;
  int Lc = 0;

  /* get Le and Lc, which depends on case (ISO 7816-3 12.2.2 and following) */
  switch (tpdu_case) {
  case 1: /* no Le, no Lc */ break;
  case 2: Le = apdu[4]; if (Le == 0) Le = 256; break;
  case 3: Lc = apdu[4]; break;
  case 4: Lc = apdu[4]; Le = apdu[length - 1]; if (Le == 0) Le = 256; break;
  }

  unsigned char *data_to_send = apdu + 5;

  /* send command header (ISO 7816-3 10.3.2) */
  unsigned char first_tpdu[5];
  for (int i = 0; i < 4; i++) first_tpdu[i] = apdu[i];
  if (tpdu_case == 1) first_tpdu[4] = 0;
  else                first_tpdu[4] = apdu[4];

  /* we try again when we receive procedure byte 6c
   * or when we send GET_RESPONSE
   *
   * note: 102.221 7.3.1.1.5 explains how 61 and 6c are used, here we
   * accept more cases (card may send several 61 and/or 6c, we would
   * accept), so it's not fully standard
   * (but no commercial simcard will ever do that, right?)
   */
try_again:
  put_bytes(card, first_tpdu, 5);

  /* receive procedure byte (ISO 7816-3 10.3.3 and Table 11) */
  unsigned char ins = first_tpdu[1];
  unsigned char sw1, sw2;
  while (1) {
    unsigned char procedure_byte = get_byte(card, DEFAULT_TIMEOUT);
    if (procedure_byte == 0x60) continue;
    if ((procedure_byte & 0xf0) == 0x60
        || (procedure_byte & 0xf0) == 0x90) {
      sw1 = procedure_byte;
      break;
    }
    if (procedure_byte == ins) {
      /* send data if we have to */
      if (Lc) {
        put_bytes(card, data_to_send, Lc);
        Lc = 0;
        continue;
      }
      /* receive data if we have to */
      if (Le) {
        for (int i = 0; i < Le; i++) {
          unsigned char byte = get_byte(card, DEFAULT_TIMEOUT);
          PUT(byte);
        }
        Le = 0;
      }
      continue;
    }
    if (procedure_byte == (ins ^ 0xff)) {
      /* send data if we have to */
      if (Lc) {
        put_bytes(card, data_to_send, 1);
        Lc--;
        data_to_send++;
        continue;
      }
      /* receive one byte if we have to */
      if (Le) {
        unsigned char byte = get_byte(card, DEFAULT_TIMEOUT);
        PUT(byte);
        Le--;
      }
      continue;
    }
    /* invalid value */
    error("invalid value received from card %2.2x\n", procedure_byte);
    exit(1);
  }

  sw2 = get_byte(card, DEFAULT_TIMEOUT);

  if (sw1 == 0x90 && sw2 == 0x00) {
    if (warn_byte != -1) {
      /* 102.221 C.1.7 says we must put 62 xx when there was a warning
       * 7816-4 has 63 for warning as well, let's support it (via 'warn_type')
       */
      PUT(warn_type);
      PUT(warn_byte);
    } else {
      PUT(sw1);
      PUT(sw2);
    }
    return out_pos;
  }

  if (sw1 == 0x61) {
    /* we call GET RESPONSE to get sw2 bytes (ISO 7816-4 5.1.3) */
    ins = GET_RESPONSE;
    unsigned char get_response_tpdu[5] = {
      first_tpdu[0],                 /* same CLA */
      ins, 0, 0,            /* ISO 7816-4 7.6.1 */
      sw2
    };
    Le = sw2; if (Le == 0) Le = 256;
    if (Lc) { error("Lc not 0??\n"); exit(1); }
    /* instruction changed, now it's GET_RESPONSE */
    for (int i = 0; i < 5; i++) first_tpdu[i] = get_response_tpdu[i];
    goto try_again;
  }

  if (sw1 == 0x6c) {
    /* start over with Le == sw2 */
    Le = sw2; if (Le == 0) Le = 256;
    first_tpdu[4] = sw2;
    goto try_again;
  }

  /* for 62, see 102.221 C.1.7 for an example
   * not sure we do it properly
   * ISO 7816-4 5.1.3 has 63 for warning as well, let's accept that too
   */
  if (sw1 == 0x62 || sw1 == 0x63) {
    if (warn_byte != -1) { error("error: more than one warning condition received\n"); exit(1); }
    warn_type = sw1;
    warn_byte = sw2;

    /* case 4 is special, we do GET RESPONSE, 102.221 7.3.1.1.0 (todo: be sure of this) */
    if (tpdu_case == 4) {
      debug("warning: %2.2x received for case 4 command, not super tested, may fail\n", sw1);
      /* we call GET RESPONSE with 0 (so 256) bytes (102.221 C.1.7) */
      ins = GET_RESPONSE;
      unsigned char get_response_tpdu[5] = {
        first_tpdu[0],                 /* same CLA */
        ins, 0, 0,            /* ISO 7816-4 7.6.1 */
        0
      };
      Le = 256;
      if (Lc) { error("Lc not 0?? (line %d)\n", __LINE__); exit(1); }
      /* instruction changed, now it's GET_RESPONSE */
      for (int i = 0; i < 5; i++) first_tpdu[i] = get_response_tpdu[i];
      goto try_again;
    }
  }

  debug("error/warning/whatever sw1/sw2 %2.2x %2.2x\n", sw1, sw2);

  out_pos = 0;
  PUT(sw1);
  PUT(sw2);

  return out_pos;

#undef PUT
}
