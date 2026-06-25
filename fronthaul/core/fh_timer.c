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

#define NS_PER_SEC 1000000000ULL
#define NS_PER_MS 1000000ULL
#define NS_PER_FRAME (10 * NS_PER_MS)

static uint64_t get_gps_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return ((uint64_t)ts.tv_sec - GPS_EPOCH_OFFSET_UNIX + GPS_LEAP_SECONDS) * NS_PER_SEC + ts.tv_nsec;
}

void fh_timer_tick(fh_timer_t *timer)
{
  if (!timer->running)
    return;

  uint64_t now = get_gps_ns();
  if (now >= timer->target_gps_ns) {
    typedef __int128_t int128;
    uint64_t total_syms_per_ms = 14 << timer->numerology;

    // 1. Calculate how many symbols *should* have elapsed up to 'now'
    // This catches up if the clock jumped forward, or processes at least 1 symbol.
    uint64_t expected_s_abs = (uint64_t)((int128)now * total_syms_per_ms / NS_PER_MS);

    // PTP sync backwards adjustment - don't run callbacks until we catch up to the previously expected symbol index
    if (expected_s_abs < timer->next_s_abs) {
      return;
    }

    // 2. Drive the loop using the absolute symbol counter, not the volatile 'now' time
    while (timer->next_s_abs <= expected_s_abs && timer->running) {
      uint64_t s_abs = timer->next_s_abs;
      rte_atomic64_set(&timer->s_abs, s_abs);

      // Execute all registered callbacks
      for (int i = 0; i < timer->num_cbs; i++) {
        timer->cbs[i].fn(s_abs, timer->cbs[i].user_data);
      }

      // Prepare for next symbol
      timer->next_s_abs++;
    }

    // 3. Recalculate the next target timestamp based purely on the new symbol index
    timer->target_gps_ns = (uint64_t)((int128)timer->next_s_abs * NS_PER_MS / total_syms_per_ms);
  }
}

int fh_timer_init(fh_timer_t *timer, int numerology)
{
  if (numerology < 0 || numerology > 3)
    return -1;

  timer->numerology = numerology;
  timer->num_cbs = 0;
  timer->running = true;

  uint64_t total_syms_per_ms = 14 << numerology;
  timer->ns_per_symbol = NS_PER_MS / total_syms_per_ms;
  timer->symbols_per_frame = 10 * total_syms_per_ms;
  timer->slots_per_frame = 10 << numerology;

  // Initial timing state
  uint64_t gps_now = get_gps_ns();
  typedef __int128_t int128;
  uint64_t s_abs = (uint64_t)((int128)gps_now * total_syms_per_ms / NS_PER_MS);
  rte_atomic64_set(&timer->s_abs, s_abs);
  timer->next_s_abs = s_abs + 1;
  timer->target_gps_ns = (uint64_t)((int128)timer->next_s_abs * NS_PER_MS / total_syms_per_ms);

  return 0;
}

int fh_timer_register_cb(fh_timer_t *timer, fh_timer_cb cb, void *user_data)
{
  if (timer->num_cbs >= MAX_FH_TIMER_CBS)
    return -1;

  timer->cbs[timer->num_cbs].fn = cb;
  timer->cbs[timer->num_cbs].user_data = user_data;
  timer->num_cbs++;

  return 0;
}

void fh_timer_stop(fh_timer_t *timer)
{
  timer->running = false;
}

uint64_t fh_timer_get_current_symbol(fh_timer_t *timer)
{
  uint64_t gps_now = get_gps_ns();

  typedef __int128_t int128;
  uint64_t total_syms_per_ms = 14 << timer->numerology;

  uint64_t cur_s_abs = (uint64_t)((int128)gps_now * total_syms_per_ms / NS_PER_MS);
  return cur_s_abs;
}
