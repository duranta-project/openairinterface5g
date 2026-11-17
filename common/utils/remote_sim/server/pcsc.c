/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "pcsc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef PCSC
#include <PCSC/winscard.h>
#endif

#include "def.h"
#include "log.h"

#define CHECK(f, rv) do { \
    if (rv != SCARD_S_SUCCESS) { \
      error(f ": %s\n", pcsc_stringify_error(rv)); \
      exit(1); \
    } \
  } while (0)

typedef struct {
  char *name;
  bool has_card;
} reader_t;

#ifdef PCSC
static SCARDCONTEXT hContext;
static SCARDHANDLE hCard;
static LPTSTR mszReaders;
static SCARD_IO_REQUEST pioSendPci;
#endif

static reader_t *readers;
static int readers_count;

/* pass pcsc_card_id -1 to print available cards and exit */
void init_card_pcsc(card_t *card, int pcsc_card_id)
{
#ifdef PCSC
  LONG rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
  CHECK("SCardEstablishContext", rv);

  DWORD dwReaders = SCARD_AUTOALLOCATE;
  rv = SCardListReaders(hContext, NULL, (LPTSTR)&mszReaders, &dwReaders);
  CHECK("SCardListReaders", rv);

  /* SCardListReaders() returns [reader 0 name] ... [reader n name] NULL
   * with each name NULL-Z terminated
   * let's parse this, to get number of readers
   */
  readers_count = 0;
  char *r = mszReaders;
  int l = dwReaders;
  while (l && *r) {
    readers_count++;
    while (l && *r) { r++; l--; }
    if (!l) { error("SCardListReaders() failed\n"); exit(1); }
    r++;
    l--;
  }
  if (l != 1 || *r) { error("SCardListReaders() failed\n"); exit(1); }
  debug("%d PC/SC readers\n", readers_count);

  if (!readers_count) { error("no PC/SC reader found\n"); exit(1); }

  readers = calloc(readers_count, sizeof(reader_t));
  if (!readers) { error("out of memory\n"); exit(1); }

  /* and now parse again to populate readers' names (no need for checks this time) */
  r = mszReaders;
  int i = 0;
  while (*r) {
    readers[i].name = strdup(r);
    if (!readers[i].name) { error("out of memory\n"); exit(1); }
    i++;
    while (*r) r++;
    r++;
  }

  /* get readers' state */
  SCARD_READERSTATE rgReaderStates[readers_count];
  for (int i = 0; i < readers_count; i++) {
    rgReaderStates[i].szReader = readers[i].name;
    rgReaderStates[i].dwCurrentState = SCARD_STATE_UNAWARE;
    rgReaderStates[i].cbAtr = sizeof(rgReaderStates[i].rgbAtr);
  }

  rv = SCardGetStatusChange(hContext, 3600*1000, rgReaderStates, readers_count);
  CHECK("SCardGetStatusChange", rv);

  for (int i = 0; i < readers_count; i++)
    debug("card %d state %lx (%lx)\n", i, rgReaderStates[i].dwCurrentState, rgReaderStates[i].dwEventState);

  /* extract card presence from state for each reader */
  for (int i = 0; i < readers_count; i++)
    readers[i].has_card = (rgReaderStates[i].dwEventState & SCARD_STATE_PRESENT) != 0;

  debug("%d PCSC readers found\n", readers_count);
  for (int i = 0; i < readers_count; i++) {
    debug("reader %d [%s]: %s\n",
          i,
          readers[i].has_card ? "SIM inserted" : "no SIM",
          readers[i].name);
  }

  /* passing pcsc_card_id == -1 means to output status for the cards and exit */
  if (pcsc_card_id == -1) {
    info("%d PCSC readers found\n", readers_count);
    for (int i = 0; i < readers_count; i++) {
      info("reader %d [%s]: %s\n",
           i,
           readers[i].has_card ? "SIM inserted" : "no SIM",
           readers[i].name);
    }
    exit(0);
  }

  if (pcsc_card_id < 0 || pcsc_card_id >= readers_count)
    { error("bad card ID %d, rerun with -l to list known IDs\n", pcsc_card_id); exit(1); }

  info("PC/SC reader name: %s\n", readers[pcsc_card_id].name);

  DWORD dwActiveProtocol;
  rv = SCardConnect(hContext, readers[pcsc_card_id].name, SCARD_SHARE_EXCLUSIVE,
                    SCARD_PROTOCOL_T0, &hCard, &dwActiveProtocol);
  CHECK("SCardConnect", rv);

  if (dwActiveProtocol != SCARD_PROTOCOL_T0) { error("wrong protocol\n"); exit(1); }

  pioSendPci = *SCARD_PCI_T0;

  card->type = CARD_TYPE_PCSC;
#endif
}

void close_card_pcsc(card_t *c)
{
#ifdef PCSC
  LONG rv = SCardDisconnect(hCard, SCARD_LEAVE_CARD);
  CHECK("SCardDisconnect", rv);

  rv = SCardFreeMemory(hContext, mszReaders);
  CHECK("SCardFreeMemory", rv);

  rv = SCardReleaseContext(hContext);
  CHECK("SCardReleaseContext", rv);
#endif

  for (int i = 0; i < readers_count; i++) free(readers[i].name);
  free(readers);
  readers = NULL;
  readers_count = 0;
}

#ifdef PCSC
static void get_atr_pcsc(card_t *card)
{
  LPTSTR szReaderName;
  DWORD cchReaderLen = SCARD_AUTOALLOCATE;
  DWORD dwState;
  DWORD dwProtocol;
  DWORD atr_len = 256;
  LONG rv = SCardStatus(hCard, (LPTSTR)&szReaderName, &cchReaderLen,
                        &dwState, &dwProtocol, card->pcsc.atr, &atr_len);
  CHECK("SCardStatus", rv);

  card->pcsc.atr_length = atr_len;

  debug("ATR[%d]:", (int)atr_len);
  for (int i = 0; i < atr_len; i++) debug(" %2.2x", card->pcsc.atr[i]);
  debug("\n");

  rv = SCardFreeMemory(hContext, szReaderName);
  CHECK("SCardFreeMemory", rv);
}
#endif

void reset_card_pcsc(card_t *card)
{
#ifdef PCSC
  DWORD dwActiveProtocol;
  LONG rv = SCardReconnect(hCard, SCARD_SHARE_EXCLUSIVE, SCARD_PROTOCOL_T0,
                           SCARD_RESET_CARD, &dwActiveProtocol);
  CHECK("SCardReconnect", rv);

  if (dwActiveProtocol != SCARD_PROTOCOL_T0) { error("wrong protocol\n"); exit(1); }

  get_atr_pcsc(card);
#endif
}

int send_apdu_pcsc(card_t *card, unsigned char *apdu, int length,
                   unsigned char *resp, int resp_size)
{
#ifdef PCSC
  SCARD_IO_REQUEST pioRecvPci = *SCARD_PCI_T0;

  DWORD recv_len = resp_size;

  LONG rv = SCardTransmit(hCard, &pioSendPci, apdu, length, &pioRecvPci, resp, &recv_len);
  CHECK("SCardTransmit", rv);

  debug("SCardTransmit replies [%d]:", (int)recv_len);
  for (int i = 0; i < recv_len; i++) debug(" %2.2x", resp[i]);
  debug("\n");

  return recv_len;
#else
  return 0;
#endif
}
