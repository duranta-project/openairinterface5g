/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include "batched_fft.h"
#if defined(__aarch64__)
#include <sys/auxv.h>
#ifndef HWCAP2_SVE2
#define HWCAP2_SVE2 (1 << 1)
#endif
#endif

bool batched_fft_isa_supported(batched_fft_isa_t isa)
{
  switch (isa) {
    case BATCHED_FFT_ISA_SCALAR:
      return true;
#if defined(__x86_64__)
    case BATCHED_FFT_ISA_AVX2:
      return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma");
    case BATCHED_FFT_ISA_AVX512:
      return __builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma");
#endif
#if defined(__aarch64__)
    case BATCHED_FFT_ISA_NEON:
      return true;
#if defined(BATCHED_FFT_HAVE_SVE2)
    case BATCHED_FFT_ISA_SVE2:
      return (getauxval(AT_HWCAP2) & HWCAP2_SVE2) != 0;
#endif
#endif
    default:
      return false;
  }
}

int batched_fft_batch_size(batched_fft_isa_t isa)
{
  switch (isa) {
    case BATCHED_FFT_ISA_AVX2:
      return 8;
    case BATCHED_FFT_ISA_AVX512:
      return 16;
    case BATCHED_FFT_ISA_NEON:
      return 4;
#if defined(BATCHED_FFT_HAVE_SVE2)
    case BATCHED_FFT_ISA_SVE2:
      return batched_fft_batch_size_sve2();
#endif
    default:
      return 1;
  }
}

// All tables are built once (sum over N of N floats: 32 KB for BATCHED_FFT_MAX_LOG2 = 12).
static float *batched_fft_tw[BATCHED_FFT_MAX_LOG2 + 1];
static pthread_once_t batched_fft_tw_once = PTHREAD_ONCE_INIT;

static void batched_fft_tw_init(void)
{
  for (int log2n = 1; log2n <= BATCHED_FFT_MAX_LOG2; log2n++) {
    const int N = 1 << log2n;
    float *tw = malloc(sizeof(float) * N);
    if (!tw)
      abort();
    for (int k = 0; k < N / 2; k++) {
      tw[k] = (float)cos(-2.0 * M_PI * k / N);
      tw[N / 2 + k] = (float)sin(-2.0 * M_PI * k / N);
    }
    batched_fft_tw[log2n] = tw;
  }
}

const float *batched_fft_twiddles(int N)
{
  if (N < 2 || (N & (N - 1)) || N > (1 << BATCHED_FFT_MAX_LOG2))
    return NULL;
  pthread_once(&batched_fft_tw_once, batched_fft_tw_init);
  return batched_fft_tw[__builtin_ctz(N)];
}

void batched_fft_forward(batched_fft_isa_t isa, float *re, float *im, int N, const float *tw)
{
  switch (isa) {
#if defined(__x86_64__)
    case BATCHED_FFT_ISA_AVX2:
      batched_fft_forward_avx2(re, im, N, tw);
      break;
    case BATCHED_FFT_ISA_AVX512:
      batched_fft_forward_avx512(re, im, N, tw);
      break;
#endif
#if defined(__aarch64__)
    case BATCHED_FFT_ISA_NEON:
      batched_fft_forward_neon(re, im, N, tw);
      break;
#endif
#if defined(BATCHED_FFT_HAVE_SVE2)
    case BATCHED_FFT_ISA_SVE2:
      batched_fft_forward_sve2(re, im, N, tw);
      break;
#endif
    default:
      batched_fft_forward_scalar(re, im, N, tw);
  }
}

void batched_fft_inverse(batched_fft_isa_t isa, float *re, float *im, int N, const float *tw)
{
  switch (isa) {
#if defined(__x86_64__)
    case BATCHED_FFT_ISA_AVX2:
      batched_fft_inverse_avx2(re, im, N, tw);
      break;
    case BATCHED_FFT_ISA_AVX512:
      batched_fft_inverse_avx512(re, im, N, tw);
      break;
#endif
#if defined(__aarch64__)
    case BATCHED_FFT_ISA_NEON:
      batched_fft_inverse_neon(re, im, N, tw);
      break;
#endif
#if defined(BATCHED_FFT_HAVE_SVE2)
    case BATCHED_FFT_ISA_SVE2:
      batched_fft_inverse_sve2(re, im, N, tw);
      break;
#endif
    default:
      batched_fft_inverse_scalar(re, im, N, tw);
  }
}
