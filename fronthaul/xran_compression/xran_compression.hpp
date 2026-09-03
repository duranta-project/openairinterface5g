/*
 * SPDX-License-Identifier: Apache-2.0
 * Original file: Copyright 2020 Intel.
 * Copyright 2026 OpenAirInterface Authors
 */

#pragma once
#include <stdint.h>
#include "simd_wrapper.h"

// This configuration file sets global constants and macros which are
// of general use throughout the project.

// All current IA processors of interest align their cache lines on
// this boundary. If the cache alignment for future processors changes
// then the most restrictive alignment should be set.
constexpr unsigned k_cacheByteAlignment = 64;

// Force the data to which this macro is applied to be aligned on a cache line.
// For example:
//
// CACHE_ALIGNED float data[64];
#define CACHE_ALIGNED alignas(k_cacheByteAlignment)

// Hint to the compiler that the data to which this macro is applied
// can be assumed to be aligned to a cache line. This allows the
// compiler to generate improved code by using aligned reads and
// writes.
#define ASSUME_CACHE_ALIGNED(data)
// __assume_aligned(data, k_cacheByteAlignment);

namespace BlockFloatCompander {
/// Compute 32 RB at a time
static constexpr int k_numBitsIQ = 16;
static constexpr int k_numBitsIQPair = 2 * k_numBitsIQ;
static constexpr int k_maxNumBlocks = 16;
static constexpr int k_maxNumElements = 128;
static constexpr int k_numSampsExpanded = k_maxNumBlocks * k_maxNumElements;
static constexpr int k_numSampsCompressed = (k_numSampsExpanded * 2) + k_maxNumBlocks;

struct CompressedData {
  /// Pointer to compressed data buffer
  CACHE_ALIGNED uint8_t dataCompressedDataOut[k_numSampsCompressed];
  CACHE_ALIGNED uint8_t* dataCompressed;
  /// Size of mantissa including sign bit
  int iqWidth;

  /// Number of BFP blocks in message
  int numBlocks;

  /// Number of data elements per compression block (only required for reference function)
  int numDataElements;
};

struct ExpandedData {
  /// Pointer to expanded data buffer
  CACHE_ALIGNED int16_t dataExpandedIn[k_numSampsExpanded];
  CACHE_ALIGNED int16_t* dataExpanded;

  /// Size of mantissa including sign bit
  int iqWidth;

  /// Number of BFP blocks in message
  int numBlocks;

  /// Number of data elements per compression block (only required for reference function)
  int numDataElements;
};

/// Reference compression and expansion functions
void BFPCompressRef(const ExpandedData& dataIn, CompressedData* dataOut);
void BFPExpandRef(const CompressedData& dataIn, ExpandedData* dataOut);

/// User-Plane specific compression and expansion functions 9b Matissa 16RB ONLY
void BFPCompressUserPlaneAvx512_9b16RB(const ExpandedData& dataIn, CompressedData* dataOut);
void BFPExpandUserPlaneAvx512_9b16RB(const CompressedData& dataIn, ExpandedData* dataOut);

/// User-Plane specific compression and expansion functions
void BFPCompressUserPlaneAvx512(const ExpandedData& dataIn, CompressedData* dataOut);
void BFPExpandUserPlaneAvx512(const CompressedData& dataIn, ExpandedData* dataOut);

} // namespace BlockFloatCompander
