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

static void BM_dot_product(benchmark::State &state)
{
  int vector_size = state.range(0);
  auto x = generate_random_c16(vector_size);
  auto y = generate_random_c16(vector_size);
  int shift = 2;
  c32_t result = {0, 0};
  for (auto _ : state) {
    result = dot_product(x.data(), y.data(), vector_size, shift);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(BM_dot_product)->RangeMultiplier(4)->Range(16, 20000);

#if defined(__x86_64__) || defined(__i386__)

// Benchmarks dot_product() as dispatched in production: the AVX512/AVX2/SSE2 tiers inside it are
// each gated at compile time (__AVX512BW__/__AVX2__) and at runtime only on N, so this exercises
// whichever tiers the build was compiled with, across the full size range. The range starts at 4
// (rather than 16) to also cover small, non-16/8-aligned sizes.
static void BM_dot_product_shift15(benchmark::State &state)
{
  int vector_size = state.range(0);
  auto x = generate_random_c16(vector_size);
  auto y = generate_random_c16(vector_size);
  int shift = 15;
  c32_t result = {0, 0};
  for (auto _ : state) {
    result = dot_product(x.data(), y.data(), vector_size, shift);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(BM_dot_product_shift15)->RangeMultiplier(2)->Range(4, 8192);

#endif // defined(__x86_64__) || defined(__i386__)

BENCHMARK_MAIN();
