/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Google Benchmark suite for 3GPP TS 38.212 clause 5.4.2.1 / 5.4.2.2 NR LDPC
 * rate matching (TX bit selection), RX de-rate matching (SIMD and scalar LLRs),
 * and bit interleaving / de-interleaving across Base Graph 1 and Base Graph 2.
 */

#include "benchmark/benchmark.h"
#include <cstdint>
#include <random>
#include <vector>

extern "C" {
#include "openair1/PHY/CODING/nrLDPC_coding/nrLDPC_coding_segment/nr_rate_matching.h"
#include "common/utils/assertions.h"
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

static void RateMatchingSweepArgs(benchmark::internal::Benchmark *b)
{
  // BG1 lifting sizes (TS 38.212 Table 5.3.2-1)
  const int bg1_zs[] = {24, 52, 104, 176, 256, 320, 384};
  const int bg1_es[] = {576, 1800, 4320, 8640, 12960, 25344, 51840};
  const int rvs[] = {0, 1, 2, 3};
  const int fs[] = {0, 64, 256};

  for (int z : bg1_zs) {
    const int max_f = 20 * z;
    for (int e_len : bg1_es) {
      for (int rv : rvs) {
        for (int f : fs) {
          if (f < max_f)
            b->Args({1, z, e_len, rv, f});
        }
      }
    }
  }

  // BG2 lifting sizes
  const int bg2_zs[] = {16, 36, 72, 128, 192, 256, 384};
  const int bg2_es[] = {360, 900, 1800, 3600, 7200, 14400};
  for (int z : bg2_zs) {
    const int max_f = 8 * z;
    for (int e_len : bg2_es) {
      for (int rv : rvs) {
        for (int f : fs) {
          if (f < max_f)
            b->Args({2, z, e_len, rv, f});
        }
      }
    }
  }
}

static void InterleavingSweepArgs(benchmark::internal::Benchmark *b)
{
  // E must be divisible by Qm in {2, 4, 6, 8} (multiples of 24)
  const int es[] = {240, 960, 1800, 4320, 8640, 12960, 25344, 51840};
  const int qms[] = {2, 4, 6, 8};
  for (int e_len : es) {
    for (int qm : qms) {
      b->Args({e_len, qm});
    }
  }
}

static void BM_nr_rate_matching_tx(benchmark::State &state)
{
  const uint8_t bg = static_cast<uint8_t>(state.range(0));
  const uint16_t z = static_cast<uint16_t>(state.range(1));
  const uint32_t e_len = static_cast<uint32_t>(state.range(2));
  const uint8_t rvidx = static_cast<uint8_t>(state.range(3));
  const uint32_t f = static_cast<uint32_t>(state.range(4));
  const uint32_t n = (bg == 1 ? 66u : 50u) * z;
  const uint32_t f_offset = compute_filler_offset(bg, z, f);

  std::mt19937 rng(42);
  std::vector<uint8_t> d(n);
  for (auto &b : d)
    b = static_cast<uint8_t>(rng() & 1u);
  std::vector<uint8_t> e(e_len, 0);

  for (auto _ : state) {
    int rc = nr_rate_matching_ldpc(0, bg, z, d.data(), e.data(), 1, f, f_offset, rvidx, e_len);
    DevAssert(rc == 0);
    benchmark::DoNotOptimize(e.data());
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * e_len);
  state.SetBytesProcessed(state.iterations() * e_len * sizeof(uint8_t));
}

static void BM_nr_rate_matching_rx_simd(benchmark::State &state)
{
  const uint8_t bg = static_cast<uint8_t>(state.range(0));
  const uint16_t z = static_cast<uint16_t>(state.range(1));
  const uint32_t e_len = static_cast<uint32_t>(state.range(2));
  const uint8_t rvidx = static_cast<uint8_t>(state.range(3));
  const uint32_t f = static_cast<uint32_t>(state.range(4));
  const uint32_t n = (bg == 1 ? 66u : 50u) * z;
  const uint32_t f_offset = compute_filler_offset(bg, z, f);

  std::mt19937 rng(42);
  std::vector<int16_t> soft_in(e_len);
  for (auto &s : soft_in)
    s = static_cast<int16_t>((rng() % 127) - 63);
  std::vector<int16_t> d(n, 0);

  for (auto _ : state) {
    int rc = nr_rate_matching_ldpc_rx_simd(0, bg, z, d.data(), soft_in.data(), 1, rvidx, 1, e_len, f, f_offset);
    DevAssert(rc == 0);
    benchmark::DoNotOptimize(d.data());
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * e_len);
  state.SetBytesProcessed(state.iterations() * e_len * sizeof(int16_t));
}

static void BM_nr_rate_matching_rx_scalar(benchmark::State &state)
{
  const uint8_t bg = static_cast<uint8_t>(state.range(0));
  const uint16_t z = static_cast<uint16_t>(state.range(1));
  const uint32_t e_len = static_cast<uint32_t>(state.range(2));
  const uint8_t rvidx = static_cast<uint8_t>(state.range(3));
  const uint32_t f = static_cast<uint32_t>(state.range(4));
  const uint32_t n = (bg == 1 ? 66u : 50u) * z;
  const uint32_t f_offset = compute_filler_offset(bg, z, f);

  std::mt19937 rng(42);
  std::vector<int16_t> soft_in(e_len);
  for (auto &s : soft_in)
    s = static_cast<int16_t>((rng() % 127) - 63);
  std::vector<int16_t> d(n, 0);

  for (auto _ : state) {
    int rc = nr_rate_matching_ldpc_rx(0, bg, z, d.data(), soft_in.data(), 1, rvidx, 1, e_len, f, f_offset);
    DevAssert(rc == 0);
    benchmark::DoNotOptimize(d.data());
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * e_len);
  state.SetBytesProcessed(state.iterations() * e_len * sizeof(int16_t));
}

static void BM_nr_interleaving_ldpc(benchmark::State &state)
{
  const uint32_t e_len = static_cast<uint32_t>(state.range(0));
  const uint8_t qm = static_cast<uint8_t>(state.range(1));

  std::mt19937 rng(42);
  std::vector<uint8_t> e(e_len);
  for (auto &b : e)
    b = static_cast<uint8_t>(rng() & 1u);
  std::vector<uint8_t> f(e_len, 0);

  for (auto _ : state) {
    nr_interleaving_ldpc(e_len, qm, e.data(), f.data());
    benchmark::DoNotOptimize(f.data());
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * e_len);
  state.SetBytesProcessed(state.iterations() * e_len * sizeof(uint8_t));
}

static void BM_nr_deinterleaving_ldpc(benchmark::State &state)
{
  const uint32_t e_len = static_cast<uint32_t>(state.range(0));
  const uint8_t qm = static_cast<uint8_t>(state.range(1));

  std::mt19937 rng(42);
  std::vector<int16_t> f(e_len);
  for (auto &s : f)
    s = static_cast<int16_t>((rng() % 127) - 63);
  std::vector<int16_t> e(e_len, 0);

  for (auto _ : state) {
    nr_deinterleaving_ldpc(e_len, qm, e.data(), f.data());
    benchmark::DoNotOptimize(e.data());
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * e_len);
  state.SetBytesProcessed(state.iterations() * e_len * sizeof(int16_t));
}

BENCHMARK(BM_nr_rate_matching_tx)->Apply(RateMatchingSweepArgs);
BENCHMARK(BM_nr_rate_matching_rx_simd)->Apply(RateMatchingSweepArgs);
BENCHMARK(BM_nr_rate_matching_rx_scalar)->Apply(RateMatchingSweepArgs);
BENCHMARK(BM_nr_interleaving_ldpc)->Apply(InterleavingSweepArgs);
BENCHMARK(BM_nr_deinterleaving_ldpc)->Apply(InterleavingSweepArgs);

} // namespace

int main(int argc, char **argv)
{
  logInit();
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  return 0;
}
