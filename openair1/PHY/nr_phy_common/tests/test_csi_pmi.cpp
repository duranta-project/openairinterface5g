/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>
#include <complex>
#include <random>
#include <vector>
#include "nr_csi_pmi.h"

using cd = std::complex<double>;

// Type I single-panel codebook, written directly from TS 38.214 5.2.2.2.1 (codebookMode 1)
struct Codebook {
  int n1, n2, o1, o2;
  int ports() const
  {
    return 2 * n1 * n2;
  }
  std::vector<cd> v(int l, int m) const
  {
    std::vector<cd> out(n1 * n2);
    for (int a1 = 0; a1 < n1; a1++)
      for (int a2 = 0; a2 < n2; a2++)
        out[a1 * n2 + a2] = std::polar(1.0, 2 * M_PI * l * a1 / (n1 * o1)) * std::polar(1.0, 2 * M_PI * m * a2 / (n2 * o2));
    return out;
  }
  // Table 5.2.2.2.1-3
  void k1k2(int i13, int &k1, int &k2) const
  {
    k1 = k2 = 0;
    if (n2 == 1) {
      k1 = i13 * o1;
    } else if (n1 == n2) {
      k1 = (i13 & 1) * o1;
      k2 = (i13 >> 1) * o2;
    } else {
      const int t1[4] = {0, 1, 0, 2}, t2[4] = {0, 0, 1, 0};
      k1 = t1[i13] * o1;
      k2 = t2[i13] * o2;
    }
  }
  // columns of W, P x rank
  std::vector<std::vector<cd>> W(int rank, const nr_csi_pmi_t &p) const
  {
    const cd phi = std::pow(cd(0, 1), p.i_2);
    const int half = n1 * n2;
    std::vector<std::vector<cd>> w(rank, std::vector<cd>(ports()));
    auto v1 = v(p.i_1_1, p.i_1_2);
    if (rank == 1) {
      for (int i = 0; i < half; i++) {
        w[0][i] = v1[i] / std::sqrt(ports());
        w[0][half + i] = phi * v1[i] / std::sqrt(ports());
      }
      return w;
    }
    int k1, k2;
    k1k2(p.i_1_3, k1, k2);
    auto v2 = v(p.i_1_1 + k1, p.i_1_2 + k2);
    const double s = std::sqrt(2.0 * ports());
    for (int i = 0; i < half; i++) {
      w[0][i] = v1[i] / s;
      w[0][half + i] = phi * v1[i] / s;
      w[1][i] = v2[i] / s;
      w[1][half + i] = -phi * v2[i] / s;
    }
    return w;
  }
  int num_i13() const
  {
    return (n1 == 2 && n2 == 1) ? 2 : 4;
  }
};

// R = sum over RX antennas of h^H h for channel rows h, scaled to int, num_re identical REs
static std::vector<c64_t> covariance(const std::vector<std::vector<cd>> &H, int num_re, double scale)
{
  const int P = H[0].size();
  std::vector<c64_t> R(P * P);
  for (int i = 0; i < P; i++)
    for (int j = 0; j < P; j++) {
      cd acc = 0;
      for (auto &h : H) {
        // quantize like the c16_t channel estimate
        cd hi(std::round(h[i].real() * scale), std::round(h[i].imag() * scale));
        cd hj(std::round(h[j].real() * scale), std::round(h[j].imag() * scale));
        acc += std::conj(hi) * hj;
      }
      R[i * P + j] = {(int64_t)std::llround(acc.real() * num_re), (int64_t)std::llround(acc.imag() * num_re)};
    }
  return R;
}

struct Config {
  int n1, n2;
};

class CsiPmi : public ::testing::TestWithParam<Config> {
 protected:
  Codebook cb() const
  {
    auto c = GetParam();
    return {c.n1, c.n2, 4, c.n2 > 1 ? 4 : 1};
  }
};

// A channel matched to a codeword (H = W^H) must select exactly that codeword.
TEST_P(CsiPmi, Rank1MatchedChannel)
{
  const Codebook c = cb();
  for (int l = 0; l < c.n1 * c.o1; l++)
    for (int m = 0; m < c.n2 * c.o2; m++)
      for (int i2 = 0; i2 < 4; i2++) {
        nr_csi_pmi_t want = {(uint8_t)l, (uint8_t)m, 0, (uint8_t)i2};
        auto w = c.W(1, want);
        std::vector<std::vector<cd>> H(1, std::vector<cd>(c.ports()));
        for (int p = 0; p < c.ports(); p++)
          H[0][p] = std::conj(w[0][p]);
        auto R = covariance(H, 10, 4000);
        nr_csi_pmi_t got;
        int sinr;
        ASSERT_TRUE(nr_csi_pmi_type1_single_panel(c.n1, c.n2, 1, R.data(), 10, 100, &got, &sinr));
        EXPECT_EQ(got.i_1_1, l);
        EXPECT_EQ(got.i_1_2, m);
        EXPECT_EQ(got.i_2, i2);
        EXPECT_EQ(got.i_1_3, 0);
      }
}

