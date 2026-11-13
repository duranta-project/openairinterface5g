/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * The two ways UE Low-PHY threads wait for each other. Shared state is guarded by a
 * mutex (a monitor's, or a plain one) or is a progress value. Bulk data (sample
 * rings, symbol and PRACH entries) is handed over by ownership instead: whoever owns
 * it writes it, then hands it over through one of these (a record, a tag, a progress
 * value), and the lock taken there orders the data.
 *
 * Progress: a value that only grows (symbols published, events posted, jobs run).
 * Writers raise it, readers wait until it reaches a target.
 *
 * Monitor: a mutex and a condition. State it guards is read and changed holding it;
 * whoever changes the state notifies, waiters re-check after every wake.
 *
 * Waits never hold a second lock: a progress or monitor lock is held only inside
 * these functions, or around the state it guards.
 */

#ifndef UE_LOW_PHY_WAIT_H
#define UE_LOW_PHY_WAIT_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/// Absolute CLOCK_MONOTONIC time, or none.
typedef struct {
  struct timespec at;
  bool none;
} ue_low_phy_deadline_t;

/// timeout_us < 0: no deadline.
ue_low_phy_deadline_t ue_low_phy_deadline_in(int timeout_us);

typedef struct {
  pthread_mutex_t lock;
  pthread_cond_t cond;
  uint64_t value;
  uint64_t wake_at; ///< lowest target a waiter sleeps for: smaller values wake nobody
  bool closed;
} ue_low_phy_progress_t;

void ue_low_phy_progress_init(ue_low_phy_progress_t *p, uint64_t value);
void ue_low_phy_progress_destroy(ue_low_phy_progress_t *p);
uint64_t ue_low_phy_progress_get(ue_low_phy_progress_t *p);
/// Raises the value to v (no effect if already there) and wakes the waiters.
void ue_low_phy_progress_raise(ue_low_phy_progress_t *p, uint64_t v);
/// Adds n and wakes the waiters; returns the new value.
uint64_t ue_low_phy_progress_add(ue_low_phy_progress_t *p, uint64_t n);
typedef enum {
  UE_LOW_PHY_REACHED,
  UE_LOW_PHY_TIMEOUT,
  UE_LOW_PHY_CLOSED,
} ue_low_phy_wait_result_t;

/// Waits until the value is at least target, at most timeout_us (< 0: no limit).
ue_low_phy_wait_result_t ue_low_phy_progress_wait(ue_low_phy_progress_t *p, uint64_t target, int timeout_us);
/// The value, ordered after every read made before the call: for data read without a
/// lock while its writer may move on (a ring), then checked against the value.
uint64_t ue_low_phy_progress_get_after_reads(ue_low_phy_progress_t *p);
/// Releases every waiter, now and later.
void ue_low_phy_progress_close(ue_low_phy_progress_t *p);

typedef struct {
  pthread_mutex_t lock;
  pthread_cond_t cond;
} ue_low_phy_monitor_t;

void ue_low_phy_monitor_init(ue_low_phy_monitor_t *m);
void ue_low_phy_monitor_destroy(ue_low_phy_monitor_t *m);
void ue_low_phy_monitor_lock(ue_low_phy_monitor_t *m);
void ue_low_phy_monitor_unlock(ue_low_phy_monitor_t *m);
/// Holding the lock: wakes every waiter to re-check the state.
void ue_low_phy_monitor_notify(ue_low_phy_monitor_t *m);
/// Holding the lock: releases it until notified (or a spurious wake), then retakes it.
/// False once the deadline passed.
bool ue_low_phy_monitor_wait(ue_low_phy_monitor_t *m, const ue_low_phy_deadline_t *deadline);

#endif
