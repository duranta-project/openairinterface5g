/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "uicc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tpdu-apdu.h"
#include "util.h"
#include "log.h"

void select_file(card_t *card, unsigned char name_1, unsigned char name_2)
{
  int channel = 0;
  unsigned char resp[1024];

  unsigned char apdu[] = {
    channel,
    SELECT,
    SELECT_BY_FILE_ID,
    SELECT_ACTIVATION_RETURN_FCP,
    2,               /* Lc, length of file ID, which is 2 bytes here */
    name_1, name_2,
    0                /* Le, because we want FCP in return */
  };
  int resp_size = send_apdu(card, apdu, sizeof(apdu), resp, sizeof(resp));
  debug("resp size %d\n", resp_size);
  for (int i = 0; i < resp_size; i++) debug(" %2.2x", resp[i]);
  debug("\n");

  if (!expected_status(resp_size, resp, 0x90, 0x00)) { error("failed selecting file\n"); exit(1); }
}

/* we store the application FCP for further analysis
 * (for example to check if PIN is needed for USIM application)
 */
int select_app(card_t *card, unsigned char *name, int name_length,
               unsigned char *aid_fcp, int aid_fcp_size)
{
  int channel = 0;
  unsigned char resp[1024];
  unsigned char apdu[6 + name_length];
  apdu[0] = channel;
  apdu[1] = SELECT;
  apdu[2] = SELECT_BY_DF_NAME;
  apdu[3] = SELECT_ACTIVATION_RETURN_FCP;
  apdu[4] = name_length;                  /* Lc: length of data */

  for (int i = 0; i < name_length; i++) apdu[5 + i] = name[i];

  apdu[5 + name_length] = 0;              /* Le */

  int resp_size = send_apdu(card, apdu, sizeof(apdu), resp, sizeof resp);
  debug("resp size %d\n", resp_size);
  for (int i = 0; i < resp_size; i++) debug(" %2.2x", resp[i]);
  debug("\n");

  if (!expected_status(resp_size, resp, 0x90, 0x00)) { error("failed selecting app\n"); exit(1); }

  if (resp_size - 2 > aid_fcp_size) { error("AID FCP is too big\n"); exit(1); }

  memcpy(aid_fcp, resp, resp_size - 2);

  return resp_size - 2;
}

/* return -1 in case of error/no record, length of rec otherwise */
int read_record(card_t *card, int rec, unsigned char *out, int out_size)
{
  int channel = 0;
  unsigned char apdu[] = {
    channel,
    READ_RECORD,
    rec,
    READ_RECORD_ABSOLUTE,
    0x00
  };

  int resp_size = send_apdu(card, apdu, sizeof(apdu), out, out_size);
  debug("resp size %d\n", resp_size);
  for (int i = 0; i < resp_size; i++) debug(" %2.2x", out[i]);
  debug("\n");
  if (expected_status(resp_size, out, 0x6a, 0x83))
    return -1;
  if (!expected_status(resp_size, out, 0x90, 0x00)) {
    if (resp_size < 2) debug("read record failed\n");
    else debug("read record failed sw1/sw2 %2.2x %2.2x\n",
               out[resp_size - 2], out[resp_size - 1]);
    return -1;
  }
  return resp_size;
}

/* return -1 in case of error/no record, length of data otherwise */
int read_binary(card_t *card, unsigned char *out, int out_size)
{
  int channel = 0;
  unsigned char apdu[] = {
    channel,
    READ_BINARY,
    0x00,            /* 102.221 Table 11.10 - offset 0 */
    0x00,            /* 102.221 11.1.3.2 - offset 0 (low part) */
    0x00             /* Le */
  };

  int resp_size = send_apdu(card, apdu, sizeof(apdu), out, out_size);
  debug("resp size %d\n", resp_size);
  for (int i = 0; i < resp_size; i++) debug(" %2.2x", out[i]);
  debug("\n");
  if (!expected_status(resp_size, out, 0x90, 0x00)) { debug("read binary fails\n"); return -1; }
  return resp_size;
}

void status(card_t *card)
{
  int channel = 0;
  unsigned char resp[1024];

  unsigned char apdu[] = {
    CLASS_80 | channel,
    STATUS,
    STATUS_INITIALIZED,
    0x00,                  /* 102.221 Table 11.9 */
    0x00                   /* Le */
  };
  int resp_size = send_apdu(card, apdu, sizeof(apdu), resp, 1024);
  debug("resp size %d\n", resp_size);
  for (int i = 0; i < resp_size; i++) debug(" %2.2x", resp[i]);
  debug("\n");
  if (!expected_status(resp_size, resp, 0x90, 0x00)) { error("STATUS failed\n"); exit(1); }
}

/* return number of tries remaining (-1 if error) */
int pin_count(card_t *card)
{
  int channel = 0;
  unsigned char resp[1024];

  unsigned char apdu[] = {
    channel,
    VERIFY_PIN,
    0x00,            /* 102.221 11.1.9.3 */
    0x01             /* global(?) pin 1, 102.221 Table 11.15, (todo: be sure) */
  };

  int resp_size = send_apdu(card, apdu, sizeof(apdu), resp, 1024);
  debug("resp size %d\n", resp_size);
  for (int i = 0; i < resp_size; i++) debug(" %2.2x", resp[i]);
  debug("\n");

  if (resp_size != 2
      || resp[0] != 0x63
      || (resp[1] & 0xf0) != 0xc0) {
    error("bad return from PIN check (resp_size %d)", resp_size);
    for (int i = 0; i < resp_size; i++) error(" %2.2x", resp[i]);
    error("\n");
    return -1;
  }

  debug("PIN count is %d\n", resp[1] & 0x0f);

  return resp[1] & 0x0f;
}

/* return true if verification successful, false if failure */
bool verify_pin(card_t *card, char *pin)
{
  int channel = 0;
  unsigned char resp[1024];

  unsigned char apdu[13] = {
    channel,
    VERIFY_PIN,
    0x00,            /* 102.221 11.1.9.3 */
    0x01,            /* global(?) pin 1, 102.221 Table 11.15, (todo: be sure) */
    0x08
  };

  /* coding of PIN as defined in 31.101 9.6 (for digits ITU-T T.50 is same as ASCII) */
  int pos;
  for (pos = 0; pin[pos]; pos++)
    apdu[5 + pos] = pin[pos];
  for (; pos < 8; pos++)
    apdu[5 + pos] = 0xff;

  int resp_size = send_apdu(card, apdu, sizeof(apdu), resp, 1024);
  debug("resp size %d\n", resp_size);
  for (int i = 0; i < resp_size; i++) debug(" %2.2x", resp[i]);
  debug("\n");
  if (expected_status(resp_size, resp, 0x90, 0x00)) return true;
  /* this 69 84 return code comes from real testing
   * when there is no PIN, it's what is returned
   */
  if (expected_status(resp_size, resp, 0x69, 0x84)) {
    info("PIN was given, but there is no PIN, card is open\n");
    return true;
  }
  return false;
}
