/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// AVX512F channel pipeline kernels, 16 lanes (no BW/DQ/VL needed, so any AVX512 CPU runs them).
// Built with -mavx512f regardless of the global flags; only called when channel_pipeline_isa_supported() says
// so. See channel_pipeline_scalar.c for the reference versions and channel_pipeline_simd.h for the
// data layouts.

#if defined(__x86_64__)

#include <stddef.h>
#include <immintrin.h>
#include "channel_pipeline_simd.h"

#define LANES 16

static inline void unpack_c16(__m512i v, __m512 *re, __m512 *im)
{
  *re = _mm512_cvtepi32_ps(_mm512_srai_epi32(_mm512_slli_epi32(v, 16), 16));
  *im = _mm512_cvtepi32_ps(_mm512_srai_epi32(v, 16));
}

static inline __m512i pack_c16(__m512 re, __m512 im)
{
  const __m512 lo = _mm512_set1_ps(-32768.0f), hi = _mm512_set1_ps(32767.0f);
  __m512i r = _mm512_cvtps_epi32(_mm512_min_ps(_mm512_max_ps(re, lo), hi));
  __m512i i = _mm512_cvtps_epi32(_mm512_min_ps(_mm512_max_ps(im, lo), hi));
  return _mm512_or_si512(_mm512_and_si512(r, _mm512_set1_epi32(0xffff)), _mm512_slli_epi32(i, 16));
}

static inline __m512i lane_idx(int stride)
{
  return _mm512_mullo_epi32(_mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15), _mm512_set1_epi32(stride));
}

// 16x16 transpose of 32-bit elements (one c16_t each), AVX512F only.
static inline void transpose16(__m512i r[16])
{
  __m512i t[16];
  for (int i = 0; i < 16; i += 2) {
    t[i] = _mm512_unpacklo_epi32(r[i], r[i + 1]);
    t[i + 1] = _mm512_unpackhi_epi32(r[i], r[i + 1]);
  }
  for (int i = 0; i < 16; i += 4) {
    r[i] = _mm512_unpacklo_epi64(t[i], t[i + 2]);
    r[i + 1] = _mm512_unpackhi_epi64(t[i], t[i + 2]);
    r[i + 2] = _mm512_unpacklo_epi64(t[i + 1], t[i + 3]);
    r[i + 3] = _mm512_unpackhi_epi64(t[i + 1], t[i + 3]);
  }
  // r[4g + k] now holds column k of each 4x4 sub-block of rows 4g..4g+3. Combine 128-bit lanes.
  for (int g = 0; g < 16; g += 8) {
    for (int k = 0; k < 4; k++) {
      t[g + k] = _mm512_shuffle_i32x4(r[g + k], r[g + 4 + k], 0x88);
      t[g + 4 + k] = _mm512_shuffle_i32x4(r[g + k], r[g + 4 + k], 0xdd);
    }
  }
  for (int k = 0; k < 8; k++) {
    r[k] = _mm512_shuffle_i32x4(t[k], t[k + 8], 0x88);
    r[k + 8] = _mm512_shuffle_i32x4(t[k], t[k + 8], 0xdd);
  }
}

void channel_pipeline_c16_to_split_avx512(float *re, float *im, const c16_t *src, int n)
{
  int i = 0;
  for (; i + LANES <= n; i += LANES) {
    __m512 r, m;
    unpack_c16(_mm512_loadu_si512((const void *)(src + i)), &r, &m);
    _mm512_storeu_ps(re + i, r);
    _mm512_storeu_ps(im + i, m);
  }
  for (; i < n; i++) {
    re[i] = src[i].r;
    im[i] = src[i].i;
  }
}

void channel_pipeline_split_to_c16_avx512(c16_t *dst, const float *re, const float *im, int n)
{
  int i = 0;
  for (; i + LANES <= n; i += LANES)
    _mm512_storeu_si512((void *)(dst + i), pack_c16(_mm512_loadu_ps(re + i), _mm512_loadu_ps(im + i)));
  for (; i < n; i++) {
    dst[i].r = channel_pipeline_round_s16(re[i]);
    dst[i].i = channel_pipeline_round_s16(im[i]);
  }
}

