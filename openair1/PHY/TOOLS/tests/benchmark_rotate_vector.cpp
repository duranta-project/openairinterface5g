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
void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  if (assert) {
    abort();
  } else {
    exit(EXIT_SUCCESS);
  }
}
}
#include <cstdio>
#include "common/utils/LOG/log.h"
#include "benchmark/benchmark.h"
#include "openair1/PHY/TOOLS/phy_test_tools.hpp"

// Undefine macro 'T' from common/utils/T/T.h to prevent macro collision with Highway template parameter names.
#undef T
#include <hwy/highway.h>

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

static void BM_rotate_cpx_vector_shift15(benchmark::State &state)
{
  int vector_size = state.range(0);
  auto input_complex_16 = generate_random_c16(vector_size);
  auto input_alpha = generate_random_c16(1);
  AlignedVector512<c16_t> output;
  output.resize(vector_size);
  int shift = 15;
  for (auto _ : state) {
    rotate_cpx_vector(input_complex_16.data(), input_alpha.data()[0], output.data(), vector_size, shift);
  }
}

BENCHMARK(BM_rotate_cpx_vector)->RangeMultiplier(4)->Range(100, 20000);
BENCHMARK(BM_rotate_cpx_vector_shift15)->RangeMultiplier(4)->Range(100, 20000);

int main(int argc, char** argv)
{
  std::printf("Highway active target: %s\n", hwy::TargetName(HWY_TARGET));
  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  return 0;
}
