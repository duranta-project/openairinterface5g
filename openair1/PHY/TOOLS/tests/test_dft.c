/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <complex.h>
#include <immintrin.h>

#include "openair1/PHY/TOOLS/tools_defs.h"
#include "openair1/SIMULATION/TOOLS/sim.h"
#include "common/utils/utils.h"

#ifdef USE_FFTW_BACKEND
#include <fftw3.h>
#endif

#ifdef USE_FFTZ_BACKEND
#include <aoclfftz.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define ONE_OVER_SQRT2_Q15 23170
#define N_DFT 32
#ifndef Q15_INV_SQRT2
#define Q15_INV_SQRT2 ((int16_t)23170) /* round(0.70710678118 * 32768) */
#endif
#define Q15_HALF_SQRT3 ((int16_t)28378)
#define Q15_SCALE_1_OVER_SQRT64 ((int16_t)4096)

#define Q15_HALF 16384
static inline __m128i scale_half_q15_128(__m128i x)
{
  return _mm_srai_epi16(x, 1); // divide by 2
}
static inline __m256i scale_q15_1_over_sqrt64_256(__m256i x)
{
  const __m256i s = _mm256_set1_epi16(Q15_SCALE_1_OVER_SQRT64);
  return _mm256_mulhrs_epi16(x, s);
}

static volatile float bench_sink;

typedef enum { DFT_DIR_FORWARD = -1, DFT_DIR_INVERSE = 1 } dft_dir_t;

typedef struct {
  int N;
  int blocks;
  simde__m256i *W1_RE_NEGIM;
  simde__m256i *W1_IM_RE;
  simde__m256i *W3_RE_NEGIM;
  simde__m256i *W3_IM_RE;
} sr_twiddle_simd_t;

typedef struct {
  int N;
  int blocks;

  __m256 *W1_RE_RE;
  __m256 *W1_IM_IM;

  __m256 *W3_RE_RE;
  __m256 *W3_IM_IM;
} sr_twiddle_f32_prepack_t;

#define SR_MAX_LOG2 17
#define SR_STOP_N 256

static inline int log2_int(unsigned int N)
{
  return __builtin_ctz(N);
}
static sr_twiddle_f32_prepack_t sr_twiddles_f32[SR_MAX_LOG2 + 1];
static sr_twiddle_simd_t sr_twiddles_fwd[SR_MAX_LOG2 + 1];
static sr_twiddle_simd_t sr_twiddles_bwd[SR_MAX_LOG2 + 1];

#define MAX_N 98304
#define MAX_SIMD_BLOCKS ((MAX_N + 3) / 4)

static volatile float bench_sink;

static void consume_output(const float complex *dst, int N)
{
  float s = 0.0f;
  for (int i = 0; i < N; i++) {
    s += crealf(dst[i]) + cimagf(dst[i]);
  }
  bench_sink += s;
}

static volatile int64_t bench_sink_c16;

__attribute__((noinline)) static void consume_output_c16(const c16_t *out, int N)
{
  int64_t s = 0;

  for (int i = 0; i < N; i++) {
    s += out[i].r;
    s += out[i].i;
  }

  bench_sink_c16 += s;
}

static inline int16_t sat16_i32(int32_t x)
{
  if (x > 32767)
    return 32767;
  if (x < -32767)
    return -32767;
  return (int16_t)x;
}

static inline int16_t q15_from_float(float x)
{
  return sat16_i32((int32_t)lrintf(32767.0f * x));
}

static inline __m128i swap_complex_pairs_i16_128(__m128i a)
{
  const __m128i shuf = _mm_set_epi8(13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2);

  return _mm_shuffle_epi8(a, shuf);
}

static inline __m128i complex_mul4_prepack_q15_128(__m128i a, __m128i w_re_re, __m128i w_im_signed)
{
  const __m128i a_swapped = swap_complex_pairs_i16_128(a);

  const __m128i prod_re = _mm_mulhrs_epi16(a, w_re_re);
  const __m128i prod_im = _mm_mulhrs_epi16(a_swapped, w_im_signed);

  return _mm_adds_epi16(prod_re, prod_im);
}

static inline __m128i mullts_q15_128(__m128i z)
{
  /*
   * j * (r + ji) = -i + jr
   *
   * [r i] -> [-i r]
   */
  const __m128i swapped = swap_complex_pairs_i16_128(z);

  const __m128i sign = _mm_setr_epi16(-1, 1, -1, 1, -1, 1, -1, 1);

  return _mm_sign_epi16(swapped, sign);
}

static inline __m128i mul_minuslts_q15_128(__m128i z)
{
  /*
   * -j * (r + ji) = i - jr
   *
   * [r i] -> [i -r]
   */
  const __m128i swapped = swap_complex_pairs_i16_128(z);

  const __m128i sign = _mm_setr_epi16(1, -1, 1, -1, 1, -1, 1, -1);

  return _mm_sign_epi16(swapped, sign);
}

static inline __m128i q15_mul_i16_128(__m128i x, int16_t q15)
{
  return _mm_mulhrs_epi16(x, _mm_set1_epi16(q15));
}

#define Q15_INV_SQRT3 18919 /* 1 / sqrt(3) */
#define Q15_INV_2SQRT3 9459 /* 1 / (2 * sqrt(3)) */
#define Q15_HALF 16384 /* 1 / 2 */

static inline __m128i add3s_epi16(__m128i a, __m128i b, __m128i c)
{
  return _mm_adds_epi16(_mm_adds_epi16(a, b), c);
}

static inline void radix3_combine4_q15_128_fast(__m128i A,
                                                __m128i X1,
                                                __m128i X2,
                                                __m128i w1_re_re,
                                                __m128i w1_im_im,
                                                __m128i w2_re_re,
                                                __m128i w2_im_im,
                                                __m128i *Y0,
                                                __m128i *Y1,
                                                __m128i *Y2,
                                                dft_dir_t dir)
{
  /*
   * B = W1 * X1
   * C = W2 * X2
   */
  const __m128i Bs = complex_mul4_prepack_q15_128(X1, w1_re_re, w1_im_im);
  const __m128i Cs = complex_mul4_prepack_q15_128(X2, w2_re_re, w2_im_im);

  /*
   * Correct scaling for N = 3 * size when sub-FFTs are already scaled:
   *
   * final scale = 1 / sqrt(3)
   *
   * Y0 = (A + B + C) / sqrt(3)
   */
  const __m128i As = q15_mul_i16_128(A, Q15_INV_SQRT3);
  // const __m128i Bs = q15_mul_i16_128(B, Q15_INV_SQRT3);
  // const __m128i Cs = q15_mul_i16_128(C, Q15_INV_SQRT3);

  *Y0 = add3s_epi16(As, Bs, Cs);

  /*
   * base = A/sqrt(3) - B/(2*sqrt(3)) - C/(2*sqrt(3))
   */
  const __m128i Bh = _mm_srai_epi16(Bs, 1);
  const __m128i Ch = _mm_srai_epi16(Cs, 1);

  const __m128i base = _mm_subs_epi16(_mm_subs_epi16(As, Bh), Ch);

  /*
   * Z = c3 * (B - C) / sqrt(3)
   *   = 0.5 * (B - C)
   */

  const __m128i Z = q15_mul_i16_128(_mm_subs_epi16(Bs, Cs), Q15_HALF_SQRT3);

  /*
   * Y1 = base - j*Z
   * Y2 = base + j*Z
   */
  *Y1 = _mm_adds_epi16(base, (dir == DFT_DIR_FORWARD) ? mul_minuslts_q15_128(Z) : mullts_q15_128(Z));
  *Y2 = _mm_adds_epi16(base, (dir == DFT_DIR_FORWARD) ? mullts_q15_128(Z) : mul_minuslts_q15_128(Z));
}

static inline void *aligned_malloc32(size_t size)
{
  void *ptr = NULL;

  if (posix_memalign(&ptr, 32, size) != 0) {
    return NULL;
  }

  return ptr;
}

typedef struct {
  int N;
  int initialized;

  float complex *forward;
  float complex *inverse;

  int r2_blocks;
  __m256 *r2_w_re;
  __m256 *r2_w_im;

  int r3_blocks;
  __m256 *r3_w1_re;
  __m256 *r3_w1_im;
  __m256 *r3_w2_re;
  __m256 *r3_w2_im;

  int r3_q15_blocks;
  __m128i *r3_q15_w1_re;
  __m128i *r3_q15_w1_im;
  __m128i *r3_q15_w2_re;
  __m128i *r3_q15_w2_im;
  __m128i *r3_q15_w1_re_inv;
  __m128i *r3_q15_w1_im_inv;
  __m128i *r3_q15_w2_re_inv;
  __m128i *r3_q15_w2_im_inv;

  int r4_blocks;
  __m256 *r4_w1_re;
  __m256 *r4_w1_im;
  __m256 *r4_w2_re;
  __m256 *r4_w2_im;
  __m256 *r4_w3_re;
  __m256 *r4_w3_im;

  int r5_blocks;
  __m256 *r5_w1_re;
  __m256 *r5_w1_im;
  __m256 *r5_w2_re;
  __m256 *r5_w2_im;
  __m256 *r5_w3_re;
  __m256 *r5_w3_im;
  __m256 *r5_w4_re;
  __m256 *r5_w4_im;

} TwiddleTable;

static void *aligned_malloc64(size_t size)
{
  void *ptr = NULL;

  if (posix_memalign(&ptr, 64, size) != 0) {
    return NULL;
  }

  return ptr;
}

static inline int is_power_of_two_int(int x)
{
  return x > 0 && ((x & (x - 1)) == 0);
}
static inline __m256 pack4_twiddle_re(const float complex *W, int k0, int mul, int N)
{
  float tmp[8] __attribute__((aligned(32)));

  for (int lane = 0; lane < 4; lane++) {
    int k = k0 + lane;
    int idx = (mul * k) % N;

    float wr = crealf(W[idx]);

    tmp[2 * lane + 0] = wr;
    tmp[2 * lane + 1] = wr;
  }

  return _mm256_load_ps(tmp);
}

static inline __m128i pack4_twiddle_q15_re_re(const float complex *W, int k0, int mul, int N)
{
  int16_t v[8];

  for (int j = 0; j < 4; j++) {
    const int k = (mul * (k0 + j)) % N;
    const int16_t wr = q15_from_float(crealf(W[k])) / sqrtf(3);

    v[2 * j + 0] = wr;
    v[2 * j + 1] = wr;
  }

  return _mm_loadu_si128((const __m128i *)v);
}

static inline __m128i pack4_twiddle_q15_im_im(const float complex *W, int k0, int mul, int N)
{
  int16_t v[8];

  for (int j = 0; j < 4; j++) {
    const int k = (mul * (k0 + j)) % N;
    const int16_t wi = q15_from_float(cimagf(W[k])) / sqrtf(3);

    v[2 * j + 0] = -wi;
    v[2 * j + 1] = wi;
  }

  return _mm_loadu_si128((const __m128i *)v);
}

static inline __m256 pack4_twiddle_im(const float complex *W, int k0, int mul, int N)
{
  float tmp[8] __attribute__((aligned(32)));

  for (int lane = 0; lane < 4; lane++) {
    int k = k0 + lane;
    int idx = (mul * k) % N;

    float wi = cimagf(W[idx]);

    tmp[2 * lane + 0] = wi;
    tmp[2 * lane + 1] = wi;
  }

  return _mm256_load_ps(tmp);
}

static int twiddle_table_create_radix2_simd(TwiddleTable *table)
{
  const int N = table->N;
  table->r2_blocks = 0;
  table->r2_w_re = NULL;
  table->r2_w_im = NULL;

  if (N <= 0) {
    return 0;
  }

  if (N % 2 != 0) {
    return 1;
  }

  const int size = N / 2;
  const int blocks = (size + 3) / 4;

  table->r2_blocks = blocks;

  table->r2_w_re = aligned_malloc64(blocks * sizeof(__m256));
  table->r2_w_im = aligned_malloc64(blocks * sizeof(__m256));

  if (!table->r2_w_re || !table->r2_w_im) {
    return 0;
  }

  for (int b = 0; b < blocks; b++) {
    const int k0 = 4 * b;

    table->r2_w_re[b] = pack4_twiddle_re(table->forward, k0, 1, N);
    table->r2_w_im[b] = pack4_twiddle_im(table->forward, k0, 1, N);
  }

  return 1;
}

static int twiddle_table_create_radix4_simd(TwiddleTable *table)
{
  const int N = table->N;
  table->r4_blocks = 0;
  table->r4_w1_re = NULL;
  table->r4_w1_im = NULL;
  table->r4_w2_re = NULL;
  table->r4_w2_im = NULL;
  table->r4_w3_re = NULL;
  table->r4_w3_im = NULL;

  if (N <= 0) {
    return 0;
  }

  if (N % 4 != 0) {
    return 1;
  }

  const int size = N / 4;
  const int blocks = (size + 3) / 4;

  table->r4_blocks = blocks;

  table->r4_w1_re = aligned_malloc64(blocks * sizeof(__m256));
  table->r4_w1_im = aligned_malloc64(blocks * sizeof(__m256));

  table->r4_w2_re = aligned_malloc64(blocks * sizeof(__m256));
  table->r4_w2_im = aligned_malloc64(blocks * sizeof(__m256));

  table->r4_w3_re = aligned_malloc64(blocks * sizeof(__m256));
  table->r4_w3_im = aligned_malloc64(blocks * sizeof(__m256));

  if (!table->r4_w1_re || !table->r4_w1_im || !table->r4_w2_re || !table->r4_w2_im || !table->r4_w3_re || !table->r4_w3_im) {
    return 0;
  }

  for (int b = 0; b < blocks; b++) {
    const int k0 = 4 * b;

    table->r4_w1_re[b] = pack4_twiddle_re(table->forward, k0, 1, N);
    table->r4_w1_im[b] = pack4_twiddle_im(table->forward, k0, 1, N);

    table->r4_w2_re[b] = pack4_twiddle_re(table->forward, k0, 2, N);
    table->r4_w2_im[b] = pack4_twiddle_im(table->forward, k0, 2, N);

    table->r4_w3_re[b] = pack4_twiddle_re(table->forward, k0, 3, N);
    table->r4_w3_im[b] = pack4_twiddle_im(table->forward, k0, 3, N);
  }

  return 1;
}

static int twiddle_table_create_radix3_q15_simd(TwiddleTable *table)
{
  const int N = table->N;

  table->r3_q15_blocks = 0;

  table->r3_q15_w1_re = NULL;
  table->r3_q15_w1_im = NULL;
  table->r3_q15_w2_re = NULL;
  table->r3_q15_w2_im = NULL;

  table->r3_q15_w1_re_inv = NULL;
  table->r3_q15_w1_im_inv = NULL;
  table->r3_q15_w2_re_inv = NULL;
  table->r3_q15_w2_im_inv = NULL;

  if (N <= 0) {
    return 0;
  }

  if (N % 3 != 0) {
    return 1;
  }

  const int size = N / 3;
  const int blocks = (size + 3) / 4;

  table->r3_q15_blocks = blocks;

  table->r3_q15_w1_re = aligned_malloc64(blocks * sizeof(__m128i));
  table->r3_q15_w1_im = aligned_malloc64(blocks * sizeof(__m128i));
  table->r3_q15_w2_re = aligned_malloc64(blocks * sizeof(__m128i));
  table->r3_q15_w2_im = aligned_malloc64(blocks * sizeof(__m128i));

  table->r3_q15_w1_re_inv = aligned_malloc64(blocks * sizeof(__m128i));
  table->r3_q15_w1_im_inv = aligned_malloc64(blocks * sizeof(__m128i));
  table->r3_q15_w2_re_inv = aligned_malloc64(blocks * sizeof(__m128i));
  table->r3_q15_w2_im_inv = aligned_malloc64(blocks * sizeof(__m128i));

  if (!table->r3_q15_w1_re || !table->r3_q15_w1_im || !table->r3_q15_w2_re || !table->r3_q15_w2_im || !table->r3_q15_w1_re_inv
      || !table->r3_q15_w1_im_inv || !table->r3_q15_w2_re_inv || !table->r3_q15_w2_im_inv) {
    return 0;
  }

  for (int b = 0; b < blocks; b++) {
    const int k0 = 4 * b;

    table->r3_q15_w1_re[b] = pack4_twiddle_q15_re_re(table->forward, k0, 1, N);
    table->r3_q15_w1_im[b] = pack4_twiddle_q15_im_im(table->forward, k0, 1, N);

    table->r3_q15_w2_re[b] = pack4_twiddle_q15_re_re(table->forward, k0, 2, N);
    table->r3_q15_w2_im[b] = pack4_twiddle_q15_im_im(table->forward, k0, 2, N);

    table->r3_q15_w1_re_inv[b] = pack4_twiddle_q15_re_re(table->inverse, k0, 1, N);
    table->r3_q15_w1_im_inv[b] = pack4_twiddle_q15_im_im(table->inverse, k0, 1, N);

    table->r3_q15_w2_re_inv[b] = pack4_twiddle_q15_re_re(table->inverse, k0, 2, N);
    table->r3_q15_w2_im_inv[b] = pack4_twiddle_q15_im_im(table->inverse, k0, 2, N);
  }

  return 1;
}

static int twiddle_table_create_radix3_simd(TwiddleTable *table)
{
  const int N = table->N;
  table->r3_blocks = 0;
  table->r3_w1_re = NULL;
  table->r3_w1_im = NULL;
  table->r3_w2_re = NULL;
  table->r3_w2_im = NULL;

  if (N <= 0) {
    return 0;
  }
  if (N % 3 != 0) {
    return 1;
  }

  const int size = N / 3;
  const int blocks = (size + 3) / 4;

  table->r3_blocks = blocks;

  table->r3_w1_re = aligned_malloc64(blocks * sizeof(__m256));
  table->r3_w1_im = aligned_malloc64(blocks * sizeof(__m256));
  table->r3_w2_re = aligned_malloc64(blocks * sizeof(__m256));
  table->r3_w2_im = aligned_malloc64(blocks * sizeof(__m256));

  if (!table->r3_w1_re || !table->r3_w1_im || !table->r3_w2_re || !table->r3_w2_im) {
    return 0;
  }

  for (int b = 0; b < blocks; b++) {
    const int k0 = 4 * b;

    table->r3_w1_re[b] = pack4_twiddle_re(table->forward, k0, 1, N);
    table->r3_w1_im[b] = pack4_twiddle_im(table->forward, k0, 1, N);

    table->r3_w2_re[b] = pack4_twiddle_re(table->forward, k0, 2, N);
    table->r3_w2_im[b] = pack4_twiddle_im(table->forward, k0, 2, N);
  }

  return 1;
}

static int twiddle_table_create_radix5_simd(TwiddleTable *table)
{
  const int N = table->N;
  table->r5_blocks = 0;
  table->r5_w1_re = NULL;
  table->r5_w1_im = NULL;
  table->r5_w2_re = NULL;
  table->r5_w2_im = NULL;
  table->r5_w3_re = NULL;
  table->r5_w3_im = NULL;
  table->r5_w4_re = NULL;
  table->r5_w4_im = NULL;

  if (N <= 0) {
    return 0;
  }

  if (N % 5 != 0) {
    return 1;
  }

  const int size = N / 5;
  const int blocks = (size + 3) / 4;

  table->r5_blocks = blocks;

  table->r5_w1_re = aligned_malloc64(blocks * sizeof(__m256));
  table->r5_w1_im = aligned_malloc64(blocks * sizeof(__m256));

  table->r5_w2_re = aligned_malloc64(blocks * sizeof(__m256));
  table->r5_w2_im = aligned_malloc64(blocks * sizeof(__m256));

  table->r5_w3_re = aligned_malloc64(blocks * sizeof(__m256));
  table->r5_w3_im = aligned_malloc64(blocks * sizeof(__m256));

  table->r5_w4_re = aligned_malloc64(blocks * sizeof(__m256));
  table->r5_w4_im = aligned_malloc64(blocks * sizeof(__m256));

  if (!table->r5_w1_re || !table->r5_w1_im || !table->r5_w2_re || !table->r5_w2_im || !table->r5_w3_re || !table->r5_w3_im
      || !table->r5_w4_re || !table->r5_w4_im) {
    return 0;
  }

  for (int b = 0; b < blocks; b++) {
    const int k0 = 4 * b;

    table->r5_w1_re[b] = pack4_twiddle_re(table->forward, k0, 1, N);
    table->r5_w1_im[b] = pack4_twiddle_im(table->forward, k0, 1, N);

    table->r5_w2_re[b] = pack4_twiddle_re(table->forward, k0, 2, N);
    table->r5_w2_im[b] = pack4_twiddle_im(table->forward, k0, 2, N);

    table->r5_w3_re[b] = pack4_twiddle_re(table->forward, k0, 3, N);
    table->r5_w3_im[b] = pack4_twiddle_im(table->forward, k0, 3, N);

    table->r5_w4_re[b] = pack4_twiddle_re(table->forward, k0, 4, N);
    table->r5_w4_im[b] = pack4_twiddle_im(table->forward, k0, 4, N);
  }

  return 1;
}