void channel_pipeline_direct_mac_avx512(float *acc_re,
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
    __m512 r0, r1, r2, r3, i0, i1, i2, i3;
    if (in_re) {
      const float *sr = in_re + i, *si = in_im + i;
      r0 = _mm512_loadu_ps(sr), r1 = _mm512_loadu_ps(sr + LANES);
      r2 = _mm512_loadu_ps(sr + 2 * LANES), r3 = _mm512_loadu_ps(sr + 3 * LANES);
      i0 = _mm512_loadu_ps(si), i1 = _mm512_loadu_ps(si + LANES);
      i2 = _mm512_loadu_ps(si + 2 * LANES), i3 = _mm512_loadu_ps(si + 3 * LANES);
    } else {
      r0 = r1 = r2 = r3 = i0 = i1 = i2 = i3 = _mm512_setzero_ps();
    }
    const float *xr = x_re + i;
    const float *xi = x_im + i;
    for (int k = 0; k < L; k++) {
      const __m512 hr = _mm512_set1_ps(g_re[k]);
      const __m512 hi = _mm512_set1_ps(g_im[k]);
      __m512 a, b;
      a = _mm512_loadu_ps(xr + k);
      b = _mm512_loadu_ps(xi + k);
      r0 = _mm512_fnmadd_ps(b, hi, _mm512_fmadd_ps(a, hr, r0));
      i0 = _mm512_fmadd_ps(b, hr, _mm512_fmadd_ps(a, hi, i0));
      a = _mm512_loadu_ps(xr + k + LANES);
      b = _mm512_loadu_ps(xi + k + LANES);
      r1 = _mm512_fnmadd_ps(b, hi, _mm512_fmadd_ps(a, hr, r1));
      i1 = _mm512_fmadd_ps(b, hr, _mm512_fmadd_ps(a, hi, i1));
      a = _mm512_loadu_ps(xr + k + 2 * LANES);
      b = _mm512_loadu_ps(xi + k + 2 * LANES);
      r2 = _mm512_fnmadd_ps(b, hi, _mm512_fmadd_ps(a, hr, r2));
      i2 = _mm512_fmadd_ps(b, hr, _mm512_fmadd_ps(a, hi, i2));
      a = _mm512_loadu_ps(xr + k + 3 * LANES);
      b = _mm512_loadu_ps(xi + k + 3 * LANES);
      r3 = _mm512_fnmadd_ps(b, hi, _mm512_fmadd_ps(a, hr, r3));
      i3 = _mm512_fmadd_ps(b, hr, _mm512_fmadd_ps(a, hi, i3));
    }
    float *ar = acc_re + i;
    float *ai = acc_im + i;
    _mm512_storeu_ps(ar, r0);
    _mm512_storeu_ps(ar + LANES, r1);
    _mm512_storeu_ps(ar + 2 * LANES, r2);
    _mm512_storeu_ps(ar + 3 * LANES, r3);
    _mm512_storeu_ps(ai, i0);
    _mm512_storeu_ps(ai + LANES, i1);
    _mm512_storeu_ps(ai + 2 * LANES, i2);
    _mm512_storeu_ps(ai + 3 * LANES, i3);
  }
}

void channel_pipeline_freq_mac_avx512(float *y_re, float *y_im, const float *X, const float *H, int N, int nb_tx)
{
  const size_t xstride = (size_t)2 * N * LANES;
  for (int n = 0; n < N; n++) {
    __m512 yr = _mm512_setzero_ps(), yi = _mm512_setzero_ps();
    for (int t = 0; t < nb_tx; t++) {
      const float *xr = X + t * xstride + (size_t)n * LANES;
      const float *xi = xr + (size_t)N * LANES;
      const __m512 ar = _mm512_loadu_ps(xr), ai = _mm512_loadu_ps(xi);
      const __m512 hr = _mm512_set1_ps(H[(size_t)t * 2 * N + n]);
      const __m512 hi = _mm512_set1_ps(H[(size_t)t * 2 * N + N + n]);
      yr = _mm512_fnmadd_ps(ai, hi, _mm512_fmadd_ps(ar, hr, yr));
      yi = _mm512_fmadd_ps(ai, hr, _mm512_fmadd_ps(ar, hi, yi));
    }
    _mm512_storeu_ps(y_re + (size_t)n * LANES, yr);
    _mm512_storeu_ps(y_im + (size_t)n * LANES, yi);
  }
}

void channel_pipeline_gather_c16_avx512(float *re, float *im, const c16_t *src, int stride, int npts)
{
  int n = 0;
  // 16x16 blocks through an in-register transpose (hardware gathers are slow)
  for (; n + LANES <= npts; n += LANES) {
    __m512i m[16];
    for (int b = 0; b < 16; b++)
      m[b] = _mm512_loadu_si512((const void *)(src + n + (size_t)b * stride));
    transpose16(m);
    for (int j = 0; j < 16; j++) {
      __m512 r, i;
      unpack_c16(m[j], &r, &i);
      _mm512_storeu_ps(re + (size_t)(n + j) * LANES, r);
      _mm512_storeu_ps(im + (size_t)(n + j) * LANES, i);
    }
  }
  const __m512i idx = lane_idx(stride);
  for (; n < npts; n++) {
    __m512 r, i;
    unpack_c16(_mm512_i32gather_epi32(idx, (const void *)(src + n), 4), &r, &i);
    _mm512_storeu_ps(re + (size_t)n * LANES, r);
    _mm512_storeu_ps(im + (size_t)n * LANES, i);
  }
}

void channel_pipeline_scatter_c16_avx512(c16_t *dst, int stride, const float *re, const float *im, int npts)
{
  int n = 0;
  for (; n + LANES <= npts; n += LANES) {
    __m512i m[16];
    for (int j = 0; j < 16; j++)
      m[j] = pack_c16(_mm512_loadu_ps(re + (size_t)(n + j) * LANES), _mm512_loadu_ps(im + (size_t)(n + j) * LANES));
    transpose16(m);
    for (int b = 0; b < 16; b++)
      _mm512_storeu_si512((void *)(dst + n + (size_t)b * stride), m[b]);
  }
  const __m512i idx = lane_idx(stride);
  for (; n < npts; n++)
    _mm512_i32scatter_epi32((void *)(dst + n),
                            idx,
                            pack_c16(_mm512_loadu_ps(re + (size_t)n * LANES), _mm512_loadu_ps(im + (size_t)n * LANES)),
                            4);
}

#endif
