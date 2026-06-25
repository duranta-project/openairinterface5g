/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "fh_timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#define GPS_EPOCH_OFFSET_UNIX 315964800ULL
#define NS_PER_SEC 1000000000ULL

// Mock clock state
static struct timespec mock_time = {0, 0};

// Mock implementation of clock_gettime
int __wrap_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
  if (clk_id == CLOCK_REALTIME) {
    *tp = mock_time;
    return 0;
  }
  extern int __real_clock_gettime(clockid_t clk_id, struct timespec * tp);
  return __real_clock_gettime(clk_id, tp);
}

static struct timespec gps_ns_to_timespec(uint64_t gps_ns)
{
  struct timespec ts;
  uint64_t unix_ns = gps_ns + (GPS_EPOCH_OFFSET_UNIX - GPS_LEAP_SECONDS) * NS_PER_SEC;
  ts.tv_sec = unix_ns / NS_PER_SEC;
  ts.tv_nsec = unix_ns % NS_PER_SEC;
  return ts;
}

#define MAX_CALLBACKS 100
typedef struct {
  uint64_t symbols[MAX_CALLBACKS];
  int count;
} cb_tracker_t;

static void test_cb(uint64_t s_abs, void *user_data)
{
  cb_tracker_t *tracker = (cb_tracker_t *)user_data;
  if (tracker->count < MAX_CALLBACKS) {
    tracker->symbols[tracker->count++] = s_abs;
  }
}

int main(int argc, char **argv)
{
  // 1. Initialize timer under mock clock
  // Set mock time to UNIX 1718000000 (June 2024)
  mock_time.tv_sec = 1718000000;
  mock_time.tv_nsec = 0;

  fh_timer_t timer;
  int numerology = 1; // 30kHz
  if (fh_timer_init(&timer, numerology) < 0) {
    fprintf(stderr, "Timer init failed\n");
    return 1;
  }

  cb_tracker_t tracker = {0};
  if (fh_timer_register_cb(&timer, test_cb, &tracker) < 0) {
    fprintf(stderr, "Callback registration failed\n");
    return 1;
  }

  // Verify initial state
  // Expected target is 35714 ns past the initial time (Unix 1718000000s)
  printf("Initial symbol abs: %lu, expected next_s_abs: %lu\n", rte_atomic64_read(&timer.s_abs), timer.next_s_abs);
  assert(timer.next_s_abs == rte_atomic64_read(&timer.s_abs) + 1);

  // --- Test Case 1: No tick if target not reached ---
  fh_timer_tick(&timer);
  if (tracker.count != 0) {
    fprintf(stderr, "FAIL: callback triggered before target time\n");
    return 1;
  }
  printf("Test Case 1 passed: No premature callback.\n");

  // --- Test Case 2: Normal Tick when target is reached ---
  // Advance mock time by 36000 ns (slightly more than 35714 ns symbol duration)
  mock_time.tv_nsec = 36000;
  fh_timer_tick(&timer);
  if (tracker.count != 1) {
    fprintf(stderr, "FAIL: callback not triggered when target reached (count=%d)\n", tracker.count);
    return 1;
  }
  if (tracker.symbols[0] != rte_atomic64_read(&timer.s_abs)) {
    fprintf(stderr,
            "FAIL: callback symbol %lu does not match current timer symbol %lu\n",
            tracker.symbols[0],
            rte_atomic64_read(&timer.s_abs));
    return 1;
  }
  printf("Test Case 2 passed: Normal tick called successfully.\n");

  // --- Test Case 3: Clock Jump Forward (Catching Up) ---
  // Reset tracker
  tracker.count = 0;
  // Jump clock forward by 250,000 ns (roughly 7 symbol durations)
  mock_time.tv_nsec = 260000;
  fh_timer_tick(&timer);
  // We expected it to catch up by running callbacks for all missed symbols
  // Let's verify the number of symbols it executed.
  printf("After forward jump, processed %d callbacks.\n", tracker.count);
  if (tracker.count < 5) {
    fprintf(stderr, "FAIL: forward jump did not trigger catch up callbacks (count=%d)\n", tracker.count);
    return 1;
  }
  // Let's verify the sequence of symbols is contiguous
  for (int i = 0; i < tracker.count; i++) {
    printf("Callback %d: Symbol %lu\n", i, tracker.symbols[i]);
    if (i > 0 && tracker.symbols[i] != tracker.symbols[i - 1] + 1) {
      fprintf(stderr, "FAIL: non-contiguous symbol sequence in catch up\n");
      return 1;
    }
  }
  printf("Test Case 3 passed: Forward jump catch up works.\n");

  // --- Test Case 4: Clock Jump Backward (PTP Sync Backward) ---
  // Reset tracker
  tracker.count = 0;
  // Get current state
  uint64_t pre_jump_next = timer.next_s_abs;
  uint64_t pre_jump_target = timer.target_gps_ns;
  printf("Before backward jump: next_s_abs=%lu, target_gps_ns=%lu\n", pre_jump_next, pre_jump_target);

  // Jump mock time backward to tv_nsec = 100000
  mock_time.tv_nsec = 100000;
  fh_timer_tick(&timer);
  if (tracker.count != 0) {
    fprintf(stderr, "FAIL: callbacks executed during backward clock jump (count=%d)\n", tracker.count);
    return 1;
  }
  // Move time forward a bit but still below target
  mock_time.tv_nsec = 200000;
  fh_timer_tick(&timer);
  if (tracker.count != 0) {
    fprintf(stderr, "FAIL: callbacks executed before catching up to previous state (count=%d)\n", tracker.count);
    return 1;
  }

  // Now move time past the old target
  mock_time = gps_ns_to_timespec(pre_jump_target + 1000);

  printf("Setting mock time past old target: tv_sec=%ld, tv_nsec=%ld\n", mock_time.tv_sec, mock_time.tv_nsec);
  fh_timer_tick(&timer);
  if (tracker.count == 0) {
    fprintf(stderr, "FAIL: timer did not resume after clock caught up to previous target\n");
    return 1;
  }
  printf("Resumed successfully with symbol %lu\n", tracker.symbols[0]);
  if (tracker.symbols[0] != pre_jump_next) {
    fprintf(stderr, "FAIL: resumed symbol %lu does not match expected %lu\n", tracker.symbols[0], pre_jump_next);
    return 1;
  }
  printf("Test Case 4 passed: Backward jump (PTP sync backward) handling works\n");

  // --- Test Case 5: fh_timer_get_current_symbol ---
  mock_time.tv_sec = 1718000000;
  mock_time.tv_nsec = 500000; // 0.5 ms
  uint64_t cur_sym = fh_timer_get_current_symbol(&timer);
  typedef __int128_t int128;
  uint64_t expected_sym =
      (uint64_t)((((int128)1718000000 - GPS_EPOCH_OFFSET_UNIX + GPS_LEAP_SECONDS) * NS_PER_SEC + 500000) * 28 / 1000000);
  if (cur_sym != expected_sym) {
    fprintf(stderr, "FAIL: fh_timer_get_current_symbol returned %lu, expected %lu\n", cur_sym, expected_sym);
    return 1;
  }
  printf("Test Case 5 passed: fh_timer_get_current_symbol works.\n");

  printf("ALL TESTS PASSED SUCCESSFULLY!\n");
  return 0;
}
