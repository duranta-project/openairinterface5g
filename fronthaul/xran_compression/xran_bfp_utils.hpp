/*
 * SPDX-License-Identifier: Apache-2.0
 * Original file: Copyright 2020 Intel.
 * Copyright 2026 OpenAirInterface Authors
 */

#pragma once
#include "simd_wrapper.h"

namespace BlockFloatCompander {
/// Calculate exponent based on 16 max abs values using leading zero count.
inline __m512i maskUpperWord(const __m512i inData)
{
  const auto k_upperWordMask = _mm512_set_epi64(0x0000FFFF0000FFFF,
                                                0x0000FFFF0000FFFF,
                                                0x0000FFFF0000FFFF,
                                                0x0000FFFF0000FFFF,
                                                0x0000FFFF0000FFFF,
                                                0x0000FFFF0000FFFF,
                                                0x0000FFFF0000FFFF,
                                                0x0000FFFF0000FFFF);
  return _mm512_and_epi64(inData, k_upperWordMask);
}

/// Calculate exponent based on 16 max abs values using leading zero count.
inline __m512i expLzCnt(const __m512i maxAbs, const __m512i totShiftBits)
{
  /// Compute exponent
  const auto lzCount = _mm512_lzcnt_epi32(maxAbs);
  return _mm512_subs_epu16(totShiftBits, lzCount);
}

/// Full horizontal max of 16b values
inline int horizontalMax1x32(const __m512i maxAbsReg)
{
  /// Swap each IQ pair in each lane (via 32b rotation) and compute max of
  /// each pair.
  const auto maxRot16 = _mm512_rol_epi32(maxAbsReg, BlockFloatCompander::k_numBitsIQ);
  const auto maxAbsIQ = _mm512_max_epi16(maxAbsReg, maxRot16);
  /// Convert to 32b by removing repeated values in maxAbs
  const auto maxAbs32 = maskUpperWord(maxAbsIQ);
  /// Return reduced max
  return _mm512_reduce_max_epi32(maxAbs32);
}

/// Perform horizontal max of 16 bit values across each lane
inline __m512i horizontalMax4x16(const __m512i maxAbsIn)
{
  /// Swap 64b in each lane and compute max
  const auto k_perm64b = _mm512_set_epi64(6, 7, 4, 5, 2, 3, 0, 1);
  auto maxAbsPerm = _mm512_permutexvar_epi64(k_perm64b, maxAbsIn);
  auto maxAbsHorz = _mm512_max_epi16(maxAbsIn, maxAbsPerm);

  /// Swap each pair of 32b in each lane and compute max
  const auto k_perm32b = _mm512_set_epi32(14, 15, 12, 13, 10, 11, 8, 9, 6, 7, 4, 5, 2, 3, 0, 1);
  maxAbsPerm = _mm512_permutexvar_epi32(k_perm32b, maxAbsHorz);
  maxAbsHorz = _mm512_max_epi16(maxAbsHorz, maxAbsPerm);

  /// Swap each IQ pair in each lane (via 32b rotation) and compute max
  maxAbsPerm = _mm512_rol_epi32(maxAbsHorz, BlockFloatCompander::k_numBitsIQ);
  return _mm512_max_epi16(maxAbsHorz, maxAbsPerm);
}

/// Perform U-plane input data re-ordering and vertical max abs of 16b values
/// Works on 4 RB at a time
inline __m512i maxAbsVertical4RB(const __m512i inA, const __m512i inB, const __m512i inC)
{
  /// Re-order the next 4RB in input data into 3 registers
  /// Input SIMD vectors are:
  ///   [A A A A A A A A A A A A B B B B]
  ///   [B B B B B B B B C C C C C C C C]
  ///   [C C C C D D D D D D D D D D D D]
  /// Re-ordered SIMD vectors are:
  ///   [A A A A B B B B C C C C D D D D]
  ///   [A A A A B B B B C C C C D D D D]
  ///   [A A A A B B B B C C C C D D D D]
  constexpr uint8_t k_msk1 = 0b11111100; // Copy first lane of src
  constexpr int k_shuff1 = 0x41;
  const auto z_w1 = _mm512_mask_shuffle_i64x2(inA, k_msk1, inB, inC, k_shuff1);

  constexpr uint8_t k_msk2 = 0b11000011; // Copy middle two lanes of src
  constexpr int k_shuff2 = 0xB1;
  const auto z_w2 = _mm512_mask_shuffle_i64x2(inB, k_msk2, inA, inC, k_shuff2);

  constexpr uint8_t k_msk3 = 0b00111111; // Copy last lane of src
  constexpr int k_shuff3 = 0xBE;
  const auto z_w3 = _mm512_mask_shuffle_i64x2(inC, k_msk3, inA, inB, k_shuff3);

  /// Perform max abs on these 3 registers
  const auto abs16_1 = _mm512_abs_epi16(z_w1);
  const auto abs16_2 = _mm512_abs_epi16(z_w2);
  const auto abs16_3 = _mm512_abs_epi16(z_w3);
  return _mm512_max_epi16(_mm512_max_epi16(abs16_1, abs16_2), abs16_3);
}

/// Selects first 32 bit value in each src lane and packs into laneNum of dest
inline __m512i slidePermute(const __m512i src, const __m512i dest, const int laneNum)
{
  const auto k_selectVals = _mm512_set_epi32(28, 24, 20, 16, 28, 24, 20, 16, 28, 24, 20, 16, 28, 24, 20, 16);
  constexpr uint16_t k_laneMsk[4] = {0x000F, 0x00F0, 0x0F00, 0xF000};
  return _mm512_mask_permutex2var_epi32(dest, k_laneMsk[laneNum], k_selectVals, src);
}
} // namespace BlockFloatCompander
