/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Slot-batched GPU 2-layer inner RX (nr_gpu_inner_rx_2layer) against the CPU chain: the front end bit-exact with
// nr_channel_compensation, the fused chain bit-exact with {nr_channel_compensation, uncontracted CPU detectors,
// output shift, nr_layer_demapping, descrambling}, and rounding-level against the production fused nr_inner_rx.

#include "gtest/gtest.h"
#include <stdint.h>
#include <vector>
#include <random>
#include <cmath>
#include <cstring>

#include "PHY/gpu_compat.h" // C++ runtime headers: outside extern "C"
#include "openair1/PHY/TOOLS/phy_test_tools.hpp" // AlignedVector512: the CPU kernels load aligned vectors
extern "C" {
#include "openair1/PHY/nr_phy_common/inc/nr_detector_gpu.h"
// the CPU entry points take VLA parameters, which C++ cannot parse: declared with the decayed types
void nr_channel_compensation(uint32_t buffer_length,
                             uint32_t pdsch_buf_size_max,
                             int nb_rx_ant,
                             int nb_layers,
                             c16_t *rxFext,
                             c16_t *chFext,
                             c16_t *ch_maga,
                             c16_t *ch_magb,
                             c16_t *ch_magc,
                             c16_t **rxComp,
                             c16_t *rho,
                             c16_t cpe,
                             int mod_order,
                             uint32_t symbol,
                             uint32_t output_shift);
bool nr_inner_rx(uint32_t length,
                 uint32_t buffer_length,
                 int nb_rx_ant,
                 int nb_layer,
                 int mod_order,
                 c16_t *rxFext,
                 c16_t *chFext,
                 c16_t cpe,
                 c16_t **rxComp,
                 c16_t **mag_a,
                 c16_t **mag_b,
                 c16_t **mag_c,
                 c16_t *rho01,
                 c16_t *rho10,
                 int output_shift,
                 bool fuse,
                 bool ml2,
                 int lbest,
                 float lambda,
                 int16_t **layer_scratch,
                 const int16_t *scramble,
                 int16_t *llr_cw);
void nr_qam64_llr_2layer_lbest_f32_simd_w128_exact(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, c16_t *, uint32_t, int, float);
void nr_qam256_llr_2layer_lbest_f32_simd_w128_exact(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, c16_t *, uint32_t, int, float);
void nr_qpsk_llr_2layer(c16_t *, c16_t *, int16_t *, c16_t *, uint32_t);
void nr_qam16_llr_2layer(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, c16_t *, uint32_t);
void nr_llr_2layer_mmse_simd_w128_exact(const c16_t *, const c16_t *, const c16_t *, const c16_t *, const c16_t *, int16_t *, uint32_t, int, float);
}

