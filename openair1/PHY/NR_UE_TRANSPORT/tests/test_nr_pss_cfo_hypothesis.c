/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "../nr_pss_cfo_hypothesis.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SYMBOL_SIZE 64
#define SEARCH_LENGTH 64
#define BUFFER_SIZE (SEARCH_LENGTH + SYMBOL_SIZE)
#define SAMPLING_RATE_HZ 64000
#define SIGNAL_POSITION 20

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

static void make_reference(c16_t reference[SYMBOL_SIZE])
{
  unsigned int state = 1;
  for (int k = 0; k < SYMBOL_SIZE; ++k) {
    state = 1664525U * state + 1013904223U;
    reference[k].r = (int16_t)(2000 + (state % 4001U));
    state = 1664525U * state + 1013904223U;
    reference[k].i = (int16_t)(-2000 - (state % 4001U));
  }
}

static void insert_signal(c16_t buffer[BUFFER_SIZE], const c16_t reference[SYMBOL_SIZE], const int cfo_hz, const int sign)
{
  memset(buffer, 0, BUFFER_SIZE * sizeof(*buffer));
  const double phase_step = 2.0 * M_PI * cfo_hz / SAMPLING_RATE_HZ;
  for (int k = 0; k < SYMBOL_SIZE; ++k) {
    const double phase = phase_step * (SIGNAL_POSITION + k);
    const double cosine = cos(phase);
    const double sine = sin(phase);
    const double real = reference[k].r * cosine - reference[k].i * sine;
    const double imag = reference[k].r * sine + reference[k].i * cosine;
    buffer[SIGNAL_POSITION + k].r = sign * saturate_int16(lround(real));
    buffer[SIGNAL_POSITION + k].i = sign * saturate_int16(lround(imag));
  }
}

static nr_pss_cfo_hypothesis_result_t evaluate(const c16_t *const *rxdata,
                                                const int antennas,
                                                const c16_t reference[SYMBOL_SIZE],
                                                const int cfo_hz)
{
  const nr_pss_cfo_hypothesis_t hypothesis = {
      .rxdata = rxdata,
      .nb_antennas_rx = antennas,
      .rxdata_length = SEARCH_LENGTH,
      .ofdm_symbol_size = SYMBOL_SIZE,
      .pss_time = reference,
      .sampling_rate_hz = SAMPLING_RATE_HZ,
      .cfo_hz = cfo_hz,
      .nid2 = 1,
  };
  return nr_pss_cfo_evaluate_hypothesis(&hypothesis);
}

static void test_zero_cfo_and_immutable_input(void)
{
  c16_t reference[SYMBOL_SIZE];
  c16_t rx[BUFFER_SIZE];
  make_reference(reference);
  insert_signal(rx, reference, 0, 1);
  c16_t reference_before[SYMBOL_SIZE];
  c16_t rx_before[BUFFER_SIZE];
  memcpy(reference_before, reference, sizeof(reference));
  memcpy(rx_before, rx, sizeof(rx));

  const c16_t *antennas[] = {rx};
  const nr_pss_cfo_hypothesis_result_t result = evaluate(antennas, 1, reference, 0);
  CHECK(result.status == NR_PSS_CFO_SEARCH_OK);
  CHECK(result.cfo_hz == 0);
  CHECK(result.nid2 == 1);
  CHECK(result.pos == SIGNAL_POSITION);
  CHECK(result.peak_raw > result.avg_raw);
  CHECK(result.positions_evaluated == SEARCH_LENGTH / NR_PSS_CFO_TIMING_STRIDE);
  CHECK(memcmp(reference, reference_before, sizeof(reference)) == 0);
  CHECK(memcmp(rx, rx_before, sizeof(rx)) == 0);

  const nr_pss_cfo_hypothesis_result_t repeated = evaluate(antennas, 1, reference, 0);
  CHECK(repeated.status == result.status);
  CHECK(repeated.pos == result.pos);
  CHECK(repeated.peak_raw == result.peak_raw);
  CHECK(repeated.avg_raw == result.avg_raw);
}

