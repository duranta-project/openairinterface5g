/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// AVX512F batched_fft kernels: 16 transforms per call, one per lane. Same algorithm as
// batched_fft_scalar.c. Built with -mavx512f regardless of the global flags; only called when
// batched_fft_isa_supported() says so.

#if defined(__x86_64__)

#include <stddef.h>
#include <immintrin.h>
#include "batched_fft.h"

#define LANES 16

// o = x * w and o = x * conj(w)
static inline void cmul(__m512 xr, __m512 xi, __m512 wr, __m512 wi, __m512 * or, __m512 *oi)
{
  const __m512 t = _mm512_mul_ps(xr, wr);
  *oi = _mm512_fmadd_ps(xr, wi, _mm512_mul_ps(xi, wr));
  * or = _mm512_fnmadd_ps(xi, wi, t);
}

static inline void cmul_conj(__m512 xr, __m512 xi, __m512 wr, __m512 wi, __m512 * or, __m512 *oi)
{
  const __m512 t = _mm512_mul_ps(xr, wr);
  *oi = _mm512_fnmadd_ps(xr, wi, _mm512_mul_ps(xi, wr));
  * or = _mm512_fmadd_ps(xi, wi, t);
}

static void radix2_pass(float *re, float *im, int n)
{
  for (int s = 0; s < n; s += 2) {
    float *pr = re + (size_t)s * LANES, *pi = im + (size_t)s * LANES;
    const __m512 ar = _mm512_loadu_ps(pr), ai = _mm512_loadu_ps(pi);
    const __m512 br = _mm512_loadu_ps(pr + LANES), bi = _mm512_loadu_ps(pi + LANES);
    _mm512_storeu_ps(pr, _mm512_add_ps(ar, br));
    _mm512_storeu_ps(pi, _mm512_add_ps(ai, bi));
    _mm512_storeu_ps(pr + LANES, _mm512_sub_ps(ar, br));
    _mm512_storeu_ps(pi + LANES, _mm512_sub_ps(ai, bi));
  }
}

static void dif_pass(float *re, float *im, int n, int len, int N, const float *tw)
{
  const float *tw_im = tw + N / 2;
  const int tstride = N / len;
  const int q = len >> 2;
  const size_t d = (size_t)q * LANES;
  for (int j = 0; j < q; j++) {
    const __m512 w1r = _mm512_set1_ps(tw[j * tstride]), w1i = _mm512_set1_ps(tw_im[j * tstride]);
    const __m512 w2r = _mm512_set1_ps(tw[(j + q) * tstride]), w2i = _mm512_set1_ps(tw_im[(j + q) * tstride]);
    const __m512 w3r = _mm512_set1_ps(tw[2 * j * tstride]), w3i = _mm512_set1_ps(tw_im[2 * j * tstride]);
    for (int s = j; s < n; s += len) {
      float *pr = re + (size_t)s * LANES, *pi = im + (size_t)s * LANES;
      const __m512 ar = _mm512_loadu_ps(pr), ai = _mm512_loadu_ps(pi);
      const __m512 br = _mm512_loadu_ps(pr + d), bi = _mm512_loadu_ps(pi + d);
      const __m512 cr = _mm512_loadu_ps(pr + 2 * d), ci = _mm512_loadu_ps(pi + 2 * d);
      const __m512 dr = _mm512_loadu_ps(pr + 3 * d), di = _mm512_loadu_ps(pi + 3 * d);
      const __m512 a1r = _mm512_add_ps(ar, cr), a1i = _mm512_add_ps(ai, ci);
      const __m512 b1r = _mm512_add_ps(br, dr), b1i = _mm512_add_ps(bi, di);
      __m512 c1r, c1i, d1r, d1i, tr, ti;
      cmul(_mm512_sub_ps(ar, cr), _mm512_sub_ps(ai, ci), w1r, w1i, &c1r, &c1i);
      cmul(_mm512_sub_ps(br, dr), _mm512_sub_ps(bi, di), w2r, w2i, &d1r, &d1i);
      _mm512_storeu_ps(pr, _mm512_add_ps(a1r, b1r));
      _mm512_storeu_ps(pi, _mm512_add_ps(a1i, b1i));
      _mm512_storeu_ps(pr + 2 * d, _mm512_add_ps(c1r, d1r));
      _mm512_storeu_ps(pi + 2 * d, _mm512_add_ps(c1i, d1i));
      cmul(_mm512_sub_ps(a1r, b1r), _mm512_sub_ps(a1i, b1i), w3r, w3i, &tr, &ti);
      _mm512_storeu_ps(pr + d, tr);
      _mm512_storeu_ps(pi + d, ti);
      cmul(_mm512_sub_ps(c1r, d1r), _mm512_sub_ps(c1i, d1i), w3r, w3i, &tr, &ti);
      _mm512_storeu_ps(pr + 3 * d, tr);
      _mm512_storeu_ps(pi + 3 * d, ti);
    }
  }
}

