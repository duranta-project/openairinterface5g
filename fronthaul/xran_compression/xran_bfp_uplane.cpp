/*
 * SPDX-License-Identifier: Apache-2.0
 * Original file: Copyright 2020 Intel.
 * Copyright 2026 OpenAirInterface Authors
 */

#include "xran_compression.hpp"
#include "xran_bfp_utils.hpp"
#include "xran_bfp_byte_packing_utils.hpp"
#include <complex>
#include <algorithm>
#include "simd_wrapper.h"

namespace BFP_UPlane {
/// Namespace constants
const int k_numREReal = 24; /// 12 IQ pairs

/// Compute exponent value for a set of 16 RB from the maximum absolute value.
/// Max Abs operates in a loop, executing 4 RB per iteration. The results are
/// packed into the final output register.
__m512i computeExponent_16RB(const BlockFloatCompander::ExpandedData& dataIn, const __m512i totShiftBits)
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

/// Compute exponent value for a set of 4 RB from the maximum absolute value.
/// Note that we do not need to perform any packing of result as we are only
/// computing 4 RB. The appropriate offset is taken later when extracting the
/// exponent.
__m512i computeExponent_4RB(const BlockFloatCompander::ExpandedData& dataIn, const __m512i totShiftBits)
{
  const __m512i* rawData = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
  /// Re-order and vertical max abs
  const auto maxAbsVert = BlockFloatCompander::maxAbsVertical4RB(rawData[0], rawData[1], rawData[2]);
  /// Horizontal max abs
  const auto maxAbsHorz = BlockFloatCompander::horizontalMax4x16(maxAbsVert);
  /// Calculate exponent
  const auto maxAbs = BlockFloatCompander::maskUpperWord(maxAbsHorz);
  return BlockFloatCompander::expLzCnt(maxAbs, totShiftBits);
}

/// Compute exponent value for 1 RB from the maximum absolute value.
/// This works with horizontal max abs only, and needs to include a
/// step to select the final exponent from the 4 lanes.
uint8_t computeExponent_1RB(const BlockFloatCompander::ExpandedData& dataIn, const __m512i totShiftBits)
{
  const __m512i* rawData = reinterpret_cast<const __m512i*>(dataIn.dataExpanded);
  /// Abs
  const auto rawDataAbs = _mm512_abs_epi16(_mm512_loadu_epi16(rawData));
  /// No need to do a full horizontal max operation here, just do a max IQ step,
  /// compute the exponents and then use a reduce max over all exponent values. This
  /// is the fastest way to handle a single RB.
  const auto rawAbsIQSwap = _mm512_rol_epi32(rawDataAbs, BlockFloatCompander::k_numBitsIQ);
  const auto maxAbsIQ = _mm512_max_epi16(rawDataAbs, rawAbsIQSwap);
  /// Calculate exponent
  const auto maxAbsIQ32 = BlockFloatCompander::maskUpperWord(maxAbsIQ);
  const auto exps = BlockFloatCompander::expLzCnt(maxAbsIQ32, totShiftBits);
  /// At this point we have exponent values for the maximum of each IQ pair.
  /// Run a reduce max step to compute the maximum exponent value in the first
  /// three lanes - this will give the desired exponent for this RB.
  constexpr uint16_t k_expMsk = 0x0FFF;
  return (uint8_t)_mm512_mask_reduce_max_epi32(k_expMsk, exps);
}

/// Apply compression to 1 RB
template <BlockFloatCompander::PackFunction networkBytePack>
void applyCompressionN_1RB(const BlockFloatCompander::ExpandedData& dataIn,
                           BlockFloatCompander::CompressedData* dataOut,
                           const int numREOffset,
                           const uint8_t thisExp,
                           const int thisRBExpAddr,
                           const uint16_t rbWriteMask)
{
  /// Get AVX512 pointer aligned to desired RB
  const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(dataIn.dataExpanded + numREOffset);
  /// Apply the exponent shift
  const auto compData = _mm512_srai_epi16(_mm512_loadu_si512(rawDataIn), thisExp);
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

/// Apply 9, 10, or 12bit compression to 16 RB
template <BlockFloatCompander::PackFunction networkBytePack>
void compressN_16RB(const BlockFloatCompander::ExpandedData& dataIn,
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

/// Apply 9, 10, or 12bit compression to 4 RB
template <BlockFloatCompander::PackFunction networkBytePack>
void compressN_4RB(const BlockFloatCompander::ExpandedData& dataIn,
                   BlockFloatCompander::CompressedData* dataOut,
                   const __m512i totShiftBits,
                   const int totNumBytesPerRB,
                   const uint16_t rbWriteMask)
{
  const auto exponents = computeExponent_4RB(dataIn, totShiftBits);
  for (int n = 0; n < 4; ++n) {
    applyCompressionN_1RB<networkBytePack>(dataIn,
                                           dataOut,
                                           n * k_numREReal,
                                           ((uint8_t*)&exponents)[n * 16],
                                           n * totNumBytesPerRB,
                                           rbWriteMask);
  }
}

/// Apply 9, 10, or 12bit compression to 1 RB
template <BlockFloatCompander::PackFunction networkBytePack>
void compressN_1RB(const BlockFloatCompander::ExpandedData& dataIn,
                   BlockFloatCompander::CompressedData* dataOut,
                   const __m512i totShiftBits,
                   const int totNumBytesPerRB,
                   const uint16_t rbWriteMask)
{
  const auto thisExponent = computeExponent_1RB(dataIn, totShiftBits);
  applyCompressionN_1RB<networkBytePack>(dataIn, dataOut, 0, thisExponent, 0, rbWriteMask);
}

/// Calls compression function specific to the number of RB to be executed. For 9, 10, or 12bit iqWidth.
template <BlockFloatCompander::PackFunction networkBytePack>
void compressByAllocN(const BlockFloatCompander::ExpandedData& dataIn,
                      BlockFloatCompander::CompressedData* dataOut,
                      const __m512i totShiftBits,
                      const int totNumBytesPerRB,
                      const uint16_t rbWriteMask)
{
  switch (dataIn.numBlocks) {
    case 16:
      compressN_16RB<networkBytePack>(dataIn, dataOut, totShiftBits, totNumBytesPerRB, rbWriteMask);
      break;

    case 4:
      compressN_4RB<networkBytePack>(dataIn, dataOut, totShiftBits, totNumBytesPerRB, rbWriteMask);
      break;

    case 1:
      compressN_1RB<networkBytePack>(dataIn, dataOut, totShiftBits, totNumBytesPerRB, rbWriteMask);
      break;
  }
}

/// Apply compression to 1 RB
void applyCompression8_1RB(const BlockFloatCompander::ExpandedData& dataIn,
                           BlockFloatCompander::CompressedData* dataOut,
                           const int numREOffset,
                           const uint8_t thisExp,
                           const int thisRBExpAddr)
{
  /// Get AVX512 pointer aligned to desired RB
  const __m512i* rawDataIn = reinterpret_cast<const __m512i*>(dataIn.dataExpanded + numREOffset);
  /// Apply the exponent shift
  const auto compData = _mm512_srai_epi16(_mm512_loadu_si512(rawDataIn), thisExp);
  /// Store exponent first
  dataOut->dataCompressed[thisRBExpAddr] = thisExp;
  /// Now have 1 RB worth of bytes separated into 3 chunks (1 per lane)
  /// Use three offset stores to join
  constexpr uint32_t k_rbMask = 0x00FFFFFF; // Write mask for 1RB (24 values)
  _mm256_mask_storeu_epi8(dataOut->dataCompressed + thisRBExpAddr + 1, k_rbMask, _mm512_cvtepi16_epi8(compData));
}

/// 8bit RB compression loop for 16 RB
void compress8_16RB(const BlockFloatCompander::ExpandedData& dataIn,
                    BlockFloatCompander::CompressedData* dataOut,
                    const __m512i totShiftBits)
{
  const auto exponents = computeExponent_16RB(dataIn, totShiftBits);
  for (int n = 0; n < 16; ++n) {
    applyCompression8_1RB(dataIn, dataOut, n * k_numREReal, ((uint8_t*)&exponents)[n * 4], n * (k_numREReal + 1));
  }
}

/// 8bit RB compression loop for 4 RB
void compress8_4RB(const BlockFloatCompander::ExpandedData& dataIn,
                   BlockFloatCompander::CompressedData* dataOut,
                   const __m512i totShiftBits)
{
  const auto exponents = computeExponent_4RB(dataIn, totShiftBits);
  for (int n = 0; n < 4; ++n) {
    applyCompression8_1RB(dataIn, dataOut, n * k_numREReal, ((uint8_t*)&exponents)[n * 16], n * (k_numREReal + 1));
  }
}

/// 8bit RB compression loop for 4 RB
void compress8_1RB(const BlockFloatCompander::ExpandedData& dataIn,
                   BlockFloatCompander::CompressedData* dataOut,
                   const __m512i totShiftBits)
{
  const auto thisExponent = computeExponent_1RB(dataIn, totShiftBits);
  applyCompression8_1RB(dataIn, dataOut, 0, thisExponent, 0);
}

/// Calls compression function specific to the number of RB to be executed. For 8 bit iqWidth.
void compressByAlloc8(const BlockFloatCompander::ExpandedData& dataIn,
                      BlockFloatCompander::CompressedData* dataOut,
                      const __m512i totShiftBits)
{
  switch (dataIn.numBlocks) {
    case 16:
      compress8_16RB(dataIn, dataOut, totShiftBits);
      break;

    case 4:
      compress8_4RB(dataIn, dataOut, totShiftBits);
      break;

    case 1:
      compress8_1RB(dataIn, dataOut, totShiftBits);
      break;
  }
}

/// Apply compression to 1 RB
template <BlockFloatCompander::UnpackFunction networkByteUnpack>
void applyExpansionN_1RB(const BlockFloatCompander::CompressedData& dataIn,
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
void expandByAllocN(const BlockFloatCompander::CompressedData& dataIn,
                    BlockFloatCompander::ExpandedData* dataOut,
                    const int totNumBytesPerRB,
                    const int maxExpShift)
{
  switch (dataIn.numBlocks) {
    case 16:
      for (int n = 0; n < 16; ++n) {
        applyExpansionN_1RB<networkByteUnpack>(dataIn, dataOut, n * totNumBytesPerRB, n * k_numREReal, maxExpShift);
      }
      break;

    case 4:
      for (int n = 0; n < 4; ++n) {
        applyExpansionN_1RB<networkByteUnpack>(dataIn, dataOut, n * totNumBytesPerRB, n * k_numREReal, maxExpShift);
      }
      break;

    case 1:
      applyExpansionN_1RB<networkByteUnpack>(dataIn, dataOut, 0, 0, maxExpShift);
      break;
  }
}

/// Apply expansion to 1 RB and store
void applyExpansion8_1RB(const BlockFloatCompander::CompressedData& dataIn,
                         BlockFloatCompander::ExpandedData* dataOut,
                         const int expAddr,
                         const int thisRBAddr)
{
  const __m256i* rawDataIn = reinterpret_cast<const __m256i*>(dataIn.dataCompressed + expAddr + 1);
  const auto compData16 = _mm512_cvtepi8_epi16(_mm256_loadu_si256(rawDataIn));
  const auto expData = _mm512_slli_epi16(compData16, *(dataIn.dataCompressed + expAddr));
  constexpr uint8_t k_rbMask64 = 0b00111111; // 64b write mask for 1RB (24 int16 values)
  _mm512_mask_storeu_epi64(dataOut->dataExpanded + thisRBAddr, k_rbMask64, expData);
}

/// Calls expansion function specific to the number of RB to be executed. For 8 bit iqWidth.
void expandByAlloc8(const BlockFloatCompander::CompressedData& dataIn, BlockFloatCompander::ExpandedData* dataOut)
{
  switch (dataIn.numBlocks) {
    case 16:
      for (int n = 0; n < 16; ++n) {
        applyExpansion8_1RB(dataIn, dataOut, n * (k_numREReal + 1), n * k_numREReal);
      }
      break;

    case 4:
      for (int n = 0; n < 4; ++n) {
        applyExpansion8_1RB(dataIn, dataOut, n * (k_numREReal + 1), n * k_numREReal);
      }
      break;

    case 1:
      applyExpansion8_1RB(dataIn, dataOut, 0, 0);
      break;
  }
}
} // namespace BFP_UPlane

/// Main kernel function for compression.
/// Starts by determining iqWidth specific parameters and functions.
void BlockFloatCompander::BFPCompressUserPlaneAvx512(const ExpandedData& dataIn, CompressedData* dataOut)
{
  /// Compensation for extra zeros in 32b leading zero count when computing exponent
  const auto totShiftBits8 = _mm512_set1_epi32(25);
  const auto totShiftBits9 = _mm512_set1_epi32(24);
  const auto totShiftBits10 = _mm512_set1_epi32(23);
  const auto totShiftBits12 = _mm512_set1_epi32(21);

  /// Total number of compressed bytes per RB for each iqWidth option
  constexpr int totNumBytesPerRB9 = 28;
  constexpr int totNumBytesPerRB10 = 31;
  constexpr int totNumBytesPerRB12 = 37;

  /// Compressed data write mask for each iqWidth option
  constexpr uint16_t rbWriteMask9 = 0x01FF;
  constexpr uint16_t rbWriteMask10 = 0x03FF;
  constexpr uint16_t rbWriteMask12 = 0x0FFF;

  switch (dataIn.iqWidth) {
    case 8:
      BFP_UPlane::compressByAlloc8(dataIn, dataOut, totShiftBits8);
      break;

    case 9:
      BFP_UPlane::compressByAllocN<BlockFloatCompander::networkBytePack9b>(dataIn,
                                                                           dataOut,
                                                                           totShiftBits9,
                                                                           totNumBytesPerRB9,
                                                                           rbWriteMask9);
      break;

    case 10:
      BFP_UPlane::compressByAllocN<BlockFloatCompander::networkBytePack10b>(dataIn,
                                                                            dataOut,
                                                                            totShiftBits10,
                                                                            totNumBytesPerRB10,
                                                                            rbWriteMask10);
      break;

    case 12:
      BFP_UPlane::compressByAllocN<BlockFloatCompander::networkBytePack12b>(dataIn,
                                                                            dataOut,
                                                                            totShiftBits12,
                                                                            totNumBytesPerRB12,
                                                                            rbWriteMask12);
      break;
  }
}

/// Main kernel function for expansion.
/// Starts by determining iqWidth specific parameters and functions.
void BlockFloatCompander::BFPExpandUserPlaneAvx512(const CompressedData& dataIn, ExpandedData* dataOut)
{
  constexpr int k_totNumBytesPerRB9 = 28;
  constexpr int k_totNumBytesPerRB10 = 31;
  constexpr int k_totNumBytesPerRB12 = 37;

  constexpr int k_maxExpShift9 = 7;
  constexpr int k_maxExpShift10 = 6;
  constexpr int k_maxExpShift12 = 4;

  switch (dataIn.iqWidth) {
    case 8:
      BFP_UPlane::expandByAlloc8(dataIn, dataOut);
      break;

    case 9:
      BFP_UPlane::expandByAllocN<BlockFloatCompander::networkByteUnpack9b>(dataIn, dataOut, k_totNumBytesPerRB9, k_maxExpShift9);
      break;

    case 10:
      BFP_UPlane::expandByAllocN<BlockFloatCompander::networkByteUnpack10b>(dataIn, dataOut, k_totNumBytesPerRB10, k_maxExpShift10);
      break;

    case 12:
      BFP_UPlane::expandByAllocN<BlockFloatCompander::networkByteUnpack12b>(dataIn, dataOut, k_totNumBytesPerRB12, k_maxExpShift12);
      break;
  }
}
