/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "tools_defs.h"
#include "PHY/sse_intrin.h"

#if defined(__x86_64__) || defined(__i386__)
// Narrow AVX512 includes for the ops used by the AVX512 tier below, rather than the
// <simde/x86/avx512.h> umbrella header (see the same rationale in tools_defs.h, next to
// rotate_cpx_vector): the umbrella drags in simde/x86/svml.h -> C++ <complex>, which
// breaks extern "C" blocks in C++ callers such as the benchmark files.
#include <simde/x86/avx512/setzero.h>
#include <simde/x86/avx512/add.h>
#include <simde/x86/avx512/mullo.h>
#include <simde/x86/avx512/extract.h>
#include <simde/x86/avx512/cast.h>
#endif // defined(__x86_64__) || defined(__i386__)

/*! \brief Complex number dot_product
@param x input vector
@param y input vector
@param N size of vectors
@param output_shift normalization of int multiplications
*/

static inline int32_t sat32(int64_t v)
{
  return v > INT32_MAX ? INT32_MAX : v < INT32_MIN ? INT32_MIN : (int32_t)v;
}

c32_t dot_product(const c16_t *x, const c16_t *y, const uint32_t N, const int output_shift)
{
  // Combine the SIMD lane sums and the scalar tail in 64 bits, then saturate to 32 bits
  int64_t acc_r = 0, acc_i = 0;
  uint32_t i = 0;

#if defined(__x86_64__) || defined(__i386__)
#if defined(__AVX512F__) && defined(__AVX512BW__)
  // Same VPMADDWD multiply-add / conj+swap / shift algorithm as the AVX2 tier below, widened to
  // 512-bit registers (16 complex numbers per iteration instead of 8).
  {
    const c16_t for_conj = {1, -1};
    const simde__m512i neg_imag = simde_mm512_set1_epi32(*(const uint32_t *)&for_conj);

    simde__m512i cumul_re = simde_mm512_setzero_si512();
    simde__m512i cumul_im = simde_mm512_setzero_si512();

    for (; i < (N & ~15u); i += 16) {
      const simde__m512i in1 = simde_mm512_loadu_si512((const simde__m512i *)(x + i));
      const simde__m512i in2 = simde_mm512_loadu_si512((const simde__m512i *)(y + i));

      const simde__m512i tmpRe = simde_mm512_srai_epi32(simde_mm512_madd_epi16(in1, in2), output_shift);
      const simde__m512i conj_swap = oai_mm512_swap(simde_mm512_mullo_epi16(in1, neg_imag));
      const simde__m512i tmpIm = simde_mm512_srai_epi32(simde_mm512_madd_epi16(conj_swap, in2), output_shift);

      cumul_re = simde_mm512_add_epi32(cumul_re, tmpRe);
      cumul_im = simde_mm512_add_epi32(cumul_im, tmpIm);
    }

    const simde__m256i re256 =
        simde_mm256_add_epi32(simde_mm512_castsi512_si256(cumul_re), simde_mm512_extracti64x4_epi64(cumul_re, 1));
    const simde__m256i im256 =
        simde_mm256_add_epi32(simde_mm512_castsi512_si256(cumul_im), simde_mm512_extracti64x4_epi64(cumul_im, 1));
    const simde__m256i cumulTmp = simde_mm256_hadd_epi32(re256, im256);
    const simde__m256i cumul = simde_mm256_hadd_epi32(cumulTmp, cumulTmp);
    acc_r += (int64_t)simde_mm256_extract_epi32(cumul, 0) + (int64_t)simde_mm256_extract_epi32(cumul, 4);
    acc_i += (int64_t)simde_mm256_extract_epi32(cumul, 1) + (int64_t)simde_mm256_extract_epi32(cumul, 5);
  }
#endif
#ifdef __AVX2__
  {
    simde__m256i cumul_re = simde_mm256_setzero_si256();
    simde__m256i cumul_im = simde_mm256_setzero_si256();

    for (; i < (N & ~7u); i += 8) {
      const simde__m256i in1 = simde_mm256_loadu_si256((const simde__m256i *)(x + i));
      const simde__m256i in2 = simde_mm256_loadu_si256((const simde__m256i *)(y + i));

      const simde__m256i tmpRe = oai_mm256_smadd(in1, in2, output_shift);
      const simde__m256i tmpIm = oai_mm256_smadd(oai_mm256_swap(oai_mm256_conj(in1)), in2, output_shift);

      cumul_re = simde_mm256_add_epi32(cumul_re, tmpRe);
      cumul_im = simde_mm256_add_epi32(cumul_im, tmpIm);
    }

    // this gives Re Re Im Im Re Re Im Im
    const simde__m256i cumulTmp = simde_mm256_hadd_epi32(cumul_re, cumul_im);
    const simde__m256i cumul = simde_mm256_hadd_epi32(cumulTmp, cumulTmp);

    acc_r += (int64_t)simde_mm256_extract_epi32(cumul, 0) + (int64_t)simde_mm256_extract_epi32(cumul, 4);
    acc_i += (int64_t)simde_mm256_extract_epi32(cumul, 1) + (int64_t)simde_mm256_extract_epi32(cumul, 5);
  }
#endif
#endif // defined(__x86_64__) || defined(__i386__)

  // 128-bit tier: SSE2 on x86, transparently portable to other architectures (e.g. NEON on
  // aarch64) via SIMDe. This is what makes small dot products worth vectorizing too.
  {
    simde__m128i cumul_re = simde_mm_setzero_si128();
    simde__m128i cumul_im = simde_mm_setzero_si128();

    for (; i < (N & ~3u); i += 4) {
      const simde__m128i in1 = simde_mm_loadu_si128((const simde__m128i *)(x + i));
      const simde__m128i in2 = simde_mm_loadu_si128((const simde__m128i *)(y + i));

      const simde__m128i tmpRe = oai_mm_smadd(in1, in2, output_shift);
      const simde__m128i tmpIm = oai_mm_smadd(oai_mm_swap(oai_mm_conj(in1)), in2, output_shift);

      cumul_re = simde_mm_add_epi32(cumul_re, tmpRe);
      cumul_im = simde_mm_add_epi32(cumul_im, tmpIm);
    }

    const simde__m128i cumulTmp = simde_mm_hadd_epi32(cumul_re, cumul_im);
    const simde__m128i cumul = simde_mm_hadd_epi32(cumulTmp, cumulTmp);
    acc_r += (int64_t)simde_mm_extract_epi32(cumul, 0);
    acc_i += (int64_t)simde_mm_extract_epi32(cumul, 1);
  }

  // scalar tail
  for (; i < N; i++) {
    acc_r += (int64_t)((x[i].r * y[i].r) >> output_shift) + (int64_t)((x[i].i * y[i].i) >> output_shift);
    acc_i += (int64_t)((x[i].r * y[i].i) >> output_shift) - (int64_t)((x[i].i * y[i].r) >> output_shift);
  }
  return (c32_t){sat32(acc_r), sat32(acc_i)};
}
