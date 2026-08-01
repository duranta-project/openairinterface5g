/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Unit tests for the 2-layer L-best MIMO soft-output (LLR) kernels.
 *
 * (A) Width equivalence -- the width-parameterized fixed-point SIMD kernels must
 *     be BIT-IDENTICAL across widths: w128 (SSE->NEON) == w256 (AVX2) == w512
 *     (AVX-512, when compiled in). This is the primary regression guard for the
 *     width parameterization and the AVX-512 port: one source, all widths equal.
 *
 * (B) Fidelity vs the float reference model (nr_qam{64,256}_llr_2layer_lbest):
 *     the fixed-point kernel must track the float L-best within a bounded best-fit
 *     residual and sign agreement -- the same methodology as OAI_LBEST_DBG. This
 *     catches structural/scale/sign regressions against the analytical model.
 */
#include "gtest/gtest.h"
#include <stdint.h>
#include <vector>
#include <cstring>
#include <cstdlib>

extern "C" {
struct configmodule_interface_s;
struct configmodule_interface_s *uniqCfg = NULL;
void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  if (assert)
    abort();
  else
    exit(EXIT_SUCCESS);
}
#include "openair1/PHY/TOOLS/tools_defs.h"

// Float reference L-best (float internally, int16 out). L = candidate count
// (L>=full reproduces the exact max-log ML search); seed_lambda unused here.
void nr_qam64_llr_2layer_lbest(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, c16_t *, uint32_t, int, float);
void nr_qam256_llr_2layer_lbest(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, c16_t *, uint32_t, int, float);

// Width-specific fixed-point SIMD kernels (last arg = candidate pattern).
void nr_qam64_llr_2layer_lbest_q15_simd_w128(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, c16_t *, uint32_t, int);
void nr_qam64_llr_2layer_lbest_q15_simd_w256(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, c16_t *, uint32_t, int);
void nr_qam256_llr_2layer_lbest_q15_simd_w128(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, c16_t *, uint32_t, int);
void nr_qam256_llr_2layer_lbest_q15_simd_w256(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, c16_t *, uint32_t, int);
#if defined(__AVX512BW__) && defined(__AVX512VL__) && defined(__AVX512F__)
void nr_qam64_llr_2layer_lbest_q15_simd_w512(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, c16_t *, uint32_t, int);
void nr_qam256_llr_2layer_lbest_q15_simd_w512(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, c16_t *, uint32_t, int);
#define NRLB_TEST_HAVE_W512 1
#endif
}

#include "openair1/PHY/TOOLS/phy_test_tools.hpp"
#include <random>

