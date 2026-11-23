/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>
#include <cmath>
#include <complex>
#include <vector>
extern "C" {
#include "cdl_channel.h"
#include "common/utils/LOG/log.h"
}

using cplx = std::complex<double>;

static const double carrier = 3.5e9;
static const double wavelength = 299792458.0 / carrier;

static cdl_channel_t *make(cdl_model_t model, double ds = 100e-9, uint64_t seed = 1)
{
  cdl_channel_config_t cfg = {.model = model, .delay_spread_s = ds, .carrier_hz = carrier, .seed = seed};
  return cdl_channel_create(&cfg);
}

static double power(const channel_path_t &p)
{
  return p.gain.r * p.gain.r + p.gain.i * p.gain.i;
}

static cplx at(const std::vector<cd_t> &G, int i)
{
  return {G[i].r, G[i].i};
}

class AllModels : public testing::TestWithParam<cdl_model_t> {};

TEST_P(AllModels, total_power_is_one)
{
  cdl_channel_t *ch = make(GetParam());
  const channel_path_t *p;
  const int n = cdl_channel_paths(ch, &p);
  double total = 0;
  for (int i = 0; i < n; i++)
    total += power(p[i]);
  EXPECT_NEAR(total, 1.0, 1e-12);
  cdl_channel_free(ch);
}

TEST_P(AllModels, rms_delay_spread_is_the_configured_one)
{
  const double ds = 300e-9;
  cdl_channel_t *ch = make(GetParam(), ds);
  const channel_path_t *p;
  const int n = cdl_channel_paths(ch, &p);
  double mean = 0;
  for (int i = 0; i < n; i++)
    mean += power(p[i]) * p[i].delay_s;
  double var = 0;
  for (int i = 0; i < n; i++)
    var += power(p[i]) * std::pow(p[i].delay_s - mean, 2);
  EXPECT_NEAR(std::sqrt(var) / ds, 1.0, 0.01);
  cdl_channel_free(ch);
}

INSTANTIATE_TEST_SUITE_P(cdl, AllModels, testing::Values(CDL_A, CDL_B, CDL_C, CDL_D, CDL_E));

TEST(cdl, same_seed_same_rays_other_seed_other_phases)
{
  cdl_channel_t *a = make(CDL_C, 100e-9, 7), *b = make(CDL_C, 100e-9, 7), *c = make(CDL_C, 100e-9, 8);
  const channel_path_t *pa, *pb, *pc;
  const int n = cdl_channel_paths(a, &pa);
  cdl_channel_paths(b, &pb);
  cdl_channel_paths(c, &pc);
  int differ = 0;
  for (int i = 0; i < n; i++) {
    EXPECT_EQ(pa[i].gain.r, pb[i].gain.r);
    EXPECT_EQ(pa[i].aoa, pb[i].aoa);
    differ += pa[i].gain.r != pc[i].gain.r;
  }
  EXPECT_GT(differ, n / 2);
  cdl_channel_free(a);
  cdl_channel_free(b);
  cdl_channel_free(c);
}

TEST(cdl, departure_angles_turn_with_the_ue)
{
  // Moving the UE in azimuth turns every departure ray by the same angle, zeniths unchanged
  cdl_channel_t *ch = make(CDL_A);
  const double v[3] = {0, 0, 0};
  const double zod = 100 * M_PI / 180, aod1 = -20 * M_PI / 180, aod2 = 40 * M_PI / 180;
  const channel_path_t *p;
  const int n = cdl_channel_paths(ch, &p);
  cdl_channel_set_geometry(ch, aod1, zod, v);
  std::vector<channel_path_t> first(p, p + n);
  cdl_channel_set_geometry(ch, aod2, zod, v);
  for (int i = 0; i < n; i++) {
    EXPECT_NEAR(std::remainder(p[i].aod - first[i].aod - (aod2 - aod1), 2 * M_PI), 0, 1e-12) << "ray " << i;
    EXPECT_EQ(p[i].zod, first[i].zod) << "ray " << i;
  }
  cdl_channel_free(ch);
}

TEST(cdl, rays_rotate_with_their_doppler_shift)
{
  cdl_channel_t *ch = make(CDL_D);
  const double v[3] = {30, 0, 0};
  cdl_channel_set_geometry(ch, 0, M_PI / 2, v);
  const channel_path_t *p;
  cdl_channel_paths(ch, &p);
  // CDL-D specular ray: AoA -180, ZoA 81.5 degrees (TR 38.901 Table 7.7.1-4)
  const double expected = std::sin(81.5 * M_PI / 180) * std::cos(M_PI) * 30 / wavelength;
  EXPECT_NEAR(p[0].doppler_hz, expected, 1e-6);
  const cplx before(p[0].gain.r, p[0].gain.i);
  const double dt = 1e-3;
  cdl_channel_advance(ch, dt);
  const cplx after(p[0].gain.r, p[0].gain.i);
  EXPECT_NEAR(std::arg(after / before), std::remainder(2 * M_PI * expected * dt, 2 * M_PI), 1e-9);
  EXPECT_NEAR(std::abs(after), std::abs(before), 1e-12);
  cdl_channel_free(ch);
}

