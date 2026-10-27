/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
#include "sim.h"
#include <math.h>
#include <pthread.h>
#include <string.h>
enum { ZIGGURAT_LAYERS = 256, SIGN_BIT = 256, SIGN_SHIFT = 55 };
_Static_assert(sizeof(gaussian_rng_t) <= 256, "Gaussian state size");
#define RADIUS 3.6541528853610088
#define AREA 0.004928673233974655
#if defined(GAUSSIAN_RNG_FORCE_SCALAR) || (!defined(SIMDE_X86_AVX2_NATIVE) && !defined(GAUSSIAN_RNG_TEST_SIMDE))
#define GAUSSIAN_RNG_SCALAR
#endif
static double wd[ZIGGURAT_LAYERS], density[ZIGGURAT_LAYERS];
static float wf[ZIGGURAT_LAYERS];
static uint64_t kd[ZIGGURAT_LAYERS];
static int32_t kf[ZIGGURAT_LAYERS];
static pthread_once_t once = PTHREAD_ONCE_INIT;
/* Adapted PRNG routines only:
 * step, jump and vectorized next: https://prng.di.unimi.it/xoshiro256plusplus.c
 * Written in 2019 by David Blackman and Sebastiano Vigna (vigna@acm.org).
 * splitmix: https://prng.di.unimi.it/splitmix64.c
 * Written in 2015 by Sebastiano Vigna (vigna@acm.org).
 * Both sources carry the following notice. */
