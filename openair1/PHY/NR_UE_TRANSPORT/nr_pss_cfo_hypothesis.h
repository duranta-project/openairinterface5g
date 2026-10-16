/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_PSS_CFO_HYPOTHESIS_H
#define NR_PSS_CFO_HYPOTHESIS_H

#include "common/platform_types.h"
#include "nr_pss_cfo_search_config.h"
#include <stdint.h>

#define NR_PSS_CFO_TIMING_STRIDE 4
#define NR_PSS_CFO_MAX_OFDM_SYMBOL_SIZE 8192

/** Input view for one CFO and one PSS hypothesis.
 *
 * rxdata_length is the number of candidate start samples to search. Each
 * antenna buffer must also contain ofdm_symbol_size - 1 readable samples after
 * the final candidate start, matching the contract of pss_search_time_nr().
 */
typedef struct {
  const c16_t *const *rxdata;
  int nb_antennas_rx;
  int rxdata_length;
  int ofdm_symbol_size;
  const c16_t *pss_time;
  int sampling_rate_hz;
  int cfo_hz;
  int nid2;
} nr_pss_cfo_hypothesis_t;

typedef struct {
  nr_pss_cfo_search_status_t status;
  int cfo_hz;
  int nid2;
  int pos;
  uint64_t peak_raw;
  uint64_t avg_raw;
  int positions_evaluated;
} nr_pss_cfo_hypothesis_result_t;

/** Evaluate one CFO candidate against one time-domain PSS reference.
 *
 * A positive CFO candidate represents a positive frequency offset in the
 * received signal. The function does not modify rxdata or pss_time.
 */
nr_pss_cfo_hypothesis_result_t nr_pss_cfo_evaluate_hypothesis(const nr_pss_cfo_hypothesis_t *hypothesis);

/** Find the strongest timing competitor outside a guard around best_pos. */
nr_pss_cfo_search_status_t nr_pss_cfo_evaluate_second_timing_peak(const nr_pss_cfo_hypothesis_t *hypothesis,
                                                                   int best_pos,
                                                                   int guard_samples,
                                                                   uint64_t *second_peak_raw);

#endif /* NR_PSS_CFO_HYPOTHESIS_H */
