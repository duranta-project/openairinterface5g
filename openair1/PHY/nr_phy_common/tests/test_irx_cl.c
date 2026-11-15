/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * The OpenCL inner-RX module (nr_irx_cl.c) through its offload interface against the CPU nr_inner_rx_2layer_f16: a
 * batch of 12 symbols of 273 PRB, two channel estimates, RE maps skipping PTRS-like REs, a PTRS phase error, packed
 * descrambling bits; the noisy 2-layer channels of test_llr_2layer fused_f16_units at the normal and 3x input level.
 * RX and LLRs through the staging, and in place (rxdataF-like rows and the codeword buffer from alloc_host) on the GPUs
 * whose module has alloc_host memory.
 * Reports the LLRs that differ, the first-bit decisions against the sent symbols, and the run time against the CPU on
 * one thread. Passes if the LLRs are the CPU's (the CPU receiver built without contraction) on a GPU running the exact
 * FP16 operations (Mali), or the decisions within 2% + 5 of the CPU's when the module runs mad() (Adreno).
 * Single layer, QPSK to 256QAM, 1 / 2 / 4 Rx, normal and saturating channel amplitudes, lengths with the scalar tail of
 * nr_compute_llr, one PTRS phase error per symbol: bit-exact with the production nr_inner_rx (nr_channel_compensation
 * and nr_compute_llr) on every GPU. Exit status 77 (skipped) without an OpenCL GPU.
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "PHY/TOOLS/tools_defs.h"
#include "nr_inner_rx_offload.h"
#include "openair1/PHY/nr_phy_common/src/nr_lbest_f16.h"
#include "nr_channel_compensation.h"

int nr_irx_init(uint32_t, int, int);
int nr_irx_shutdown(void);
nr_irx_ctx_t *nr_irx_acquire(void);
void nr_irx_release(nr_irx_ctx_t *);
void *nr_irx_staging(nr_irx_ctx_t *, int, int);
int nr_irx_run(nr_irx_ctx_t *, const nr_irx_sym_t *, int, const nr_irx_params_t *);
void *nr_irx_alloc_host(size_t);
bool nr_irx_cl_exact(void);
int nr_irx_free_host(void *);

static uint64_t rs;
static double urand(void)
{
  rs = rs * 6364136223846793005ull + 1442695040888963407ull;
  return ((double)(rs >> 11) + 0.5) * (1.0 / 9007199254740992.0);
}
static double gauss(void)
{
  return sqrt(-2.0 * log(urand())) * cos(2.0 * M_PI * urand());
}

static double now(void)
{
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + 1e-9 * t.tv_nsec;
}

// CPU time of the process: what a run costs the CPUs while the GPU works (a driver may spin or sleep in its waits)
static double cpu_now(void)
{
  struct timespec t;
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
  return t.tv_sec + 1e-9 * t.tv_nsec;
}

#define NSYM 12
#define NRE (273 * 12)
#define FFT 4096 // rxdataF row of a symbol: the allocation from RE 0

