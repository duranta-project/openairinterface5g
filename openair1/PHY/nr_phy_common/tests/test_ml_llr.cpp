/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// 2-layer ML LLR (nr_compute_ML_llr) must produce LLRs for every RE, whatever the RE count. The SIMD kernels
// process 8 (128-bit) or 16 (256-bit) REs per iteration; with a count that is not a multiple of that, the
// final partial group used to be skipped, leaving stale memory in the LLR buffer (e.g. 1638 REs: the data
// REs of a type-1 DMRS symbol with 273 PRBs).
//
// The ML metric is computed independently per RE, so running on n REs must give the same LLRs for those REs
// as running on n rounded up to a multiple of 16 with the same input.

#include "gtest/gtest.h"
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

extern "C" {
#include "nr_compute_llr.h"
#include "common/utils/LOG/log.h"
struct configmodule_interface_s;
struct configmodule_interface_s *uniqCfg = NULL;
void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  abort();
}
}

namespace {

constexpr int16_t sentinel = 0x7a5a;

uint32_t ceil16(uint32_t n)
{
  return (n + 15) & ~15U;
}

template <typename T>
struct aligned_buf {
  explicit aligned_buf(size_t n) : v(n + 64 / sizeof(T)) { p = (T *)(((uintptr_t)v.data() + 63) & ~(uintptr_t)63); }
  std::vector<T> v;
  T *p;
};

void check_ml_llr(uint8_t mod_order, uint32_t nb_re, std::mt19937 &rng)
{
  SCOPED_TRACE(::testing::Message() << "mod_order " << int(mod_order) << " nb_re " << nb_re);
  // callers pad every per-symbol buffer to a multiple of 16 REs
  const uint32_t len = ceil16(nb_re);
  aligned_buf<c16_t> comp0(len), comp1(len), mag0(len), mag1(len), rho0(len), rho1(len);
  std::uniform_int_distribution<int> sym(-2000, 2000), mag(500, 1500), rho(-300, 300);
  for (uint32_t i = 0; i < len; i++) {
    comp0.p[i] = {(int16_t)sym(rng), (int16_t)sym(rng)};
    comp1.p[i] = {(int16_t)sym(rng), (int16_t)sym(rng)};
    const int16_t m0 = mag(rng), m1 = mag(rng);
    mag0.p[i] = {m0, m0};
    mag1.p[i] = {m1, m1};
    rho0.p[i] = {(int16_t)rho(rng), (int16_t)rho(rng)};
    rho1.p[i] = {rho0.p[i].r, (int16_t)-rho0.p[i].i};
  }

  auto run = [&](uint32_t n, aligned_buf<int16_t> &llr0, aligned_buf<int16_t> &llr1) {
    for (uint32_t i = 0; i < len * mod_order; i++)
      llr0.p[i] = llr1.p[i] = sentinel;
    nr_compute_ML_llr(comp0.p, comp1.p, mag0.p, mag1.p, llr0.p, llr1.p, rho0.p, rho1.p, n, mod_order);
  };

  aligned_buf<int16_t> llr0(len * mod_order), llr1(len * mod_order), ref0(len * mod_order), ref1(len * mod_order);
  run(nb_re, llr0, llr1);
  run(len, ref0, ref1);
  for (uint32_t k = 0; k < nb_re * mod_order; k++) {
    ASSERT_EQ(llr0.p[k], ref0.p[k]) << "layer 0, RE " << k / mod_order << " bit " << k % mod_order;
    ASSERT_EQ(llr1.p[k], ref1.p[k]) << "layer 1, RE " << k / mod_order << " bit " << k % mod_order;
  }
}

} // namespace

class ml_llr : public ::testing::TestWithParam<uint8_t> {};

TEST_P(ml_llr, every_re_count)
{
  std::mt19937 rng(GetParam());
  for (uint32_t nb_re = 1; nb_re <= 96; nb_re++)
    check_ml_llr(GetParam(), nb_re, rng);
}

// PUSCH/PDSCH symbol sizes: type-1 DMRS symbol with 273 / 107 PRBs (6 data REs per PRB) and full symbols
TEST_P(ml_llr, symbol_sizes)
{
  std::mt19937 rng(100 + GetParam());
  for (uint32_t nb_rb : {1u, 3u, 7u, 105u, 106u, 107u, 272u, 273u}) {
    check_ml_llr(GetParam(), 6 * nb_rb, rng);
    check_ml_llr(GetParam(), 12 * nb_rb, rng);
  }
}

INSTANTIATE_TEST_SUITE_P(nr_compute_ML_llr, ml_llr, ::testing::Values(2, 4, 6), [](const auto &info) {
  return "qam" + std::to_string(1 << info.param);
});

int main(int argc, char **argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
