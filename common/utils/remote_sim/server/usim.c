/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "usim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tpdu-apdu.h"
#include "uicc.h"
#include "util.h"
#include "log.h"

/* return true if we got imsi, false if not */
bool get_imsi(card_t *card, char *imsi)
{
  select_file(card, EF_IMSI);
  unsigned char out[1024];
  int out_size = read_binary(card, out, 1024);
  if (out_size > 2) {
    int imsi_length = out[0];
    if (imsi_length != 8 || out_size - 3 != 8)
      { error("unsupported IMSI length %d\n", imsi_length); exit(1); }
    decode_imsi(&out[1], imsi);
    return true;
  }
  return false;
}

authenticate_result_t authenticate(card_t * card, unsigned char *rand, unsigned char *autn)
{
  int channel = 0;
  unsigned char apdu[128];

  /* from 33.102 (dig it for precise location):
   * AUTN = SQN xor AK | AMF | MAC
   * AUTS = SQNMS xor AK | MAC-S
   *
   * 33.102 6.3.7 gives length:
   * RAND 128b
   * SQN  48b  (same for SQNMS)
   * AK   48b
   * AMF  16b
   * MAC  64b  (same for MAC-S)
   *
   * so we deduce:
   * AUTN 128b
   * AUTS 64b
   */

  /* 31.102 7.1.2 */
  apdu[0] = channel;
  apdu[1] = AUTHENTICATE;
  apdu[2] = 0;
  apdu[3] = 0x81;            /* 31.102 Table 7.1.2-1: specific ref. data and 3G context */
  apdu[4] = (16 + 1) * 2;    /* Lc: length of RAND and AUTN fields */

  /* 31.102 7.1.2.1 */
  /* RAND (128b, 16 bytes) */
  apdu[5] = 16;
  memcpy(apdu + 5 + 1, rand, 16);

  /* AUTN (128b) */
  apdu[5 + 1 + 16] = 16;
  memcpy(apdu + 5 + 1 + 16 + 1, autn, 16);

  /* Le (0, as defined in 31.102 7.1.2) */
  apdu[5 + 1 + 16 + 1 + 16] = 0;

  debug("AUTHENTICATE[%d]:", 5 + (1 + 16) * 2 + 1);
  for (int i = 0; i < 5 + (1 + 16) * 2 + 1; i++) debug(" %2.2x", apdu[i]);
  debug("\n");

  unsigned char resp[1024];

  int resp_size = send_apdu(card, apdu, 5 + (1 + 16) * 2 + 1, resp, sizeof resp);
  debug("resp size %d\n", resp_size);
  for (int i = 0; i < resp_size; i++) debug(" %2.2x", resp[i]);
  debug("\n");

  authenticate_result_t ret;

  /* interpret answer (31.102 7.1.2.1) */

  if (resp_size < 2) goto err;

  /* 69 82 is MAC failure */
  if (expected_status(resp_size, resp, 0x69, 0x82)) goto err;

  /* anything but 90 00 is whatever failure */
  if (!expected_status(resp_size, resp, 0x90, 0x00)) goto err;

  /* DC [len] ['len' bytes] 90 00 is sync failure, 'len' must be 14 */
  if (resp[0] == 0xdc) {
    int len = resp[1];
    if (len != 14) { error("bad AUTS length\n"); goto err; }
    int available_bytes = resp_size - 4;
    if (available_bytes != 14) goto err;
    ret.type = AUTHENTICATE_RESYNC;
    memcpy(ret.resync.auts, resp + 2, 14);
    return ret;
  }

  if (resp[0] != 0xdb) { error("AUTHENTICATE: unknown answer code %2.2x\n", resp[0]); goto err; }

  byte_reader_t b;
  init_byte_reader(&b, resp, resp_size);

#define R(x) if (!(x)) goto err

  unsigned char c;

  /* DB, drop it, already processed above */
  R(byte_reader_get_bytes(&b, &c, 1));

  /* get RES */
  unsigned char res_len;
  R(byte_reader_get_bytes(&b, &res_len, 1));
  if (res_len < 4 || res_len > 16) goto err;
  ret.success.res_length = res_len;
  R(byte_reader_get_bytes(&b, ret.success.res, res_len));

  /* get CK */
  unsigned char ck_len;
  R(byte_reader_get_bytes(&b, &ck_len, 1));
  if (ck_len != 16) goto err;
  R(byte_reader_get_bytes(&b, ret.success.ck, 16));

  /* get IK */
  unsigned char ik_len;
  R(byte_reader_get_bytes(&b, &ik_len, 1));
  if (ik_len != 16) goto err;
  R(byte_reader_get_bytes(&b, ret.success.ik, 16));

  /* (try to) get Kc (no big deal if failure) */
  unsigned char kc_len;
  ret.success.kc_valid = byte_reader_get_bytes(&b, &kc_len, 1)
                         && kc_len == 8
                         && byte_reader_get_bytes(&b, ret.success.kc, 8);

  ret.type = AUTHENTICATE_SUCCESS;
  return ret;

err:
  ret.type = AUTHENTICATE_FAILURE;
  return ret;

#undef R
}

