/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Work units of the CPU channel pipeline and ISA selection. The kernels
// are in channel_pipeline_<isa>.c; the FFT is batched_fft.

#include <stddef.h>
#include <string.h>
#include "channel_pipeline_simd.h"
#include "batched_fft.h"

_Static_assert(CHANNEL_PIPELINE_ISA_SCALAR == (int)BATCHED_FFT_ISA_SCALAR && CHANNEL_PIPELINE_ISA_AVX2 == (int)BATCHED_FFT_ISA_AVX2
                   && CHANNEL_PIPELINE_ISA_AVX512 == (int)BATCHED_FFT_ISA_AVX512
                   && CHANNEL_PIPELINE_ISA_NEON == (int)BATCHED_FFT_ISA_NEON
                   && CHANNEL_PIPELINE_ISA_SVE2 == (int)BATCHED_FFT_ISA_SVE2,
               "channel_pipeline_isa_t must use the batched_fft_isa_t ids");

bool channel_pipeline_isa_supported(channel_pipeline_isa_t isa)
{
  return isa != CHANNEL_PIPELINE_ISA_AUTO && batched_fft_isa_supported((batched_fft_isa_t)isa);
}

channel_pipeline_isa_t channel_pipeline_isa_best(void)
{
  if (channel_pipeline_isa_supported(CHANNEL_PIPELINE_ISA_AVX512))
    return CHANNEL_PIPELINE_ISA_AVX512;
  if (channel_pipeline_isa_supported(CHANNEL_PIPELINE_ISA_AVX2))
    return CHANNEL_PIPELINE_ISA_AVX2;
  // At 128-bit VL SVE2 does the same work as NEON at the same width and measured no faster (GB10),
  // so it is only preferred when its vectors are wider.
  if (channel_pipeline_isa_supported(CHANNEL_PIPELINE_ISA_SVE2) && channel_pipeline_isa_lanes(CHANNEL_PIPELINE_ISA_SVE2) > 4)
    return CHANNEL_PIPELINE_ISA_SVE2;
  if (channel_pipeline_isa_supported(CHANNEL_PIPELINE_ISA_NEON))
    return CHANNEL_PIPELINE_ISA_NEON;
  return CHANNEL_PIPELINE_ISA_SCALAR;
}

int channel_pipeline_isa_lanes(channel_pipeline_isa_t isa)
{
  return batched_fft_batch_size((batched_fft_isa_t)isa);
}

static void c16_to_split(channel_pipeline_isa_t isa, float *re, float *im, const c16_t *src, int n)
{
  switch (isa) {
#if defined(__x86_64__)
    case CHANNEL_PIPELINE_ISA_AVX2:
      channel_pipeline_c16_to_split_avx2(re, im, src, n);
      break;
    case CHANNEL_PIPELINE_ISA_AVX512:
      channel_pipeline_c16_to_split_avx512(re, im, src, n);
      break;
#endif
#if defined(__aarch64__)
    case CHANNEL_PIPELINE_ISA_NEON:
      channel_pipeline_c16_to_split_neon(re, im, src, n);
      break;
#endif
#if defined(BATCHED_FFT_HAVE_SVE2)
    case CHANNEL_PIPELINE_ISA_SVE2:
      channel_pipeline_c16_to_split_sve2(re, im, src, n);
      break;
#endif
    default:
      channel_pipeline_c16_to_split_scalar(re, im, src, n);
  }
}

static void split_to_c16(channel_pipeline_isa_t isa, c16_t *dst, const float *re, const float *im, int n)
{
  switch (isa) {
#if defined(__x86_64__)
    case CHANNEL_PIPELINE_ISA_AVX2:
      channel_pipeline_split_to_c16_avx2(dst, re, im, n);
      break;
    case CHANNEL_PIPELINE_ISA_AVX512:
      channel_pipeline_split_to_c16_avx512(dst, re, im, n);
      break;
#endif
#if defined(__aarch64__)
    case CHANNEL_PIPELINE_ISA_NEON:
      channel_pipeline_split_to_c16_neon(dst, re, im, n);
      break;
#endif
#if defined(BATCHED_FFT_HAVE_SVE2)
    case CHANNEL_PIPELINE_ISA_SVE2:
      channel_pipeline_split_to_c16_sve2(dst, re, im, n);
      break;
#endif
    default:
      channel_pipeline_split_to_c16_scalar(dst, re, im, n);
  }
}

