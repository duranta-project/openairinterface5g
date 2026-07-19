/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV port harness -- kernel #10: DLSCH resource-mapping interleavers
 * (nr_dlsch.c interleave_with_0_signal_first / _start_with_0 / interleave_signals).
 * On RISC-V these run the plain scalar tail (no __AVX2__/USE128BIT) -> ~217 us/slot.
 *
 * Each scales input(s) by amp via mulhrs and interleaves at c16 (32-bit)
 * granularity into the grid:
 *   signal_first : out[2i]=mulhrs(dmrs[i],amp), out[2i+1]=0
 *   start_with_0 : out[2i]=0,                    out[2i+1]=mulhrs(dmrs[i],amp)
 *   signals      : out[2i]=mulhrs(sig2[i],amp2), out[2i+1]=mulhrs(sig1[i],amp)
 * RVV: vlseg2e16 load + mulhrs + vsseg4e16 (fields a.r,a.i,b.r,b.i -> [a_c16,b_c16]).
 * Reference/vector path rounds (mulhrs); the OAI scalar tail truncates
 * (c16mulRealShift) -- we match the vector/rounding path (as x86/ARM do).
 *
 * scalar(mulhrs) / simde(AVX2 path) / rvv checked BYTE-FOR-BYTE + benchmarked.
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <simde/x86/avx2.h>

typedef struct {
  int16_t r, i;
} c16_t;

static int pin_to_cpu(int cpu)
{
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  return sched_setaffinity(0, sizeof(set), &set);
}
static uint64_t rng = 0x14057b7ef767814fULL;
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
static inline int16_t mulhrs(int16_t a, int16_t b)
{
  int32_t p = (int32_t)a * (int32_t)b;
  return (int16_t)((((p >> 14) + 1) >> 1) & 0xFFFF);
}
static inline c16_t sc(c16_t a, int16_t amp)
{
  return (c16_t){mulhrs(a.r, amp), mulhrs(a.i, amp)};
}

/* ---- scalar ground truth (rounding, matches the vector path) --------------- */
static void first_scalar(c16_t *o, c16_t *d, int16_t amp, int sz)
{
  for (int i = 0; i < sz / 2; i++) {
    *o++ = sc(d[i], amp);
    *o++ = (c16_t){0, 0};
  }
}
static void start0_scalar(c16_t *o, c16_t *d, int16_t amp, int sz)
{
  for (int i = 0; i < sz / 2; i++) {
    *o++ = (c16_t){0, 0};
    *o++ = sc(d[i], amp);
  }
}
static void sig_scalar(c16_t *o, c16_t *s1, int amp, c16_t *s2, int amp2, int sz)
{
  for (int i = 0; i < sz / 2; i++) {
    *o++ = sc(s2[i], amp2);
    *o++ = sc(s1[i], amp);
  }
}
static void noptrs_scalar(c16_t *o, c16_t *t, int amp, int sz)
{
  for (int i = 0; i < sz; i++)
    o[i] = sc(t[i], (int16_t)amp);
}

/* ---- SIMDe baseline: exact AVX2 paths -------------------------------------- */
static void first_simde(c16_t *output, c16_t *mod_dmrs, const int16_t amp_dmrs, int sz)
{
  c16_t *out = output;
  int i = 0, end = sz / 2;
  simde__m256i zeros256 = simde_mm256_setzero_si256(), amp256 = simde_mm256_set1_epi16(amp_dmrs);
  for (; i < (end & ~7); i += 8) {
    simde__m256i d0 = simde_mm256_mulhrs_epi16(simde_mm256_loadu_si256((simde__m256i *)(mod_dmrs + i)), amp256);
    simde__m256i d2 = simde_mm256_unpacklo_epi32(d0, zeros256);
    simde__m256i d3 = simde_mm256_unpackhi_epi32(d0, zeros256);
    simde_mm256_storeu_si256((simde__m256i *)out, simde_mm256_permute2x128_si256(d2, d3, 32));
    out += 8;
    simde_mm256_storeu_si256((simde__m256i *)out, simde_mm256_permute2x128_si256(d2, d3, 49));
    out += 8;
  }
  for (; i < end; i++) {
    *out++ = sc(mod_dmrs[i], amp_dmrs);
    *out++ = (c16_t){0, 0};
  }
}
static void sig_simde(c16_t *output, c16_t *signal1, const int amp, c16_t *signal2, const int amp2, int sz)
{
  c16_t *out = output;
  int i = 0, end = sz / 2;
  simde__m256i amp2256 = simde_mm256_set1_epi16(amp2), amp256 = simde_mm256_set1_epi16(amp);
  for (; i < (end & ~7); i += 8) {
    simde__m256i d0 = simde_mm256_mulhrs_epi16(simde_mm256_loadu_si256((simde__m256i *)(signal2 + i)), amp2256);
    simde__m256i d1 = simde_mm256_mulhrs_epi16(simde_mm256_loadu_si256((simde__m256i *)(signal1 + i)), amp256);
    simde__m256i d2 = simde_mm256_unpacklo_epi32(d0, d1);
    simde__m256i d3 = simde_mm256_unpackhi_epi32(d0, d1);
    simde_mm256_storeu_si256((simde__m256i *)out, simde_mm256_permute2x128_si256(d2, d3, 32));
    out += 8;
    simde_mm256_storeu_si256((simde__m256i *)out, simde_mm256_permute2x128_si256(d2, d3, 49));
    out += 8;
  }
  for (; i < end; i++) {
    *out++ = sc(signal2[i], amp2);
    *out++ = sc(signal1[i], amp);
  }
}

