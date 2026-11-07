/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// GPU 2-layer detectors (nr_detector_gpu.h) against the CPU kernels: bit-identical LLRs with the same source built
// without FMA contraction, for every modulation, pattern, noise term and channel conditioning (adaptive window
// included); against the production (contracted) build, rounding-level differences only.

#include "gtest/gtest.h"
#include <stdint.h>
#include <vector>
#include <random>
#include <cmath>
#include <cstring>

#include "PHY/gpu_compat.h" // C++ runtime headers: outside extern "C"
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
#include "openair1/PHY/nr_phy_common/inc/nr_compute_llr.h"
#include "openair1/PHY/nr_phy_common/inc/nr_detector_gpu.h"
// production CPU kernels (FMA-contracted where the compiler chooses) and the same source without contraction
void nr_qam64_llr_2layer_lbest_f32_simd_w128(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, c16_t *, uint32_t, int, float);
void nr_qam256_llr_2layer_lbest_f32_simd_w128(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, c16_t *, uint32_t, int, float);
void nr_llr_2layer_mmse_simd_w128(const c16_t *, const c16_t *, const c16_t *, const c16_t *, const c16_t *, int16_t *, uint32_t, int, float);
void nr_qam64_llr_2layer_lbest_f32_simd_w128_exact(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, c16_t *, uint32_t, int, float);
void nr_qam256_llr_2layer_lbest_f32_simd_w128_exact(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, c16_t *, uint32_t, int, float);
void nr_qpsk_llr_2layer(c16_t *, c16_t *, int16_t *, c16_t *, uint32_t);
void nr_qam16_llr_2layer(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, c16_t *, uint32_t);
void nr_llr_2layer_mmse_simd_w128_exact(const c16_t *, const c16_t *, const c16_t *, const c16_t *, const c16_t *, int16_t *, uint32_t, int, float);
}