static void dit_pass(float *re, float *im, int n, int len, int N, const float *tw)
{
  const float *tw_im = tw + N / 2;
  const int h = len >> 1;
  const size_t d = (size_t)h * LANES;
  const int ts1 = N / len, ts2 = N / (2 * len);
  for (int j = 0; j < h; j++) {
    const __m512 w1r = _mm512_set1_ps(tw[j * ts1]), w1i = _mm512_set1_ps(tw_im[j * ts1]);
    const __m512 w2r = _mm512_set1_ps(tw[j * ts2]), w2i = _mm512_set1_ps(tw_im[j * ts2]);
    const __m512 w3r = _mm512_set1_ps(tw[(j + h) * ts2]), w3i = _mm512_set1_ps(tw_im[(j + h) * ts2]);
    for (int s = j; s < n; s += 2 * len) {
      float *pr = re + (size_t)s * LANES, *pi = im + (size_t)s * LANES;
      const __m512 ar = _mm512_loadu_ps(pr), ai = _mm512_loadu_ps(pi);
      const __m512 br = _mm512_loadu_ps(pr + d), bi = _mm512_loadu_ps(pi + d);
      const __m512 cr = _mm512_loadu_ps(pr + 2 * d), ci = _mm512_loadu_ps(pi + 2 * d);
      const __m512 dr = _mm512_loadu_ps(pr + 3 * d), di = _mm512_loadu_ps(pi + 3 * d);
      __m512 tr, ti;
      cmul_conj(br, bi, w1r, w1i, &tr, &ti);
      const __m512 a1r = _mm512_add_ps(ar, tr), a1i = _mm512_add_ps(ai, ti);
      const __m512 b1r = _mm512_sub_ps(ar, tr), b1i = _mm512_sub_ps(ai, ti);
      cmul_conj(dr, di, w1r, w1i, &tr, &ti);
      const __m512 c1r = _mm512_add_ps(cr, tr), c1i = _mm512_add_ps(ci, ti);
      const __m512 d1r = _mm512_sub_ps(cr, tr), d1i = _mm512_sub_ps(ci, ti);
      cmul_conj(c1r, c1i, w2r, w2i, &tr, &ti);
      _mm512_storeu_ps(pr, _mm512_add_ps(a1r, tr));
      _mm512_storeu_ps(pi, _mm512_add_ps(a1i, ti));
      _mm512_storeu_ps(pr + 2 * d, _mm512_sub_ps(a1r, tr));
      _mm512_storeu_ps(pi + 2 * d, _mm512_sub_ps(a1i, ti));
      cmul_conj(d1r, d1i, w3r, w3i, &tr, &ti);
      _mm512_storeu_ps(pr + d, _mm512_add_ps(b1r, tr));
      _mm512_storeu_ps(pi + d, _mm512_add_ps(b1i, ti));
      _mm512_storeu_ps(pr + 3 * d, _mm512_sub_ps(b1r, tr));
      _mm512_storeu_ps(pi + 3 * d, _mm512_sub_ps(b1i, ti));
    }
  }
}

static void dif_recursive(float *re, float *im, int n, int N, const float *tw)
{
  if ((size_t)n * LANES > BATCHED_FFT_LEAF_FLOATS && n >= 16) {
    dif_pass(re, im, n, n, N, tw);
    const size_t quarter = (size_t)(n / 4) * LANES;
    for (int k = 0; k < 4; k++)
      dif_recursive(re + k * quarter, im + k * quarter, n / 4, N, tw);
    return;
  }
  int len = n;
  for (; len >= 4; len >>= 2)
    dif_pass(re, im, n, len, N, tw);
  if (len == 2)
    radix2_pass(re, im, n);
}

static void dit_recursive(float *re, float *im, int n, int N, const float *tw)
{
  if ((size_t)n * LANES > BATCHED_FFT_LEAF_FLOATS && n >= 16) {
    const size_t quarter = (size_t)(n / 4) * LANES;
    for (int k = 0; k < 4; k++)
      dit_recursive(re + k * quarter, im + k * quarter, n / 4, N, tw);
    dit_pass(re, im, n, n / 2, N, tw);
    return;
  }
  int len = 2;
  if (__builtin_ctz(n) & 1) {
    radix2_pass(re, im, n);
    len = 4;
  }
  for (; 2 * len <= n; len <<= 2)
    dit_pass(re, im, n, len, N, tw);
}

void batched_fft_forward_avx512(float *re, float *im, int N, const float *tw)
{
  dif_recursive(re, im, N, N, tw);
}

void batched_fft_inverse_avx512(float *re, float *im, int N, const float *tw)
{
  dit_recursive(re, im, N, N, tw);
}

#endif
