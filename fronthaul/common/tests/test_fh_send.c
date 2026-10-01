/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "fh_timer.h"
#include "fh_send.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_launch.h>
#include <rte_lcore.h>

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250

int main(int argc, char **argv)
{
  int ret = rte_eal_init(argc, argv);
  if (ret < 0)
    exit(1);

  uint16_t port_id = 0;
  if (!rte_eth_dev_is_valid_port(port_id)) {
    fprintf(stderr, "No valid eth port found. Please run with --vdev=net_null0\n");
    return 1;
  }

  struct rte_eth_conf port_conf = {0};
  rte_eth_dev_configure(port_id, 1, 1, &port_conf);
  rte_eth_tx_queue_setup(port_id, 0, 1024, rte_eth_dev_socket_id(port_id), NULL);
  rte_eth_dev_start(port_id);

  struct rte_mempool *mbuf_pool =
      rte_pktmbuf_pool_create("MBUF_POOL_SEND", NUM_MBUFS, MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
  if (!mbuf_pool) {
    fprintf(stderr, "Cannot create mbuf pool\n");
    return 1;
  }

  fh_timer_t timer;
  if (fh_timer_init(&timer, 1) < 0)
    return 1;

  fh_send_t send;
  if (fh_send_init(&send, &timer, port_id, 0) < 0)
    return 1;

  // Test immediate transmission directly
  struct rte_mbuf *m_imm = rte_pktmbuf_alloc(mbuf_pool);
  if (m_imm) {
    printf("Testing immediate transmission directly...\n");
    uint16_t sent = fh_send_immediate(&send, &m_imm, 1);
    printf("Immediate Sent: %u\n", sent);
  }

  printf("SUCCESS: fh_send test complete.\n");
  return 0;
}