namespace {

// zeroed, 64-byte aligned buffer: the int16 CPU kernels load aligned vectors (phy_test_tools.hpp is already linked
// in from test_inner_rx_gpu.cpp and defines non-inline functions)
template <typename T>
struct ABuf {
  std::vector<T> v;
  T *p;
  explicit ABuf(size_t n) : v(n + 64 / sizeof(T)), p((T *)(((uintptr_t)v.data() + 63) & ~(uintptr_t)63)) {}
  T *data() { return p; }
  T &operator[](size_t i) { return p[i]; }
};

struct In {
  std::vector<c16_t> s0, s1, mag, magi, rho;
};

// ill: fraction of nearly singular 2x2 channels (|rho|^2 ~ 0.99 P0 P1, the adaptive window's full-grid REs);
// gram: physical Gram matrix in ||h||^2 units for the MMSE (else ch_mag units, psc = ch_mag -> ||h||^2)
In gen(uint32_t n, unsigned seed, double ill, float psc)
{
  std::mt19937 rng(seed);
  std::uniform_real_distribution<double> pw(2000, 16000), ph(0, 2 * M_PI), u(0, 1), z(-12000, 12000);
  In in;
  for (auto *v : {&in.s0, &in.s1, &in.mag, &in.magi, &in.rho})
    v->resize(n);
  for (uint32_t i = 0; i < n; i++) {
    const double p0 = pw(rng), p1 = pw(rng), a = (u(rng) < ill ? 0.995 : 0.5 * u(rng)) * sqrt(p0 * p1) * psc, t = ph(rng);
    in.mag[i] = {(int16_t)p0, (int16_t)p0};
    in.magi[i] = {(int16_t)p1, (int16_t)p1};
    in.rho[i] = {(int16_t)(a * cos(t)), (int16_t)(a * sin(t))};
    in.s0[i] = {(int16_t)z(rng), (int16_t)z(rng)};
    in.s1[i] = {(int16_t)z(rng), (int16_t)z(rng)};
  }
  return in;
}

bool have_gpu()
{
  void *p = nullptr;
  if (gpuMalloc(&p, 16) != gpuSuccess)
    return false;
  gpuFree(p);
  return true;
}

template <typename T>
T *dev(const std::vector<T> &h)
{
  T *d = nullptr;
  EXPECT_EQ(gpuSuccess, gpuMalloc((void **)&d, h.size() * sizeof(T)));
  EXPECT_EQ(gpuSuccess, gpuMemcpy(d, h.data(), h.size() * sizeof(T), gpuMemcpyHostToDevice));
  return d;
}

struct DevIn {
  c16_t *s0, *s1, *mag, *magi, *rho;
  int16_t *out;
  explicit DevIn(const In &in, size_t nllr)
      : s0(dev(in.s0)), s1(dev(in.s1)), mag(dev(in.mag)), magi(dev(in.magi)), rho(dev(in.rho)), out(dev(std::vector<int16_t>(nllr, 0x5a5a)))
  {
  }
  ~DevIn()
  {
    for (void *p : {(void *)s0, (void *)s1, (void *)mag, (void *)magi, (void *)rho, (void *)out})
      gpuFree(p);
  }
};

// mismatching REs between two LLR buffers of n REs x qm
uint32_t diff_re(const std::vector<int16_t> &a, const std::vector<int16_t> &b, uint32_t n, int qm)
{
  uint32_t bad = 0;
  for (uint32_t i = 0; i < n; i++)
    bad += memcmp(&a[(size_t)i * qm], &b[(size_t)i * qm], qm * sizeof(int16_t)) != 0;
  return bad;
}

// against the FMA-contracted production build: LLRs that differ, and those of opposite sign (both non-zero)
struct Close {
  size_t ndiff = 0, nsign = 0, n = 0;
  void add(const std::vector<int16_t> &a, const std::vector<int16_t> &b)
  {
    for (size_t k = 0; k < a.size(); k++) {
      ndiff += a[k] != b[k];
      nsign += (a[k] > 0 && b[k] < 0) || (a[k] < 0 && b[k] > 0);
    }
    n += a.size();
  }
};

TEST(test_detector_gpu, lbest_and_ml_bit_exact)
{
  if (!have_gpu())
    GTEST_SKIP() << "no GPU";
  nr_gpu_det_ws_t *ws = nr_gpu_det_ws_alloc(1u << 16);
  ASSERT_NE(nullptr, ws);
  Close close[2];
  for (int qm : {6, 8})
    for (int pattern : {1, 3})
      for (double ill : {0.0, 0.3})
        for (float lambda : {0.0f, 37.0f, 900.0f})
          for (uint32_t n : {33u, 1272u, 39312u}) {
            const float psc = qm == 6 ? 6.48074069840786f / 4 : 13.038404810405298f / 8;
            In in = gen(n, 0x6b0u + qm * 131 + pattern * 17 + n + (unsigned)lambda + (unsigned)(ill * 10), ill, psc);
            const size_t nllr = (size_t)n * qm;
            std::vector<int16_t> cpu(nllr), prod(nllr), gpu(nllr);
            auto *k = qm == 6 ? nr_qam64_llr_2layer_lbest_f32_simd_w128_exact : nr_qam256_llr_2layer_lbest_f32_simd_w128_exact;
            auto *kp = qm == 6 ? nr_qam64_llr_2layer_lbest_f32_simd_w128 : nr_qam256_llr_2layer_lbest_f32_simd_w128;
            k(in.s0.data(), in.s1.data(), in.mag.data(), in.magi.data(), cpu.data(), in.rho.data(), n, pattern, lambda);
            kp(in.s0.data(), in.s1.data(), in.mag.data(), in.magi.data(), prod.data(), in.rho.data(), n, pattern, lambda);
            DevIn d(in, nllr);
            ASSERT_EQ(0, nr_gpu_llr_2layer_lbest(d.s0, d.s1, d.mag, d.magi, d.out, d.rho, n, qm, pattern, lambda, ws, nullptr));
            ASSERT_EQ(gpuSuccess, gpuMemcpy(gpu.data(), d.out, nllr * sizeof(int16_t), gpuMemcpyDeviceToHost));
            EXPECT_EQ(0u, diff_re(cpu, gpu, n, qm))
                << "Qm=" << qm << " pattern=" << pattern << " ill=" << ill << " lambda=" << lambda << " n=" << n;
            close[qm == 8].add(prod, gpu);
          }
  nr_gpu_det_ws_free(ws);
  for (int q = 0; q < 2; q++) {
    printf("[  INFO  ] %s vs FMA-contracted CPU: %.4f%% LLRs differ, %.5f%% opposite sign\n",
           q ? "256QAM" : "64QAM", 100.0 * close[q].ndiff / close[q].n, 100.0 * close[q].nsign / close[q].n);
    EXPECT_LT(close[q].nsign, close[q].n / 10000) << "rounding-level only";
  }
}

TEST(test_detector_gpu, mmse_bit_exact)
{
  if (!have_gpu())
    GTEST_SKIP() << "no GPU";
  Close close;
  for (int qm : {2, 4, 6, 8})
    for (double ill : {0.0, 0.3})
      for (float lambda : {0.0f, 300.0f})
        for (uint32_t n : {33u, 1272u, 39312u}) {
          In in = gen(n, 0x33eu + qm * 7 + n + (unsigned)lambda + (unsigned)(ill * 10), ill, 1.0f);
          const size_t nllr = (size_t)n * qm;
          std::vector<int16_t> cpu(nllr), prod(nllr), gpu(nllr);
          nr_llr_2layer_mmse_simd_w128_exact(in.s0.data(), in.s1.data(), in.mag.data(), in.magi.data(), in.rho.data(), cpu.data(), n, qm, lambda);
          nr_llr_2layer_mmse_simd_w128(in.s0.data(), in.s1.data(), in.mag.data(), in.magi.data(), in.rho.data(), prod.data(), n, qm, lambda);
          DevIn d(in, nllr);
          ASSERT_EQ(0, nr_gpu_llr_2layer_mmse(d.s0, d.s1, d.mag, d.magi, d.rho, d.out, n, qm, lambda, nullptr));
          ASSERT_EQ(gpuSuccess, gpuMemcpy(gpu.data(), d.out, nllr * sizeof(int16_t), gpuMemcpyDeviceToHost));
          EXPECT_EQ(0u, diff_re(cpu, gpu, n, qm)) << "Qm=" << qm << " ill=" << ill << " lambda=" << lambda << " n=" << n;
          close.add(prod, gpu);
        }
  printf("[  INFO  ] MMSE vs FMA-contracted CPU: %.4f%% LLRs differ, %.5f%% opposite sign\n", 100.0 * close.ndiff / close.n,
         100.0 * close.nsign / close.n);
  EXPECT_LT(close.nsign, close.n / 10000) << "rounding-level only";
}

// QPSK / 16QAM 2-layer ML: integer kernels, bit-exact with the production CPU build (saturating inputs included)
TEST(test_detector_gpu, ml_qpsk_16qam_bit_exact)
{
  if (!have_gpu())
    GTEST_SKIP() << "no GPU";
  for (int qm : {2, 4})
    for (double scale : {0.1, 1.0}) // 1.0: inputs over the full int16 range, saturating the int16 arithmetic
      for (uint32_t n : {33u, 1272u, 39312u}) {
        std::mt19937 rng(0x1640u + qm * 13 + n + (unsigned)(scale * 10));
        std::uniform_int_distribution<int> v(-32768, 32767), m(0, 32767);
        const uint32_t np = n + 64; // the CPU kernels read whole vectors past n
        ABuf<c16_t> s0(np), s1(np), mag(np), magi(np), rho(np);
        auto sc = [&](int x) { return (int16_t)(x * scale); };
        for (uint32_t i = 0; i < np; i++) {
          s0[i] = {sc(v(rng)), sc(v(rng))};
          s1[i] = {sc(v(rng)), sc(v(rng))};
          const int16_t a = sc(m(rng)), b = sc(m(rng));
          mag[i] = {a, a};
          magi[i] = {b, b};
          rho[i] = {sc(v(rng)), sc(v(rng))};
        }
        const size_t nllr = (size_t)n * qm;
        ABuf<int16_t> cpu(nllr + 64);
        if (qm == 2)
          nr_qpsk_llr_2layer(s0.data(), s1.data(), cpu.data(), rho.data(), n);
        else
          nr_qam16_llr_2layer(s0.data(), s1.data(), mag.data(), magi.data(), cpu.data(), rho.data(), n);
        In in;
        in.s0.assign(s0.p, s0.p + np);
        in.s1.assign(s1.p, s1.p + np);
        in.mag.assign(mag.p, mag.p + np);
        in.magi.assign(magi.p, magi.p + np);
        in.rho.assign(rho.p, rho.p + np);
        DevIn d(in, nllr);
        ASSERT_EQ(0, nr_gpu_llr_2layer_lbest(d.s0, d.s1, d.mag, d.magi, d.out, d.rho, n, qm, 3, 0.0f, nullptr, nullptr));
        std::vector<int16_t> gpu(nllr), want(cpu.p, cpu.p + nllr);
        ASSERT_EQ(gpuSuccess, gpuMemcpy(gpu.data(), d.out, nllr * sizeof(int16_t), gpuMemcpyDeviceToHost));
        EXPECT_EQ(0u, diff_re(want, gpu, n, qm)) << "Qm=" << qm << " scale=" << scale << " n=" << n;
      }
}

} // namespace

int main(int argc, char **argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
