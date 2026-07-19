/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV port bring-up harness (phase 0).
 *
 * Self-contained, dependency-free micro-harness that validates a hand-written
 * RVV kernel BYTE-FOR-BYTE against a plain-C scalar reference and benchmarks
 * both. Cross-compile with build.sh and run the binary ON the RISC-V target
 * (there is no qemu in the build environment).
 *
 * The demonstrator kernel is a Q15 saturating complex multiply on interleaved
 * int16 (c16_t) arrays -- the core primitive behind oai_mm_cpx_mult() and the
 * c16_t helpers in PHY/TOOLS/tools_defs.h. It exercises the exact RVV toolkit
 * the rest of the complex-int16 datapath needs: strided load/store, widening
 * 16x16->32 multiply, 32-bit add/sub, arithmetic shift, saturating narrow.
 *
 * Copy this pattern per kernel as the port proceeds: (1) a scalar reference
 * that is the ground truth, (2) an RVV implementation, (3) byte-exact compare
 * over random + adversarial inputs, (4) a cycle/throughput benchmark.
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>

/* This SoC is heterogeneous: cores 0-7 are VLEN=256, cores 8-15 are VLEN=1024.
 * To run on a VLEN=1024 core you must (1) register the PID as an "AI thread" via
 * /proc/set_ai_thread, then (2) pin to that core. Pass the target core as argv[1]
 * (default: current core). See also rvv_microbench/rvv_width_bench.c. */
static int enable_ai_thread(void)
{
  FILE *fp = fopen("/proc/set_ai_thread", "w");
  if (!fp)
    return -1;
  int rc = fprintf(fp, "%ld\n", (long)getpid());
  int crc = fclose(fp);
  return (rc < 0 || crc != 0) ? -1 : 0;
}

static int pin_to_cpu(int cpu)
{
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  return sched_setaffinity(0, sizeof(set), &set);
}

typedef struct {
  int16_t r;
  int16_t i;
} c16_t;

/* ---- ground-truth semantics -------------------------------------------------
 * out = (z1 * z2) >> shift, per complex element, matching the x86/SSE path:
 *   re = (z1.r*z2.r - z1.i*z2.i)          (32-bit, WRAPS mod 2^32 like madd)
 *   im = (z1.r*z2.i + z1.i*z2.r)
 *   re >>= shift; im >>= shift            (arithmetic shift, truncating)
 *   out.r = sat_int16(re); out.i = sat_int16(im)   (signed saturation, like packs)
 *
 * The add/sub are done through uint32_t so the one pathological overflow case
 * (e.g. -32768*-32768*2) wraps in a defined way and matches the hardware; plain
 * signed overflow would be UB. This wrap-then-saturate detail is precisely the
 * kind of thing the byte-exact check exists to police.
 */
static inline int16_t sat_int16(int32_t v)
{
  if (v > 32767)
    return 32767;
  if (v < -32768)
    return -32768;
  return (int16_t)v;
}

static inline int32_t wadd(int32_t a, int32_t b)
{
  return (int32_t)((uint32_t)a + (uint32_t)b);
}

static inline int32_t wsub(int32_t a, int32_t b)
{
  return (int32_t)((uint32_t)a - (uint32_t)b);
}

static void cpx_mult_shift_scalar(const c16_t *z1, const c16_t *z2, c16_t *out, size_t n, int shift)
{
  for (size_t k = 0; k < n; k++) {
    int32_t ac = (int32_t)z1[k].r * (int32_t)z2[k].r;
    int32_t bd = (int32_t)z1[k].i * (int32_t)z2[k].i;
    int32_t ad = (int32_t)z1[k].r * (int32_t)z2[k].i;
    int32_t bc = (int32_t)z1[k].i * (int32_t)z2[k].r;
    int32_t re = wsub(ac, bd) >> shift;
    int32_t im = wadd(ad, bc) >> shift;
    out[k].r = sat_int16(re);
    out[k].i = sat_int16(im);
  }
}

/* ---- RVV implementation ---------------------------------------------------- */
#if defined(__riscv) && defined(__riscv_vector)
#include <riscv_vector.h>

