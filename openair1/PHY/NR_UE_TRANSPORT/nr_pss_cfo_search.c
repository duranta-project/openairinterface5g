/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_pss_cfo_search_config.h"
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

static bool validate_grid(const int span_hz, const int step_hz, int *num_bins)
{
  if (span_hz < 0 || step_hz <= 0)
    return false;

  /* Starting at -span, zero belongs to the grid only if step divides span. */
  if (span_hz % step_hz != 0)
    return false;

  const int64_t bins = (2LL * span_hz) / step_hz + 1;
  if (bins < 1 || bins > NR_PSS_CFO_SEARCH_MAX_BINS || bins > INT_MAX)
    return false;

  *num_bins = (int)bins;
  return true;
}

nr_pss_cfo_search_status_t nr_pss_cfo_search_validate_config(const nr_pss_cfo_search_config_t *config,
                                                              int *coarse_bins,
                                                              int *fine_bins)
{
  if (coarse_bins != NULL)
    *coarse_bins = 0;
  if (fine_bins != NULL)
    *fine_bins = 0;

  if (config == NULL)
    return NR_PSS_CFO_SEARCH_INVALID_ARGUMENT;

  if (!config->enabled)
    return NR_PSS_CFO_SEARCH_OK;

  int validated_coarse_bins = 0;
  int validated_fine_bins = 0;
  if (!validate_grid(config->coarse_span_hz, config->coarse_step_hz, &validated_coarse_bins)
      || !validate_grid(config->fine_span_hz, config->fine_step_hz, &validated_fine_bins))
    return NR_PSS_CFO_SEARCH_INVALID_GRID;

  if (coarse_bins != NULL)
    *coarse_bins = validated_coarse_bins;
  if (fine_bins != NULL)
    *fine_bins = validated_fine_bins;
  return NR_PSS_CFO_SEARCH_OK;
}

const char *nr_pss_cfo_search_status_name(const nr_pss_cfo_search_status_t status)
{
  switch (status) {
    case NR_PSS_CFO_SEARCH_OK:
      return "ok";
    case NR_PSS_CFO_SEARCH_INVALID_ARGUMENT:
      return "invalid_argument";
    case NR_PSS_CFO_SEARCH_INVALID_GRID:
      return "invalid_grid";
    case NR_PSS_CFO_SEARCH_NO_CANDIDATE:
      return "no_candidate";
    default:
      return "unknown";
  }
}
