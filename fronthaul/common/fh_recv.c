/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "fh_recv.h"
#include <stdio.h>
#include <stdlib.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_errno.h>

#define RX_BURST_SIZE 32
#define RING_SIZE 2048

int fh_recv_init(fh_recv_t *recv, fh_timer_t *timer, fh_recv_q_conf_t *q_confs, uint16_t num_qs, fh_recv_cb cb, void *user_data)
{
  if (!recv || !timer || !q_confs || num_qs == 0 || num_qs > MAX_FH_RECV_QUEUES)
    return -1;

  recv->timer = timer;
  recv->num_qs = num_qs;
  for (uint16_t i = 0; i < num_qs; i++) {
    recv->qs[i] = q_confs[i];
  }
  recv->cb = cb;
  recv->user_data = user_data;
  recv->running = false;

  return 0;
}

int fh_recv_run(void *arg)
{
  fh_recv_t *recv = (fh_recv_t *)arg;
  struct rte_mbuf *pkts[RX_BURST_SIZE * 2];

  recv->running = true;
  printf("FH Receiver Loop Started on lcore %u\n", rte_lcore_id());

  while (recv->running) {
    uint16_t total_rx = 0;

    for (uint16_t i = 0; i < recv->num_qs; i++) {
      uint16_t nb_rx = rte_eth_rx_burst(recv->qs[i].port_id, recv->qs[i].queue_id, pkts + total_rx, RX_BURST_SIZE);
      total_rx += nb_rx;
    }

    if (total_rx > 0) {
      recv->cb(pkts, total_rx, recv->user_data);
    } else {
      rte_pause();
    }

    fh_timer_tick(recv->timer);
  }

  return 0;
}

void fh_recv_stop(fh_recv_t *recv)
{
  if (recv)
    recv->running = false;
}