static void test_cfo_sign(void)
{
  c16_t reference[SYMBOL_SIZE];
  c16_t rx[BUFFER_SIZE];
  make_reference(reference);

  insert_signal(rx, reference, 8000, 1);
  const c16_t *positive_antennas[] = {rx};
  const nr_pss_cfo_hypothesis_result_t positive = evaluate(positive_antennas, 1, reference, 8000);
  const nr_pss_cfo_hypothesis_result_t positive_wrong = evaluate(positive_antennas, 1, reference, -8000);
  CHECK(positive.status == NR_PSS_CFO_SEARCH_OK);
  CHECK(positive.pos == SIGNAL_POSITION);
  CHECK(positive.peak_raw > positive_wrong.peak_raw);

  insert_signal(rx, reference, -8000, 1);
  const c16_t *negative_antennas[] = {rx};
  const nr_pss_cfo_hypothesis_result_t negative = evaluate(negative_antennas, 1, reference, -8000);
  const nr_pss_cfo_hypothesis_result_t negative_wrong = evaluate(negative_antennas, 1, reference, 8000);
  CHECK(negative.status == NR_PSS_CFO_SEARCH_OK);
  CHECK(negative.pos == SIGNAL_POSITION);
  CHECK(negative.peak_raw > negative_wrong.peak_raw);
}

static void test_noncoherent_antenna_sum(void)
{
  c16_t reference[SYMBOL_SIZE];
  c16_t rx0[BUFFER_SIZE];
  c16_t rx1[BUFFER_SIZE];
  make_reference(reference);
  insert_signal(rx0, reference, 4000, 1);
  insert_signal(rx1, reference, 4000, -1);

  const c16_t *one_antenna[] = {rx0};
  const c16_t *two_antennas[] = {rx0, rx1};
  const nr_pss_cfo_hypothesis_result_t one = evaluate(one_antenna, 1, reference, 4000);
  const nr_pss_cfo_hypothesis_result_t two = evaluate(two_antennas, 2, reference, 4000);
  CHECK(one.status == NR_PSS_CFO_SEARCH_OK);
  CHECK(two.status == NR_PSS_CFO_SEARCH_OK);
  CHECK(two.pos == SIGNAL_POSITION);
  /* Integer shifts can make the two powers differ slightly after a 180-degree phase change. */
  CHECK(two.peak_raw > one.peak_raw + (9 * one.peak_raw) / 10);
  CHECK(two.peak_raw < 2 * one.peak_raw + one.peak_raw / 10);
}

static void test_no_candidate_and_invalid_arguments(void)
{
  c16_t reference[SYMBOL_SIZE];
  c16_t rx[BUFFER_SIZE] = {0};
  make_reference(reference);
  const c16_t *antennas[] = {rx};

  const nr_pss_cfo_hypothesis_result_t empty = evaluate(antennas, 1, reference, 0);
  CHECK(empty.status == NR_PSS_CFO_SEARCH_NO_CANDIDATE);
  CHECK(empty.pos == -1);
  CHECK(empty.peak_raw == 0);

  const nr_pss_cfo_hypothesis_result_t null_input = nr_pss_cfo_evaluate_hypothesis(NULL);
  CHECK(null_input.status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);
  CHECK(null_input.pos == -1);

  nr_pss_cfo_hypothesis_t invalid = {
      .rxdata = antennas,
      .nb_antennas_rx = 1,
      .rxdata_length = SEARCH_LENGTH,
      .ofdm_symbol_size = SYMBOL_SIZE,
      .pss_time = reference,
      .sampling_rate_hz = 0,
      .cfo_hz = 0,
      .nid2 = 1,
  };
  CHECK(nr_pss_cfo_evaluate_hypothesis(&invalid).status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);
  invalid.sampling_rate_hz = SAMPLING_RATE_HZ;
  invalid.nid2 = 3;
  CHECK(nr_pss_cfo_evaluate_hypothesis(&invalid).status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);
  invalid.nid2 = 1;
  invalid.cfo_hz = SAMPLING_RATE_HZ;
  CHECK(nr_pss_cfo_evaluate_hypothesis(&invalid).status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);
  invalid.cfo_hz = 0;
  invalid.ofdm_symbol_size = NR_PSS_CFO_MAX_OFDM_SYMBOL_SIZE + 1;
  CHECK(nr_pss_cfo_evaluate_hypothesis(&invalid).status == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);
}

int main(void)
{
  test_zero_cfo_and_immutable_input();
  test_cfo_sign();
  test_noncoherent_antenna_sum();
  test_no_candidate_and_invalid_arguments();

  if (failures != 0) {
    fprintf(stderr, "%d CFO hypothesis test(s) failed\n", failures);
    return 1;
  }

  printf("All single CFO-PSS hypothesis tests passed\n");
  return 0;
}
