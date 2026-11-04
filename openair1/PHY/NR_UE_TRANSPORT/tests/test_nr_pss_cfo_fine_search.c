/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "../nr_pss_cfo_fine_search.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SYMBOL_SIZE 128
#define SEARCH_LENGTH 1024
#define BUFFER_SIZE (SEARCH_LENGTH + SYMBOL_SIZE)
#define SAMPLING_RATE_HZ 1280000
#define SIGNAL_POSITION 404

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

static void insert_signal(c16_t buffer[BUFFER_SIZE], const c16_t reference[SYMBOL_SIZE], const int cfo_hz)
{
  memset(buffer, 0, BUFFER_SIZE * sizeof(*buffer));
  const double phase_step = 2.0 * M_PI * cfo_hz / SAMPLING_RATE_HZ;
  for (int k = 0; k < SYMBOL_SIZE; ++k) {
    const double phase = phase_step * (SIGNAL_POSITION + k);
    const double cosine = cos(phase);
    const double sine = sin(phase);
    const double real = reference[k].r * cosine - reference[k].i * sine;
    const double imag = reference[k].r * sine + reference[k].i * cosine;
    buffer[SIGNAL_POSITION + k].r = saturate_int16(lround(real));
    buffer[SIGNAL_POSITION + k].i = saturate_int16(lround(imag));
  }
}

