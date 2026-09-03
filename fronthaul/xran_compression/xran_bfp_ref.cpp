/*
 * SPDX-License-Identifier: Apache-2.0
 * Original file: Copyright 2020 Intel.
 * Copyright 2026 OpenAirInterface Authors
 */

#include "xran_compression.hpp"
#include "xran_bfp_utils.hpp"
#include <complex>
#include <algorithm>
#include <limits.h>
#include <limits>

static int16_t saturateAbs(int16_t inVal)
{
  int16_t result;
  if (inVal == std::numeric_limits<int16_t>::min()) {
    result = std::numeric_limits<int16_t>::max();
  } else {
    result = (int16_t)std::abs(inVal);
  }
  return result;
}

/// Reference compression
void BlockFloatCompander::BFPCompressRef(const ExpandedData& dataIn, CompressedData* dataOut)
{
  int dataOutIdx = 0;
  int16_t iqMask = (int16_t)((1 << dataIn.iqWidth) - 1);
  int byteShiftUnits = dataIn.iqWidth - 8;

  for (int rb = 0; rb < dataIn.numBlocks; ++rb) {
    /// Find max abs value for this RB
    int16_t maxAbs = 0;
    for (int re = 0; re < dataIn.numDataElements; ++re) {
      auto dataIdx = rb * dataIn.numDataElements + re;
      auto dataAbs = saturateAbs(dataIn.dataExpanded[dataIdx]);
      maxAbs = std::max(maxAbs, dataAbs);
    }

    /// Find exponent and insert into byte stream
    auto thisExp = (uint8_t)(std::max(0, (16 - dataIn.iqWidth + 1 - __lzcnt16(maxAbs))));
    dataOut->dataCompressed[dataOutIdx++] = thisExp;

    /// ARS data by exponent and pack bytes in Network order
    /// This uses a sliding buffer where one or more bytes are
    /// extracted after the insertion of each compressed sample
    static constexpr int k_byteMask = 0xFF;
    int byteShiftVal = -8;
    int byteBuffer = {0};
    for (int re = 0; re < dataIn.numDataElements; ++re) {
      auto dataIdxIn = rb * dataIn.numDataElements + re;
      auto thisRE = dataIn.dataExpanded[dataIdxIn] >> thisExp;
      byteBuffer = (byteBuffer << dataIn.iqWidth) + (int)(thisRE & iqMask);

      byteShiftVal += (8 + byteShiftUnits);
      while (byteShiftVal >= 0) {
        auto thisByte = (uint8_t)((byteBuffer >> byteShiftVal) & k_byteMask);
        dataOut->dataCompressed[dataOutIdx++] = thisByte;
        byteShiftVal -= 8;
      }
    }
  }
  dataOut->iqWidth = dataIn.iqWidth;
  dataOut->numBlocks = dataIn.numBlocks;
  dataOut->numDataElements = dataIn.numDataElements;
}

/// Reference expansion
void BlockFloatCompander::BFPExpandRef(const CompressedData& dataIn, ExpandedData* dataOut)
{
  uint32_t iqMask = (uint32_t)(UINT_MAX - ((1 << (32 - dataIn.iqWidth)) - 1));
  uint32_t byteBuffer = {0};
  int numBytesPerRB = ((dataIn.numDataElements * dataIn.iqWidth) >> 3) + 1;
  int bitPointer = 0;
  int dataIdxOut = 0;

  for (int rb = 0; rb < dataIn.numBlocks; ++rb) {
    auto expIdx = rb * numBytesPerRB;
    auto signExtShift = 32 - dataIn.iqWidth - dataIn.dataCompressed[expIdx];

    for (int b = 0; b < numBytesPerRB - 1; ++b) {
      auto dataIdxIn = (expIdx + 1) + b;
      auto thisByte = (uint16_t)dataIn.dataCompressed[dataIdxIn];
      byteBuffer = (uint32_t)((byteBuffer << 8) + thisByte);
      bitPointer += 8;
      while (bitPointer >= dataIn.iqWidth) {
        /// byteBuffer currently has enough data in it to extract a sample
        /// Shift left first to set sign bit at MSB, then shift right to
        /// sign extend down to iqWidth. Finally recast to int16.
        int32_t thisSample32 = (int32_t)((byteBuffer << (32 - bitPointer)) & iqMask);
        int16_t thisSample = (int16_t)(thisSample32 >> signExtShift);
        bitPointer -= dataIn.iqWidth;
        dataOut->dataExpanded[dataIdxOut++] = thisSample;
      }
    }
  }
  dataOut->iqWidth = dataIn.iqWidth;
  dataOut->numBlocks = dataIn.numBlocks;
  dataOut->numDataElements = dataIn.numDataElements;
}

#ifdef _BBLIB_SPR_
/// Reference expansion
void BlockFloatCompander::BFPExpandRefSpr(const CompressedData& dataIn, ExpandedData* dataOut, float fScale)
{
  uint32_t iqMask = (uint32_t)(UINT_MAX - ((1 << (32 - dataIn.iqWidth)) - 1));
  uint32_t byteBuffer = {0};
  int numBytesPerRB = ((dataIn.numDataElements * dataIn.iqWidth) >> 3) + 1;
  int bitPointer = 0;
  int dataIdxOut = 0;

  for (int rb = 0; rb < dataIn.numBlocks; ++rb) {
    auto expIdx = rb * numBytesPerRB;
    auto signExtShift = 32 - dataIn.iqWidth - dataIn.dataCompressed[expIdx];

    for (int b = 0; b < numBytesPerRB - 1; ++b) {
      auto dataIdxIn = (expIdx + 1) + b;
      auto thisByte = (uint16_t)dataIn.dataCompressed[dataIdxIn];
      byteBuffer = (uint32_t)((byteBuffer << 8) + thisByte);
      bitPointer += 8;
      while (bitPointer >= dataIn.iqWidth) {
        /// byteBuffer currently has enough data in it to extract a sample
        /// Shift left first to set sign bit at MSB, then shift right to
        /// sign extend down to iqWidth. Finally recast to int16.
        int32_t thisSample32 = (int32_t)((byteBuffer << (32 - bitPointer)) & iqMask);
        int16_t thisSample = (int16_t)(thisSample32 >> signExtShift);
        bitPointer -= dataIn.iqWidth;
        dataOut->dataExpanded[dataIdxOut++] = thisSample;
      }
    }
  }

  int16_t* p_data_in = (int16_t*)dataOut->dataExpanded;
  _Float16* p_data_out = (_Float16*)dataOut->dataExpanded;
  for (size_t i = 0; i < dataIdxOut - 1; i++) {
    p_data_out[i] = (_Float16)((float)(p_data_in[i]) / fScale);
  }

  dataOut->iqWidth = dataIn.iqWidth;
  dataOut->numBlocks = dataIn.numBlocks;
  dataOut->numDataElements = dataIn.numDataElements;
}
#endif