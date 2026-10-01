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

typedef struct {
  fh_timer_t *timer;
  uint16_t port_id;
  uint16_t queue_id;
  rte_spinlock_t tx_lock;
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

#endif /* FH_SEND_H */
