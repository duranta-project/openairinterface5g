/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// LDPC rate matching (TS 38.212 5.4.2.1) against a bit-by-bit reference of the circular-buffer
// bit selection, including E shorter than the filler offset (E < Foffset).

#include "gtest/gtest.h"
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

struct rm_case {
  uint8_t BG;
  uint16_t Z;
  uint32_t F;
  uint8_t rv;
  uint32_t Tbslbrm;
  uint8_t C;
  uint32_t E;
};

uint32_t filler_offset(const rm_case &c)
{
  const uint32_t K = (c.BG == 1 ? 22 : 10) * c.Z;
  return K - c.F - 2 * c.Z;
}

// Circular-buffer positions selected for the E output bits, NULL (filler) bits skipped
std::vector<uint32_t> reference_positions(const rm_case &c)
{
  const nr_ldpc_geometry_t geo = nr_ldpc_soft_buffer_geometry(c.Tbslbrm, c.BG, c.Z, c.C, c.rv);
  const uint32_t Foffset = filler_offset(c);
  std::vector<uint32_t> pos;
  for (uint32_t j = 0; pos.size() < c.E; j++) {
    const uint32_t idx = (geo.k0 + j) % geo.Ncb;
    if (idx < Foffset || idx >= Foffset + c.F)
      pos.push_back(idx);
  }
  return pos;
}

constexpr uint32_t guard = 64;
constexpr uint8_t sentinel = 0xa5;

void check_tx(const rm_case &c, std::mt19937 &rng)
{
  const uint32_t N = (c.BG == 1 ? 66 : 50) * c.Z;
  std::vector<uint8_t> d(N);
  for (auto &b : d)
    b = rng() & 0xff;
  std::vector<uint8_t> e(c.E + guard, sentinel);
  ASSERT_EQ(nr_rate_matching_ldpc(c.Tbslbrm, c.BG, c.Z, d.data(), e.data(), c.C, c.F, filler_offset(c), c.rv, c.E), 0);

  const std::vector<uint32_t> pos = reference_positions(c);
  for (uint32_t k = 0; k < c.E; k++)
    ASSERT_EQ(e[k], d[pos[k]]) << "k " << k;
  for (uint32_t k = c.E; k < c.E + guard; k++)
    ASSERT_EQ(e[k], sentinel) << "write past E at " << k;
}

using rx_fn = int (*)(uint32_t, uint8_t, uint16_t, int16_t *, int16_t *, uint8_t, uint8_t, uint8_t, uint32_t, uint32_t, uint32_t);

void check_rx(rx_fn rx, const rm_case &c, std::mt19937 &rng)
{
  const uint32_t N = (c.BG == 1 ? 66 : 50) * c.Z;
  // values small enough that repetitions never saturate, so the scalar path (no saturation) matches
  std::vector<int16_t> soft(c.E + guard);
  for (auto &s : soft)
    s = int16_t(int(rng() % 201) - 100);
  std::vector<int16_t> d(N + guard, 0x1234);
  ASSERT_EQ(rx(c.Tbslbrm, c.BG, c.Z, d.data(), soft.data(), c.C, c.rv, 1, c.E, c.F, filler_offset(c)), 0);

  std::vector<int16_t> ref(N, 0);
  const std::vector<uint32_t> pos = reference_positions(c);
  for (uint32_t k = 0; k < c.E; k++)
    ref[pos[k]] += soft[k];
  for (uint32_t i = 0; i < N; i++)
    ASSERT_EQ(d[i], ref[i]) << "position " << i;
  for (uint32_t i = N; i < N + guard; i++)
    ASSERT_EQ(d[i], 0x1234) << "write past N at " << i;
}

void check_all(const rm_case &c, std::mt19937 &rng)
{
  SCOPED_TRACE(::testing::Message() << "BG " << int(c.BG) << " Z " << c.Z << " F " << c.F << " Foffset " << filler_offset(c)
                                    << " rv " << int(c.rv) << " Tbslbrm " << c.Tbslbrm << " C " << int(c.C) << " E " << c.E);
  check_tx(c, rng);
  check_rx(nr_rate_matching_ldpc_rx_simd, c, rng);
  check_rx(nr_rate_matching_ldpc_rx, c, rng); // scalar for BG2
}

} // namespace