// single layer: 4 symbols (the allocation, PTRS-like REs skipped, 1001 and 3 REs: lengths with a scalar tail) per batch,
// through the staging (cw) and in place (rxd, cwd when the module has alloc_host memory); the number of failed cases
static int test_1layer(const uint32_t *bits, c16_t *const rxd[4], int16_t *cw, int16_t *cwd, int nmode)
{
  const uint32_t stride = (NRE + 15) & ~15u, lens[4] = {NRE, NRE, 1001, 3};
  const c16_t cpes[4] = {{32767, 0}, {32610, 3200}, {32767, 0}, {31000, -10600}};
  const size_t llr_max = (size_t)NRE * 8;
  c16_t *chx = malloc(sizeof(c16_t) * 4 * stride), *rxc = malloc(sizeof(c16_t) * 4 * stride);
  c16_t *comp = malloc(sizeof(c16_t) * stride), *ma = malloc(sizeof(c16_t) * stride), *mb = malloc(sizeof(c16_t) * stride),
        *mc = malloc(sizeof(c16_t) * stride);
  int16_t *ref = malloc(4 * llr_max * 2), *scr16 = malloc(llr_max * 2), *scratch = malloc((stride + 32) * 8 * 2);
  int fails = 0;
  for (int mode = 0; mode < nmode; mode++)
    for (int qm = 2; qm <= 8; qm += 2)
      for (int nb_rx = 1; nb_rx <= 4; nb_rx *= 2)
        for (double amp = 1500; amp <= 12000; amp += 10500) {
          const int shift = nb_rx == 1 ? 7 : 8, M = 1 << (qm / 2);
          rs = 0x1A7u + qm * 31 + nb_rx * 7 + (int)amp;
          nr_irx_ctx_t *x = nr_irx_acquire();
          if (!x)
            return 1;
          c16_t *ch = nr_irx_staging(x, 0, NR_IRX_STAGING_CH); // one estimate [nb_rx][stride]
          for (int a = 0; a < nb_rx; a++)
            for (int i = 0; i < NRE; i++)
              ch[(size_t)a * stride + i] = (c16_t){(int16_t)lrint(fmax(-32768, fmin(32767, gauss() * amp))),
                                                   (int16_t)lrint(fmax(-32768, fmin(32767, gauss() * amp)))};
          nr_irx_sym_t sym[4];
          const c16_t *rx_ref[4];
          uint32_t bit = 0;
          for (int k = 0; k < 4; k++) {
            uint16_t *map = nr_irx_staging(x, k, NR_IRX_STAGING_REMAP);
            uint32_t len = 0;
            for (uint32_t i = 0; i < lens[k] && len < lens[k]; i++)
              if (k != 1 || i % 25 != 3)
                map[len++] = i;
            c16_t *rx = nr_irx_staging(x, k, NR_IRX_STAGING_RX);
            rx_ref[k] = rx;
            for (uint32_t i = 0; i < len; i++) {
              const double xr = (2 * (int)(urand() * M) - M + 1) * 0.7, xi = (2 * (int)(urand() * M) - M + 1) * 0.7;
              for (int a = 0; a < nb_rx; a++) {
                const c16_t h = ch[(size_t)a * stride + map[i]];
                const double yr = (h.r * xr - h.i * xi) / M + gauss() * amp * 0.05,
                             yi = (h.r * xi + h.i * xr) / M + gauss() * amp * 0.05;
                rx[(size_t)a * stride + i] =
                    (c16_t){(int16_t)lrint(fmax(-32768, fmin(32767, yr))), (int16_t)lrint(fmax(-32768, fmin(32767, yi)))};
                if (mode)
                  rxd[a][k * FFT + map[i]] = rx[(size_t)a * stride + i];
              }
            }
            sym[k] = (nr_irx_sym_t){.rxF = mode ? NULL : rx,
                                    .chF = ch,
                                    .re_map = map,
                                    .scramble = nr_irx_staging(x, 0, NR_IRX_STAGING_SCRAMBLE),
                                    .scramble_offset = bit,
                                    .llr_cw = (mode ? cwd : cw) + k * llr_max,
                                    .length = len,
                                    .stride = stride,
                                    .ch_stride = stride,
                                    .cpe = cpes[k]};
            if (mode)
              for (int a = 0; a < nb_rx; a++)
                sym[k].rx_ant[a] = rxd[a] + k * FFT;
            bit += len * qm;
          }
          memcpy(nr_irx_staging(x, 0, NR_IRX_STAGING_SCRAMBLE), bits, (bit + 31) / 32 * 4);
          for (int k = 0; k < 4; k++) { // the production CPU receiver, unfused
            const nr_irx_sym_t *s = &sym[k];
            for (int a = 0; a < nb_rx; a++)
              for (uint32_t i = 0; i < s->length; i++) {
                chx[(size_t)a * stride + i] = ch[(size_t)a * stride + s->re_map[i]];
                rxc[(size_t)a * stride + i] = rx_ref[k][(size_t)a * stride + i];
              }
            for (uint32_t i = 0; i < s->length * qm; i++) {
              const uint32_t b = s->scramble_offset + i;
              scr16[i] = (bits[b >> 5] >> (b & 31)) & 1 ? -1 : 1;
            }
            c16_t *rxComp[1] = {comp}, *mas[1] = {ma}, *mbs[1] = {mb}, *mcs[1] = {mc};
            int16_t *ls[1] = {scratch};
            nr_channel_compensation(stride, stride, nb_rx, 1, rxc, chx, ma, mb, mc, rxComp, NULL, s->cpe, qm, 0, shift);
            nr_inner_rx(s->length,
                        stride,
                        nb_rx,
                        1,
                        qm,
                        rxc,
                        chx,
                        s->cpe,
                        rxComp,
                        mas,
                        mbs,
                        mcs,
                        NULL,
                        NULL,
                        shift,
                        false,
                        false,
                        0,
                        0.0f,
                        ls,
                        scr16,
                        ref + k * llr_max);
          }
          const nr_irx_params_t p = {.nb_layers = 1, .nb_rx = nb_rx, .mod_order = qm, .detector = 0, .output_shift = shift};
          const double t0 = now(), c0 = cpu_now();
          const int r = nr_irx_run(x, sym, 4, &p);
          const double tg = now() - t0, cg = cpu_now() - c0;
          nr_irx_release(x);
          long diff = 0;
          size_t nl = 0;
          for (int k = 0; k < 4 && r == 0; k++) {
            const int16_t *g = sym[k].llr_cw, *rf = ref + k * llr_max;
            for (uint32_t i = 0; i < sym[k].length * qm; i++)
              diff += g[i] != rf[i];
            nl += sym[k].length * qm;
          }
          printf("%s 1 layer Qm %d %d rx amp %5.0f: %s%ld/%zu LLRs differ | run %.0f us (CPU %.0f us)\n",
                 mode ? "in place" : "staging ",
                 qm,
                 nb_rx,
                 amp,
                 r ? "RUN FAILED " : "",
                 diff,
                 nl,
                 tg * 1e6,
                 cg * 1e6);
          fails += r != 0 || diff != 0;
        }
  free(chx);
  free(rxc);
  free(comp);
  free(ma);
  free(mb);
  free(mc);
  free(ref);
  free(scr16);
  free(scratch);
  return fails;
}

