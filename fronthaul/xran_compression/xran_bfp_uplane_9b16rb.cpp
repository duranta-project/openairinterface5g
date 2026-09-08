/*
 * SPDX-License-Identifier: Apache-2.0
 * Original file: Copyright 2020 Intel.
 * Copyright 2026 OpenAirInterface Authors
 */

#include "xran_compression.hpp"
#include "xran_bfp_utils.hpp"
#include "xran_bfp_byte_packing_utils.hpp"
#include "xran_compression.h"
#include <complex>
#include <algorithm>
#include "simd_wrapper.h"
#include <limits.h>
#include <cstring>

namespace BFP_UPlane_9b16RB {
/// Namespace constants
const int k_numREReal = 24; /// 12 IQ pairs

/// Compute exponent value for a set of 16 RB from the maximum absolute value.
/// Max Abs operates in a loop, executing 4 RB per iteration. The results are
/// packed into the final output register.
inline __m512i computeExponent_16RB(const BlockFloatCompander::ExpandedData& dataIn, const __m512i totShiftBits)
{
  __m512i maxAbs = __m512i();
  const __m512i* rawData = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
  /// Max Abs loop operates on 4RB at a time
  for (int n = 0; n < 4; ++n) {
    /// Re-order and vertical max abs
    auto maxAbsVert = BlockFloatCompander::maxAbsVertical4RB(rawData[3 * n + 0], rawData[3 * n + 1], rawData[3 * n + 2]);
    /// Horizontal max abs
    auto maxAbsHorz = BlockFloatCompander::horizontalMax4x16(maxAbsVert);
    /// Pack these 4 values into maxAbs
    maxAbs = BlockFloatCompander::slidePermute(maxAbsHorz, maxAbs, n);
  }
  /// Calculate exponent
  const auto maxAbs32 = BlockFloatCompander::maskUpperWord(maxAbs);
  return BlockFloatCompander::expLzCnt(maxAbs32, totShiftBits);
}

/// Apply compression to 1 RB
template <BlockFloatCompander::PackFunction networkBytePack>
inline void applyCompressionN_1RB(const BlockFloatCompander::ExpandedData& dataIn,
                                  BlockFloatCompander::CompressedData* dataOut,
                                  const int numREOffset,
                                  const uint8_t thisExp,
                                  const int thisRBExpAddr,
                                  const uint16_t rbWriteMask)
{
  /// Get AVX512 pointer aligned to desired RB
  const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(dataIn.dataExpanded + numREOffset);
  /// Apply the exponent shift
  const auto compData = _mm512_srai_epi16(*rawDataIn, thisExp);
  /// Pack compressed data network byte order
  const auto compDataBytePacked = networkBytePack(compData);
  /// Store exponent first
  dataOut->dataCompressed[thisRBExpAddr] = thisExp;
  /// Now have 1 RB worth of bytes separated into 3 chunks (1 per lane)
  /// Use three offset stores to join
  _mm_mask_storeu_epi8(dataOut->dataCompressed + thisRBExpAddr + 1, rbWriteMask, _mm512_extracti64x2_epi64(compDataBytePacked, 0));
  _mm_mask_storeu_epi8(dataOut->dataCompressed + thisRBExpAddr + 1 + dataIn.iqWidth,
                       rbWriteMask,
                       _mm512_extracti64x2_epi64(compDataBytePacked, 1));
  _mm_mask_storeu_epi8(dataOut->dataCompressed + thisRBExpAddr + 1 + (2 * dataIn.iqWidth),
                       rbWriteMask,
                       _mm512_extracti64x2_epi64(compDataBytePacked, 2));
}

/// Calls compression function specific to the number of RB to be executed. For 9, 10, or 12bit iqWidth.
template <BlockFloatCompander::PackFunction networkBytePack>
inline void compressByAllocN(const BlockFloatCompander::ExpandedData& dataIn,
                             BlockFloatCompander::CompressedData* dataOut,
                             const __m512i totShiftBits,
                             const int totNumBytesPerRB,
                             const uint16_t rbWriteMask)
{
  const auto exponents = computeExponent_16RB(dataIn, totShiftBits);
  for (int n = 0; n < 16; ++n) {
    applyCompressionN_1RB<networkBytePack>(dataIn,
                                           dataOut,
                                           n * k_numREReal,
                                           ((uint8_t*)&exponents)[n * 4],
                                           n * totNumBytesPerRB,
                                           rbWriteMask);
  }
}

/// Apply compression to 1 RB
template <BlockFloatCompander::UnpackFunction networkByteUnpack>
inline void applyExpansionN_1RB(const BlockFloatCompander::CompressedData& dataIn,
                                BlockFloatCompander::ExpandedData* dataOut,
                                const int expAddr,
                                const int thisRBAddr,
                                const int maxExpShift)
{
  /// Unpack network order packed data
  const auto dataUnpacked = networkByteUnpack(dataIn.dataCompressed + expAddr + 1);
  /// Apply exponent scaling (by appropriate arithmetic shift right)
  const auto dataExpanded = _mm512_srai_epi16(dataUnpacked, maxExpShift - *(dataIn.dataCompressed + expAddr));
  /// Write expanded data to output
  static constexpr uint32_t k_WriteMask = 0x00FFFFFF;
  _mm512_mask_storeu_epi16(dataOut->dataExpanded + thisRBAddr, k_WriteMask, dataExpanded);
}

/// Calls compression function specific to the number of RB to be executed. For 9, 10, or 12bit iqWidth.
template <BlockFloatCompander::UnpackFunction networkByteUnpack>
inline void expandByAllocN(const BlockFloatCompander::CompressedData& dataIn,
                           BlockFloatCompander::ExpandedData* dataOut,
                           const int totNumBytesPerRB,
                           const int maxExpShift)
{
  for (int n = 0; n < 16; ++n) {
    applyExpansionN_1RB<networkByteUnpack>(dataIn, dataOut, n * totNumBytesPerRB, n * k_numREReal, maxExpShift);
  }
}
} // namespace BFP_UPlane_9b16RB

/// Main kernel function for compression.
/// Starts by determining iqWidth specific parameters and functions.
void BlockFloatCompander::BFPCompressUserPlaneAvx512_9b16RB(const ExpandedData& dataIn, CompressedData* dataOut)
{
  /// Compensation for extra zeros in 32b leading zero count when computing exponent
  const auto totShiftBits9 = _mm512_set1_epi32(24);

  /// Total number of compressed bytes per RB for each iqWidth option
  constexpr int totNumBytesPerRB9 = 28;

  /// Compressed data write mask for each iqWidth option
  constexpr uint16_t rbWriteMask9 = 0x01FF;

  BFP_UPlane_9b16RB::compressByAllocN<BlockFloatCompander::networkBytePack9b>(dataIn,
                                                                              dataOut,
                                                                              totShiftBits9,
                                                                              totNumBytesPerRB9,
                                                                              rbWriteMask9);
}

/// Main kernel function for expansion.
/// Starts by determining iqWidth specific parameters and functions.
void BlockFloatCompander::BFPExpandUserPlaneAvx512_9b16RB(const CompressedData& dataIn, ExpandedData* dataOut)
{
  constexpr int k_totNumBytesPerRB9 = 28;
  constexpr int k_maxExpShift9 = 7;
  BFP_UPlane_9b16RB::expandByAllocN<BlockFloatCompander::networkByteUnpack9b>(dataIn, dataOut, k_totNumBytesPerRB9, k_maxExpShift9);
}
