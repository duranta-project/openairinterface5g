/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "../nr_pss_cfo_search.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SYMBOL_SIZE 128
#define SEARCH_LENGTH 1536
#define BUFFER_SIZE (SEARCH_LENGTH + SYMBOL_SIZE)
#define SAMPLING_RATE_HZ 1280000
#define MAIN_POSITION 404
#define SECOND_POSITION 1000

static int failures;

#define CHECK(condition)                                                                                                         \
  do {                                                                                                                           \
    if (!(condition)) {                                                                                                          \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                                                      \
      failures++;                                                                                                                \
    }                                                                                                                            \
  } while (0)

static int16_t saturate_int16(const long value)
{
  if (value > INT16_MAX)
    return INT16_MAX;
  if (value < INT16_MIN)
    return INT16_MIN;
  return (int16_t)value;
}

static void make_references(c16_t references[NUMBER_PSS_SEQUENCE][SYMBOL_SIZE])
{
  for (int nid2 = 0; nid2 < NUMBER_PSS_SEQUENCE; ++nid2) {
    unsigned int state = 1U + 7919U * nid2;
    for (int k = 0; k < SYMBOL_SIZE; ++k) {
      state = 1664525U * state + 1013904223U;
      references[nid2][k].r = ((state >> 16) & 1U) != 0 ? 4000 : -4000;
      state = 1664525U * state + 1013904223U;
      references[nid2][k].i = ((state >> 16) & 1U) != 0 ? 4000 : -4000;
    }
  }
}

static void add_signal(c16_t buffer[BUFFER_SIZE],
                       const c16_t reference[SYMBOL_SIZE],
                       const int position,
                       const int cfo_hz,
                       const double amplitude)
{
  const double phase_step = 2.0 * M_PI * cfo_hz / SAMPLING_RATE_HZ;
  for (int k = 0; k < SYMBOL_SIZE; ++k) {
    const double phase = phase_step * (position + k);
    const double cosine = cos(phase);
    const double sine = sin(phase);
    const double real = amplitude * (reference[k].r * cosine - reference[k].i * sine);
    const double imag = amplitude * (reference[k].r * sine + reference[k].i * cosine);
    buffer[position + k].r = saturate_int16(lround(buffer[position + k].r + real));
    buffer[position + k].i = saturate_int16(lround(buffer[position + k].i + imag));
  }
}

static nr_pss_cfo_coarse_search_t make_search(const c16_t *const *rxdata,
                                               c16_t references[NUMBER_PSS_SEQUENCE][SYMBOL_SIZE])
{
  nr_pss_cfo_coarse_search_t search = {
      .rxdata = rxdata,
      .nb_antennas_rx = 1,
      .rxdata_length = SEARCH_LENGTH,
      .ofdm_symbol_size = SYMBOL_SIZE,
      .sampling_rate_hz = SAMPLING_RATE_HZ,
  };
  for (int nid2 = 0; nid2 < NUMBER_PSS_SEQUENCE; ++nid2)
    search.pss_time[nid2] = references[nid2];
  return search;
}

static nr_pss_cfo_search_config_t blind_config(void)
{
  return (nr_pss_cfo_search_config_t){
      .enabled = true,
      .coarse_span_hz = NR_PSS_CFO_SEARCH_DEFAULT_COARSE_SPAN_HZ,
      .coarse_step_hz = NR_PSS_CFO_SEARCH_DEFAULT_COARSE_STEP_HZ,
      .fine_span_hz = NR_PSS_CFO_SEARCH_DEFAULT_FINE_SPAN_HZ,
      .fine_step_hz = NR_PSS_CFO_SEARCH_DEFAULT_FINE_STEP_HZ,
  };
}