/*
To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide.

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

static inline uint64_t rot(uint64_t v, unsigned n)
{
  return v << n | v >> (64 - n);
}
static inline uint64_t step(uint64_t *s)
{
  uint64_t result = rot(s[0] + s[3], 23) + s[0], t = s[1] << 17;
  s[2] ^= s[0];
  s[3] ^= s[1];
  s[1] ^= s[2];
  s[0] ^= s[3];
  s[2] ^= t;
  s[3] = rot(s[3], 45);
  return result;
}
static void jump(uint64_t *s)
{
  const uint64_t j[4] = {0x180ec6d33cfd0abaULL, 0xd5a61266f0c9392cULL, 0xa9582618e03fc9aaULL, 0x39abdc4529b1661cULL};
  uint64_t r[4] = {0};
  for (unsigned i = 0; i < 4; i++)
    for (unsigned b = 0; b < 64; b++) {
      if (j[i] & (UINT64_C(1) << b))
        for (unsigned w = 0; w < 4; w++)
          r[w] ^= s[w];
      step(s);
    }
  memcpy(s, r, sizeof(r));
}
static uint64_t splitmix(uint64_t *s)
{
  uint64_t z = (*s += 0x9e3779b97f4a7c15ULL);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}
static void init_tables(void)
{
  double d = RADIUS, prev = d, q = AREA / exp(-.5 * d * d);
  kd[0] = (uint64_t)(d / q * 0x1p52);
  kd[1] = 0;
  wd[0] = q * 0x1p-52;
  wd[ZIGGURAT_LAYERS - 1] = d * 0x1p-52;
  density[0] = 1;
  density[ZIGGURAT_LAYERS - 1] = exp(-.5 * d * d);
  for (int i = ZIGGURAT_LAYERS - 2; i >= 1; --i) {
    d = sqrt(-2 * log(AREA / d + exp(-.5 * d * d)));
    kd[i + 1] = (uint64_t)(d / prev * 0x1p52);
    prev = d;
    density[i] = exp(-.5 * d * d);
    wd[i] = d * 0x1p-52;
  }
  for (unsigned i = 0; i < ZIGGURAT_LAYERS; i++) {
    wf[i] = (float)(wd[i] * 0x1p29);
    kf[i] = (int32_t)(kd[i] >> 29);
  }
}
void gaussian_rng_init(uint64_t seed, gaussian_rng_t *s)
{
  pthread_once(&once, init_tables);
  memset(s, 0, sizeof(*s));
  uint64_t lane[4];
  for (unsigned i = 0; i < 4; i++)
    lane[i] = splitmix(&seed);
  for (unsigned l = 0; l < 4; l++) {
    for (unsigned w = 0; w < 4; w++)
      s->words[w][l] = lane[w];
    jump(lane);
  }
  memcpy(s->repair, lane, sizeof(lane));
  s->cursor_double = 4;
  s->cursor_float = 8;
}
static inline double uniform(gaussian_rng_t *s)
{
  return (step(s->repair) >> 11) * 0x1p-53;
}
static inline double value_d(uint64_t w)
{
  double v = (w >> 12) * wd[w & (ZIGGURAT_LAYERS - 1)];
  return w & SIGN_BIT ? -v : v;
}
static inline float value_f(uint32_t w)
{
  float v = (w >> 9) * wf[w & (ZIGGURAT_LAYERS - 1)];
  return w & 256 ? -v : v;
}
static double slow_d(gaussian_rng_t *s, uint64_t w)
{
  for (;;) {
    unsigned i = w & (ZIGGURAT_LAYERS - 1);
    double x = value_d(w);
    if (!i) {
      double a, b;
      do {
        a = -log1p(-uniform(s)) / RADIUS;
        b = -log1p(-uniform(s));
      } while (2 * b < a * a);
      return w & SIGN_BIT ? -RADIUS - a : RADIUS + a;
    }
    if (fma(uniform(s), density[i - 1] - density[i], density[i]) < exp(-.5 * x * x))
      return x;
    w = step(s->repair);
    if ((w >> 12) < kd[w & (ZIGGURAT_LAYERS - 1)])
      return value_d(w);
  }
}
static float slow_f(gaussian_rng_t *s, uint32_t w)
{
  for (;;) {
    unsigned i = w & (ZIGGURAT_LAYERS - 1);
    float x = value_f(w);
    if (!i) {
      double a, b;
      do {
        a = -log1p(-uniform(s)) / RADIUS;
        b = -log1p(-uniform(s));
      } while (2 * b < a * a);
      return (float)(w & 256 ? -RADIUS - a : RADIUS + a);
    }
    if (fma(uniform(s), density[i - 1] - density[i], density[i]) < exp(-.5 * (double)x * x))
      return x;
    w = (uint32_t)step(s->repair);
    if ((w >> 9) < (uint32_t)kf[w & (ZIGGURAT_LAYERS - 1)])
      return value_f(w);
  }
}
typedef struct {
  simde__m256i a, b, c, d;
} vec_state;
static inline vec_state load(const gaussian_rng_t *s)
{
  return (vec_state){simde_mm256_loadu_si256((const simde__m256i *)s->words[0]),
                     simde_mm256_loadu_si256((const simde__m256i *)s->words[1]),
                     simde_mm256_loadu_si256((const simde__m256i *)s->words[2]),
                     simde_mm256_loadu_si256((const simde__m256i *)s->words[3])};
}
static inline void save(gaussian_rng_t *s, vec_state v)
{
  simde_mm256_storeu_si256((simde__m256i *)s->words[0], v.a);
  simde_mm256_storeu_si256((simde__m256i *)s->words[1], v.b);
  simde_mm256_storeu_si256((simde__m256i *)s->words[2], v.c);
  simde_mm256_storeu_si256((simde__m256i *)s->words[3], v.d);
}
static inline simde__m256i next(vec_state *v)
{
  simde__m256i sum = simde_mm256_add_epi64(v->a, v->d);
  simde__m256i r =
      simde_mm256_add_epi64(simde_mm256_or_si256(simde_mm256_slli_epi64(sum, 23), simde_mm256_srli_epi64(sum, 41)), v->a);
  simde__m256i t = simde_mm256_slli_epi64(v->b, 17);
  v->c = simde_mm256_xor_si256(v->c, v->a);
  v->d = simde_mm256_xor_si256(v->d, v->b);
  v->b = simde_mm256_xor_si256(v->b, v->c);
  v->a = simde_mm256_xor_si256(v->a, v->d);
  v->c = simde_mm256_xor_si256(v->c, t);
  v->d = simde_mm256_or_si256(simde_mm256_slli_epi64(v->d, 45), simde_mm256_srli_epi64(v->d, 19));
  return r;
}
static inline simde__m256d candidate_d(simde__m256i w, unsigned *mask)
{
  simde__m256i idx = simde_mm256_and_si256(w, simde_mm256_set1_epi64x(ZIGGURAT_LAYERS - 1));
  simde__m256i mag = simde_mm256_srli_epi64(w, 12);
  simde__m256d widths = simde_mm256_i64gather_pd(wd, idx, 8);
  simde__m256i limits = simde_mm256_i64gather_epi64((const int64_t *)kd, idx, 8);
  simde__m256d x =
      simde_mm256_sub_pd(simde_mm256_castsi256_pd(simde_mm256_or_si256(mag, simde_mm256_set1_epi64x(0x4330000000000000LL))),
                         simde_mm256_set1_pd(0x1p52));
  *mask = simde_mm256_movemask_pd(simde_mm256_castsi256_pd(simde_mm256_cmpgt_epi64(limits, mag)));
  simde__m256i sign = simde_mm256_slli_epi64(simde_mm256_and_si256(w, simde_mm256_set1_epi64x(SIGN_BIT)), SIGN_SHIFT);
  return simde_mm256_xor_pd(simde_mm256_mul_pd(x, widths), simde_mm256_castsi256_pd(sign));
}
/* Each 64-bit output supplies two disjoint words: 8 index bits, one sign bit,
 * and 23 magnitude bits per float candidate. */
