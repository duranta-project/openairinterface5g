/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// SVE2 channel pipeline kernels, vector-length agnostic (svcntw() lanes; all vectors are full,
// callers pad to a multiple of 4 * lanes). Built with GCC's target("+sve2") when the baseline has
// no SVE2; only called when channel_pipeline_isa_supported() says so. See channel_pipeline_scalar.c for the
// reference versions and channel_pipeline_simd.h for the data layouts.

#include <stddef.h>
#include "channel_pipeline_simd.h"
#include "batched_fft.h"

#if defined(BATCHED_FFT_HAVE_SVE2)

#if !defined(__ARM_FEATURE_SVE2)
#pragma GCC target("+sve2")
#endif
#include <arm_sve.h>

static inline void unpack_c16(svint32_t v, svfloat32_t *re, svfloat32_t *im)
{
  const svbool_t pg = svptrue_b32();
  *re = svcvt_f32_s32_x(pg, svasr_n_s32_x(pg, svlsl_n_s32_x(pg, v, 16), 16));
  *im = svcvt_f32_s32_x(pg, svasr_n_s32_x(pg, v, 16));
}

// FRINTN rounds to nearest, FCVTZS saturates; SQXTNB/SQXTNT (SVE2) saturate-narrow re into the
// even and im into the odd halfwords, which is exactly the interleaved c16_t layout.
static inline svint32_t pack_c16(svfloat32_t re, svfloat32_t im)
{
  const svbool_t pg = svptrue_b32();
  const svint32_t r = svcvt_s32_f32_x(pg, svrintn_f32_x(pg, re));
  const svint32_t i = svcvt_s32_f32_x(pg, svrintn_f32_x(pg, im));
  return svreinterpret_s32_s16(svqxtnt_s32(svqxtnb_s32(r), i));
}

void channel_pipeline_c16_to_split_sve2(float *re, float *im, const c16_t *src, int n)
{
  const svbool_t pg = svptrue_b32();
  const int lanes = svcntw();
  int i = 0;
  for (; i + lanes <= n; i += lanes) {
    svfloat32_t r, m;
    unpack_c16(svld1_s32(pg, (const int32_t *)(src + i)), &r, &m);
    svst1_f32(pg, re + i, r);
    svst1_f32(pg, im + i, m);
  }
  for (; i < n; i++) {
    re[i] = src[i].r;
    im[i] = src[i].i;
  }
}

void channel_pipeline_split_to_c16_sve2(c16_t *dst, const float *re, const float *im, int n)
{
  const svbool_t pg = svptrue_b32();
  const int lanes = svcntw();
  int i = 0;
  for (; i + lanes <= n; i += lanes)
    svst1_s32(pg, (int32_t *)(dst + i), pack_c16(svld1_f32(pg, re + i), svld1_f32(pg, im + i)));
  for (; i < n; i++) {
    dst[i].r = channel_pipeline_round_s16(re[i]);
    dst[i].i = channel_pipeline_round_s16(im[i]);
  }
}