namespace {

bool have_gpu()
{
  void *p = nullptr;
  if (gpuMalloc(&p, 16) != gpuSuccess)
    return false;
  (void)gpuFree(p);
  return true;
}

template <typename V>
typename V::value_type *to_dev(const V &h)
{
  typedef typename V::value_type T;
  T *d = nullptr;
  EXPECT_EQ(gpuSuccess, gpuMalloc((void **)&d, h.size() * sizeof(T) + 64));
  EXPECT_EQ(gpuSuccess, gpuMemcpy(d, h.data(), h.size() * sizeof(T), gpuMemcpyHostToDevice));
  return d;
}

template <typename T>
std::vector<T> from_dev(const T *d, size_t n)
{
  std::vector<T> h(n);
  EXPECT_EQ(gpuSuccess, gpuMemcpy(h.data(), d, n * sizeof(T), gpuMemcpyDeviceToHost));
  return h;
}

// One symbol: rxF [nb_rx][stride], chF [2][nb_rx][stride]. ill: fraction of REs whose layer-1 channel is nearly
// collinear with layer 0 (the adaptive window's full-grid REs); amp: channel amplitude (large values exercise the
// saturations and wrap-arounds of the integer front end)
struct Sym {
  uint32_t length, stride;
  AlignedVector512<c16_t> rxF, chF;
  AlignedVector512<int16_t> scr;
};

Sym gen_sym(uint32_t length, int nb_rx, int qm, double ill, double amp, std::mt19937 &rng)
{
  std::normal_distribution<double> g(0, 1);
  std::uniform_real_distribution<double> u(0, 1);
  Sym s;
  s.length = length;
  s.stride = (length + 15) & ~15u;
  s.rxF.assign((size_t)nb_rx * s.stride, {0, 0});
  s.chF.assign((size_t)2 * nb_rx * s.stride, {0, 0});
  auto c16 = [](double r, double i) {
    return c16_t{(int16_t)std::max(-32768.0, std::min(32767.0, r)), (int16_t)std::max(-32768.0, std::min(32767.0, i))};
  };
  for (uint32_t i = 0; i < length; i++) {
    const bool bad = u(rng) < ill;
    for (int a = 0; a < nb_rx; a++) {
      const double h0r = g(rng) * amp, h0i = g(rng) * amp;
      const double h1r = bad ? 0.98 * h0i + 0.05 * amp * g(rng) : g(rng) * amp, h1i = bad ? -0.98 * h0r + 0.05 * amp * g(rng) : g(rng) * amp;
      s.chF[(size_t)a * s.stride + i] = c16(h0r, h0i);
      s.chF[(size_t)(nb_rx + a) * s.stride + i] = c16(h1r, h1i);
      s.rxF[(size_t)a * s.stride + i] = c16(g(rng) * amp * 1.5, g(rng) * amp * 1.5);
    }
  }
  s.scr.resize((size_t)length * 2 * qm);
  for (auto &v : s.scr)
    v = u(rng) < 0.5 ? 1 : -1;
  return s;
}

// CPU front end of one symbol: comp0/1, mag_a0/1, rho[2][2], length REs
struct CpuFe {
  AlignedVector512<c16_t> comp[2], mag[2], rho[4]; // length REs + 64 zero padding (the int16 kernels read whole vectors)
};

CpuFe cpu_front_end(Sym &s, int nb_rx, c16_t cpe, int qm, int shift)
{
  const uint32_t bl = s.stride;
  CpuFe f;
  AlignedVector512<c16_t> magb(2 * bl), magc(2 * bl), maga(2 * bl), rho(4 * bl), comp0(bl), comp1(bl);
  c16_t *rxComp[2] = {comp0.data(), comp1.data()};
  nr_channel_compensation(bl, bl, nb_rx, 2, s.rxF.data(), s.chF.data(), maga.data(), magb.data(), magc.data(), rxComp, rho.data(), cpe, qm, 0, shift);
  f.comp[0].assign(comp0.begin(), comp0.begin() + s.length);
  f.comp[1].assign(comp1.begin(), comp1.begin() + s.length);
  for (int l = 0; l < 2; l++)
    f.mag[l].assign(maga.begin() + l * bl, maga.begin() + l * bl + s.length);
  for (int k = 0; k < 4; k++)
    f.rho[k].assign(rho.begin() + k * bl, rho.begin() + k * bl + s.length);
  for (auto *v : {&f.comp[0], &f.comp[1], &f.mag[0], &f.mag[1], &f.rho[0], &f.rho[1], &f.rho[2], &f.rho[3]})
    v->resize(s.length + 64, c16_t{0, 0});
  return f;
}

TEST(test_inner_rx_gpu, front_end_bit_exact)
{
  if (!have_gpu())
    GTEST_SKIP() << "no GPU";
  std::mt19937 rng(0xFE0);
  for (int nb_rx : {1, 2, 4})
    for (int qm : {2, 4, 6, 8})
      for (c16_t cpe : {c16_t{32767, 0}, c16_t{32700, -2000}})
        for (double amp : {600.0, 12000.0})
          for (int shift : {3, 9}) {
            Sym s = gen_sym(1000, nb_rx, qm, 0.2, amp, rng);
            CpuFe ref = cpu_front_end(s, nb_rx, cpe, qm, shift);
            c16_t *rx = to_dev(s.rxF), *ch = to_dev(s.chF), *out[8];
            for (auto &o : out)
              ASSERT_EQ(gpuSuccess, gpuMalloc((void **)&o, s.length * sizeof(c16_t)));
            ASSERT_EQ(0, nr_gpu_compensation_2layer(rx, ch, s.stride, s.length, nb_rx, cpe, qm, shift, out, nullptr));
            const AlignedVector512<c16_t> *want[8] = {&ref.comp[0], &ref.comp[1], &ref.mag[0], &ref.mag[1], &ref.rho[0], &ref.rho[1], &ref.rho[2], &ref.rho[3]};
            const char *name[8] = {"comp0", "comp1", "mag0", "mag1", "rho00", "rho01", "rho10", "rho11"};
            for (int k = 0; k < 8; k++) {
              if (qm == 2 && (k == 2 || k == 3)) // no magnitudes for QPSK
                continue;
              auto got = from_dev(out[k], s.length);
              uint32_t i = 0;
              while (i < s.length && got[i].r == (*want[k])[i].r && got[i].i == (*want[k])[i].i)
                i++;
              EXPECT_EQ(s.length, i) << name[k] << " nb_rx=" << nb_rx << " Qm=" << qm << " cpe=" << cpe.r << "," << cpe.i << " amp=" << amp
                                     << " shift=" << shift << ": RE " << i << " GPU (" << (i < s.length ? got[i].r : 0) << ","
                                     << (i < s.length ? got[i].i : 0) << ") CPU (" << (i < s.length ? (*want[k])[i].r : 0) << ","
                                     << (i < s.length ? (*want[k])[i].i : 0) << "), h0 (" << s.chF[i].r << "," << s.chF[i].i << ")";
            }
            for (auto *p : {(void *)rx, (void *)ch})
              (void)gpuFree(p);
            for (auto &o : out)
              (void)gpuFree(o);
          }
}

// CPU reference chain of one symbol: front end, uncontracted detectors, output shift, layer demap, descrambling
std::vector<int16_t> cpu_chain(Sym &s, int nb_rx, c16_t cpe, int qm, int det, float lambda, int shift)
{
  CpuFe f = cpu_front_end(s, nb_rx, cpe, qm, shift);
  const uint32_t n = s.length;
  AlignedVector512<int16_t> lay[2] = {AlignedVector512<int16_t>((size_t)(n + 64) * qm), AlignedVector512<int16_t>((size_t)(n + 64) * qm)};
  int osh;
  if (det == NR_GPU_DET_MMSE) {
    nr_llr_2layer_mmse_simd_w128_exact(f.comp[0].data(), f.comp[1].data(), f.rho[0].data(), f.rho[3].data(), f.rho[1].data(), lay[0].data(), n, qm, lambda);
    nr_llr_2layer_mmse_simd_w128_exact(f.comp[1].data(), f.comp[0].data(), f.rho[3].data(), f.rho[0].data(), f.rho[2].data(), lay[1].data(), n, qm, lambda);
    osh = qm == 2 ? 4 : qm == 6 ? 1 : 0;
  } else if (qm == 2) { // exact ML for L-best and ML alike, as nr_compute_ML_llr
    nr_qpsk_llr_2layer(f.comp[0].data(), f.comp[1].data(), lay[0].data(), f.rho[1].data(), n);
    nr_qpsk_llr_2layer(f.comp[1].data(), f.comp[0].data(), lay[1].data(), f.rho[2].data(), n);
    osh = 4;
  } else if (qm == 4) {
    nr_qam16_llr_2layer(f.comp[0].data(), f.comp[1].data(), f.mag[0].data(), f.mag[1].data(), lay[0].data(), f.rho[1].data(), n);
    nr_qam16_llr_2layer(f.comp[1].data(), f.comp[0].data(), f.mag[1].data(), f.mag[0].data(), lay[1].data(), f.rho[2].data(), n);
    osh = 0;
  } else {
    auto *k = qm == 6 ? nr_qam64_llr_2layer_lbest_f32_simd_w128_exact : nr_qam256_llr_2layer_lbest_f32_simd_w128_exact;
    k(f.comp[0].data(), f.comp[1].data(), f.mag[0].data(), f.mag[1].data(), lay[0].data(), f.rho[1].data(), n, det, lambda);
    k(f.comp[1].data(), f.comp[0].data(), f.mag[1].data(), f.mag[0].data(), lay[1].data(), f.rho[2].data(), n, det, lambda);
    osh = qm == 6 ? 1 : 0;
  }
  std::vector<int16_t> cw((size_t)n * 2 * qm);
  for (uint32_t i = 0; i < n; i++)
    for (int t = 0; t < 2; t++)
      for (int p = 0; p < qm; p++) {
        const size_t k = ((size_t)i * 2 + t) * qm + p;
        cw[k] = (int16_t)((int16_t)(lay[t][(size_t)i * qm + p] >> osh) * s.scr[k]);
      }
  return cw;
}

struct Slot {
  std::vector<Sym> sym;
  std::vector<c16_t *> drx, dch;
  std::vector<int16_t *> dscr, dcw;
  std::vector<nr_gpu_rx_sym_t> desc;
  Slot(std::vector<Sym> s, int qm, const std::vector<c16_t> &cpe) : sym(std::move(s))
  {
    for (size_t k = 0; k < sym.size(); k++) {
      drx.push_back(to_dev(sym[k].rxF));
      dch.push_back(to_dev(sym[k].chF));
      dscr.push_back(to_dev(sym[k].scr));
      dcw.push_back(to_dev(AlignedVector512<int16_t>((size_t)sym[k].length * 2 * qm, 0x5a5a)));
      desc.push_back({drx[k], dch[k], dscr[k], dcw[k], sym[k].length, sym[k].stride, cpe[k]});
    }
  }
  ~Slot()
  {
    for (size_t k = 0; k < sym.size(); k++)
      for (void *p : {(void *)drx[k], (void *)dch[k], (void *)dscr[k], (void *)dcw[k]})
        (void)gpuFree(p);
  }
};

TEST(test_inner_rx_gpu, fused_chain_bit_exact)
{
  if (!have_gpu())
    GTEST_SKIP() << "no GPU";
  nr_gpu_det_ws_t *ws = nr_gpu_det_ws_alloc(1u << 16);
  ASSERT_NE(nullptr, ws);
  std::mt19937 rng(0xF05);
  const std::vector<c16_t> cpe = {{32767, 0}, {32610, 3200}, {32767, 0}};
  struct {
    int det, qm;
  } cases[] = {{NR_GPU_DET_MMSE, 2},
               {NR_GPU_DET_MMSE, 4},
               {NR_GPU_DET_MMSE, 6},
               {NR_GPU_DET_MMSE, 8},
               {NR_GPU_DET_LBEST, 2},
               {NR_GPU_DET_LBEST, 4},
               {NR_GPU_DET_ML, 2},
               {NR_GPU_DET_ML, 4},
               {NR_GPU_DET_LBEST, 6},
               {NR_GPU_DET_LBEST, 8},
               {NR_GPU_DET_ML, 6},
               {NR_GPU_DET_ML, 8}};
  for (auto &c : cases)
    for (int nb_rx : {2, 4})
      for (float lambda : {0.0f, 40.0f}) {
        const int shift = nb_rx == 2 ? 7 : 8;
        std::vector<Sym> syms;
        for (uint32_t len : {1272u, 1000u, 636u})
          syms.push_back(gen_sym(len, nb_rx, c.qm, 0.15, 1500, rng));
        Slot slot(syms, c.qm, cpe);
        ASSERT_EQ(0, nr_gpu_inner_rx_2layer(slot.desc.data(), (int)syms.size(), nb_rx, c.qm, c.det, lambda, shift, ws, nullptr));
        for (size_t k = 0; k < syms.size(); k++) {
          auto want = cpu_chain(slot.sym[k], nb_rx, cpe[k], c.qm, c.det, lambda, shift);
          auto got = from_dev(slot.dcw[k], want.size());
          uint32_t bad = 0;
          for (uint32_t i = 0; i < syms[k].length; i++)
            bad += memcmp(&got[(size_t)i * 2 * c.qm], &want[(size_t)i * 2 * c.qm], 2 * c.qm * sizeof(int16_t)) != 0;
          EXPECT_EQ(0u, bad) << "det=" << c.det << " Qm=" << c.qm << " nb_rx=" << nb_rx << " lambda=" << lambda << " symbol " << k;
        }
      }
  nr_gpu_det_ws_free(ws);
}

// against the production fused CPU inner RX (FMA-contracted detectors): rounding-level only
TEST(test_inner_rx_gpu, close_to_production_inner_rx)
{
  if (!have_gpu())
    GTEST_SKIP() << "no GPU";
  nr_gpu_det_ws_t *ws = nr_gpu_det_ws_alloc(1u << 16);
  std::mt19937 rng(0xC10);
  size_t n = 0, ndiff = 0, nsign = 0;
  for (int qm : {2, 4, 6, 8})
    for (int det : {NR_GPU_DET_LBEST, NR_GPU_DET_ML}) {
      const int nb_rx = 2, shift = 7;
      std::vector<Sym> syms = {gen_sym(1272, nb_rx, qm, 0.15, 1500, rng)};
      Slot slot(syms, qm, {c16_t{32767, 0}});
      ASSERT_EQ(0, nr_gpu_inner_rx_2layer(slot.desc.data(), 1, nb_rx, qm, det, 20.0f, shift, ws, nullptr));
      auto got = from_dev(slot.dcw[0], (size_t)1272 * 2 * qm);
      Sym &s = slot.sym[0];
      AlignedVector512<int16_t> cw(got.size()), l0((size_t)(s.stride + 32) * qm), l1((size_t)(s.stride + 32) * qm);
      int16_t *scratch[2] = {l0.data(), l1.data()};
      // lbest 2 = fp32 3x3 for 256QAM as on the GPU; 64QAM L-best is fp32 at lbest 1
      const int lbest = det == NR_GPU_DET_ML ? 0 : (qm == 8 ? 2 : 1);
      ASSERT_TRUE(nr_inner_rx(s.length, s.stride, nb_rx, 2, qm, s.rxF.data(), s.chF.data(), c16_t{32767, 0}, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, shift, true, true, lbest, 20.0f, scratch, s.scr.data(), cw.data()));
      for (size_t k = 0; k < got.size(); k++) {
        ndiff += got[k] != cw[k];
        nsign += (got[k] > 0 && cw[k] < 0) || (got[k] < 0 && cw[k] > 0);
      }
      n += got.size();
    }
  printf("[  INFO  ] fused GPU vs production nr_inner_rx: %.4f%% LLRs differ, %.5f%% opposite sign\n", 100.0 * ndiff / n, 100.0 * nsign / n);
  EXPECT_LT(ndiff, n / 100);
  EXPECT_LT(nsign, n / 10000);
  nr_gpu_det_ws_free(ws);
}

} // namespace