static inline simde__m256 candidate_f(simde__m256i w, unsigned *mask)
{
  simde__m256i idx = simde_mm256_and_si256(w, simde_mm256_set1_epi32(ZIGGURAT_LAYERS - 1));
  simde__m256i mag = simde_mm256_srli_epi32(w, 9);
  simde__m256 widths = simde_mm256_i32gather_ps((const float *)wf, idx, 4);
  simde__m256i limits = simde_mm256_i32gather_epi32((const int32_t *)kf, idx, 4);
  *mask = simde_mm256_movemask_ps(simde_mm256_castsi256_ps(simde_mm256_cmpgt_epi32(limits, mag)));
  simde__m256i sign = simde_mm256_slli_epi32(simde_mm256_and_si256(w, simde_mm256_set1_epi32(256)), 23);
  return simde_mm256_xor_ps(simde_mm256_mul_ps(simde_mm256_cvtepi32_ps(mag), widths), simde_mm256_castsi256_ps(sign));
}
#ifdef GAUSSIAN_RNG_SCALAR
static inline uint64_t scalar_word(gaussian_rng_t *s, unsigned lane)
{
  uint64_t w[4];
  for (unsigned j = 0; j < 4; j++)
    w[j] = s->words[j][lane];
  uint64_t out = step(w);
  for (unsigned j = 0; j < 4; j++)
    s->words[j][lane] = w[j];
  return out;
}
#endif
/* Main lanes never consume repair words. Deferred repairs therefore produce the
 * same output regardless of tile size or request partitioning. */
