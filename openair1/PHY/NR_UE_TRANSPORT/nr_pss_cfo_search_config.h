/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_PSS_CFO_SEARCH_CONFIG_H
#define NR_PSS_CFO_SEARCH_CONFIG_H

#include <stdbool.h>

#define NR_PSS_CFO_SEARCH_DEFAULT_COARSE_SPAN_HZ 60000
#define NR_PSS_CFO_SEARCH_DEFAULT_COARSE_STEP_HZ 5000
#define NR_PSS_CFO_SEARCH_DEFAULT_FINE_SPAN_HZ 6000
#define NR_PSS_CFO_SEARCH_DEFAULT_FINE_STEP_HZ 400
#define NR_PSS_CFO_SEARCH_MAX_BINS 257

typedef enum {
  NR_PSS_CFO_SEARCH_OK = 0,
  NR_PSS_CFO_SEARCH_INVALID_ARGUMENT,
  NR_PSS_CFO_SEARCH_INVALID_GRID,
  NR_PSS_CFO_SEARCH_NO_CANDIDATE,
} nr_pss_cfo_search_status_t;

typedef struct {
  bool enabled;
  int coarse_span_hz;
  int coarse_step_hz;
  int fine_span_hz;
  int fine_step_hz;
} nr_pss_cfo_search_config_t;

/** Validate a CFO search grid and optionally return its number of bins.
 *
 * A disabled configuration is valid and reports zero bins. For an enabled
 * configuration, both grids must contain zero, have positive steps and stay
 * within NR_PSS_CFO_SEARCH_MAX_BINS.
 */
nr_pss_cfo_search_status_t nr_pss_cfo_search_validate_config(const nr_pss_cfo_search_config_t *config,
                                                              int *coarse_bins,
                                                              int *fine_bins);

const char *nr_pss_cfo_search_status_name(nr_pss_cfo_search_status_t status);

#endif /* NR_PSS_CFO_SEARCH_CONFIG_H */