static void cpx_mult_shift_rvv(const c16_t *z1, const c16_t *z2, c16_t *out, size_t n, int shift)
{
  const ptrdiff_t stride = sizeof(c16_t); /* bytes between successive .r (and .i) */
  const int16_t *z1r = &z1[0].r, *z1i = &z1[0].i;
  const int16_t *z2r = &z2[0].r, *z2i = &z2[0].i;
  int16_t *or = &out[0].r, *oi = &out[0].i;

  for (size_t k = 0; k < n;) {
    size_t vl = __riscv_vsetvl_e16m1(n - k);

    /* Strided loads deinterleave re/im. vlseg2e16 is the faster option to
     * benchmark later; strided is used here for API stability and clarity. */
    vint16m1_t a = __riscv_vlse16_v_i16m1(z1r + 2 * k, stride, vl); /* z1.r */
    vint16m1_t b = __riscv_vlse16_v_i16m1(z1i + 2 * k, stride, vl); /* z1.i */
    vint16m1_t c = __riscv_vlse16_v_i16m1(z2r + 2 * k, stride, vl); /* z2.r */
    vint16m1_t d = __riscv_vlse16_v_i16m1(z2i + 2 * k, stride, vl); /* z2.i */

    /* Exact 16x16->32 products (each fits in int32; no loss here). */
    vint32m2_t ac = __riscv_vwmul_vv_i32m2(a, c, vl);
    vint32m2_t bd = __riscv_vwmul_vv_i32m2(b, d, vl);
    vint32m2_t ad = __riscv_vwmul_vv_i32m2(a, d, vl);
    vint32m2_t bc = __riscv_vwmul_vv_i32m2(b, c, vl);

    /* 32-bit add/sub WRAP (mod 2^32), matching x86 madd. */
    vint32m2_t re = __riscv_vsub_vv_i32m2(ac, bd, vl);
    vint32m2_t im = __riscv_vadd_vv_i32m2(ad, bc, vl);

    /* Arithmetic (truncating) shift -- matches the reference `>> shift`.
     * Doing the shift here rather than inside vnclip keeps the result exact
     * regardless of the vnclip rounding mode. */
    re = __riscv_vsra_vx_i32m2(re, (size_t)shift, vl);
    im = __riscv_vsra_vx_i32m2(im, (size_t)shift, vl);

    /* Saturating narrow 32->16 (shift 0 => pure signed saturation, like packs). */
    vint16m1_t re16 = __riscv_vnclip_wx_i16m1(re, 0, __RISCV_VXRM_RDN, vl);
    vint16m1_t im16 = __riscv_vnclip_wx_i16m1(im, 0, __RISCV_VXRM_RDN, vl);

    __riscv_vsse16_v_i16m1(or + 2 * k, stride, re16, vl);
    __riscv_vsse16_v_i16m1(oi + 2 * k, stride, im16, vl);

    k += vl;
  }
}

/* Same math, but unit-stride SEGMENT load/store (vlseg2/vsseg2) instead of
 * strided access. Segment ops are the natural fit for interleaved complex data
 * and are usually far cheaper than strided access on real hardware. */
static void cpx_mult_shift_rvv_seg(const c16_t *z1, const c16_t *z2, c16_t *out, size_t n, int shift)
{
  const int16_t *p1 = &z1[0].r, *p2 = &z2[0].r;
  int16_t *po = &out[0].r;

  for (size_t k = 0; k < n;) {
    size_t vl = __riscv_vsetvl_e16m1(n - k);

    vint16m1x2_t s1 = __riscv_vlseg2e16_v_i16m1x2(p1 + 2 * k, vl);
    vint16m1x2_t s2 = __riscv_vlseg2e16_v_i16m1x2(p2 + 2 * k, vl);
    vint16m1_t a = __riscv_vget_v_i16m1x2_i16m1(s1, 0); /* z1.r */
    vint16m1_t b = __riscv_vget_v_i16m1x2_i16m1(s1, 1); /* z1.i */
    vint16m1_t c = __riscv_vget_v_i16m1x2_i16m1(s2, 0); /* z2.r */
    vint16m1_t d = __riscv_vget_v_i16m1x2_i16m1(s2, 1); /* z2.i */

    vint32m2_t re = __riscv_vsub_vv_i32m2(__riscv_vwmul_vv_i32m2(a, c, vl), __riscv_vwmul_vv_i32m2(b, d, vl), vl);
    vint32m2_t im = __riscv_vadd_vv_i32m2(__riscv_vwmul_vv_i32m2(a, d, vl), __riscv_vwmul_vv_i32m2(b, c, vl), vl);
    re = __riscv_vsra_vx_i32m2(re, (size_t)shift, vl);
    im = __riscv_vsra_vx_i32m2(im, (size_t)shift, vl);
    vint16m1_t re16 = __riscv_vnclip_wx_i16m1(re, 0, __RISCV_VXRM_RDN, vl);
    vint16m1_t im16 = __riscv_vnclip_wx_i16m1(im, 0, __RISCV_VXRM_RDN, vl);

    vint16m1x2_t so = __riscv_vcreate_v_i16m1x2(re16, im16);
    __riscv_vsseg2e16_v_i16m1x2(po + 2 * k, so, vl);

    k += vl;
  }
}
#endif /* __riscv_vector */

/* ---- timing (portable; no rdcycle SIGILL risk in user space) --------------- */
static double now_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

