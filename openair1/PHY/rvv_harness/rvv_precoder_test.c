/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV port harness -- kernel #7: layer precoder (nr_layer_precoder_simd /
 * cmac_prec* in PHY/MODULATION/nr_modulation.c). Largest instrumented gNB-TX
 * cost (~318 us/slot in nr_dlsim). For one output antenna:
 *   y[i] = sat( sum_L (weights[L] * x[L][i]) >> 15 )   over n_layers layers
 * per complex element, matching cmac0_prec / cmac_prec:
 *   madd_r = x.r*w.r - x.i*w.i;  pr = (int16)(madd_r >> 15)           (low word)
 *   madd_i = x.r*w.i + x.i*w.r;  pi = (int16)((madd_i << 1) >> 16)    (high word)
 *   y += (pr, pi)  (saturating)
 * The srai/low-word (real) and slli-1/high-word (imag) forms are OAI's blend
 * trick; both equal madd>>15 in range but differ at overflow, so replicate
 * exactly for bit-exactness.
 *
 * scalar / simde(OAI cmac256 body) / rvv checked BYTE-FOR-BYTE + benchmarked.
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>
#include <simde/x86/avx2.h>

typedef struct {
  int16_t r;
  int16_t i;
} c16_t;

static int enable_ai_thread(void)
{
  FILE *fp = fopen("/proc/set_ai_thread", "w");
  if (!fp)
    return -1;
  int rc = fprintf(fp, "%ld\n", (long)getpid());
  return (rc < 0 || fclose(fp) != 0) ? -1 : 0;
}
static int pin_to_cpu(int cpu)
{
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  return sched_setaffinity(0, sizeof(set), &set);
}

/* ---- scalar ground truth --------------------------------------------------- */
static inline int16_t sadd16(int16_t a, int16_t b)
{
  int32_t v = (int32_t)a + (int32_t)b;
  return v > 32767 ? 32767 : (v < -32768 ? -32768 : (int16_t)v);
}
static void precoder_scalar(int n_layers, const c16_t *const x[4], const c16_t w[4], c16_t *y, uint32_t N)
{
  for (uint32_t j = 0; j < N; j++) {
    int16_t yr = 0, yi = 0;
    for (int L = 0; L < n_layers; L++) {
      int32_t mr = (int32_t)x[L][j].r * w[L].r - (int32_t)x[L][j].i * w[L].i;
      int32_t mi = (int32_t)x[L][j].r * w[L].i + (int32_t)x[L][j].i * w[L].r;
      int16_t pr = (int16_t)(mr >> 15);
      int16_t pi = (int16_t)(((uint32_t)mi << 1) >> 16);
      yr = sadd16(yr, pr);
      yi = sadd16(yi, pi);
    }
    y[j] = (c16_t){yr, yi};
  }
}

/* ---- SIMDe baseline: OAI cmac256 (N % 8 == 0) ------------------------------ */
static inline int32_t c16toI32(c16_t a)
{
  int32_t v;
  memcpy(&v, &a, 4);
  return v;
}
static inline simde__m256i cmac0_prec256(simde__m256i x, simde__m256i w_c, simde__m256i w_s)
{
  const simde__m256i reals = simde_mm256_srai_epi32(simde_mm256_madd_epi16(x, w_c), 15);
  const simde__m256i imags = simde_mm256_slli_epi32(simde_mm256_madd_epi16(x, w_s), 1);
  return simde_mm256_blend_epi16(reals, imags, 0xAA);
}
static void precoder_simde(int n_layers, const c16_t *const x[4], const c16_t w[4], c16_t *y, uint32_t N)
{
  simde__m256i w_c[4], w_s[4];
  for (int L = 0; L < n_layers; L++) {
    w_c[L] = simde_mm256_set1_epi32(c16toI32((c16_t){w[L].r, (int16_t)-w[L].i})); /* conj */
    w_s[L] = simde_mm256_set1_epi32(c16toI32((c16_t){w[L].i, w[L].r})); /* swap */
  }
  for (uint32_t j = 0; j < N; j += 8) {
    simde__m256i acc = cmac0_prec256(simde_mm256_loadu_si256((const simde__m256i *)&x[0][j]), w_c[0], w_s[0]);
    for (int L = 1; L < n_layers; L++) {
      const simde__m256i p = cmac0_prec256(simde_mm256_loadu_si256((const simde__m256i *)&x[L][j]), w_c[L], w_s[L]);
      acc = simde_mm256_adds_epi16(acc, p);
    }
    simde_mm256_storeu_si256((simde__m256i *)&y[j], acc);
  }
}

