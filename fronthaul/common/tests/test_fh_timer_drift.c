/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "fh_timer.h"
#include <math.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

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
  double sum_err;
  double sum_err_sq;
  double max_err;
  double min_err;
  uint64_t start_time;
  uint64_t end_time;
  double first_err;
  double last_err;
} drift_stats_t;

static drift_stats_t global_stats = {0, 0, 0, -1e18, 1e18, 0, 0, 0, 0};

void drift_callback(uint64_t s_abs, void *user_data)
{
  fh_timer_t *timer = (fh_timer_t *)user_data;
  uint64_t now_ns = get_gps_ns();

  // Theoretical target time as calculated by the library
  uint64_t syms_per_ms = 14 << timer->numerology;
  typedef __int128_t int128;
  uint64_t target_ns = (uint64_t)((int128)s_abs * 1000000ULL / syms_per_ms);

  double err = (double)now_ns - (double)target_ns;

  // Discard first 1000 samples to avoid startup transients
  if (global_stats.count < 1000) {
    global_stats.count++;
    return;
  }

  if (global_stats.count == 1000) {
    global_stats.start_time = now_ns;
    global_stats.first_err = err;
  }
  global_stats.end_time = now_ns;
  global_stats.last_err = err;

  global_stats.count++;
  global_stats.sum_err += err;
  global_stats.sum_err_sq += (err * err);

  if (err > global_stats.max_err)
    global_stats.max_err = err;
  if (err < global_stats.min_err)
    global_stats.min_err = err;
}

static int timer_worker(void *arg)
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
    fprintf(stderr, "Error with EAL initialization\n");
    return 1;
  }

  fh_timer_t timer;
  int numerology = 1; // 30kHz
  if (fh_timer_init(&timer, numerology) < 0) {
    fprintf(stderr, "Failed to initialize timer\n");
    return 1;
  }

  if (fh_timer_register_cb(&timer, drift_callback, &timer) < 0) {
    fprintf(stderr, "Failed to register callback\n");
    return 1;
  }

  unsigned int lcore_id = rte_get_next_lcore(-1, 1, 0);
  if (lcore_id == RTE_MAX_LCORE) {
    fprintf(stderr, "No available lcore for timer worker\n");
    return 1;
  }

  printf("Starting drift measurement for 5 seconds on lcore %u...\n", lcore_id);
  if (rte_eal_remote_launch(timer_worker, &timer, lcore_id) < 0) {
    fprintf(stderr, "Failed to launch timer worker\n");
    return 1;
  }

  sleep(5);

  fh_timer_stop(&timer);
  rte_eal_wait_lcore(lcore_id);

  if (global_stats.count == 0) {
    fprintf(stderr, "No callbacks received!\n");
    return 1;
  }

  double mean = global_stats.sum_err / global_stats.count;
  double variance = (global_stats.sum_err_sq / global_stats.count) - (mean * mean);
  if (variance < 0)
    variance = 0;
  double stddev = sqrt(variance);
  double duration = (double)(global_stats.end_time - global_stats.start_time) / NS_PER_SEC;
  double drift_rate = (global_stats.last_err - global_stats.first_err) / duration;

  printf("\n--- Drift Analysis Results ---\n");
  printf("Total Samples:     %lu\n", global_stats.count);
  printf("Duration:          %.3f s\n", duration);
  printf("Mean Offset:       %.2f ns\n", mean);
  printf("Std Deviation:     %.2f ns\n", stddev);
  printf("Min/Max Error:     %.2f / %.2f ns\n", global_stats.min_err, global_stats.max_err);
  printf("Peak-to-Peak:      %.2f ns\n", global_stats.max_err - global_stats.min_err);
  printf("Estimated Drift:   %.2f ns/s\n", drift_rate);

  // Success criteria:
  // 1. Mean offset should be small (alignment error).
  // 2. Drift rate should be near zero (since both use CLOCK_REALTIME).
  // 3. Jitter (stddev) should be reasonably low for a real-time system.

  bool success = true;
  if (fabs(drift_rate) > 100.0) { // More than 100ns/s drift is suspicious
    printf("WARNING: High drift rate detected (> 100 ns/s)\n");
    success = false;
  }
  if (stddev > 1000000.0) { // More than 1ms jitter is huge
    printf("WARNING: High jitter detected (> 1 ms)\n");
    success = false;
  }

  if (success) {
    printf("\nSUCCESS: Timing is stable and synchronized with CLOCK_REALTIME.\n");
    return 0;
  } else {
    printf("\nFAILURE: Performance metrics outside of expected bounds.\n");
    return 1;
  }
}
