/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV port harness -- kernel #4: c16multaddVectRealComplex (PHY/TOOLS/tools_defs.h).
 *
 * The single most-used vectorized helper in NR channel estimation
 * (nr_ul_channel_estimation / nr_dl_channel_estimation). For N points:
 *     y[j].r += sat( 2 * mulhrs(alpha.r, x[j]) )
 *     y[j].i += sat( 2 * mulhrs(alpha.i, x[j]) )
 * where x is a real int16 vector, alpha a complex scalar, y complex, and
 * mulhrs(a,b) = round(a*b / 2^15) truncated to int16 (simde_mm256_mulhrs_epi16).
 * The "2*" is OAI's normalization quirk (see the doubling in the x86 code).
 *
 * scalar / simde(OAI x86 path) / rvv checked BYTE-FOR-BYTE and benchmarked.
 * y is an accumulator, so all three start from the same y buffer.
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
static inline int16_t mulhrs(int16_t a, int16_t b)
{
  int32_t p = (int32_t)a * (int32_t)b;
  return (int16_t)((((p >> 14) + 1) >> 1) & 0xFFFF); /* round to Q15, wrap to int16 */
}
static void c16madd_scalar(const int16_t *x, const c16_t *alpha, c16_t *y, int N)
{
  for (int j = 0; j < N; j++) {
    int16_t pr = mulhrs(alpha->r, x[j]);
    int16_t pi = mulhrs(alpha->i, x[j]);
    y[j].r = sadd16(y[j].r, sadd16(pr, pr));
    y[j].i = sadd16(y[j].i, sadd16(pi, pi));
  }
}

/* ---- SIMDe baseline: OAI's exact x86 body (N % 8 == 0) --------------------- */
static void c16madd_simde(const int16_t *x, const c16_t *alpha, c16_t *y, int N)
{
  const int8_t makePairs[32] __attribute__((aligned(32))) = {0,  1,  0 + 16, 1 + 16, 2,  3,  2 + 16, 3 + 16,
                                                             4,  5,  4 + 16, 5 + 16, 6,  7,  6 + 16, 7 + 16,
                                                             8,  9,  8 + 16, 9 + 16, 10, 11, 10 + 16, 11 + 16,
                                                             12, 13, 12 + 16, 13 + 16, 14, 15, 14 + 16, 15 + 16};
  simde__m256i alpha256 = simde_mm256_set1_epi32(*(int32_t *)alpha);
  simde__m128i *x128 = (simde__m128i *)x;
  simde__m128i *y128 = (simde__m128i *)y;
  for (int i = 0; i < N / 8; i++) {
    const simde__m256i xduplicate = simde_mm256_broadcastsi128_si256(*x128);
    const simde__m256i x_ord = simde_mm256_shuffle_epi8(xduplicate, *(simde__m256i *)makePairs);
    const simde__m256i xa = simde_mm256_mulhrs_epi16(alpha256, x_ord);
    const simde__m256i xa2 = simde_mm256_adds_epi16(xa, xa);
    *y128 = simde_mm_adds_epi16(simde_mm256_extracti128_si256(xa2, 0), *y128);
    y128++;
    *y128 = simde_mm_adds_epi16(simde_mm256_extracti128_si256(xa2, 1), *y128);
    y128++;
    x128++;
  }
}

