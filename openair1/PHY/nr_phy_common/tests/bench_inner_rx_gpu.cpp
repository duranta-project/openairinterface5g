/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Slot timing of the fused GPU 2-layer inner RX (nr_gpu_inner_rx_2layer: 12 data symbols x 273 PRB in one call)
// against the CPU path on one core (fused nr_inner_rx for L-best / ML; nr_channel_compensation + nr_compute_MMSE_llr
// for the MMSE), same inputs. usage: bench_inner_rx_gpu [nb_rx (default 4)] [ill-conditioned PRB fraction (0.05)]

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <random>
#include <vector>

#include "PHY/gpu_compat.h"
#include "openair1/PHY/TOOLS/phy_test_tools.hpp"
extern "C" {
#include "openair1/PHY/nr_phy_common/inc/nr_detector_gpu.h"
struct configmodule_interface_s;
struct configmodule_interface_s *uniqCfg = NULL;
void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  exit(1);
}
// VLA parameters in the C headers: declared with the decayed types
void nr_channel_compensation(uint32_t, uint32_t, int, int, c16_t *, c16_t *, c16_t *, c16_t *, c16_t *, c16_t **, c16_t *, c16_t, int, uint32_t, uint32_t);
bool nr_inner_rx(uint32_t, uint32_t, int, int, int, c16_t *, c16_t *, c16_t, c16_t **, c16_t **, c16_t **, c16_t **, c16_t *, c16_t *, int, bool, bool, int, float, int16_t **, const int16_t *, int16_t *);
void nr_compute_MMSE_llr(c16_t *, c16_t *, c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, int16_t *, uint32_t, uint8_t, float);
}

#define CHECK(x)                                                    \
  do {                                                              \
    if ((x) != gpuSuccess) {                                        \
      fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #x); \
      exit(1);                                                      \
    }                                                               \
  } while (0)

static double now_us(void)
{
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1e6 + t.tv_nsec * 1e-3;
}