static nr_pss_cfo_search_config_t make_config(const int coarse_span_hz)
{
  return (nr_pss_cfo_search_config_t){
      .enabled = true,
      .coarse_span_hz = coarse_span_hz,
      .coarse_step_hz = NR_PSS_CFO_SEARCH_DEFAULT_COARSE_STEP_HZ,
      .fine_span_hz = NR_PSS_CFO_SEARCH_DEFAULT_FINE_SPAN_HZ,
      .fine_step_hz = NR_PSS_CFO_SEARCH_DEFAULT_FINE_STEP_HZ,
  };
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

static nr_pss_cfo_hypothesis_result_t coarse_candidate(const int coarse_cfo_hz, const int nid2)
{
  return (nr_pss_cfo_hypothesis_result_t){
      .status = NR_PSS_CFO_SEARCH_OK,
      .cfo_hz = coarse_cfo_hz,
      .nid2 = nid2,
      .pos = SIGNAL_POSITION,
      .peak_raw = 100,
      .avg_raw = 10,
      .positions_evaluated = SEARCH_LENGTH / NR_PSS_CFO_TIMING_STRIDE,
  };
}

static void test_refines_cfo(const int injected_cfo_hz,
                             const int coarse_cfo_hz,
                             const int expected_fine_cfo_hz,
                             const int nid2,
                             const int coarse_span_hz)
{
  c16_t references[NUMBER_PSS_SEQUENCE][SYMBOL_SIZE];
  c16_t rx[BUFFER_SIZE];
  make_references(references);
  insert_signal(rx, references[nid2], injected_cfo_hz);
  c16_t rx_before[BUFFER_SIZE];
  c16_t references_before[NUMBER_PSS_SEQUENCE][SYMBOL_SIZE];
  memcpy(rx_before, rx, sizeof(rx));
  memcpy(references_before, references, sizeof(references));

  const c16_t *antennas[] = {rx};
  const nr_pss_cfo_coarse_search_t search = make_search(antennas, references);
  const nr_pss_cfo_hypothesis_result_t coarse = coarse_candidate(coarse_cfo_hz, nid2);
  const nr_pss_cfo_search_config_t config = make_config(coarse_span_hz);
  const nr_pss_cfo_fine_search_result_t result = nr_pss_cfo_search_fine(&search, &coarse, &config);

  CHECK(result.status == NR_PSS_CFO_SEARCH_OK);
  CHECK(result.coarse_cfo_hz == coarse_cfo_hz);
  CHECK(result.fine_cfo_hz == expected_fine_cfo_hz);
  CHECK(result.best.cfo_hz == injected_cfo_hz);
  CHECK(result.best.nid2 == nid2);
  CHECK(result.best.pos == SIGNAL_POSITION);
  CHECK(result.bins_evaluated == 31);
  CHECK(result.hypotheses_evaluated == 31);
  CHECK(memcmp(rx, rx_before, sizeof(rx)) == 0);
  CHECK(memcmp(references, references_before, sizeof(references)) == 0);
}

static void test_zero_signal_has_no_candidate(void)
{
  c16_t references[NUMBER_PSS_SEQUENCE][SYMBOL_SIZE];
  c16_t rx[BUFFER_SIZE] = {0};
  make_references(references);
  const c16_t *antennas[] = {rx};
  const nr_pss_cfo_coarse_search_t search = make_search(antennas, references);
  const nr_pss_cfo_hypothesis_result_t coarse = coarse_candidate(5000, 1);
  const nr_pss_cfo_search_config_t config = make_config(15000);
  const nr_pss_cfo_fine_search_result_t result = nr_pss_cfo_search_fine(&search, &coarse, &config);
  CHECK(result.status == NR_PSS_CFO_SEARCH_NO_CANDIDATE);
  CHECK(result.best.pos == -1);
  CHECK(result.bins_evaluated == 31);
  CHECK(result.hypotheses_evaluated == 31);
}

static void test_invalid_inputs(void)
{
  c16_t references[NUMBER_PSS_SEQUENCE][SYMBOL_SIZE];
  c16_t rx[BUFFER_SIZE] = {0};
  make_references(references);
  const c16_t *antennas[] = {rx};
  nr_pss_cfo_coarse_search_t search = make_search(antennas, references);
  nr_pss_cfo_hypothesis_result_t coarse = coarse_candidate(5000, 1);
  nr_pss_cfo_search_config_t config = make_config(15000);

  CHECK(nr_pss_cfo_search_fine(NULL, &coarse, &config).status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);
  CHECK(nr_pss_cfo_search_fine(&search, NULL, &config).status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);
  CHECK(nr_pss_cfo_search_fine(&search, &coarse, NULL).status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);

  config.enabled = false;
  CHECK(nr_pss_cfo_search_fine(&search, &coarse, &config).status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);
  config = make_config(15000);
  config.fine_step_hz = 700;
  CHECK(nr_pss_cfo_search_fine(&search, &coarse, &config).status == NR_PSS_CFO_SEARCH_INVALID_GRID);

  config = make_config(15000);
  coarse.status = NR_PSS_CFO_SEARCH_NO_CANDIDATE;
  CHECK(nr_pss_cfo_search_fine(&search, &coarse, &config).status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);
  coarse = coarse_candidate(3000, 1);
  CHECK(nr_pss_cfo_search_fine(&search, &coarse, &config).status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);
  coarse = coarse_candidate(5000, 3);
  CHECK(nr_pss_cfo_search_fine(&search, &coarse, &config).status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);

  coarse = coarse_candidate(640000, 1);
  config = make_config(640000);
  const nr_pss_cfo_fine_search_result_t outside_nyquist = nr_pss_cfo_search_fine(&search, &coarse, &config);
  CHECK(outside_nyquist.status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);
  CHECK(outside_nyquist.bins_evaluated == 0);

  coarse = coarse_candidate(5000, 1);
  config = make_config(15000);
  search.pss_time[1] = NULL;
  CHECK(nr_pss_cfo_search_fine(&search, &coarse, &config).status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);
}

static void test_tie_keeps_coarse_cfo(void)
{
  c16_t references[NUMBER_PSS_SEQUENCE][SYMBOL_SIZE] = {0};
  c16_t rx[BUFFER_SIZE] = {0};
  references[1][0] = (c16_t){.r = 4000, .i = -4000};
  rx[SIGNAL_POSITION] = references[1][0];

  const c16_t *antennas[] = {rx};
  const nr_pss_cfo_coarse_search_t search = make_search(antennas, references);
  const nr_pss_cfo_hypothesis_result_t coarse = coarse_candidate(5000, 1);
  const nr_pss_cfo_search_config_t config = make_config(15000);
  const nr_pss_cfo_fine_search_result_t result = nr_pss_cfo_search_fine(&search, &coarse, &config);
  CHECK(result.status == NR_PSS_CFO_SEARCH_OK);
  CHECK(result.coarse_cfo_hz == 5000);
  CHECK(result.fine_cfo_hz == 0);
  CHECK(result.best.cfo_hz == 5000);
  CHECK(result.best.nid2 == 1);
  CHECK(result.best.pos == SIGNAL_POSITION);
}

static void test_blind_coarse_to_fine_pipeline(void)
{
  c16_t references[NUMBER_PSS_SEQUENCE][SYMBOL_SIZE];
  c16_t rx[BUFFER_SIZE];
  make_references(references);
  insert_signal(rx, references[0], 57400);

  const c16_t *antennas[] = {rx};
  const nr_pss_cfo_coarse_search_t search = make_search(antennas, references);
  const nr_pss_cfo_search_config_t config = make_config(60000);
  const nr_pss_cfo_coarse_search_result_t coarse = nr_pss_cfo_search_coarse(&search, &config);
  CHECK(coarse.status == NR_PSS_CFO_SEARCH_OK);
  CHECK(coarse.best.cfo_hz == 55000);
  CHECK(coarse.best.nid2 == 0);
  CHECK(coarse.best.pos == SIGNAL_POSITION);
  CHECK(coarse.bins_evaluated == 25);
  CHECK(coarse.hypotheses_evaluated == 75);

  const nr_pss_cfo_fine_search_result_t fine = nr_pss_cfo_search_fine(&search, &coarse.best, &config);
  CHECK(fine.status == NR_PSS_CFO_SEARCH_OK);
  CHECK(fine.coarse_cfo_hz == 55000);
  CHECK(fine.fine_cfo_hz == 2400);
  CHECK(fine.best.cfo_hz == 57400);
  CHECK(fine.best.nid2 == 0);
  CHECK(fine.best.pos == SIGNAL_POSITION);
  CHECK(coarse.hypotheses_evaluated + fine.hypotheses_evaluated == 106);
}

static void test_off_grid_cfo_is_quantized(void)
{
  c16_t references[NUMBER_PSS_SEQUENCE][SYMBOL_SIZE];
  c16_t rx[BUFFER_SIZE];
  make_references(references);
  insert_signal(rx, references[2], 57340);

  const c16_t *antennas[] = {rx};
  const nr_pss_cfo_coarse_search_t search = make_search(antennas, references);
  const nr_pss_cfo_hypothesis_result_t coarse = coarse_candidate(55000, 2);
  const nr_pss_cfo_search_config_t config = make_config(60000);
  const nr_pss_cfo_fine_search_result_t fine = nr_pss_cfo_search_fine(&search, &coarse, &config);
  CHECK(fine.status == NR_PSS_CFO_SEARCH_OK);
  CHECK(fine.coarse_cfo_hz == 55000);
  CHECK(fine.fine_cfo_hz == 2400);
  CHECK(fine.best.cfo_hz == 57400);
  CHECK(abs(fine.best.cfo_hz - 57340) <= NR_PSS_CFO_SEARCH_DEFAULT_FINE_STEP_HZ / 2);
  CHECK(fine.best.nid2 == 2);
  CHECK(fine.best.pos == SIGNAL_POSITION);
}

int main(void)
{
  test_refines_cfo(7400, 5000, 2400, 2, 15000);
  test_refines_cfo(-7400, -5000, -2400, 1, 15000);
  test_refines_cfo(57400, 55000, 2400, 0, 60000);
  test_refines_cfo(11000, 5000, 6000, 2, 15000);
  test_zero_signal_has_no_candidate();
  test_invalid_inputs();
  test_tie_keeps_coarse_cfo();
  test_blind_coarse_to_fine_pipeline();
  test_off_grid_cfo_is_quantized();

  if (failures != 0) {
    fprintf(stderr, "%d fine CFO-PSS search test(s) failed\n", failures);
    return 1;
  }

  printf("All fine CFO-PSS search tests passed\n");
  return 0;
}
