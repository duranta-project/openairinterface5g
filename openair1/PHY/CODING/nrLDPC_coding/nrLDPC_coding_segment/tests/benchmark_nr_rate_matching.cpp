/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Google Benchmark suite for 3GPP TS 38.212 clause 5.4.2.1 NR LDPC rate
 * matching (TX bit selection) and RX de-rate matching (SIMD and scalar LLRs)
 * across Base Graph 1 (BG1, Zc = 384) and Base Graph 2 (BG2, Zc = 192).
 */

#include "benchmark/benchmark.h"
#include <cstdint>
#include <random>
#include <vector>

extern "C" {
#include "openair1/PHY/CODING/nrLDPC_coding/nrLDPC_coding_segment/nr_rate_matching.h"
#include "common/utils/LOG/log.h"
int nr_rate_matching_ldpc_rx_simd(uint32_t Tbslbrm,
                                  uint8_t BG,
                                  uint16_t Z,
                                  int16_t *d,
                                  int16_t *soft_input,
                                  uint8_t C,
                                  uint8_t rvidx,
                                  uint8_t clear,
                                  uint32_t E,
                                  uint32_t F,
                                  uint32_t Foffset);
}

namespace {

uint32_t compute_filler_offset(uint8_t bg, uint16_t z, uint32_t f)
{
  const uint32_t k = (bg == 1 ? 22u : 10u) * z;
  return k - f - 2u * z;
}

static void BM_nr_rate_matching_tx(benchmark::State &state)
{
  const uint8_t bg = static_cast<uint8_t>(state.range(0));
  const uint16_t z = (bg == 1) ? 384 : 192;
  const uint32_t e_len = static_cast<uint32_t>(state.range(1));
  const uint8_t rvidx = static_cast<uint8_t>(state.range(2));
  const uint32_t n = (bg == 1 ? 66u : 50u) * z;
  const uint32_t f = 64u;
  const uint32_t f_offset = compute_filler_offset(bg, z, f);

  std::mt19937 rng(42);
  std::vector<uint8_t> d(n);
  for (auto &b : d)
    b = static_cast<uint8_t>(rng() & 1u);
  std::vector<uint8_t> e(e_len, 0);

  for (auto _ : state) {
    int rc = nr_rate_matching_ldpc(0, bg, z, d.data(), e.data(), 1, f, f_offset, rvidx, e_len);
    benchmark::DoNotOptimize(rc);
    benchmark::DoNotOptimize(e.data());
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * e_len);
  state.SetBytesProcessed(state.iterations() * e_len * sizeof(uint8_t));
}

static void BM_nr_rate_matching_rx_simd(benchmark::State &state)
{
  const uint8_t bg = static_cast<uint8_t>(state.range(0));
  const uint16_t z = (bg == 1) ? 384 : 192;
  const uint32_t e_len = static_cast<uint32_t>(state.range(1));
  const uint8_t rvidx = static_cast<uint8_t>(state.range(2));
  const uint32_t n = (bg == 1 ? 66u : 50u) * z;
  const uint32_t f = 64u;
  const uint32_t f_offset = compute_filler_offset(bg, z, f);

  std::mt19937 rng(42);
  std::vector<int16_t> soft_in(e_len);
  for (auto &s : soft_in)
    s = static_cast<int16_t>((rng() % 127) - 63);
  std::vector<int16_t> d(n, 0);

  for (auto _ : state) {
    int rc = nr_rate_matching_ldpc_rx_simd(0, bg, z, d.data(), soft_in.data(), 1, rvidx, 1, e_len, f, f_offset);
    benchmark::DoNotOptimize(rc);
    benchmark::DoNotOptimize(d.data());
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * e_len);
  state.SetBytesProcessed(state.iterations() * e_len * sizeof(int16_t));
}

static void BM_nr_rate_matching_rx_scalar(benchmark::State &state)
{
  const uint8_t bg = static_cast<uint8_t>(state.range(0));
  const uint16_t z = (bg == 1) ? 384 : 192;
  const uint32_t e_len = static_cast<uint32_t>(state.range(1));
  const uint8_t rvidx = static_cast<uint8_t>(state.range(2));
  const uint32_t n = (bg == 1 ? 66u : 50u) * z;
  const uint32_t f = 64u;
  const uint32_t f_offset = compute_filler_offset(bg, z, f);

  std::mt19937 rng(42);
  std::vector<int16_t> soft_in(e_len);
  for (auto &s : soft_in)
    s = static_cast<int16_t>((rng() % 127) - 63);
  std::vector<int16_t> d(n, 0);

  for (auto _ : state) {
    int rc = nr_rate_matching_ldpc_rx(0, bg, z, d.data(), soft_in.data(), 1, rvidx, 1, e_len, f, f_offset);
    benchmark::DoNotOptimize(rc);
    benchmark::DoNotOptimize(d.data());
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * e_len);
  state.SetBytesProcessed(state.iterations() * e_len * sizeof(int16_t));
}

BENCHMARK(BM_nr_rate_matching_tx)
    ->Args({1, 4320, 0})
    ->Args({1, 12960, 0})
    ->Args({1, 25344, 2})
    ->Args({2, 1800, 0})
    ->Args({2, 7200, 1});

BENCHMARK(BM_nr_rate_matching_rx_simd)
    ->Args({1, 4320, 0})
    ->Args({1, 12960, 0})
    ->Args({1, 25344, 2})
    ->Args({2, 1800, 0})
    ->Args({2, 7200, 1});

BENCHMARK(BM_nr_rate_matching_rx_scalar)
    ->Args({1, 4320, 0})
    ->Args({1, 12960, 0})
    ->Args({1, 25344, 2})
    ->Args({2, 1800, 0})
    ->Args({2, 7200, 1});

} // namespace

int main(int argc, char **argv)
{
  logInit();
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  return 0;
}