static void direct_mac(channel_pipeline_isa_t isa,
                       float *acc_re,
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
  switch (isa) {
#if defined(__x86_64__)
    case CHANNEL_PIPELINE_ISA_AVX2:
      channel_pipeline_direct_mac_avx2(acc_re, acc_im, in_re, in_im, x_re, x_im, g_re, g_im, n, L);
      break;
    case CHANNEL_PIPELINE_ISA_AVX512:
      channel_pipeline_direct_mac_avx512(acc_re, acc_im, in_re, in_im, x_re, x_im, g_re, g_im, n, L);
      break;
#endif
#if defined(__aarch64__)
    case CHANNEL_PIPELINE_ISA_NEON:
      channel_pipeline_direct_mac_neon(acc_re, acc_im, in_re, in_im, x_re, x_im, g_re, g_im, n, L);
      break;
#endif
#if defined(BATCHED_FFT_HAVE_SVE2)
    case CHANNEL_PIPELINE_ISA_SVE2:
      channel_pipeline_direct_mac_sve2(acc_re, acc_im, in_re, in_im, x_re, x_im, g_re, g_im, n, L);
      break;
#endif
    default:
      channel_pipeline_direct_mac_scalar(acc_re, acc_im, in_re, in_im, x_re, x_im, g_re, g_im, n, L);
  }
}

static void freq_mac(channel_pipeline_isa_t isa, float *y_re, float *y_im, const float *X, const float *H, int N, int nb_tx)
{
  switch (isa) {
#if defined(__x86_64__)
    case CHANNEL_PIPELINE_ISA_AVX2:
      channel_pipeline_freq_mac_avx2(y_re, y_im, X, H, N, nb_tx);
      break;
    case CHANNEL_PIPELINE_ISA_AVX512:
      channel_pipeline_freq_mac_avx512(y_re, y_im, X, H, N, nb_tx);
      break;
#endif
#if defined(__aarch64__)
    case CHANNEL_PIPELINE_ISA_NEON:
      channel_pipeline_freq_mac_neon(y_re, y_im, X, H, N, nb_tx);
      break;
#endif
#if defined(BATCHED_FFT_HAVE_SVE2)
    case CHANNEL_PIPELINE_ISA_SVE2:
      channel_pipeline_freq_mac_sve2(y_re, y_im, X, H, N, nb_tx);
      break;
#endif
    default:
      channel_pipeline_freq_mac_scalar(y_re, y_im, X, H, N, nb_tx);
  }
}

static void gather_c16(channel_pipeline_isa_t isa, float *re, float *im, const c16_t *src, int stride, int npts)
{
  switch (isa) {
#if defined(__x86_64__)
    case CHANNEL_PIPELINE_ISA_AVX2:
      channel_pipeline_gather_c16_avx2(re, im, src, stride, npts);
      break;
    case CHANNEL_PIPELINE_ISA_AVX512:
      channel_pipeline_gather_c16_avx512(re, im, src, stride, npts);
      break;
#endif
#if defined(__aarch64__)
    case CHANNEL_PIPELINE_ISA_NEON:
      channel_pipeline_gather_c16_neon(re, im, src, stride, npts);
      break;
#endif
#if defined(BATCHED_FFT_HAVE_SVE2)
    case CHANNEL_PIPELINE_ISA_SVE2:
      channel_pipeline_gather_c16_sve2(re, im, src, stride, npts);
      break;
#endif
    default:
      channel_pipeline_gather_c16_scalar(re, im, src, stride, npts);
  }
}

static void scatter_c16(channel_pipeline_isa_t isa, c16_t *dst, int stride, const float *re, const float *im, int npts)
{
  switch (isa) {
#if defined(__x86_64__)
    case CHANNEL_PIPELINE_ISA_AVX2:
      channel_pipeline_scatter_c16_avx2(dst, stride, re, im, npts);
      break;
    case CHANNEL_PIPELINE_ISA_AVX512:
      channel_pipeline_scatter_c16_avx512(dst, stride, re, im, npts);
      break;
#endif
#if defined(__aarch64__)
    case CHANNEL_PIPELINE_ISA_NEON:
      channel_pipeline_scatter_c16_neon(dst, stride, re, im, npts);
      break;
#endif
#if defined(BATCHED_FFT_HAVE_SVE2)
    case CHANNEL_PIPELINE_ISA_SVE2:
      channel_pipeline_scatter_c16_sve2(dst, stride, re, im, npts);
      break;
#endif
    default:
      channel_pipeline_scatter_c16_scalar(dst, stride, re, im, npts);
  }
}

