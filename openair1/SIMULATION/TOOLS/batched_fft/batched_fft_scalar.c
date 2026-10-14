/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Portable batched_fft kernels, one transform per call. The SIMD files implement the same
// algorithm with B lanes per point; see batched_fft.h for the layout and transform definitions.

#include <stddef.h>
#include "batched_fft.h"

// o = x * w and o = x * conj(w)
static inline void cmul(float xr, float xi, float wr, float wi, float * or, float *oi)
{
  const float t = xr * wr;
  *oi = xi * wr + xr * wi;
  * or = t - xi * wi;
}

static inline void cmul_conj(float xr, float xi, float wr, float wi, float * or, float *oi)
{
  const float t = xr * wr;
  *oi = xi * wr - xr * wi;
  * or = t + xi * wi;
}

// Twiddle-free radix-2 pass on consecutive pairs (last DIF stage / first DIT stage).
static void radix2_pass(float *re, float *im, int n)
{
  for (int s = 0; s < n; s += 2) {
    const float ar = re[s], ai = im[s], br = re[s + 1], bi = im[s + 1];
    re[s] = ar + br;
    im[s] = ai + bi;
    re[s + 1] = ar - br;
    im[s + 1] = ai - bi;
  }
}

// DIF stages len and len / 2 fused (radix-2^2) over n points: pairs (0, 2) at w^j and (1, 3) at
// w^(j + q) for stage len, then (0, 1) and (2, 3) at w'^j for stage len / 2. N is the full transform
// size the twiddles are indexed for.
static void dif_pass(float *re, float *im, int n, int len, int N, const float *tw)
{
  const float *tw_im = tw + N / 2;
  const int tstride = N / len;
  const int q = len >> 2;
  for (int j = 0; j < q; j++) {
    const float w1r = tw[j * tstride], w1i = tw_im[j * tstride];
    const float w2r = tw[(j + q) * tstride], w2i = tw_im[(j + q) * tstride];
    const float w3r = tw[2 * j * tstride], w3i = tw_im[2 * j * tstride];
    for (int s = j; s < n; s += len) {
      float *pr = re + s, *pi = im + s;
      const float ar = pr[0], ai = pi[0];
      const float br = pr[q], bi = pi[q];
      const float cr = pr[2 * q], ci = pi[2 * q];
      const float dr = pr[3 * q], di = pi[3 * q];
      const float a1r = ar + cr, a1i = ai + ci;
      const float b1r = br + dr, b1i = bi + di;
      float c1r, c1i, d1r, d1i;
      cmul(ar - cr, ai - ci, w1r, w1i, &c1r, &c1i);
      cmul(br - dr, bi - di, w2r, w2i, &d1r, &d1i);
      pr[0] = a1r + b1r;
      pi[0] = a1i + b1i;
      pr[2 * q] = c1r + d1r;
      pi[2 * q] = c1i + d1i;
      cmul(a1r - b1r, a1i - b1i, w3r, w3i, &pr[q], &pi[q]);
      cmul(c1r - d1r, c1i - d1i, w3r, w3i, &pr[3 * q], &pi[3 * q]);
    }
  }
}

// Inverse DIT stages len and 2 len fused over n points, conjugate twiddles: pairs (0, 1) and (2, 3)
// at w^j for stage len, then (0, 2) at w'^j and (1, 3) at w'^(j + h) for stage 2 len.
static void dit_pass(float *re, float *im, int n, int len, int N, const float *tw)
{
  const float *tw_im = tw + N / 2;
  const int h = len >> 1;
  const int ts1 = N / len, ts2 = N / (2 * len);
  for (int j = 0; j < h; j++) {
    const float w1r = tw[j * ts1], w1i = tw_im[j * ts1];
    const float w2r = tw[j * ts2], w2i = tw_im[j * ts2];
    const float w3r = tw[(j + h) * ts2], w3i = tw_im[(j + h) * ts2];
    for (int s = j; s < n; s += 2 * len) {
      float *pr = re + s, *pi = im + s;
      const float ar = pr[0], ai = pi[0];
      const float br = pr[h], bi = pi[h];
      const float cr = pr[2 * h], ci = pi[2 * h];
      const float dr = pr[3 * h], di = pi[3 * h];
      float tr, ti;
      cmul_conj(br, bi, w1r, w1i, &tr, &ti);
      const float a1r = ar + tr, a1i = ai + ti, b1r = ar - tr, b1i = ai - ti;
      cmul_conj(dr, di, w1r, w1i, &tr, &ti);
      const float c1r = cr + tr, c1i = ci + ti, d1r = cr - tr, d1i = ci - ti;
      cmul_conj(c1r, c1i, w2r, w2i, &tr, &ti);
      pr[0] = a1r + tr;
      pi[0] = a1i + ti;
      pr[2 * h] = a1r - tr;
      pi[2 * h] = a1i - ti;
      cmul_conj(d1r, d1i, w3r, w3i, &tr, &ti);
      pr[h] = b1r + tr;
      pi[h] = b1i + ti;
      pr[3 * h] = b1r - tr;
      pi[3 * h] = b1i - ti;
    }
  }
}

// Forward DIF FFT of the n-point sub-transform at re/im (n = N at the top), natural order in,
// bit-reversed order out.
static void dif_recursive(float *re, float *im, int n, int N, const float *tw)
{
  if (n > BATCHED_FFT_LEAF_FLOATS && n >= 16) {
    dif_pass(re, im, n, n, N, tw);
    const int quarter = n / 4;
    for (int k = 0; k < 4; k++)
      dif_recursive(re + k * quarter, im + k * quarter, quarter, N, tw);
    return;
  }
  int len = n;
  for (; len >= 4; len >>= 2)
    dif_pass(re, im, n, len, N, tw);
  if (len == 2)
    radix2_pass(re, im, n);
}

// Inverse DIT FFT (unscaled) of the n-point sub-transform: bit-reversed order in, natural out.
static void dit_recursive(float *re, float *im, int n, int N, const float *tw)
{
  if (n > BATCHED_FFT_LEAF_FLOATS && n >= 16) {
    const int quarter = n / 4;
    for (int k = 0; k < 4; k++)
      dit_recursive(re + k * quarter, im + k * quarter, quarter, N, tw);
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

void batched_fft_forward_scalar(float *re, float *im, int N, const float *tw)
{
  dif_recursive(re, im, N, N, tw);
}

void batched_fft_inverse_scalar(float *re, float *im, int N, const float *tw)
{
  dit_recursive(re, im, N, N, tw);
}
