/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// AVX2 + FMA channel pipeline kernels, 8 lanes. Built with -mavx2 -mfma regardless of the global
// flags; only called when channel_pipeline_isa_supported() says so. See channel_pipeline_scalar.c for the
// reference versions and channel_pipeline_simd.h for the data layouts.

#if defined(__x86_64__)

#include <stddef.h>
#include <immintrin.h>
#include "channel_pipeline_simd.h"

#define LANES 8

// One c16_t is one 32-bit lane: sign-extend the low half for re, arithmetic-shift for im.
static inline void unpack_c16(__m256i v, __m256 *re, __m256 *im)
{
  *re = _mm256_cvtepi32_ps(_mm256_srai_epi32(_mm256_slli_epi32(v, 16), 16));
  *im = _mm256_cvtepi32_ps(_mm256_srai_epi32(v, 16));
}

// Clamp in float, round to nearest (MXCSR default), then pack re into the low and im into the high
// half of each lane.
static inline __m256i pack_c16(__m256 re, __m256 im)
{
  const __m256 lo = _mm256_set1_ps(-32768.0f), hi = _mm256_set1_ps(32767.0f);
  __m256i r = _mm256_cvtps_epi32(_mm256_min_ps(_mm256_max_ps(re, lo), hi));
  __m256i i = _mm256_cvtps_epi32(_mm256_min_ps(_mm256_max_ps(im, lo), hi));
  return _mm256_or_si256(_mm256_and_si256(r, _mm256_set1_epi32(0xffff)), _mm256_slli_epi32(i, 16));
}

// 8x8 transpose of 32-bit elements (one c16_t each).
static inline void transpose8(__m256i m[8])
{
  __m256 r[8], t[8];
  for (int i = 0; i < 8; i++)
    r[i] = _mm256_castsi256_ps(m[i]);
  for (int i = 0; i < 8; i += 2) {
    t[i] = _mm256_unpacklo_ps(r[i], r[i + 1]);
    t[i + 1] = _mm256_unpackhi_ps(r[i], r[i + 1]);
  }
  for (int i = 0; i < 8; i += 4) {
    r[i] = _mm256_shuffle_ps(t[i], t[i + 2], 0x44);
    r[i + 1] = _mm256_shuffle_ps(t[i], t[i + 2], 0xee);
    r[i + 2] = _mm256_shuffle_ps(t[i + 1], t[i + 3], 0x44);
    r[i + 3] = _mm256_shuffle_ps(t[i + 1], t[i + 3], 0xee);
  }
  for (int i = 0; i < 4; i++) {
    m[i] = _mm256_castps_si256(_mm256_permute2f128_ps(r[i], r[i + 4], 0x20));
    m[i + 4] = _mm256_castps_si256(_mm256_permute2f128_ps(r[i], r[i + 4], 0x31));
  }
}

void channel_pipeline_c16_to_split_avx2(float *re, float *im, const c16_t *src, int n)
{
  int i = 0;
  for (; i + LANES <= n; i += LANES) {
    __m256 r, m;
    unpack_c16(_mm256_loadu_si256((const __m256i *)(src + i)), &r, &m);
    _mm256_storeu_ps(re + i, r);
    _mm256_storeu_ps(im + i, m);
  }
  for (; i < n; i++) {
    re[i] = src[i].r;
    im[i] = src[i].i;
  }
}

void channel_pipeline_split_to_c16_avx2(c16_t *dst, const float *re, const float *im, int n)
{
  int i = 0;
  for (; i + LANES <= n; i += LANES)
    _mm256_storeu_si256((__m256i *)(dst + i), pack_c16(_mm256_loadu_ps(re + i), _mm256_loadu_ps(im + i)));
  for (; i < n; i++) {
    dst[i].r = channel_pipeline_round_s16(re[i]);
    dst[i].i = channel_pipeline_round_s16(im[i]);
  }
}