namespace {

// A plausible 2-layer scenario: matched-filter outputs z0/z1, positive channel
// magnitudes (Pp0/Pp1) and a cross-correlation rho, all in the Q15-ish range the
// kernels operate on. Buffers are padded so the SIMD block loop never over-reads.
struct Inputs2L {
  AlignedVector512<c16_t> s0, s1, mag, magi, rho;
};

Inputs2L gen_inputs(uint32_t nb_re, unsigned seed)
{
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> sig(-16000, 16000), pos(3000, 16000), rh(-9000, 9000);
  const uint32_t N = nb_re + 64; // pad against the widest (w512, 32-RE) block load
  Inputs2L in;
  in.s0.resize(N); in.s1.resize(N); in.mag.resize(N); in.magi.resize(N); in.rho.resize(N);
  for (uint32_t i = 0; i < N; i++) {
    in.s0[i] = {(int16_t)sig(rng), (int16_t)sig(rng)};
    in.s1[i] = {(int16_t)sig(rng), (int16_t)sig(rng)};
    const int16_t p0 = (int16_t)pos(rng), p1 = (int16_t)pos(rng);
    in.mag[i] = {p0, p0};   // ch_mag: real part is the used magnitude (Pp0), replicated
    in.magi[i] = {p1, p1};  // ch_mag_i: interferer magnitude (Pp1)
    in.rho[i] = {(int16_t)rh(rng), (int16_t)rh(rng)};
  }
  return in;
}

// A well-conditioned scenario: each layer carries a clean constellation point
// (z ~= step*level, the matched-filter output at high SNR), low cross-correlation,
// and light noise. Decisions are unambiguous, so the fixed-point kernel and the
// float model agree closely -- the regime where a fidelity bound is meaningful
// (unlike uniform-random streams, which sit on decision boundaries by construction).
// step derives from the seed's level<->z mapping: est = z*sqrt(K)/Pp, Pp = mag*c,
// so z = level*Pp/sqrt(K); with mag=8192 that is ~2048/level for 64QAM (K=42) and
// ~1000/level for 256QAM (K=170).
Inputs2L gen_clean_inputs(uint32_t nb_re, unsigned seed, int maxlvl, int step)
{
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> lvlIdx(0, maxlvl / 2);   // 0..(N/2-1) -> odd levels 1,3,..,maxlvl
  std::uniform_int_distribution<int> noise(-180, 180), sgn(0, 1);
  const int16_t P = 8192;
  auto level = [&]() { return (2 * lvlIdx(rng) + 1) * (sgn(rng) ? 1 : -1); };
  const uint32_t N = nb_re + 64;
  Inputs2L in;
  in.s0.resize(N); in.s1.resize(N); in.mag.resize(N); in.magi.resize(N); in.rho.resize(N);
  for (uint32_t i = 0; i < N; i++) {
    in.s0[i] = {(int16_t)(step * level() + noise(rng)), (int16_t)(step * level() + noise(rng))};
    in.s1[i] = {(int16_t)(step * level() + noise(rng)), (int16_t)(step * level() + noise(rng))};
    in.mag[i] = {P, P};
    in.magi[i] = {P, P};
    in.rho[i] = {(int16_t)noise(rng), (int16_t)noise(rng)};  // low correlation
  }
  return in;
}

struct Fit {
  double signDisagree_pct, fitScale, residFrac;
};

// Scale-invariant agreement of a fixed-point kernel output vs the float reference,
// identical to the OAI_LBEST_DBG statistic: best-fit scale a = <p,r>/<r,r>, then
// residual fraction ||p - a r||^2 / ||p||^2, plus the fraction of sign flips.
Fit fit_stats(const int16_t *dut, const int16_t *ref, size_t n)
{
  long sdis = 0;
  double sPR = 0, sRR = 0, sPP = 0;
  for (size_t k = 0; k < n; k++) {
    const double p = dut[k], r = ref[k];
    if ((long)dut[k] * (long)ref[k] < 0)
      sdis++;
    sPR += p * r;
    sRR += r * r;
    sPP += p * p;
  }
  const double a = sRR > 0 ? sPR / sRR : 0;
  const double resid = sPP - 2 * a * sPR + a * a * sRR;
  return {100.0 * (double)sdis / (double)n, a, sPP > 0 ? resid / sPP : 0};
}

// ---- (A) width equivalence ------------------------------------------------

void width_equiv_64qam(uint32_t nb_re, int pattern)
{
  auto in = gen_inputs(nb_re, 0x64ull + nb_re * 131 + pattern);
  const size_t nll = (size_t)nb_re * 6;
  AlignedVector512<int16_t> o128(nll + 64, 0), o256(nll + 64, 0);
  nr_qam64_llr_2layer_lbest_q15_simd_w128(in.s0.data(), in.s1.data(), in.mag.data(), in.magi.data(), o128.data(), in.rho.data(), nb_re, pattern);
  nr_qam64_llr_2layer_lbest_q15_simd_w256(in.s0.data(), in.s1.data(), in.mag.data(), in.magi.data(), o256.data(), in.rho.data(), nb_re, pattern);
  EXPECT_EQ(0, memcmp(o128.data(), o256.data(), nll * sizeof(int16_t))) << "w128 != w256 (64QAM, nb_re=" << nb_re << ", pat=" << pattern << ")";
#ifdef NRLB_TEST_HAVE_W512
  AlignedVector512<int16_t> o512(nll + 64, 0);
  nr_qam64_llr_2layer_lbest_q15_simd_w512(in.s0.data(), in.s1.data(), in.mag.data(), in.magi.data(), o512.data(), in.rho.data(), nb_re, pattern);
  EXPECT_EQ(0, memcmp(o128.data(), o512.data(), nll * sizeof(int16_t))) << "w128 != w512 (64QAM, nb_re=" << nb_re << ", pat=" << pattern << ")";
#endif
}

void width_equiv_256qam(uint32_t nb_re, int pattern)
{
  auto in = gen_inputs(nb_re, 0x256ull + nb_re * 131 + pattern);
  const size_t nll = (size_t)nb_re * 8;
  AlignedVector512<int16_t> o128(nll + 64, 0), o256(nll + 64, 0);
  nr_qam256_llr_2layer_lbest_q15_simd_w128(in.s0.data(), in.s1.data(), in.mag.data(), in.magi.data(), o128.data(), in.rho.data(), nb_re, pattern);
  nr_qam256_llr_2layer_lbest_q15_simd_w256(in.s0.data(), in.s1.data(), in.mag.data(), in.magi.data(), o256.data(), in.rho.data(), nb_re, pattern);
  EXPECT_EQ(0, memcmp(o128.data(), o256.data(), nll * sizeof(int16_t))) << "w128 != w256 (256QAM, nb_re=" << nb_re << ", pat=" << pattern << ")";
#ifdef NRLB_TEST_HAVE_W512
  AlignedVector512<int16_t> o512(nll + 64, 0);
  nr_qam256_llr_2layer_lbest_q15_simd_w512(in.s0.data(), in.s1.data(), in.mag.data(), in.magi.data(), o512.data(), in.rho.data(), nb_re, pattern);
  EXPECT_EQ(0, memcmp(o128.data(), o512.data(), nll * sizeof(int16_t))) << "w128 != w512 (256QAM, nb_re=" << nb_re << ", pat=" << pattern << ")";
#endif
}

// RE counts that are multiples of 32 -> every width runs pure-SIMD (no scalar
// remainder), so the outputs must be bit-identical.
const std::vector<uint32_t> kMul32 = {32, 64, 96, 320, 3200};

}  // namespace

