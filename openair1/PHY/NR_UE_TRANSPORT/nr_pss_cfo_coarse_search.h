/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_PSS_CFO_COARSE_SEARCH_H
#define NR_PSS_CFO_COARSE_SEARCH_H

#include "nr_pss_cfo_hypothesis.h"
#include "nr_pss_cfo_search_config.h"
#include "PHY/NR_REFSIG/ss_pbch_nr.h"

typedef struct {
  const c16_t *const *rxdata;
  int nb_antennas_rx;
  int rxdata_length;
  int ofdm_symbol_size;
  const c16_t *pss_time[NUMBER_PSS_SEQUENCE];
  int sampling_rate_hz;
} nr_pss_cfo_coarse_search_t;

typedef struct {
  nr_pss_cfo_search_status_t status;
  nr_pss_cfo_hypothesis_result_t best;
  int bins_evaluated;
  int hypotheses_evaluated;
} nr_pss_cfo_coarse_search_result_t;

/** Evaluate the coarse CFO grid for all three NR PSS sequences.
 *
 * Candidates are ranked by peak_raw / avg_raw. No confidence threshold is
 * applied at this stage: S1a only proposes the best numerical candidate.
 */
nr_pss_cfo_coarse_search_result_t nr_pss_cfo_search_coarse(const nr_pss_cfo_coarse_search_t *search,
                                                            const nr_pss_cfo_search_config_t *config);

#endif /* NR_PSS_CFO_COARSE_SEARCH_H */
