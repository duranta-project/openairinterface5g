/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "../nr_pss_cfo_coarse_search.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SYMBOL_SIZE 128
#define SEARCH_LENGTH 1024
#define BUFFER_SIZE (SEARCH_LENGTH + SYMBOL_SIZE)
#define SAMPLING_RATE_HZ 128000
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

static nr_pss_cfo_search_config_t config(const int span_hz, const int step_hz)
{
  return (nr_pss_cfo_search_config_t){
      .enabled = true,
      .coarse_span_hz = span_hz,
      .coarse_step_hz = step_hz,
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

static void test_selects_cfo_nid2_and_timing(const int injected_cfo_hz, const int injected_nid2)
{
  c16_t references[NUMBER_PSS_SEQUENCE][SYMBOL_SIZE];
  c16_t rx[BUFFER_SIZE];
  make_references(references);
  insert_signal(rx, references[injected_nid2], injected_cfo_hz);
  c16_t rx_before[BUFFER_SIZE];
  c16_t references_before[NUMBER_PSS_SEQUENCE][SYMBOL_SIZE];
  memcpy(rx_before, rx, sizeof(rx));
  memcpy(references_before, references, sizeof(references));

  const c16_t *antennas[] = {rx};
  const nr_pss_cfo_coarse_search_t search = make_search(antennas, references);
  const nr_pss_cfo_search_config_t grid = config(10000, 5000);
  const nr_pss_cfo_coarse_search_result_t result = nr_pss_cfo_search_coarse(&search, &grid);

  CHECK(result.status == NR_PSS_CFO_SEARCH_OK);
  CHECK(result.best.cfo_hz == injected_cfo_hz);
  CHECK(result.best.nid2 == injected_nid2);
  CHECK(result.best.pos == SIGNAL_POSITION);
  CHECK(result.best.peak_raw > result.best.avg_raw);
  CHECK(result.bins_evaluated == 5);
  CHECK(result.hypotheses_evaluated == 5 * NUMBER_PSS_SEQUENCE);
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
  const nr_pss_cfo_search_config_t grid = config(NR_PSS_CFO_SEARCH_DEFAULT_COARSE_SPAN_HZ,
                                                  NR_PSS_CFO_SEARCH_DEFAULT_COARSE_STEP_HZ);
  const nr_pss_cfo_coarse_search_result_t result = nr_pss_cfo_search_coarse(&search, &grid);
  CHECK(result.status == NR_PSS_CFO_SEARCH_NO_CANDIDATE);
  CHECK(result.best.pos == -1);
  CHECK(result.bins_evaluated == 25);
  CHECK(result.hypotheses_evaluated == 25 * NUMBER_PSS_SEQUENCE);
}

static void test_invalid_inputs_and_grid(void)
{
  c16_t references[NUMBER_PSS_SEQUENCE][SYMBOL_SIZE];
  c16_t rx[BUFFER_SIZE] = {0};
  make_references(references);
  const c16_t *antennas[] = {rx};
  nr_pss_cfo_coarse_search_t search = make_search(antennas, references);
  nr_pss_cfo_search_config_t grid = config(10000, 5000);

  CHECK(nr_pss_cfo_search_coarse(NULL, &grid).status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);
  CHECK(nr_pss_cfo_search_coarse(&search, NULL).status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);

  grid.enabled = false;
  CHECK(nr_pss_cfo_search_coarse(&search, &grid).status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);
  grid = config(10000, 3000);
  CHECK(nr_pss_cfo_search_coarse(&search, &grid).status == NR_PSS_CFO_SEARCH_INVALID_GRID);

  grid = config(10000, 5000);
  search.pss_time[2] = NULL;
  CHECK(nr_pss_cfo_search_coarse(&search, &grid).status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);

  search.pss_time[2] = references[2];
  grid = config(SAMPLING_RATE_HZ, SAMPLING_RATE_HZ);
  const nr_pss_cfo_coarse_search_result_t outside_nyquist = nr_pss_cfo_search_coarse(&search, &grid);
  CHECK(outside_nyquist.status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);
  CHECK(outside_nyquist.bins_evaluated == 0);
  CHECK(outside_nyquist.hypotheses_evaluated == 0);
}

static void test_tie_is_deterministic(void)
{
  c16_t references[NUMBER_PSS_SEQUENCE][SYMBOL_SIZE];
  c16_t rx[BUFFER_SIZE] = {0};
  memset(references, 0, sizeof(references));
  for (int nid2 = 0; nid2 < NUMBER_PSS_SEQUENCE; ++nid2)
    references[nid2][0] = (c16_t){.r = 4000, .i = -4000};
  rx[SIGNAL_POSITION] = references[0][0];

  const c16_t *antennas[] = {rx};
  const nr_pss_cfo_coarse_search_t search = make_search(antennas, references);
  const nr_pss_cfo_search_config_t grid = config(10000, 5000);
  const nr_pss_cfo_coarse_search_result_t result = nr_pss_cfo_search_coarse(&search, &grid);
  CHECK(result.status == NR_PSS_CFO_SEARCH_OK);
  CHECK(result.best.cfo_hz == 0);
  CHECK(result.best.nid2 == 0);
  CHECK(result.best.pos == SIGNAL_POSITION);
}

int main(void)
{
  test_selects_cfo_nid2_and_timing(5000, 2);
  test_selects_cfo_nid2_and_timing(-5000, 1);
  test_selects_cfo_nid2_and_timing(0, 0);
  test_zero_signal_has_no_candidate();
  test_invalid_inputs_and_grid();
  test_tie_is_deterministic();

  if (failures != 0) {
    fprintf(stderr, "%d coarse CFO-PSS search test(s) failed\n", failures);
    return 1;
  }

  printf("All coarse CFO-PSS search tests passed\n");
  return 0;
}
