/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "fh_timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <rte_eal.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_launch.h>

#define GPS_EPOCH_OFFSET_UNIX 315964800ULL
#define NS_PER_SEC 1000000000ULL

static uint64_t get_gps_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return ((uint64_t)ts.tv_sec - GPS_EPOCH_OFFSET_UNIX + GPS_LEAP_SECONDS) * NS_PER_SEC + ts.tv_nsec;
}

typedef struct {
  uint64_t count;
  double sum_jitter;
  double max_jitter;
  double min_jitter;
} stats_t;

static stats_t global_stats = {0, 0, 0, 1e12};

void timer_callback(uint64_t s_abs, void *user_data)
{
  fh_timer_t *timer = (fh_timer_t *)user_data;
  uint64_t now_ns = get_gps_ns();

  // Theoretical target time
  uint64_t syms_per_ms = 14 << timer->numerology;
  typedef __int128_t int128;
  uint64_t target_ns = (uint64_t)((int128)s_abs * 1000000ULL / syms_per_ms);

  double jitter = (double)now_ns - (double)target_ns;

  global_stats.count++;
  global_stats.sum_jitter += jitter;
  if (jitter > global_stats.max_jitter)
    global_stats.max_jitter = jitter;
  if (jitter < global_stats.min_jitter)
    global_stats.min_jitter = jitter;

  if (global_stats.count % 1000 == 0) {
    printf("Symbol: %lu | Jitter: %.2f ns\n", s_abs, jitter);
  }
}

static int dummy_timer_worker(void *arg)
{
  fh_timer_t *timer = (fh_timer_t *)arg;
  while (timer->running) {
    fh_timer_tick(timer);
    rte_pause();
  }
  return 0;
}

int main(int argc, char **argv)
{
  int ret = rte_eal_init(argc, argv);
  if (ret < 0) {
    fprintf(stderr, "DPDK init failed\n");
    return 1;
  }

  fh_timer_t timer;
  int numerology = 1; // 30kHz
  if (fh_timer_init(&timer, numerology) < 0) {
    fprintf(stderr, "Timer init failed\n");
    return 1;
  }

  if (fh_timer_register_cb(&timer, timer_callback, &timer) < 0) {
    fprintf(stderr, "Callback registration failed\n");
    return 1;
  }

  unsigned int lcore_id = rte_get_next_lcore(-1, 1, 0);
  if (lcore_id == RTE_MAX_LCORE) {
    fprintf(stderr, "No worker lcore available.\n");
    return 1;
  }

  printf("Timer starting on lcore %u for numerology %d (30kHz). Running for 1 second...\n", lcore_id, numerology);

  if (rte_eal_remote_launch(dummy_timer_worker, &timer, lcore_id) < 0) {
    fprintf(stderr, "Failed to launch timer on lcore %u\n", lcore_id);
    return 1;
  }

  sleep(1);

  fh_timer_stop(&timer);
  rte_eal_wait_lcore(lcore_id);

  printf("\n--- Test Results ---\n");
  printf("Total callbacks: %lu\n", global_stats.count);
  uint32_t expected = (1000 * (14 << numerology)); // 1s = 1000ms
  printf("Expected callbacks (approx): %u\n", expected);

  if (global_stats.count > expected * 0.9 && global_stats.count < expected * 1.1) {
    printf("SUCCESS: Callback count is within reasonable range.\n");
  } else {
    printf("FAILURE: Callback count is way off (%lu vs %u).\n", global_stats.count, expected);
    return 1;
  }

  return 0;
}