#define MAX_DFT_SIZE 8192

static TwiddleTable g_tables[MAX_N + 1];

static void fft_forward_recursive_core(const float complex *src, float complex *dst, int N);

static void radix_2_fft_forward(const float complex *src, float complex *dst, int N);

static void radix_3_fft_forward(const float complex *src, float complex *dst, int N);

static void radix_4_fft_forward_lts(const float complex *src, float complex *dst, int N);

static void radix_5_fft_forward(const float complex *src, float complex *dst, int N);

static void classic_dft_forward_cached(const float complex *src, float complex *dst, int N);

static inline void radix4_combine_lts(const TwiddleTable *tw,
                                      const int i,
                                      const __m256 A0,
                                      const __m256 A1,
                                      const __m256 A2,
                                      const __m256 A3,
                                      __m256 *Y0,
                                      __m256 *Y1,
                                      __m256 *Y2,
                                      __m256 *Y3);

static inline void dft64lts(const float complex *src, float complex *dst);

static inline void
dft4x4lts(const __m256 x0, const __m256 x1, const __m256 x2, const __m256 x3, __m256 *Y0, __m256 *Y1, __m256 *Y2, __m256 *Y3);

static inline __m256 complex_mul4_prepack(const __m256 a, const __m256 w_re_re, const __m256 w_im_im);

/*
 * Public API.
 */
void fft_recursive_forward(const float complex *src, float complex *dst, int N);

/* =========================================================
 * Helpers
 * ========================================================= */

static sr_twiddle_simd_t tw512;
static sr_twiddle_simd_t tw1024;

static inline int16_t sat_i16(long v)
{
  if (v > 32767)
    return 32767;
  if (v < -32768)
    return -32767;
  return (int16_t)v;
}