void channel_pipeline_direct_mac_avx2(float *acc_re,
                                      float *acc_im,
                                      const float *in_re,
                                      const float *in_im,
                                      const float *x_re,
                                      const float *x_im,
                                      const float *g_re,
                                      const float *g_im,
                                      int n,
                                      int L)
{
  for (int i = 0; i < n; i += 4 * LANES) {
    __m256 r0, r1, r2, r3, i0, i1, i2, i3;
    if (in_re) {
      const float *sr = in_re + i, *si = in_im + i;
      r0 = _mm256_loadu_ps(sr), r1 = _mm256_loadu_ps(sr + LANES);
      r2 = _mm256_loadu_ps(sr + 2 * LANES), r3 = _mm256_loadu_ps(sr + 3 * LANES);
      i0 = _mm256_loadu_ps(si), i1 = _mm256_loadu_ps(si + LANES);
      i2 = _mm256_loadu_ps(si + 2 * LANES), i3 = _mm256_loadu_ps(si + 3 * LANES);
    } else {
      r0 = r1 = r2 = r3 = i0 = i1 = i2 = i3 = _mm256_setzero_ps();
    }
    const float *xr = x_re + i;
    const float *xi = x_im + i;
    for (int k = 0; k < L; k++) {
      const __m256 hr = _mm256_set1_ps(g_re[k]);
      const __m256 hi = _mm256_set1_ps(g_im[k]);
      __m256 a, b;
      a = _mm256_loadu_ps(xr + k);
      b = _mm256_loadu_ps(xi + k);
      r0 = _mm256_fnmadd_ps(b, hi, _mm256_fmadd_ps(a, hr, r0));
      i0 = _mm256_fmadd_ps(b, hr, _mm256_fmadd_ps(a, hi, i0));
      a = _mm256_loadu_ps(xr + k + LANES);
      b = _mm256_loadu_ps(xi + k + LANES);
      r1 = _mm256_fnmadd_ps(b, hi, _mm256_fmadd_ps(a, hr, r1));
      i1 = _mm256_fmadd_ps(b, hr, _mm256_fmadd_ps(a, hi, i1));
      a = _mm256_loadu_ps(xr + k + 2 * LANES);
      b = _mm256_loadu_ps(xi + k + 2 * LANES);
      r2 = _mm256_fnmadd_ps(b, hi, _mm256_fmadd_ps(a, hr, r2));
      i2 = _mm256_fmadd_ps(b, hr, _mm256_fmadd_ps(a, hi, i2));
      a = _mm256_loadu_ps(xr + k + 3 * LANES);
      b = _mm256_loadu_ps(xi + k + 3 * LANES);
      r3 = _mm256_fnmadd_ps(b, hi, _mm256_fmadd_ps(a, hr, r3));
      i3 = _mm256_fmadd_ps(b, hr, _mm256_fmadd_ps(a, hi, i3));
    }
    float *ar = acc_re + i;
    float *ai = acc_im + i;
    _mm256_storeu_ps(ar, r0);
    _mm256_storeu_ps(ar + LANES, r1);
    _mm256_storeu_ps(ar + 2 * LANES, r2);
    _mm256_storeu_ps(ar + 3 * LANES, r3);
    _mm256_storeu_ps(ai, i0);
    _mm256_storeu_ps(ai + LANES, i1);
    _mm256_storeu_ps(ai + 2 * LANES, i2);
    _mm256_storeu_ps(ai + 3 * LANES, i3);
  }
}

void channel_pipeline_freq_mac_avx2(float *y_re, float *y_im, const float *X, const float *H, int N, int nb_tx)
{
  const size_t xstride = (size_t)2 * N * LANES;
  for (int n = 0; n < N; n++) {
    __m256 yr = _mm256_setzero_ps(), yi = _mm256_setzero_ps();
    for (int t = 0; t < nb_tx; t++) {
      const float *xr = X + t * xstride + (size_t)n * LANES;
      const float *xi = xr + (size_t)N * LANES;
      const __m256 ar = _mm256_loadu_ps(xr), ai = _mm256_loadu_ps(xi);
      const __m256 hr = _mm256_set1_ps(H[(size_t)t * 2 * N + n]);
      const __m256 hi = _mm256_set1_ps(H[(size_t)t * 2 * N + N + n]);
      yr = _mm256_fnmadd_ps(ai, hi, _mm256_fmadd_ps(ar, hr, yr));
      yi = _mm256_fmadd_ps(ai, hr, _mm256_fmadd_ps(ar, hi, yi));
    }
    _mm256_storeu_ps(y_re + (size_t)n * LANES, yr);
    _mm256_storeu_ps(y_im + (size_t)n * LANES, yi);
  }
}

void channel_pipeline_gather_c16_avx2(float *re, float *im, const c16_t *src, int stride, int npts)
{
  int n = 0;
  // 8x8 blocks through an in-register transpose (hardware gathers are slow)
  for (; n + LANES <= npts; n += LANES) {
    __m256i m[8];
    for (int b = 0; b < 8; b++)
      m[b] = _mm256_loadu_si256((const __m256i *)(src + n + (size_t)b * stride));
    transpose8(m);
    for (int j = 0; j < 8; j++) {
      __m256 r, i;
      unpack_c16(m[j], &r, &i);
      _mm256_storeu_ps(re + (size_t)(n + j) * LANES, r);
      _mm256_storeu_ps(im + (size_t)(n + j) * LANES, i);
    }
  }
  const __m256i idx = _mm256_mullo_epi32(_mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7), _mm256_set1_epi32(stride));
  for (; n < npts; n++) {
    __m256 r, i;
    unpack_c16(_mm256_i32gather_epi32((const int *)(src + n), idx, 4), &r, &i);
    _mm256_storeu_ps(re + (size_t)n * LANES, r);
    _mm256_storeu_ps(im + (size_t)n * LANES, i);
  }
}

void channel_pipeline_scatter_c16_avx2(c16_t *dst, int stride, const float *re, const float *im, int npts)
{
  int n = 0;
  for (; n + LANES <= npts; n += LANES) {
    __m256i m[8];
    for (int j = 0; j < 8; j++)
      m[j] = pack_c16(_mm256_loadu_ps(re + (size_t)(n + j) * LANES), _mm256_loadu_ps(im + (size_t)(n + j) * LANES));
    transpose8(m);
    for (int b = 0; b < 8; b++)
      _mm256_storeu_si256((__m256i *)(dst + n + (size_t)b * stride), m[b]);
  }
  // AVX2 has no scatter: pack in registers, then 8 scalar stores.
  for (; n < npts; n++) {
    int32_t t[8];
    _mm256_storeu_si256((__m256i *)t, pack_c16(_mm256_loadu_ps(re + (size_t)n * LANES), _mm256_loadu_ps(im + (size_t)n * LANES)));
    int32_t *d = (int32_t *)(dst + n);
    for (int b = 0; b < 8; b++)
      d[b * stride] = t[b];
  }
}

#endif
