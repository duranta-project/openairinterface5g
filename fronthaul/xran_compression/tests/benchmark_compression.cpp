/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <benchmark/benchmark.h>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <iostream>
#include "xran_compression.h"
#include "xran_pkt_api.h"

// Helper to allocate 64-byte aligned memory
static void* aligned_alloc_64(size_t size)
{
  void* ptr = nullptr;
  if (posix_memalign(&ptr, 64, size) != 0) {
    std::cerr << "Failed to allocate aligned memory in benchmark" << std::endl;
    std::exit(1);
  }
  return ptr;
}

static void BM_BfpCompress(benchmark::State& state)
{
  int num_rbs = state.range(0);
  int iq_width = state.range(1);
  const int num_elements_per_rb = 24;
  const int total_elements = num_rbs * num_elements_per_rb;

  int16_t* input_data = static_cast<int16_t*>(aligned_alloc_64(total_elements * sizeof(int16_t) + 64));
  int max_compressed_size = ((3 * iq_width) + 1) * num_rbs;
  int8_t* compressed_data = static_cast<int8_t*>(aligned_alloc_64(max_compressed_size + 64));

  // Populate test data
  for (int i = 0; i < total_elements; ++i) {
    input_data[i] = static_cast<int16_t>((std::rand() % 20000) - 10000);
  }

  xranlib_compress_request comp_req;
  comp_req.data_in = input_data;
  comp_req.numRBs = num_rbs;
  comp_req.numDataElements = num_elements_per_rb;
  comp_req.compMethod = XRAN_COMPMETHOD_BLKFLOAT;
  comp_req.iqWidth = iq_width;
  comp_req.reMask = 0;
  comp_req.csf = 0;
  comp_req.ScaleFactor = 0;
  comp_req.len = total_elements * sizeof(int16_t);

  xranlib_compress_response comp_resp;
  comp_resp.data_out = compressed_data;
  comp_resp.len = 0;

  for (auto _ : state) {
    int32_t status = xranlib_compress(&comp_req, &comp_resp);
    benchmark::DoNotOptimize(status);
    benchmark::DoNotOptimize(comp_resp);
  }

  state.SetItemsProcessed(state.iterations() * num_rbs);
  state.SetBytesProcessed(state.iterations() * total_elements * sizeof(int16_t));

  std::free(input_data);
  std::free(compressed_data);
}

static void BM_BfpDecompress(benchmark::State& state)
{
  int num_rbs = state.range(0);
  int iq_width = state.range(1);
  const int num_elements_per_rb = 24;
  const int total_elements = num_rbs * num_elements_per_rb;

  int16_t* input_data = static_cast<int16_t*>(aligned_alloc_64(total_elements * sizeof(int16_t) + 64));
  int max_compressed_size = ((3 * iq_width) + 1) * num_rbs;
  int8_t* compressed_data = static_cast<int8_t*>(aligned_alloc_64(max_compressed_size + 64));
  int16_t* decompressed_data = static_cast<int16_t*>(aligned_alloc_64(total_elements * sizeof(int16_t) + 64));

  for (int i = 0; i < total_elements; ++i) {
    input_data[i] = static_cast<int16_t>((std::rand() % 20000) - 10000);
  }

  // Compress first to generate input for decompression
  xranlib_compress_request comp_req;
  comp_req.data_in = input_data;
  comp_req.numRBs = num_rbs;
  comp_req.numDataElements = num_elements_per_rb;
  comp_req.compMethod = XRAN_COMPMETHOD_BLKFLOAT;
  comp_req.iqWidth = iq_width;
  comp_req.reMask = 0;
  comp_req.csf = 0;
  comp_req.ScaleFactor = 0;
  comp_req.len = total_elements * sizeof(int16_t);

  xranlib_compress_response comp_resp;
  comp_resp.data_out = compressed_data;
  comp_resp.len = 0;

  xranlib_compress(&comp_req, &comp_resp);

  // Set up decompression request
  xranlib_decompress_request decomp_req;
  decomp_req.data_in = compressed_data;
  decomp_req.numRBs = num_rbs;
  decomp_req.numDataElements = num_elements_per_rb;
  decomp_req.compMethod = XRAN_COMPMETHOD_BLKFLOAT;
  decomp_req.iqWidth = iq_width;
  decomp_req.reMask = 0;
  decomp_req.csf = 0;
  decomp_req.ScaleFactor = 0;
  decomp_req.len = comp_resp.len;
  decomp_req.SprEnable = 0;
  decomp_req.fScale = 1.0f;

  xranlib_decompress_response decomp_resp;
  decomp_resp.data_out = decompressed_data;
  decomp_resp.len = 0;

  for (auto _ : state) {
    int32_t status = xranlib_decompress(&decomp_req, &decomp_resp);
    benchmark::DoNotOptimize(status);
    benchmark::DoNotOptimize(decomp_resp);
  }

  state.SetItemsProcessed(state.iterations() * num_rbs);
  state.SetBytesProcessed(state.iterations() * total_elements * sizeof(int16_t));

  std::free(input_data);
  std::free(compressed_data);
  std::free(decompressed_data);
}

// Common 5G configurations: 106 RBs (40MHz) and 273 RBs (100MHz), with 9-bit and 12-bit BFP compression
BENCHMARK(BM_BfpCompress)->Args({106, 9})->Args({106, 12})->Args({273, 9})->Args({273, 12});
BENCHMARK(BM_BfpDecompress)->Args({106, 9})->Args({106, 12})->Args({273, 9})->Args({273, 12});

BENCHMARK_MAIN();