TEST(test_llr_2layer, width_equiv_64qam_pattern0)
{
  for (auto n : kMul32) width_equiv_64qam(n, 0);
}
TEST(test_llr_2layer, width_equiv_256qam_pattern1)
{
  for (auto n : kMul32) width_equiv_256qam(n, 1);
}
// non-default patterns must be width-exact too
TEST(test_llr_2layer, width_equiv_64qam_other_patterns)
{
  for (int p = 1; p <= 2; p++) for (auto n : kMul32) width_equiv_64qam(n, p);
}
TEST(test_llr_2layer, width_equiv_256qam_other_patterns)
{
  for (int p : {0, 2, 3}) for (auto n : kMul32) width_equiv_256qam(n, p);
}

// ---- (B) fidelity vs the float reference model ----------------------------

TEST(test_llr_2layer, fidelity_vs_float_64qam)
{
  const uint32_t nb_re = 3200;
  auto in = gen_clean_inputs(nb_re, 0xF64, 7, 2048);
  AlignedVector512<int16_t> dut((size_t)nb_re * 6 + 64, 0), ref((size_t)nb_re * 6 + 64, 0);
  nr_qam64_llr_2layer_lbest_q15_simd_w256(in.s0.data(), in.s1.data(), in.mag.data(), in.magi.data(), dut.data(), in.rho.data(), nb_re, 0);
  nr_qam64_llr_2layer_lbest(in.s0.data(), in.s1.data(), in.mag.data(), in.magi.data(), ref.data(), in.rho.data(), nb_re, 64, 0.0f);
  Fit f = fit_stats(dut.data(), ref.data(), (size_t)nb_re * 6);
  fprintf(stderr, "[  FIT  ] 64QAM  signDisagree=%.3f%% fitScale=%.3f residFrac=%.3f\n", f.signDisagree_pct, f.fitScale, f.residFrac);
  EXPECT_LT(f.signDisagree_pct, 1.0) << "too many LLR sign flips vs float model";
  EXPECT_GT(f.fitScale, 0.2) << "kernel output not positively correlated with the model";
  EXPECT_LT(f.residFrac, 0.40) << "post-scale residual too large vs float model";
}

TEST(test_llr_2layer, fidelity_vs_float_256qam)
{
  const uint32_t nb_re = 3200;
  auto in = gen_clean_inputs(nb_re, 0xF256, 15, 1000);
  AlignedVector512<int16_t> dut((size_t)nb_re * 8 + 64, 0), ref((size_t)nb_re * 8 + 64, 0);
  nr_qam256_llr_2layer_lbest_q15_simd_w256(in.s0.data(), in.s1.data(), in.mag.data(), in.magi.data(), dut.data(), in.rho.data(), nb_re, 1);
  nr_qam256_llr_2layer_lbest(in.s0.data(), in.s1.data(), in.mag.data(), in.magi.data(), ref.data(), in.rho.data(), nb_re, 256, 0.0f);
  Fit f = fit_stats(dut.data(), ref.data(), (size_t)nb_re * 8);
  fprintf(stderr, "[  FIT  ] 256QAM signDisagree=%.3f%% fitScale=%.3f residFrac=%.3f\n", f.signDisagree_pct, f.fitScale, f.residFrac);
  EXPECT_LT(f.signDisagree_pct, 1.0) << "too many LLR sign flips vs float model";
  EXPECT_GT(f.fitScale, 0.2) << "kernel output not positively correlated with the model";
  EXPECT_LT(f.residFrac, 0.40) << "post-scale residual too large vs float model";
}

int main(int argc, char **argv)
{
  logInit();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