static inline uint64_t ns_now(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void fft_random_complex_numbers(float complex *out, int n)
{
  for (int i = 0; i < n; i++) {
    float re = (float)((rand() % 2001) - 1000);
    float im = (float)((rand() % 2001) - 1000);
    out[i] = re + im * I;
  }
}

static void build_oai_input_from_float(const float complex *in, c16_t *out, int n)
{
  for (int i = 0; i < n; i++) {
    out[i].r = sat_i16(lrintf(crealf(in[i])));
    out[i].i = sat_i16(lrintf(cimagf(in[i])));
  }
}

static void oai_out_to_float_complex(const c16_t *in, float complex *out, int n)
{
  for (int i = 0; i < n; i++) {
    out[i] = (float)in[i].r + (float)in[i].i * I;
  }
}

static void classic_dft_forward(const float complex *x, float complex *y, int n)
{
  for (int k = 0; k < n; k++) {
    y[k] = 0.0f + 0.0f * I;

    for (int m = 0; m < n; m++) {
      float theta = -2.0f * (float)M_PI * k * m / n;
      y[k] += x[m] * (cosf(theta) + I * sinf(theta));
    }
  }
}

static void scale_complex(const float complex *in, float complex *out, int n, float s)
{
  for (int i = 0; i < n; i++)
    out[i] = in[i] * s;
}

static double rms_evm_percent_fc(const float complex *ref, const float complex *got, int n)
{
  double num = 0.0;
  double den = 0.0;

  for (int i = 0; i < n; i++) {
    double rr = crealf(ref[i]);
    double ri = cimagf(ref[i]);
    double gr = crealf(got[i]);
    double gi = cimagf(got[i]);

    double er = gr - rr;
    double ei = gi - ri;

    num += er * er + ei * ei;
    den += rr * rr + ri * ri;
  }

  if (den < 1e-30)
    return 0.0;

  return 100.0 * sqrt(num / den);
}

static double max_abs_err_fc(const float complex *ref, const float complex *got, int n)
{
  double m = 0.0;

  for (int i = 0; i < n; i++) {
    double er = fabs((double)crealf(got[i]) - (double)crealf(ref[i]));
    double ei = fabs((double)cimagf(got[i]) - (double)cimagf(ref[i]));

    if (er > m)
      m = er;
    if (ei > m)
      m = ei;
  }

  return m;
}

/* =========================================================
 * dft64 float twiddles
 * ========================================================= */

typedef struct {
  float complex W16_forward[16] __attribute__((aligned(64)));
  float complex W16_inverse[16] __attribute__((aligned(64)));

  float complex W32_forward[32] __attribute__((aligned(64)));
  float complex W32_inverse[32] __attribute__((aligned(64)));

  float complex W64_forward[64] __attribute__((aligned(64)));
  float complex W64_inverse[64] __attribute__((aligned(64)));

  float complex C16_forward[16][4] __attribute__((aligned(64)));
  float complex C16_inverse[16][4] __attribute__((aligned(64)));

  float complex C32_forward[32][8] __attribute__((aligned(64)));
  float complex C32_inverse[32][8] __attribute__((aligned(64)));

  float complex C64_forward[64][4] __attribute__((aligned(64)));
  float complex C64_inverse[64][4] __attribute__((aligned(64)));

  float complex C64_forward512[64][8] __attribute__((aligned(64)));
  float complex C64_inverse512[64][8] __attribute__((aligned(64)));

  /*
   * SIMD packed twiddles for complex dot products.
   *
   * For each k:
   *
   * C16_RE_NEGIM_forward[k] = [wr0,-wi0, wr1,-wi1, wr2,-wi2, wr3,-wi3]
   * C16_IM_RE_forward[k]    = [wi0, wr0, wi1, wr1, wi2, wr2, wi3, wr3]
   */
  __m256 C16_RE_RE_forward[16] __attribute__((aligned(64)));
  __m256 C16_IM_IM_forward[16] __attribute__((aligned(64)));

  __m256 C16_RE_NEGIM_forward[16] __attribute__((aligned(64)));
  __m256 C16_IM_RE_forward[16] __attribute__((aligned(64)));

  __m256 C64_RE_RE_forward[64] __attribute__((aligned(64)));
  __m256 C64_IM_IM_forward[64] __attribute__((aligned(64)));

  __m128 C64_RE_RE_forward512[64] __attribute__((aligned(64)));
  __m128 C64_IM_IM_forward512[64] __attribute__((aligned(64)));

  __m256 C64_RE_NEGIM_forward[64] __attribute__((aligned(64)));
  __m256 C64_IM_RE_forward[64] __attribute__((aligned(64)));

  __m256 W64_stage1[4][3] __attribute__((aligned(64)));
  __m256 W16_stage2[3] __attribute__((aligned(64)));

  __m256 C16_BCAST_forward[16][4] __attribute__((aligned(64)));
  __m256 C16_BCAST_RE_RE_forward[16][4] __attribute__((aligned(64)));
  __m256 C16_BCAST_IM_IM_forward[16][4] __attribute__((aligned(64)));
  __m256 C16_BCAST_RE_NEGIM_forward[16][4] __attribute__((aligned(64)));
  __m256 C16_BCAST_IM_RE_forward[16][4] __attribute__((aligned(64)));

  __m128 C32_RE_RE_forward[32] __attribute__((aligned(64)));
  __m128 C32_IM_IM_forward[32] __attribute__((aligned(64)));

  __m256 C32_RE_RE_forward256[32][2] __attribute__((aligned(64)));
  __m256 C32_IM_IM_forward256[32][2] __attribute__((aligned(64)));

  __m128 C32_BCAST_RE_RE_forward[32][8] __attribute__((aligned(64)));
  __m128 C32_BCAST_IM_IM_forward[32][8] __attribute__((aligned(64)));
  /*
   * Q15 SIMD tables for lts path
   *
   * Layout:
   * [wr0 wr0 wr1 wr1 wr2 wr2 wr3 wr3]
   * [wi0 wi0 wi1 wi1 wi2 wi2 wi3 wi3]
   */
  __m128i C16_BCAST_RE_RE_q15[16][4] __attribute__((aligned(64)));
  __m128i C16_BCAST_IM_IM_q15[16][4] __attribute__((aligned(64)));

  __m128i C64_RE_RE_q15[64] __attribute__((aligned(64)));
  __m128i C64_IM_IM_q15[64] __attribute__((aligned(64)));

  __m256i C64_RE_RE_q15_256[64] __attribute__((aligned(64)));
  __m256i C64_IM_SIGNED_q15_256[64] __attribute__((aligned(64)));

  __m256i W128_RE_RE_q15_256[8] __attribute__((aligned(64)));
  __m256i W128_IM_SIGNED_q15_256[8] __attribute__((aligned(64)));

  __m256i C64_RE_RE_q15_256_inverse[64] __attribute__((aligned(64)));
  __m256i C64_IM_SIGNED_q15_256_inverse[64] __attribute__((aligned(64)));

  __m256i W128_RE_RE_q15_256_inverse[8] __attribute__((aligned(64)));
  __m256i W128_IM_SIGNED_q15_256_inverse[8] __attribute__((aligned(64)));

  int init_done;
} dft64f_twiddle_t;

static dft64f_twiddle_t g_dft64f_tw;
static inline int16_t f32_to_q15(float x)
{
  int v = (int)lrintf(x * 32768.0f);

  if (v > 32767)
    v = 32767;
  if (v < -32767)
    v = -32767;

  return (int16_t)v;
}

static inline __m256 pack4_complex_twiddles(const float complex *W, int N, int k0, int mult)
{
  float tmp[8] __attribute__((aligned(64)));

  for (int j = 0; j < 4; j++) {
    float complex w = W[((k0 + j) * mult) & (N - 1)];

    tmp[2 * j + 0] = crealf(w);
    tmp[2 * j + 1] = cimagf(w);
  }

  return _mm256_load_ps(tmp);
}

static void init_dft64_float_twiddles(dft64f_twiddle_t *tw)
{
  if (tw->init_done)
    return;

  /*
   * W16
   */
  for (int i = 0; i < 16; i++) {
    float theta = 2.0f * (float)M_PI * i / 16.0f;

    tw->W16_forward[i] = cosf(theta) - I * sinf(theta);
    tw->W16_inverse[i] = cosf(theta) + I * sinf(theta);
  }

  /*
   * W32
   */
  for (int i = 0; i < 32; i++) {
    float theta = 2.0f * (float)M_PI * i / 32.0f;

    tw->W32_forward[i] = cosf(theta) - I * sinf(theta);
    tw->W32_inverse[i] = cosf(theta) + I * sinf(theta);
  }

  /*
   * W64
   */
  for (int i = 0; i < 64; i++) {
    float theta = 2.0f * (float)M_PI * i / 64.0f;

    tw->W64_forward[i] = cosf(theta) - I * sinf(theta);
    tw->W64_inverse[i] = cosf(theta) + I * sinf(theta);
  }

  /*
   * C16[k][0] = W16[0]
   * C16[k][1] = W16[k]
   * C16[k][2] = W16[2k]
   * C16[k][3] = W16[3k]
   */
  for (int k = 0; k < 16; k++) {
    for (int r = 0; r < 4; r++) {
      tw->C16_forward[k][r] = tw->W16_forward[(k * r) & 15];
      tw->C16_inverse[k][r] = tw->W16_inverse[(k * r) & 15];
    }
  }

  /*
   * C32[k][0] = W32[0]
   * C32[k][1] = W32[k]
   * C32[k][2] = W32[2k]
   * C32[k][3] = W32[3k]
   */
  for (int k = 0; k < 32; k++) {
    for (int r = 0; r < 8; r++) {
      tw->C32_forward[k][r] = tw->W32_forward[(k * r) & 31];
      tw->C32_inverse[k][r] = tw->W32_inverse[(k * r) & 31];
    }
  }

  /*
   * C64[k][0] = W64[0]
   * C64[k][1] = W64[k]
   * C64[k][2] = W64[2k]
   * C64[k][3] = W64[3k]
   */
  for (int k = 0; k < 64; k++) {
    for (int r = 0; r < 4; r++) {
      tw->C64_forward[k][r] = tw->W64_forward[(k * r) & 63];
      tw->C64_inverse[k][r] = tw->W64_inverse[(k * r) & 63];
    }
  }

  for (int k = 0; k < 64; k++) {
    for (int r = 0; r < 8; r++) {
      tw->C64_forward512[k][r] = tw->W64_forward[(k * r) & 63];
      tw->C64_inverse512[k][r] = tw->W64_inverse[(k * r) & 63];
    }
  }
  /*
   * SIMD-packed C16 twiddles.
   */
  for (int k = 0; k < 16; k++) {
    float re_re[8] __attribute__((aligned(64)));
    float im_im[8] __attribute__((aligned(64)));

    for (int r = 0; r < 4; r++) {
      float complex w = tw->C16_forward[k][r];

      float wr = crealf(w);
      float wi = cimagf(w);

      re_re[2 * r + 0] = wr;
      re_re[2 * r + 1] = wr;

      im_im[2 * r + 0] = wi;
      im_im[2 * r + 1] = wi;
    }

    tw->C16_RE_RE_forward[k] = _mm256_load_ps(re_re);
    tw->C16_IM_IM_forward[k] = _mm256_load_ps(im_im);
  }

  /*
   * SIMD-packed C32 twiddles.
   */

  for (int k = 0; k < 32; k++) {
    float re_re[16] __attribute__((aligned(64)));
    float im_im[16] __attribute__((aligned(64)));

    for (int r = 0; r < 8; r++) {
      float complex w = tw->C32_forward[k][r];

      float wr = crealf(w);
      float wi = cimagf(w);

      re_re[2 * r + 0] = wr;
      re_re[2 * r + 1] = wr;

      im_im[2 * r + 0] = wi;
      im_im[2 * r + 1] = wi;
    }

    tw->C32_RE_RE_forward256[k][0] = _mm256_load_ps(&re_re[0]); // r = 0..3
    tw->C32_RE_RE_forward256[k][1] = _mm256_load_ps(&re_re[8]); // r = 4..7

    tw->C32_IM_IM_forward256[k][0] = _mm256_load_ps(&im_im[0]); // r = 0..3
    tw->C32_IM_IM_forward256[k][1] = _mm256_load_ps(&im_im[8]); // r = 4..7
  }

  /*
   * SIMD-packed C64 twiddles.
   */
  for (int k = 0; k < 64; k++) {
    float re_re[8] __attribute__((aligned(64)));
    float im_im[8] __attribute__((aligned(64)));

    for (int r = 0; r < 4; r++) {
      float complex w = tw->C64_forward[k][r];

      float wr = crealf(w);
      float wi = cimagf(w);

      re_re[2 * r + 0] = wr;
      re_re[2 * r + 1] = wr;

      im_im[2 * r + 0] = wi;
      im_im[2 * r + 1] = wi;
    }

    tw->C64_RE_RE_forward[k] = _mm256_load_ps(re_re);
    tw->C64_IM_IM_forward[k] = _mm256_load_ps(im_im);
  }

  for (int b = 0; b < 4; b++) {
    int k0 = 4 * b;

    g_dft64f_tw.W64_stage1[b][0] = pack4_complex_twiddles(tw->W64_forward, 64, k0, 1);

    g_dft64f_tw.W64_stage1[b][1] = pack4_complex_twiddles(tw->W64_forward, 64, k0, 2);

    g_dft64f_tw.W64_stage1[b][2] = pack4_complex_twiddles(tw->W64_forward, 64, k0, 3);
  }

  g_dft64f_tw.W16_stage2[0] = pack4_complex_twiddles(tw->W16_forward, 16, 0, 1);

  g_dft64f_tw.W16_stage2[1] = pack4_complex_twiddles(tw->W16_forward, 16, 0, 2);

  g_dft64f_tw.W16_stage2[2] = pack4_complex_twiddles(tw->W16_forward, 16, 0, 3);

  for (int k = 0; k < 16; k++) {
    for (int r = 0; r < 4; r++) {
      float complex w = tw->C16_forward[k][r];

      float wr = crealf(w);
      float wi = cimagf(w);

      float tmp[8] __attribute__((aligned(64))) = {wr, wi, wr, wi, wr, wi, wr, wi};

      tw->C16_BCAST_forward[k][r] = _mm256_load_ps(tmp);
    }
  }

  for (int k = 0; k < 16; k++) {
    for (int r = 0; r < 4; r++) {
      float complex w = tw->C16_forward[k][r];

      float wr = crealf(w);
      float wi = cimagf(w);

      float re_re[8] __attribute__((aligned(64))) = {wr, wr, wr, wr, wr, wr, wr, wr};

      float im_im[8] __attribute__((aligned(64))) = {wi, wi, wi, wi, wi, wi, wi, wi};

      tw->C16_BCAST_RE_RE_forward[k][r] = _mm256_load_ps(re_re);
      tw->C16_BCAST_IM_IM_forward[k][r] = _mm256_load_ps(im_im);
    }
  }
  /*
   * Q15 C16 broadcast tables
   */
  for (int k = 0; k < 16; k++) {
    for (int r = 0; r < 4; r++) {
      float complex w = tw->C16_forward[k][r];

      int16_t wr = f32_to_q15(crealf(w));
      int16_t wi = f32_to_q15(cimagf(w));

      tw->C16_BCAST_RE_RE_q15[k][r] = _mm_set1_epi16(wr);
      tw->C16_BCAST_IM_IM_q15[k][r] = _mm_set_epi16(+wi, -wi, +wi, -wi, +wi, -wi, +wi, -wi);
    }
  }

  /*
   * Q15 C64 packed tables
   */
  for (int k = 0; k < 64; k++) {
    int16_t re_re[8] __attribute__((aligned(16)));
    int16_t im_im[8] __attribute__((aligned(16)));

    for (int r = 0; r < 4; r++) {
      float complex w = tw->C64_forward[k][r];

      int16_t wr = f32_to_q15(crealf(w));
      int16_t wi = f32_to_q15(cimagf(w));

      re_re[2 * r + 0] = wr;
      re_re[2 * r + 1] = wr;

      im_im[2 * r + 0] = -wi;
      im_im[2 * r + 1] = wi;
    }

    tw->C64_RE_RE_q15[k] = _mm_load_si128((const __m128i *)re_re);
    tw->C64_IM_IM_q15[k] = _mm_load_si128((const __m128i *)im_im);
  }

  for (int k = 0; k < 64; k++) {
    int16_t re_re[16] __attribute__((aligned(32)));
    int16_t im_signed[16] __attribute__((aligned(32)));

    for (int r = 0; r < 8; r++) {
      float complex w = tw->C64_forward512[k][r];

      int16_t wr = f32_to_q15(crealf(w)) / 8;
      int16_t wi = f32_to_q15(cimagf(w)) / 8;

      re_re[2 * r + 0] = wr;
      re_re[2 * r + 1] = wr;

      im_signed[2 * r + 0] = -wi;
      im_signed[2 * r + 1] = wi;
    }

    tw->C64_RE_RE_q15_256[k] = _mm256_load_si256((const __m256i *)(const void *)re_re);

    tw->C64_IM_SIGNED_q15_256[k] = _mm256_load_si256((const __m256i *)(const void *)im_signed);
  }

  for (int k = 0; k < 64; k++) {
    int16_t re_re[16] __attribute__((aligned(32)));
    int16_t im_signed[16] __attribute__((aligned(32)));

    for (int r = 0; r < 8; r++) {
      float complex w = tw->C64_inverse512[k][r];

      int16_t wr = f32_to_q15(crealf(w)) / 8;
      int16_t wi = f32_to_q15(cimagf(w)) / 8;

      re_re[2 * r + 0] = wr;
      re_re[2 * r + 1] = wr;

      im_signed[2 * r + 0] = -wi;
      im_signed[2 * r + 1] = wi;
    }

    tw->C64_RE_RE_q15_256_inverse[k] = _mm256_load_si256((const __m256i *)(const void *)re_re);

    tw->C64_IM_SIGNED_q15_256_inverse[k] = _mm256_load_si256((const __m256i *)(const void *)im_signed);
  }

  for (int b = 0; b < 8; b++) {
    int16_t re_re[16] __attribute__((aligned(32)));
    int16_t im_signed[16] __attribute__((aligned(32)));

    for (int r = 0; r < 8; r++) {
      int n = 8 * b + r;

      float theta = 2.0f * (float)M_PI * n / 128.0f;

      float wr_f = cosf(theta);
      float wi_f = -sinf(theta);

      int16_t wr = f32_to_q15(wr_f) / sqrtf(2.0f);
      int16_t wi = f32_to_q15(wi_f) / sqrtf(2.0f);

      re_re[2 * r + 0] = wr;
      re_re[2 * r + 1] = wr;

      im_signed[2 * r + 0] = -wi;
      im_signed[2 * r + 1] = wi;
    }

    tw->W128_RE_RE_q15_256[b] = _mm256_load_si256((const __m256i *)(const void *)re_re);

    tw->W128_IM_SIGNED_q15_256[b] = _mm256_load_si256((const __m256i *)(const void *)im_signed);
  }

  for (int b = 0; b < 8; b++) {
    int16_t re_re[16] __attribute__((aligned(32)));
    int16_t im_signed[16] __attribute__((aligned(32)));

    for (int r = 0; r < 8; r++) {
      int n = 8 * b + r;

      float theta = 2.0f * (float)M_PI * n / 128.0f;

      float wr_f = cosf(theta);
      float wi_f = sinf(theta);

      int16_t wr = f32_to_q15(wr_f) / sqrtf(2.0f);
      int16_t wi = f32_to_q15(wi_f) / sqrtf(2.0f);

      re_re[2 * r + 0] = wr;
      re_re[2 * r + 1] = wr;

      im_signed[2 * r + 0] = -wi;
      im_signed[2 * r + 1] = wi;
    }

    tw->W128_RE_RE_q15_256_inverse[b] = _mm256_load_si256((const __m256i *)(const void *)re_re);

    tw->W128_IM_SIGNED_q15_256_inverse[b] = _mm256_load_si256((const __m256i *)(const void *)im_signed);
  }

  tw->init_done = 1;
}

/* =========================================================
 * Q15 complex helpers
 * ========================================================= */
static inline simde__m256i c16_mul_q15_simd256(simde__m256i x, simde__m256i w_re_negim, simde__m256i w_im_re)
{
  // simde__m256i zero  = simde_mm256_setzero_si256();
  simde__m256i round = simde_mm256_set1_epi32(1 << 14);

  simde__m256i re32 = simde_mm256_madd_epi16(x, w_re_negim);
  simde__m256i im32 = simde_mm256_madd_epi16(x, w_im_re);

  re32 = simde_mm256_srai_epi32(simde_mm256_add_epi32(re32, round), 15);
  im32 = simde_mm256_srai_epi32(simde_mm256_add_epi32(im32, round), 15);

  simde__m256i packed = simde_mm256_packs_epi32(re32, im32);

  const simde__m256i mask = simde_mm256_set_epi8(15,
                                                 14,
                                                 7,
                                                 6,
                                                 13,
                                                 12,
                                                 5,
                                                 4,
                                                 11,
                                                 10,
                                                 3,
                                                 2,
                                                 9,
                                                 8,
                                                 1,
                                                 0,
                                                 15,
                                                 14,
                                                 7,
                                                 6,
                                                 13,
                                                 12,
                                                 5,
                                                 4,
                                                 11,
                                                 10,
                                                 3,
                                                 2,
                                                 9,
                                                 8,
                                                 1,
                                                 0);

  return simde_mm256_shuffle_epi8(packed, mask);
}

static void init_sr_twiddle_f32_prepack(sr_twiddle_f32_prepack_t *tw, int N)
{
  const int quarter = N / 4;

  const int blocks = quarter / 4;

  tw->N = N;
  tw->blocks = blocks;

  tw->W1_RE_RE = aligned_alloc(32, sizeof(__m256) * blocks);
  tw->W1_IM_IM = aligned_alloc(32, sizeof(__m256) * blocks);
  tw->W3_RE_RE = aligned_alloc(32, sizeof(__m256) * blocks);
  tw->W3_IM_IM = aligned_alloc(32, sizeof(__m256) * blocks);

  if (!tw->W1_RE_RE || !tw->W1_IM_IM || !tw->W3_RE_RE || !tw->W3_IM_IM) {
    fprintf(stderr, "aligned_alloc failed in init_sr_twiddle_f32_prepack\n");
    exit(EXIT_FAILURE);
  }

  for (int b = 0; b < blocks; b++) {
    float w1_re_re[8] __attribute__((aligned(64)));
    float w1_im_im[8] __attribute__((aligned(64)));

    float w3_re_re[8] __attribute__((aligned(64)));
    float w3_im_im[8] __attribute__((aligned(64)));

    for (int j = 0; j < 4; j++) {
      int k = 4 * b + j;

      float theta1 = -2.0f * (float)M_PI * (float)k / (float)N;
      float theta3 = -6.0f * (float)M_PI * (float)k / (float)N;

      float w1r = cosf(theta1);
      float w1i = sinf(theta1);

      float w3r = cosf(theta3);
      float w3i = sinf(theta3);

      /*
       * [wr, wr]
       * [wi, wi]
       */
      w1_re_re[2 * j] = w1r;
      w1_re_re[2 * j + 1] = w1r;
      w1_im_im[2 * j] = w1i;
      w1_im_im[2 * j + 1] = w1i;

      w3_re_re[2 * j] = w3r;
      w3_re_re[2 * j + 1] = w3r;
      w3_im_im[2 * j] = w3i;
      w3_im_im[2 * j + 1] = w3i;
    }

    tw->W1_RE_RE[b] = _mm256_load_ps(w1_re_re);
    tw->W1_IM_IM[b] = _mm256_load_ps(w1_im_im);

    tw->W3_RE_RE[b] = _mm256_load_ps(w3_re_re);
    tw->W3_IM_IM[b] = _mm256_load_ps(w3_im_im);
  }
}

static void init_sr_twiddle_simd(sr_twiddle_simd_t *tw, int N, dft_dir_t dir)
{
  int quarter = N / 4;
  int blocks = quarter / 8;

  tw->N = N;
  tw->blocks = blocks;

  tw->W1_RE_NEGIM = aligned_alloc(32, sizeof(simde__m256i) * blocks);
  tw->W1_IM_RE = aligned_alloc(32, sizeof(simde__m256i) * blocks);
  tw->W3_RE_NEGIM = aligned_alloc(32, sizeof(simde__m256i) * blocks);
  tw->W3_IM_RE = aligned_alloc(32, sizeof(simde__m256i) * blocks);

  for (int b = 0; b < blocks; b++) {
    int16_t w1_re_negim[16] __attribute__((aligned(64)));
    int16_t w1_im_re[16] __attribute__((aligned(64)));
    int16_t w3_re_negim[16] __attribute__((aligned(64)));
    int16_t w3_im_re[16] __attribute__((aligned(64)));

    for (int j = 0; j < 8; j++) {
      int k = 8 * b + j;

      float theta1 = (float)dir * 2.0f * (float)M_PI * k / (float)N;
      float theta3 = (float)dir * 6.0f * (float)M_PI * k / (float)N;

      int16_t w1r = sat_i16(lrintf(32767.0f * cosf(theta1))) / 2;
      int16_t w1i = sat_i16(lrintf(32767.0f * sinf(theta1))) / 2;

      int16_t w3r = sat_i16(lrintf(32767.0f * cosf(theta3))) / 2;
      int16_t w3i = sat_i16(lrintf(32767.0f * sinf(theta3))) / 2;

      w1_re_negim[2 * j] = w1r;
      w1_re_negim[2 * j + 1] = sat_i16(-(long)w1i);
      w1_im_re[2 * j] = w1i;
      w1_im_re[2 * j + 1] = w1r;

      w3_re_negim[2 * j] = w3r;
      w3_re_negim[2 * j + 1] = sat_i16(-(long)w3i);
      w3_im_re[2 * j] = w3i;
      w3_im_re[2 * j + 1] = w3r;
    }

    tw->W1_RE_NEGIM[b] = simde_mm256_load_si256((simde__m256i *)w1_re_negim);
    tw->W1_IM_RE[b] = simde_mm256_load_si256((simde__m256i *)w1_im_re);
    tw->W3_RE_NEGIM[b] = simde_mm256_load_si256((simde__m256i *)w3_re_negim);
    tw->W3_IM_RE[b] = simde_mm256_load_si256((simde__m256i *)w3_im_re);
  }
}

static inline void sr_combine_simd(c16_t *E, c16_t *O1, c16_t *O3, c16_t *y, int N, const sr_twiddle_simd_t *tw, dft_dir_t dir)
{
  int half = N / 2;
  int quarter = N / 4;

  const simde__m256i swap_mask = simde_mm256_setr_epi8(2,
                                                       3,
                                                       0,
                                                       1,
                                                       6,
                                                       7,
                                                       4,
                                                       5,
                                                       10,
                                                       11,
                                                       8,
                                                       9,
                                                       14,
                                                       15,
                                                       12,
                                                       13,
                                                       2,
                                                       3,
                                                       0,
                                                       1,
                                                       6,
                                                       7,
                                                       4,
                                                       5,
                                                       10,
                                                       11,
                                                       8,
                                                       9,
                                                       14,
                                                       15,
                                                       12,
                                                       13);

  simde__m256i sign_mask;

  if (dir == DFT_DIR_FORWARD) {
    sign_mask = simde_mm256_setr_epi16(1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1);
  } else {
    sign_mask = simde_mm256_setr_epi16(-1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1);
  }

  const simde__m256i sqrt2_inv = simde_mm256_set1_epi16(ONE_OVER_SQRT2_Q15);

  for (int b = 0; b < tw->blocks; b++) {
    int k = 8 * b;

    simde__m256i O1v = simde_mm256_load_si256((simde__m256i *)&O1[k]);
    simde__m256i O3v = simde_mm256_load_si256((simde__m256i *)&O3[k]);

    simde__m256i t1 = c16_mul_q15_simd256(O1v, tw->W1_RE_NEGIM[b], tw->W1_IM_RE[b]);

    simde__m256i t2 = c16_mul_q15_simd256(O3v, tw->W3_RE_NEGIM[b], tw->W3_IM_RE[b]);

    simde__m256i a = simde_mm256_add_epi16(t1, t2);
    simde__m256i d = simde_mm256_sub_epi16(t1, t2);

    simde__m256i bval = simde_mm256_shuffle_epi8(d, swap_mask);
    bval = simde_mm256_sign_epi16(bval, sign_mask);

    simde__m256i E0 = simde_mm256_load_si256((simde__m256i *)&E[k]);
    simde__m256i E1 = simde_mm256_load_si256((simde__m256i *)&E[k + quarter]);

    E0 = simde_mm256_mulhrs_epi16(E0, sqrt2_inv);
    E1 = simde_mm256_mulhrs_epi16(E1, sqrt2_inv);

    simde__m256i Y0 = simde_mm256_add_epi16(E0, a);
    simde__m256i Y2 = simde_mm256_sub_epi16(E0, a);
    simde__m256i Y1 = simde_mm256_add_epi16(E1, bval);
    simde__m256i Y3 = simde_mm256_sub_epi16(E1, bval);

    simde_mm256_store_si256((simde__m256i *)&y[k], Y0);
    simde_mm256_store_si256((simde__m256i *)&y[k + quarter], Y1);
    simde_mm256_store_si256((simde__m256i *)&y[k + half], Y2);
    simde_mm256_store_si256((simde__m256i *)&y[k + 3 * quarter], Y3);
  }
}

/* =========================================================
 * Timing
 * ========================================================= */

static double time_classic_dft64(const float complex *in, float complex *out)
{
  const int W = 20;
  const int T = 200;

  for (int i = 0; i < W; i++)
    classic_dft_forward(in, out, 64);

  uint64_t t0 = ns_now();

  for (int i = 0; i < T; i++)
    classic_dft_forward(in, out, 64);

  uint64_t t1 = ns_now();

  return (double)(t1 - t0) / (double)T;
}

static volatile int32_t g_sink_i32;

static inline void consume_c16_output(const c16_t *y, int N)
{
  int32_t acc = 0;

  for (int i = 0; i < N; i++) {
    acc += y[i].r;
    acc += y[i].i;
  }

  g_sink_i32 += acc;
}
#define REPEAT 100000
static inline uint64_t rdtsc_begin(void)
{
  unsigned int lo, hi;

  __asm__ __volatile__(
      "lfence\n\t"
      "rdtsc\n\t"
      : "=a"(lo), "=d"(hi)
      :
      : "memory");

  return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t rdtsc_end(void)
{
  unsigned int lo, hi, aux;

  __asm__ __volatile__(
      "rdtscp\n\t"
      "lfence\n\t"
      : "=a"(lo), "=d"(hi), "=c"(aux)
      :
      : "memory");

  return ((uint64_t)hi << 32) | lo;
}

static double time_oai256_ns_per_dft(c16_t *in, c16_t *out, int N)
{
  const int W = 2000000;
  const int T = 20000000;

  dft_size_idx_t idx = get_dft(N);

  for (int i = 0; i < W; i++)
    dft(idx, (int16_t *)in, (int16_t *)out, 1);

  uint64_t t0 = ns_now();

  for (int i = 0; i < T; i++) {
    dft(idx, (int16_t *)in, (int16_t *)out, 1);
  }

  uint64_t t1 = ns_now();

  return (double)(t1 - t0) / (double)T;
}

/* =========================================================
 * split radix int
 * ========================================================= */

static inline void pack_split_radix_input_avx2_fused(const c16_t *__restrict x, c16_t *__restrict sub_in, int N)
{
  _Static_assert(sizeof(c16_t) == 4, "c16_t must be 32-bit");

  const int half = N >> 1;
  const int quarter = N >> 2;

  c16_t *__restrict E_in = sub_in;
  c16_t *__restrict O1_in = sub_in + half;
  c16_t *__restrict O3_in = sub_in + half + quarter;

  const simde__m256i idx = simde_mm256_setr_epi32(0, 2, 4, 6, 1, 5, 3, 7);

  int in = 0;
  int e = 0;
  int o = 0;

  for (; in + 32 <= N; in += 32, e += 16, o += 8) {
    simde__m256i v0 = simde_mm256_loadu_si256((const simde__m256i *)&x[in + 0]);

    simde__m256i v1 = simde_mm256_loadu_si256((const simde__m256i *)&x[in + 8]);

    simde__m256i v2 = simde_mm256_loadu_si256((const simde__m256i *)&x[in + 16]);

    simde__m256i v3 = simde_mm256_loadu_si256((const simde__m256i *)&x[in + 24]);

    /*
     * p0 = [x0,  x2,  x4,  x6,  x1,  x5,  x3,  x7]
     * p1 = [x8,  x10, x12, x14, x9,  x13, x11, x15]
     * p2 = [x16, x18, x20, x22, x17, x21, x19, x23]
     * p3 = [x24, x26, x28, x30, x25, x29, x27, x31]
     */
    simde__m256i p0 = simde_mm256_permutevar8x32_epi32(v0, idx);
    simde__m256i p1 = simde_mm256_permutevar8x32_epi32(v1, idx);
    simde__m256i p2 = simde_mm256_permutevar8x32_epi32(v2, idx);
    simde__m256i p3 = simde_mm256_permutevar8x32_epi32(v3, idx);

    /*
     * E output:
     *
     * low128(p0) = x0,  x2,  x4,  x6
     * low128(p1) = x8,  x10, x12, x14
     * low128(p2) = x16, x18, x20, x22
     * low128(p3) = x24, x26, x28, x30
     */
    simde_mm_store_si128((simde__m128i *)&E_in[e + 0], simde_mm256_castsi256_si128(p0));

    simde_mm_store_si128((simde__m128i *)&E_in[e + 4], simde_mm256_castsi256_si128(p1));

    simde_mm_store_si128((simde__m128i *)&E_in[e + 8], simde_mm256_castsi256_si128(p2));

    simde_mm_store_si128((simde__m128i *)&E_in[e + 12], simde_mm256_castsi256_si128(p3));

    /*
     * high128(p0) = x1,  x5,  x3,  x7
     * high128(p1) = x9,  x13, x11, x15
     * high128(p2) = x17, x21, x19, x23
     * high128(p3) = x25, x29, x27, x31
     */
    simde__m128i h0 = simde_mm256_extracti128_si256(p0, 1);
    simde__m128i h1 = simde_mm256_extracti128_si256(p1, 1);
    simde__m128i h2 = simde_mm256_extracti128_si256(p2, 1);
    simde__m128i h3 = simde_mm256_extracti128_si256(p3, 1);

    /*
     * O1:
     * unpacklo_epi64(h0,h1) = x1,  x5,  x9,  x13
     * unpacklo_epi64(h2,h3) = x17, x21, x25, x29
     *
     * O3:
     * unpackhi_epi64(h0,h1) = x3,  x7,  x11, x15
     * unpackhi_epi64(h2,h3) = x19, x23, x27, x31
     */
    simde__m128i o1_0 = simde_mm_unpacklo_epi64(h0, h1);
    simde__m128i o3_0 = simde_mm_unpackhi_epi64(h0, h1);

    simde__m128i o1_1 = simde_mm_unpacklo_epi64(h2, h3);
    simde__m128i o3_1 = simde_mm_unpackhi_epi64(h2, h3);

    simde_mm_store_si128((simde__m128i *)&O1_in[o + 0], o1_0);
    simde_mm_store_si128((simde__m128i *)&O1_in[o + 4], o1_1);

    simde_mm_store_si128((simde__m128i *)&O3_in[o + 0], o3_0);
    simde_mm_store_si128((simde__m128i *)&O3_in[o + 4], o3_1);
  }
}

/* =========================================================
 * dft64 float
 * ========================================================= */

static void init_sr_twiddles_f32_power2(int maxN)
{
  for (int N = 32; N <= maxN; N <<= 1) {
    init_sr_twiddle_f32_prepack(&sr_twiddles_f32[log2_int(N)], N);
  }
}

static void init_sr_twiddles_power2(int maxN)
{
  for (int N = 32; N <= maxN; N <<= 1) {
    init_sr_twiddle_simd(&sr_twiddles_fwd[log2_int(N)], N, DFT_DIR_FORWARD);
    init_sr_twiddle_simd(&sr_twiddles_bwd[log2_int(N)], N, DFT_DIR_INVERSE);
  }
}

static inline __m256 mullts(__m256 z)
{
  const __m256 swapped = _mm256_permute_ps(z, 0xB1);
  const __m256 sign = _mm256_setr_ps(-0.0f, 0.0f, -0.0f, 0.0f, -0.0f, 0.0f, -0.0f, 0.0f);
  return _mm256_xor_ps(swapped, sign);
}

static inline __m256 mul_minuslts(__m256 z)
{
  const __m256 swapped = _mm256_permute_ps(z, 0xB1);
  const __m256 sign = _mm256_setr_ps(0.0f, -0.0f, 0.0f, -0.0f, 0.0f, -0.0f, 0.0f, -0.0f);
  return _mm256_xor_ps(swapped, sign);
}

static inline void
dft4x4lts(const __m256 x0, const __m256 x1, const __m256 x2, const __m256 x3, __m256 *Y0, __m256 *Y1, __m256 *Y2, __m256 *Y3)
{
  const __m256 s02 = _mm256_add_ps(x0, x2);
  const __m256 d02 = _mm256_sub_ps(x0, x2);

  const __m256 s13 = _mm256_add_ps(x1, x3);
  const __m256 d13 = _mm256_sub_ps(x1, x3);

  *Y0 = _mm256_add_ps(s02, s13);
  *Y2 = _mm256_sub_ps(s02, s13);
  *Y1 = _mm256_add_ps(d02, mul_minuslts(d13));
  *Y3 = _mm256_add_ps(d02, mullts(d13));
}

static inline __m256 complex_mul4_prepack(const __m256 a, const __m256 w_re_re, const __m256 w_im_im)
{
  __m256 a_swapped = _mm256_shuffle_ps(a, a, 0xB1);
  __m256 prod_im = _mm256_mul_ps(a_swapped, w_im_im);
  return _mm256_fmaddsub_ps(a, w_re_re, prod_im);
}

static inline void combine16lts(__m256 H[4][4], __m256 G[16])
{
  for (int i = 0; i < 4; i++) {
    __m256 B[4];
    B[0] = H[0][i];
    for (int j = 1; j < 4; j++)
      B[j] = complex_mul4_prepack(H[j][i], g_dft64f_tw.C16_BCAST_RE_RE_forward[i][j], g_dft64f_tw.C16_BCAST_IM_IM_forward[i][j]);
    dft4x4lts(B[0], B[1], B[2], B[3], G + i, G + i + 4, G + i + 8, G + i + 12);
  }
}

static inline void dft16x4lts(const float complex *src, __m256 G[16])
{
  __m256 H[4][4] __attribute__((aligned(64)));
  const __m256 *tmp = (const __m256 *)src;
  for (int i = 0; i < 4; i++)
    dft4x4lts(tmp[i], tmp[i + 4], tmp[i + 8], tmp[i + 12], H[i], &H[i][1], H[i] + 2, H[i] + 3);
  combine16lts(H, G);
}

static inline void dft4x4lts_dst(const __m256 x0, const __m256 x1, const __m256 x2, const __m256 x3, float complex *dst)
{
  const __m256 s02 = _mm256_add_ps(x0, x2);
  const __m256 d02 = _mm256_sub_ps(x0, x2);

  const __m256 s13 = _mm256_add_ps(x1, x3);
  const __m256 d13 = _mm256_sub_ps(x1, x3);

  __m256 *tmp = (__m256 *)dst;
  tmp[0] = _mm256_add_ps(s02, s13);
  tmp[4] = _mm256_add_ps(d02, mul_minuslts(d13));
  tmp[8] = _mm256_sub_ps(s02, s13);
  tmp[12] = _mm256_add_ps(d02, mullts(d13));
}

static inline void transpose4_complex_shuffle_ps(__m256 *Y0, __m256 *Y1, __m256 *Y2, __m256 *Y3)
{
  __m256 a = *Y0;
  __m256 b = *Y1;
  __m256 c = *Y2;
  __m256 d = *Y3;

  __m256 ab_lo = _mm256_shuffle_ps(a, b, _MM_SHUFFLE(1, 0, 1, 0));
  __m256 ab_hi = _mm256_shuffle_ps(a, b, _MM_SHUFFLE(3, 2, 3, 2));
  __m256 cd_lo = _mm256_shuffle_ps(c, d, _MM_SHUFFLE(1, 0, 1, 0));
  __m256 cd_hi = _mm256_shuffle_ps(c, d, _MM_SHUFFLE(3, 2, 3, 2));

  *Y0 = _mm256_permute2f128_ps(ab_lo, cd_lo, 0x20);
  *Y1 = _mm256_permute2f128_ps(ab_hi, cd_hi, 0x20);
  *Y2 = _mm256_permute2f128_ps(ab_lo, cd_lo, 0x31);
  *Y3 = _mm256_permute2f128_ps(ab_hi, cd_hi, 0x31);
}

static inline void combine64lts_lts(const __m256 G[16], float complex *dst)
{
  for (int i = 0; i < 16; i += 4) {
    __m256 B0 = complex_mul4_prepack(G[i + 0], g_dft64f_tw.C64_RE_RE_forward[i + 0], g_dft64f_tw.C64_IM_IM_forward[i + 0]);

    __m256 B1 = complex_mul4_prepack(G[i + 1], g_dft64f_tw.C64_RE_RE_forward[i + 1], g_dft64f_tw.C64_IM_IM_forward[i + 1]);

    __m256 B2 = complex_mul4_prepack(G[i + 2], g_dft64f_tw.C64_RE_RE_forward[i + 2], g_dft64f_tw.C64_IM_IM_forward[i + 2]);

    __m256 B3 = complex_mul4_prepack(G[i + 3], g_dft64f_tw.C64_RE_RE_forward[i + 3], g_dft64f_tw.C64_IM_IM_forward[i + 3]);
    transpose4_complex_shuffle_ps(&B0, &B1, &B2, &B3);

    dft4x4lts_dst(B0, B1, B2, B3, dst + i);
  }
}

static void dft64lts(const float complex *src, float complex *dst)
{
  __m256 G[16] __attribute__((aligned(64)));
  dft16x4lts(src, G);
  combine64lts_lts(G, dst);
}

//===================================================================
// DFT64 8x8 int
//===================================================================

static inline __m256i swap_complex_pairs_i16_256(__m256i a)
{
  const __m256i shuf = _mm256_setr_epi8(2,
                                        3,
                                        0,
                                        1,
                                        6,
                                        7,
                                        4,
                                        5,
                                        10,
                                        11,
                                        8,
                                        9,
                                        14,
                                        15,
                                        12,
                                        13,

                                        2,
                                        3,
                                        0,
                                        1,
                                        6,
                                        7,
                                        4,
                                        5,
                                        10,
                                        11,
                                        8,
                                        9,
                                        14,
                                        15,
                                        12,
                                        13);

  return _mm256_shuffle_epi8(a, shuf);
}

static inline __m256i mul_j_i16_256(__m256i z)
{
  const __m256i swapped = swap_complex_pairs_i16_256(z);

  const __m256i sign = _mm256_setr_epi16(-1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1);

  return _mm256_sign_epi16(swapped, sign);
}

static inline __m256i mul_minus_j_i16_256(__m256i z)
{
  const __m256i swapped = swap_complex_pairs_i16_256(z);

  const __m256i sign = _mm256_setr_epi16(+1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1);

  return _mm256_sign_epi16(swapped, sign);
}

static inline __m256i mul_minus_j_dir_i16_256(__m256i z, dft_dir_t dir)
{
  return (dir == DFT_DIR_FORWARD) ? mul_minus_j_i16_256(z) : mul_j_i16_256(z);
}

static inline __m256i mul_plus_j_dir_i16_256(__m256i z, dft_dir_t dir)
{
  return (dir == DFT_DIR_FORWARD) ? mul_j_i16_256(z) : mul_minus_j_i16_256(z);
}

static inline void
transpose8_complex_i16_256(__m256i *r0, __m256i *r1, __m256i *r2, __m256i *r3, __m256i *r4, __m256i *r5, __m256i *r6, __m256i *r7)
{
  const __m256i a = *r0;
  const __m256i b = *r1;
  const __m256i c = *r2;
  const __m256i d = *r3;
  const __m256i e = *r4;
  const __m256i f = *r5;
  const __m256i g = *r6;
  const __m256i h = *r7;

  const __m256i t0 = _mm256_unpacklo_epi32(a, b);
  const __m256i t1 = _mm256_unpackhi_epi32(a, b);
  const __m256i t2 = _mm256_unpacklo_epi32(c, d);
  const __m256i t3 = _mm256_unpackhi_epi32(c, d);
  const __m256i t4 = _mm256_unpacklo_epi32(e, f);
  const __m256i t5 = _mm256_unpackhi_epi32(e, f);
  const __m256i t6 = _mm256_unpacklo_epi32(g, h);
  const __m256i t7 = _mm256_unpackhi_epi32(g, h);

  const __m256i s0 = _mm256_unpacklo_epi64(t0, t2);
  const __m256i s1 = _mm256_unpackhi_epi64(t0, t2);
  const __m256i s2 = _mm256_unpacklo_epi64(t1, t3);
  const __m256i s3 = _mm256_unpackhi_epi64(t1, t3);

  const __m256i s4 = _mm256_unpacklo_epi64(t4, t6);
  const __m256i s5 = _mm256_unpackhi_epi64(t4, t6);
  const __m256i s6 = _mm256_unpacklo_epi64(t5, t7);
  const __m256i s7 = _mm256_unpackhi_epi64(t5, t7);

  *r0 = _mm256_permute2x128_si256(s0, s4, 0x20);
  *r1 = _mm256_permute2x128_si256(s1, s5, 0x20);
  *r2 = _mm256_permute2x128_si256(s2, s6, 0x20);
  *r3 = _mm256_permute2x128_si256(s3, s7, 0x20);

  *r4 = _mm256_permute2x128_si256(s0, s4, 0x31);
  *r5 = _mm256_permute2x128_si256(s1, s5, 0x31);
  *r6 = _mm256_permute2x128_si256(s2, s6, 0x31);
  *r7 = _mm256_permute2x128_si256(s3, s7, 0x31);
}

static inline __m256i complex_mul8_prepack_q15_256(__m256i a, __m256i w_re_re, __m256i w_im_signed)
{
  const __m256i a_swapped = swap_complex_pairs_i16_256(a);

  const __m256i prod_re = _mm256_mulhrs_epi16(a, w_re_re);
  const __m256i prod_im = _mm256_mulhrs_epi16(a_swapped, w_im_signed);

  return _mm256_adds_epi16(prod_re, prod_im);
}

static inline void dft8x8lts_q15_256_dir(const __m256i x0,
                                         const __m256i x1,
                                         const __m256i x2,
                                         const __m256i x3,
                                         const __m256i x4,
                                         const __m256i x5,
                                         const __m256i x6,
                                         const __m256i x7,
                                         __m256i *Y0,
                                         __m256i *Y1,
                                         __m256i *Y2,
                                         __m256i *Y3,
                                         __m256i *Y4,
                                         __m256i *Y5,
                                         __m256i *Y6,
                                         __m256i *Y7,
                                         dft_dir_t dir)
{
  const __m256i c = _mm256_set1_epi16(Q15_INV_SQRT2);

  const __m256i s04 = _mm256_adds_epi16(x0, x4);
  const __m256i d04 = _mm256_subs_epi16(x0, x4);

  const __m256i s15 = _mm256_adds_epi16(x1, x5);
  const __m256i d15 = _mm256_subs_epi16(x1, x5);

  const __m256i s26 = _mm256_adds_epi16(x2, x6);
  const __m256i d26 = _mm256_subs_epi16(x2, x6);

  const __m256i s37 = _mm256_adds_epi16(x3, x7);
  const __m256i d37 = _mm256_subs_epi16(x3, x7);

  const __m256i s02 = _mm256_adds_epi16(s04, s26);
  const __m256i d02 = _mm256_subs_epi16(s04, s26);

  const __m256i s13 = _mm256_adds_epi16(s15, s37);
  const __m256i d13 = _mm256_subs_epi16(s15, s37);

  *Y0 = _mm256_adds_epi16(s02, s13);
  *Y4 = _mm256_subs_epi16(s02, s13);

  *Y2 = _mm256_adds_epi16(d02, mul_minus_j_dir_i16_256(d13, dir));
  *Y6 = _mm256_adds_epi16(d02, mul_plus_j_dir_i16_256(d13, dir));

  const __m256i p = _mm256_adds_epi16(d15, d37);
  const __m256i q = _mm256_subs_epi16(d15, d37);

  const __m256i d26_mj = mul_minus_j_dir_i16_256(d26, dir);
  const __m256i d26_pj = mul_plus_j_dir_i16_256(d26, dir);

  const __m256i base_mj = _mm256_adds_epi16(d04, d26_mj);
  const __m256i base_pj = _mm256_adds_epi16(d04, d26_pj);

  const __m256i t1_arg = _mm256_adds_epi16(q, mul_minus_j_dir_i16_256(p, dir));
  const __m256i t3_arg = _mm256_adds_epi16(q, mul_plus_j_dir_i16_256(p, dir));

  const __m256i t1 = _mm256_mulhrs_epi16(c, t1_arg);
  const __m256i t3 = _mm256_mulhrs_epi16(c, t3_arg);

  *Y1 = _mm256_adds_epi16(base_mj, t1);
  *Y5 = _mm256_subs_epi16(base_mj, t1);

  *Y7 = _mm256_adds_epi16(base_pj, t3);
  *Y3 = _mm256_subs_epi16(base_pj, t3);
}

static inline void dft64ltslts(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  const __m256i x0 = _mm256_loadu_si256((const __m256i *)(const void *)(src + 0));
  const __m256i x1 = _mm256_loadu_si256((const __m256i *)(const void *)(src + 8));
  const __m256i x2 = _mm256_loadu_si256((const __m256i *)(const void *)(src + 16));
  const __m256i x3 = _mm256_loadu_si256((const __m256i *)(const void *)(src + 24));
  const __m256i x4 = _mm256_loadu_si256((const __m256i *)(const void *)(src + 32));
  const __m256i x5 = _mm256_loadu_si256((const __m256i *)(const void *)(src + 40));
  const __m256i x6 = _mm256_loadu_si256((const __m256i *)(const void *)(src + 48));
  const __m256i x7 = _mm256_loadu_si256((const __m256i *)(const void *)(src + 56));

  __m256i H0, H1, H2, H3;
  __m256i H4, H5, H6, H7;

  dft8x8lts_q15_256_dir(x0, x1, x2, x3, x4, x5, x6, x7, &H0, &H1, &H2, &H3, &H4, &H5, &H6, &H7, dir);
  const __m256i *C64_RE = (dir == DFT_DIR_FORWARD) ? g_dft64f_tw.C64_RE_RE_q15_256 : g_dft64f_tw.C64_RE_RE_q15_256_inverse;

  const __m256i *C64_IM = (dir == DFT_DIR_FORWARD) ? g_dft64f_tw.C64_IM_SIGNED_q15_256 : g_dft64f_tw.C64_IM_SIGNED_q15_256_inverse;
  H0 = _mm256_srai_epi16(H0, 3);

  H1 = complex_mul8_prepack_q15_256(H1, C64_RE[1], C64_IM[1]);
  H2 = complex_mul8_prepack_q15_256(H2, C64_RE[2], C64_IM[2]);
  H3 = complex_mul8_prepack_q15_256(H3, C64_RE[3], C64_IM[3]);
  H4 = complex_mul8_prepack_q15_256(H4, C64_RE[4], C64_IM[4]);
  H5 = complex_mul8_prepack_q15_256(H5, C64_RE[5], C64_IM[5]);
  H6 = complex_mul8_prepack_q15_256(H6, C64_RE[6], C64_IM[6]);
  H7 = complex_mul8_prepack_q15_256(H7, C64_RE[7], C64_IM[7]);

  /*
   * Transpose complex 8x8.
   */
  transpose8_complex_i16_256(&H0, &H1, &H2, &H3, &H4, &H5, &H6, &H7);

  __m256i Y0, Y1, Y2, Y3;
  __m256i Y4, Y5, Y6, Y7;

  /*
   * Second stage.
   */
  dft8x8lts_q15_256_dir(H0, H1, H2, H3, H4, H5, H6, H7, &Y0, &Y1, &Y2, &Y3, &Y4, &Y5, &Y6, &Y7, dir);
  _mm256_storeu_si256((__m256i *)(void *)(dst + 0), Y0);
  _mm256_storeu_si256((__m256i *)(void *)(dst + 8), Y1);
  _mm256_storeu_si256((__m256i *)(void *)(dst + 16), Y2);
  _mm256_storeu_si256((__m256i *)(void *)(dst + 24), Y3);
  _mm256_storeu_si256((__m256i *)(void *)(dst + 32), Y4);
  _mm256_storeu_si256((__m256i *)(void *)(dst + 40), Y5);
  _mm256_storeu_si256((__m256i *)(void *)(dst + 48), Y6);
  _mm256_storeu_si256((__m256i *)(void *)(dst + 56), Y7);
}

//===================================================================
// DFT128 int
//===================================================================
static inline __m256i scale_q15_inv_sqrt2_256(__m256i x)
{
  const __m256i s = _mm256_set1_epi16(Q15_INV_SQRT2);
  return _mm256_mulhrs_epi16(x, s);
}

static inline void dft128_stage0_blk_q15_256_dir(const c16_t *src, c16_t *a, c16_t *b, int blk, dft_dir_t dir)
{
  const __m256i x0 = _mm256_loadu_si256((const __m256i *)(const void *)(src + 8 * blk));

  const __m256i x1 = _mm256_loadu_si256((const __m256i *)(const void *)(src + 64 + 8 * blk));

  __m256i sum = _mm256_adds_epi16(x0, x1);
  __m256i diff = _mm256_subs_epi16(x0, x1);

  sum = scale_q15_inv_sqrt2_256(sum);

  const __m256i *W128_RE = (dir == DFT_DIR_FORWARD) ? g_dft64f_tw.W128_RE_RE_q15_256 : g_dft64f_tw.W128_RE_RE_q15_256_inverse;

  const __m256i *W128_IM =
      (dir == DFT_DIR_FORWARD) ? g_dft64f_tw.W128_IM_SIGNED_q15_256 : g_dft64f_tw.W128_IM_SIGNED_q15_256_inverse;

  diff = complex_mul8_prepack_q15_256(diff, W128_RE[blk], W128_IM[blk]);

  _mm256_store_si256((__m256i *)(void *)(a + 8 * blk), sum);

  _mm256_store_si256((__m256i *)(void *)(b + 8 * blk), diff);
}

static inline void interleave64_complex_q15_256(const c16_t *A, const c16_t *B, c16_t *dst)
{
  for (int blk = 0; blk < 8; blk++) {
    const __m256i va = _mm256_load_si256((const __m256i *)(const void *)(A + 8 * blk));

    const __m256i vb = _mm256_load_si256((const __m256i *)(const void *)(B + 8 * blk));

    /*
     * va = [A0 A1 A2 A3 | A4 A5 A6 A7]
     * vb = [B0 B1 B2 B3 | B4 B5 B6 B7]
     *
     * Each A0/B0 is one c16_t = 32 bits.
     */
    const __m256i lo = _mm256_unpacklo_epi32(va, vb);
    const __m256i hi = _mm256_unpackhi_epi32(va, vb);

    /*
     * out0 = [A0 B0 A1 B1 A2 B2 A3 B3]
     * out1 = [A4 B4 A5 B5 A6 B6 A7 B7]
     */
    const __m256i out0 = _mm256_permute2x128_si256(lo, hi, 0x20);
    const __m256i out1 = _mm256_permute2x128_si256(lo, hi, 0x31);

    _mm256_storeu_si256((__m256i *)(void *)(dst + 16 * blk), out0);

    _mm256_storeu_si256((__m256i *)(void *)(dst + 16 * blk + 8), out1);
  }
}

static inline void dft128lts_dir(const c16_t *src, c16_t *dst, dft_dir_t dir)
{
  c16_t a[64] __attribute__((aligned(32)));
  c16_t b[64] __attribute__((aligned(32)));

  c16_t A[64] __attribute__((aligned(32)));
  c16_t B[64] __attribute__((aligned(32)));

  for (int blk = 0; blk < 8; blk++) {
    dft128_stage0_blk_q15_256_dir(src, a, b, blk, dir);
  }

  dft64ltslts(a, A, dir);
  dft64ltslts(b, B, dir);

  interleave64_complex_q15_256(A, B, dst);
}

//===================================================================
// DFT64 int
//===================================================================
static inline void dft4x4lts_q15(const __m128i x0,
                                 const __m128i x1,
                                 const __m128i x2,
                                 const __m128i x3,
                                 __m128i *Y0,
                                 __m128i *Y1,
                                 __m128i *Y2,
                                 __m128i *Y3)
{
  const __m128i s02 = _mm_add_epi16(x0, x2);
  const __m128i d02 = _mm_sub_epi16(x0, x2);

  const __m128i s13 = _mm_add_epi16(x1, x3);
  const __m128i d13 = _mm_sub_epi16(x1, x3);

  *Y0 = _mm_srai_epi16(_mm_add_epi16(s02, s13), 1);
  *Y2 = _mm_srai_epi16(_mm_sub_epi16(s02, s13), 1);
  *Y1 = _mm_srai_epi16(_mm_add_epi16(d02, mul_minuslts_q15_128(d13)), 1);
  *Y3 = _mm_srai_epi16(_mm_add_epi16(d02, mullts_q15_128(d13)), 1);
}

static inline void combine16lts_q15(__m128i H[4][4], __m128i G[16])
{
  for (int i = 0; i < 4; i++) {
    __m128i B[4];

    B[0] = H[0][i];

    if (i == 0) {
      B[1] = H[1][i];
      B[2] = H[2][i];
      B[3] = H[3][i];
    } else {
      B[1] = complex_mul4_prepack_q15_128(H[1][i], g_dft64f_tw.C16_BCAST_RE_RE_q15[i][1], g_dft64f_tw.C16_BCAST_IM_IM_q15[i][1]);

      B[2] = complex_mul4_prepack_q15_128(H[2][i], g_dft64f_tw.C16_BCAST_RE_RE_q15[i][2], g_dft64f_tw.C16_BCAST_IM_IM_q15[i][2]);

      B[3] = complex_mul4_prepack_q15_128(H[3][i], g_dft64f_tw.C16_BCAST_RE_RE_q15[i][3], g_dft64f_tw.C16_BCAST_IM_IM_q15[i][3]);
    }

    dft4x4lts_q15(B[0], B[1], B[2], B[3], G + i, G + i + 4, G + i + 8, G + i + 12);
  }
}

static inline void dft16x4lts_q15(const c16_t *src, __m128i G[16])
{
  __m128i H[4][4] __attribute__((aligned(64)));
  const __m128i *tmp = (const __m128i *)src;
  for (int i = 0; i < 4; i++)
    dft4x4lts_q15(tmp[i], tmp[i + 4], tmp[i + 8], tmp[i + 12], H[i], &H[i][1], H[i] + 2, H[i] + 3);
  combine16lts_q15(H, G);
}

static inline void dft4x4lts_dst_q15(const __m128i x0, const __m128i x1, const __m128i x2, const __m128i x3, c16_t *dst)
{
  const __m128i s02 = _mm_add_epi16(x0, x2);
  const __m128i d02 = _mm_sub_epi16(x0, x2);

  const __m128i s13 = _mm_add_epi16(x1, x3);
  const __m128i d13 = _mm_sub_epi16(x1, x3);

  __m128i *tmp = (__m128i *)dst;
  tmp[0] = _mm_srai_epi16(_mm_add_epi16(s02, s13), 1);
  tmp[4] = _mm_srai_epi16(_mm_add_epi16(d02, mul_minuslts_q15_128(d13)), 1);
  tmp[8] = _mm_srai_epi16(_mm_sub_epi16(s02, s13), 1);
  tmp[12] = _mm_srai_epi16(_mm_add_epi16(d02, mullts_q15_128(d13)), 1);
}

static inline void transpose4_complex_epi16(__m128i *Y0, __m128i *Y1, __m128i *Y2, __m128i *Y3)
{
  __m128i a = *Y0;
  __m128i b = *Y1;
  __m128i c = *Y2;
  __m128i d = *Y3;

  __m128i ab_lo = _mm_unpacklo_epi32(a, b); // [a0 b0 a1 b1]
  __m128i ab_hi = _mm_unpackhi_epi32(a, b); // [a2 b2 a3 b3]

  __m128i cd_lo = _mm_unpacklo_epi32(c, d); // [c0 d0 c1 d1]
  __m128i cd_hi = _mm_unpackhi_epi32(c, d); // [c2 d2 c3 d3]

  *Y0 = _mm_unpacklo_epi64(ab_lo, cd_lo); // [a0 b0 c0 d0]
  *Y1 = _mm_unpackhi_epi64(ab_lo, cd_lo); // [a1 b1 c1 d1]
  *Y2 = _mm_unpacklo_epi64(ab_hi, cd_hi); // [a2 b2 c2 d2]
  *Y3 = _mm_unpackhi_epi64(ab_hi, cd_hi); // [a3 b3 c3 d3]
}

static inline void combine64lts_lts_q15(const __m128i G[16], c16_t *dst)
{
  for (int i = 0; i < 16; i += 4) {
    __m128i B0;
    if (i == 0) {
      B0 = G[0];
    } else {
      B0 = complex_mul4_prepack_q15_128(G[i + 0], g_dft64f_tw.C64_RE_RE_q15[i + 0], g_dft64f_tw.C64_IM_IM_q15[i + 0]);
    }

    __m128i B1 = complex_mul4_prepack_q15_128(G[i + 1], g_dft64f_tw.C64_RE_RE_q15[i + 1], g_dft64f_tw.C64_IM_IM_q15[i + 1]);

    __m128i B2 = complex_mul4_prepack_q15_128(G[i + 2], g_dft64f_tw.C64_RE_RE_q15[i + 2], g_dft64f_tw.C64_IM_IM_q15[i + 2]);

    __m128i B3 = complex_mul4_prepack_q15_128(G[i + 3], g_dft64f_tw.C64_RE_RE_q15[i + 3], g_dft64f_tw.C64_IM_IM_q15[i + 3]);
    transpose4_complex_epi16(&B0, &B1, &B2, &B3);

    dft4x4lts_dst_q15(B0, B1, B2, B3, dst + i);
  }
}

static void dft64lts_q15(const c16_t *src, c16_t *dst)
{
  __m128i G[16] __attribute__((aligned(64)));
  dft16x4lts_q15(src, G);
  combine64lts_lts_q15(G, dst);
}

static void dft_split_radix_pure_simd_core(c16_t *__restrict x, c16_t *__restrict y, c16_t *__restrict work, int N, dft_dir_t dir)
{
  if (N == 64) {
    dft64ltslts(x, y, dir);
    return;
  }

  if (N == 128) {
    dft128lts_dir(x, y, dir);
    return;
  }

  const int half = N >> 1;
  const int quarter = N >> 2;

  c16_t *sub_in = work;
  c16_t *sub_out = work + N;

  c16_t *E = sub_out;
  c16_t *O1 = sub_out + half;
  c16_t *O3 = sub_out + half + quarter;

  pack_split_radix_input_avx2_fused(x, sub_in, N);

  dft_split_radix_pure_simd_core(sub_in, E, work + 2 * N, half, dir);

  dft_split_radix_pure_simd_core(sub_in + half, O1, work + 2 * N, quarter, dir);

  dft_split_radix_pure_simd_core(sub_in + half + quarter, O3, work + 2 * N, quarter, dir);
  const sr_twiddle_simd_t *table = (dir == DFT_DIR_FORWARD) ? sr_twiddles_fwd : sr_twiddles_bwd;

  sr_combine_simd(E, O1, O3, y, N, &table[log2_int(N)], dir);
}

static void dft_split_radix_pure_simd(c16_t *x, c16_t *y, int N, dft_dir_t dir)
{
  c16_t work[262144] __attribute__((aligned(64)));

  if (!work) {
    printf("work allocation failed\n");
    return;
  }

  dft_split_radix_pure_simd_core(x, y, work, N, dir);
}
//===================================================================
// DFT32 float
//===================================================================

static inline __m256 complex_mul4_bcast(const __m256 a, const float wr, const float wi)
{
  const __m256 w_re = _mm256_set1_ps(wr);
  const __m256 w_im = _mm256_set1_ps(wi);

  const __m256 a_swapped = _mm256_shuffle_ps(a, a, 0xB1);
  const __m256 prod_im = _mm256_mul_ps(a_swapped, w_im);

  return _mm256_fmaddsub_ps(a, w_re, prod_im);
}

static inline void dft8x4lts(const __m256 x0,
                             const __m256 x1,
                             const __m256 x2,
                             const __m256 x3,
                             const __m256 x4,
                             const __m256 x5,
                             const __m256 x6,
                             const __m256 x7,
                             __m256 *Y0,
                             __m256 *Y1,
                             __m256 *Y2,
                             __m256 *Y3,
                             __m256 *Y4,
                             __m256 *Y5,
                             __m256 *Y6,
                             __m256 *Y7)
{
  __m256 E0, E1, E2, E3;
  __m256 O0, O1, O2, O3;

  dft4x4lts(x0, x2, x4, x6, &E0, &E1, &E2, &E3);
  dft4x4lts(x1, x3, x5, x7, &O0, &O1, &O2, &O3);

  const float c = 0.70710678118654752440f;

  const __m256 T0 = O0;
  const __m256 T1 = complex_mul4_bcast(O1, c, -c);
  const __m256 T2 = complex_mul4_bcast(O2, 0.0f, -1.0f);
  const __m256 T3 = complex_mul4_bcast(O3, -c, -c);

  *Y0 = _mm256_add_ps(E0, T0);
  *Y4 = _mm256_sub_ps(E0, T0);

  *Y1 = _mm256_add_ps(E1, T1);
  *Y5 = _mm256_sub_ps(E1, T1);

  *Y2 = _mm256_add_ps(E2, T2);
  *Y6 = _mm256_sub_ps(E2, T2);

  *Y3 = _mm256_add_ps(E3, T3);
  *Y7 = _mm256_sub_ps(E3, T3);
}

static inline void combine32lts_avx2(const __m256 H_lo[4], const __m256 H_hi[4], float complex *dst)
{
  const __m256 A0_lo = H_lo[0];
  const __m256 A0_hi = H_hi[0];

  const __m256 A1_lo =
      complex_mul4_prepack(H_lo[1], g_dft64f_tw.C32_RE_RE_forward256[1][0], g_dft64f_tw.C32_IM_IM_forward256[1][0]);

  const __m256 A1_hi =
      complex_mul4_prepack(H_hi[1], g_dft64f_tw.C32_RE_RE_forward256[1][1], g_dft64f_tw.C32_IM_IM_forward256[1][1]);

  const __m256 A2_lo =
      complex_mul4_prepack(H_lo[2], g_dft64f_tw.C32_RE_RE_forward256[2][0], g_dft64f_tw.C32_IM_IM_forward256[2][0]);

  const __m256 A2_hi =
      complex_mul4_prepack(H_hi[2], g_dft64f_tw.C32_RE_RE_forward256[2][1], g_dft64f_tw.C32_IM_IM_forward256[2][1]);

  const __m256 A3_lo =
      complex_mul4_prepack(H_lo[3], g_dft64f_tw.C32_RE_RE_forward256[3][0], g_dft64f_tw.C32_IM_IM_forward256[3][0]);

  const __m256 A3_hi =
      complex_mul4_prepack(H_hi[3], g_dft64f_tw.C32_RE_RE_forward256[3][1], g_dft64f_tw.C32_IM_IM_forward256[3][1]);

  __m256 lo0 = A0_lo;
  __m256 lo1 = A1_lo;
  __m256 lo2 = A2_lo;
  __m256 lo3 = A3_lo;

  __m256 hi0 = A0_hi;
  __m256 hi1 = A1_hi;
  __m256 hi2 = A2_hi;
  __m256 hi3 = A3_hi;

  transpose4_complex_shuffle_ps(&lo0, &lo1, &lo2, &lo3);
  transpose4_complex_shuffle_ps(&hi0, &hi1, &hi2, &hi3);

  __m256 Y0, Y1, Y2, Y3;
  __m256 Y4, Y5, Y6, Y7;

  dft8x4lts(lo0, lo1, lo2, lo3, hi0, hi1, hi2, hi3, &Y0, &Y1, &Y2, &Y3, &Y4, &Y5, &Y6, &Y7);

  _mm256_store_ps((float *)(dst + 0), Y0);
  _mm256_store_ps((float *)(dst + 4), Y1);
  _mm256_store_ps((float *)(dst + 8), Y2);
  _mm256_store_ps((float *)(dst + 12), Y3);

  _mm256_store_ps((float *)(dst + 16), Y4);
  _mm256_store_ps((float *)(dst + 20), Y5);
  _mm256_store_ps((float *)(dst + 24), Y6);
  _mm256_store_ps((float *)(dst + 28), Y7);
}

static void dft32lts_avx2(const float complex *src, float complex *dst)
{
  __m256 H_lo[4] __attribute__((aligned(64)));
  __m256 H_hi[4] __attribute__((aligned(64)));

  const __m256 x0_lo = _mm256_load_ps((const float *)(src + 0));
  const __m256 x0_hi = _mm256_load_ps((const float *)(src + 4));

  const __m256 x1_lo = _mm256_load_ps((const float *)(src + 8));
  const __m256 x1_hi = _mm256_load_ps((const float *)(src + 12));

  const __m256 x2_lo = _mm256_load_ps((const float *)(src + 16));
  const __m256 x2_hi = _mm256_load_ps((const float *)(src + 20));

  const __m256 x3_lo = _mm256_load_ps((const float *)(src + 24));
  const __m256 x3_hi = _mm256_load_ps((const float *)(src + 28));

  dft4x4lts(x0_lo, x1_lo, x2_lo, x3_lo, &H_lo[0], &H_lo[1], &H_lo[2], &H_lo[3]);

  dft4x4lts(x0_hi, x1_hi, x2_hi, x3_hi, &H_hi[0], &H_hi[1], &H_hi[2], &H_hi[3]);

  combine32lts_avx2(H_lo, H_hi, dst);
}

//======================================================
// DFT16 float
//======================================================
static inline void combine16lts_avx2(const __m256 H[4], float complex *dst)
{
  const __m256 A0 = H[0];

  const __m256 A1 = complex_mul4_prepack(H[1], g_dft64f_tw.C16_RE_RE_forward[1], g_dft64f_tw.C16_IM_IM_forward[1]);

  const __m256 A2 = complex_mul4_prepack(H[2], g_dft64f_tw.C16_RE_RE_forward[2], g_dft64f_tw.C16_IM_IM_forward[2]);

  const __m256 A3 = complex_mul4_prepack(H[3], g_dft64f_tw.C16_RE_RE_forward[3], g_dft64f_tw.C16_IM_IM_forward[3]);

  __m256 t0 = A0;
  __m256 t1 = A1;
  __m256 t2 = A2;
  __m256 t3 = A3;

  transpose4_complex_shuffle_ps(&t0, &t1, &t2, &t3);

  __m256 Y0, Y1, Y2, Y3;

  dft4x4lts(t0, t1, t2, t3, &Y0, &Y1, &Y2, &Y3);

  _mm256_storeu_ps((float *)(dst + 0), Y0);
  _mm256_storeu_ps((float *)(dst + 4), Y1);
  _mm256_storeu_ps((float *)(dst + 8), Y2);
  _mm256_storeu_ps((float *)(dst + 12), Y3);
}

static inline void dft16lts_avx2(const float complex *src, float complex *dst)
{
  __m256 H[4] __attribute__((aligned(32)));

  const __m256 x0 = _mm256_loadu_ps((const float *)(src + 0));
  const __m256 x1 = _mm256_loadu_ps((const float *)(src + 4));
  const __m256 x2 = _mm256_loadu_ps((const float *)(src + 8));
  const __m256 x3 = _mm256_loadu_ps((const float *)(src + 12));

  dft4x4lts(x0, x1, x2, x3, &H[0], &H[1], &H[2], &H[3]);

  combine16lts_avx2(H, dst);
}

//======================================================
// DFT8 float
//======================================================

static inline __m128 mul2lts(__m128 z)
{
  const __m128 swapped = _mm_permute_ps(z, 0xB1);
  const __m128 sign = _mm_setr_ps(-0.0f, 0.0f, -0.0f, 0.0f);
  return _mm_xor_ps(swapped, sign);
}

static inline __m128 mul2_minuslts(__m128 z)
{
  const __m128 swapped = _mm_permute_ps(z, 0xB1);
  const __m128 sign = _mm_setr_ps(0.0f, -0.0f, 0.0f, -0.0f);
  return _mm_xor_ps(swapped, sign);
}

static inline __m128 complex_mul2_prepack(const __m128 a, const __m128 w_re_re, const __m128 w_im_im)
{
  __m128 a_swapped = _mm_shuffle_ps(a, a, 0xB1);
  __m128 prod_im = _mm_mul_ps(a_swapped, w_im_im);
  return _mm_fmaddsub_ps(a, w_re_re, prod_im);
}

static inline __m128 complex_mul2_bcast(const __m128 a, const float wr, const float wi)
{
  const __m128 w_re = _mm_set1_ps(wr);
  const __m128 w_im = _mm_set1_ps(wi);

  const __m128 a_swapped = _mm_shuffle_ps(a, a, 0xB1);
  const __m128 prod_im = _mm_mul_ps(a_swapped, w_im);

  return _mm_fmaddsub_ps(a, w_re, prod_im);
}
static inline void
dft4x2lts(const __m128 x0, const __m128 x1, const __m128 x2, const __m128 x3, __m128 *Y0, __m128 *Y1, __m128 *Y2, __m128 *Y3)
{
  const __m128 s02 = _mm_add_ps(x0, x2);
  const __m128 d02 = _mm_sub_ps(x0, x2);

  const __m128 s13 = _mm_add_ps(x1, x3);
  const __m128 d13 = _mm_sub_ps(x1, x3);

  *Y0 = _mm_add_ps(s02, s13);
  *Y2 = _mm_sub_ps(s02, s13);
  *Y1 = _mm_add_ps(d02, mul2_minuslts(d13));
  *Y3 = _mm_add_ps(d02, mul2lts(d13));
}

static inline void dft8lts(const float complex *src, float complex *dst)
{
  __m128 H_lo[2] __attribute__((aligned(64)));
  __m128 H_hi[2] __attribute__((aligned(64)));

  const __m128 x0 = _mm_load_ps((const float *)(src + 0));
  const __m128 x1 = _mm_load_ps((const float *)(src + 2));

  const __m128 x2 = _mm_load_ps((const float *)(src + 4));
  const __m128 x3 = _mm_load_ps((const float *)(src + 6));

  __m128 P0, P1, P2, P3;

  dft4x2lts(x0, x1, x2, x3, &P0, &P1, &P2, &P3);

  /*
   * Repack :
   *
   * E01 = [E0, E1]
   * O01 = [O0, O1]
   * E23 = [E2, E3]
   * O23 = [O2, O3]
   */
  const __m128 E01 = _mm_castpd_ps(_mm_unpacklo_pd(_mm_castps_pd(P0), _mm_castps_pd(P1)));

  const __m128 O01 = _mm_castpd_ps(_mm_unpackhi_pd(_mm_castps_pd(P0), _mm_castps_pd(P1)));

  const __m128 E23 = _mm_castpd_ps(_mm_unpacklo_pd(_mm_castps_pd(P2), _mm_castps_pd(P3)));

  const __m128 O23 = _mm_castpd_ps(_mm_unpackhi_pd(_mm_castps_pd(P2), _mm_castps_pd(P3)));

  const float c = 0.70710678118654752440f;

  /*
   * Twiddles W8 :
   *
   * W8^0 =  1 + j0
   * W8^1 =  c - jc
   * W8^2 =  0 - j1
   * W8^3 = -c - jc
   */
  const __m128 W01_re = _mm_setr_ps(1.0f, 1.0f, c, c);
  const __m128 W01_im = _mm_setr_ps(0.0f, 0.0f, -c, -c);

  const __m128 W23_re = _mm_setr_ps(0.0f, 0.0f, -c, -c);
  const __m128 W23_im = _mm_setr_ps(-1.0f, -1.0f, -c, -c);

  const __m128 T01 = complex_mul2_prepack(O01, W01_re, W01_im);
  const __m128 T23 = complex_mul2_prepack(O23, W23_re, W23_im);

  /*
   * Radix-2 combine :
   *
   * Y[k]     = E[k] + W8^k * O[k]
   * Y[k + 4] = E[k] - W8^k * O[k]
   */
  const __m128 Y01 = _mm_add_ps(E01, T01); // Y0, Y1
  const __m128 Y45 = _mm_sub_ps(E01, T01); // Y4, Y5

  const __m128 Y23 = _mm_add_ps(E23, T23); // Y2, Y3
  const __m128 Y67 = _mm_sub_ps(E23, T23); // Y6, Y7

  /*
   * Store 8 complexes only.
   */
  _mm_storeu_ps((float *)(dst + 0), Y01); // dst[0], dst[1]
  _mm_storeu_ps((float *)(dst + 2), Y23); // dst[2], dst[3]
  _mm_storeu_ps((float *)(dst + 4), Y45); // dst[4], dst[5]
  _mm_storeu_ps((float *)(dst + 6), Y67); // dst[6], dst[7]
}

//======================================================
// DFT12 float
//======================================================

#define ALIGNMENT 32

static void *aligned_malloc(size_t size)
{
  void *ptr = NULL;

  if (posix_memalign(&ptr, ALIGNMENT, size) != 0)
    return NULL;

  return ptr;
}

static void twiddle_table_destroy(TwiddleTable *table)
{
  if (!table)
    return;

  free(table->forward);
  free(table->inverse);

  free(table->r2_w_re);
  free(table->r2_w_im);

  free(table->r3_w1_re);
  free(table->r3_w1_im);
  free(table->r3_w2_re);
  free(table->r3_w2_im);

  free(table->r3_q15_w1_re);
  free(table->r3_q15_w1_im);
  free(table->r3_q15_w2_re);
  free(table->r3_q15_w2_im);

  free(table->r4_w1_re);
  free(table->r4_w1_im);
  free(table->r4_w2_re);
  free(table->r4_w2_im);
  free(table->r4_w3_re);
  free(table->r4_w3_im);

  free(table->r5_w1_re);
  free(table->r5_w1_im);
  free(table->r5_w2_re);
  free(table->r5_w2_im);
  free(table->r5_w3_re);
  free(table->r5_w3_im);
  free(table->r5_w4_re);
  free(table->r5_w4_im);

  memset(table, 0, sizeof(*table));
}
static TwiddleTable *twiddle_table_create(int N)
{
  if (N <= 0 || N > MAX_N) {
    fprintf(stderr, "twiddle_table_create: invalid N=%d, MAX_N=%d\n", N, MAX_N);
    abort();
  }

  TwiddleTable *table = &g_tables[N];

  memset(table, 0, sizeof(*table));

  table->N = N;

  table->forward = aligned_malloc((size_t)N * sizeof(*table->forward));
  table->inverse = aligned_malloc((size_t)N * sizeof(*table->inverse));

  if (!table->forward || !table->inverse) {
    fprintf(stderr, "twiddle_table_create: allocation failed for N=%d\n", N);
    twiddle_table_destroy(table);
    return NULL;
  }

  for (int k = 0; k < N; k++) {
    float theta = 2.0f * (float)M_PI * (float)k / (float)N;

    float c = cosf(theta);
    float s = sinf(theta);

    table->forward[k] = c - I * s;
    table->inverse[k] = c + I * s;
  }

  if (N % 2 == 0) {
    if (!twiddle_table_create_radix2_simd(table)) {
      twiddle_table_destroy(table);
      return NULL;
    }
  }

  if (N % 3 == 0) {
    if (!twiddle_table_create_radix3_simd(table)) {
      twiddle_table_destroy(table);
      return NULL;
    }
    if (!twiddle_table_create_radix3_q15_simd(table)) {
      twiddle_table_destroy(table);
      return NULL;
    }
  }

  if (N % 4 == 0) {
    if (!twiddle_table_create_radix4_simd(table)) {
      twiddle_table_destroy(table);
      return NULL;
    }
  }

  if (N % 5 == 0) {
    if (!twiddle_table_create_radix5_simd(table)) {
      twiddle_table_destroy(table);
      return NULL;
    }
  }

  table->initialized = 1;
  return table;
}
const TwiddleTable *twiddle_table_get(int N)
{
  if (N <= 0 || N > MAX_N) {
    fprintf(stderr, "twiddle_table_get: invalid N=%d, MAX_N=%d\n", N, MAX_N);
    abort();
  }

  if (!g_tables[N].initialized) {
    if (!twiddle_table_create(N)) {
      return NULL;
    }
  }

  return &g_tables[N];
}
static inline __m256 pack3_complex_plus_zero(const float complex a, const float complex b, const float complex c)
{
  return _mm256_setr_ps(crealf(a), cimagf(a), crealf(b), cimagf(b), crealf(c), cimagf(c), 0.0f, 0.0f);
}

static inline void dft12lts_avx2(const float complex *src, float complex *dst)
{
  /*
   * Lane 0 : src[0], src[3], src[6], src[9]
   * Lane 1 : src[1], src[4], src[7], src[10]
   * Lane 2 : src[2], src[5], src[8], src[11]
   * Lane 3 : dummy
   */
  const __m256 x0 = pack3_complex_plus_zero(src[0], src[1], src[2]);
  const __m256 x1 = pack3_complex_plus_zero(src[3], src[4], src[5]);
  const __m256 x2 = pack3_complex_plus_zero(src[6], src[7], src[8]);
  const __m256 x3 = pack3_complex_plus_zero(src[9], src[10], src[11]);

  __m256 H0, H1, H2, H3;

  /*
   * H0 = [F0[0], F1[0], F2[0], dummy]
   * H1 = [F0[1], F1[1], F2[1], dummy]
   * H2 = [F0[2], F1[2], F2[2], dummy]
   * H3 = [F0[3], F1[3], F2[3], dummy]
   */
  dft4x4lts(x0, x1, x2, x3, &H0, &H1, &H2, &H3);

  /*
   * A  = [F0[0], F0[1], F0[2], F0[3]]
   * X1 = [F1[0], F1[1], F1[2], F1[3]]
   * X2 = [F2[0], F2[1], F2[2], F2[3]]
   */
  transpose4_complex_shuffle_ps(&H0, &H1, &H2, &H3);

  const __m256 A = H0;
  const __m256 X1 = H1;
  const __m256 X2 = H2;

  const TwiddleTable *tw = twiddle_table_get(12);
  if (!tw) {
    return;
  }

  /*
   * B[k] = W12^k    * X1[k]
   * C[k] = W12^(2k) * X2[k]
   */
  const __m256 B = complex_mul4_prepack(X1, tw->r3_w1_re[0], tw->r3_w1_im[0]);

  const __m256 C = complex_mul4_prepack(X2, tw->r3_w2_re[0], tw->r3_w2_im[0]);

  const __m256 S = _mm256_add_ps(B, C);
  const __m256 D = _mm256_sub_ps(B, C);

  const __m256 Y0 = _mm256_add_ps(A, S);

  const __m256 half = _mm256_set1_ps(0.5f);
  const __m256 c3v = _mm256_set1_ps(0.86602540378443864676f);

  const __m256 base = _mm256_sub_ps(A, _mm256_mul_ps(half, S));
  const __m256 c3D = _mm256_mul_ps(c3v, D);

  const __m256 Y1 = _mm256_add_ps(base, mul_minuslts(c3D));
  const __m256 Y2 = _mm256_add_ps(base, mullts(c3D));

  /*
   * size = 4
   *
   * dst[0..3]  = Y0
   * dst[4..7]  = Y1
   * dst[8..11] = Y2
   */
  _mm256_storeu_ps((float *)(dst + 0), Y0);
  _mm256_storeu_ps((float *)(dst + 4), Y1);
  _mm256_storeu_ps((float *)(dst + 8), Y2);
}

//======================================================
// Split float
//======================================================
static inline void sr_combine_f32_prepack(float complex *__restrict E,
                                          float complex *__restrict O1,
                                          float complex *__restrict O3,
                                          float complex *__restrict y,
                                          int N,
                                          const sr_twiddle_f32_prepack_t *tw)
{
  const int half = N / 2;
  const int quarter = N / 4;

  for (int b = 0; b < tw->blocks; b++) {
    int k = 4 * b;

    __m256 O1v = _mm256_loadu_ps((const float *)(const void *)&O1[k]);
    __m256 O3v = _mm256_loadu_ps((const float *)(const void *)&O3[k]);

    /*
     * t1 = W_N^k  * O1[k]
     * t2 = W_N^3k * O3[k]
     */
    __m256 t1 = complex_mul4_prepack(O1v, tw->W1_RE_RE[b], tw->W1_IM_IM[b]);

    __m256 t2 = complex_mul4_prepack(O3v, tw->W3_RE_RE[b], tw->W3_IM_IM[b]);

    __m256 a = _mm256_add_ps(t1, t2);
    __m256 d = _mm256_sub_ps(t1, t2);

    /*
     * bval = -i * (t1 - t2)
     */

    __m256 E0 = _mm256_loadu_ps((const float *)(const void *)&E[k]);
    __m256 E1 = _mm256_loadu_ps((const float *)(const void *)&E[k + quarter]);

    __m256 Y0 = _mm256_add_ps(E0, a);
    __m256 Y2 = _mm256_sub_ps(E0, a);

    __m256 Y1 = _mm256_add_ps(E1, mul_minuslts(d));
    __m256 Y3 = _mm256_add_ps(E1, mullts(d));

    _mm256_storeu_ps((float *)(void *)&y[k], Y0);
    _mm256_storeu_ps((float *)(void *)&y[k + quarter], Y1);
    _mm256_storeu_ps((float *)(void *)&y[k + half], Y2);
    _mm256_storeu_ps((float *)(void *)&y[k + 3 * quarter], Y3);
  }
}

static inline void sr_combine_simdlts(float complex *E,
                                      float complex *O1,
                                      float complex *O3,
                                      float complex *y,
                                      int N,
                                      const sr_twiddle_simd_t *tw)
{
  int half = N / 2;
  int quarter = N / 4;

  const simde__m256i swap_mask = simde_mm256_setr_epi8(2,
                                                       3,
                                                       0,
                                                       1,
                                                       6,
                                                       7,
                                                       4,
                                                       5,
                                                       10,
                                                       11,
                                                       8,
                                                       9,
                                                       14,
                                                       15,
                                                       12,
                                                       13,
                                                       2,
                                                       3,
                                                       0,
                                                       1,
                                                       6,
                                                       7,
                                                       4,
                                                       5,
                                                       10,
                                                       11,
                                                       8,
                                                       9,
                                                       14,
                                                       15,
                                                       12,
                                                       13);

  const simde__m256i sign_mask = simde_mm256_setr_epi16(1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1);

  const simde__m256i sqrt2_inv = simde_mm256_set1_epi16(ONE_OVER_SQRT2_Q15);

  for (int b = 0; b < tw->blocks; b++) {
    int k = 8 * b;

    simde__m256i O1v = simde_mm256_load_si256((simde__m256i *)&O1[k]);
    simde__m256i O3v = simde_mm256_load_si256((simde__m256i *)&O3[k]);

    simde__m256i t1 = c16_mul_q15_simd256(O1v, tw->W1_RE_NEGIM[b], tw->W1_IM_RE[b]);

    simde__m256i t2 = c16_mul_q15_simd256(O3v, tw->W3_RE_NEGIM[b], tw->W3_IM_RE[b]);

    simde__m256i a = simde_mm256_adds_epi16(t1, t2);
    simde__m256i d = simde_mm256_subs_epi16(t1, t2);

    simde__m256i bval = simde_mm256_shuffle_epi8(d, swap_mask);
    bval = simde_mm256_sign_epi16(bval, sign_mask);

    simde__m256i E0 = simde_mm256_load_si256((simde__m256i *)&E[k]);
    simde__m256i E1 = simde_mm256_load_si256((simde__m256i *)&E[k + quarter]);

    E0 = simde_mm256_mulhrs_epi16(E0, sqrt2_inv);
    E1 = simde_mm256_mulhrs_epi16(E1, sqrt2_inv);

    a = simde_mm256_srai_epi16(a, 1);
    bval = simde_mm256_srai_epi16(bval, 1);

    simde__m256i Y0 = simde_mm256_adds_epi16(E0, a);
    simde__m256i Y2 = simde_mm256_subs_epi16(E0, a);
    simde__m256i Y1 = simde_mm256_adds_epi16(E1, bval);
    simde__m256i Y3 = simde_mm256_subs_epi16(E1, bval);

    simde_mm256_store_si256((simde__m256i *)&y[k], Y0);
    simde_mm256_store_si256((simde__m256i *)&y[k + quarter], Y1);
    simde_mm256_store_si256((simde__m256i *)&y[k + half], Y2);
    simde_mm256_store_si256((simde__m256i *)&y[k + 3 * quarter], Y3);
  }
}

static inline void pack_split_radix_input_f32_avx2_fused(const float complex *__restrict x, float complex *__restrict sub_in, int N)
{
  _Static_assert(sizeof(float complex) == 8, "float complex must be 64-bit");

  const int half = N >> 1;
  const int quarter = N >> 2;

  float complex *__restrict E_in = sub_in;
  float complex *__restrict O1_in = sub_in + half;
  float complex *__restrict O3_in = sub_in + half + quarter;

  const int perm_even_odd = 0xD8;

  int in = 0;
  int e = 0;
  int o = 0;

  for (; in + 16 <= N; in += 16, e += 8, o += 4) {
    /*
     * v0 = [x0,  x1,  x2,  x3]
     * v1 = [x4,  x5,  x6,  x7]
     * v2 = [x8,  x9,  x10, x11]
     * v3 = [x12, x13, x14, x15]
     */
    simde__m256i v0 = simde_mm256_loadu_si256((const simde__m256i *)(const void *)&x[in + 0]);

    simde__m256i v1 = simde_mm256_loadu_si256((const simde__m256i *)(const void *)&x[in + 4]);

    simde__m256i v2 = simde_mm256_loadu_si256((const simde__m256i *)(const void *)&x[in + 8]);

    simde__m256i v3 = simde_mm256_loadu_si256((const simde__m256i *)(const void *)&x[in + 12]);

    /*
     * p0 = [x0,  x2,  x1,  x3]
     * p1 = [x4,  x6,  x5,  x7]
     * p2 = [x8,  x10, x9,  x11]
     * p3 = [x12, x14, x13, x15]
     */
    simde__m256i p0 = simde_mm256_permute4x64_epi64(v0, 0xD8);

    simde__m256i p1 = simde_mm256_permute4x64_epi64(v1, 0xD8);

    simde__m256i p2 = simde_mm256_permute4x64_epi64(v2, 0xD8);

    simde__m256i p3 = simde_mm256_permute4x64_epi64(v3, 0xD8);

    /*
     * E :
     *
     * low128(p0) = [x0,  x2]
     * low128(p1) = [x4,  x6]
     *
     * E0 = [x0, x2, x4, x6]
     *
     * low128(p2) = [x8,  x10]
     * low128(p3) = [x12, x14]
     *
     * E1 = [x8, x10, x12, x14]
     */
    simde__m256i E0 = simde_mm256_permute2x128_si256(p0, p1, 0x20);

    simde__m256i E1 = simde_mm256_permute2x128_si256(p2, p3, 0x20);

    simde_mm256_storeu_si256((simde__m256i *)(void *)&E_in[e + 0], E0);
    simde_mm256_storeu_si256((simde__m256i *)(void *)&E_in[e + 4], E1);

    /*
     * high128(p0) = [x1,  x3]
     * high128(p1) = [x5,  x7]
     * high128(p2) = [x9,  x11]
     * high128(p3) = [x13, x15]
     */
    simde__m128i h0 = simde_mm256_extracti128_si256(p0, 1);
    simde__m128i h1 = simde_mm256_extracti128_si256(p1, 1);
    simde__m128i h2 = simde_mm256_extracti128_si256(p2, 1);
    simde__m128i h3 = simde_mm256_extracti128_si256(p3, 1);

    /*
     * O1 :
     *
     * unpacklo_epi64(h0, h1) = [x1, x5]
     * unpacklo_epi64(h2, h3) = [x9, x13]
     *
     * O3 :
     *
     * unpackhi_epi64(h0, h1) = [x3, x7]
     * unpackhi_epi64(h2, h3) = [x11, x15]
     */
    simde__m128i o1_0 = simde_mm_unpacklo_epi64(h0, h1);
    simde__m128i o3_0 = simde_mm_unpackhi_epi64(h0, h1);

    simde__m128i o1_1 = simde_mm_unpacklo_epi64(h2, h3);
    simde__m128i o3_1 = simde_mm_unpackhi_epi64(h2, h3);

    simde_mm_storeu_si128((simde__m128i *)(void *)&O1_in[o + 0], o1_0);
    simde_mm_storeu_si128((simde__m128i *)(void *)&O1_in[o + 2], o1_1);

    simde_mm_storeu_si128((simde__m128i *)(void *)&O3_in[o + 0], o3_0);
    simde_mm_storeu_si128((simde__m128i *)(void *)&O3_in[o + 2], o3_1);
  }

  for (; in < N; in++) {
    if ((in & 1) == 0) {
      E_in[in >> 1] = x[in];
    } else if ((in & 3) == 1) {
      O1_in[in >> 2] = x[in];
    } else {
      O3_in[in >> 2] = x[in];
    }
  }
}

static void dft_split_radix_pure_simd_corelts(const float complex *x, float complex *y, float complex *work, int N)
{
  if (N == 64) {
    dft64lts(x, y);
    return;
  }

  if (N == 32) {
    dft32lts_avx2(x, y);
    return;
  }

  int half = N / 2;
  int quarter = N / 4;

  float complex *sub_in = work;
  float complex *sub_out = work + N;

  float complex *E = sub_out;
  float complex *O1 = sub_out + half;
  float complex *O3 = sub_out + half + quarter;
  pack_split_radix_input_f32_avx2_fused(x, sub_in, N);

  dft_split_radix_pure_simd_corelts(sub_in, E, work + 2 * N, half);
  dft_split_radix_pure_simd_corelts(sub_in + half, O1, work + 2 * N, quarter);
  dft_split_radix_pure_simd_corelts(sub_in + half + quarter, O3, work + 2 * N, quarter);

  sr_combine_f32_prepack(E, O1, O3, y, N, &sr_twiddles_f32[log2_int(N)]);
}

static void dft_split_radix_pure_simdlts(const float complex *x, float complex *y, int N)
{
  float complex work[262144] __attribute__((aligned(64)));

  if (4 * N > 262144) {
    printf("work buffer too small: need %d, have 16384\n", 4 * N);
    return;
  }

  dft_split_radix_pure_simd_corelts(x, y, work, N);
}

static void radix_3_fft_c16_scaled(const c16_t *src, c16_t *dst, int N, dft_dir_t dir)
{
  if ((N % 3) != 0) {
    printf("radix_3_fft_forward_c16_scaled: N must be divisible by 3\n");
    return;
  }

  const int size = N / 3;

  if (!is_power_of_two_int(size)) {
    printf("radix_3_fft_forward_c16_scaled: N/3 must be power of two\n");
    return;
  }

  const TwiddleTable *tw = twiddle_table_get(N);

  if (!tw || !tw->r3_q15_w1_re || !tw->r3_q15_w1_im || !tw->r3_q15_w2_re || !tw->r3_q15_w2_im || !tw->r3_q15_w1_re_inv
      || !tw->r3_q15_w1_im_inv || !tw->r3_q15_w2_re_inv || !tw->r3_q15_w2_im_inv) {
    printf("radix_3_fft_c16_scaled: missing radix-3 Q15 twiddles\n");
    return;
  }

  c16_t *work = aligned_malloc64(sizeof(c16_t) * 6 * (size_t)N);

  if (!work) {
    printf("radix_3_fft_forward_c16_scaled: work allocation failed\n");
    return;
  }

  c16_t *in = work;
  c16_t *tmp = work + N;
  c16_t *sub_work = work + 2 * N;

  for (int n = 0; n < size; n++) {
    in[0 * size + n] = src[3 * n + 0];
    in[1 * size + n] = src[3 * n + 1];
    in[2 * size + n] = src[3 * n + 2];
  }

  dft_split_radix_pure_simd_core(in + 0 * size, tmp + 0 * size, sub_work, size, dir);

  dft_split_radix_pure_simd_core(in + 1 * size, tmp + 1 * size, sub_work, size, dir);

  dft_split_radix_pure_simd_core(in + 2 * size, tmp + 2 * size, sub_work, size, dir);

  const __m128i *w1_re_tbl = (dir == DFT_DIR_FORWARD) ? tw->r3_q15_w1_re : tw->r3_q15_w1_re_inv;

  const __m128i *w1_im_tbl = (dir == DFT_DIR_FORWARD) ? tw->r3_q15_w1_im : tw->r3_q15_w1_im_inv;

  const __m128i *w2_re_tbl = (dir == DFT_DIR_FORWARD) ? tw->r3_q15_w2_re : tw->r3_q15_w2_re_inv;

  const __m128i *w2_im_tbl = (dir == DFT_DIR_FORWARD) ? tw->r3_q15_w2_im : tw->r3_q15_w2_im_inv;

  int k = 0;

  for (; k + 3 < size; k += 4) {
    const int b = k >> 2;

    const __m128i A = _mm_loadu_si128((const __m128i *)(tmp + 0 * size + k));
    const __m128i X1 = _mm_loadu_si128((const __m128i *)(tmp + 1 * size + k));
    const __m128i X2 = _mm_loadu_si128((const __m128i *)(tmp + 2 * size + k));

    __m128i Y0;
    __m128i Y1;
    __m128i Y2;

    radix3_combine4_q15_128_fast(A, X1, X2, w1_re_tbl[b], w1_im_tbl[b], w2_re_tbl[b], w2_im_tbl[b], &Y0, &Y1, &Y2, dir);

    _mm_storeu_si128((__m128i *)(dst + 0 * size + k), Y0);
    _mm_storeu_si128((__m128i *)(dst + 1 * size + k), Y1);
    _mm_storeu_si128((__m128i *)(dst + 2 * size + k), Y2);
  }

  if (k != size) {
    printf("radix_3_fft_forward_c16_scaled: scalar tail not implemented, size=%d\n", size);
  }

  free(work);
}

static void dft_mixed_radix_c16_scaled(c16_t *x, c16_t *y, int N, dft_dir_t dir)
{
  if ((N % 3) == 0 && is_power_of_two_int(N / 3)) {
    radix_3_fft_c16_scaled(x, y, N, dir);
    return;
  }

  if (is_power_of_two_int(N)) {
    dft_split_radix_pure_simd(x, y, N, dir);
    return;
  }

  printf("dft_mixed_radix_c16_scaled: unsupported N = %d\n", N);
}

static double time_mixed_ns_per_dftlts(float complex *in, float complex *out, int N)
{
  const int W = 20000;
  const int T = 200000;

  for (int i = 0; i < W; i++)
    fft_forward_recursive_core(in, out, N);
  uint64_t t0 = ns_now();

  for (int i = 0; i < T; i++)
    fft_forward_recursive_core(in, out, N);
  uint64_t t1 = ns_now();
  consume_output(out, N);

  return (double)(t1 - t0) / (double)T;
}

static double time_splitflt_ns_per_dftlts(float complex *in, float complex *out, int N)
{
  const int W = 20;
  const int T = 200;

  for (int i = 0; i < W; i++)
    dft_split_radix_pure_simdlts(in, out, N);
  uint64_t t0 = ns_now();

  for (int i = 0; i < T; i++)
    dft_split_radix_pure_simdlts(in, out, N);
  uint64_t t1 = ns_now();
  consume_output(out, N);
  return (double)(t1 - t0) / (double)T;
}

static double time_split256_ns_per_dft(c16_t *in, c16_t *out, int N)
{
  const int W = 20000;
  const int T = 200000;

  for (int i = 0; i < W; i++)
    dft_mixed_radix_c16_scaled(in, out, N, -1);
  uint64_t t0 = ns_now();

  for (int i = 0; i < T; i++)
    dft_mixed_radix_c16_scaled(in, out, N, -1);
  uint64_t t1 = ns_now();
  consume_output_c16(out, N);

  return (double)(t1 - t0) / (double)T;
}

//===================================================================
// MIXED RADIX
//===================================================================

static inline void radix4_split_input_simd_lts(const float complex *src, float complex *sub_in, int size)
{
  for (int i = 0; i < size; i += 4) {
    __m256 a = *((const __m256 *)(src + 4 * i));
    __m256 b = *((const __m256 *)(src + 4 * i + 4));
    __m256 c = *((const __m256 *)(src + 4 * i + 8));
    __m256 d = *((const __m256 *)(src + 4 * i + 12));

    transpose4_complex_shuffle_ps(&a, &b, &c, &d);

    *((__m256 *)(sub_in + i)) = a;
    _mm256_storeu_ps((float *)(sub_in + size + i), b);
    *((__m256 *)(sub_in + 2 * size + i)) = c;
    _mm256_storeu_ps((float *)(sub_in + 3 * size + i), d);
  }
}

static inline void radix4_combine_lts(const TwiddleTable *tw,
                                      const int i,
                                      const __m256 A0,
                                      const __m256 A1,
                                      const __m256 A2,
                                      const __m256 A3,
                                      __m256 *Y0,
                                      __m256 *Y1,
                                      __m256 *Y2,
                                      __m256 *Y3)
{
  __m256 B[4];

  B[0] = A0;

  B[1] = complex_mul4_prepack(A1, tw->r4_w1_re[i], tw->r4_w1_im[i]);

  B[2] = complex_mul4_prepack(A2, tw->r4_w2_re[i], tw->r4_w2_im[i]);

  B[3] = complex_mul4_prepack(A3, tw->r4_w3_re[i], tw->r4_w3_im[i]);

  dft4x4lts(B[0], B[1], B[2], B[3], Y0, Y1, Y2, Y3);
}

static inline void radix4_combine_scalar_forward(const float complex A0,
                                                 const float complex A1,
                                                 const float complex A2,
                                                 const float complex A3,
                                                 float complex *Y0,
                                                 float complex *Y1,
                                                 float complex *Y2,
                                                 float complex *Y3)
{
  const float complex s02 = A0 + A2;
  const float complex d02 = A0 - A2;

  const float complex s13 = A1 + A3;
  const float complex d13 = A1 - A3;

  *Y0 = s02 + s13;
  *Y2 = s02 - s13;

  /*
   * Forward DFT4:
   * Y1 = d02 - j*d13
   * Y3 = d02 + j*d13
   */
  *Y1 = d02 - I * d13;
  *Y3 = d02 + I * d13;
}

static void radix_4_fft_forward_lts(const float complex *src, float complex *dst, int N)
{
  const int size = N / 4;

  float complex sub_in[N] __attribute__((aligned(64)));
  float complex tmp[N] __attribute__((aligned(64)));

  /*
   * Pour N = 4, 8, 12, size = 1, 2, 3.
   * Donc il faut que le split marche aussi en scalaire.
   *
   * Si radix4_split_input_simd_lts() ne gère pas les tails,
   * utilise cette version scalaire.
   */
  radix4_split_input_simd_lts(src, sub_in, size);

  fft_forward_recursive_core(sub_in + 0 * size, tmp + 0 * size, size);
  fft_forward_recursive_core(sub_in + 1 * size, tmp + 1 * size, size);
  fft_forward_recursive_core(sub_in + 2 * size, tmp + 2 * size, size);
  fft_forward_recursive_core(sub_in + 3 * size, tmp + 3 * size, size);

  const TwiddleTable *tw = twiddle_table_get(N);
  if (!tw) {
    return;
  }

  const float complex *W = tw->forward;

  int b = 0;

  for (; b < size / 4; b++) {
    const int k = b * 4;
    const __m256 A0 = _mm256_loadu_ps((const float *)(tmp + 0 * size + k));
    const __m256 A1 = _mm256_loadu_ps((const float *)(tmp + 1 * size + k));
    const __m256 A2 = _mm256_loadu_ps((const float *)(tmp + 2 * size + k));
    const __m256 A3 = _mm256_loadu_ps((const float *)(tmp + 3 * size + k));

    __m256 Y0, Y1, Y2, Y3;

    radix4_combine_lts(tw, b, A0, A1, A2, A3, &Y0, &Y1, &Y2, &Y3);

    _mm256_storeu_ps((float *)(dst + 0 * size + k), Y0);
    _mm256_storeu_ps((float *)(dst + 1 * size + k), Y1);
    _mm256_storeu_ps((float *)(dst + 2 * size + k), Y2);
    _mm256_storeu_ps((float *)(dst + 3 * size + k), Y3);
  }

  /*
   * Scalar tail.
   * Très important :
   * on commence à k = b * 4, pas à k = b.
   */
  for (int k = b * 4; k < size; k++) {
    const float complex A0 = tmp[0 * size + k];
    const float complex A1 = W[1 * k] * tmp[1 * size + k];
    const float complex A2 = W[2 * k] * tmp[2 * size + k];
    const float complex A3 = W[3 * k] * tmp[3 * size + k];

    radix4_combine_scalar_forward(A0, A1, A2, A3, &dst[0 * size + k], &dst[1 * size + k], &dst[2 * size + k], &dst[3 * size + k]);
  }
}

static int count_power_two_sizes(int N)
{
  int count = 0;
  return count = __builtin_popcount(N - 1);
}

int is_power_of_two(int N)
{
  return (N > 0) && ((N & (N - 1)) == 0);
}

static inline void radix2_split_input_simd_lts(const float complex *src, float complex *sub_in, int size)
{
  int i = 0;

  const __m256i perm = _mm256_setr_epi32(0, 1, 4, 5, 2, 3, 6, 7);

  for (; i + 3 < size; i += 4) {
    const float *p = (const float *)(src + 2 * i);

    __m256 a = _mm256_loadu_ps(p + 0);
    __m256 b = _mm256_loadu_ps(p + 8);

    __m256 even_tmp = _mm256_shuffle_ps(a, b, _MM_SHUFFLE(1, 0, 1, 0));
    __m256 odd_tmp = _mm256_shuffle_ps(a, b, _MM_SHUFFLE(3, 2, 3, 2));

    __m256 even_v = _mm256_permutevar8x32_ps(even_tmp, perm);
    __m256 odd_v = _mm256_permutevar8x32_ps(odd_tmp, perm);

    _mm256_storeu_ps((float *)(sub_in + i), even_v);
    _mm256_storeu_ps((float *)(sub_in + size + i), odd_v);
  }
}

static void radix_2_fft_forward(const float complex *src, float complex *dst, int N)
{
  const int size = N / 2;

  float complex sub_in[N] __attribute__((aligned(64)));
  float complex tmp[N] __attribute__((aligned(64)));

  /*
   * DIT radix-2 split:
   *
   * sub_in[0       ... size-1] = src[0], src[2], src[4], ...
   * sub_in[size    ... N-1]    = src[1], src[3], src[5], ...
   */
  for (int i = 0; i < size; i++) {
    sub_in[i] = src[2 * i];
    sub_in[i + size] = src[2 * i + 1];
  }

  fft_forward_recursive_core(sub_in + 0 * size, tmp + 0 * size, size);
  fft_forward_recursive_core(sub_in + 1 * size, tmp + 1 * size, size);

  const TwiddleTable *tw = twiddle_table_get(N);
  if (!tw) {
    return;
  }

  int k = 0;

  for (; k + 3 < size; k += 4) {
    const int b = k >> 2;

    const __m256 A = _mm256_loadu_ps((const float *)(tmp + k));
    const __m256 X = _mm256_loadu_ps((const float *)(tmp + size + k));

    const __m256 B = complex_mul4_prepack(X, tw->r2_w_re[b], tw->r2_w_im[b]);

    const __m256 Y0 = _mm256_add_ps(A, B);
    const __m256 Y1 = _mm256_sub_ps(A, B);

    _mm256_storeu_ps((float *)(dst + k), Y0);
    _mm256_storeu_ps((float *)(dst + size + k), Y1);
  }

  /*
   * Scalar tail.
   */
  {
    const float complex *W = tw->forward;

    for (; k < size; k++) {
      const float complex A = tmp[k];
      const float complex B = W[k] * tmp[k + size];

      dst[k] = A + B;
      dst[k + size] = A - B;
    }
  }
}

static void radix_3_fft_forward(const float complex *src, float complex *dst, int N)
{
  const int size = N / 3;
  float complex *in = NULL;
  float complex *tmp = NULL;

  if (posix_memalign((void **)&in, 64, sizeof(float complex) * N) != 0 || !in) {
    abort();
  }

  if (posix_memalign((void **)&tmp, 64, sizeof(float complex) * N) != 0 || !tmp) {
    free(in);
    abort();
  }

  for (int n = 0; n < size; n++) {
    in[0 * size + n] = src[3 * n + 0];
    in[1 * size + n] = src[3 * n + 1];
    in[2 * size + n] = src[3 * n + 2];
  }
  fft_forward_recursive_core(in + 0 * size, tmp + 0 * size, size);
  fft_forward_recursive_core(in + 1 * size, tmp + 1 * size, size);
  fft_forward_recursive_core(in + 2 * size, tmp + 2 * size, size);

  const TwiddleTable *tw = twiddle_table_get(N);

  const __m256 half = _mm256_set1_ps(0.5f);
  const __m256 c3v = _mm256_set1_ps(0.86602540378443864676f);

  int k = 0;

  for (; k + 3 < size; k += 4) {
    const int b = k >> 2;

    const __m256 A = _mm256_loadu_ps((const float *)(tmp + 0 * size + k));
    const __m256 X1 = _mm256_loadu_ps((const float *)(tmp + 1 * size + k));
    const __m256 X2 = _mm256_loadu_ps((const float *)(tmp + 2 * size + k));

    const __m256 B = complex_mul4_prepack(X1, tw->r3_w1_re[b], tw->r3_w1_im[b]);

    const __m256 C = complex_mul4_prepack(X2, tw->r3_w2_re[b], tw->r3_w2_im[b]);

    const __m256 S = _mm256_add_ps(B, C);
    const __m256 D = _mm256_sub_ps(B, C);

    /*
     * Y0 = A + S
     */
    const __m256 Y0 = _mm256_add_ps(A, S);

    /*
     * base = A - 0.5 * S
     */
    const __m256 base = _mm256_sub_ps(A, _mm256_mul_ps(half, S));

    /*
     * c3D = c3 * D
     */
    const __m256 c3D = _mm256_mul_ps(c3v, D);

    /*
     * Y1 = A - 0.5*S - j*c3*D
     * Y2 = A - 0.5*S + j*c3*D
     */
    const __m256 Y1 = _mm256_add_ps(base, mul_minuslts(c3D));
    const __m256 Y2 = _mm256_add_ps(base, mullts(c3D));

    _mm256_storeu_ps((float *)(dst + k + 0 * size), Y0);
    _mm256_storeu_ps((float *)(dst + k + 1 * size), Y1);
    _mm256_storeu_ps((float *)(dst + k + 2 * size), Y2);
  }

  {
    const float complex *W = tw->forward;
    const float c3 = 0.86602540378443864676f;

    for (; k < size; k++) {
      const float complex A = tmp[0 * size + k];
      const float complex B = W[k] * tmp[1 * size + k];
      const float complex C = W[2 * k] * tmp[2 * size + k];

      const float complex S = B + C;
      const float complex D = B - C;

      dst[k + 0 * size] = A + S;
      dst[k + 1 * size] = A - 0.5f * S - I * c3 * D;
      dst[k + 2 * size] = A - 0.5f * S + I * c3 * D;
    }
  }
  free(in);
  free(tmp);
}

static void radix_5_fft_forward(const float complex *src, float complex *dst, int N)
{
  const int size = N / 5;

  float complex *in = NULL;
  float complex *tmp = NULL;

  if (posix_memalign((void **)&in, 64, sizeof(float complex) * N) != 0 || !in) {
    abort();
  }

  if (posix_memalign((void **)&tmp, 64, sizeof(float complex) * N) != 0 || !tmp) {
    free(in);
    abort();
  }

  for (int n = 0; n < size; n++) {
    in[0 * size + n] = src[5 * n + 0];
    in[1 * size + n] = src[5 * n + 1];
    in[2 * size + n] = src[5 * n + 2];
    in[3 * size + n] = src[5 * n + 3];
    in[4 * size + n] = src[5 * n + 4];
  }

  fft_forward_recursive_core(in + 0 * size, tmp + 0 * size, size);
  fft_forward_recursive_core(in + 1 * size, tmp + 1 * size, size);
  fft_forward_recursive_core(in + 2 * size, tmp + 2 * size, size);
  fft_forward_recursive_core(in + 3 * size, tmp + 3 * size, size);
  fft_forward_recursive_core(in + 4 * size, tmp + 4 * size, size);

  const TwiddleTable *tw = twiddle_table_get(N);

  const __m256 c1v = _mm256_set1_ps(0.30901699437494742410f);
  const __m256 c2v = _mm256_set1_ps(-0.80901699437494742410f);
  const __m256 s1v = _mm256_set1_ps(0.95105651629515357212f);
  const __m256 s2v = _mm256_set1_ps(0.58778525229247312917f);

  int k = 0;

  for (; k + 3 < size; k += 4) {
    const int b = k >> 2;

    const __m256 A = _mm256_loadu_ps((const float *)(tmp + 0 * size + k));
    const __m256 X1 = _mm256_loadu_ps((const float *)(tmp + 1 * size + k));
    const __m256 X2 = _mm256_loadu_ps((const float *)(tmp + 2 * size + k));
    const __m256 X3 = _mm256_loadu_ps((const float *)(tmp + 3 * size + k));
    const __m256 X4 = _mm256_loadu_ps((const float *)(tmp + 4 * size + k));

    const __m256 B = complex_mul4_prepack(X1, tw->r5_w1_re[b], tw->r5_w1_im[b]);

    const __m256 C = complex_mul4_prepack(X2, tw->r5_w2_re[b], tw->r5_w2_im[b]);

    const __m256 D = complex_mul4_prepack(X3, tw->r5_w3_re[b], tw->r5_w3_im[b]);

    const __m256 E = complex_mul4_prepack(X4, tw->r5_w4_re[b], tw->r5_w4_im[b]);

    const __m256 T1 = _mm256_add_ps(B, E);
    const __m256 T2 = _mm256_add_ps(C, D);

    const __m256 U1 = _mm256_sub_ps(B, E);
    const __m256 U2 = _mm256_sub_ps(C, D);

    /*
     * Y0 = A + T1 + T2
     */
    const __m256 Y0 = _mm256_add_ps(A, _mm256_add_ps(T1, T2));

    /*
     * Y1 / Y4
     *
     * base1 = A + c1*T1 + c2*T2
     * imag1 = s1*U1 + s2*U2
     *
     * Y1 = base1 - j*imag1
     * Y4 = base1 + j*imag1
     */
    const __m256 base1 = _mm256_add_ps(A, _mm256_add_ps(_mm256_mul_ps(c1v, T1), _mm256_mul_ps(c2v, T2)));

    const __m256 imag1 = _mm256_add_ps(_mm256_mul_ps(s1v, U1), _mm256_mul_ps(s2v, U2));

    const __m256 Y1 = _mm256_add_ps(base1, mul_minuslts(imag1));
    const __m256 Y4 = _mm256_add_ps(base1, mullts(imag1));

    /*
     * Y2 / Y3
     *
     * base2 = A + c2*T1 + c1*T2
     * imag2 = s2*U1 - s1*U2
     *
     * Y2 = base2 - j*imag2
     * Y3 = base2 + j*imag2
     */
    const __m256 base2 = _mm256_add_ps(A, _mm256_add_ps(_mm256_mul_ps(c2v, T1), _mm256_mul_ps(c1v, T2)));

    const __m256 imag2 = _mm256_sub_ps(_mm256_mul_ps(s2v, U1), _mm256_mul_ps(s1v, U2));

    const __m256 Y2 = _mm256_add_ps(base2, mul_minuslts(imag2));
    const __m256 Y3 = _mm256_add_ps(base2, mullts(imag2));

    _mm256_storeu_ps((float *)(dst + k + 0 * size), Y0);
    _mm256_storeu_ps((float *)(dst + k + 1 * size), Y1);
    _mm256_storeu_ps((float *)(dst + k + 2 * size), Y2);
    _mm256_storeu_ps((float *)(dst + k + 3 * size), Y3);
    _mm256_storeu_ps((float *)(dst + k + 4 * size), Y4);
  }

  {
    const float complex *W = tw->forward;

    const float c1 = 0.30901699437494742410f;
    const float c2 = -0.80901699437494742410f;
    const float s1 = 0.95105651629515357212f;
    const float s2 = 0.58778525229247312917f;

    for (; k < size; k++) {
      const float complex A = tmp[0 * size + k];

      const float complex B = W[1 * k] * tmp[1 * size + k];
      const float complex C = W[2 * k] * tmp[2 * size + k];
      const float complex D = W[3 * k] * tmp[3 * size + k];
      const float complex E = W[4 * k] * tmp[4 * size + k];

      const float complex T1 = B + E;
      const float complex T2 = C + D;

      const float complex U1 = B - E;
      const float complex U2 = C - D;

      dst[k + 0 * size] = A + T1 + T2;

      const float complex base1 = A + c1 * T1 + c2 * T2;
      const float complex imag1 = s1 * U1 + s2 * U2;

      dst[k + 1 * size] = base1 - I * imag1;
      dst[k + 4 * size] = base1 + I * imag1;

      const float complex base2 = A + c2 * T1 + c1 * T2;
      const float complex imag2 = s2 * U1 - s1 * U2;

      dst[k + 2 * size] = base2 - I * imag2;
      dst[k + 3 * size] = base2 + I * imag2;
    }
  }
  free(in);
  free(tmp);
}
static void classic_dft_forward_cached(const float complex *src, float complex *dst, int N)
{
  abort();
  const TwiddleTable *tw = twiddle_table_get(N);
  const float complex *W = tw->forward;

  for (int k = 0; k < N; k++) {
    float complex acc = 0.0f + 0.0f * I;

    for (int n = 0; n < N; n++) {
      acc += src[n] * W[(k * n) % N];
    }

    dst[k] = acc;
  }
}

static void fft_forward_recursive_core(const float complex *src, float complex *dst, int N)
{
  if (N == 1) {
    dst[0] = src[0];
    return;
  }

  /*
   * Main SIMD leaf.
   */

  if (N == 8) {
    dft8lts(src, dst);
    return;
  }

  if (N == 12) {
    dft12lts_avx2(src, dst);
    return;
  }

  if (N == 16) {
    dft16lts_avx2(src, dst);
    return;
  }
  if (N == 64) {
    dft64lts(src, dst);
    return;
  }

  if (N == 128) {
    dft_split_radix_pure_simdlts(src, dst, N);
    return;
  }

  if (N % 3 == 0) {
    radix_3_fft_forward(src, dst, N);
    return;
  }

  if (N % 5 == 0) {
    radix_5_fft_forward(src, dst, N);
    return;
  }

  if (N == 32) {
    dft32lts_avx2(src, dst);
    return;
  }

  if (N % 4 == 0) {
    radix_4_fft_forward_lts(src, dst, N);
    return;
  }

  if (is_power_of_two(N) && N > 128) {
    dft_split_radix_pure_simdlts(src, dst, N);
    return;
  }

  if (N % 2 == 0) {
    radix_2_fft_forward(src, dst, N);
    return;
  }

  /*
   * Prime or unsupported size.
   */
  classic_dft_forward_cached(src, dst, N);
}

void fft_recursive_forward(const float complex *src, float complex *dst, int N)
{
  fft_forward_recursive_core(src, dst, N);
}

#ifdef USE_FFTW_BACKEND
static double time_fftw_ns_per_dft(fftwf_plan p)
{
  const int W = 20;
  const int T = 200;

  for (int i = 0; i < W; i++)
    fftwf_execute(p);

  uint64_t t0 = ns_now();
  for (int i = 0; i < T; i++)
    fftwf_execute(p);
  uint64_t t1 = ns_now();

  return (double)(t1 - t0) / (double)T;
}
#endif

#ifdef USE_FFTZ_BACKEND
static double time_fftz_ns_per_dft(void *handle, float *in, float *out)
{
  const int W = 20;
  const int T = 200;

  for (int i = 0; i < W; i++)
    aoclfftz_execute_io(handle, in, out);

  uint64_t t0 = ns_now();
  for (int i = 0; i < T; i++)
    aoclfftz_execute_io(handle, in, out);
  uint64_t t1 = ns_now();

  return (double)(t1 - t0) / (double)T;
}
#endif

static inline int is_oai_dft_supported_lts(int N)
{
  switch (N) {
    case 4:
    case 32:
    case 8:
    case 64:
    case 128:
    case 256:
    case 512:
    case 1024:
    case 1536:
    case 2048:
    case 3072:
    case 3240:
    case 3000:
    case 2916:
    case 2880:
    case 2700:
    case 2592:
    case 2400:
    case 2304:
    case 2160:
    case 1944:
    case 1920:
    case 1800:
    case 1728:
    case 1620:
    case 1500:
    case 1440:
    case 1296:
    case 1200:
    case 1152:
    case 1080:
    case 972:
    case 960:
    case 900:
    case 864:
    case 768:
    case 720:
    case 648:
    case 600:
    case 576:
    case 540:
    case 480:
    case 432:
    case 384:
    case 360:
    case 324:
    case 300:
    case 288:
    case 240:
    case 216:
    case 192:
    case 180:
    case 144:
    case 120:
    case 108:
    case 96:
    case 72:
    case 60:
    case 48:
    case 36:
    case 24:
    case 12:
    case 16:
    case 6144:
    case 12288:
    case 16384:
    case 18432:
    case 24576:
    case 32768:
    case 36864:
    case 65536:
    case 49152:
    case 98304:
    case 1048576:
    case 1572864:
    case 4096:
    case 8192:
      return 1;

    default:
      return 0;
  }
}
static double rms_evm_percent_fc_gain_corrected(const float complex *ref, const float complex *got, int n, float complex alpha)
{
  double num = 0.0;
  double den = 0.0;

  for (int i = 0; i < n; i++) {
    float complex corrected = got[i] / alpha;

    double rr = crealf(ref[i]);
    double ri = cimagf(ref[i]);
    double gr = crealf(corrected);
    double gi = cimagf(corrected);

    double er = gr - rr;
    double ei = gi - ri;

    num += er * er + ei * ei;
    den += rr * rr + ri * ri;
  }

  if (den < 1e-30)
    return 0.0;

  return 100.0 * sqrt(num / den);
}
static float complex estimate_gain_fc(const float complex *ref, const float complex *got, int n)
{
  double num_re = 0.0;
  double num_im = 0.0;
  double den = 0.0;

  for (int i = 0; i < n; i++) {
    double rr = crealf(ref[i]);
    double ri = cimagf(ref[i]);
    double gr = crealf(got[i]);
    double gi = cimagf(got[i]);

    num_re += gr * rr + gi * ri;
    num_im += gi * rr - gr * ri;
    den += rr * rr + ri * ri;
  }

  if (den < 1e-30)
    return 0.0f + 0.0f * I;

  return (float)(num_re / den) + (float)(num_im / den) * I;
}
static inline int is_split_radix_supported_lts(int N)
{
  if (N <= 0)
    return 0;
  if (N == 96)
    return 0;

  /*
   * Cas split-radix pur power-of-two.
   */
  if (is_power_of_two_int(N) && N > 32)
    return 1;

  if ((N % 3) == 0) {
    const int size = N / 3;

    if (is_power_of_two_int(size) && size > 32)
      return 1;
  }

  return 0;
}

static inline int is_split_radix_supported_ltsflt(int N)
{
  return (is_power_of_two_int(N) && N > 32);
}

static inline void print_evm_col(double v)
{
  if (isfinite(v)) {
    printf("%12.6f", v);
  } else {
    printf("%12s", "N/A");
  }
}

static inline void print_time_col(double v)
{
  if (isfinite(v)) {
    printf("%12.1f", v);
  } else {
    printf("%12s", "N/A");
  }
}

/* =========================================================
 * Main
 * ========================================================= */

/* =========================================================
 * Main
 * ========================================================= */
int main(void)
{
  load_dftslib();

  init_dft64_float_twiddles(&g_dft64f_tw);

  const int sizes[] = {4,    8,    16,   32,   36,   48,   12,    24,    60,    64,    72,    96,    108,   120,   128,
                       144,  180,  192,  216,  240,  256,  288,   300,   324,   360,   384,   432,   480,   512,   540,
                       576,  600,  648,  720,  768,  864,  900,   960,   972,   1024,  1080,  1152,  1200,  1296,  1440,
                       1500, 1536, 1620, 1728, 1800, 1920, 1944,  2048,  2160,  2304,  2400,  2592,  2700,  2880,  2916,
                       3000, 3072, 3240, 4096, 6144, 8192, 12288, 16384, 18432, 24576, 32768, 36864, 49152, 65536, 98304};

  const int nb_sizes = sizeof(sizes) / sizeof(sizes[0]);
  const int maxN = 98304;

  /*
   * Twiddles split-radix.
   * Seulement power-of-two.
   */
  init_sr_twiddles_power2(maxN);
  init_sr_twiddles_f32_power2(maxN);

  const unsigned seed = 12345;
  randominit(seed);

  double coeffs[] = {1, 10, 20, 30, 40, 50, 60, 70};
  const int nb_coeffs = sizeof(coeffs) / sizeof(coeffs[0]);

  printf(
      "============================================================================================================================"
      "===================================\n");
  printf("DFT / IDFT robustness comparison\n");
  printf("Forward reference: FFTW_FORWARD / sqrt(N)\n");
  printf("Inverse reference: FFTW_BACKWARD(freq_input) / sqrt(N)\n");
  printf("Roundtrip check: IDFT(DFT(x_oai)) compared to quantized x_oai\n");
  printf("Seed = %u\n", seed);
  printf(
      "============================================================================================================================"
      "===================================\n\n");

  printf("%8s | %8s | %12s | %12s | %12s | %12s | %12s | %12s | %12s || %12s | %12s | %12s | %12s | %12s | %12s\n",
         "N",
         "Coeff",
         "OAI DFT %",
         "OAI IDFT %",
         "OAI RT %",
         "SplitOAI %",
         "FFTW EVM %",
         "FFTZ EVM %",
         "Splitflt %",
         "OAI ns",
         "SplitOAI ns",
         "FFTW ns",
         "FFTZ ns",
         "Mixed ns",
         "Splitflt ns");

  printf(
      "---------+----------+--------------+--------------+--------------+--------------+--------------+--------------+-------------"
      "-++--------------+--------------+--------------+--------------+--------------+--------------\n");

  for (int si = 0; si < nb_sizes; si++) {
    const int N = sizes[si];
    const float ref_scale = 1.0f / sqrtf((float)N);

    const int has_oai = is_oai_dft_supported_lts(N);
    const int has_split_c16 = is_split_radix_supported_lts(N);
    const int has_split_f32 = is_split_radix_supported_ltsflt(N);

    cd_t *data = NULL;
    int rett = posix_memalign((void **)&data, 64, sizeof(cd_t) * N);

    if (rett != 0 || !data) {
      printf("allocation failed: data\n");
      return 2;
    }

    for (int i = 0; i < N; i++) {
      data[i].r = gaussZiggurat(0, 1.0);
      data[i].i = gaussZiggurat(0, 1.0);
    }

    float complex *x = NULL;

    c16_t *x_oai = NULL;
    c16_t *oai_out_q = NULL;
    c16_t *oai_idft_out_q = NULL;
    c16_t *split_out_q = NULL;

    float complex *x_oai_f = NULL;
    float complex *oai_out_f = NULL;
    float complex *oai_idft_out_f = NULL;
    float complex *split_out_f = NULL;

    float complex *avx64_out_f = NULL;
    float complex *avx64_scaled_f = NULL;

    float complex *splitflt_out_f = NULL;
    float complex *splitflt_scaled_f = NULL;

    float complex *fftw_out_f = NULL;
    float complex *fftw_scaled_f = NULL;
    float complex *fftw_idft_scaled_f = NULL;

    float complex *fftz_out_f = NULL;
    float complex *fftz_scaled_f = NULL;

    int ret = 0;

    ret |= posix_memalign((void **)&x, 64, sizeof(float complex) * N);

    ret |= posix_memalign((void **)&x_oai, 64, sizeof(c16_t) * N);
    ret |= posix_memalign((void **)&oai_out_q, 64, sizeof(c16_t) * N);
    ret |= posix_memalign((void **)&oai_idft_out_q, 64, sizeof(c16_t) * N);
    ret |= posix_memalign((void **)&split_out_q, 64, sizeof(c16_t) * N);

    ret |= posix_memalign((void **)&x_oai_f, 64, sizeof(float complex) * N);
    ret |= posix_memalign((void **)&oai_out_f, 64, sizeof(float complex) * N);
    ret |= posix_memalign((void **)&oai_idft_out_f, 64, sizeof(float complex) * N);
    ret |= posix_memalign((void **)&split_out_f, 64, sizeof(float complex) * N);

    ret |= posix_memalign((void **)&avx64_out_f, 64, sizeof(float complex) * N);
    ret |= posix_memalign((void **)&avx64_scaled_f, 64, sizeof(float complex) * N);

    ret |= posix_memalign((void **)&splitflt_out_f, 64, sizeof(float complex) * N);
    ret |= posix_memalign((void **)&splitflt_scaled_f, 64, sizeof(float complex) * N);

#ifdef USE_FFTW_BACKEND
    fftwf_complex *fftw_in = NULL;
    fftwf_complex *fftw_out = NULL;
    fftwf_plan fftw_plan = NULL;
    fftwf_plan fftw_plan_idft = NULL;
    int has_fftw_runtime = 0;

    fftw_in = fftwf_malloc(sizeof(fftwf_complex) * N);
    fftw_out = fftwf_malloc(sizeof(fftwf_complex) * N);

    if (fftw_in && fftw_out) {
      fftw_plan = fftwf_plan_dft_1d(N, fftw_in, fftw_out, FFTW_FORWARD, FFTW_ESTIMATE);

      fftw_plan_idft = fftwf_plan_dft_1d(N, fftw_in, fftw_out, FFTW_BACKWARD, FFTW_ESTIMATE);

      if (fftw_plan && fftw_plan_idft) {
        has_fftw_runtime = 1;

        ret |= posix_memalign((void **)&fftw_out_f, 64, sizeof(float complex) * N);
        ret |= posix_memalign((void **)&fftw_scaled_f, 64, sizeof(float complex) * N);
        ret |= posix_memalign((void **)&fftw_idft_scaled_f, 64, sizeof(float complex) * N);
      } else {
        printf("FFTW plan creation failed for N=%d, FFTW reference disabled for this size\n", N);
      }
    } else {
      printf("FFTW allocation failed for N=%d, FFTW reference disabled for this size\n", N);
    }
#endif

#ifdef USE_FFTZ_BACKEND
    float *fftz_in = NULL;
    float *fftz_out = NULL;
    void *fftz_handle = NULL;

    aoclfftz_dim_t fftz_dims[1];
    aoclfftz_dim_t fftz_vecs[1];
    aoclfftz_prob_desc_f fftz_prob;

    fftz_in = aligned_alloc(64, sizeof(float) * 2 * N);
    fftz_out = aligned_alloc(64, sizeof(float) * 2 * N);

    fftz_dims[0].n = N;
    fftz_dims[0].in_stride = 1;
    fftz_dims[0].out_stride = 1;

    fftz_vecs[0].n = 1;
    fftz_vecs[0].in_stride = 2 * N;
    fftz_vecs[0].out_stride = 2 * N;

    memset(&fftz_prob, 0, sizeof(fftz_prob));
    fftz_prob.in = fftz_in;
    fftz_prob.out = fftz_out;
    fftz_prob.vec_rank = 1;
    fftz_prob.dim_rank = 1;
    fftz_prob.dims = fftz_dims;
    fftz_prob.vecs = fftz_vecs;

    fftz_prob.flags.fft_type = 0;
    fftz_prob.flags.fft_direction = 0;
    fftz_prob.flags.storage_order = 0;
    fftz_prob.flags.fft_placement = 1;
    fftz_prob.flags.transpose_mode = 0;
    fftz_prob.flags.bit_reproducibility = 0;

    fftz_prob.pthr_fft.num_threads = 1;
    fftz_prob.pthr_fft.dynamic_load_model = 0;

    fftz_prob.cntrl_params.opt_level = 2;
    fftz_prob.cntrl_params.opt_off = 0;
    fftz_prob.cntrl_params.logger_mode = AOCLFFTZ_LOG_NONE;
    fftz_prob.cntrl_params.measure_stats = 0;

    fftz_handle = aoclfftz_setup_f(&fftz_prob);

    ret |= posix_memalign((void **)&fftz_out_f, 64, sizeof(float complex) * N);
    ret |= posix_memalign((void **)&fftz_scaled_f, 64, sizeof(float complex) * N);
#endif

    if (ret != 0 || !x || !x_oai || !oai_out_q || !oai_idft_out_q || !split_out_q || !x_oai_f || !oai_out_f || !oai_idft_out_f
        || !split_out_f || !avx64_out_f || !avx64_scaled_f || !splitflt_out_f || !splitflt_scaled_f) {
      printf("allocation failed for N=%d\n", N);
      return 2;
    }

    memset(x_oai, 0, sizeof(c16_t) * N);
    memset(oai_out_q, 0, sizeof(c16_t) * N);
    memset(oai_idft_out_q, 0, sizeof(c16_t) * N);
    memset(split_out_q, 0, sizeof(c16_t) * N);

    memset(x_oai_f, 0, sizeof(float complex) * N);
    memset(oai_out_f, 0, sizeof(float complex) * N);
    memset(oai_idft_out_f, 0, sizeof(float complex) * N);
    memset(split_out_f, 0, sizeof(float complex) * N);

    memset(avx64_out_f, 0, sizeof(float complex) * N);
    memset(avx64_scaled_f, 0, sizeof(float complex) * N);

    memset(splitflt_out_f, 0, sizeof(float complex) * N);
    memset(splitflt_scaled_f, 0, sizeof(float complex) * N);

    for (int c = 0; c < nb_coeffs; c++) {
      double coeff = coeffs[c];
      double expand = pow(10.0, .05 * coeff) / sqrt(2);

      for (int i = 0; i < N; i++) {
        double rr = expand * data[i].r;
        double ii = expand * data[i].i;

        x[i] = (float)rr + (float)ii * I;

#ifdef USE_FFTZ_BACKEND
        fftz_in[2 * i + 0] = crealf(x[i]);
        fftz_in[2 * i + 1] = cimagf(x[i]);
#endif

        x_oai[i].r = sat_i16(lrint(rr));
        x_oai[i].i = sat_i16(lrint(ii));
      }

      /*
       * Quantized input as float, used for roundtrip:
       * IDFT(DFT(x_oai)) vs x_oai.
       */
      oai_out_to_float_complex(x_oai, x_oai_f, N);

      /*
       * FFTW forward reference:
       * FFTW_FORWARD(x) / sqrt(N)
       */
#ifdef USE_FFTW_BACKEND
      if (has_fftw_runtime) {
        for (int i = 0; i < N; i++) {
          ((float complex *)fftw_in)[i] = crealf(x[i]) + cimagf(x[i]) * I;
        }

        fftwf_execute(fftw_plan);

        for (int i = 0; i < N; i++) {
          fftw_out_f[i] = ((float complex *)fftw_out)[i];
        }

        scale_complex(fftw_out_f, fftw_scaled_f, N, ref_scale);
      }
#endif

      /*
       * OAI DFT / IDFT c16.
       */
      double evm_oai_scaled = NAN;
      double evm_oai_idft_scaled = NAN;
      double evm_oai_roundtrip = NAN;
      double t_oai = NAN;

      if (has_oai) {
        /*
         * Forward:
         * X = DFT(x_oai)
         */
        dft(get_dft(N), (int16_t *)x_oai, (int16_t *)oai_out_q, 1);

        oai_out_to_float_complex(oai_out_q, oai_out_f, N);

#ifdef USE_FFTW_BACKEND
        if (has_fftw_runtime) {
          evm_oai_scaled = rms_evm_percent_fc(fftw_scaled_f, oai_out_f, N);
        }
#endif

        t_oai = time_oai256_ns_per_dft(x_oai, oai_out_q, N);

#ifdef USE_FFTW_BACKEND
        if (has_fftw_runtime) {
          for (int i = 0; i < N; i++) {
            ((float complex *)fftw_in)[i] = crealf(oai_out_f[i]) + cimagf(oai_out_f[i]) * I;
          }

          fftwf_execute(fftw_plan_idft);

          for (int i = 0; i < N; i++) {
            fftw_out_f[i] = ((float complex *)fftw_out)[i];
          }

          scale_complex(fftw_out_f, fftw_idft_scaled_f, N, ref_scale);
        }
#endif

        idft(get_dft(N), (int16_t *)oai_out_q, (int16_t *)oai_idft_out_q, 1);

        oai_out_to_float_complex(oai_idft_out_q, oai_idft_out_f, N);

#ifdef USE_FFTW_BACKEND
        if (has_fftw_runtime) {
          evm_oai_idft_scaled = rms_evm_percent_fc(fftw_idft_scaled_f, oai_idft_out_f, N);
        }
#endif

        /*
         * Roundtrip:
         * IDFT(DFT(x_oai)) vs x_oai.
         *
         * Since oai_idft_out_q is already IDFT(oai_out_q),
         * this is exactly the roundtrip result.
         */
        evm_oai_roundtrip = rms_evm_percent_fc(x_oai_f, oai_idft_out_f, N);
      }

      /*
       * Split-radix pure SIMD c16.
       * Currently disabled in your test.
       */
      double evm_split_scaled = NAN;
      double t_split = NAN;

      if (has_split_c16) {
        oai_out_to_float_complex(split_out_q, split_out_f, N);

        evm_split_scaled = 0.0;
        t_split = 0.0;
      }

      scale_complex(avx64_out_f, avx64_scaled_f, N, ref_scale);

      double evm_avx64_scaled = 0.0;
      double t_avx64 = 0.0;

#ifdef USE_FFTW_BACKEND
      double evm_fftw = has_fftw_runtime ? 0.0 : NAN;
      double t_fftw = 0.0;
#else
      double evm_fftw = NAN;
      double t_fftw = NAN;
#endif

#ifdef USE_FFTZ_BACKEND
      aoclfftz_execute_io(fftz_handle, fftz_in, fftz_out);

      for (int i = 0; i < N; i++) {
        fftz_out_f[i] = fftz_out[2 * i + 0] + fftz_out[2 * i + 1] * I;
      }

      scale_complex(fftz_out_f, fftz_scaled_f, N, ref_scale);

#ifdef USE_FFTW_BACKEND
      double evm_fftz = has_fftw_runtime ? rms_evm_percent_fc(fftw_scaled_f, fftz_scaled_f, N) : NAN;
#else
      double evm_fftz = NAN;
#endif

      double t_fftz = time_fftz_ns_per_dft(fftz_handle, fftz_in, fftz_out);
#else
      double evm_fftz = NAN;
      double t_fftz = NAN;
#endif

      /*
       * Split-radix float LTS.
       */
      double evm_splitflt_scaled = NAN;
      double t_splitflt = NAN;

      if (has_split_f32) {
        dft_split_radix_pure_simdlts(x, splitflt_out_f, N);
        scale_complex(splitflt_out_f, splitflt_scaled_f, N, ref_scale);

#ifdef USE_FFTW_BACKEND
        if (has_fftw_runtime) {
          evm_splitflt_scaled = rms_evm_percent_fc(fftw_scaled_f, splitflt_scaled_f, N);
        }
#endif

        t_splitflt = 0.0;
      }

      /*
       * Print.
       */
      printf("%8d | %8.2f | ", N, coeff);

      print_evm_col(evm_oai_scaled);
      printf(" | ");

      print_evm_col(evm_oai_idft_scaled);
      printf(" | ");

      print_evm_col(evm_oai_roundtrip);
      printf(" | ");

      print_evm_col(evm_split_scaled);
      printf(" | ");

      print_evm_col(evm_fftw);
      printf(" | ");

      print_evm_col(evm_fftz);
      printf(" | ");

      print_evm_col(evm_splitflt_scaled);
      printf(" || ");

      print_time_col(t_oai);
      printf(" | ");

      print_time_col(t_split);
      printf(" | ");

      print_time_col(t_fftw);
      printf(" | ");

      print_time_col(t_fftz);
      printf(" | ");

      print_time_col(t_avx64);
      printf(" | ");

      print_time_col(t_splitflt);
      printf("\n");
    }

    free(data);

    free(x);

    free(x_oai);
    free(oai_out_q);
    free(oai_idft_out_q);
    free(split_out_q);

    free(x_oai_f);
    free(oai_out_f);
    free(oai_idft_out_f);
    free(split_out_f);

    free(avx64_out_f);
    free(avx64_scaled_f);

    free(splitflt_out_f);
    free(splitflt_scaled_f);

#ifdef USE_FFTW_BACKEND
    if (fftw_plan) {
      fftwf_destroy_plan(fftw_plan);
    }

    if (fftw_plan_idft) {
      fftwf_destroy_plan(fftw_plan_idft);
    }

    if (fftw_in) {
      fftwf_free(fftw_in);
    }

    if (fftw_out) {
      fftwf_free(fftw_out);
    }

    free(fftw_out_f);
    free(fftw_scaled_f);
    free(fftw_idft_scaled_f);
#endif

#ifdef USE_FFTZ_BACKEND
    if (fftz_handle) {
      aoclfftz_destroy(fftz_handle);
    }

    free(fftz_in);
    free(fftz_out);
    free(fftz_out_f);
    free(fftz_scaled_f);
#endif

    printf("\n");
  }

  return 0;
}