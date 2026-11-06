/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Unpack the 32-segment bit-sliced interleaver output of the CUDA LDPC encoder into a packed bit stream
 */

#pragma once

#include <stdint.h>

#if defined(__AVX2__) || defined(__aarch64__)
#define NR_UNPACK_OUTPUT_LDPC32_SIMD 1
#endif

/**
 * \brief unpack the bit-sliced output of the encoder into the packed output of the transport block
 *
 * Segment s is bit (s & 31) of a word array. Segments [0, E2_first_segment) have E bits each and use f, 32-segment
 * group g starting at f + g * E. Segments [E2_first_segment, nb_segments) have E2 bits each and use f2, group g
 * starting at f2 + (g - E2_first_segment32) * E2. No alignment is required for f and f2.
 * \param f input of the segments of size E
 * \param E size of the first segments in bits
 * \param f2 input of the segments of size E2
 * \param E2 size of the last segments in bits
 * \param E2_first_segment32 index of the 32-segment group of the first segment of size E2
 * \param E2_first_segment index of the first segment of size E2
 * \param nb_segments number of segments in the transport block
 * \param output packed output, zeroed by the caller, the bits are ORed into it
 */
void nr_unpack_output_ldpc32(const uint32_t *f,
                             uint32_t E,
                             const uint32_t *f2,
                             uint32_t E2,
                             uint32_t E2_first_segment32,
                             uint32_t E2_first_segment,
                             uint32_t nb_segments,
                             uint8_t *output);

/// scalar implementation of nr_unpack_output_ldpc32(), exposed for testing
void nr_unpack_output_ldpc32_scalar(const uint32_t *f,
                                    uint32_t E,
                                    const uint32_t *f2,
                                    uint32_t E2,
                                    uint32_t E2_first_segment32,
                                    uint32_t E2_first_segment,
                                    uint32_t nb_segments,
                                    uint8_t *output);

#ifdef NR_UNPACK_OUTPUT_LDPC32_SIMD
/// AVX2 or NEON implementation of nr_unpack_output_ldpc32(), exposed for testing
void nr_unpack_output_ldpc32_simd(const uint32_t *f,
                                  uint32_t E,
                                  const uint32_t *f2,
                                  uint32_t E2,
                                  uint32_t E2_first_segment32,
                                  uint32_t E2_first_segment,
                                  uint32_t nb_segments,
                                  uint8_t *output);
#endif