static void test_complete_stage1_result(void)
{
  c16_t references[NUMBER_PSS_SEQUENCE][SYMBOL_SIZE];
  c16_t rx[BUFFER_SIZE] = {0};
  make_references(references);
  add_signal(rx, references[2], MAIN_POSITION, 57340, 1.0);
  add_signal(rx, references[2], SECOND_POSITION, 57340, 0.5);
  c16_t rx_before[BUFFER_SIZE];
  c16_t references_before[NUMBER_PSS_SEQUENCE][SYMBOL_SIZE];
  memcpy(rx_before, rx, sizeof(rx));
  memcpy(references_before, references, sizeof(references));

  const c16_t *antennas[] = {rx};
  const nr_pss_cfo_coarse_search_t search = make_search(antennas, references);
  const nr_pss_cfo_search_config_t config = blind_config();
  const nr_pss_cfo_search_result_t result = nr_pss_cfo_search_stage1(&search, &config);

  CHECK(result.status == NR_PSS_CFO_SEARCH_OK);
  CHECK(result.pss.success);
  CHECK(result.pss.nid2 == 2);
  CHECK(result.pss.pos == MAIN_POSITION);
  CHECK(result.coarse_cfo_hz == 55000);
  CHECK(result.fine_cfo_hz == 2400);
  CHECK(result.residual_cfo_hz == 0);
  CHECK(result.pss.freq_offset == 57400);
  CHECK(abs(result.pss.freq_offset - 57340) <= NR_PSS_CFO_SEARCH_DEFAULT_FINE_STEP_HZ / 2);
  CHECK(result.pss.peak_raw > result.pss.avg_raw);
  CHECK(result.pss.second_sequence_peak_raw > 0);
  CHECK(result.pss.second_sequence_peak_raw < result.pss.peak_raw);
  CHECK(result.second_timing_peak_raw > 0);
  CHECK(result.second_timing_peak_raw < result.pss.peak_raw);
  CHECK(result.coarse_bins_evaluated == 25);
  CHECK(result.fine_bins_evaluated == 31);
  CHECK(result.diagnostic_passes_evaluated == 3);
  CHECK(memcmp(rx, rx_before, sizeof(rx)) == 0);
  CHECK(memcmp(references, references_before, sizeof(references)) == 0);
}

static void test_zero_signal_and_invalid_config(void)
{
  c16_t references[NUMBER_PSS_SEQUENCE][SYMBOL_SIZE];
  c16_t rx[BUFFER_SIZE] = {0};
  make_references(references);
  const c16_t *antennas[] = {rx};
  const nr_pss_cfo_coarse_search_t search = make_search(antennas, references);
  nr_pss_cfo_search_config_t config = blind_config();

  const nr_pss_cfo_search_result_t empty = nr_pss_cfo_search_stage1(&search, &config);
  CHECK(empty.status == NR_PSS_CFO_SEARCH_NO_CANDIDATE);
  CHECK(!empty.pss.success);
  CHECK(empty.pss.pos == -1);
  CHECK(empty.coarse_bins_evaluated == 25);
  CHECK(empty.fine_bins_evaluated == 0);
  CHECK(empty.diagnostic_passes_evaluated == 0);

  config.fine_step_hz = 700;
  const nr_pss_cfo_search_result_t invalid = nr_pss_cfo_search_stage1(&search, &config);
  CHECK(invalid.status == NR_PSS_CFO_SEARCH_INVALID_GRID);
  CHECK(!invalid.pss.success);
  CHECK(invalid.coarse_bins_evaluated == 0);

  CHECK(nr_pss_cfo_search_stage1(NULL, &config).status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);
  CHECK(nr_pss_cfo_search_stage1(&search, NULL).status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);
}

static void test_diagnostic_metrics(void)
{
  c16_t references[NUMBER_PSS_SEQUENCE][SYMBOL_SIZE];
  c16_t rx[BUFFER_SIZE] = {0};
  make_references(references);
  memcpy(references[1], references[0], sizeof(references[0]));
  add_signal(rx, references[0], MAIN_POSITION, 0, 1.0);

  const c16_t *antennas[] = {rx};
  const nr_pss_cfo_coarse_search_t search = make_search(antennas, references);
  const nr_pss_cfo_search_config_t config = blind_config();
  const nr_pss_cfo_search_result_t result = nr_pss_cfo_search_stage1(&search, &config);
  CHECK(result.status == NR_PSS_CFO_SEARCH_OK);
  CHECK(result.pss.nid2 == 0);
  CHECK(result.pss.pos == MAIN_POSITION);
  CHECK(result.pss.freq_offset == 0);
  CHECK(result.pss.second_sequence_peak_raw == result.pss.peak_raw);
  CHECK(result.second_timing_peak_raw == 0);
  CHECK(result.diagnostic_passes_evaluated == 3);
}

int main(void)
{
  test_complete_stage1_result();
  test_zero_signal_and_invalid_config();
  test_diagnostic_metrics();

  if (failures != 0) {
    fprintf(stderr, "%d complete Stage 1 test(s) failed\n", failures);
    return 1;
  }

  printf("All complete CFO-PSS Stage 1 tests passed\n");
  return 0;
}
