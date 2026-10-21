#include <chrono>
#include <algorithm>
#include <cstdio>
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
void nr_compute_ML_llr(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, int16_t *, c16_t *, c16_t *, uint32_t, uint8_t, int);
uint8_t nr_compute_MMSE_llr(c16_t **, uint32_t, uint32_t, int, int, c16_t *, c16_t *, c16_t *, c16_t *, unsigned short,
                            unsigned char, int, unsigned char, int, uint32_t, c16_t *, c16_t *, c16_t *, c16_t *, int16_t **);
}
#include "openair1/PHY/TOOLS/phy_test_tools.hpp"
#include <random>

// Per-slot (12 data symbols) cost of the 2-layer PUSCH LLR stage, from the matched-filter output
// on: ML / L-best = nr_compute_ML_llr (OAI_LBEST=0/1, separate processes: the gate is a static),
// MMSE = nr_compute_MMSE_llr (Gram-fed 2x2 equalization + scalar LLR). Both consume the same
// rxdataF_comp/ch_mag/rho that channel compensation produces. usage: bench_rx2l ml|mmse [Qm]
int main(int argc, char **argv)
{
  logInit();
  const bool mmse = argc > 1 && !strcmp(argv[1], "mmse");
  const char *e = getenv("OAI_LBEST");
  const int qm = argc > 2 ? atoi(argv[2]) : 6, nsym = 12, iters = 300;
  for (uint32_t nb_re : {1272u, 3276u}) {
    const uint32_t B = (nb_re + 64 + 15) & ~15u; // layer-1 slices (base + B) must stay 64-byte aligned
    std::mt19937 rng(0xBE);
    std::uniform_int_distribution<int> sig(-6000, 6000), nz(-300, 300);
    AlignedVector512<c16_t> s0(B), s1(B), w0(B), w1(B), mag(2 * B), magb(2 * B), magc(2 * B), chF(2 * 4 * B);
    AlignedVector512<c16_t> r00(B), r01(B), r10(B), r11(B);
    for (uint32_t i = 0; i < B; i++) {
      s0[i] = {(int16_t)sig(rng), (int16_t)sig(rng)};
      s1[i] = {(int16_t)sig(rng), (int16_t)sig(rng)};
      r00[i] = {(int16_t)(4096 + nz(rng)), 0};
      r11[i] = {(int16_t)(4096 + nz(rng)), 0};
      r01[i] = {(int16_t)nz(rng), (int16_t)nz(rng)};
      r10[i] = {r01[i].r, (int16_t)-r01[i].i};
      mag[i] = mag[B + i] = {4096, 4096};
    }
    AlignedVector512<int16_t> o0((size_t)nb_re * 8 + 256), o1((size_t)nb_re * 8 + 256);
    int16_t *llr[2] = {o0.data(), o1.data()};
    std::vector<double> t;
    for (int it = 0; it < iters; it++) {
      double us = 0;
      for (int s = 0; s < nsym; s++) {
        memcpy(w0.data(), s0.data(), B * sizeof(c16_t)); // MMSE equalizes in place
        memcpy(w1.data(), s1.data(), B * sizeof(c16_t));
        c16_t *comp[2] = {w0.data(), w1.data()};
        auto a = std::chrono::steady_clock::now();
        if (mmse)
          nr_compute_MMSE_llr(comp, B, B, 4, 2, mag.data(), magb.data(), magc.data(), chF.data(), nb_re / 12, qm, 12, 0,
                              nb_re, 100, r00.data(), r01.data(), r10.data(), r11.data(), llr);
        else
          nr_compute_ML_llr(w0.data(), w1.data(), mag.data(), mag.data() + B, llr[0], llr[1], r01.data(), r10.data(), nb_re, qm, e && atoi(e));
        auto b = std::chrono::steady_clock::now();
        us += std::chrono::duration<double, std::micro>(b - a).count();
      }
      t.push_back(us);
    }
    std::sort(t.begin(), t.end());
    printf("%-7s Qm=%d nb_re=%5u  median %8.1f us/slot  min %8.1f\n", mmse ? "MMSE" : (e && atoi(e) ? "L-best" : "ML"), qm, nb_re,
           t[t.size() / 2], t[0]);
  }
  return 0;
}
