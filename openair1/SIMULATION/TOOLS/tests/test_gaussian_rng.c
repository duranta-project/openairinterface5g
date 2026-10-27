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
static uint64_t rot(uint64_t v, unsigned n)
{
  return v << n | v >> (64 - n);
}
static void streams(void)
{
  const uint64_t seeds[] = {0, 1, 42, 123456789, UINT64_MAX};
  for (unsigned seed = 0; seed < 5; seed++) {
    gaussian_rng_t a, b;
    gaussian_rng_init(seeds[seed], &a);
    b = a;
    enum { N = 10003 };
    double x[N], y[N];
    float f[N], g[N];
    gaussian_rng_fill(&a, N, x);
    for (unsigned i = 0; i < N;) {
      unsigned n = 1 + i % 71;
      if (n > N - i)
        n = N - i;
      gaussian_rng_fill(&b, n, y + i);
      i += n;
    }
    CHECK(!memcmp(x, y, sizeof(x)));
    gaussian_rng_t reference;
    scalar_gaussian_rng_init(seeds[seed], &reference);
    scalar_gaussian_rng_fill(&reference, N, y);
    CHECK(!memcmp(x, y, sizeof(x)));
    gaussian_rng_init(seeds[seed], &reference);
    for (unsigned i = 0; i < N; i++)
      CHECK(x[i] == gaussian_rng_sample(&reference));
    CHECK(!memcmp(a.words, b.words, sizeof(a.words)));
    CHECK(!memcmp(a.repair, b.repair, sizeof(a.repair)));
    gaussian_rng_init(seeds[seed], &a);
    b = a;
    gaussian_rng_fill_float(&a, N, f);
    for (unsigned i = 0; i < N;) {
      unsigned n = 1 + i % 71;
      if (n > N - i)
        n = N - i;
      gaussian_rng_fill_float(&b, n, g + i);
      i += n;
    }
    CHECK(!memcmp(f, g, sizeof(f)));
    scalar_gaussian_rng_init(seeds[seed], &reference);
    scalar_gaussian_rng_fill_float(&reference, N, g);
    CHECK(!memcmp(f, g, sizeof(f)));
    CHECK(!memcmp(a.words, b.words, sizeof(a.words)));
    CHECK(!memcmp(a.repair, b.repair, sizeof(a.repair)));
    gaussian_rng_t copy = a;
    gaussian_rng_fill_float(&a, 0, NULL);
    gaussian_rng_fill(&a, 0, NULL);
    CHECK(!memcmp(&a, &copy, sizeof(a)));
    gaussian_rng_fill(&a, 13, x);
    gaussian_rng_fill(&copy, 13, y);
    CHECK(!memcmp(x, y, 13 * sizeof(*x)));
    gaussian_rng_fill_float(&a, 19, f);
    gaussian_rng_fill_float(&copy, 19, g);
    CHECK(!memcmp(f, g, 19 * sizeof(*f)));
    for (unsigned mask = 0; mask < 256; mask++) {
      gaussian_rng_init(seeds[seed], &a);
      for (unsigned l = 0; l < 4; l++) {
        uint32_t lo = mask & (1u << (2 * l)) ? 0xfffffe00 : 2;
        uint32_t hi = mask & (1u << (2 * l + 1)) ? 0xffffff00 : 2;
        uint64_t w = (uint64_t)hi << 32 | lo;
        a.words[0][l] = 0;
        a.words[3][l] = rot(w, 41);
      }
      b = a;
      gaussian_rng_fill_float(&a, 64, f);
      for (unsigned i = 0; i < 64; i++)
        gaussian_rng_fill_float(&b, 1, g + i);
      CHECK(!memcmp(f, g, 64 * sizeof(*f)));
      for (unsigned i = 0; i < 64; i++)
        CHECK(isfinite(f[i]));
    }
  }
}
static void double_masks_and_endpoints(void)
{
  for (unsigned mask = 0; mask < 16; mask++)
    for (unsigned endpoint = 0; endpoint < 2; endpoint++) {
      gaussian_rng_t a, b;
      gaussian_rng_init(42, &a);
      for (unsigned l = 0; l < 4; l++) {
        uint64_t w = mask & (1u << l) ? UINT64_C(0xfffffffffffff000) | (l & 1 ? 256 : 0) : 2;
        a.words[0][l] = 0;
        a.words[3][l] = rot(w, 41);
      }
      a.repair[0] = 0;
      a.repair[3] = rot(endpoint ? UINT64_MAX : 0, 41);
      b = a;
      double x[64], y[64];
      gaussian_rng_fill(&a, 64, x);
      scalar_gaussian_rng_fill(&b, 64, y);
      CHECK(!memcmp(x, y, sizeof(x)));
      for (unsigned i = 0; i < 64; i++)
        CHECK(isfinite(x[i]));
      if (mask == 15) {
        gaussian_rng_init(42, &a);
        for (unsigned l = 0; l < 4; l++) {
          a.words[0][l] = 0;
          a.words[3][l] = rot(UINT64_C(0xffffff00fffffe00), 41);
        }
        a.repair[0] = 0;
        a.repair[3] = rot(endpoint ? UINT64_MAX : 0, 41);
        b = a;
        float f[64], g[64];
        gaussian_rng_fill_float(&a, 64, f);
        scalar_gaussian_rng_fill_float(&b, 64, g);
        CHECK(!memcmp(f, g, sizeof(f)));
        for (unsigned i = 0; i < 64; i++)
          CHECK(isfinite(f[i]));
      }
    }
  gaussian_rng_t a, b, c;
  gaussian_rng_init(1, &a);
  gaussian_rng_init(UINT64_C(0x100000001), &b);
  CHECK(memcmp(a.words, b.words, sizeof(a.words)) != 0);
  c = a;
  for (unsigned i = 0; i < 1000; i++) {
    CHECK(gaussian_rng_sample(&a) == gaussian_rng_sample(&c));
    gaussian_rng_sample(&b);
    if (i % 17 == 0)
      gaussian_rng_init(i, &b);
  }
}

