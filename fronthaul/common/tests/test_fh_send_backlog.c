/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// fh_send_ordered() on a NIC that refuses packets: a net_ring port whose 7-packet ring stands in
// for the TX queue, drained by the test.

#include "fh_send.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_eth_ring.h>
#include <rte_mbuf.h>
#include <rte_ring.h>

#define NIC_RING_SIZE 8 // holds 7

static struct rte_mempool *pool;
static struct rte_ring *nic;

static void send_numbered(fh_send_t *send, uint32_t first, uint32_t n)
{
  struct rte_mbuf *mbufs[64];
  for (uint32_t i = 0; i < n; i++) {
    mbufs[i] = rte_pktmbuf_alloc(pool);
    assert(mbufs[i]);
    *rte_pktmbuf_mtod(mbufs[i], uint32_t *) = first + i;
    rte_pktmbuf_append(mbufs[i], sizeof(uint32_t));
  }
  fh_send_ordered(send, mbufs, n);
}

// Takes what the NIC holds and checks it continues the sequence from *next
static uint32_t drain_nic(uint32_t *next)
{
  struct rte_mbuf *m = NULL;
  uint32_t n = 0;
  while (rte_ring_dequeue(nic, (void **)&m) == 0) {
    assert(*rte_pktmbuf_mtod(m, uint32_t *) == *next);
    (*next)++;
    n++;
    rte_pktmbuf_free(m);
  }
  return n;
}

int main(int argc, char **argv)
{
  assert(rte_eal_init(argc, argv) >= 0);
  pool = rte_pktmbuf_pool_create("fh_send_backlog", 4095, 0, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
  assert(pool);
  nic = rte_ring_create("fh_send_nic", NIC_RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
  assert(nic);
  int port = rte_eth_from_ring(nic);
  assert(port >= 0);
  struct rte_eth_conf conf = {0};
  assert(rte_eth_dev_configure(port, 1, 1, &conf) == 0);
  assert(rte_eth_rx_queue_setup(port, 0, NIC_RING_SIZE, rte_socket_id(), NULL, pool) == 0);
  assert(rte_eth_tx_queue_setup(port, 0, NIC_RING_SIZE, rte_socket_id(), NULL) == 0);
  assert(rte_eth_dev_start(port) == 0);

  fh_timer_t timer;
  assert(fh_timer_init(&timer, 1) == 0);
  fh_send_t send;
  assert(fh_send_init(&send, &timer, port, 0) == 0);
  uint32_t next = 0;

  printf("Refused packets are kept and go out in order...\n");
  send_numbered(&send, 0, 10); // NIC takes 7
  assert(send.backlog_count == 3 && send.backlog_queued == 3);
  assert(drain_nic(&next) == 7);
  fh_send_flush(&send);
  assert(send.backlog_count == 0);
  assert(drain_nic(&next) == 3 && next == 10);

  printf("New packets go after the backlog...\n");
  send_numbered(&send, 10, 10); // NIC takes 7, 3 kept
  send_numbered(&send, 20, 2); // NIC full: both kept behind the 3
  assert(send.backlog_count == 5);
  assert(drain_nic(&next) == 7);
  send_numbered(&send, 22, 1); // the 5 first, then this one
  assert(send.backlog_count == 0);
  assert(drain_nic(&next) == 6 && next == 23);

  printf("A full backlog frees what does not fit...\n");
  for (uint32_t i = 0; i < (FH_SEND_BACKLOG_SIZE + 7) / 64 + 1; i++)
    send_numbered(&send, 23 + i * 64, 64);
  assert(send.backlog_count == FH_SEND_BACKLOG_SIZE);
  assert(send.backlog_full > 0);
  fh_send_cleanup(&send);
  next = 23;
  assert(drain_nic(&next) == 7);
  assert(rte_mempool_avail_count(pool) == 4095);

  printf("SUCCESS: fh_send backlog test complete.\n");
  return 0;
}