/* ---- RVV ------------------------------------------------------------------- */
#if defined(__riscv) && defined(__riscv_vector)
#include <riscv_vector.h>
static inline vint16m1_t rvv_mulhrs(vint16m1_t x, int16_t s, size_t vl)
{
  vint32m2_t p = __riscv_vwmul_vx_i32m2(x, s, vl);
  p = __riscv_vsra_vx_i32m2(__riscv_vadd_vx_i32m2(__riscv_vsra_vx_i32m2(p, 14, vl), 1, vl), 1, vl);
  return __riscv_vnsra_wx_i16m1(p, 0, vl);
}
/* store [a.r,a.i,b.r,b.i] per element -> interleaved c16 pairs */
static inline void seg4(int16_t *op, vint16m1_t ar, vint16m1_t ai, vint16m1_t br, vint16m1_t bi, size_t vl)
{
  __riscv_vsseg4e16_v_i16m1x4(op, __riscv_vcreate_v_i16m1x4(ar, ai, br, bi), vl);
}
static void first_rvv(c16_t *o, c16_t *d, int16_t amp, int sz)
{
  int end = sz / 2;
  int16_t *op = (int16_t *)o;
  for (int i = 0; i < end;) {
    size_t vl = __riscv_vsetvl_e16m1(end - i);
    vint16m1x2_t s = __riscv_vlseg2e16_v_i16m1x2((const int16_t *)&d[i], vl);
    vint16m1_t z = __riscv_vmv_v_x_i16m1(0, vl);
    seg4(op + 4 * i, rvv_mulhrs(__riscv_vget_v_i16m1x2_i16m1(s, 0), amp, vl),
         rvv_mulhrs(__riscv_vget_v_i16m1x2_i16m1(s, 1), amp, vl), z, z, vl);
    i += vl;
  }
}
static void start0_rvv(c16_t *o, c16_t *d, int16_t amp, int sz)
{
  int end = sz / 2;
  int16_t *op = (int16_t *)o;
  for (int i = 0; i < end;) {
    size_t vl = __riscv_vsetvl_e16m1(end - i);
    vint16m1x2_t s = __riscv_vlseg2e16_v_i16m1x2((const int16_t *)&d[i], vl);
    vint16m1_t z = __riscv_vmv_v_x_i16m1(0, vl);
    seg4(op + 4 * i, z, z, rvv_mulhrs(__riscv_vget_v_i16m1x2_i16m1(s, 0), amp, vl),
         rvv_mulhrs(__riscv_vget_v_i16m1x2_i16m1(s, 1), amp, vl), vl);
    i += vl;
  }
}
static void noptrs_rvv(c16_t *o, c16_t *t, int amp, int sz)
{
  int16_t *op = (int16_t *)o;
  const int16_t *ip = (const int16_t *)t;
  const int n = 2 * sz;
  for (int k = 0; k < n;) {
    size_t vl = __riscv_vsetvl_e16m1(n - k);
    __riscv_vse16_v_i16m1(op + k, rvv_mulhrs(__riscv_vle16_v_i16m1(ip + k, vl), (int16_t)amp, vl), vl);
    k += vl;
  }
}
static void sig_rvv(c16_t *o, c16_t *s1, int amp, c16_t *s2, int amp2, int sz)
{
  int end = sz / 2;
  int16_t *op = (int16_t *)o;
  for (int i = 0; i < end;) {
    size_t vl = __riscv_vsetvl_e16m1(end - i);
    vint16m1x2_t a = __riscv_vlseg2e16_v_i16m1x2((const int16_t *)&s2[i], vl);
    vint16m1x2_t b = __riscv_vlseg2e16_v_i16m1x2((const int16_t *)&s1[i], vl);
    seg4(op + 4 * i, rvv_mulhrs(__riscv_vget_v_i16m1x2_i16m1(a, 0), amp2, vl),
         rvv_mulhrs(__riscv_vget_v_i16m1x2_i16m1(a, 1), amp2, vl),
         rvv_mulhrs(__riscv_vget_v_i16m1x2_i16m1(b, 0), amp, vl),
         rvv_mulhrs(__riscv_vget_v_i16m1x2_i16m1(b, 1), amp, vl), vl);
    i += vl;
  }
}
#endif

