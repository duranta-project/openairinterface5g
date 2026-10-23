/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// SVE2 batched_fft kernels, vector-length agnostic: svcntw() transforms per call, one per lane.
// Same algorithm as batched_fft_scalar.c. Built with GCC's target("+sve2") when the baseline has no
// SVE2; only called when batched_fft_isa_supported() says so.

#include <stddef.h>
#include "batched_fft.h"

#if defined(BATCHED_FFT_HAVE_SVE2)

#if !defined(__ARM_FEATURE_SVE2)
#pragma GCC target("+sve2")
#endif
#include <arm_sve.h>

// o = x * w and o = x * conj(w)
static inline void cmul(svfloat32_t xr, svfloat32_t xi, svfloat32_t wr, svfloat32_t wi, svfloat32_t * or, svfloat32_t *oi)
{
  const svbool_t pg = svptrue_b32();
  const svfloat32_t t = svmul_f32_x(pg, xr, wr);
  *oi = svmla_f32_x(pg, svmul_f32_x(pg, xi, wr), xr, wi);
  * or = svmls_f32_x(pg, t, xi, wi);
}

static inline void cmul_conj(svfloat32_t xr, svfloat32_t xi, svfloat32_t wr, svfloat32_t wi, svfloat32_t * or, svfloat32_t *oi)
{
  const svbool_t pg = svptrue_b32();
  const svfloat32_t t = svmul_f32_x(pg, xr, wr);
  *oi = svmls_f32_x(pg, svmul_f32_x(pg, xi, wr), xr, wi);
  * or = svmla_f32_x(pg, t, xi, wi);
}

static void radix2_pass(float *re, float *im, int n)
{
  const svbool_t pg = svptrue_b32();
  const int lanes = svcntw();
  for (int s = 0; s < n; s += 2) {
    float *pr = re + (size_t)s * lanes, *pi = im + (size_t)s * lanes;
    const svfloat32_t ar = svld1_f32(pg, pr), ai = svld1_f32(pg, pi);
    const svfloat32_t br = svld1_f32(pg, pr + lanes), bi = svld1_f32(pg, pi + lanes);
    svst1_f32(pg, pr, svadd_f32_x(pg, ar, br));
    svst1_f32(pg, pi, svadd_f32_x(pg, ai, bi));
    svst1_f32(pg, pr + lanes, svsub_f32_x(pg, ar, br));
    svst1_f32(pg, pi + lanes, svsub_f32_x(pg, ai, bi));
  }
}

static void dif_pass(float *re, float *im, int n, int len, int N, const float *tw)
{
  const svbool_t pg = svptrue_b32();
  const int lanes = svcntw();
  const float *tw_im = tw + N / 2;
  const int tstride = N / len;
  const int q = len >> 2;
  const size_t d = (size_t)q * lanes;
  for (int j = 0; j < q; j++) {
    const svfloat32_t w1r = svdup_n_f32(tw[j * tstride]), w1i = svdup_n_f32(tw_im[j * tstride]);
    const svfloat32_t w2r = svdup_n_f32(tw[(j + q) * tstride]), w2i = svdup_n_f32(tw_im[(j + q) * tstride]);
    const svfloat32_t w3r = svdup_n_f32(tw[2 * j * tstride]), w3i = svdup_n_f32(tw_im[2 * j * tstride]);
    for (int s = j; s < n; s += len) {
      float *pr = re + (size_t)s * lanes, *pi = im + (size_t)s * lanes;
      const svfloat32_t ar = svld1_f32(pg, pr), ai = svld1_f32(pg, pi);
      const svfloat32_t br = svld1_f32(pg, pr + d), bi = svld1_f32(pg, pi + d);
      const svfloat32_t cr = svld1_f32(pg, pr + 2 * d), ci = svld1_f32(pg, pi + 2 * d);
      const svfloat32_t dr = svld1_f32(pg, pr + 3 * d), di = svld1_f32(pg, pi + 3 * d);
      const svfloat32_t a1r = svadd_f32_x(pg, ar, cr), a1i = svadd_f32_x(pg, ai, ci);
      const svfloat32_t b1r = svadd_f32_x(pg, br, dr), b1i = svadd_f32_x(pg, bi, di);
      svfloat32_t c1r, c1i, d1r, d1i, tr, ti;
      cmul(svsub_f32_x(pg, ar, cr), svsub_f32_x(pg, ai, ci), w1r, w1i, &c1r, &c1i);
      cmul(svsub_f32_x(pg, br, dr), svsub_f32_x(pg, bi, di), w2r, w2i, &d1r, &d1i);
      svst1_f32(pg, pr, svadd_f32_x(pg, a1r, b1r));
      svst1_f32(pg, pi, svadd_f32_x(pg, a1i, b1i));
      svst1_f32(pg, pr + 2 * d, svadd_f32_x(pg, c1r, d1r));
      svst1_f32(pg, pi + 2 * d, svadd_f32_x(pg, c1i, d1i));
      cmul(svsub_f32_x(pg, a1r, b1r), svsub_f32_x(pg, a1i, b1i), w3r, w3i, &tr, &ti);
      svst1_f32(pg, pr + d, tr);
      svst1_f32(pg, pi + d, ti);
      cmul(svsub_f32_x(pg, c1r, d1r), svsub_f32_x(pg, c1i, d1i), w3r, w3i, &tr, &ti);
      svst1_f32(pg, pr + 3 * d, tr);
      svst1_f32(pg, pi + 3 * d, ti);
    }
  }
}

