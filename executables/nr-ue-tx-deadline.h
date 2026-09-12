/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UE_TX_DEADLINE_H
#define NR_UE_TX_DEADLINE_H

#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <time.h>

typedef struct {
  int64_t radio_timestamp;
  uint64_t monotonic_ns;
  int error_code;
  bool valid;
} nr_ue_tx_deadline_anchor_t;

typedef struct {
  uint64_t monotonic_ns;
  int error_code;
  bool valid;
} nr_ue_tx_deadline_t;

typedef struct {
  uint64_t monotonic_now_ns;
  int64_t lateness_ns;
  int error_code;
  bool valid;
  bool missed;
} nr_ue_tx_deadline_check_t;

nr_ue_tx_deadline_anchor_t nr_ue_tx_deadline_make_anchor(int64_t first_sample_timestamp,
                                                         int sample_count,
                                                         const struct timespec *monotonic_time,
                                                         int clock_error);

nr_ue_tx_deadline_t nr_ue_tx_deadline_compute(const nr_ue_tx_deadline_anchor_t *anchor,
                                              int64_t write_timestamp,
                                              int64_t guard_samples,
                                              int64_t samples_per_subframe);

nr_ue_tx_deadline_check_t nr_ue_tx_deadline_check(const nr_ue_tx_deadline_t *deadline,
                                                  const struct timespec *monotonic_time,
                                                  int clock_error);

bool nr_ue_tx_deadline_log_due(_Atomic(uint64_t) *counter);

#endif /* NR_UE_TX_DEADLINE_H */
