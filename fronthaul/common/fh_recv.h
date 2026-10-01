/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FH_RECV_H
#define FH_RECV_H

#include <stdint.h>
#include <stdbool.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include "fh_timer.h"

typedef void (*fh_recv_cb)(struct rte_mbuf **pkts, uint16_t n, void *user_data);

#define MAX_FH_RECV_QUEUES 2

typedef struct {
  uint16_t port_id;
  uint16_t queue_id;
} fh_recv_q_conf_t;

typedef struct {
  fh_timer_t *timer;
  fh_recv_q_conf_t qs[MAX_FH_RECV_QUEUES];
  uint16_t num_qs;
  fh_recv_cb cb;
  void *user_data;
  volatile bool running;
} fh_recv_t;

/**
 * @brief Initialize fh_recv library.
 * Merges traffic from multiple DPDK interfaces into a single stream.
 *
 * @param recv Pointer to fh_recv handle
 * @param timer Pointer to initialized fh_timer handle
 * @param q_confs Array of port/queue configurations
 * @param num_qs Number of configurations (max 2)
 * @param cb Callback function
 * @param user_data User data for callback
 * @return int 0 on success, negative on error
 */
int fh_recv_init(fh_recv_t *recv, fh_timer_t *timer, fh_recv_q_conf_t *q_confs, uint16_t num_qs, fh_recv_cb cb, void *user_data);

/**
 * @brief Run the receiver polling loop.
 * This is a blocking function and should be called from a dedicated DPDK lcore.
 *
 * @param arg Pointer to initialized fh_recv handle
 * @return int 0 on normal exit
 */
int fh_recv_run(void *arg);

/**
 * @brief Stop the receiver loop.
 *
 * @param recv Pointer to fh_recv handle
 */
void fh_recv_stop(fh_recv_t *recv);

#endif /* FH_RECV_H */
