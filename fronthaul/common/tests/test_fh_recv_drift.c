/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "fh_recv.h"
#include "fh_timer.h"
#include <math.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define GPS_EPOCH_OFFSET_UNIX 315964800ULL
#define NS_PER_SEC 1000000000ULL
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250

static uint64_t get_gps_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return ((uint64_t)ts.tv_sec - GPS_EPOCH_OFFSET_UNIX + GPS_LEAP_SECONDS) * NS_PER_SEC + ts.tv_nsec;
}

typedef struct {
  uint64_t total_callbacks;
  uint64_t sample_count;
  double sum_err;
  double sum_err_sq;
  double max_err;
  double min_err;
  uint64_t start_time;
  uint64_t end_time;
  double first_err;
  double last_err;
} drift_stats_t;

static drift_stats_t global_stats = {0, 0, 0, 0, -1e18, 1e18, 0, 0, 0, 0};

void drift_callback(uint64_t s_abs, void *user_data)
{
  fh_timer_t *timer = (fh_timer_t *)user_data;
  uint64_t now_ns = get_gps_ns();

  uint64_t syms_per_ms = 14 << timer->numerology;
  typedef __int128_t int128;
  uint64_t target_ns = (uint64_t)((int128)s_abs * 1000000ULL / syms_per_ms);

  double err = (double)now_ns - (double)target_ns;

  global_stats.total_callbacks++;

  // Discard first 1000 samples for startup stability
  if (global_stats.total_callbacks <= 1000) {
    return;
  }

  if (global_stats.sample_count == 0) {
    global_stats.start_time = now_ns;
    global_stats.first_err = err;
  }

  global_stats.end_time = now_ns;
  global_stats.last_err = err;
  global_stats.sample_count++;
  global_stats.sum_err += err;
  global_stats.sum_err_sq += (err * err);

  if (err > global_stats.max_err)
    global_stats.max_err = err;
  if (err < global_stats.min_err)
    global_stats.min_err = err;
}

void dummy_rx_cb(struct rte_mbuf **pkts, uint16_t n, void *user_data)
{
  for (uint16_t i = 0; i < n; i++)
    rte_pktmbuf_free(pkts[i]);
}

static int setup_port(uint16_t port_id, struct rte_mempool *mp)
{
  struct rte_eth_conf port_conf = {0};
  int ret = rte_eth_dev_configure(port_id, 1, 1, &port_conf);
  if (ret < 0)
    return ret;
  ret = rte_eth_rx_queue_setup(port_id, 0, 1024, rte_eth_dev_socket_id(port_id), NULL, mp);
  if (ret < 0)
    return ret;
  ret = rte_eth_tx_queue_setup(port_id, 0, 1024, rte_eth_dev_socket_id(port_id), NULL);
  if (ret < 0)
    return ret;
  return rte_eth_dev_start(port_id);
}

int main(int argc, char **argv)
{
  int ret = rte_eal_init(argc, argv);
  if (ret < 0)
    return 1;

  uint16_t port0 = 0;
  uint16_t port1 = 1;

  if (!rte_eth_dev_is_valid_port(port0) || !rte_eth_dev_is_valid_port(port1)) {
    fprintf(stderr, "Two valid eth ports required. Use --vdev=net_null0 --vdev=net_null1\n");
    return 1;
  }

  struct rte_mempool *mp =
      rte_pktmbuf_pool_create("RX_DRIFT_POOL", NUM_MBUFS, MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
  if (!mp)
    return 1;

  if (setup_port(port0, mp) < 0 || setup_port(port1, mp) < 0)
    return 1;

  fh_timer_t timer;
  if (fh_timer_init(&timer, 1) < 0)
    return 1;

  if (fh_timer_register_cb(&timer, drift_callback, &timer) < 0)
    return 1;

  fh_recv_q_conf_t q_confs[] = {{.port_id = port0, .queue_id = 0}, {.port_id = port1, .queue_id = 0}};

  fh_recv_t recv;
  if (fh_recv_init(&recv, &timer, q_confs, 2, dummy_rx_cb, NULL) < 0)
    return 1;

  unsigned int lcore_id = rte_get_next_lcore(-1, 1, 0);
  if (lcore_id == RTE_MAX_LCORE) {
    fprintf(stderr, "No available lcore\n");
    return 1;
  }

  printf("Starting shared-core drift measurement (Timer + RX polling) for 5 seconds on lcore %u...\n", lcore_id);
  rte_eal_remote_launch(fh_recv_run, &recv, lcore_id);

  sleep(5);

  fh_recv_stop(&recv);
  fh_timer_stop(&timer);
  rte_eal_wait_lcore(lcore_id);

  if (global_stats.sample_count == 0) {
    fprintf(stderr, "No valid samples received!\n");
    return 1;
  }

  double mean = global_stats.sum_err / global_stats.sample_count;
  double variance = (global_stats.sum_err_sq / global_stats.sample_count) - (mean * mean);
  if (variance < 0)
    variance = 0;
  double stddev = sqrt(variance);
  double duration = (double)(global_stats.end_time - global_stats.start_time) / NS_PER_SEC;
  double drift_rate = (global_stats.last_err - global_stats.first_err) / duration;

  printf("\n--- Shared Core Drift Analysis Results ---\n");
  printf("Total Callbacks:   %lu\n", global_stats.total_callbacks);
  printf("Valid Samples:     %lu\n", global_stats.sample_count);
  printf("Duration:          %.3f s\n", duration);
  printf("Mean Offset:       %.2f ns\n", mean);
  printf("Std Deviation:     %.2f ns\n", stddev);
  printf("Min/Max Error:     %.2f / %.2f ns\n", global_stats.min_err, global_stats.max_err);
  printf("Peak-to-Peak:      %.2f ns\n", global_stats.max_err - global_stats.min_err);
  printf("Estimated Drift:   %.2f ns/s\n", drift_rate);

  bool success = true;
  if (fabs(drift_rate) > 500.0) { // A bit more relaxed than standalone
    printf("WARNING: High drift rate detected (> 500 ns/s)\n");
    success = false;
  }
  if (stddev > 2000000.0) { // Max 2ms jitter
    printf("WARNING: High jitter detected (> 2 ms)\n");
    success = false;
  }

  if (success) {
    printf("\nSUCCESS: Timing stable even with RX polling overhead.\n");
    return 0;
  } else {
    printf("\nFAILURE: Performance metrics outside of expected bounds.\n");
    return 1;
  }
}
