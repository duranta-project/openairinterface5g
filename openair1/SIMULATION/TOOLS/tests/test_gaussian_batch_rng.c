/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
#include "sim.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                      \
  do {                                                \
    if (!(x)) {                                       \
      fprintf(stderr, "line %d: %s\n", __LINE__, #x); \
      abort();                                        \
    }                                                 \
  } while (0)
void scalar_gaussian_rng_init(uint64_t, gaussian_rng_t *);
void scalar_gaussian_rng_fill(gaussian_rng_t *, size_t, double *);
void scalar_gaussian_rng_fill_float(gaussian_rng_t *, size_t, float *);
static int16_t narrow(double x)
{
  int32_t v = (x >= -0x1p31 && x < 0x1p31) ? (int32_t)x : 0;
  unsigned lo = (uint32_t)v & 65535;
  return (int16_t)(lo < 32768 ? (int)lo : (int)lo - 65536);
}
static void reference_phase_noise(double ts, int16_t *re, int16_t *im)
{
  static uint64_t index;
  c16_t x = {*re, *im};
  c16_t rotation = get_sin_cos((double)(index++ * 300) * ts);
  x = c16mulShift(x, rotation, 14);
  *re = x.r;
  *im = x.i;
}
static void callers(bool pending)
{
  const unsigned lengths[] = {0, 1, 2, 3, 4, 7, 8, 16, 31, 32, 33, 63, 64, 65, 255, 1025};
  const float sigmas[] = {0, 2, 20, 1000000};
  InitSinLUT();
  for (unsigned ant = 1; ant <= 4; ant++)
    for (unsigned l = 0; l < 16; l++)
      for (unsigned v = 0; v < 4; v++)
        for (unsigned phase = 0; phase < 2; phase++) {
          unsigned n = lengths[l];
          float *re[4], *im[4];
          c16_t *rx[4];
          for (unsigned a = 0; a < ant; a++) {
            re[a] = malloc((n + 1) * sizeof(float));
            im[a] = malloc((n + 1) * sizeof(float));
            rx[a] = malloc((n + 13) * sizeof(c16_t));
            for (unsigned i = 0; i < n; i++) {
              re[a][i] = (i % 29) - 14.25f;
              im[a][i] = 25.5f - (int)(i % 51);
            }
            for (unsigned i = 0; i < n + 13; i++)
              rx[a][i] = (c16_t){12345, -23456};
          }
          gaussian_rng_t s, r;
          gaussian_rng_init(42, &s);
          float discarded;
          if (pending)
            gaussian_rng_fill_float(&s, 1, &discarded);
          r = s;
          float f[8200];
          unsigned total = 2 * n * ant;
          gaussian_rng_fill_float(&r, total, f);
          add_noise_float_batch(rx, (const float **)re, (const float **)im, sigmas[v], n, 4, 1. / 30720000, 3, phase, ant, &s);
          float amp = sqrtf(sigmas[v] / 2);
          for (unsigned i = 0; i < n; i++)
            for (unsigned a = 0; a < ant; a++) {
              unsigned k = 2 * (i * ant + a);
              c16_t x;
              x.r = narrow(fmaf(f[k], amp, re[a][i]));
              x.i = narrow(fmaf(f[k + 1], amp, im[a][i]));
              if (phase)
                reference_phase_noise(1. / 30720000, &x.r, &x.i);
              CHECK(rx[a][i + 7].r == x.r);
              CHECK(rx[a][i + 7].i == x.i);
              {
                CHECK(abs(narrow(fma((double)f[k], amp, re[a][i])) - narrow(fmaf(f[k], amp, re[a][i]))) <= 1);
              }
            }
          CHECK(!memcmp(s.words, r.words, sizeof(s.words)));
          CHECK(!memcmp(s.repair, r.repair, sizeof(s.repair)));
          for (unsigned a = 0; a < ant; a++) {
            CHECK(rx[a][6].r == 12345);
            CHECK(rx[a][n + 7].i == -23456);
            free(re[a]);
            free(im[a]);
            free(rx[a]);
          }
        }
}
static void compatibility(void)
{
  gaussian_rng_t expected;
  gaussian_rng_init(42, &expected);
  setenv("OAI_RNGSEED", "42", 1);
  randominit();
  for (unsigned i = 0; i < 13; i++)
    CHECK(gaussZiggurat(0, 1) == gaussian_rng_sample(&expected));
  float re[33] = {0}, im[33] = {0};
  const float *r[] = {re}, *q[] = {im};
  c16_t a[33], b[33];
  c16_t *pa[] = {a}, *pb[] = {b};
  add_noise_float(pa, r, q, 20, 33, 0, 0, 0, false, 1);
  add_noise_float_batch(pb, r, q, 20, 33, 0, 0, 0, false, 1, &expected);
  CHECK(!memcmp(a, b, sizeof(a)));
  for (unsigned i = 0; i < 13; i++)
    CHECK(gaussZiggurat(0, 1) == gaussian_rng_sample(&expected));
  randominit();
  gaussian_rng_init(42, &expected);
  CHECK(gaussZiggurat(0, 1) == gaussian_rng_sample(&expected));
}
static void boundaries(void)
{
  float input[] = {-INFINITY,
                   INFINITY,
                   NAN,
                   -0x1p31f,
                   0x1p31f,
                   -32769,
                   -32768,
                   -32767.75f,
                   -1.75f,
                   -0.f,
                   0.f,
                   1.75f,
                   32767.75f,
                   32768,
                   65535,
                   65536};
  const float *re[] = {input}, *im[] = {input};
  c16_t out[16];
  c16_t *rx[] = {out};
  gaussian_rng_t s;
  gaussian_rng_init(42, &s);
  add_noise_float_batch(rx, re, im, 0, 16, 0, 0, 0, false, 1, &s);
  for (unsigned i = 0; i < 16; i++) {
    CHECK(out[i].r == narrow(input[i]));
    CHECK(out[i].i == narrow(input[i]));
    const float *single[] = {input + i};
    add_noise_float_batch(rx, single, single, 0, 1, i, 0, 0, false, 1, &s);
    CHECK(out[i].r == narrow(input[i]));
    CHECK(out[i].i == narrow(input[i]));
  }
}
/* Compare arithmetic using identical candidates; different precision streams are
 * deliberately not paired. Exclude wrapped outputs from signed error statistics. */
static void arithmetic_accuracy(void)
{
  enum { BLOCK = 1024, BLOCKS = 256 };
  const float amplitudes[] = {.5f, 32, 2048, 8192};
  float signal[BLOCK], f[2 * BLOCK];
  for (unsigned i = 0; i < BLOCK; i++)
    signal[i] = (int)(i % 61) * 1000 - 30000.25f;
  const float *re[] = {signal}, *im[] = {signal};
  c16_t values[BLOCK];
  c16_t *rx[] = {values};
  for (unsigned a = 0; a < sizeof(amplitudes) / sizeof(*amplitudes); a++) {
    gaussian_rng_t s, reference;
    gaussian_rng_init(42, &s);
    reference = s;
    long error_sum = 0;
    unsigned differing = 0;
    for (unsigned block = 0; block < BLOCKS; block++) {
      gaussian_rng_fill_float(&reference, 2 * BLOCK, f);
      add_noise_float_batch(rx, re, im, 2 * amplitudes[a] * amplitudes[a], BLOCK, 0, 0, 0, false, 1, &s);
      for (unsigned i = 0; i < 2 * BLOCK; i++) {
        double expected = fma((double)f[i], amplitudes[a], signal[i / 2]);
        if (fabs(expected) >= 32767)
          continue;
        int actual = i & 1 ? values[i / 2].i : values[i / 2].r;
        int error = actual - (int)expected;
        CHECK(abs(error) <= 1);
        error_sum += error;
        differing += error != 0;
      }
    }
    CHECK(fabs((double)error_sum) <= 6 * sqrt(differing) + 2);
  }
}

static void quantized_distribution(void)
{
  enum { BLOCK = 1024, BLOCKS = 1024 };
  float zeros[BLOCK] = {0};
  const float *re[] = {zeros}, *im[] = {zeros};
  c16_t values[BLOCK];
  c16_t *rx[] = {values};
  gaussian_rng_t s;
  gaussian_rng_init(123456789, &s);
  unsigned histogram[9] = {0};
  for (unsigned block = 0; block < BLOCKS; block++) {
    add_noise_float_batch(rx, re, im, 8, BLOCK, 0, 0, 0, false, 1, &s);
    for (unsigned i = 0; i < BLOCK; i++) {
      if (values[i].r >= -4 && values[i].r <= 4)
        histogram[values[i].r + 4]++;
      if (values[i].i >= -4 && values[i].i <= 4)
        histogram[values[i].i + 4]++;
    }
  }
  const double n = 2.0 * BLOCK * BLOCKS;
  for (int k = -4; k <= 4; k++) {
    double low = k > 0 ? k : k - 1, high = k < 0 ? k : k + 1;
    double probability = .5 * (erf(high / (2 * sqrt(2))) - erf(low / (2 * sqrt(2))));
    CHECK(fabs(histogram[k + 4] - n * probability) < 6 * sqrt(n * probability * (1 - probability)) + 2);
  }
}

int main(void)
{
  callers(false);
  callers(true);
  compatibility();
  boundaries();
  arithmetic_accuracy();
  quantized_distribution();
  puts("PASS C16 ordering, bounds, phase, conversion and shared default stream");
}