// Direct unit u = (batch u / nb_rx, rx antenna u % nb_rx).
static void direct_unit(const channel_pipeline_call_t *c, int job, int u)
{
  const channel_pipeline_isa_t isa = c->isa;
  const int vw = channel_pipeline_isa_lanes(isa);
  const int L = c->L;
  const int a = u % c->nb_rx;
  const int start = (u / c->nb_rx) * c->block;
  const int n = c->block < c->num_samples - start ? c->block : c->num_samples - start;
  const int npad = channel_pipeline_round_up(n, 4 * vw);
  const int xlen = channel_pipeline_round_up(npad + L - 1, vw);
  // acc re/im (npad each), x re/im (xlen each), c16 staging (max(xlen, npad) entries)
  float *acc_re = channel_pipeline_scratch(c, job, 2 * (size_t)npad + 3 * (size_t)xlen);
  float *acc_im = acc_re + npad;
  float *x_re = acc_im + npad;
  float *x_im = x_re + xlen;
  c16_t *tmp = (c16_t *)(x_im + xlen);
  // Noise snapshot as the starting value of the accumulators (the noise device keeps rewriting its
  // table, so it is copied out rather than referenced); without noise they start from zero.
  const float *in_re = NULL, *in_im = NULL;
  if (c->noise) {
    get_noise_vector(c->noise, acc_re, npad);
    get_noise_vector(c->noise, acc_im, npad);
    in_re = acc_re;
    in_im = acc_im;
  }
  for (int t = 0; t < c->nb_tx; t++) {
    c16_to_split(isa, x_re, x_im, channel_pipeline_input_view(c, t, start, xlen, tmp), xlen);
    const float *g = c->g + (size_t)(a * c->nb_tx + t) * 2 * L;
    direct_mac(isa, acc_re, acc_im, in_re, in_im, x_re, x_im, g, g + L, npad, L);
    in_re = acc_re;
    in_im = acc_im;
  }
  c16_t *dst = channel_pipeline_output_view(c, a, start, n);
  if (dst) {
    split_to_c16(isa, dst, acc_re, acc_im, n);
  } else {
    split_to_c16(isa, tmp, acc_re, acc_im, n);
    channel_pipeline_copy_output(c, a, start, n, tmp);
  }
}

// FFT unit u = one group of `lanes` consecutive overlap-save blocks, all tx and rx antennas.
// Block b of the group reads input window [start + b * B, start + b * B + N) and produces outputs
// [start + b * B, start + (b + 1) * B) from circular outputs L - 1 .. N - 1.
static void fft_unit(const channel_pipeline_call_t *c, int job, int u)
{
  const channel_pipeline_isa_t isa = c->isa;
  const int vw = channel_pipeline_isa_lanes(isa);
  const int N = c->fft_n;
  const int B = c->block;
  const int L = c->L;
  const int start = u * vw * B;
  const int n_out = vw * B < c->num_samples - start ? vw * B : c->num_samples - start;
  const size_t plane = (size_t)N * vw;
  const size_t group = (size_t)vw * B;
  const int in_len = group + L - 1;
  // X (nb_tx transforms), Y, c16 staging for input (in_len) and output (group)
  float *X = channel_pipeline_scratch(c, job, (size_t)c->nb_tx * 2 * plane + 2 * plane + in_len + group);
  float *y_re = X + (size_t)c->nb_tx * 2 * plane;
  float *y_im = y_re + plane;
  c16_t *in_tmp = (c16_t *)(y_im + plane);
  c16_t *out_tmp = in_tmp + in_len;

  for (int t = 0; t < c->nb_tx; t++) {
    float *xr = X + t * 2 * plane;
    float *xi = xr + plane;
    gather_c16(isa, xr, xi, channel_pipeline_input_view(c, t, start, in_len, in_tmp), B, N);
    batched_fft_forward((batched_fft_isa_t)isa, xr, xi, N, c->tw);
  }
  for (int a = 0; a < c->nb_rx; a++) {
    freq_mac(isa, y_re, y_im, X, c->H + (size_t)a * c->nb_tx * 2 * N, N, c->nb_tx);
    batched_fft_inverse((batched_fft_isa_t)isa, y_re, y_im, N, c->tw);
    // Overlap-save: the first L - 1 circular outputs of each block are aliased, skip them. The valid
    // part is contiguous (points L - 1 .. N - 1 of every lane), so noise goes straight onto it.
    float *vr = y_re + (size_t)(L - 1) * vw;
    float *vi = y_im + (size_t)(L - 1) * vw;
    if (c->noise) {
      add_noise_vector(c->noise, vr, group);
      add_noise_vector(c->noise, vi, group);
    }
    c16_t *dst = n_out == (int)group ? channel_pipeline_output_view(c, a, start, n_out) : NULL;
    if (dst) {
      scatter_c16(isa, dst, B, vr, vi, B);
    } else {
      scatter_c16(isa, out_tmp, B, vr, vi, B);
      channel_pipeline_copy_output(c, a, start, n_out, out_tmp);
    }
  }
}

void channel_pipeline_run_units(const channel_pipeline_call_t *c, int index)
{
  for (int u = index; u < c->num_units; u += c->num_jobs) {
    if (c->fft_n)
      fft_unit(c, index, u);
    else
      direct_unit(c, index, u);
  }
}
