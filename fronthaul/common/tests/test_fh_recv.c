/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "fh_timer.h"
#include "fh_recv.h"
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

void rx_callback(struct rte_mbuf **pkts,
                 uint16_t n,
                 void *user_data)
{
  static uint64_t total_rx = 0;
  total_rx += n;
  if (total_rx % 1000 == 0) {
    printf("RX Callback: Received %u pkts. Total: %lu\n", n, total_rx);
  }
  for (uint16_t i = 0; i < n; i++)
    rte_pktmbuf_free(pkts[i]);
}

int setup_port(uint16_t port_id, struct rte_mempool *mp)
{
  struct rte_eth_conf port_conf = {0};
  uint16_t nb_rxd = 1024;
  uint16_t nb_txd = 1024;

  int ret = rte_eth_dev_configure(port_id, 1, 1, &port_conf);
  if (ret < 0)
    return ret;

  ret = rte_eth_rx_queue_setup(port_id, 0, nb_rxd, rte_eth_dev_socket_id(port_id), NULL, mp);
  if (ret < 0)
    return ret;

  ret = rte_eth_tx_queue_setup(port_id, 0, nb_txd, rte_eth_dev_socket_id(port_id), NULL);
  if (ret < 0)
    return ret;

  return rte_eth_dev_start(port_id);
}

int main(int argc, char **argv)
{
  int ret = rte_eal_init(argc, argv);
  if (ret < 0)
    exit(1);

  uint16_t port0 = 0;
  uint16_t port1 = 1;

  if (!rte_eth_dev_is_valid_port(port0) || !rte_eth_dev_is_valid_port(port1)) {
    fprintf(stderr, "Two valid eth ports required. Use --vdev=net_null0 --vdev=net_null1\n");
    return 1;
  }

  struct rte_mempool *mp =
      rte_pktmbuf_pool_create("RX_POOL_RECV", NUM_MBUFS, MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
  if (!mp)
    return 1;

  if (setup_port(port0, mp) < 0 || setup_port(port1, mp) < 0)
    return 1;

  fh_timer_t timer;
  if (fh_timer_init(&timer, 1) < 0)
    return 1;

  fh_recv_q_conf_t q_confs[] = {{.port_id = port0, .queue_id = 0}, {.port_id = port1, .queue_id = 0}};

  fh_recv_t recv;
  if (fh_recv_init(&recv, &timer, q_confs, 2, rx_callback, NULL) < 0)
    return 1;

  unsigned int worker_lcore = rte_get_next_lcore(-1, 1, 0);

  if (worker_lcore == RTE_MAX_LCORE) {
    fprintf(stderr, "Not enough lcores. Need -c 0x3\n");
    return 1;
  }

  printf("Starting Receiver (with integrated Timer) on lcore %u\n", worker_lcore);
  rte_eal_remote_launch(fh_recv_run, &recv, worker_lcore);

  sleep(2);

  printf("Stopping test...\n");
  fh_recv_stop(&recv);
  fh_timer_stop(&timer);

  rte_eal_wait_lcore(worker_lcore);
  printf("SUCCESS: fh_recv test complete.\n");

  return 0;
}