int main(void)
{
  const uint32_t stride = (NRE + 15) & ~15u;
  const int init = nr_irx_init(NRE, 4, 1);
  if (init == -1) {
    fprintf(stderr, "no OpenCL GPU for the module: skipped\n");
    return 77;
  }
  if (init != 0) {
    fprintf(stderr, "nr_irx_init failed (%d)\n", init);
    return 1;
  }
  const bool exact = nr_irx_cl_exact();
  const size_t llr_max = (size_t)NRE * 2 * 8;
  int16_t *cw = malloc(NSYM * llr_max * 2), *ref = malloc(NSYM * llr_max * 2), *scr16 = malloc(NSYM * llr_max * 2);
  c16_t *chf = malloc(2 * sizeof(c16_t) * 2 * 4 * NRE), *chx = malloc(sizeof(c16_t) * 2 * 4 * stride),
        *rxc = malloc(sizeof(c16_t) * 4 * stride);
  int8_t *sgn = malloc(NSYM * 2 * NRE);
  uint32_t *bits = malloc(NSYM * llr_max / 8 + 64);
  rs = 0x5c4;
  for (size_t i = 0; i < NSYM * llr_max / 32 + 16; i++)
    bits[i] = (uint32_t)(urand() * 4294967296.0);
  // in-place memory (rxdataF of 4 antennas, the codeword buffer) when the module has some
  c16_t *rxd[4] = {NULL};
  int16_t *cwd = nr_irx_alloc_host(NSYM * llr_max * 2);
  for (int a = 0; a < 4 && cwd; a++)
    rxd[a] = nr_irx_alloc_host(NSYM * FFT * sizeof(c16_t));
  const int nmode = cwd && rxd[3] ? 2 : 1;
  int fails = 0;
  for (int mode = 0; mode < nmode; mode++)
    for (int qm = 6; qm <= 8; qm += 2)
      for (int det = NR_IRX_DET_LBEST; det <= NR_IRX_DET_ML; det += 2)
        for (int nb_rx = 2; nb_rx <= 4; nb_rx += 2)
          for (double amp = 400; amp <= 1200; amp += 800) {
            const int shift = nb_rx == 2 ? 8 : 9, M = 1 << (qm / 2);
            const double sk = qm == 6 ? 6.48 : 13.04;
            const float lambda = 1.0f;
            rs = 0xFE16u + qm * 31 + det * 7 + nb_rx + (int)amp;
            nr_irx_ctx_t *x = nr_irx_acquire();
            if (!x) {
              fprintf(stderr, "acquire failed\n");
              return 2;
            }
            const void *rx_stage0 = nr_irx_staging(x, 0, NR_IRX_STAGING_RX);
            // two channel estimates [2][nb_rx][stride], over the allocation
            for (int c = 0; c < 2; c++) {
              c16_t *ch = nr_irx_staging(x, c, NR_IRX_STAGING_CH);
              for (int r = 0; r < 2 * nb_rx; r++)
                for (int i = 0; i < NRE; i++) {
                  const c16_t h = {(int16_t)lrint(gauss() * amp), (int16_t)lrint(gauss() * amp)};
                  ch[(size_t)r * stride + i] = chf[((size_t)c * 2 * 4 + r) * NRE + i] = h;
                }
            }
            nr_irx_sym_t sym[NSYM];
            const c16_t *rx_ref[NSYM]; // the extracted RX of each symbol (in the staging), the CPU reference's input
            uint32_t bit = 0;
            for (int k = 0; k < NSYM; k++) {
              const int c = k < NSYM / 2 ? 0 : 1, skip = k % 3 == 1 ? 25 : 0; // every 25th RE out (PTRS) on a third
              uint16_t *map = nr_irx_staging(x, k, NR_IRX_STAGING_REMAP);
              uint32_t len = 0;
              for (int i = 0; i < NRE; i++)
                if (!skip || i % skip != 3)
                  map[len++] = i;
              c16_t *rx = nr_irx_staging(x, k, NR_IRX_STAGING_RX);
              rx_ref[k] = rx;
              for (uint32_t i = 0; i < len; i++) {
                double xs[2][2];
                for (int l = 0; l < 2; l++) {
                  xs[l][0] = (2 * (int)(urand() * M) - M + 1) / sk;
                  xs[l][1] = (2 * (int)(urand() * M) - M + 1) / sk;
                  sgn[(k * NRE + i) * 2 + l] = xs[l][0] > 0 ? 1 : -1;
                }
                for (int a = 0; a < nb_rx; a++) {
                  double yr = gauss() * amp * 0.05, yi = gauss() * amp * 0.05;
                  for (int l = 0; l < 2; l++) {
                    const c16_t h = chf[((size_t)c * 2 * 4 + l * nb_rx + a) * NRE + map[i]];
                    yr += h.r * xs[l][0] - h.i * xs[l][1];
                    yi += h.r * xs[l][1] + h.i * xs[l][0];
                  }
                  rx[(size_t)a * stride + i] =
                      (c16_t){(int16_t)lrint(fmax(-32767, fmin(32767, yr))), (int16_t)lrint(fmax(-32767, fmin(32767, yi)))};
                }
              }
              if (mode) // the extracted RX scattered back to its allocation REs, read in place
                for (int a = 0; a < nb_rx; a++)
                  for (uint32_t i = 0; i < len; i++)
                    rxd[a][k * FFT + map[i]] = rx[(size_t)a * stride + i];
              sym[k] = (nr_irx_sym_t){.rxF = rx,
                                      .chF = nr_irx_staging(x, c, NR_IRX_STAGING_CH),
                                      .re_map = skip ? map : NULL,
                                      .scramble = nr_irx_staging(x, 0, NR_IRX_STAGING_SCRAMBLE),
                                      .scramble_offset = bit,
                                      .llr_cw = (mode ? cwd : cw) + k * llr_max,
                                      .length = len,
                                      .stride = stride,
                                      .ch_stride = stride,
                                      .cpe = k == 4 ? (c16_t){32610, -3212} : (c16_t){32767, 0}};
              if (mode) {
                sym[k].rxF = NULL;
                for (int a = 0; a < nb_rx; a++)
                  sym[k].rx_ant[a] = rxd[a] + k * FFT;
                if (!skip) { // the identity map: the allocation is the first len REs
                  for (int a = 0; a < nb_rx; a++)
                    for (uint32_t i = 0; i < len; i++)
                      rxd[a][k * FFT + i] = rx[(size_t)a * stride + i];
                }
              }
              bit += len * 2 * qm;
            }
            memcpy(nr_irx_staging(x, 0, NR_IRX_STAGING_SCRAMBLE), bits, (bit + 31) / 32 * 4);
            // CPU reference, symbol by symbol (channel estimates gathered through the RE map)
            double tc = now();
            for (int k = 0; k < NSYM; k++) {
              const nr_irx_sym_t *s = &sym[k];
              const int c = k < NSYM / 2 ? 0 : 1;
              for (int r = 0; r < 2 * nb_rx; r++)
                for (uint32_t i = 0; i < s->length; i++)
                  chx[(size_t)r * stride + i] = chf[((size_t)c * 2 * 4 + r) * NRE + (s->re_map ? s->re_map[i] : i)];
              for (int a = 0; a < nb_rx; a++)
                memcpy(rxc + (size_t)a * stride, rx_ref[k] + (size_t)a * stride, s->length * sizeof(c16_t));
              for (uint32_t i = 0; i < s->length * 2 * qm; i++) {
                const uint32_t b = s->scramble_offset + i;
                scr16[i] = (bits[b >> 5] >> (b & 31)) & 1 ? -1 : 1;
              }
              nr_inner_rx_2layer_f16(s->length,
                                     stride,
                                     nb_rx,
                                     rxc,
                                     chx,
                                     s->cpe,
                                     qm,
                                     det == NR_IRX_DET_LBEST,
                                     lambda,
                                     shift,
                                     NULL,
                                     NULL,
                                     ref + k * llr_max,
                                     scr16);
            }
            tc = now() - tc;
            const nr_irx_params_t p =
                {.nb_layers = 2, .nb_rx = nb_rx, .mod_order = qm, .detector = det, .output_shift = shift, .lambda = lambda};
            double tg = 1e30, cg = 0;
            for (int r = 0; r < 4; r++) { // the first run warms up; the staging is reused (run maps it again)
              const double t0 = now(), c0 = cpu_now();
              if (nr_irx_run(x, sym, NSYM, &p) != 0) {
                fprintf(stderr, "run failed\n");
                return 2;
              }
              if (r && now() - t0 < tg) {
                tg = now() - t0;
                cg = cpu_now() - c0;
              }
              if (nr_irx_staging(x, 0, NR_IRX_STAGING_RX) != rx_stage0) {
                fprintf(stderr, "the staging moved when mapped again\n");
                return 2;
              }
            }
            nr_irx_release(x);
            long diff = 0, flips = 0, maxd = 0, err_cpu = 0, err_gpu = 0;
            size_t nl = 0;
            for (int k = 0; k < NSYM; k++) {
              const int16_t *g = (mode ? cwd : cw) + k * llr_max, *r = ref + k * llr_max;
              const uint32_t n = sym[k].length * 2 * qm;
              nl += n;
              for (uint32_t i = 0; i < n; i++)
                if (g[i] != r[i]) {
                  diff++;
                  maxd = labs((long)g[i] - r[i]) > maxd ? labs((long)g[i] - r[i]) : maxd;
                  flips += (long)g[i] * r[i] < 0;
                }
              for (uint32_t i = 0; i < sym[k].length; i++)
                for (int l = 0; l < 2; l++) { // first bit of layer l, descrambled back
                  const uint32_t o = i * 2 * qm + l * qm, b = sym[k].scramble_offset + o;
                  const int s = (bits[b >> 5] >> (b & 31)) & 1 ? -1 : 1;
                  err_cpu += (r[o] * s > 0 ? 1 : -1) != sgn[(k * NRE + i) * 2 + l];
                  err_gpu += (g[o] * s > 0 ? 1 : -1) != sgn[(k * NRE + i) * 2 + l];
                }
            }
            printf(
                "%s Qm %d %-5s %d rx amp %4.0f: %ld/%zu LLRs differ (max %ld, %ld sign flips), MSB errors cpu %ld gpu %ld | run "
                "%.0f us (CPU %.0f us), cpu 1 thread %.0f us\n",
                mode ? "in place" : "staging ",
                qm,
                det == NR_IRX_DET_LBEST ? "lbest" : "ml",
                nb_rx,
                amp,
                diff,
                nl,
                maxd,
                flips,
                err_cpu,
                err_gpu,
                tg * 1e6,
                cg * 1e6,
                tc * 1e6);
            fails += exact ? diff != 0 : err_gpu > err_cpu + err_cpu / 50 + 5;
          }
  const int fails1 = test_1layer(bits, rxd, cw, cwd, nmode);
  nr_irx_shutdown();
  for (int a = 0; a < 4; a++)
    if (rxd[a])
      nr_irx_free_host(rxd[a]);
  if (cwd)
    nr_irx_free_host(cwd);
  printf("2 layers: %s\n", fails ? "FAILED" : exact ? "PASSED: bit-exact" : "PASSED: decisions of the CPU receiver");
  printf("1 layer: %s\n", fails1 ? "FAILED" : "PASSED: bit-exact");
  return fails + fails1 != 0;
}
