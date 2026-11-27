/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "gtest/gtest.h"

#include <cstdint>
#include <vector>

#include "nr_layer_demapping.h"

/* Fill a layer buffer with a deterministic pattern.
 * Value at RE=re, bit=m:  layer_id*1000 + re*10 + m */
static void fill_layer(std::vector<int16_t> &buf, int nb_re, int mod_order, int layer_id)
{
  for (int re = 0; re < nb_re; ++re)
    for (int m = 0; m < mod_order; ++m)
      buf[re * mod_order + m] = static_cast<int16_t>(layer_id * 1000 + re * 10 + m);
}

/* Check that the interleaved output matches the expected RE×layer×bit layout:
 *   out[ re*Nl*MO + l*MO + m ] == layer_data[l][ re*MO + m ] */
static void check_interleaved(const std::vector<std::vector<int16_t>> &layers,
                              const std::vector<int16_t> &out,
                              int nb_re,
                              uint8_t Nl,
                              uint8_t mod_order)
{
  for (int re = 0; re < nb_re; ++re)
    for (uint8_t l = 0; l < Nl; ++l)
      for (uint8_t m = 0; m < mod_order; ++m)
        EXPECT_EQ(out[re * Nl * mod_order + l * mod_order + m], layers[l][re * mod_order + m])
            << "RE=" << re << " layer=" << int(l) << " bit=" << int(m);
}

/* ------------------------------------------------------------------ */
/* Single-layer (plain memcpy, no interleaving)                       */
/* ------------------------------------------------------------------ */

TEST(nr_layer_demapping, single_layer_mod2)
{
  const uint8_t Nl = 1, mod_order = 2;
  const int nb_re = 4;
  std::vector<int16_t> src(nb_re * mod_order);
  fill_layer(src, nb_re, mod_order, 0);
  std::vector<int16_t *> p_layer{src.data()};
  std::vector<int16_t> out(nb_re * mod_order);

  nr_layer_demapping(Nl, mod_order, nb_re, p_layer.data(), out.data());

  for (size_t i = 0; i < out.size(); ++i)
    EXPECT_EQ(out[i], src[i]) << "index=" << i;
}

TEST(nr_layer_demapping, single_layer_mod8)
{
  const uint8_t Nl = 1, mod_order = 8;
  const int nb_re = 3;
  std::vector<int16_t> src(nb_re * mod_order);
  fill_layer(src, nb_re, mod_order, 0);
  std::vector<int16_t *> p_layer{src.data()};
  std::vector<int16_t> out(nb_re * mod_order);

  nr_layer_demapping(Nl, mod_order, nb_re, p_layer.data(), out.data());

  for (size_t i = 0; i < out.size(); ++i)
    EXPECT_EQ(out[i], src[i]) << "index=" << i;
}

/* ------------------------------------------------------------------ */
/* Multi-layer interleaving: all mod_order values, Nl 2–4             */
/* ------------------------------------------------------------------ */

TEST(nr_layer_demapping, two_layers_mod2)
{
  const uint8_t Nl = 2, mod_order = 2;
  const int nb_re = 5;
  std::vector<std::vector<int16_t>> data(Nl, std::vector<int16_t>(nb_re * mod_order));
  std::vector<int16_t *> p_layer(Nl);
  for (uint8_t l = 0; l < Nl; ++l) {
    fill_layer(data[l], nb_re, mod_order, l + 1);
    p_layer[l] = data[l].data();
  }
  std::vector<int16_t> out(nb_re * mod_order * Nl);

  nr_layer_demapping(Nl, mod_order, nb_re, p_layer.data(), out.data());
  check_interleaved(data, out, nb_re, Nl, mod_order);
}

TEST(nr_layer_demapping, two_layers_mod4)
{
  const uint8_t Nl = 2, mod_order = 4;
  const int nb_re = 3;
  std::vector<std::vector<int16_t>> data(Nl, std::vector<int16_t>(nb_re * mod_order));
  std::vector<int16_t *> p_layer(Nl);
  for (uint8_t l = 0; l < Nl; ++l) {
    fill_layer(data[l], nb_re, mod_order, l + 1);
    p_layer[l] = data[l].data();
  }
  std::vector<int16_t> out(nb_re * mod_order * Nl);

  nr_layer_demapping(Nl, mod_order, nb_re, p_layer.data(), out.data());
  check_interleaved(data, out, nb_re, Nl, mod_order);
}

TEST(nr_layer_demapping, three_layers_mod6)
{
  const uint8_t Nl = 3, mod_order = 6;
  const int nb_re = 4;
  std::vector<std::vector<int16_t>> data(Nl, std::vector<int16_t>(nb_re * mod_order));
  std::vector<int16_t *> p_layer(Nl);
  for (uint8_t l = 0; l < Nl; ++l) {
    fill_layer(data[l], nb_re, mod_order, l + 1);
    p_layer[l] = data[l].data();
  }
  std::vector<int16_t> out(nb_re * mod_order * Nl);

  nr_layer_demapping(Nl, mod_order, nb_re, p_layer.data(), out.data());
  check_interleaved(data, out, nb_re, Nl, mod_order);
}

TEST(nr_layer_demapping, four_layers_mod8)
{
  const uint8_t Nl = 4, mod_order = 8;
  const int nb_re = 2;
  std::vector<std::vector<int16_t>> data(Nl, std::vector<int16_t>(nb_re * mod_order));
  std::vector<int16_t *> p_layer(Nl);
  for (uint8_t l = 0; l < Nl; ++l) {
    fill_layer(data[l], nb_re, mod_order, l + 1);
    p_layer[l] = data[l].data();
  }
  std::vector<int16_t> out(nb_re * mod_order * Nl);

  nr_layer_demapping(Nl, mod_order, nb_re, p_layer.data(), out.data());
  check_interleaved(data, out, nb_re, Nl, mod_order);
}