static void check_pin_needed(card_t *card, unsigned char *fcp, int fcp_size)
{
  /* check if PIN is needed */
  byte_reader_t r;
  init_byte_reader(&r, fcp, fcp_size);

  /* decode FCP as described in 102.221 11.1.1.3 */
  unsigned char tag;
#define R(x) if (!(x)) goto err

  R(byte_reader_get_bytes(&r, &tag, 1));
  if (tag != 0x62) goto err;

  unsigned char fcp_len;
  R(byte_reader_get_bytes(&r, &fcp_len, 1));

  /* find c6 tag */
  unsigned char c6_tag[256];
  int c6_tag_len = 0;

  while (fcp_len) {
    unsigned char len;
    R(byte_reader_get_bytes(&r, &tag, 1)); fcp_len--;
    R(byte_reader_get_bytes(&r, &len, 1)); fcp_len--;
    if (tag != 0xc6) {
      if (len) {
        unsigned char b[len];
        R(byte_reader_get_bytes(&r, b, len)); fcp_len -= len;
      }
      continue;
    }
    /* tag c6 found */
    if (!len) goto err;
    c6_tag_len = len;
    R(byte_reader_get_bytes(&r, c6_tag, len)); fcp_len -= len;
    break;
  }

  /* parse c6 tag */
  init_byte_reader(&r, c6_tag, c6_tag_len);

  /* spec is unclear, so let's accept only: 90 01 XX
   * followed by many 95 01 xx 83 01 xx (with 95 01 xx optional)
   * (actually we accept 95 01 xx anywhere, and discard them)
   * what is know to work? the following byte sequence (PIN required):
   * c6 0f 90 01 f0 83 01 01 83 01 81 83 01 0a 83 01 0b
   * this one also works (PIN not required):
   * c6 0c 90 01 60 83 01 01 83 01 81 83 01 0a
   */
  unsigned char len;
  unsigned char value;
  R(byte_reader_get_bytes(&r, &tag, 1));
  R(byte_reader_get_bytes(&r, &len, 1));
  R(byte_reader_get_bytes(&r, &value, 1));
  if (tag != 0x90 || len != 1) goto err;

  unsigned char ps_do = value;

  int pin_list[256]; for (int i = 0; i < 256; i++) pin_list[i] = -1;
  int pin_list_size = 0;

  while (1) {
    if (!byte_reader_get_bytes(&r, &tag, 1)) break;
    R(byte_reader_get_bytes(&r, &len, 1));
    R(byte_reader_get_bytes(&r, &value, 1));
    if (len != 1) goto err;
    if (tag == 0x95) continue;     /* ignore usage qualifier */
    if (tag != 0x83) goto err;
    pin_list[pin_list_size] = value;
    pin_list_size++;
  }

  /* PIN is numbered 1 (102.221 Table 9.3) (not super clear...) */
  /* todo: be sure of the logic and the loop below */
  bool pin_needed = false;

  /* search if PIN is in 'pin_list' for when its ps_do bit is 1 */
  for (int pin_item = 0; pin_item < 8 && pin_item < pin_list_size; pin_item++) {
    int bit = 1 << (7 - pin_item);
    if (ps_do & bit) {
      if (pin_list[pin_item] == 1)    /* PIN identity is 1 (todo: be sure it's correct) */
        pin_needed = 1;
    }
  }

  card->pin_needed = pin_needed;

  debug("PIN needed? %s\n", pin_needed ? "yes" : "no");

  return;

err:
  error("problem with FCP of USIM app\n");
  exit(1);

#undef R
}