/* ---- test driver ----------------------------------------------------------- */
#define N 4096

static uint64_t rng = 0x1234567890abcdefULL;
static int16_t rnd16(void)
{
  rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
  return (int16_t)(rng >> 33);
}

#if defined(__riscv) && defined(__riscv_vector)
static int compare(const c16_t *a, const c16_t *b, size_t n, const char *tag)
{
  if (memcmp(a, b, n * sizeof(c16_t)) == 0)
    return 0;
  for (size_t k = 0; k < n; k++) {
    if (a[k].r != b[k].r || a[k].i != b[k].i) {
      fprintf(stderr, "  MISMATCH [%s] idx %zu: ref=(%d,%d) rvv=(%d,%d)\n", tag, k, a[k].r, a[k].i, b[k].r, b[k].i);
      return 1;
    }
  }
  return 1;
}
#endif /* __riscv_vector */

int main(int argc, char **argv)
{
  static c16_t z1[N], z2[N], ref[N], got[N];
  int fails = 0;

  if (argc > 1) {
    int cpu = atoi(argv[1]);
    if (cpu >= 8 && enable_ai_thread() != 0)
      perror("/proc/set_ai_thread (need VLEN=1024 core?)");
    if (pin_to_cpu(cpu) != 0)
      perror("sched_setaffinity");
    else
      printf("pinned to CPU %d\n", cpu);
  }

#if defined(__riscv) && defined(__riscv_vector)
  printf("RVV enabled: VLEN = %zu bits (vlenb = %zu bytes)\n", (size_t)__riscv_vlenb() * 8, (size_t)__riscv_vlenb());
#else
  printf("Built WITHOUT RVV (__riscv_vector undefined) -- scalar reference only, no comparison.\n");
#endif

  for (int shift = 0; shift <= 15; shift += 5) {
    /* random inputs */
    for (size_t k = 0; k < N; k++) {
      z1[k].r = rnd16();
      z1[k].i = rnd16();
      z2[k].r = rnd16();
      z2[k].i = rnd16();
    }
    /* adversarial edges in the first few elements (extremes + overflow case) */
    z1[0] = (c16_t){-32768, -32768};
    z2[0] = (c16_t){-32768, -32768};
    z1[1] = (c16_t){32767, 32767};
    z2[1] = (c16_t){32767, -32768};
    z1[2] = (c16_t){0, -32768};
    z2[2] = (c16_t){-32768, 0};

    cpx_mult_shift_scalar(z1, z2, ref, N, shift);

#if defined(__riscv) && defined(__riscv_vector)
    char tag[40];
    memset(got, 0, sizeof(got));
    cpx_mult_shift_rvv(z1, z2, got, N, shift);
    snprintf(tag, sizeof(tag), "strided shift=%d", shift);
    int bad = compare(ref, got, N, tag);

    memset(got, 0, sizeof(got));
    cpx_mult_shift_rvv_seg(z1, z2, got, N, shift);
    snprintf(tag, sizeof(tag), "segment shift=%d", shift);
    bad |= compare(ref, got, N, tag);

    if (bad) {
      fails++;
      printf("  shift=%2d: FAIL\n", shift);
    } else {
      printf("  shift=%2d: byte-exact OK (strided + segment)\n", shift);
    }
#else
    (void)got;
    printf("  shift=%2d: scalar ran (%d,%d)\n", shift, ref[0].r, ref[0].i);
#endif
  }

  /* benchmark (last shift value) */
  const int shift = 10;
  const int iters = 20000;
  double t0 = now_ns();
  for (int it = 0; it < iters; it++)
    cpx_mult_shift_scalar(z1, z2, ref, N, shift);
  double t_scalar = now_ns() - t0;
  printf("\nscalar : %.2f ns/elem\n", t_scalar / ((double)iters * N));

#if defined(__riscv) && defined(__riscv_vector)
  t0 = now_ns();
  for (int it = 0; it < iters; it++)
    cpx_mult_shift_rvv(z1, z2, got, N, shift);
  double t_rvv = now_ns() - t0;
  printf("rvv strided: %.2f ns/elem  (%.2fx vs scalar)\n", t_rvv / ((double)iters * N), t_scalar / t_rvv);

  t0 = now_ns();
  for (int it = 0; it < iters; it++)
    cpx_mult_shift_rvv_seg(z1, z2, got, N, shift);
  double t_seg = now_ns() - t0;
  printf("rvv segment: %.2f ns/elem  (%.2fx vs scalar)\n", t_seg / ((double)iters * N), t_scalar / t_seg);
#endif

  if (fails) {
    printf("\nRESULT: %d FAILURE(S)\n", fails);
    return 1;
  }
  printf("\nRESULT: PASS\n");
  return 0;
}
