/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_pss_cfo_fine_search.h"
#include <limits.h>
#include <stdlib.h>

static bool search_input_is_valid(const nr_pss_cfo_coarse_search_t *search, const int nid2)
{
  if (search == NULL || search->rxdata == NULL || search->nb_antennas_rx <= 0 || search->rxdata_length <= 0)
    return false;
  if (search->ofdm_symbol_size <= 0 || search->ofdm_symbol_size > NR_PSS_CFO_MAX_OFDM_SYMBOL_SIZE)
    return false;
  if (search->sampling_rate_hz <= 0 || nid2 < 0 || nid2 >= NUMBER_PSS_SEQUENCE || search->pss_time[nid2] == NULL)
    return false;
  for (int antenna = 0; antenna < search->nb_antennas_rx; ++antenna) {
    if (search->rxdata[antenna] == NULL)
      return false;
  }
  return true;
}

static bool coarse_candidate_is_valid(const nr_pss_cfo_hypothesis_result_t *candidate,
                                      const nr_pss_cfo_search_config_t *config,
                                      const int rxdata_length)
{
  if (candidate == NULL || candidate->status != NR_PSS_CFO_SEARCH_OK)
    return false;
  if (candidate->nid2 < 0 || candidate->nid2 >= NUMBER_PSS_SEQUENCE)
    return false;
  if (candidate->pos < 0 || candidate->pos >= rxdata_length || candidate->peak_raw == 0 || candidate->avg_raw == 0)
    return false;
  if (candidate->cfo_hz < -config->coarse_span_hz || candidate->cfo_hz > config->coarse_span_hz)
    return false;
  return ((int64_t)candidate->cfo_hz + config->coarse_span_hz) % config->coarse_step_hz == 0;
}

static bool fine_range_is_valid(const int coarse_cfo_hz,
                                const int fine_span_hz,
                                const int sampling_rate_hz)
{
  const int64_t minimum_cfo_hz = (int64_t)coarse_cfo_hz - fine_span_hz;
  const int64_t maximum_cfo_hz = (int64_t)coarse_cfo_hz + fine_span_hz;
  if (minimum_cfo_hz < INT_MIN || maximum_cfo_hz > INT_MAX)
    return false;
  return 2LL * llabs(minimum_cfo_hz) <= sampling_rate_hz && 2LL * llabs(maximum_cfo_hz) <= sampling_rate_hz;
}

static bool candidate_is_better(const nr_pss_cfo_hypothesis_result_t *candidate,
                                const nr_pss_cfo_hypothesis_result_t *current,
                                const int coarse_cfo_hz)
{
  const unsigned __int128 candidate_score = (unsigned __int128)candidate->peak_raw * current->avg_raw;
  const unsigned __int128 current_score = (unsigned __int128)current->peak_raw * candidate->avg_raw;
  if (candidate_score != current_score)
    return candidate_score > current_score;
  if (candidate->peak_raw != current->peak_raw)
    return candidate->peak_raw > current->peak_raw;

  const long long candidate_offset = (long long)candidate->cfo_hz - coarse_cfo_hz;
  const long long current_offset = (long long)current->cfo_hz - coarse_cfo_hz;
  if (llabs(candidate_offset) != llabs(current_offset))
    return llabs(candidate_offset) < llabs(current_offset);
  if (candidate_offset != current_offset)
    return candidate_offset < current_offset;
  return candidate->pos < current->pos;
}

nr_pss_cfo_fine_search_result_t nr_pss_cfo_search_fine(const nr_pss_cfo_coarse_search_t *search,
                                                        const nr_pss_cfo_hypothesis_result_t *coarse_candidate,
                                                        const nr_pss_cfo_search_config_t *config)
{
  nr_pss_cfo_fine_search_result_t result = {
      .status = NR_PSS_CFO_SEARCH_INVALID_ARGUMENT,
      .best = {.status = NR_PSS_CFO_SEARCH_NO_CANDIDATE, .pos = -1},
  };
  if (config == NULL || !config->enabled || coarse_candidate == NULL)
    return result;
  if (!search_input_is_valid(search, coarse_candidate->nid2))
    return result;

  int fine_bins = 0;
  const nr_pss_cfo_search_status_t config_status = nr_pss_cfo_search_validate_config(config, NULL, &fine_bins);
  if (config_status != NR_PSS_CFO_SEARCH_OK) {
    result.status = config_status;
    return result;
  }
  if (!coarse_candidate_is_valid(coarse_candidate, config, search->rxdata_length))
    return result;
  if (!fine_range_is_valid(coarse_candidate->cfo_hz, config->fine_span_hz, search->sampling_rate_hz))
    return result;

  result.coarse_cfo_hz = coarse_candidate->cfo_hz;
  bool found_candidate = false;
  for (int bin = 0; bin < fine_bins; ++bin) {
    const int fine_cfo_hz = (int)(-(int64_t)config->fine_span_hz + (int64_t)bin * config->fine_step_hz);
    const int total_cfo_hz = result.coarse_cfo_hz + fine_cfo_hz;
    const nr_pss_cfo_hypothesis_t hypothesis = {
        .rxdata = search->rxdata,
        .nb_antennas_rx = search->nb_antennas_rx,
        .rxdata_length = search->rxdata_length,
        .ofdm_symbol_size = search->ofdm_symbol_size,
        .pss_time = search->pss_time[coarse_candidate->nid2],
        .sampling_rate_hz = search->sampling_rate_hz,
        .cfo_hz = total_cfo_hz,
        .nid2 = coarse_candidate->nid2,
    };
    const nr_pss_cfo_hypothesis_result_t candidate = nr_pss_cfo_evaluate_hypothesis(&hypothesis);
    ++result.bins_evaluated;
    ++result.hypotheses_evaluated;
    if (candidate.status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT) {
      result.status = candidate.status;
      return result;
    }
    if (candidate.status != NR_PSS_CFO_SEARCH_OK)
      continue;
    if (!found_candidate || candidate_is_better(&candidate, &result.best, result.coarse_cfo_hz)) {
      result.best = candidate;
      result.fine_cfo_hz = candidate.cfo_hz - result.coarse_cfo_hz;
      found_candidate = true;
    }
  }

  result.status = found_candidate ? NR_PSS_CFO_SEARCH_OK : NR_PSS_CFO_SEARCH_NO_CANDIDATE;
  return result;
}
