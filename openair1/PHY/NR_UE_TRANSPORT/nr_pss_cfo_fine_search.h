/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_PSS_CFO_FINE_SEARCH_H
#define NR_PSS_CFO_FINE_SEARCH_H

#include "nr_pss_cfo_coarse_search.h"

typedef struct {
  nr_pss_cfo_search_status_t status;
  nr_pss_cfo_hypothesis_result_t best;
  int coarse_cfo_hz;
  int fine_cfo_hz;
  int bins_evaluated;
  int hypotheses_evaluated;
} nr_pss_cfo_fine_search_result_t;

/** Refine one valid S1a candidate with the configured fine CFO grid.
 *
 * Only the NID2 selected by S1a is evaluated. best.cfo_hz contains the total
 * CFO candidate, while fine_cfo_hz is the offset relative to coarse_cfo_hz.
 */
nr_pss_cfo_fine_search_result_t nr_pss_cfo_search_fine(const nr_pss_cfo_coarse_search_t *search,
                                                        const nr_pss_cfo_hypothesis_result_t *coarse_candidate,
                                                        const nr_pss_cfo_search_config_t *config);

#endif /* NR_PSS_CFO_FINE_SEARCH_H */
