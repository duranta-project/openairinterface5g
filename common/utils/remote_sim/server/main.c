/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "pl2303.h"
#include "atr.h"
#include "usim.h"
#include "uicc.h"
#include "net.h"
#include "log.h"

void run_client(card_t *card, int s)
{
  net_message_t m;

  m.type = NET_CARD_READY;
  if (!net_send(s, &m)) goto end;

  while (true) {
    if (!net_receive(s, &m)) goto end;

    switch (m.type) {

    case NET_AUTN: {
      authenticate_result_t auth_resp = authenticate(card, m.autn.rand, m.autn.autn);
      switch (auth_resp.type) {
      case AUTHENTICATE_SUCCESS:
        m.type = NET_RES;
        m.res.res_len = auth_resp.success.res_length;
        memcpy(m.res.res, auth_resp.success.res, auth_resp.success.res_length);
        memcpy(m.res.ck, auth_resp.success.ck, 16);
        memcpy(m.res.ik, auth_resp.success.ik, 16);
        break;
      case AUTHENTICATE_FAILURE:
        m.type = NET_ERR;
        break;
      case AUTHENTICATE_RESYNC:
        m.type = NET_AUTS;
        memcpy(m.auts.auts, auth_resp.resync.auts, 14);
        break;
      }
      break;
    }

    case NET_GET_IMSI: {
      m.type = get_imsi(card, m.imsi.imsi) ? NET_IMSI : NET_ERR;
      break;
    }

    default: error("unknown message received %d\n", m.type); goto end;
    }

    if (!net_send(s, &m)) goto end;
  }

end:
  info("connection with client finished\n");
  net_close(s);
}

void usage(void)
{
  printf("options:\n");
  printf("    <card filename>      use this card (default /dev/ttyUSB0)\n");
  printf("    -pin <PIN code (if absent: don't use PIN)>\n");
  printf("    -ip <listening IP (default 0.0.0.0)>\n");
  printf("    -port <listening port (default 4098)>\n");
  printf("    -d                   print debug logs\n");
  printf("    -q                   disable all logs\n");
  exit(0);
}

int main(int n, char **v)
{
  card_t card;
  char *default_card_filename = "/dev/ttyUSB0";
  char *card_filename = NULL;
  char *pin = NULL;
  char *ip = "0.0.0.0";
  int port = 4098;
  bool log_error = true;
  bool log_info = true;
  bool log_debug = false;

  /* write on a socket fails if the other end is closed and we get SIGPIPE */
  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) abort();

  for (int i = 1; i < n; i++) {
    if (!strcmp(v[i], "-h") || !strcmp(v[i], "--help")) usage();
    if (!strcmp(v[i], "-pin")) { if (i > n - 2) usage(); pin = v[++i]; continue; }
    if (!strcmp(v[i], "-ip")) { if (i > n - 2) usage(); ip = v[++i]; continue; }
    if (!strcmp(v[i], "-port")) { if (i > n - 2) usage(); port = atoi(v[++i]); continue; }
    if (!strcmp(v[i], "-d")) { log_debug = true; continue; }
    if (!strcmp(v[i], "-q")) { log_error = log_info = log_debug = false; continue; }
    if (card_filename != NULL) usage();
    card_filename = v[i];
  }

  if (card_filename == NULL)
    card_filename = default_card_filename;

  /* init_log() must be called right after command line processing */
  init_log(log_error, log_info, log_debug);

  if (pin != NULL) {
    int pin_len;
    int pin_valid = true;
    for (pin_len = 0; pin[pin_len]; pin_len++) {
      if (!(pin[pin_len] >= '0' && pin[pin_len] <= '9')) {
        pin_valid = false;
        break;
      }
    }
    if (!pin_valid || pin_len < 4 || pin_len > 8) {
      error("bad pin %s, must be four to eight digits (0..9)\n", pin);
      exit(1);
    }
  }

  info("use card %s\n", card_filename);

  /* do a first run without a client to check that all is fine with the card */
  bool first_run = true;

  while (true) {
    int s = -1;

    if (!first_run) s = net_get_client(ip, port);

    memset(&card, 0, sizeof(card));

    init_card(&card, card_filename);
    reset_card(&card);
    get_atr(&card);
    open_usim(&card);
    init_usim(&card, pin);
    char imsi[16];
    if (!get_imsi(&card, imsi)) error("cannot get IMSI\n");
    else info("IMSI %s\n", imsi);

    if (!first_run) run_client(&card, s);

    close_card(&card);

    first_run = false;
  }

  return 0;
}
