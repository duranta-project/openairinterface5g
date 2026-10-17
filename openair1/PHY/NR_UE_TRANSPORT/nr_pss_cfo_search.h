/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_PSS_CFO_SEARCH_H
#define NR_PSS_CFO_SEARCH_H

#include "nr_pss_cfo_coarse_search.h"
#include "nr_pss_cfo_fine_search.h"
#include "nr_pss_cfo_hypothesis.h"
#include "nr_pss_cfo_search_config.h"
#include "PHY/NR_REFSIG/pss_nr.h"
#include <stdint.h>

typedef struct {
  nr_pss_cfo_search_status_t status;
  pss_detection_result_t pss;
  int coarse_cfo_hz;
  int fine_cfo_hz;
  int residual_cfo_hz;
  uint64_t second_timing_peak_raw;
  int coarse_bins_evaluated;
  int fine_bins_evaluated;
  int diagnostic_passes_evaluated;
} nr_pss_cfo_search_result_t;

/** Run the isolated Stage 1 pipeline: S1a, S1b and diagnostic metrics. */
nr_pss_cfo_search_result_t nr_pss_cfo_search_stage1(const nr_pss_cfo_coarse_search_t *search,
                                                     const nr_pss_cfo_search_config_t *config);

#endif /* NR_PSS_CFO_SEARCH_H */
