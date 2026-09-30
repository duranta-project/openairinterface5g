/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Portable channel pipeline kernels, one lane. They define what the SIMD versions compute; see
// channel_pipeline_simd.h for the data layouts.

#include <stddef.h>
#include "channel_pipeline_simd.h"

void channel_pipeline_c16_to_split_scalar(float *re, float *im, const c16_t *src, int n)
{
  for (int i = 0; i < n; i++) {
    re[i] = src[i].r;
    im[i] = src[i].i;
  }
}

void channel_pipeline_split_to_c16_scalar(c16_t *dst, const float *re, const float *im, int n)
{
  for (int i = 0; i < n; i++) {
    dst[i].r = channel_pipeline_round_s16(re[i]);
    dst[i].i = channel_pipeline_round_s16(im[i]);
  }
}

void channel_pipeline_direct_mac_scalar(float *acc_re,
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
  for (int i = 0; i < n; i++) {
    float r = in_re ? in_re[i] : 0.0f;
    float m = in_re ? in_im[i] : 0.0f;
    for (int k = 0; k < L; k++) {
      const float a = x_re[i + k], b = x_im[i + k];
      r = (r + a * g_re[k]) - b * g_im[k];
      m = (m + a * g_im[k]) + b * g_re[k];
    }
    acc_re[i] = r;
    acc_im[i] = m;
  }
}

void channel_pipeline_freq_mac_scalar(float *y_re, float *y_im, const float *X, const float *H, int N, int nb_tx)
{
  for (int n = 0; n < N; n++) {
    float yr = 0.0f, yi = 0.0f;
    for (int t = 0; t < nb_tx; t++) {
      const float ar = X[(size_t)t * 2 * N + n], ai = X[(size_t)t * 2 * N + N + n];
      const float hr = H[(size_t)t * 2 * N + n], hi = H[(size_t)t * 2 * N + N + n];
      yr = (yr + ar * hr) - ai * hi;
      yi = (yi + ar * hi) + ai * hr;
    }
    y_re[n] = yr;
    y_im[n] = yi;
  }
}

// One lane: the lane stride never matters.
void channel_pipeline_gather_c16_scalar(float *re, float *im, const c16_t *src, int stride, int npts)
{
  (void)stride;
  channel_pipeline_c16_to_split_scalar(re, im, src, npts);
}

void channel_pipeline_scatter_c16_scalar(c16_t *dst, int stride, const float *re, const float *im, int npts)
{
  (void)stride;
  channel_pipeline_split_to_c16_scalar(dst, re, im, npts);
}