/* ---- RVV ------------------------------------------------------------------- */
#if defined(__riscv) && defined(__riscv_vector)
#include <riscv_vector.h>
static inline vint16m1_t rvv_mulhrs(vint16m1_t X, int16_t s, size_t vl)
{
  vint32m2_t p = __riscv_vwmul_vx_i32m2(X, s, vl);
  p = __riscv_vsra_vx_i32m2(__riscv_vadd_vx_i32m2(__riscv_vsra_vx_i32m2(p, 14, vl), 1, vl), 1, vl);
  return __riscv_vnsra_wx_i16m1(p, 0, vl); /* truncating narrow, wrap (mulhrs) */
}
static void c16madd_rvv(const int16_t *x, const c16_t *alpha, c16_t *y, int N)
{
  const int16_t ar = alpha->r, ai = alpha->i;
  int16_t *yp = &y[0].r;
  for (int j = 0; j < N;) {
    size_t vl = __riscv_vsetvl_e16m1((size_t)(N - j));
    vint16m1_t X = __riscv_vle16_v_i16m1(x + j, vl);
    vint16m1_t pr = __riscv_vsadd_vv_i16m1(rvv_mulhrs(X, ar, vl), rvv_mulhrs(X, ar, vl), vl);
    vint16m1_t pi = __riscv_vsadd_vv_i16m1(rvv_mulhrs(X, ai, vl), rvv_mulhrs(X, ai, vl), vl);
    vint16m1x2_t Y = __riscv_vlseg2e16_v_i16m1x2(yp + 2 * j, vl);
    vint16m1_t Yr = __riscv_vsadd_vv_i16m1(__riscv_vget_v_i16m1x2_i16m1(Y, 0), pr, vl);
    vint16m1_t Yi = __riscv_vsadd_vv_i16m1(__riscv_vget_v_i16m1x2_i16m1(Y, 1), pi, vl);
    __riscv_vsseg2e16_v_i16m1x2(yp + 2 * j, __riscv_vcreate_v_i16m1x2(Yr, Yi), vl);
    j += vl;
  }
}
#endif

/* ---- driver ---------------------------------------------------------------- */
#define N 4080 /* multiple of 8; not a power of two -> exercises RVV tail vl */
static uint64_t rng = 0xda3e39cb94b95bdbULL;
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

int main(int argc, char **argv)
{
  static int16_t x[N];
  static c16_t y0[N], ys[N], yv[N], yq[N];
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

  for (int t = 0; t < 4; t++) {
    c16_t alpha = {rnd16(), rnd16()};
    for (int k = 0; k < N; k++) {
      x[k] = rnd16();
      y0[k] = (c16_t){rnd16(), rnd16()};
    }
    x[0] = -32768;
    alpha.r = -32768;
    alpha.i = 32767; /* mulhrs / saturation edges */

    memcpy(ys, y0, sizeof(y0));
    c16madd_scalar(x, &alpha, ys, N);
    memcpy(yv, y0, sizeof(y0));
    c16madd_simde(x, &alpha, yv, N);
    int bad = cmp(ys, yv, N, "simde");
#if defined(__riscv) && defined(__riscv_vector)
    memcpy(yq, y0, sizeof(y0));
    c16madd_rvv(x, &alpha, yq, N);
    bad |= cmp(ys, yq, N, "rvv");
#else
    (void)yq;
#endif
    printf("  trial %d: %s\n", t, bad ? "FAIL" : "byte-exact OK (simde + rvv vs scalar)");
    fails += bad;
  }

  c16_t alpha = {12345, -9876};
  const int iters = 20000;
  double t0 = now_ns();
  for (int it = 0; it < iters; it++) {
    memcpy(ys, y0, sizeof(y0));
    c16madd_scalar(x, &alpha, ys, N);
  }
  double ts = now_ns() - t0;
  t0 = now_ns();
  for (int it = 0; it < iters; it++) {
    memcpy(yv, y0, sizeof(y0));
    c16madd_simde(x, &alpha, yv, N);
  }
  double tsi = now_ns() - t0;
  printf("\nscalar     : %.2f ns/pt\n", ts / ((double)iters * N));
  printf("simde(OAI) : %.2f ns/pt  (%.2fx vs scalar)\n", tsi / ((double)iters * N), ts / tsi);
#if defined(__riscv) && defined(__riscv_vector)
  t0 = now_ns();
  for (int it = 0; it < iters; it++) {
    memcpy(yq, y0, sizeof(y0));
    c16madd_rvv(x, &alpha, yq, N);
  }
  double tv = now_ns() - t0;
  printf("rvv        : %.2f ns/pt  (%.2fx vs scalar, %.2fx vs simde/OAI)\n",
         tv / ((double)iters * N),
         ts / tv,
         tsi / tv);
#endif

  printf("\nRESULT: %s\n", fails ? "FAIL" : "PASS");
  return fails ? 1 : 0;
}
