/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * The two ways split7 threads wait for each other. Shared state is guarded by a
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

#ifndef SPLIT7_WAIT_H
#define SPLIT7_WAIT_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/// Absolute CLOCK_MONOTONIC time, or none.
typedef struct {
  struct timespec at;
  bool none;
} split7_deadline_t;

/// timeout_us < 0: no deadline.
split7_deadline_t split7_deadline_in(int timeout_us);

typedef struct {
  pthread_mutex_t lock;
  pthread_cond_t cond;
  uint64_t value;
  uint64_t wake_at; ///< lowest target a waiter sleeps for: smaller values wake nobody
  bool closed;
} split7_progress_t;

void split7_progress_init(split7_progress_t *p, uint64_t value);
void split7_progress_destroy(split7_progress_t *p);
uint64_t split7_progress_get(split7_progress_t *p);
/// Raises the value to v (no effect if already there) and wakes the waiters.
void split7_progress_raise(split7_progress_t *p, uint64_t v);
/// Adds n and wakes the waiters; returns the new value.
uint64_t split7_progress_add(split7_progress_t *p, uint64_t n);
typedef enum {
  SPLIT7_REACHED,
  SPLIT7_TIMEOUT,
  SPLIT7_CLOSED,
} split7_wait_result_t;

/// Waits until the value is at least target, at most timeout_us (< 0: no limit).
split7_wait_result_t split7_progress_wait(split7_progress_t *p, uint64_t target, int timeout_us);
/// The value, ordered after every read made before the call: for data read without a
/// lock while its writer may move on (a ring), then checked against the value.
uint64_t split7_progress_get_after_reads(split7_progress_t *p);
/// Releases every waiter, now and later.
void split7_progress_close(split7_progress_t *p);

typedef struct {
  pthread_mutex_t lock;
  pthread_cond_t cond;
} split7_monitor_t;

void split7_monitor_init(split7_monitor_t *m);
void split7_monitor_destroy(split7_monitor_t *m);
void split7_monitor_lock(split7_monitor_t *m);
void split7_monitor_unlock(split7_monitor_t *m);
/// Holding the lock: wakes every waiter to re-check the state.
void split7_monitor_notify(split7_monitor_t *m);
/// Holding the lock: releases it until notified (or a spurious wake), then retakes it.
/// False once the deadline passed.
bool split7_monitor_wait(split7_monitor_t *m, const split7_deadline_t *deadline);

#endif
