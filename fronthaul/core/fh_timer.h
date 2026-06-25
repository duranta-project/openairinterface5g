/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FH_TIMER_H
#define FH_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_pause.h>
#include <rte_eal.h>

/**
 * @brief Callback for 5G symbol timer
 *
 * @param s_abs Absolute symbol number since GPS epoch
 * @param user_data User provided data pointer
 */
typedef void (*fh_timer_cb)(uint64_t s_abs, void *user_data);

#define MAX_FH_TIMER_CBS 8
#define GPS_EPOCH_OFFSET_UNIX 315964800ULL
#define GPS_LEAP_SECONDS 18

typedef struct {
  fh_timer_cb fn;
  void *user_data;
} fh_timer_cb_entry_t;

typedef struct fh_timer {
  int numerology;
  fh_timer_cb_entry_t cbs[MAX_FH_TIMER_CBS];
  int num_cbs;
  volatile bool running;
  rte_atomic64_t s_abs; // Current absolute symbol
  uint64_t ns_per_symbol;
  uint32_t symbols_per_frame;
  uint32_t slots_per_frame;

  // New state for passive tick
  uint64_t target_gps_ns;
  uint64_t next_s_abs;
} fh_timer_t;

/**
 * @brief Initialize the 5G symbol timer state.
 *
 * @param timer Pointer to timer handle to initialize
 * @param numerology 5G NR numerology (0, 1, 2, 3)
 * @return int 0 on success, negative on error
 */
int fh_timer_init(fh_timer_t *timer, int numerology);

/**
 * @brief Register a callback to be executed every symbol.
 *
 * @param timer Pointer to timer handle
 * @param cb Callback function
 * @param user_data User provided data for callback
 * @return int 0 on success, negative if no space left
 */
int fh_timer_register_cb(fh_timer_t *timer, fh_timer_cb cb, void *user_data);

/**
 * @brief Drive the 5G symbol timer by checking current time.
 * This should be called frequently (e.g. from an RX poll loop).
 *
 * @param timer Pointer to the initialized timer handle
 */
void fh_timer_tick(fh_timer_t *timer);

/**
 * @brief Get a 5G time symbol.
 *
 * @param timer Pointer to timer handle
 *
 * @return Current absolute symbol number since GPS Epoch
 */
uint64_t fh_timer_get_current_symbol(fh_timer_t *timer);

/**
 * @brief Stop the 5G symbol timer.
 *
 * @param timer Pointer to timer handle
 */
void fh_timer_stop(fh_timer_t *timer);

#endif /* FH_TIMER_H */
