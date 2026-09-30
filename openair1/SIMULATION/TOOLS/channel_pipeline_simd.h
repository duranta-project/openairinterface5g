/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Internal interface between the CPU channel pipeline driver (channel_pipeline.c), the work it
// splits into units (channel_pipeline_simd.c) and the per-ISA kernels (channel_pipeline_<isa>.c).
//
// The driver plans a call (method, block size, number of units) and fills a channel_pipeline_call_t; each job
// then runs channel_pipeline_run_units(). A unit calls the kernels of the call's ISA through a switch on the ISA,
// no function pointers. Every kernel processes a whole block, so the switch is negligible.
//
// All kernels work on split (planar) float data: one array of real parts, one of imaginary parts.
// FFT data is "lane-major": point n of lane b is at [n * lanes + b], so every SIMD lane carries
// an independent overlap-save block and butterflies need no shuffles.

#ifndef CHANNEL_PIPELINE_SIMD_H
#define CHANNEL_PIPELINE_SIMD_H

#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include <stdint.h>
#include "common/platform_types.h"
#include "channel_pipeline.h"
#include "task_ans.h"
#include "noise_device.h"
#include "batched_fft.h"

typedef struct {
  channel_pipeline_t *pipeline;
  channel_pipeline_isa_t isa;
  const c16_t **tx_sig0;
  const c16_t **tx_sig1;
  int num_samples_tx_sig0;
  c16_t **rx_sig0;
  c16_t **rx_sig1;
  int num_samples_rx_sig0;
  int num_samples;
  int nb_tx;
  int nb_rx;
  int L;
  noise_device_t *noise; // NULL: no noise
  // direct: link l = aarx * nb_tx + aatx, time-reversed taps, re at g + l * 2 * L, im at + L
  const float *g;
  // fft (fft_n == 0 selects direct): twiddles (batched_fft_twiddles()) and per-link bit-reversed
  // frequency response / N, re at H + l * 2 * N, im at + N
  int fft_n;
  const float *tw;
  const float *H;
  // direct: outputs per unit; fft: B = N - L + 1
  int block;
  int num_units;
  int num_jobs;
  task_ans_t *ans;
} channel_pipeline_call_t;

// Implemented in channel_pipeline_simd.c
bool channel_pipeline_isa_supported(channel_pipeline_isa_t isa);
channel_pipeline_isa_t channel_pipeline_isa_best(void);
// Float lanes per vector of the ISA (runtime for SVE).
int channel_pipeline_isa_lanes(channel_pipeline_isa_t isa);
// Process units index, index + num_jobs, ... of the call.
void channel_pipeline_run_units(const channel_pipeline_call_t *c, int index);

// Per-ISA kernels (channel_pipeline_<isa>.c). "lanes" is channel_pipeline_isa_lanes() of the ISA.
//  c16_to_split: re/im[i] = src[i] for i < n (int16 to float)
//  split_to_c16: dst[i] = re/im[i] for i < n, rounded to nearest and saturated (channel_pipeline_round_s16())
//  direct_mac:   output-stationary direct convolution of n outputs (n a multiple of 4 * lanes):
//                acc[i] = in[i] + sum_k x[i + k] * g[k], k < L, taps time-reversed; in may be acc
//                itself, or NULL for zero
//  freq_mac:     y[n] = sum_t X_t[n] * H_t[n] over nb_tx lane-major N-point spectra
//                (X_t re at X + t * 2 * N * lanes, im at + N * lanes; H_t re at H + t * 2 * N, im at + N)
//  gather_c16:   re/im[n * lanes + b] = src[b * stride + n] for n < npts: `lanes` consecutive
//                overlap-save windows to lane-major FFT input
//  scatter_c16:  dst[b * stride + n] = re/im[n * lanes + b] for n < npts, rounded and saturated
void channel_pipeline_c16_to_split_scalar(float *re, float *im, const c16_t *src, int n);
void channel_pipeline_split_to_c16_scalar(c16_t *dst, const float *re, const float *im, int n);
void channel_pipeline_direct_mac_scalar(float *acc_re,
                                        float *acc_im,
                                        const float *in_re,
                                        const float *in_im,
                                        const float *x_re,
                                        const float *x_im,
                                        const float *g_re,
                                        const float *g_im,
                                        int n,
                                        int L);
void channel_pipeline_freq_mac_scalar(float *y_re, float *y_im, const float *X, const float *H, int N, int nb_tx);
void channel_pipeline_gather_c16_scalar(float *re, float *im, const c16_t *src, int stride, int npts);
void channel_pipeline_scatter_c16_scalar(c16_t *dst, int stride, const float *re, const float *im, int npts);
#if defined(__x86_64__)
void channel_pipeline_c16_to_split_avx2(float *re, float *im, const c16_t *src, int n);
void channel_pipeline_split_to_c16_avx2(c16_t *dst, const float *re, const float *im, int n);
void channel_pipeline_direct_mac_avx2(float *acc_re,
                                      float *acc_im,
                                      const float *in_re,
                                      const float *in_im,
                                      const float *x_re,
                                      const float *x_im,
                                      const float *g_re,
                                      const float *g_im,
                                      int n,
                                      int L);
