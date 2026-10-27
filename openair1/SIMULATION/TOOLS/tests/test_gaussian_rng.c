/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gaussian_rng.h"

#define CHECK(condition)                                              \
  do {                                                                \
    if (!(condition)) {                                               \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
      exit(EXIT_FAILURE);                                             \
    }                                                                 \
  } while (0)

enum { STREAM_SIZE = 10003 };

static void check_batching(uint32_t seed)
{
  gaussian_rng_t scalar, batch;
  gaussian_rng_init(seed, &scalar);
  gaussian_rng_init(seed, &batch);
  double reference[STREAM_SIZE];
  for (size_t i = 0; i < STREAM_SIZE; ++i)
    reference[i] = gaussian_rng_sample(&scalar);
  const size_t sizes[] = {0, 1, 2, 3, 4, 7, 8, 15, 16, 17, 63, 64, 1024, 16384};
  for (size_t k = 0; k < sizeof(sizes) / sizeof(*sizes); ++k) {
    const size_t n = sizes[k];
    double *buffer = malloc((n + 2) * sizeof(*buffer));
    CHECK(buffer != NULL);
    buffer[0] = buffer[n + 1] = 12345.0;
    gaussian_rng_init(seed, &batch);
    gaussian_rng_t before = batch;
    gaussian_rng_fill(&batch, 0, NULL);
    CHECK(batch.jsr == before.jsr);
    gaussian_rng_fill(&batch, n, buffer + 1);
    gaussian_rng_init(seed, &scalar);
    for (size_t i = 0; i < n; ++i)
      CHECK(buffer[i + 1] == gaussian_rng_sample(&scalar));
    CHECK(buffer[0] == 12345.0 && buffer[n + 1] == 12345.0);
    CHECK(batch.jsr == scalar.jsr);
    free(buffer);
    if (!n)
      continue;
    gaussian_rng_init(seed, &batch);
    double actual[STREAM_SIZE];
    for (size_t offset = 0; offset < STREAM_SIZE;) {
      size_t count = STREAM_SIZE - offset < n ? STREAM_SIZE - offset : n;
      gaussian_rng_fill(&batch, count, actual + offset);
      offset += count;
    }
    CHECK(memcmp(reference, actual, sizeof(reference)) == 0);
  }
}

static void check_legacy_prefix(void)
{
  /* Captured from gaussZiggurat at cfee4c20b4 with OAI_RNGSEED=42. */
  const double expected[] = {0.0094654187418944883,
                             -1.2274830373066112,
                             -0.59659517168617193,
                             -0.12487639503099408,
                             -0.26276622742363293,
                             1.191643243105081,
                             0.31097504837540907,
                             0.48133587165194919};
  gaussian_rng_t state;
  gaussian_rng_init(42, &state);
  for (size_t i = 0; i < sizeof(expected) / sizeof(*expected); ++i)
    CHECK(fabs(gaussian_rng_sample(&state) - expected[i]) < 1e-14);
}

static void check_interleaving(void)
{
  gaussian_rng_t a, b, reference, zero, default_seed;
  gaussian_rng_init(42, &a);
  gaussian_rng_init(17, &b);
  gaussian_rng_init(42, &reference);
  gaussian_rng_init(0, &zero);
  gaussian_rng_init(123456789, &default_seed);
  for (unsigned i = 0; i < 10000; ++i) {
    CHECK(gaussian_rng_sample(&a) == gaussian_rng_sample(&reference));
    gaussian_rng_sample(&b);
    CHECK(gaussian_rng_sample(&zero) == gaussian_rng_sample(&default_seed));
    if (i % 3 == 0)
      gaussian_rng_init(i, &b);
  }
  gaussian_rng_t copy = a;
  for (unsigned i = 0; i < 10000; ++i)
    CHECK(gaussian_rng_sample(&a) == gaussian_rng_sample(&copy));
}

static void check_int32_min_candidate(void)
{
  gaussian_rng_t state;
  /* This seed makes the first SHR3 word INT32_MIN, forcing the tail path. */
  gaussian_rng_init(UINT32_C(3532752714), &state);
  double value = gaussian_rng_sample(&state);
  CHECK(isfinite(value) && value < -3.442620);
}

static void check_distribution(uint32_t seed)
{
  enum { COUNT = 1000000, BATCH = 1024 };
  gaussian_rng_t state;
  gaussian_rng_init(seed, &state);
  double sum = 0, square = 0, pair_sum = 0, previous = 0;
  unsigned tails[4] = {0}, positive = 0;
  for (unsigned offset = 0; offset < COUNT;) {
    double out[BATCH];
    size_t n = COUNT - offset < BATCH ? COUNT - offset : BATCH;
    gaussian_rng_fill(&state, n, out);
    for (size_t i = 0; i < n; ++i) {
      const double x = out[i];
      CHECK(isfinite(x));
      sum += x;
      square += x * x;
      pair_sum += previous * x;
      previous = x;
      positive += x > 0;
      for (unsigned k = 0; k < 4; ++k)
        tails[k] += fabs(x) > k + 1;
    }
    offset += n;
  }
  const double mean = sum / COUNT;
  const double variance = (square - sum * mean) / (COUNT - 1);
  CHECK(fabs(mean) <= 6 / sqrt(COUNT));
  CHECK(fabs(variance - 1) <= 6 * sqrt(2.0 / (COUNT - 1)));
  CHECK(fabs(positive - COUNT / 2.0) <= 6 * sqrt(COUNT / 4.0));
  CHECK(fabs(pair_sum / (COUNT - 1)) <= 6 / sqrt(COUNT - 1));
  for (unsigned k = 0; k < 4; ++k) {
    double p = erfc((k + 1) / sqrt(2));
    CHECK(fabs(tails[k] - COUNT * p) <= 6 * sqrt(COUNT * p * (1 - p)));
  }
}

int main(void)
{
  check_legacy_prefix();
  check_interleaving();
  check_int32_min_candidate();
  const uint32_t seeds[] = {0, 1, 42, 123456789, UINT32_MAX, UINT32_C(0x80000000), UINT32_C(3532752714)};
  for (size_t i = 0; i < sizeof(seeds) / sizeof(*seeds); ++i) {
    check_batching(seeds[i]);
    check_distribution(seeds[i]);
  }
  puts("Gaussian state, batching, legacy prefix and distribution checks passed");
  return EXIT_SUCCESS;
}
