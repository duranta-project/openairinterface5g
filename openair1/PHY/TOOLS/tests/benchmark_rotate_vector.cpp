/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdint.h>
#include <vector>
#include <algorithm>
#include <numeric>
extern "C" {
#include "openair1/PHY/TOOLS/tools_defs.h"
struct configmodule_interface_s;
struct configmodule_interface_s *uniqCfg = NULL;
}
#include <cstdio>
#include <cstdlib>
#include "common/utils/LOG/log.h"
#include "benchmark/benchmark.h"
#include "openair1/PHY/TOOLS/phy_test_tools.hpp"

static void BM_rotate_cpx_vector(benchmark::State &state)
{
  int vector_size = state.range(0);
  auto input_complex_16 = generate_random_c16(vector_size);
  auto input_alpha = generate_random_c16(1);
  AlignedVector512<c16_t> output;
  output.resize(vector_size);
  int shift = 2;
  for (auto _ : state) {
    rotate_cpx_vector(input_complex_16.data(), input_alpha.data()[0], output.data(), vector_size, shift);
  }
}

BENCHMARK(BM_rotate_cpx_vector)->RangeMultiplier(4)->Range(100, 20000);

#if defined(__x86_64__) || defined(__i386__)

// Scalar reference for the vectorized madd+shift+pack algorithm, matching its saturating
// int32_t->int16_t rounding exactly. c16mulShift() truncates/wraps instead of saturating, so
// it isn't a valid oracle here.
static inline int16_t sat16_ref(int32_t v)
{
  if (v > 32767)
    return 32767;
  if (v < -32768)
    return -32768;
  return (int16_t)v;
}

static void rotate_cpx_vector_ref(const c16_t *const x, const c16_t alpha, c16_t *y, uint32_t N, int output_shift)
{
  for (uint32_t k = 0; k < N; k++) {
    const int32_t re = (int32_t)x[k].r * alpha.r - (int32_t)x[k].i * alpha.i;
    const int32_t im = (int32_t)x[k].r * alpha.i + (int32_t)x[k].i * alpha.r;
    y[k].r = sat16_ref(re >> output_shift);
    y[k].i = sat16_ref(im >> output_shift);
  }
}

// Benchmarks rotate_cpx_vector() as dispatched in production: the AVX512/AVX2 tiers inside it are
// gated at compile time (__AVX512BW__/__AVX2__), so this exercises whichever tiers the build was
// compiled with, across the full size range. The range starts at 4 (rather than 16) to also cover
// small sizes.
static void BM_rotate_cpx_vector_shift15(benchmark::State &state)
{
  int vector_size = state.range(0);
  auto input_complex_16 = generate_random_c16(vector_size);
  auto input_alpha = generate_random_c16(1);
  AlignedVector512<c16_t> output;
  output.resize(vector_size);
  int shift = 15;

  // Correctness verification: must be bit-exact with the scalar reference.
  AlignedVector512<c16_t> output_ref;
  output_ref.resize(vector_size);
  rotate_cpx_vector_ref(input_complex_16.data(), input_alpha.data()[0], output_ref.data(), vector_size, shift);
  rotate_cpx_vector(input_complex_16.data(), input_alpha.data()[0], output.data(), vector_size, shift);
  for (int i = 0; i < vector_size; ++i) {
    if (output_ref[i].r != output[i].r || output_ref[i].i != output[i].i) {
      std::fprintf(stderr,
                    "Mismatch at index %d: ref (%d, %d), got (%d, %d)\n",
                    i,
                    output_ref[i].r,
                    output_ref[i].i,
                    output[i].r,
                    output[i].i);
      std::abort();
    }
  }

  for (auto _ : state) {
    rotate_cpx_vector(input_complex_16.data(), input_alpha.data()[0], output.data(), vector_size, shift);
  }
}

BENCHMARK(BM_rotate_cpx_vector_shift15)->RangeMultiplier(2)->Range(4, 8192);

#endif // defined(__x86_64__) || defined(__i386__)

BENCHMARK_MAIN();
