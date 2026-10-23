/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_pss_cfo_coarse_search.h"
#include <stdlib.h>

static bool search_input_is_valid(const nr_pss_cfo_coarse_search_t *search)
{
  if (search == NULL || search->rxdata == NULL || search->nb_antennas_rx <= 0 || search->rxdata_length <= 0)
    return false;
  if (search->ofdm_symbol_size <= 0 || search->ofdm_symbol_size > NR_PSS_CFO_MAX_OFDM_SYMBOL_SIZE)
    return false;
  if (search->sampling_rate_hz <= 0)
    return false;
  for (int nid2 = 0; nid2 < NUMBER_PSS_SEQUENCE; ++nid2) {
    if (search->pss_time[nid2] == NULL)
      return false;
  }
  for (int antenna = 0; antenna < search->nb_antennas_rx; ++antenna) {
    if (search->rxdata[antenna] == NULL)
      return false;
  }
  return true;
}

static bool candidate_is_better(const nr_pss_cfo_hypothesis_result_t *candidate,
                                const nr_pss_cfo_hypothesis_result_t *current)
{
  const unsigned __int128 candidate_score = (unsigned __int128)candidate->peak_raw * current->avg_raw;
  const unsigned __int128 current_score = (unsigned __int128)current->peak_raw * candidate->avg_raw;
  if (candidate_score != current_score)
    return candidate_score > current_score;
  if (candidate->peak_raw != current->peak_raw)
    return candidate->peak_raw > current->peak_raw;

  const long long candidate_abs_cfo = llabs((long long)candidate->cfo_hz);
  const long long current_abs_cfo = llabs((long long)current->cfo_hz);
  if (candidate_abs_cfo != current_abs_cfo)
    return candidate_abs_cfo < current_abs_cfo;
  if (candidate->cfo_hz != current->cfo_hz)
    return candidate->cfo_hz < current->cfo_hz;
  if (candidate->nid2 != current->nid2)
    return candidate->nid2 < current->nid2;
  return candidate->pos < current->pos;
}

nr_pss_cfo_coarse_search_result_t nr_pss_cfo_search_coarse(const nr_pss_cfo_coarse_search_t *search,
                                                            const nr_pss_cfo_search_config_t *config)
{
  nr_pss_cfo_coarse_search_result_t result = {
      .status = NR_PSS_CFO_SEARCH_INVALID_ARGUMENT,
      .best = {.status = NR_PSS_CFO_SEARCH_NO_CANDIDATE, .pos = -1},
  };
  if (!search_input_is_valid(search) || config == NULL || !config->enabled)
    return result;

  int coarse_bins = 0;
  const nr_pss_cfo_search_status_t config_status = nr_pss_cfo_search_validate_config(config, &coarse_bins, NULL);
  if (config_status != NR_PSS_CFO_SEARCH_OK) {
    result.status = config_status;
    return result;
  }
  if (2LL * config->coarse_span_hz > search->sampling_rate_hz)
    return result;

  bool found_candidate = false;
  for (int bin = 0; bin < coarse_bins; ++bin) {
    const int cfo_hz = (int)(-(int64_t)config->coarse_span_hz + (int64_t)bin * config->coarse_step_hz);
    ++result.bins_evaluated;
    for (int nid2 = 0; nid2 < NUMBER_PSS_SEQUENCE; ++nid2) {
      const nr_pss_cfo_hypothesis_t hypothesis = {
          .rxdata = search->rxdata,
          .nb_antennas_rx = search->nb_antennas_rx,
          .rxdata_length = search->rxdata_length,
          .ofdm_symbol_size = search->ofdm_symbol_size,
          .pss_time = search->pss_time[nid2],
          .sampling_rate_hz = search->sampling_rate_hz,
          .cfo_hz = cfo_hz,
          .nid2 = nid2,
      };
      const nr_pss_cfo_hypothesis_result_t candidate = nr_pss_cfo_evaluate_hypothesis(&hypothesis);
      ++result.hypotheses_evaluated;
      if (candidate.status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT) {
        result.status = candidate.status;
        return result;
      }
      if (candidate.status != NR_PSS_CFO_SEARCH_OK)
        continue;
      if (!found_candidate || candidate_is_better(&candidate, &result.best)) {
        result.best = candidate;
        found_candidate = true;
      }
    }
  }

  result.status = found_candidate ? NR_PSS_CFO_SEARCH_OK : NR_PSS_CFO_SEARCH_NO_CANDIDATE;
  return result;
}
