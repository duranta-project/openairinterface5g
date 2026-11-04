/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_pss_cfo_search.h"
#include "PHY/TOOLS/tools_defs.h"

static nr_pss_cfo_hypothesis_t make_hypothesis(const nr_pss_cfo_coarse_search_t *search,
                                                const int cfo_hz,
                                                const int nid2)
{
  return (nr_pss_cfo_hypothesis_t){
      .rxdata = search->rxdata,
      .nb_antennas_rx = search->nb_antennas_rx,
      .rxdata_length = search->rxdata_length,
      .ofdm_symbol_size = search->ofdm_symbol_size,
      .pss_time = search->pss_time[nid2],
      .sampling_rate_hz = search->sampling_rate_hz,
      .cfo_hz = cfo_hz,
      .nid2 = nid2,
  };
}

nr_pss_cfo_search_result_t nr_pss_cfo_search_stage1(const nr_pss_cfo_coarse_search_t *search,
                                                     const nr_pss_cfo_search_config_t *config)
{
  nr_pss_cfo_search_result_t result = {
      .status = NR_PSS_CFO_SEARCH_INVALID_ARGUMENT,
      .pss = {.success = false, .pos = -1},
  };

  const nr_pss_cfo_coarse_search_result_t coarse = nr_pss_cfo_search_coarse(search, config);
  result.coarse_bins_evaluated = coarse.bins_evaluated;
  if (coarse.status != NR_PSS_CFO_SEARCH_OK) {
    result.status = coarse.status;
    return result;
  }

  const nr_pss_cfo_fine_search_result_t fine = nr_pss_cfo_search_fine(search, &coarse.best, config);
  result.fine_bins_evaluated = fine.bins_evaluated;
  if (fine.status != NR_PSS_CFO_SEARCH_OK) {
    result.status = fine.status;
    return result;
  }

  uint64_t second_sequence_peak_raw = 0;
  for (int nid2 = 0; nid2 < NUMBER_PSS_SEQUENCE; ++nid2) {
    if (nid2 == fine.best.nid2)
      continue;
    const nr_pss_cfo_hypothesis_t alternative = make_hypothesis(search, fine.best.cfo_hz, nid2);
    const nr_pss_cfo_hypothesis_result_t alternative_result = nr_pss_cfo_evaluate_hypothesis(&alternative);
    ++result.diagnostic_passes_evaluated;
    if (alternative_result.status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT) {
      result.status = alternative_result.status;
      return result;
    }
    if (alternative_result.status == NR_PSS_CFO_SEARCH_OK
        && alternative_result.peak_raw > second_sequence_peak_raw)
      second_sequence_peak_raw = alternative_result.peak_raw;
  }

  const nr_pss_cfo_hypothesis_t winner = make_hypothesis(search, fine.best.cfo_hz, fine.best.nid2);
  const nr_pss_cfo_search_status_t timing_status = nr_pss_cfo_evaluate_second_timing_peak(&winner,
                                                                                          fine.best.pos,
                                                                                          search->ofdm_symbol_size,
                                                                                          &result.second_timing_peak_raw);
  ++result.diagnostic_passes_evaluated;
  if (timing_status != NR_PSS_CFO_SEARCH_OK) {
    result.status = timing_status;
    return result;
  }

  result.status = NR_PSS_CFO_SEARCH_OK;
  result.coarse_cfo_hz = fine.coarse_cfo_hz;
  result.fine_cfo_hz = fine.fine_cfo_hz;
  result.residual_cfo_hz = 0;
  result.pss = (pss_detection_result_t){
      .success = true,
      .pos = fine.best.pos,
      .nid2 = fine.best.nid2,
      .freq_offset = fine.best.cfo_hz,
      .peak = dB_fixed64(fine.best.peak_raw),
      .avg = dB_fixed64(fine.best.avg_raw),
      .peak_raw = fine.best.peak_raw,
      .avg_raw = fine.best.avg_raw,
      .second_sequence_peak_raw = second_sequence_peak_raw,
  };
  return result;
}
