/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Unpack the 32-segment bit-sliced interleaver output of the CUDA LDPC encoder into a packed bit stream.
 * Header-only so that it can be unit tested without CUDA.
 */

#ifndef NRLDPC_CUDA_UNPACK_OUTPUT_H
#define NRLDPC_CUDA_UNPACK_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "PHY/sse_intrin.h"

/* Layout of the input: segment s is bit (s & 31) of a word array. Segments [0, E2_first_segment) have E bits
 * each and use f, 32-segment group g starting at f + g * E. Segments [E2_first_segment, nb_segments) have E2
 * bits each and use f2, group g starting at f2 + (g - E2_first_segment32) * E2. Bits are ORed into output,
 * which must be zeroed by the caller. */

static inline void nr_ldpc_cuda_unpack_output_scalar(const uint32_t *f,
                                                     uint32_t E,
                                                     const uint32_t *f2,
                                                     uint32_t E2,
                                                     uint32_t E2_first_segment32,
                                                     uint32_t E2_first_segment,
                                                     uint32_t nb_segments,
                                                     uint8_t *output)
{
  uint32_t *output_p = (uint32_t *)output;
  uint32_t bit_index = 0;
  for (uint32_t s = 0; s < nb_segments; s++) {
    const bool first = s < E2_first_segment;
    const uint32_t len = first ? E : E2;
    const uint32_t *fp = first ? f + (s >> 5) * E : f2 + ((s >> 5) - E2_first_segment32) * E2;
    const uint32_t segpos = 1U << (s & 31);
    for (uint32_t i = 0; i < len; i++) {
      output_p[bit_index >> 5] |= (uint32_t)((fp[i] & segpos) != 0) << (bit_index & 31);
      bit_index++;
    }
  }
}

#ifdef __AVX2__
/* Gather bit s2 of p[0..31] into one word. p has no alignment requirement: segment slices start at
 * multiples of E words, which are not 32-byte aligned unless E % 8 == 0. */
static inline uint32_t nr_ldpc_cuda_gather_bit32(const uint32_t *p, int s2)
{
  const simde__m256i shift0 = simde_mm256_set_epi32(7, 6, 5, 4, 3, 2, 1, 0);
  const simde__m256i one = simde_mm256_set1_epi32(1);
  simde__m256i acc = simde_mm256_setzero_si256();
  for (int j = 0; j < 4; j++) {
    simde__m256i v = simde_mm256_srli_epi32(simde_mm256_loadu_si256((const simde__m256i *)(p + 8 * j)), s2);
    v = simde_mm256_and_si256(v, one);
    acc = simde_mm256_or_si256(acc, simde_mm256_sllv_epi32(v, simde_mm256_add_epi32(shift0, simde_mm256_set1_epi32(8 * j))));
  }
  const simde__m128i x = simde_mm_or_si128(simde_mm256_castsi256_si128(acc), simde_mm256_extracti128_si256(acc, 1));
  return simde_mm_extract_epi32(x, 0) | simde_mm_extract_epi32(x, 1) | simde_mm_extract_epi32(x, 2)
         | simde_mm_extract_epi32(x, 3);
}

/* OR the n low bits of w into the output stream at bit_index, without touching words past the last bit */
static inline void nr_ldpc_cuda_put_bits(uint32_t *output_p, uint32_t *bit_index, uint32_t w, uint32_t n)
{
  const uint32_t b = *bit_index & 31;
  output_p[*bit_index >> 5] |= w << b;
  if (b + n > 32)
    output_p[(*bit_index >> 5) + 1] |= w >> (32 - b);
  *bit_index += n;
}

static inline void nr_ldpc_cuda_unpack_output_avx2(const uint32_t *f,
                                                   uint32_t E,
                                                   const uint32_t *f2,
                                                   uint32_t E2,
                                                   uint32_t E2_first_segment32,
                                                   uint32_t E2_first_segment,
                                                   uint32_t nb_segments,
                                                   uint8_t *output)
{
  uint32_t *output_p = (uint32_t *)output;
  uint32_t bit_index = 0;
  for (uint32_t s = 0; s < nb_segments; s++) {
    const bool first = s < E2_first_segment;
    const uint32_t len = first ? E : E2;
    const uint32_t *fp = first ? f + (s >> 5) * E : f2 + ((s >> 5) - E2_first_segment32) * E2;
    const int s2 = s & 31;
    uint32_t i = 0;
    for (; i + 32 <= len; i += 32)
      nr_ldpc_cuda_put_bits(output_p, &bit_index, nr_ldpc_cuda_gather_bit32(fp + i, s2), 32);
    const uint32_t rem = len - i;
    if (rem) {
      // the slice ends here: gather from a zero-padded copy instead of reading past it
      uint32_t tail[32] = {0};
      memcpy(tail, fp + i, rem * sizeof(*tail));
      nr_ldpc_cuda_put_bits(output_p, &bit_index, nr_ldpc_cuda_gather_bit32(tail, s2), rem);
    }
  }
}
#endif

static inline void nr_ldpc_cuda_unpack_output(const uint32_t *f,
                                              uint32_t E,
                                              const uint32_t *f2,
                                              uint32_t E2,
                                              uint32_t E2_first_segment32,
                                              uint32_t E2_first_segment,
                                              uint32_t nb_segments,
                                              uint8_t *output)
{
#ifdef __AVX2__
  nr_ldpc_cuda_unpack_output_avx2(f, E, f2, E2, E2_first_segment32, E2_first_segment, nb_segments, output);
#else
  nr_ldpc_cuda_unpack_output_scalar(f, E, f2, E2, E2_first_segment32, E2_first_segment, nb_segments, output);
#endif
}

#endif