// rv0 retransmission on a smaller allocation: E well below the filler offset
TEST(nr_rate_matching, short_e_below_filler_offset)
{
  std::mt19937 rng(1);
  for (uint8_t BG = 1; BG <= 2; BG++)
    for (uint8_t rv = 0; rv < 4; rv++)
      for (uint32_t E : {1u, 7u, 100u, 1000u, 2800u}) {
        const rm_case c = {BG, 384, 200, rv, 0, 1, E};
        ASSERT_GT(filler_offset(c), E);
        check_all(c, rng);
      }
}

// Filler runs to the end of the circular buffer (Foffset + F == Ncb) and E forces wrapping
TEST(nr_rate_matching, filler_at_end_of_buffer)
{
  std::mt19937 rng(2);
  const uint16_t Z = 24; // multiple of 3 so that Tbslbrm = 2 * Ncb / 3 is exact
  for (uint8_t BG = 1; BG <= 2; BG++) {
    const uint32_t K = (BG == 1 ? 22 : 10) * Z;
    const uint32_t F = 40;
    // LBRM Ncb == K - 2Z, so Foffset + F == Ncb and the filler is the tail of the buffer
    const uint32_t Ncb = K - 2 * Z;
    const uint8_t C = 1;
    const uint32_t Tbslbrm = 2 * C * Ncb / 3;
    for (uint8_t rv = 0; rv < 4; rv++)
      for (uint32_t E : {1u, Ncb - F - 1, Ncb - F, Ncb - F + 1, 3 * Ncb}) {
        const rm_case c = {BG, Z, F, rv, Tbslbrm, C, E};
        ASSERT_EQ(nr_ldpc_soft_buffer_geometry(Tbslbrm, BG, Z, C, rv).Ncb, Ncb);
        check_all(c, rng);
      }
  }
}

TEST(nr_rate_matching, invalid_filler_interval_rejected)
{
  const uint16_t Z = 24;
  const uint32_t F = 40;
  uint8_t d[66 * Z] = {0}, e[16];
  int16_t ds[66 * Z] = {0}, soft[16] = {0};
  for (uint8_t BG = 1; BG <= 2; BG++) {
    const uint32_t Foffset = (BG == 1 ? 22 : 10) * Z - F - 2 * Z;
    // LBRM Ncb 8 bits short of the end of the filler
    const uint32_t Tbslbrm = 2 * (Foffset + F - 8) / 3;
    EXPECT_EQ(nr_rate_matching_ldpc(Tbslbrm, BG, Z, d, e, 1, F, Foffset, 0, sizeof(e)), -1);
    EXPECT_EQ(nr_rate_matching_ldpc_rx(Tbslbrm, BG, Z, ds, soft, 1, 0, 1, 16, F, Foffset), -1);
    EXPECT_EQ(nr_rate_matching_ldpc_rx_simd(Tbslbrm, BG, Z, ds, soft, 1, 0, 1, 16, F, Foffset), -1);
  }
}

TEST(nr_rate_matching, random)
{
  std::mt19937 rng(3);
  const uint16_t Zs[] = {2, 3, 5, 8, 15, 26, 52, 104, 176, 208, 320, 384};
  int n_run = 0;
  while (n_run < 3000) {
    rm_case c;
    c.BG = 1 + rng() % 2;
    c.Z = Zs[rng() % (sizeof(Zs) / sizeof(Zs[0]))];
    const uint32_t K = (c.BG == 1 ? 22 : 10) * c.Z;
    const uint32_t N = (c.BG == 1 ? 66 : 50) * c.Z;
    c.F = rng() % (K - 2 * c.Z);
    c.rv = rng() % 4;
    c.C = 1 + rng() % 8;
    // LBRM off, or Ncb anywhere between the end of the systematic part and N
    c.Tbslbrm = (rng() % 2) ? 0 : (2 * c.C * (K - 2 * c.Z + rng() % (N - K + 2 * c.Z + 1)) + 2) / 3;
    const uint32_t Ncb = nr_ldpc_soft_buffer_geometry(c.Tbslbrm, c.BG, c.Z, c.C, c.rv).Ncb;
    const uint32_t Foffset = filler_offset(c);
    if (Foffset + c.F > Ncb || c.F == Ncb)
      continue;
    // bias towards E < Foffset
    c.E = 1 + ((rng() % 2) ? rng() % Foffset : rng() % (3 * N));
    check_all(c, rng);
    if (::testing::Test::HasFatalFailure())
      return;
    n_run++;
  }
}

int main(int argc, char **argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
