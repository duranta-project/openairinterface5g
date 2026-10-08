/*
 * SPDX-License-Identifier: Apache-2.0
 * Original file: Copyright 2020 Intel.
 * Copyright 2026 OpenAirInterface Authors
 */

#include "xran_compression.hpp"
#include "xran_compression.h"
#include "xran_pkt_api.h"
#include <complex>
#include <algorithm>
#include "simd_wrapper.h"
#include <limits.h>
#include <cstring>
#include <stdint.h>

using namespace BlockFloatCompander;

/** callback function type for Symbol packet */
typedef void (*xran_bfp_compress_fn)(const BlockFloatCompander::ExpandedData &dataIn, BlockFloatCompander::CompressedData *dataOut);

/** callback function type for Symbol packet */
typedef void (*xran_bfp_decompress_fn)(const BlockFloatCompander::CompressedData &dataIn,
                                       BlockFloatCompander::ExpandedData *dataOut);

int32_t xranlib_compress(const struct xranlib_compress_request *request, struct xranlib_compress_response *response)
{
  if (request->compMethod == XRAN_COMPMETHOD_MODULATION) {
    return -1;
  } else {
    return xranlib_compress_avx512(request, response);
  }
}

int32_t xranlib_decompress(const struct xranlib_decompress_request *request, struct xranlib_decompress_response *response)
{
  if (request->compMethod == XRAN_COMPMETHOD_MODULATION) {
    return -1;
  } else {
    return xranlib_decompress_avx512(request, response);
  }
}

int32_t xranlib_compress_avx512(const struct xranlib_compress_request *request, struct xranlib_compress_response *response)
{
  BlockFloatCompander::ExpandedData expandedDataInput;
  BlockFloatCompander::CompressedData compressedDataOut;
  xran_bfp_compress_fn com_fn = NULL;
  uint16_t totalRBs = request->numRBs;
  uint16_t remRBs = totalRBs;
  int16_t len = 0;
  int16_t block_idx_bytes = 0;

  switch (request->iqWidth) {
    case 8:
    case 9:
    case 10:
    case 12:
      com_fn = BlockFloatCompander::BFPCompressUserPlaneAvx512;
      break;
    default:
      com_fn = BlockFloatCompander::BFPCompressRef;
      break;
  }

  expandedDataInput.iqWidth = request->iqWidth;
  expandedDataInput.numDataElements = 24;

  while (remRBs) {
    expandedDataInput.dataExpanded = &request->data_in[block_idx_bytes];
    compressedDataOut.dataCompressed = (uint8_t *)&response->data_out[len];
    if (remRBs >= 16) {
      expandedDataInput.numBlocks = 16;
      com_fn(expandedDataInput, &compressedDataOut);
      len += ((3 * expandedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_maxNumBlocks, (int16_t)16);
      block_idx_bytes += 16 * expandedDataInput.numDataElements;
      remRBs -= 16;
    } else if (remRBs >= 4) {
      expandedDataInput.numBlocks = 4;
      com_fn(expandedDataInput, &compressedDataOut);
      len += ((3 * expandedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_maxNumBlocks, (int16_t)4);
      block_idx_bytes += 4 * expandedDataInput.numDataElements;
      remRBs -= 4;
    } else if (remRBs >= 1) {
      expandedDataInput.numBlocks = 1;
      com_fn(expandedDataInput, &compressedDataOut);
      len += ((3 * expandedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_maxNumBlocks, (int16_t)1);
      block_idx_bytes += 1 * expandedDataInput.numDataElements;
      remRBs = remRBs - 1;
    }
  }

  response->len = ((3 * expandedDataInput.iqWidth) + 1) * totalRBs;

  return 0;
}

int32_t xranlib_decompress_avx512(const struct xranlib_decompress_request *request, struct xranlib_decompress_response *response)
{
  BlockFloatCompander::CompressedData compressedDataInput;
  BlockFloatCompander::ExpandedData expandedDataOut;

  xran_bfp_decompress_fn decom_fn = NULL;
  uint16_t totalRBs = request->numRBs;
  uint16_t remRBs = totalRBs;
  int16_t len = 0;
  int16_t block_idx_bytes = 0;

  switch (request->iqWidth) {
    case 8:
    case 9:
    case 10:
    case 12:
      decom_fn = BlockFloatCompander::BFPExpandUserPlaneAvx512;
      break;
    default:
      decom_fn = BlockFloatCompander::BFPExpandRef;
      break;
  }

  compressedDataInput.iqWidth = request->iqWidth;
  compressedDataInput.numDataElements = 24;

  while (remRBs) {
    compressedDataInput.dataCompressed = (uint8_t *)&request->data_in[block_idx_bytes];
    expandedDataOut.dataExpanded = &response->data_out[len];
    if (remRBs >= 16) {
      compressedDataInput.numBlocks = 16;
      decom_fn(compressedDataInput, &expandedDataOut);
      len += 16 * compressedDataInput.numDataElements;
      block_idx_bytes +=
          ((3 * compressedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_maxNumBlocks, (int16_t)16);
      remRBs -= 16;
    } else if (remRBs >= 4) {
      compressedDataInput.numBlocks = 4;
      decom_fn(compressedDataInput, &expandedDataOut);
      len += 4 * compressedDataInput.numDataElements;
      block_idx_bytes +=
          ((3 * compressedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_maxNumBlocks, (int16_t)4);
      remRBs -= 4;
    } else if (remRBs >= 1) {
      compressedDataInput.numBlocks = 1;
      decom_fn(compressedDataInput, &expandedDataOut);
      len += 1 * compressedDataInput.numDataElements;
      block_idx_bytes +=
          ((3 * compressedDataInput.iqWidth) + 1) * std::min((int16_t)BlockFloatCompander::k_maxNumBlocks, (int16_t)1);
      remRBs = remRBs - 1;
    }
  }

  response->len = totalRBs * compressedDataInput.numDataElements * sizeof(int16_t);

  return 0;
}

int32_t xranlib_compress_avx512_bfw(const struct xranlib_compress_request *request, struct xranlib_compress_response *response)
{
  return -1;
}

int32_t xranlib_decompress_avx512_bfw(const struct xranlib_decompress_request *request,
                                      struct xranlib_decompress_response *response)
{
  return -1;
}