void open_usim(card_t *card)
{
  /* read EF_DIR to find the USIM AID (application ID)
   * USIM AID is up to 16 bytes and starts with a0 00 00 00 87 10 02
   * (see 101.220 Annex E)
   */

  /* select MF, to be sure (should not be necessary) */
  select_file(card, MF);

  /* select EF_DIR */
  select_file(card, EF_DIR);

  /* get records until we find the USIM application */
  bool has_usim = false;
  unsigned char resp[1024];
  for (int rec = 1; ; rec++) {
    int rec_size = read_record(card, rec, resp, 1024);
    if (rec_size <= 0) break;
    int aid_size = 0;
    unsigned char *aid = aid_from_app_template(resp, rec_size, &aid_size);
    if (aid && aid_size >= sizeof(STR(AID_USIM))
        && !memcmp(aid, STR(AID_USIM), sizeof(STR(AID_USIM)))) {
      has_usim = true;
      break;
    }
  }
  if (!has_usim) { error("error: no USIM application found\n"); exit(1); }

  /* select the USIM application */
  unsigned char fcp[1024];
  int fcp_size = select_app(card, STR(AID_USIM), sizeof(STR(AID_USIM)),
                            fcp, sizeof(fcp));

  check_pin_needed(card, fcp, fcp_size);
}

/* pass pin -1 to not do pin verification */
void init_usim(card_t *card, int pin)
{
  /* 31.102 5.1.1.2 */

  /* emergency call codes: todo */
  /* language indication: todo */

  /* user verification */

  /* don't verify pin if no pin was passed here */
  if (pin == -1) goto no_pin;

  /* don't verify pin if not needed */
  if (!card->pin_needed) { info("PIN verification not needed\n"); goto no_pin; }

  /* only accept a count >= 3, so as not to accidentally lock the SIM card in
   * case of bad PIN given by the user
   */
  int count = pin_count(card);
  if (count < 3) {
    error("PIN count (%d) is less than 3, fix\n", count);
    exit(1);
  }

  if (!verify_pin(card, pin)) { error("bad pin\n"); exit(1); }

no_pin:;

  int mnc_length = -1;

  unsigned char out[1024];
  int out_size;

  /* administration information request */
  select_file(card, EF_AD);
  out_size = read_binary(card, out, 1024);
  if (out_size >= 2 + 4)
    mnc_length = out[3] & 0x0f;
  debug("MNC has %d digits (is -1 if error reading EF_AD, 0 if service 130 is available)\n", mnc_length);

  if (mnc_length != 2 && mnc_length != 3) { error("bad MNC length %d\n", mnc_length); exit(1); }

  card->mnc_length = mnc_length;

  /* usim service table request */
  select_file(card, EF_UST);
  out_size = read_binary(card, out, 1024);
  if (out_size >= 2) out_size -= 2;
  bool available_services[137]; for (int i = 0; i < 137; i++) available_services[i] = false;
  for (int i = 0; i < out_size * 8; i++)
    if (out[i / 8] & (1 << (i % 8))) {
      int service_id = i + 1;
      debug("service %d available\n", service_id);
      if (i + 1 < 137) available_services[service_id] = true;
    }

  /* enabled services table request (31.102 4.2.47) */
  bool enabled_services[137]; for (int i = 0; i < 137; i++) enabled_services[i] = false;
  if (available_services[2]
      || available_services[6]
      || available_services[34]
      || available_services[35]) {
    select_file(card, EF_EST);
    unsigned char out[1024];
    int out_size = read_binary(card, out, 1024);
    if (out_size >= 2) out_size -= 2;
    for (int i = 0; i < out_size * 8; i++)
      if (out[i / 8] & (1 << (i % 8))) {
        int service_id = i + 1;
        debug("service %d enabled\n", service_id);
        if (i + 1 < 137) enabled_services[service_id] = true;
      }
    (void)enabled_services;
  }

  /* get IMSI if service 130 is not available (31.102 4.2.2) */
  /* todo: test presence of service 130 */
  if (1 || !available_services[130]) {
    char imsi[16];
    if (!get_imsi(card, imsi)) error("couldn't read IMSI (PIN needed?)\n");
  }

  /* get SUPI_NAI if service 130 is available: todo */

  /* all other things in 31.102 5.1.1.2 not done: todo (or not) */

  /* send STATUS to tell the USIM we (the terminal) are ready */
  status(card);
}