static void generate_d(gaussian_rng_t *s, unsigned n, double *out)
{
#ifdef GAUSSIAN_RNG_SCALAR
  for (unsigned k = 0; k < n; k += 4)
    for (unsigned l = 0; l < 4; l++) {
      uint64_t w = scalar_word(s, l);
      out[k + l] = (w >> 12) < kd[w & (ZIGGURAT_LAYERS - 1)] ? value_d(w) : slow_d(s, w);
    }
  return;
#endif
  uint64_t rejected[64];
  unsigned positions[64], nr = 0;
  vec_state v = load(s);
  for (unsigned k = 0; k < n; k += 4) {
    simde__m256i w = next(&v);
    unsigned mask;
    simde_mm256_storeu_pd(out + k, candidate_d(w, &mask));
    if (mask != 15) {
      uint64_t words[4];
      simde_mm256_storeu_si256((simde__m256i *)words, w);
      for (unsigned j = 0; j < 4; j++)
        if (!(mask & (1u << j))) {
          rejected[nr] = words[j];
          positions[nr++] = k + j;
        }
    }
  }
  save(s, v);
  for (unsigned j = 0; j < nr; j++)
    out[positions[j]] = slow_d(s, rejected[j]);
}
/* Scalar draws refill one vector, without the full-tile rejection queue. */
static inline void refill_double(gaussian_rng_t *s)
{
#ifdef GAUSSIAN_RNG_SCALAR
  generate_d(s, 4, s->pending_double);
#else
  vec_state v = load(s);
  simde__m256i word = next(&v);
  unsigned mask;
  simde_mm256_storeu_pd(s->pending_double, candidate_d(word, &mask));
  save(s, v);
  if (mask != 15) {
    uint64_t words[4];
    simde_mm256_storeu_si256((simde__m256i *)words, word);
    for (unsigned lane = 0; lane < 4; lane++)
      if (!(mask & (1u << lane)))
        s->pending_double[lane] = slow_d(s, words[lane]);
  }
#endif
}
static void generate_f(gaussian_rng_t *s, unsigned n, float *out)
{
#ifdef GAUSSIAN_RNG_SCALAR
  for (unsigned k = 0; k < n; k += 8)
    for (unsigned l = 0; l < 4; l++) {
      uint64_t raw = scalar_word(s, l);
      for (unsigned h = 0; h < 2; h++) {
        uint32_t w = raw >> (32 * h);
        out[k + 2 * l + h] = (w >> 9) < (uint32_t)kf[w & (ZIGGURAT_LAYERS - 1)] ? value_f(w) : slow_f(s, w);
      }
    }
  return;
#endif
  uint32_t rejected[64];
  unsigned positions[64], nr = 0;
  vec_state v = load(s);
  for (unsigned k = 0; k < n; k += 8) {
    simde__m256i w = next(&v);
    unsigned mask;
    simde_mm256_storeu_ps(out + k, candidate_f(w, &mask));
    if (mask != 255) {
      uint32_t words[8];
      simde_mm256_storeu_si256((simde__m256i *)words, w);
      for (unsigned j = 0; j < 8; j++)
        if (!(mask & (1u << j))) {
          rejected[nr] = words[j];
          positions[nr++] = k + j;
        }
    }
  }
  save(s, v);
  for (unsigned j = 0; j < nr; j++)
    out[positions[j]] = slow_f(s, rejected[j]);
}
void gaussian_rng_fill(gaussian_rng_t *s, size_t n, double *out)
{
  while (n && s->cursor_double < 4) {
    *out++ = s->pending_double[s->cursor_double++];
    --n;
  }
  while (n >= 4) {
    unsigned count = n >= 64 ? 64 : n / 4 * 4;
    generate_d(s, count, out);
    out += count;
    n -= count;
  }
  if (n) {
    refill_double(s);
    memcpy(out, s->pending_double, n * sizeof(*out));
    s->cursor_double = n;
  }
}
void gaussian_rng_fill_float(gaussian_rng_t *s, size_t n, float *out)
{
  while (n && s->cursor_float < 8) {
    *out++ = s->pending_float[s->cursor_float++];
    --n;
  }
  while (n >= 8) {
    unsigned count = n >= 64 ? 64 : n / 8 * 8;
    generate_f(s, count, out);
    out += count;
    n -= count;
  }
  if (n) {
    generate_f(s, 8, s->pending_float);
    memcpy(out, s->pending_float, n * sizeof(*out));
    s->cursor_float = n;
  }
}
double gaussian_rng_sample(gaussian_rng_t *s)
{
  if (s->cursor_double == 4) {
    refill_double(s);
    s->cursor_double = 0;
  }
  return s->pending_double[s->cursor_double++];
}
static inline int16_t narrow(double x)
{
  int32_t v = (x >= -0x1p31 && x < 0x1p31) ? (int32_t)x : 0;
  unsigned lo = (uint32_t)v & 65535;
  return (int16_t)(lo < 32768 ? (int)lo : (int)lo - 65536);
}
#ifndef GAUSSIAN_RNG_SCALAR
static inline simde__m128i packed(simde__m256 x)
{
  simde__m256i ints = simde_mm256_cvttps_epi32(x);
  simde__m256i bytes = simde_mm256_shuffle_epi8(ints,
                                                simde_mm256_setr_epi8(0,
                                                                      1,
                                                                      4,
                                                                      5,
                                                                      8,
                                                                      9,
                                                                      12,
                                                                      13,
                                                                      -1,
                                                                      -1,
                                                                      -1,
                                                                      -1,
                                                                      -1,
                                                                      -1,
                                                                      -1,
                                                                      -1,
                                                                      0,
                                                                      1,
                                                                      4,
                                                                      5,
                                                                      8,
                                                                      9,
                                                                      12,
                                                                      13,
                                                                      -1,
                                                                      -1,
                                                                      -1,
                                                                      -1,
                                                                      -1,
                                                                      -1,
                                                                      -1,
                                                                      -1));
  return simde_mm_unpacklo_epi64(simde_mm256_castsi256_si128(bytes), simde_mm256_extracti128_si256(bytes, 1));
}
static inline __attribute__((always_inline)) simde__m256 load_signal(const float **re, const float **im, unsigned i, unsigned ant)
{
  if (ant == 1) {
    simde__m128 r = simde_mm_loadu_ps(re[0] + i), q = simde_mm_loadu_ps(im[0] + i);
    return simde_mm256_insertf128_ps(simde_mm256_castps128_ps256(simde_mm_unpacklo_ps(r, q)), simde_mm_unpackhi_ps(r, q), 1);
  }
  if (ant == 2)
    return simde_mm256_setr_ps(re[0][i], im[0][i], re[1][i], im[1][i], re[0][i + 1], im[0][i + 1], re[1][i + 1], im[1][i + 1]);
  return simde_mm256_setr_ps(re[0][i], im[0][i], re[1][i], im[1][i], re[2][i], im[2][i], re[3][i], im[3][i]);
}
static inline __attribute__((always_inline)) void store_c16(c16_t **out, unsigned i, unsigned ant, simde__m128i data)
{
  if (ant == 1)
    simde_mm_storeu_si128((simde__m128i *)(out[0] + i), data);
  else if (ant == 2) {
    data = simde_mm_shuffle_epi32(data, SIMDE_MM_SHUFFLE(3, 1, 2, 0));
    simde_mm_storel_epi64((simde__m128i *)(out[0] + i), data);
    simde_mm_storel_epi64((simde__m128i *)(out[1] + i), simde_mm_srli_si128(data, 8));
  } else {
    uint32_t tmp[4];
    simde_mm_storeu_si128((simde__m128i *)tmp, data);
    for (unsigned a = 0; a < 4; a++)
      memcpy(out[a] + i, tmp + a, 4);
  }
}
static inline __attribute__((always_inline)) void
fused(gaussian_rng_t *s, c16_t **out, const float **re, const float **im, float amp, unsigned n, unsigned offset, unsigned ant)
{
  const size_t total = (size_t)2 * n * ant;
  simde__m256 amplitude = simde_mm256_set1_ps(amp);
  for (size_t start = 0; start < total; start += 64) {
    unsigned count = total - start < 64 ? total - start : 64, nr = 0;
    size_t positions[64];
    uint32_t rejected[64];
    vec_state v = load(s);
    for (unsigned k = 0; k < count; k += 8) {
      unsigned i = (start + k) / (2 * ant);
      simde__m256i word = next(&v);
      unsigned mask;
      simde__m256 x = candidate_f(word, &mask);
      simde__m256 y = simde_mm256_fmadd_ps(x, amplitude, load_signal(re, im, i, ant));
      store_c16(out, offset + i, ant, packed(y));
      if (mask != 255) {
        uint32_t words[8];
        simde_mm256_storeu_si256((simde__m256i *)words, word);
        for (unsigned j = 0; j < 8; j++)
          if (!(mask & (1u << j))) {
            rejected[nr] = words[j];
            positions[nr++] = start + k + j;
          }
      }
    }
    save(s, v);
    for (unsigned k = 0; k < nr; k++) {
      size_t pos = positions[k];
      unsigned a = (pos / 2) % ant, i = pos / (2 * ant);
      float x = slow_f(s, rejected[k]);
      if (pos & 1)
        out[a][offset + i].i = narrow(fmaf(x, amp, im[a][i]));
      else
        out[a][offset + i].r = narrow(fmaf(x, amp, re[a][i]));
    }
  }
}
#endif
void add_noise_float_batch(c16_t **out,
                           const float **re,
                           const float **im,
                           float sigma,
                           int n,
                           int offset,
                           double ts,
                           int delay,
                           bool phase,
                           uint8_t ant,
                           gaussian_rng_t *s)
{
  if (n <= 0 || !ant)
    return;
  float amp = sqrtf(sigma / 2);
  offset += delay;
  if (n == 1 && ant == 1) {
    float x, y;
    if (s->cursor_float <= 6) {
      x = s->pending_float[s->cursor_float++];
      y = s->pending_float[s->cursor_float++];
    } else {
      float pair[2];
      gaussian_rng_fill_float(s, 2, pair);
      x = pair[0];
      y = pair[1];
    }
    simde__m128 z =
        simde_mm_fmadd_ps(simde_mm_setr_ps(x, y, 0, 0), simde_mm_set1_ps(amp), simde_mm_setr_ps(re[0][0], im[0][0], 0, 0));
    simde__m128i q = simde_mm_shuffle_epi8(simde_mm_cvttps_epi32(z),
                                           simde_mm_setr_epi8(0, 1, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1));
    uint32_t bits = (uint32_t)simde_mm_cvtsi128_si32(q);
    memcpy(out[0] + offset, &bits, 4);
    if (phase)
      phase_noise(ts, &out[0][offset].r, &out[0][offset].i);
    return;
  }
#ifndef GAUSSIAN_RNG_SCALAR
  if (s->cursor_float == 8 && (2 * (unsigned)n * ant) % 8 == 0 && (ant == 1 || ant == 2 || ant == 4)) {
    if (ant == 1)
      fused(s, out, re, im, amp, n, offset, 1);
    else if (ant == 2)
      fused(s, out, re, im, amp, n, offset, 2);
    else
      fused(s, out, re, im, amp, n, offset, 4);
  } else
#endif
  {
    size_t total = (size_t)n * ant * 2;
    for (size_t start = 0; start < total; start += 64) {
      size_t count = total - start < 64 ? total - start : 64;
      float f[64];
      gaussian_rng_fill_float(s, count, f);
      for (size_t j = 0; j < count; j++) {
        size_t pos = start + j;
        unsigned a = (pos / 2) % ant, i = pos / (2 * ant);
        double x = fmaf(f[j], amp, (pos & 1) ? im[a][i] : re[a][i]);
        if (pos & 1)
          out[a][offset + i].i = narrow(x);
        else
          out[a][offset + i].r = narrow(x);
      }
    }
  }
  if (phase)
    for (int i = 0; i < n; i++)
      for (unsigned a = 0; a < ant; a++)
        phase_noise(ts, &out[a][offset + i].r, &out[a][offset + i].i);
}
