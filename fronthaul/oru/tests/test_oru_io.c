/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "oru_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include "log.h"
#include "common/config/config_userapi.h"

#define NUM_MBUFS 1024
#define MBUF_CACHE_SIZE 250

configmodule_interface_t *uniqCfg = NULL;

void timer_callback(uint64_t s_abs, void *user_data)
{
  if (s_abs % 10000 == 0) {
    printf("Timer callback: s_abs=%lu\n", s_abs);
  }
}

void rx_callback(struct rte_mbuf **pkts, uint16_t n, void *user_data)
{
  static uint64_t total_rx = 0;
  total_rx += n;
  if (total_rx % 100000 == 0) {
    printf("RU RX: Received %u pkts. Total: %lu\n", n, total_rx);
  }
  for (uint16_t i = 0; i < n; i++)
    rte_pktmbuf_free(pkts[i]);
}

int main(int argc, char **argv)
{
  int ret = rte_eal_init(argc, argv);
  if (ret < 0)
    exit(1);

  logInit();

  uint16_t port0 = 0;
  uint16_t port1 = 1;

  if (!rte_eth_dev_is_valid_port(port0) || !rte_eth_dev_is_valid_port(port1)) {
    fprintf(stderr, "Two valid eth ports NOT found. Please run with --vdev=net_null0 --vdev=net_null1\n");
    return 1;
  }

  oru_io_config_t config = {.port_ids = {port0, port1},
                            .num_ports = 2,
                            .numerology = 1, // 30kHz
                            .num_macs = 2,
                            .rx_cb = rx_callback,
                            .rx_user_data = NULL,
                            .timer_cb = timer_callback,
                            .timer_user_data = NULL,
                            .mbuf_data_room = 2000,
                            .mbuf_count = 1024};
  rte_eth_random_addr(config.du_macs[0].addr_bytes);
  rte_eth_random_addr(config.du_macs[1].addr_bytes);

  oru_io_t oru_io;
  if (oru_io_init(&oru_io, &config) < 0) {
    fprintf(stderr, "Failed to initialize ru_fh\n");
    return 1;
  }

  unsigned int worker_lcore = rte_get_next_lcore(-1, 1, 0);

  printf("Starting Radio Unit on lcore %u...\n", worker_lcore);
  oru_io_run(&oru_io, worker_lcore);

  // Simulate some U-Plane traffic scheduling
  for (int i = 0; i < 10; i++) {
    struct rte_mbuf *m = oru_io_get_sendbuf(&oru_io);
    if (m) {
      // Need headroom for Ethernet header (14 bytes)
      if (rte_pktmbuf_headroom(m) < 14) {
        fprintf(stderr, "Insufficient headroom for ethernet header!\n");
      } else {
        oru_io_send_uplane(&oru_io, &m, 1);
      }
    }
    usleep(1000);
  }

  printf("-----------------------------------------\n\n");

  sleep(2);

  printf("Stopping Radio Unit...\n");
  oru_io_stop(&oru_io);
  rte_eal_wait_lcore(worker_lcore);

  oru_io_cleanup(&oru_io);
  printf("Test Complete.\n");

  return 0;
}
