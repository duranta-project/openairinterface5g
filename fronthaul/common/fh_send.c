/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "fh_send.h"
#include <stdio.h>
#include <stdlib.h>
#include <rte_ethdev.h>
#include <rte_errno.h>
#include <rte_atomic.h>

int fh_send_init(fh_send_t *send, fh_timer_t *timer, uint16_t port_id, uint16_t queue_id)
{
  send->timer = timer;
  send->port_id = port_id;
  send->queue_id = queue_id;
  rte_spinlock_init(&send->tx_lock);
  send->backlog_head = 0;
  send->backlog_count = 0;
  send->backlog_queued = 0;
  send->backlog_full = 0;

  return 0;
}

uint16_t fh_send_immediate(fh_send_t *send, struct rte_mbuf **mbufs, uint32_t n)
{
  if (!send)
    return 0;
  rte_spinlock_lock(&send->tx_lock);
  uint16_t sent = rte_eth_tx_burst(send->port_id, send->queue_id, mbufs, (uint16_t)n);
  rte_spinlock_unlock(&send->tx_lock);
  return sent;
}

// Sends from the backlog head until it is empty or the NIC refuses. Called under tx_lock.
static void flush_backlog(fh_send_t *send)
{
  while (send->backlog_count > 0) {
    uint32_t n = RTE_MIN(send->backlog_count, FH_SEND_BACKLOG_SIZE - send->backlog_head);
    uint16_t sent = rte_eth_tx_burst(send->port_id, send->queue_id, &send->backlog[send->backlog_head], (uint16_t)n);
    send->backlog_head = (send->backlog_head + sent) % FH_SEND_BACKLOG_SIZE;
    send->backlog_count -= sent;
    if (sent < n)
      return;
  }
}

void fh_send_ordered(fh_send_t *send, struct rte_mbuf **mbufs, unsigned int n)
{
  rte_spinlock_lock(&send->tx_lock);
  flush_backlog(send);
  unsigned int sent = 0;
  if (send->backlog_count == 0)
    sent = rte_eth_tx_burst(send->port_id, send->queue_id, mbufs, (uint16_t)n);
  for (unsigned int i = sent; i < n; i++) {
    if (send->backlog_count == FH_SEND_BACKLOG_SIZE) {
      rte_pktmbuf_free(mbufs[i]);
      send->backlog_full++;
      continue;
    }
    send->backlog[(send->backlog_head + send->backlog_count) % FH_SEND_BACKLOG_SIZE] = mbufs[i];
    send->backlog_count++;
    send->backlog_queued++;
  }
  rte_spinlock_unlock(&send->tx_lock);
}

void fh_send_flush(fh_send_t *send)
{
  // Unlocked peek: the poll loop checks again on its next iteration
  if (__atomic_load_n(&send->backlog_count, __ATOMIC_RELAXED) == 0)
    return;
  rte_spinlock_lock(&send->tx_lock);
  flush_backlog(send);
  rte_spinlock_unlock(&send->tx_lock);
}

void fh_send_cleanup(fh_send_t *send)
{
  rte_spinlock_lock(&send->tx_lock);
  for (; send->backlog_count > 0; send->backlog_count--) {
    rte_pktmbuf_free(send->backlog[send->backlog_head]);
    send->backlog_head = (send->backlog_head + 1) % FH_SEND_BACKLOG_SIZE;
  }
  rte_spinlock_unlock(&send->tx_lock);
}
