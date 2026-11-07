/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Slot-level timing of the GPU 2-layer detectors: both layers of a 273-PRB, 12-data-symbol slot in one call each.
// usage: bench_detector_gpu [ill-conditioned fraction, default 0.05]

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <random>
#include <vector>

#include "PHY/gpu_compat.h" // C++ runtime headers: outside extern "C"
extern "C" {
#include "openair1/PHY/nr_phy_common/inc/nr_detector_gpu.h"
}

#define CHECK(x)                                                    \
  do {                                                              \
    if ((x) != gpuSuccess) {                                        \
      fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #x); \
      exit(1);                                                      \
    }                                                               \
  } while (0)

int main(int argc, char **argv)
{
  const double ill = argc > 1 ? atof(argv[1]) : 0.05;
  const uint32_t n = 273 * 12 * 12, iters = 200;
  gpuDeviceProp_t prop;
  CHECK(gpuGetDeviceProperties(&prop, 0));
  printf("%s, %u REs/layer, %.0f%% ill-conditioned (in runs of one PRB)\n", prop.name, n, ill * 100);

  // ill-conditioned REs in runs of 12 (a PRB), as frequency-selective fades come
  std::mt19937 rng(42);
  std::uniform_real_distribution<double> pw(2000, 16000), ph(0, 2 * M_PI), u(0, 1), z(-12000, 12000);
  // h[4]: rho in Gram units (MMSE), h[5] / h[6]: in the ch_mag units of the 64QAM / 256QAM L-best (x ch_mag -> ||h||^2)
  const double psc[2] = {6.48074069840786 / 4, 13.038404810405298 / 8};
  std::vector<c16_t> h[7];
  for (auto &v : h)
    v.resize(n);
  bool bad = false;
  for (uint32_t i = 0; i < n; i++) {
    if (i % 12 == 0)
      bad = u(rng) < ill;
    const double p0 = pw(rng), p1 = pw(rng), a = (bad ? 0.995 : 0.5 * u(rng)) * sqrt(p0 * p1), t = ph(rng);
    for (int q = 0; q < 2; q++)
      h[5 + q][i] = {(int16_t)(a * psc[q] * cos(t)), (int16_t)(a * psc[q] * sin(t))};
    h[0][i] = {(int16_t)z(rng), (int16_t)z(rng)};
    h[1][i] = {(int16_t)z(rng), (int16_t)z(rng)};
    h[2][i] = {(int16_t)p0, (int16_t)p0};
    h[3][i] = {(int16_t)p1, (int16_t)p1};
    h[4][i] = {(int16_t)(a * cos(t)), (int16_t)(a * sin(t))};
  }
  c16_t *d[7];
  for (int k = 0; k < 7; k++) {
    CHECK(gpuMalloc((void **)&d[k], n * sizeof(c16_t)));
    CHECK(gpuMemcpy(d[k], h[k].data(), n * sizeof(c16_t), gpuMemcpyHostToDevice));
  }
  int16_t *out[2];
  for (int l = 0; l < 2; l++)
    CHECK(gpuMalloc((void **)&out[l], (size_t)n * 8 * sizeof(int16_t)));
  nr_gpu_det_ws_t *ws = nr_gpu_det_ws_alloc(n);
  gpuStream_t st;
  CHECK(gpuStreamCreateWithFlags(&st, gpuStreamNonBlocking));
  gpuEvent_t e0, e1;
  CHECK(gpuEventCreate(&e0));
  CHECK(gpuEventCreate(&e1));

  struct {
    const char *name;
    int qm, kind; // kind: 0 MMSE, 1 L-best, 3 full ML
  } cases[] = {{"MMSE QPSK", 2, 0},
               {"MMSE 16QAM", 4, 0},
               {"MMSE 64QAM", 6, 0},
               {"MMSE 256QAM", 8, 0},
               {"L-best 64QAM", 6, 1},
               {"L-best 256QAM", 8, 1},
               {"ML 64QAM", 6, 3},
               {"ML 256QAM", 8, 3}};
  for (auto &c : cases) {
    float best = 1e30f;
    for (uint32_t it = 0; it < iters + 5; it++) {
      CHECK(gpuEventRecord(e0, st));
      for (int l = 0; l < 2; l++) { // layer 1: the same buffers swapped, as the CPU receiver calls it
        c16_t *s0 = d[l], *s1 = d[1 - l], *m0 = d[2 + l], *m1 = d[3 - l];
        if (c.kind == 0)
          nr_gpu_llr_2layer_mmse(s0, s1, m0, m1, d[4], out[l], n, c.qm, 30.0f, st);
        else
          nr_gpu_llr_2layer_lbest(s0, s1, m0, m1, out[l], d[c.qm == 6 ? 5 : 6], n, c.qm, c.kind, 30.0f, ws, st);
      }
      CHECK(gpuEventRecord(e1, st));
      CHECK(gpuEventSynchronize(e1));
      float ms;
      CHECK(gpuEventElapsedTime(&ms, e0, e1));
      if (it >= 5 && ms < best)
        best = ms;
    }
    printf("  %-14s %8.1f us/slot (2 layers)\n", c.name, best * 1000.0f);
  }

  // host <-> device traffic of one slot (inputs in, LLRs out) for an explicit-copy integration
  std::vector<int16_t> llr((size_t)n * 8);
  float best_in = 1e30f, best_out = 1e30f;
  for (int it = 0; it < 50; it++) {
    float ms;
    CHECK(gpuEventRecord(e0, st));
    for (int k = 0; k < 5; k++)
      CHECK(gpuMemcpyAsync(d[k], h[k].data(), n * sizeof(c16_t), gpuMemcpyHostToDevice, st));
    CHECK(gpuEventRecord(e1, st));
    CHECK(gpuEventSynchronize(e1));
    CHECK(gpuEventElapsedTime(&ms, e0, e1));
    best_in = ms < best_in ? ms : best_in;
    CHECK(gpuEventRecord(e0, st));
    for (int l = 0; l < 2; l++)
      CHECK(gpuMemcpyAsync(llr.data(), out[l], (size_t)n * 6 * sizeof(int16_t), gpuMemcpyDeviceToHost, st));
    CHECK(gpuEventRecord(e1, st));
    CHECK(gpuEventSynchronize(e1));
    CHECK(gpuEventElapsedTime(&ms, e0, e1));
    best_out = ms < best_out ? ms : best_out;
  }
  printf("  copy in (5 x c16, pageable) %8.1f us, LLRs out (2 layers 64QAM) %8.1f us\n", best_in * 1000.0f, best_out * 1000.0f);
  return 0;
}