int main(int argc, char **argv)
{
  const int nb_rx = argc > 1 ? atoi(argv[1]) : 4;
  const double ill = argc > 2 ? atof(argv[2]) : 0.05;
  const int nsym = 12, shift = nb_rx == 2 ? 7 : 8;
  const uint32_t len = 273 * 12, stride = (len + 15) & ~15u; // rows 32-byte aligned for the CPU kernels
  const float lambda = 20.0f;
  gpuDeviceProp_t prop;
  CHECK(gpuGetDeviceProperties(&prop, 0));
  printf("%s: 2 layers, %d RX, %d symbols x %u REs, %.0f%% ill-conditioned PRBs; us per slot\n", prop.name, nb_rx, nsym, len, ill * 100);

  // per-PRB channel (frequency-flat within the PRB), ill-conditioned PRBs with nearly collinear layers
  std::mt19937 rng(7);
  std::normal_distribution<double> g(0, 1);
  std::uniform_real_distribution<double> u(0, 1);
  auto c16 = [](double r, double i) {
    return c16_t{(int16_t)fmax(-32768, fmin(32767, r)), (int16_t)fmax(-32768, fmin(32767, i))};
  };
  const double amp = 400; // ch_mag ~ 5000 after the shift, as log2_maxh sets it
  std::vector<AlignedVector512<c16_t>> rx(nsym), ch(nsym);
  std::vector<AlignedVector512<int16_t>> scr(nsym), cw(nsym);
  for (int s = 0; s < nsym; s++) {
    rx[s].resize((size_t)nb_rx * stride);
    ch[s].resize((size_t)2 * nb_rx * stride);
    scr[s].resize((size_t)len * 16);
    cw[s].resize((size_t)len * 16);
    for (auto &v : scr[s])
      v = u(rng) < 0.5 ? 1 : -1;
  }
  for (uint32_t prb = 0; prb < len / 12; prb++) {
    const bool bad = u(rng) < ill;
    std::vector<double> h(4 * nb_rx);
    for (int a = 0; a < nb_rx; a++) {
      h[4 * a] = g(rng) * amp;
      h[4 * a + 1] = g(rng) * amp;
      h[4 * a + 2] = bad ? 0.98 * h[4 * a + 1] + 0.05 * amp * g(rng) : g(rng) * amp;
      h[4 * a + 3] = bad ? -0.98 * h[4 * a] + 0.05 * amp * g(rng) : g(rng) * amp;
    }
    for (int s = 0; s < nsym; s++)
      for (uint32_t i = prb * 12; i < prb * 12 + 12; i++)
        for (int a = 0; a < nb_rx; a++) {
          ch[s][(size_t)a * stride + i] = c16(h[4 * a], h[4 * a + 1]);
          ch[s][(size_t)(nb_rx + a) * stride + i] = c16(h[4 * a + 2], h[4 * a + 3]);
          rx[s][(size_t)a * stride + i] = c16(g(rng) * amp * 1.5, g(rng) * amp * 1.5);
        }
  }

  // GPU buffers and symbol descriptors
  std::vector<nr_gpu_rx_sym_t> desc(nsym);
  for (int s = 0; s < nsym; s++) {
    c16_t *drx, *dch;
    int16_t *dscr, *dcw;
    CHECK(gpuMalloc((void **)&drx, rx[s].size() * sizeof(c16_t)));
    CHECK(gpuMalloc((void **)&dch, ch[s].size() * sizeof(c16_t)));
    CHECK(gpuMalloc((void **)&dscr, scr[s].size() * sizeof(int16_t)));
    CHECK(gpuMalloc((void **)&dcw, cw[s].size() * sizeof(int16_t)));
    CHECK(gpuMemcpy(drx, rx[s].data(), rx[s].size() * sizeof(c16_t), gpuMemcpyHostToDevice));
    CHECK(gpuMemcpy(dch, ch[s].data(), ch[s].size() * sizeof(c16_t), gpuMemcpyHostToDevice));
    CHECK(gpuMemcpy(dscr, scr[s].data(), scr[s].size() * sizeof(int16_t), gpuMemcpyHostToDevice));
    desc[s] = {drx, dch, dscr, dcw, len, stride, {32767, 0}};
  }
  nr_gpu_det_ws_t *ws = nr_gpu_det_ws_alloc(2 * nsym * len);
  gpuStream_t st;
  CHECK(gpuStreamCreateWithFlags(&st, gpuStreamNonBlocking));
  gpuEvent_t e0, e1;
  CHECK(gpuEventCreate(&e0));
  CHECK(gpuEventCreate(&e1));

  // CPU scratch (production fused path / compensation + MMSE)
  AlignedVector512<c16_t> comp0(stride), comp1(stride), maga(2 * stride), magb(2 * stride), magc(2 * stride), rho(4 * stride);
  AlignedVector512<int16_t> l0((size_t)(stride + 32) * 8), l1((size_t)(stride + 32) * 8);

  struct {
    const char *name;
    int qm, det;
  } cases[] = {{"MMSE QPSK", 2, NR_GPU_DET_MMSE},
               {"MMSE 16QAM", 4, NR_GPU_DET_MMSE},
               {"MMSE 64QAM", 6, NR_GPU_DET_MMSE},
               {"MMSE 256QAM", 8, NR_GPU_DET_MMSE},
               {"ML QPSK", 2, NR_GPU_DET_ML},
               {"ML 16QAM", 4, NR_GPU_DET_ML},
               {"L-best 64QAM", 6, NR_GPU_DET_LBEST},
               {"L-best 256QAM", 8, NR_GPU_DET_LBEST},
               {"ML 64QAM", 6, NR_GPU_DET_ML},
               {"ML 256QAM", 8, NR_GPU_DET_ML}};
  printf("  %-14s %10s %12s %8s\n", "", "GPU", "CPU 1 core", "ratio");
  for (auto &c : cases) {
    float best = 1e30f;
    for (int it = 0; it < 105; it++) {
      CHECK(gpuEventRecord(e0, st));
      if (nr_gpu_inner_rx_2layer(desc.data(), nsym, nb_rx, c.qm, c.det, lambda, shift, ws, st))
        exit(1);
      CHECK(gpuEventRecord(e1, st));
      CHECK(gpuEventSynchronize(e1));
      float ms;
      CHECK(gpuEventElapsedTime(&ms, e0, e1));
      if (it >= 5 && ms * 1000 < best)
        best = ms * 1000;
    }
    double cbest = 1e30;
    for (int it = 0; it < 7; it++) {
      const double t0 = now_us();
      for (int s = 0; s < nsym; s++) {
        int16_t *lay[2] = {l0.data(), l1.data()};
        if (c.det == NR_GPU_DET_MMSE) {
          c16_t *rxComp[2] = {comp0.data(), comp1.data()};
          nr_channel_compensation(stride, stride, nb_rx, 2, rx[s].data(), ch[s].data(), maga.data(), magb.data(), magc.data(), rxComp, rho.data(), {32767, 0}, c.qm, 0, shift);
          nr_compute_MMSE_llr(comp0.data(), comp1.data(), &rho[0], &rho[stride], &rho[2 * stride], &rho[3 * stride], l0.data(), l1.data(), len, c.qm, lambda);
        } else {
          const int lbest = c.det == NR_GPU_DET_ML ? 0 : (c.qm == 8 ? 2 : 1);
          nr_inner_rx(len, stride, nb_rx, 2, c.qm, rx[s].data(), ch[s].data(), {32767, 0}, NULL, NULL, NULL, NULL, NULL, NULL, shift, true, true, lbest, lambda, lay, scr[s].data(), cw[s].data());
        }
      }
      const double dt = now_us() - t0;
      cbest = dt < cbest ? dt : cbest;
    }
    printf("  %-14s %10.1f %12.0f %7.0fx\n", c.name, best, cbest, cbest / best);
  }
  return 0;
}
