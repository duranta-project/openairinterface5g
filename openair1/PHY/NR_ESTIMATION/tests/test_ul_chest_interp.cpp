/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>
#include <random>
#include <vector>
extern "C" {
#include "PHY/NR_UE_ESTIMATION/filt16a_32.h"
#include "PHY/NR_ESTIMATION/nr_ul_chest_interp.h"
}

// The per-pilot filter calls that nr_pusch_dmrs_type1_interp() replaces
static void interp_per_pilot(const c16_t *ls_est, const c16_t *delay, int nb_rb, c16_t *ul_ch)
{
  for (int p = 0; p < 6 * nb_rb; p++) {
    c16_t ch16 = c16mulShift(ls_est[2 * p], delay[2 * p], 8);
    if (p == 0) {
      c16multaddVectRealComplex(filt16_ul_p0, &ch16, ul_ch, 16);
    } else if (p == 1 || p == 2) {
      c16multaddVectRealComplex(filt16_ul_p1p2, &ch16, ul_ch, 16);
    } else if (p == 6 * nb_rb - 1) {
      c16multaddVectRealComplex(filt16_ul_last, &ch16, ul_ch, 16);
    } else {
      c16multaddVectRealComplex(filt16_ul_middle, &ch16, ul_ch, 16);
      if (p % 2 == 0)
        ul_ch += 4;
    }
  }
}

static void check(int nb_rb, int16_t amp, int16_t delay_amp, std::mt19937 &gen)
{
  const int n = 12 * nb_rb + 64;
  std::uniform_int_distribution<int> d(-amp - 1, amp), dd(-delay_amp - 1, delay_amp), ext(0, 15);
  std::vector<c16_t> ls(n), dl(n), ref(n), out(n);
  for (int k = 0; k < n; k++) {
    ls[k] = {(int16_t)d(gen), (int16_t)d(gen)};
    if (ext(gen) == 0) // full-scale values, where the saturating sums matter
      ls[k] = {(int16_t)(ext(gen) & 1 ? INT16_MAX : INT16_MIN), (int16_t)(ext(gen) & 1 ? INT16_MAX : INT16_MIN)};
    dl[k] = {(int16_t)dd(gen), (int16_t)dd(gen)};
  }
  interp_per_pilot(ls.data(), dl.data(), nb_rb, ref.data());
  nr_pusch_dmrs_type1_interp(ls.data(), dl.data(), nb_rb, out.data());
  for (int k = 0; k < n; k++) {
    ASSERT_EQ(ref[k].r, out[k].r) << "nb_rb " << nb_rb << " amp " << amp << " delay " << delay_amp << " re " << k;
    ASSERT_EQ(ref[k].i, out[k].i) << "nb_rb " << nb_rb << " amp " << amp << " delay " << delay_amp << " re " << k;
  }
}

TEST(ul_chest_interp, matches_per_pilot_filters)
{
  std::mt19937 gen(42);
  for (int nb_rb = 1; nb_rb <= 275; nb_rb++)
    for (int16_t amp : {300, 4000, INT16_MAX})
      for (int16_t delay_amp : {256, INT16_MAX})
        check(nb_rb, amp, delay_amp, gen);
}

int main(int argc, char **argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