// With two RX antennas matched to the two columns of a rank-2 codeword, that codeword is selected.
TEST_P(CsiPmi, Rank2MatchedChannel)
{
  const Codebook c = cb();
  for (int l = 0; l < c.n1 * c.o1; l++)
    for (int m = 0; m < c.n2 * c.o2; m++)
      for (int i13 = 0; i13 < c.num_i13(); i13++)
        for (int i2 = 0; i2 < 2; i2++) {
          nr_csi_pmi_t want = {(uint8_t)l, (uint8_t)m, (uint8_t)i13, (uint8_t)i2};
          auto w = c.W(2, want);
          std::vector<std::vector<cd>> H(2, std::vector<cd>(c.ports()));
          for (int r = 0; r < 2; r++)
            for (int p = 0; p < c.ports(); p++)
              H[r][p] = std::conj(w[r][p]);
          auto R = covariance(H, 10, 4000);
          nr_csi_pmi_t got;
          int sinr;
          ASSERT_TRUE(nr_csi_pmi_type1_single_panel(c.n1, c.n2, 2, R.data(), 10, 100, &got, &sinr));
          // Another codeword can span the same space (same log det), so compare the precoded gain
          auto wg = c.W(2, got);
          auto gain = [&](const std::vector<std::vector<cd>> &W) {
            cd g[2][2] = {};
            for (int a = 0; a < 2; a++)
              for (int b = 0; b < 2; b++)
                for (auto &h : H) {
                  cd ha = 0, hb = 0;
                  for (int p = 0; p < c.ports(); p++) {
                    ha += h[p] * W[a][p];
                    hb += h[p] * W[b][p];
                  }
                  g[a][b] += std::conj(ha) * hb;
                }
            const double s = 1e3;
            return ((1.0 + s * g[0][0]) * (1.0 + s * g[1][1]) - s * s * g[0][1] * g[1][0]).real();
          };
          EXPECT_NEAR(gain(wg), gain(w), 1e-3 * gain(w)) << "l " << l << " m " << m << " i13 " << i13 << " i2 " << i2;
          EXPECT_GT(sinr, 0);
        }
}

// Random channels: the selected codeword is the best of an exhaustive search over the reference codebook.
TEST_P(CsiPmi, RandomChannelIsExhaustiveOptimum)
{
  const Codebook c = cb();
  std::mt19937 rng(1234);
  std::normal_distribution<double> g(0, 1);
  const double noise = 100;
  for (int trial = 0; trial < 50; trial++) {
    std::vector<std::vector<cd>> H(2, std::vector<cd>(c.ports()));
    for (auto &h : H)
      for (auto &x : h)
        x = cd(g(rng), g(rng)) * 0.3;
    auto R = covariance(H, 1, 4000);
    // reference metric on the same integer covariance
    const int P = c.ports();
    auto Rf = [&](int i, int j) {
      c64_t r = i <= j ? R[i * P + j] : R[j * P + i];
      cd v((double)r.r, (double)r.i);
      return i <= j ? v : std::conj(v);
    };
    auto quad = [&](const std::vector<cd> &a, const std::vector<cd> &b) {
      cd s = 0;
      for (int i = 0; i < P; i++)
        for (int j = 0; j < P; j++)
          s += std::conj(a[i]) * Rf(i, j) * b[j];
      return s;
    };
    for (int rank = 1; rank <= 2; rank++) {
      double best = -1;
      for (int l = 0; l < c.n1 * c.o1; l++)
        for (int m = 0; m < c.n2 * c.o2; m++)
          for (int i13 = 0; i13 < (rank == 1 ? 1 : c.num_i13()); i13++)
            for (int i2 = 0; i2 < (rank == 1 ? 4 : 2); i2++) {
              auto W = c.W(rank, {(uint8_t)l, (uint8_t)m, (uint8_t)i13, (uint8_t)i2});
              double metric;
              if (rank == 1) {
                metric = quad(W[0], W[0]).real();
              } else {
                cd g11 = quad(W[0], W[0]) / noise, g22 = quad(W[1], W[1]) / noise, g12 = quad(W[0], W[1]) / noise;
                metric = ((1.0 + g11) * (1.0 + g22) - g12 * std::conj(g12)).real();
              }
              best = std::max(best, metric);
            }
      nr_csi_pmi_t got;
      int sinr;
      ASSERT_TRUE(nr_csi_pmi_type1_single_panel(c.n1, c.n2, rank, R.data(), 1, noise, &got, &sinr));
      auto W = c.W(rank, got);
      double metric;
      if (rank == 1) {
        metric = quad(W[0], W[0]).real();
      } else {
        cd g11 = quad(W[0], W[0]) / noise, g22 = quad(W[1], W[1]) / noise, g12 = quad(W[0], W[1]) / noise;
        metric = ((1.0 + g11) * (1.0 + g22) - g12 * std::conj(g12)).real();
      }
      EXPECT_NEAR(metric, best, 1e-9 * best) << "trial " << trial << " rank " << rank;
      // index ranges match the X1/X2 bit widths of TS 38.212 Table 6.3.1.1.2-1
      EXPECT_LT(got.i_1_1, c.n1 * c.o1);
      EXPECT_LT(got.i_1_2, c.n2 * c.o2);
      EXPECT_LT(got.i_2, rank == 1 ? 4 : 2);
      EXPECT_LT(got.i_1_3, rank == 1 ? 1 : c.num_i13());
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    Type1SinglePanel,
    CsiPmi,
    ::testing::Values(Config{2, 1}, Config{2, 2}, Config{4, 1}, Config{3, 2}, Config{6, 1}, Config{4, 2}, Config{8, 1}),
    [](const ::testing::TestParamInfo<Config> &info) {
      return "N1_" + std::to_string(info.param.n1) + "_N2_" + std::to_string(info.param.n2);
    });

TEST(CsiPmiInvalid, Unsupported)
{
  c64_t R[4] = {};
  nr_csi_pmi_t p;
  int s;
  EXPECT_FALSE(nr_csi_pmi_type1_single_panel(1, 1, 1, R, 1, 1, &p, &s)); // 2 ports: separate codebook
  EXPECT_FALSE(nr_csi_pmi_type1_single_panel(2, 1, 3, R, 1, 1, &p, &s)); // rank 3
  EXPECT_FALSE(nr_csi_pmi_type1_single_panel(1, 2, 1, R, 1, 1, &p, &s)); // N2 > N1
}
