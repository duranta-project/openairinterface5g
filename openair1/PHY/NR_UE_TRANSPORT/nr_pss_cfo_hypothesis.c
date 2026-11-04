/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_pss_cfo_hypothesis.h"
#include "PHY/NR_REFSIG/ss_pbch_nr.h"
#include "PHY/TOOLS/tools_defs.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int16_t saturate_int16(const long value)
{
  if (value > INT16_MAX)
    return INT16_MAX;
  if (value < INT16_MIN)
    return INT16_MIN;
  return (int16_t)value;
}

static void rotate_pss_reference(const c16_t *input,
                                 c16_t *output,
                                 const int length,
                                 const int cfo_hz,
                                 const int sampling_rate_hz)
{
  if (cfo_hz == 0) {
    memcpy(output, input, length * sizeof(*output));
    return;
  }

  /*
   * Correcting rx[n] by exp(-j*w*n) is equivalent in correlation power to
   * rotating the PSS reference by exp(+j*w*k). The omitted exp(-j*w*n0)
   * term is common to the complete dot product and therefore has unit power.
   */
  const double phase_step = 2.0 * M_PI * cfo_hz / sampling_rate_hz;
  for (int k = 0; k < length; ++k) {
    const double phase = phase_step * k;
    const double cosine = cos(phase);
    const double sine = sin(phase);
    const double real = input[k].r * cosine - input[k].i * sine;
    const double imag = input[k].r * sine + input[k].i * cosine;
    output[k].r = saturate_int16(lround(real));
    output[k].i = saturate_int16(lround(imag));
  }
}

static uint64_t correlation_power(const c32_t correlation)
{
  const uint64_t real = correlation.r < 0 ? (uint64_t)(-(int64_t)correlation.r) : (uint64_t)correlation.r;
  const uint64_t imag = correlation.i < 0 ? (uint64_t)(-(int64_t)correlation.i) : (uint64_t)correlation.i;
  return real * real + imag * imag;
}

static uint64_t saturating_add(const uint64_t left, const uint64_t right)
{
  return UINT64_MAX - left < right ? UINT64_MAX : left + right;
}

static bool hypothesis_is_valid(const nr_pss_cfo_hypothesis_t *hypothesis)
{
  if (hypothesis == NULL || hypothesis->rxdata == NULL || hypothesis->pss_time == NULL)
    return false;
  if (hypothesis->nb_antennas_rx <= 0 || hypothesis->rxdata_length <= 0)
    return false;
  if (hypothesis->ofdm_symbol_size <= 0 || hypothesis->ofdm_symbol_size > NR_PSS_CFO_MAX_OFDM_SYMBOL_SIZE)
    return false;
  if (hypothesis->sampling_rate_hz <= 0 || hypothesis->nid2 < 0 || hypothesis->nid2 >= NUMBER_PSS_SEQUENCE)
    return false;
  if (2LL * llabs((long long)hypothesis->cfo_hz) > hypothesis->sampling_rate_hz)
    return false;
  for (int antenna = 0; antenna < hypothesis->nb_antennas_rx; ++antenna) {
    if (hypothesis->rxdata[antenna] == NULL)
      return false;
  }
  return true;
}

nr_pss_cfo_hypothesis_result_t nr_pss_cfo_evaluate_hypothesis(const nr_pss_cfo_hypothesis_t *hypothesis)
{
  nr_pss_cfo_hypothesis_result_t result = {.status = NR_PSS_CFO_SEARCH_INVALID_ARGUMENT, .pos = -1};
  if (!hypothesis_is_valid(hypothesis))
    return result;

  result.cfo_hz = hypothesis->cfo_hz;
  result.nid2 = hypothesis->nid2;

  c16_t rotated_pss[hypothesis->ofdm_symbol_size] __attribute__((aligned(32)));
  rotate_pss_reference(hypothesis->pss_time,
                       rotated_pss,
                       hypothesis->ofdm_symbol_size,
                       hypothesis->cfo_hz,
                       hypothesis->sampling_rate_hz);

  uint64_t power_sum = 0;
  for (int position = 0; position < hypothesis->rxdata_length; position += NR_PSS_CFO_TIMING_STRIDE) {
    uint64_t position_power = 0;
    for (int antenna = 0; antenna < hypothesis->nb_antennas_rx; ++antenna) {
      const c32_t correlation = dot_product(rotated_pss,
                                            &hypothesis->rxdata[antenna][position],
                                            hypothesis->ofdm_symbol_size,
                                            SCALING_PSS_NR);
      position_power = saturating_add(position_power, correlation_power(correlation));
    }

    power_sum = saturating_add(power_sum, position_power);
    ++result.positions_evaluated;
    if (position_power > result.peak_raw) {
      result.peak_raw = position_power;
      result.pos = position;
    }
  }

  result.avg_raw = power_sum / result.positions_evaluated;
  if (result.peak_raw == 0 || result.avg_raw == 0) {
    result.status = NR_PSS_CFO_SEARCH_NO_CANDIDATE;
    result.pos = -1;
    return result;
  }

  result.status = NR_PSS_CFO_SEARCH_OK;
  return result;
}

nr_pss_cfo_search_status_t nr_pss_cfo_evaluate_second_timing_peak(const nr_pss_cfo_hypothesis_t *hypothesis,
                                                                   const int best_pos,
                                                                   const int guard_samples,
                                                                   uint64_t *second_peak_raw)
{
  if (second_peak_raw != NULL)
    *second_peak_raw = 0;
  if (!hypothesis_is_valid(hypothesis) || second_peak_raw == NULL || best_pos < 0
      || best_pos >= hypothesis->rxdata_length || guard_samples < 0)
    return NR_PSS_CFO_SEARCH_INVALID_ARGUMENT;

  c16_t rotated_pss[hypothesis->ofdm_symbol_size] __attribute__((aligned(32)));
  rotate_pss_reference(hypothesis->pss_time,
                       rotated_pss,
                       hypothesis->ofdm_symbol_size,
                       hypothesis->cfo_hz,
                       hypothesis->sampling_rate_hz);

  for (int position = 0; position < hypothesis->rxdata_length; position += NR_PSS_CFO_TIMING_STRIDE) {
    if (llabs((long long)position - best_pos) <= guard_samples)
      continue;

    uint64_t position_power = 0;
    for (int antenna = 0; antenna < hypothesis->nb_antennas_rx; ++antenna) {
      const c32_t correlation = dot_product(rotated_pss,
                                            &hypothesis->rxdata[antenna][position],
                                            hypothesis->ofdm_symbol_size,
                                            SCALING_PSS_NR);
      position_power = saturating_add(position_power, correlation_power(correlation));
    }
    if (position_power > *second_peak_raw)
      *second_peak_raw = position_power;
  }

  return NR_PSS_CFO_SEARCH_OK;
}