void channel_pipeline_freq_mac_avx2(float *y_re, float *y_im, const float *X, const float *H, int N, int nb_tx);
void channel_pipeline_gather_c16_avx2(float *re, float *im, const c16_t *src, int stride, int npts);
void channel_pipeline_scatter_c16_avx2(c16_t *dst, int stride, const float *re, const float *im, int npts);
void channel_pipeline_c16_to_split_avx512(float *re, float *im, const c16_t *src, int n);
void channel_pipeline_split_to_c16_avx512(c16_t *dst, const float *re, const float *im, int n);
void channel_pipeline_direct_mac_avx512(float *acc_re,
                                        float *acc_im,
                                        const float *in_re,
                                        const float *in_im,
                                        const float *x_re,
                                        const float *x_im,
                                        const float *g_re,
                                        const float *g_im,
                                        int n,
                                        int L);
void channel_pipeline_freq_mac_avx512(float *y_re, float *y_im, const float *X, const float *H, int N, int nb_tx);
void channel_pipeline_gather_c16_avx512(float *re, float *im, const c16_t *src, int stride, int npts);
void channel_pipeline_scatter_c16_avx512(c16_t *dst, int stride, const float *re, const float *im, int npts);
#endif
#if defined(__aarch64__)
void channel_pipeline_c16_to_split_neon(float *re, float *im, const c16_t *src, int n);
void channel_pipeline_split_to_c16_neon(c16_t *dst, const float *re, const float *im, int n);
void channel_pipeline_direct_mac_neon(float *acc_re,
                                      float *acc_im,
                                      const float *in_re,
                                      const float *in_im,
                                      const float *x_re,
                                      const float *x_im,
                                      const float *g_re,
                                      const float *g_im,
                                      int n,
                                      int L);
void channel_pipeline_freq_mac_neon(float *y_re, float *y_im, const float *X, const float *H, int N, int nb_tx);
void channel_pipeline_gather_c16_neon(float *re, float *im, const c16_t *src, int stride, int npts);
void channel_pipeline_scatter_c16_neon(c16_t *dst, int stride, const float *re, const float *im, int npts);
#endif
#if defined(BATCHED_FFT_HAVE_SVE2)
void channel_pipeline_c16_to_split_sve2(float *re, float *im, const c16_t *src, int n);
void channel_pipeline_split_to_c16_sve2(c16_t *dst, const float *re, const float *im, int n);
void channel_pipeline_direct_mac_sve2(float *acc_re,
                                      float *acc_im,
                                      const float *in_re,
                                      const float *in_im,
                                      const float *x_re,
                                      const float *x_im,
                                      const float *g_re,
                                      const float *g_im,
                                      int n,
                                      int L);
void channel_pipeline_freq_mac_sve2(float *y_re, float *y_im, const float *X, const float *H, int N, int nb_tx);
void channel_pipeline_gather_c16_sve2(float *re, float *im, const c16_t *src, int stride, int npts);
void channel_pipeline_scatter_c16_sve2(c16_t *dst, int stride, const float *re, const float *im, int npts);
#endif

// Implemented in channel_pipeline.c
// Scratch of job `job` of the call, at least nfloats floats (64-byte aligned, reused across calls).
float *channel_pipeline_scratch(const channel_pipeline_call_t *c, int job, size_t nfloats);
// Contiguous view of input positions [start, start + n) of tx antenna t of the concatenated
// sig0|sig1 stream: a pointer into sig0 or sig1 when possible, else a copy in tmp (n entries).
// Positions past the end of the input read as zero.
const c16_t *channel_pipeline_input_view(const channel_pipeline_call_t *c, int t, int start, int n, c16_t *tmp);
// Output positions [start, start + n) of rx antenna a if they lie in sig0 or sig1, else NULL.
c16_t *channel_pipeline_output_view(const channel_pipeline_call_t *c, int a, int start, int n);
void channel_pipeline_copy_output(const channel_pipeline_call_t *c, int a, int start, int n, const c16_t *src);

// Output conversion of every path: round to nearest (ties to even), saturate to int16. Rounding
// rather than truncating keeps the FFT method's ~1e-4 float error from flipping integer results.
static inline int16_t channel_pipeline_round_s16(float v)
{
  v = v > 32767.0f ? 32767.0f : v;
  v = v < -32768.0f ? -32768.0f : v;
  return (int16_t)lrintf(v);
}

static inline size_t channel_pipeline_round_up(size_t x, size_t m)
{
  return (x + m - 1) / m * m;
}

#endif
