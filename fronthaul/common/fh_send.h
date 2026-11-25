/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FH_SEND_H
#define FH_SEND_H

#include <stdint.h>
#include <rte_ring.h>
#include <rte_mbuf.h>
#include <rte_spinlock.h>
#include <rte_atomic.h>
#include "fh_timer.h"

// Packets the NIC refused, kept for fh_send_ordered() and fh_send_flush(): a few symbols' worth
#define FH_SEND_BACKLOG_SIZE 1024

typedef struct {
  fh_timer_t *timer;
  uint16_t port_id;
  uint16_t queue_id;
  rte_spinlock_t tx_lock;

  // FIFO of refused packets, in send order; under tx_lock
  struct rte_mbuf *backlog[FH_SEND_BACKLOG_SIZE];
  uint32_t backlog_head;
  uint32_t backlog_count;
  uint64_t backlog_queued; // refused by the NIC, kept for a retry
  uint64_t backlog_full; // refused by the NIC with the backlog full, freed
} fh_send_t;

/**
 * @brief Initialize fh_send library.
 * Creates one rte_ring per symbol in a 10ms frame.
 * Registers a callback with the provided fh_timer.
 *
 * @param send Pointer to fh_send handle
 * @param timer Pointer to initialized fh_timer handle
 * @param port_id DPDK ethernet port ID
 * @param queue_id DPDK TX queue ID
 * @return int 0 on success, negative on error
 */
int fh_send_init(fh_send_t *send, fh_timer_t *timer, uint16_t port_id, uint16_t queue_id);

/**
 * @brief Transmit packets immediately, bypassing scheduling.
 * This function is thread-safe and can be called from any lcore.
 *
 * @param send Pointer to fh_send handle
 * @param mbufs Array of packets to send
 * @param n Number of packets
 * @return uint16_t Number of packets actually transmitted
 */
uint16_t fh_send_immediate(fh_send_t *send, struct rte_mbuf **mbufs, unsigned int n);

// Sends the packets after any the NIC refused earlier, and keeps the ones it refuses now in the
// backlog, in order. Takes every mbuf: one that does not fit in the backlog is freed.
void fh_send_ordered(fh_send_t *send, struct rte_mbuf **mbufs, unsigned int n);

// Retries the backlog. Called from the poll loop, so refused packets go out as soon as the NIC
// takes them rather than at the next symbol.
void fh_send_flush(fh_send_t *send);

// Frees the backlog.
void fh_send_cleanup(fh_send_t *send);

#endif /* FH_SEND_H */