/* ---- RVV ------------------------------------------------------------------- */
#if defined(__riscv) && defined(__riscv_vector)
#include <riscv_vector.h>
static void precoder_rvv(int n_layers, const c16_t *const x[4], const c16_t w[4], c16_t *y, uint32_t N)
{
  int16_t *yp = &y[0].r;
  for (uint32_t j = 0; j < N;) {
    size_t vl = __riscv_vsetvl_e16m1(N - j);
    vint16m1_t yr = __riscv_vmv_v_x_i16m1(0, vl);
    vint16m1_t yi = __riscv_vmv_v_x_i16m1(0, vl);
    for (int L = 0; L < n_layers; L++) {
      const int16_t *xp = &x[L][0].r;
      vint16m1x2_t xs = __riscv_vlseg2e16_v_i16m1x2(xp + 2 * j, vl);
      vint16m1_t xr = __riscv_vget_v_i16m1x2_i16m1(xs, 0);
      vint16m1_t xi = __riscv_vget_v_i16m1x2_i16m1(xs, 1);
      const int16_t wr = w[L].r, wi = w[L].i;
      vint32m2_t mr = __riscv_vsub_vv_i32m2(__riscv_vwmul_vx_i32m2(xr, wr, vl), __riscv_vwmul_vx_i32m2(xi, wi, vl), vl);
      vint32m2_t mi = __riscv_vwmacc_vx_i32m2(__riscv_vwmul_vx_i32m2(xr, wi, vl), wr, xi, vl);
      vint16m1_t pr = __riscv_vnsra_wx_i16m1(mr, 15, vl); /* (mr>>15) low word */
      vint16m1_t pi = __riscv_vnsra_wx_i16m1(__riscv_vsll_vx_i32m2(mi, 1, vl), 16, vl); /* (mi<<1)>>16 high word */
      yr = __riscv_vsadd_vv_i16m1(yr, pr, vl);
      yi = __riscv_vsadd_vv_i16m1(yi, pi, vl);
    }
    __riscv_vsseg2e16_v_i16m1x2(yp + 2 * j, __riscv_vcreate_v_i16m1x2(yr, yi), vl);
    j += vl;
  }
}
#endif

/* ---- driver ---------------------------------------------------------------- */
#define N 4080
static uint64_t rng = 0x2545f4914f6cdd1dULL;
static int16_t rnd16(void)
{
  rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
  return (int16_t)(rng >> 33);
}
static double now_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}
static int cmp(const c16_t *a, const c16_t *b, int n, const char *tag)
{
  for (int k = 0; k < n; k++)
    if (a[k].r != b[k].r || a[k].i != b[k].i) {
      fprintf(stderr, "  MISMATCH [%s] idx %d: (%d,%d) vs (%d,%d)\n", tag, k, a[k].r, a[k].i, b[k].r, b[k].i);
      return 1;
    }
  return 0;
}

static c16_t X[4][N];
int main(int argc, char **argv)
{
  static c16_t ys[N], yv[N], yq[N];
  const c16_t *xp[4] = {X[0], X[1], X[2], X[3]};
  int fails = 0;

  if (argc > 1) {
    int cpu = atoi(argv[1]);
    if (cpu >= 8 && enable_ai_thread() != 0)
      perror("/proc/set_ai_thread");
    if (pin_to_cpu(cpu) == 0)
      printf("pinned to CPU %d\n", cpu);
  }
#if defined(__riscv) && defined(__riscv_vector)
  printf("RVV: VLEN = %zu bits\n", (size_t)__riscv_vlenb() * 8);
#else
  printf("built WITHOUT RVV\n");
#endif

  for (int nl = 1; nl <= 4; nl++) {
    c16_t w[4];
    for (int L = 0; L < 4; L++) {
      w[L] = (c16_t){rnd16(), rnd16()};
      for (int k = 0; k < N; k++)
        X[L][k] = (c16_t){rnd16(), rnd16()};
    }
    X[0][0] = (c16_t){-32768, -32768};
    w[0] = (c16_t){-32768, 32767};
    precoder_scalar(nl, xp, w, ys, N);
    precoder_simde(nl, xp, w, yv, N);
    int bad = cmp(ys, yv, N, "simde");
#if defined(__riscv) && defined(__riscv_vector)
    precoder_rvv(nl, xp, w, yq, N);
    bad |= cmp(ys, yq, N, "rvv");
#else
    (void)yq;
#endif
    printf("  n_layers=%d: %s\n", nl, bad ? "FAIL" : "byte-exact OK (simde + rvv vs scalar)");
    fails += bad;
  }

  c16_t w[4] = {{9000, -4000}, {-3000, 7000}, {5000, 5000}, {-8000, 1000}};
  const int iters = 20000, nl = 4;
  double t0 = now_ns();
  for (int it = 0; it < iters; it++)
    precoder_scalar(nl, xp, w, ys, N);
  double ts = now_ns() - t0;
  t0 = now_ns();
  for (int it = 0; it < iters; it++)
    precoder_simde(nl, xp, w, yv, N);
  double tsi = now_ns() - t0;
  printf("\nn_layers=4 scalar     : %.2f ns/RE\n", ts / ((double)iters * N));
  printf("n_layers=4 simde(OAI) : %.2f ns/RE  (%.2fx vs scalar)\n", tsi / ((double)iters * N), ts / tsi);
#if defined(__riscv) && defined(__riscv_vector)
  t0 = now_ns();
  for (int it = 0; it < iters; it++)
    precoder_rvv(nl, xp, w, yq, N);
  double tv = now_ns() - t0;
  printf("n_layers=4 rvv        : %.2f ns/RE  (%.2fx vs scalar, %.2fx vs simde/OAI)\n",
         tv / ((double)iters * N),
         ts / tv,
         tsi / tv);
#endif
  printf("\nRESULT: %s\n", fails ? "FAIL" : "PASS");
  return fails ? 1 : 0;
}
