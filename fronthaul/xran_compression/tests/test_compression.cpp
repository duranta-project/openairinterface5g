/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <cassert>
#include <algorithm>
#include "xran_compression.h"
#include "xran_pkt.h"

// Allocate 64-byte aligned memory
void* aligned_alloc_64(size_t size)
{
  void* ptr = nullptr;
  if (posix_memalign(&ptr, 64, size) != 0) {
    std::cerr << "Failed to allocate aligned memory" << std::endl;
    std::exit(1);
  }
  return ptr;
}

void test_bfp_compression(int num_rbs, int iq_width)
{
  std::cout << "Testing BFP compression/decompression with " << num_rbs << " RBs, iqWidth = " << iq_width << "..." << std::endl;

  const int num_elements_per_rb = 24; // 12 REs * 2 (IQ)
  const int total_elements = num_rbs * num_elements_per_rb;

  // Allocate aligned memory for input, compressed and decompressed data (with 64 bytes padding for AVX-512 vector accesses)
  int16_t* input_data = static_cast<int16_t*>(aligned_alloc_64(total_elements * sizeof(int16_t) + 64));
  // Max compressed size is ((3 * iqWidth) + 1) * num_rbs
  int max_compressed_size = ((3 * iq_width) + 1) * num_rbs;
  int8_t* compressed_data = static_cast<int8_t*>(aligned_alloc_64(max_compressed_size + 64));
  int16_t* decompressed_data = static_cast<int16_t*>(aligned_alloc_64(total_elements * sizeof(int16_t) + 64));

  // Generate test data: sine waves with random amplitudes to test scaling/exponent logic
  for (int rb = 0; rb < num_rbs; ++rb) {
    // Random scale for each RB to test exponent selection
    double scale = (std::rand() % 32760) + 7;
    for (int i = 0; i < num_elements_per_rb; ++i) {
      int idx = rb * num_elements_per_rb + i;
      double angle = (2.0 * M_PI * i) / num_elements_per_rb;
      input_data[idx] = static_cast<int16_t>(scale * std::sin(angle));
    }
  }

  // Set up compression request
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

  int32_t compress_status = xranlib_compress(&comp_req, &comp_resp);
  assert(compress_status == 0);
  assert(comp_resp.len > 0);

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

  int32_t decompress_status = xranlib_decompress(&decomp_req, &decomp_resp);
  assert(decompress_status == 0);
  assert(decomp_resp.len == static_cast<int32_t>(total_elements * sizeof(int16_t)));

  // Verify decompression accuracy
  // The maximum quantization error is bounded by 2^(16 - iq_width)
  int max_allowed_err = 1 << (16 - iq_width);
  for (int i = 0; i < total_elements; ++i) {
    int err = std::abs(decompressed_data[i] - input_data[i]);
    if (err > max_allowed_err) {
      std::cerr << "Error too large at index " << i << ": original=" << input_data[i] << ", decompressed=" << decompressed_data[i]
                << ", error=" << err << " (max allowed=" << max_allowed_err << ")" << std::endl;
      assert(false);
    }
  }

  std::free(input_data);
  std::free(compressed_data);
  std::free(decompressed_data);
  std::cout << "Test passed!" << std::endl;
}

int main()
{
  std::srand(42); // Seed random number generator

  // Test different RB counts to trigger 16-RB, 4-RB and 1-RB logic paths
  std::vector<int> rb_counts = {1, 2, 4, 8, 15, 16, 17, 32, 35};
  std::vector<int> iq_widths = {8, 9, 10, 12};

  for (int num_rbs : rb_counts) {
    for (int iq_width : iq_widths) {
      test_bfp_compression(num_rbs, iq_width);
    }
  }

  std::cout << "All BFP unit tests passed successfully!" << std::endl;
  return 0;
}