/* ---- driver ---------------------------------------------------------------- */
#define SZ 3276 /* 273 RB * 12 sc */
static c16_t in1[SZ], in2[SZ], os[SZ * 2], ov[SZ * 2], oq[SZ * 2];

int main(int argc, char **argv)
{
  int fails = 0;
  if (argc > 1 && pin_to_cpu(atoi(argv[1])) == 0)
    printf("pinned to CPU %d\n", atoi(argv[1]));
#if defined(__riscv) && defined(__riscv_vector)
  printf("RVV: VLEN = %zu bits\n", (size_t)__riscv_vlenb() * 8);
#else
  printf("built WITHOUT RVV\n");
#endif
  for (int k = 0; k < SZ; k++) {
    in1[k] = (c16_t){rnd16(), rnd16()};
    in2[k] = (c16_t){rnd16(), rnd16()};
  }
  const int16_t amp = 12000, amp2 = 9000;

#define CK(name, scall, sicall, rvcall)                              \
  do {                                                               \
    memset(os, 0, sizeof(os));                                       \
    memset(ov, 0, sizeof(ov));                                       \
    memset(oq, 0, sizeof(oq));                                       \
    scall;                                                           \
    sicall;                                                          \
    int bad = memcmp(os, ov, sizeof(os)) != 0;                       \
    rvcall;                                                          \
    bad |= memcmp(os, oq, sizeof(os)) != 0;                          \
    printf("  %-12s: %s\n", name, bad ? "FAIL" : "byte-exact OK");   \
    fails += bad;                                                    \
  } while (0)

#if defined(__riscv) && defined(__riscv_vector)
  CK("signal_first", first_scalar(os, in1, amp, SZ), first_simde(ov, in1, amp, SZ), first_rvv(oq, in1, amp, SZ));
  CK("signals", sig_scalar(os, in1, amp, in2, amp2, SZ), sig_simde(ov, in1, amp, in2, amp2, SZ), sig_rvv(oq, in1, amp, in2, amp2, SZ));
  /* start_with_0: no dedicated simde ref -> compare rvv vs scalar directly */
  {
    memset(os, 0, sizeof(os));
    memset(oq, 0, sizeof(oq));
    start0_scalar(os, in1, amp, SZ);
    start0_rvv(oq, in1, amp, SZ);
    int bad = memcmp(os, oq, sizeof(os)) != 0;
    printf("  %-12s: %s\n", "start_with_0", bad ? "FAIL" : "byte-exact OK");
    fails += bad;
  }
  {
    memset(os, 0, sizeof(os));
    memset(oq, 0, sizeof(oq));
    noptrs_scalar(os, in1, amp, SZ);
    noptrs_rvv(oq, in1, amp, SZ);
    int bad = memcmp(os, oq, SZ * sizeof(c16_t)) != 0;
    printf("  %-12s: %s\n", "no_ptrs_data", bad ? "FAIL" : "byte-exact OK");
    fails += bad;
  }
#endif

  const int iters = 20000;
  double t0 = now_ns();
  for (int it = 0; it < iters; it++)
    sig_simde(ov, in1, amp, in2, amp2, SZ);
  double tsi = now_ns() - t0;
  printf("\nsignals simde(OAI AVX2) : %.2f us/call\n", tsi / iters / 1000.0);
#if defined(__riscv) && defined(__riscv_vector)
  t0 = now_ns();
  for (int it = 0; it < iters; it++)
    sig_rvv(oq, in1, amp, in2, amp2, SZ);
  double tv = now_ns() - t0;
  printf("signals rvv             : %.2f us/call  (%.1fx vs simde)\n", tv / iters / 1000.0, tsi / tv);
#endif
  printf("\nRESULT: %s\n", fails ? "FAIL" : "PASS");
  return fails ? 1 : 0;
}
