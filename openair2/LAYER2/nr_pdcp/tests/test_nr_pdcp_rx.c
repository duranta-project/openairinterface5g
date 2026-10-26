/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* PDCP RX reordering (TS 38.323 §5.2.2): PDUs received out of order, duplicated
 * or lost must be delivered in COUNT order, each at most once. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "LAYER2/nr_pdcp/nr_pdcp_entity.h"
#include "common/utils/LOG/log.h"

#define T_REORDERING_MS 100

static uint32_t *out;
static int nout;

/* each SDU carries its COUNT as payload */
static void deliver_sdu(void *data, nr_pdcp_entity_t *e, char *buf, int size, const nr_pdcp_integrity_data_t *msg_integrity)
{
  uint32_t count;
  memcpy(&count, buf, sizeof(count));
  out[nout++] = count;
}

static void deliver_pdu(void *data, ue_id_t ue_id, int rb_id, char *buf, int size, int sdu_id)
{
}

static nr_pdcp_entity_t *new_drb(int sn_size)
{
  nr_pdcp_entity_security_keys_and_algos_t sec = {0};
  nout = 0;
  nr_pdcp_entity_t *e = new_nr_pdcp_entity(NR_PDCP_DRB_AM,
                                           0,
                                           1,
                                           1,
                                           false,
                                           false,
                                           deliver_sdu,
                                           NULL,
                                           deliver_pdu,
                                           NULL,
                                           sn_size,
                                           T_REORDERING_MS,
                                           -1,
                                           &sec);
  e->set_time(e, 1);
  return e;
}

/* TS 38.323 §6.2.2.2 (12-bit SN) and §6.2.2.3 (18-bit SN) data PDU headers */
static void send(nr_pdcp_entity_t *e, int sn_size, uint32_t count)
{
  unsigned char pdu[3 + 64] = {0};
  uint32_t sn = count & ((1 << sn_size) - 1);
  int hdr;
  if (sn_size == 12) {
    pdu[0] = 0x80 | (sn >> 8);
    pdu[1] = sn & 0xff;
    hdr = 2;
  } else {
    pdu[0] = 0x80 | (sn >> 16);
    pdu[1] = (sn >> 8) & 0xff;
    pdu[2] = sn & 0xff;
    hdr = 3;
  }
  memcpy(pdu + hdr, &count, sizeof(count));
  e->recv_pdu(e, (char *)pdu, hdr + 64);
}

/* expects out[] == [0, n) without the counts in skip[] */
static int check(const char *name, int n, const uint32_t *skip, int nskip)
{
  int exp = 0;
  int s = 0;
  for (int i = 0; i < nout; i++, exp++) {
    while (s < nskip && (uint32_t)exp == skip[s]) {
      exp++;
      s++;
    }
    if (out[i] != (uint32_t)exp) {
      printf("%s: out[%d] = %u, expected %d\n", name, i, out[i], exp);
      return 1;
    }
  }
  if (nout != n - nskip) {
    printf("%s: delivered %d SDUs, expected %d\n", name, nout, n - nskip);
    return 1;
  }
  printf("%s: OK\n", name);
  return 0;
}

/* blocks of 16 shuffled, one duplicate per block, one COUNT lost for good */
static int test_shuffled(int sn_size, int n)
{
  const uint32_t lost = 1000;
  nr_pdcp_entity_t *e = new_drb(sn_size);
  uint64_t t = 1;
  for (int b = 0; b < n; b += 16) {
    int idx[16];
    for (int i = 0; i < 16; i++)
      idx[i] = b + i;
    for (int i = 15; i > 0; i--) {
      int j = rand() % (i + 1);
      int x = idx[i];
      idx[i] = idx[j];
      idx[j] = x;
    }
    for (int i = 0; i < 16; i++)
      if (idx[i] != lost)
        send(e, sn_size, idx[i]);
    send(e, sn_size, b + rand() % 16);
    /* 1 ms per block: t-Reordering expires 1600 COUNTs after the loss, inside
     * the 12-bit SN window (2048) */
    e->set_time(e, ++t);
  }
  /* expire t-Reordering for the lost COUNT */
  e->set_time(e, t + 2 * T_REORDERING_MS);
  char name[64];
  snprintf(name, sizeof(name), "shuffled, %d-bit SN", sn_size);
  int ret = check(name, n, &lost, 1);
  e->delete_entity(e);
  return ret;
}

/* two gaps; the later one is filled first, in reverse, so every filler is
 * inserted in the middle of rx_list */
static int test_gap_in_middle(void)
{
  const int n = 200;
  nr_pdcp_entity_t *e = new_drb(18);
  for (int c = 0; c < n; c++)
    if (c != 10 && (c < 100 || c >= 120))
      send(e, 18, c);
  for (int c = 119; c >= 100; c--)
    send(e, 18, c);
  int ret = nout == 10 ? 0 : 1;
  if (ret)
    printf("gap in middle: delivered %d SDUs before the first gap was filled, expected 10\n", nout);
  send(e, 18, 10);
  ret |= check("gap in middle", n, NULL, 0);
  e->delete_entity(e);
  return ret;
}

/* every SDU waits in rx_list until COUNT 0 arrives last */
static int test_gap_at_head(int n)
{
  nr_pdcp_entity_t *e = new_drb(18);
  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  for (int c = 1; c < n; c++)
    send(e, 18, c);
  send(e, 18, 0);
  clock_gettime(CLOCK_MONOTONIC, &t1);
  double us = ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / 1e3;
  printf("gap at head: %d PDUs, %.3f us/PDU (informational)\n", n, us / n);
  int ret = check("gap at head", n, NULL, 0);
  e->delete_entity(e);
  return ret;
}

int main(void)
{
  logInit();
  set_glog(OAILOG_DISABLE);
  srand(1);

  const int n = 50000;
  out = calloc(n, sizeof(*out));

  int ret = 0;
  ret |= test_shuffled(18, n);
  /* 12-bit SN wraps the HFN several times */
  ret |= test_shuffled(12, n);
  ret |= test_gap_in_middle();
  ret |= test_gap_at_head(n);

  free(out);
  return ret;
}
