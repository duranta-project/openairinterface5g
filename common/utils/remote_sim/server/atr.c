/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "atr.h"

#include <stdio.h>
#include <stdlib.h>

#include "pl2303.h"
#include "log.h"

void get_atr(card_t *card)
{
  /* ISO 7816-3 8.1 */
  unsigned char ts = get_byte(card, DEFAULT_TIMEOUT);
  if (ts != 0x3b) { error("only direct convention supported\n"); exit(1); }
  /* ISO 7816-3 8.2 */
  unsigned char t0 = get_byte(card, DEFAULT_TIMEOUT);
  int y1 = t0 >> 4;
  int k = t0 & 0x0f;

  /* we don't care of value, just parsing for correct format */
  /* max size of ATR is 32 bytes (counting from T0, not TS) ISO 7816-3 8.2.1
   * so we won't have more than 32 TA, TB, TC, TD */
  int ta[32];
  int tb[32];
  int tc[32];
  int td[32];
  /* init to -1 to mean "not there" */
  for (int i = 0; i < 32; i++) ta[i] = tb[i] = tc[i] = td[i] = -1;

  int y = y1;
  int y_pos = 0;

  do {
    /* get ta, if there */
    if (y & 0x01) ta[y_pos] = get_byte(card, DEFAULT_TIMEOUT);
    /* get tb, if there */
    if (y & 0x02) tb[y_pos] = get_byte(card, DEFAULT_TIMEOUT);
    /* get tc, if there */
    if (y & 0x04) tc[y_pos] = get_byte(card, DEFAULT_TIMEOUT);
    /* get td, if there (if not, y for next turn = 0) */
    if (y & 0x08) {
      td[y_pos] = get_byte(card, DEFAULT_TIMEOUT);
      y = td[y_pos] >> 4;
    } else
      y = 0;
    y_pos++;
  } while (y);

  /* check that all td[i] are in increasing order */
  int last = -1;
  int first = -1;
  for (int i = 0; i < 32; i++) {
    if (td[i] == -1) break;
    int t = td[i] & 0x0f;
    if (first == -1) first = t;
    if (t < last) { error("bad ATR, T not in increasing order\n"); exit(1); }
    last = t;
  }

  /* get the historical bytes (if any), max 15 bytes ISO 7816-3 8.2.4 */
  int historical_bytes[15];
  for (int i = 0; i < k; i++) historical_bytes[i] = get_byte(card, DEFAULT_TIMEOUT);

  /* get TCK if it is there
   * it is absent if t==0 is sent and t==15 is not sent, present otherwise */
  unsigned char tck = 0;           /* un-necessary init but the compiler complains... */
  int no_tck = last == -1 || (first == 0 && last < 15);
  debug("TCK present? %s\n", no_tck ? "no" : "yes");
  if (!no_tck) {
    tck = get_byte(card, DEFAULT_TIMEOUT);
    /* check tck */
    unsigned char xor = t0;
    for (int i = 0; i < 32; i++) {
      if (ta[i] != -1) xor ^= ta[i];
      if (tb[i] != -1) xor ^= tb[i];
      if (tc[i] != -1) xor ^= tc[i];
      if (td[i] != -1) xor ^= td[i];
    }
    for (int i = 0; i < k; i++)
      xor ^= historical_bytes[i];
    xor ^= tck;
    if (xor) { error("bad TCK\n"); exit(1); }
    else debug("good TCK\n");
  }

  /* TD1 must not be 15 (ISO 7816-3 8.2.3) */
  if (td[0] == 15) { error("TD1 is 15, illegal\n"); exit(1); }

  /* dump it all */
  /* one line */
  debug("  %2.2x %2.2x", ts, t0);
  for (int i = 0; i < 32; i++) {
    if (ta[i] != -1) debug(" %2.2x", ta[i]);
    if (tb[i] != -1) debug(" %2.2x", tb[i]);
    if (tc[i] != -1) debug(" %2.2x", tc[i]);
    if (td[i] != -1) debug(" %2.2x", td[i]);
  }
  for (int i = 0; i < k; i++) debug(" %2.2x", historical_bytes[i]);
  if (!no_tck) debug(" %2.2x", tck);
  debug("\n");

  /* name + value */
  debug("ts %2.2x\n", ts);
  debug("t0 %2.2x\n", t0);
  for (int i = 0; i < 32; i++) if (ta[i] != -1) debug("ta[%d] %2.2x\n", i + 1, ta[i]);
  for (int i = 0; i < 32; i++) if (tb[i] != -1) debug("tb[%d] %2.2x\n", i + 1, tb[i]);
  for (int i = 0; i < 32; i++) if (tc[i] != -1) debug("tc[%d] %2.2x\n", i + 1, tc[i]);
  for (int i = 0; i < 32; i++) if (td[i] != -1) debug("td[%d] %2.2x\n", i + 1, td[i]);
  for (int i = 0; i < k; i++) debug("historical_bytes[%d] %2.2x\n", i + 1, historical_bytes[i]);
  if (!no_tck) debug("TCK %2.2x\n", tck);

  /* TA2 (ISO 7816-3 8.3) may force the use of T != 0 which is the only one we
   * support.
   * Let's warn if there is TA2, and cross fingers that things will work
   * (anyway, if not, later processing will fail, no big deal)
   * (yes, TA2 is ta[1])
   */
  if (ta[1] != -1)
    error("warning: TA2 present, hex value %2.2x, processing may fail\n", ta[1]);
}