static void dit_pass(float *re, float *im, int n, int len, int N, const float *tw)
{
  const svbool_t pg = svptrue_b32();
  const int lanes = svcntw();
  const float *tw_im = tw + N / 2;
  const int h = len >> 1;
  const size_t d = (size_t)h * lanes;
  const int ts1 = N / len, ts2 = N / (2 * len);
  for (int j = 0; j < h; j++) {
    const svfloat32_t w1r = svdup_n_f32(tw[j * ts1]), w1i = svdup_n_f32(tw_im[j * ts1]);
    const svfloat32_t w2r = svdup_n_f32(tw[j * ts2]), w2i = svdup_n_f32(tw_im[j * ts2]);
    const svfloat32_t w3r = svdup_n_f32(tw[(j + h) * ts2]), w3i = svdup_n_f32(tw_im[(j + h) * ts2]);
    for (int s = j; s < n; s += 2 * len) {
      float *pr = re + (size_t)s * lanes, *pi = im + (size_t)s * lanes;
      const svfloat32_t ar = svld1_f32(pg, pr), ai = svld1_f32(pg, pi);
      const svfloat32_t br = svld1_f32(pg, pr + d), bi = svld1_f32(pg, pi + d);
      const svfloat32_t cr = svld1_f32(pg, pr + 2 * d), ci = svld1_f32(pg, pi + 2 * d);
      const svfloat32_t dr = svld1_f32(pg, pr + 3 * d), di = svld1_f32(pg, pi + 3 * d);
      svfloat32_t tr, ti;
      cmul_conj(br, bi, w1r, w1i, &tr, &ti);
      const svfloat32_t a1r = svadd_f32_x(pg, ar, tr), a1i = svadd_f32_x(pg, ai, ti);
      const svfloat32_t b1r = svsub_f32_x(pg, ar, tr), b1i = svsub_f32_x(pg, ai, ti);
      cmul_conj(dr, di, w1r, w1i, &tr, &ti);
      const svfloat32_t c1r = svadd_f32_x(pg, cr, tr), c1i = svadd_f32_x(pg, ci, ti);
      const svfloat32_t d1r = svsub_f32_x(pg, cr, tr), d1i = svsub_f32_x(pg, ci, ti);
      cmul_conj(c1r, c1i, w2r, w2i, &tr, &ti);
      svst1_f32(pg, pr, svadd_f32_x(pg, a1r, tr));
      svst1_f32(pg, pi, svadd_f32_x(pg, a1i, ti));
      svst1_f32(pg, pr + 2 * d, svsub_f32_x(pg, a1r, tr));
      svst1_f32(pg, pi + 2 * d, svsub_f32_x(pg, a1i, ti));
      cmul_conj(d1r, d1i, w3r, w3i, &tr, &ti);
      svst1_f32(pg, pr + d, svadd_f32_x(pg, b1r, tr));
      svst1_f32(pg, pi + d, svadd_f32_x(pg, b1i, ti));
      svst1_f32(pg, pr + 3 * d, svsub_f32_x(pg, b1r, tr));
      svst1_f32(pg, pi + 3 * d, svsub_f32_x(pg, b1i, ti));
    }
  }
}

static void dif_recursive(float *re, float *im, int n, int N, const float *tw)
{
  const int lanes = svcntw();
  if ((size_t)n * lanes > BATCHED_FFT_LEAF_FLOATS && n >= 16) {
    dif_pass(re, im, n, n, N, tw);
    const size_t quarter = (size_t)(n / 4) * lanes;
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
  const int lanes = svcntw();
  if ((size_t)n * lanes > BATCHED_FFT_LEAF_FLOATS && n >= 16) {
    const size_t quarter = (size_t)(n / 4) * lanes;
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

void batched_fft_forward_sve2(float *re, float *im, int N, const float *tw)
{
  dif_recursive(re, im, N, N, tw);
}

void batched_fft_inverse_sve2(float *re, float *im, int N, const float *tw)
{
  dit_recursive(re, im, N, N, tw);
}

int batched_fft_batch_size_sve2(void)
{
  return svcntw();
}

#endif
