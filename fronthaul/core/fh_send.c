/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "fh_send.h"
#include <stdio.h>
#include <stdlib.h>
#include <rte_ethdev.h>
#include <rte_errno.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_mbuf.h>

/* How long a partially accepted burst is retried before the unsent packets are dropped. */
#define FH_SEND_RETRY_US 100

int fh_send_init(fh_send_t *send, fh_timer_t *timer, uint16_t port_id, uint16_t queue_id)
{
  send->timer = timer;
  send->port_id = port_id;
  send->queue_id = queue_id;
  rte_spinlock_init(&send->tx_lock);

  return 0;
}

uint16_t fh_send_immediate(fh_send_t *send, struct rte_mbuf **mbufs, uint32_t n)
{
  if (!send)
    return 0;
  rte_spinlock_lock(&send->tx_lock);
  uint16_t sent = rte_eth_tx_burst(send->port_id, send->queue_id, mbufs, (uint16_t)n);
  if (sent < n) {
    /* The device queue is congested (seen with the AF_PACKET PMD on a veth). Retry the unsent suffix
     * for a bounded time so that consecutive packets stay in order on the wire. */
    const uint64_t deadline = rte_get_timer_cycles() + rte_get_timer_hz() * FH_SEND_RETRY_US / 1000000ULL;
    while (sent < n && rte_get_timer_cycles() < deadline) {
      rte_pause();
      sent += rte_eth_tx_burst(send->port_id, send->queue_id, mbufs + sent, (uint16_t)(n - sent));
    }
  }
  rte_spinlock_unlock(&send->tx_lock);
  for (uint32_t i = sent; i < n; i++)
    rte_pktmbuf_free(mbufs[i]);
  return sent;
}