TEST(beam_channel, single_path_through_a_steered_ula)
{
  double pos[8][3];
  antenna_array_upa_positions(1, 8, 0.5, 0.5, pos);
  const antenna_array_t tx = {8, pos, ANTENNA_PATTERN_ISOTROPIC};
  const double origin[1][3] = {{0, 0, 0}};
  const antenna_array_t rx = {1, origin, ANTENNA_PATTERN_ISOTROPIC};
  const double phi = 20 * M_PI / 180, tau = 250e-9;
  const channel_path_t path = {.gain = {0.6, -0.8}, .delay_s = tau, .aod = phi, .zod = M_PI / 2, .aoa = 0, .zoa = M_PI / 2};
  std::vector<cd_t> w(8);
  for (int e = 0; e < 8; e++) {
    const cplx v = std::polar(1 / std::sqrt(8.0), -M_PI * e * std::sin(phi));
    w[e] = {v.real(), v.imag()};
  }
  const double f0 = -1e6, df = 360e3;
  const int nf = 16;
  std::vector<cd_t> G(nf);
  beam_channel_response(&path, 1, &tx, w.data(), 1, &rx, f0, df, nf, G.data());
  // Matched beam: array gain sqrt(8) on top of the path gain, then the delay's phase ramp
  for (int k = 0; k < nf; k++) {
    const cplx expected = cplx(0.6, -0.8) * std::sqrt(8.0) * std::polar(1.0, -2 * M_PI * (f0 + k * df) * tau);
    EXPECT_NEAR(std::abs(at(G, k) - expected), 0, 1e-12) << "k " << k;
  }
}

TEST(beam_channel, response_is_linear_in_the_beam_weights)
{
  // A beam's response equals the sum of the element responses it weights, for every path of a CDL
  // channel through a 4x8 panel and 2 UE antennas
  cdl_channel_t *ch = make(CDL_B);
  const double v[3] = {3, 4, 0};
  cdl_channel_set_geometry(ch, 0.3, 1.4, v);
  const channel_path_t *paths;
  const int np = cdl_channel_paths(ch, &paths);
  double pos[32][3];
  antenna_array_upa_positions(4, 8, 0.5, 0.5, pos);
  const antenna_array_t tx = {32, pos, ANTENNA_PATTERN_TR38901};
  const double ue_pos[2][3] = {{0, 0, 0}, {0, 0.5, 0}};
  const antenna_array_t rx = {2, ue_pos, ANTENNA_PATTERN_ISOTROPIC};
  const int nb = 2, nf = 24;
  const double f0 = -50e6, df = 12 * 30e3;

  std::vector<cd_t> w(nb * 32);
  for (size_t i = 0; i < w.size(); i++)
    w[i] = {std::cos(0.7 * i), std::sin(1.3 * i)};
  std::vector<cd_t> G(2 * nb * nf);
  beam_channel_response(paths, np, &tx, w.data(), nb, &rx, f0, df, nf, G.data());

  std::vector<cd_t> unit(32 * 32, cd_t{0, 0});
  for (int e = 0; e < 32; e++)
    unit[e * 32 + e] = {1, 0};
  std::vector<cd_t> H(2 * 32 * nf);
  beam_channel_response(paths, np, &tx, unit.data(), 32, &rx, f0, df, nf, H.data());

  double max_err = 0, max_mag = 0;
  for (int r = 0; r < 2; r++)
    for (int b = 0; b < nb; b++)
      for (int k = 0; k < nf; k++) {
        cplx sum = 0;
        for (int e = 0; e < 32; e++)
          sum += at(H, (r * 32 + e) * nf + k) * cplx(w[b * 32 + e].r, w[b * 32 + e].i);
        const cplx g = at(G, (r * nb + b) * nf + k);
        max_err = std::max(max_err, std::abs(g - sum));
        max_mag = std::max(max_mag, std::abs(sum));
      }
  EXPECT_GT(max_mag, 0.1);
  EXPECT_LT(max_err, 1e-9 * max_mag);
  cdl_channel_free(ch);
}

TEST(beam_channel, strongest_beam_points_at_the_ue)
{
  // LOS channel steered at 30 degrees azimuth: of a DFT grid of beams, the one at 30 degrees wins
  cdl_channel_t *ch = make(CDL_D, 30e-9);
  const double v[3] = {0, 0, 0};
  cdl_channel_set_geometry(ch, 30 * M_PI / 180, M_PI / 2, v);
  const channel_path_t *paths;
  const int np = cdl_channel_paths(ch, &paths);
  double pos[8][3];
  antenna_array_upa_positions(1, 8, 0.5, 0.5, pos);
  const antenna_array_t tx = {8, pos, ANTENNA_PATTERN_TR38901};
  const double origin[1][3] = {{0, 0, 0}};
  const antenna_array_t rx = {1, origin, ANTENNA_PATTERN_ISOTROPIC};
  const int nb = 9, nf = 64;
  std::vector<cd_t> w(nb * 8);
  for (int b = 0; b < nb; b++) {
    const double steer = (-60 + 15 * b) * M_PI / 180;
    for (int e = 0; e < 8; e++) {
      const cplx c = std::polar(1 / std::sqrt(8.0), -M_PI * e * std::sin(steer));
      w[b * 8 + e] = {c.real(), c.imag()};
    }
  }
  std::vector<cd_t> G(nb * nf);
  beam_channel_response(paths, np, &tx, w.data(), nb, &rx, -10e6, 360e3, nf, G.data());
  int best = -1;
  double best_power = 0;
  for (int b = 0; b < nb; b++) {
    double p = 0;
    for (int k = 0; k < nf; k++)
      p += std::norm(at(G, b * nf + k));
    if (p > best_power) {
      best_power = p;
      best = b;
    }
  }
  EXPECT_EQ(best, 6); // -60 + 15 * 6 = 30 degrees
  cdl_channel_free(ch);
}

int main(int argc, char **argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