static void distribution(void)
{
  enum { N = 2000000 };
  const double edges[] = {-4, -3, -2, -1, 0, 1, 2, 3, 4};
  for (unsigned scalar = 0; scalar < 2; scalar++)
    for (unsigned precision = 0; precision < 2; precision++)
      for (unsigned seed = 0; seed < 3; seed++) {
        gaussian_rng_t s;
        gaussian_rng_init(seed == 0 ? 0 : seed == 1 ? 42 : 123456789, &s);
        double sum[8] = {0}, sq[8] = {0}, cross[8] = {0}, adjacent[8] = {0}, last[8] = {0}, previous = 0;
        unsigned bins[8][9] = {{0}}, sign[8] = {0};
        for (unsigned start = 0; start < N; start += 64) {
          double d[64];
          float f[64];
          if (precision) {
            if (scalar)
              scalar_gaussian_rng_fill_float(&s, 64, f);
            else
              gaussian_rng_fill_float(&s, 64, f);
          } else {
            if (scalar)
              scalar_gaussian_rng_fill(&s, 64, d);
            else
              gaussian_rng_fill(&s, 64, d);
          }
          for (unsigned j = 0; j < 64; j++) {
            unsigned l = (start + j) % 8;
            double x = precision ? f[j] : d[j];
            CHECK(isfinite(x));
            sum[l] += x;
            sq[l] += x * x;
            cross[l] += x * last[l];
            adjacent[l] += x * previous;
            previous = x;
            last[l] = x;
            sign[l] += x > 0;
            for (unsigned e = 0; e < 9; e++)
              bins[l][e] += x <= edges[e];
          }
        }
        const double n = N / 8;
        for (unsigned l = 0; l < 8; l++) {
          CHECK(fabs(sum[l] / n) < 6 / sqrt(n));
          CHECK(fabs(sq[l] / n - 1) < 6 * sqrt(2 / n));
          CHECK(fabs(cross[l] / n) < 6 / sqrt(n));
          CHECK(fabs(adjacent[l] / n) < 6 / sqrt(n));
          CHECK(fabs(sign[l] - n / 2) < 6 * sqrt(n / 4));
          for (unsigned e = 0; e < 9; e++) {
            double p = .5 * erfc(-edges[e] / sqrt(2));
            CHECK(fabs(bins[l][e] - n * p) < 6 * sqrt(n * p * (1 - p)) + 2);
          }
        }
      }
}
int main(void)
{
  streams();
  double_masks_and_endpoints();
  distribution();
  puts("PASS Gaussian streams, scalar equivalence, rejection masks and lane distributions");
}