void channel_pipeline_direct_mac_sve2(float *acc_re,
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
  const svbool_t pg = svptrue_b32();
  const int lanes = svcntw();
  for (int i = 0; i < n; i += 4 * lanes) {
    svfloat32_t r0, r1, r2, r3, i0, i1, i2, i3;
    if (in_re) {
      const float *sr = in_re + i, *si = in_im + i;
      r0 = svld1_f32(pg, sr), r1 = svld1_f32(pg, sr + lanes);
      r2 = svld1_f32(pg, sr + 2 * lanes), r3 = svld1_f32(pg, sr + 3 * lanes);
      i0 = svld1_f32(pg, si), i1 = svld1_f32(pg, si + lanes);
      i2 = svld1_f32(pg, si + 2 * lanes), i3 = svld1_f32(pg, si + 3 * lanes);
    } else {
      r0 = r1 = r2 = r3 = i0 = i1 = i2 = i3 = svdup_n_f32(0.0f);
    }
    const float *xr = x_re + i;
    const float *xi = x_im + i;
    for (int k = 0; k < L; k++) {
      const svfloat32_t hr = svdup_n_f32(g_re[k]);
      const svfloat32_t hi = svdup_n_f32(g_im[k]);
      svfloat32_t a, b;
      a = svld1_f32(pg, xr + k);
      b = svld1_f32(pg, xi + k);
      r0 = svmls_f32_x(pg, svmla_f32_x(pg, r0, a, hr), b, hi);
      i0 = svmla_f32_x(pg, svmla_f32_x(pg, i0, a, hi), b, hr);
      a = svld1_f32(pg, xr + k + lanes);
      b = svld1_f32(pg, xi + k + lanes);
      r1 = svmls_f32_x(pg, svmla_f32_x(pg, r1, a, hr), b, hi);
      i1 = svmla_f32_x(pg, svmla_f32_x(pg, i1, a, hi), b, hr);
      a = svld1_f32(pg, xr + k + 2 * lanes);
      b = svld1_f32(pg, xi + k + 2 * lanes);
      r2 = svmls_f32_x(pg, svmla_f32_x(pg, r2, a, hr), b, hi);
      i2 = svmla_f32_x(pg, svmla_f32_x(pg, i2, a, hi), b, hr);
      a = svld1_f32(pg, xr + k + 3 * lanes);
      b = svld1_f32(pg, xi + k + 3 * lanes);
      r3 = svmls_f32_x(pg, svmla_f32_x(pg, r3, a, hr), b, hi);
      i3 = svmla_f32_x(pg, svmla_f32_x(pg, i3, a, hi), b, hr);
    }
    float *ar = acc_re + i;
    float *ai = acc_im + i;
    svst1_f32(pg, ar, r0);
    svst1_f32(pg, ar + lanes, r1);
    svst1_f32(pg, ar + 2 * lanes, r2);
    svst1_f32(pg, ar + 3 * lanes, r3);
    svst1_f32(pg, ai, i0);
    svst1_f32(pg, ai + lanes, i1);
    svst1_f32(pg, ai + 2 * lanes, i2);
    svst1_f32(pg, ai + 3 * lanes, i3);
  }
}

void channel_pipeline_freq_mac_sve2(float *y_re, float *y_im, const float *X, const float *H, int N, int nb_tx)
{
  const svbool_t pg = svptrue_b32();
  const int lanes = svcntw();
  const size_t xstride = (size_t)2 * N * lanes;
  for (int n = 0; n < N; n++) {
    svfloat32_t yr = svdup_n_f32(0.0f), yi = svdup_n_f32(0.0f);
    for (int t = 0; t < nb_tx; t++) {
      const float *xr = X + t * xstride + (size_t)n * lanes;
      const float *xi = xr + (size_t)N * lanes;
      const svfloat32_t ar = svld1_f32(pg, xr), ai = svld1_f32(pg, xi);
      const svfloat32_t hr = svdup_n_f32(H[(size_t)t * 2 * N + n]);
      const svfloat32_t hi = svdup_n_f32(H[(size_t)t * 2 * N + N + n]);
      yr = svmls_f32_x(pg, svmla_f32_x(pg, yr, ar, hr), ai, hi);
      yi = svmla_f32_x(pg, svmla_f32_x(pg, yi, ar, hi), ai, hr);
    }
    svst1_f32(pg, y_re + (size_t)n * lanes, yr);
    svst1_f32(pg, y_im + (size_t)n * lanes, yi);
  }
}

// SVE gathers/scatters are slower than NEON 4x4 transposes; at 128-bit VL (every SVE2 core so far)
// the SVE and NEON registers are the same width and the layouts identical, so use NEON.
void channel_pipeline_gather_c16_sve2(float *re, float *im, const c16_t *src, int stride, int npts)
{
  const svbool_t pg = svptrue_b32();
  const int lanes = svcntw();
  if (lanes == 4) {
    channel_pipeline_gather_c16_neon(re, im, src, stride, npts);
    return;
  }
  const svint32_t idx = svindex_s32(0, stride);
  for (int n = 0; n < npts; n++) {
    svfloat32_t r, i;
    unpack_c16(svld1_gather_s32index_s32(pg, (const int32_t *)(src + n), idx), &r, &i);
    svst1_f32(pg, re + (size_t)n * lanes, r);
    svst1_f32(pg, im + (size_t)n * lanes, i);
  }
}

void channel_pipeline_scatter_c16_sve2(c16_t *dst, int stride, const float *re, const float *im, int npts)
{
  const svbool_t pg = svptrue_b32();
  const int lanes = svcntw();
  if (lanes == 4) {
    channel_pipeline_scatter_c16_neon(dst, stride, re, im, npts);
    return;
  }
  const svint32_t idx = svindex_s32(0, stride);
  for (int n = 0; n < npts; n++) {
    const svint32_t v = pack_c16(svld1_f32(pg, re + (size_t)n * lanes), svld1_f32(pg, im + (size_t)n * lanes));
    svst1_scatter_s32index_s32(pg, (int32_t *)(dst + n), idx, v);
  }
}

#endif