TEST(nr_layer_demapping, two_layers_mod10)
{
  const uint8_t Nl = 2, mod_order = 10;
  const int nb_re = 3;
  std::vector<std::vector<int16_t>> data(Nl, std::vector<int16_t>(nb_re * mod_order));
  std::vector<int16_t *> p_layer(Nl);
  for (uint8_t l = 0; l < Nl; ++l) {
    fill_layer(data[l], nb_re, mod_order, l + 1);
    p_layer[l] = data[l].data();
  }
  std::vector<int16_t> out(nb_re * mod_order * Nl);

  nr_layer_demapping(Nl, mod_order, nb_re, p_layer.data(), out.data());
  check_interleaved(data, out, nb_re, Nl, mod_order);
}

TEST(nr_layer_demapping, four_layers_max_re_mod8)
{
  const uint8_t Nl = 4, mod_order = 8;
  const int nb_re = 273 * 12; // maximum number of REs in a 20 MHz FR1 carrier
  std::vector<std::vector<int16_t>> data(Nl, std::vector<int16_t>(nb_re * mod_order));
  std::vector<int16_t *> p_layer(Nl);
  for (uint8_t l = 0; l < Nl; ++l) {
    fill_layer(data[l], nb_re, mod_order, l + 1);
    p_layer[l] = data[l].data();
  }
  std::vector<int16_t> out(nb_re * mod_order * Nl);

  nr_layer_demapping(Nl, mod_order, nb_re, p_layer.data(), out.data());
  check_interleaved(data, out, nb_re, Nl, mod_order);
}

/* ------------------------------------------------------------------ */
/* Edge cases                                                          */
/* ------------------------------------------------------------------ */

/* nb_re=1: minimum input; verify the exact output byte-by-byte */
TEST(nr_layer_demapping, single_re_two_layers)
{
  const uint8_t Nl = 2, mod_order = 4;
  const int nb_re = 1;
  std::vector<int16_t> l0 = {10, 20, 30, 40};
  std::vector<int16_t> l1 = {50, 60, 70, 80};
  std::vector<int16_t *> p_layer = {l0.data(), l1.data()};
  std::vector<int16_t> out(nb_re * mod_order * Nl);

  nr_layer_demapping(Nl, mod_order, nb_re, p_layer.data(), out.data());

  /* Single RE → all L0 bits, then all L1 bits */
  const std::vector<int16_t> expected = {10, 20, 30, 40, 50, 60, 70, 80};
  for (size_t i = 0; i < expected.size(); ++i)
    EXPECT_EQ(out[i], expected[i]) << "index=" << i;
}

/* Negative LLRs: sign must be preserved through the copy */
TEST(nr_layer_demapping, negative_llr_values)
{
  const uint8_t Nl = 2, mod_order = 4;
  const int nb_re = 3;
  std::vector<int16_t> l0 = {-1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12};
  std::vector<int16_t> l1 = {1, -2, 3, -4, 5, -6, 7, -8, 9, -10, 11, -12};
  std::vector<int16_t *> p_layer = {l0.data(), l1.data()};
  std::vector<int16_t> out(nb_re * mod_order * Nl);

  nr_layer_demapping(Nl, mod_order, nb_re, p_layer.data(), out.data());

  for (int re = 0; re < nb_re; ++re)
    for (uint8_t m = 0; m < mod_order; ++m) {
      EXPECT_EQ(out[re * Nl * mod_order + m], l0[re * mod_order + m]) << "L0 RE=" << re << " bit=" << m;
      EXPECT_EQ(out[re * Nl * mod_order + mod_order + m], l1[re * mod_order + m]) << "L1 RE=" << re << " bit=" << m;
    }
}

/* INT16_MIN / INT16_MAX must pass through without truncation */
TEST(nr_layer_demapping, boundary_int16_values)
{
  const uint8_t Nl = 2, mod_order = 2;
  const int nb_re = 2;
  const int16_t LO = INT16_MIN, HI = INT16_MAX;
  std::vector<int16_t> l0 = {LO, HI, HI, LO};
  std::vector<int16_t> l1 = {HI, LO, LO, HI};
  std::vector<int16_t *> p_layer = {l0.data(), l1.data()};
  std::vector<int16_t> out(nb_re * mod_order * Nl);

  nr_layer_demapping(Nl, mod_order, nb_re, p_layer.data(), out.data());

  /* RE0: L0={LO,HI} L1={HI,LO}  RE1: L0={HI,LO} L1={LO,HI} */
  const std::vector<int16_t> expected = {LO, HI, HI, LO, HI, LO, LO, HI};
  for (size_t i = 0; i < expected.size(); ++i)
    EXPECT_EQ(out[i], expected[i]) << "index=" << i;
}

/* Invalid mod_order (not in {2,4,6,8}) must trigger AssertFatal and abort */
TEST(nr_layer_demapping, invalid_mod_order_aborts)
{
  const uint8_t Nl = 2, mod_order = 3; /* 3 is not a valid mod_order */
  const int nb_re = 4;
  std::vector<int16_t> l0(nb_re * mod_order), l1(nb_re * mod_order);
  std::vector<int16_t *> p_layer = {l0.data(), l1.data()};
  std::vector<int16_t> out(nb_re * mod_order * Nl);

  EXPECT_DEATH(nr_layer_demapping(Nl, mod_order, nb_re, p_layer.data(), out.data()), "");
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
