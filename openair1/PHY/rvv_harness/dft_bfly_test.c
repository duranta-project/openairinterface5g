/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV port harness -- kernel #11: ibfly4_256, the radix-4 inverse butterfly
 * that every non-leaf idft/dft stage (idft256/1024/4096, dft*) is built from.
 *
 * idft4096 = transpose + 4x idft1024 + 128x ibfly4_256 + scale, and 4096 = 4^6
 * is pure radix-4, so this one butterfly (plus the transpose and the idft64
 * leaf, done separately) is the whole 4096-point transform. Porting it is the
 * first and hottest piece of un-deferring the DFT on RISC-V (the idft in
 * nr_est_delay dominates gNB-RX "Antenna Processing", ~1216us, 12x slower than
 * ARM because oai_dfts runs SIMDe-scalar).
 *
 * The butterfly, per 256-bit block, operates on 8 complex lanes:
 *   ak = xk * conj(twk)        (k=1,2,3; int32 re/im via madd_epi16)
 *   y0 = x0 + pack( a1     + a2     + a3    )
 *   y3 = x0 + pack( a1i-(a2r+a3i),  (a3r-a2i)-a1r )
 *   y2 = x0 + pack( (a2r-a3r)-a1r,  (a2i-a3i)-a1i )
 *   y1 = x0 + pack( (a3i-a2r)-a1i,  a1r-(a2i+a3r) )
 * where pack(re,im) = ( sat16(re>>15), sat16(im>>15) ) interleaved, and the
 * "+ x0" is a WRAPPING int16 add (simde_mm256_add_epi16). x0 is not twiddled.
 *
 * It is layout-agnostic: the same arithmetic hits every lane, so whether a
 * register holds 8 points of one DFT or 2x4 points of two packed DFTs (the AVX2
 * leaf trick) is irrelevant here. That matters only for the transpose/leaf.
 *
 * Three implementations are compared BYTE-FOR-BYTE and benchmarked:
 *   scalar : plain C ground truth
 *   simde  : the exact cmultc_256/cpack_256/ibfly4_256 sequence OAI ships today
 *   rvv    : hand-written RVV -- vlseg2e16 (re/im deinterleave for free),
 *            vwmul/vwmacc (int16->int32 conj-mul), vadd/vsub i32, then
 *            vnclip_wx(...,15,RDN) which is exactly srai_epi32(_,15)+packs_epi32,
 *            wrapping vadd i16 for the +x0, vsseg2e16 store.
 *
 * Bit-exactness notes:
 *  - madd/vwmacc accumulate two int16*int16 products in int32; the dy0..dy3
 *    int32 add/sub tree WRAPS at 32 bits (real twiddles/data never overflow,
 *    but random inputs here do -- both paths wrap identically).
 *  - pack's >>15 is arithmetic (truncating, floor); vnclip RDN adds no rounding
 *    increment, so it matches srai exactly, then saturates 32->16 like packs.
 *  - the +x0 wraps at int16 (add_epi16), NOT saturating.
 *
 * TOOLCHAIN BUG (spacemit riscv gcc): at -O2 on VLEN=1024 the instruction
 * scheduler miscompiles this kernel -> a vsseg2e16 writes at VLMAX and smashes
 * the stack (SIGSEGV, PC=0). Correct at -O0/-O1, and at -O2 on VLEN=256.
 * Workaround (keeps the rest of -O2): -fno-schedule-insns -fno-schedule-insns2.
 * The production build (CMake) applies these to oai_dfts_rvv.c. To run THIS
 * harness on an AI core (cpu>7): rebuild with those flags or use -O1.
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
#if defined(__riscv) && defined(__riscv_vector)
#include <riscv_vector.h>
#endif

/* ---- board helpers --------------------------------------------------------- */
static int use_ai_core(void)
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
static inline uint64_t now_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

/* ---- ground-truth scalar reference ----------------------------------------- */
static inline int16_t sat16(int32_t v)
{
  return v > 32767 ? 32767 : (v < -32768 ? -32768 : (int16_t)v);
}
static inline int32_t wadd(int32_t a, int32_t b)
{
  return (int32_t)((uint32_t)a + (uint32_t)b);
}
static inline int32_t wsub(int32_t a, int32_t b)
{
  return (int32_t)((uint32_t)a - (uint32_t)b);
}
/* pack one leg: (sat16(re>>15), sat16(im>>15)), then wrapping int16 add of x0 */
static inline void pack_addx0(int16_t x0r, int16_t x0i, int32_t re, int32_t im, int16_t *yr, int16_t *yi)
{
  *yr = (int16_t)(x0r + sat16((int32_t)(re >> 15)));
  *yi = (int16_t)(x0i + sat16((int32_t)(im >> 15)));
}
static void ibfly4_scalar(const int16_t *x0, const int16_t *x1, const int16_t *x2, const int16_t *x3,
                          int16_t *y0, int16_t *y1, int16_t *y2, int16_t *y3,
                          const int16_t *w1, const int16_t *w2, const int16_t *w3)
{
  for (int k = 0; k < 8; k++) {
    int16_t x0r = x0[2 * k], x0i = x0[2 * k + 1];
    /* ak = xk * conj(wk): re = xr*wr + xi*wi ; im = xi*wr + xr*neg16(wi).
     * The im term negates wk.im with WRAPPING int16 neg (sign_epi16): when
     * wk.im == -32768, -(-32768) wraps to -32768, not +32768. Real Q15 twiddles
     * hold -32768 (sin=-1), so this must match x86/simde exactly. */
    int16_t nw1i = (int16_t)(-w1[2 * k + 1]), nw2i = (int16_t)(-w2[2 * k + 1]), nw3i = (int16_t)(-w3[2 * k + 1]);
    int32_t a1r = wadd((int32_t)x1[2 * k] * w1[2 * k], (int32_t)x1[2 * k + 1] * w1[2 * k + 1]);
    int32_t a1i = wadd((int32_t)x1[2 * k + 1] * w1[2 * k], (int32_t)x1[2 * k] * nw1i);
    int32_t a2r = wadd((int32_t)x2[2 * k] * w2[2 * k], (int32_t)x2[2 * k + 1] * w2[2 * k + 1]);
    int32_t a2i = wadd((int32_t)x2[2 * k + 1] * w2[2 * k], (int32_t)x2[2 * k] * nw2i);
    int32_t a3r = wadd((int32_t)x3[2 * k] * w3[2 * k], (int32_t)x3[2 * k + 1] * w3[2 * k + 1]);
    int32_t a3i = wadd((int32_t)x3[2 * k + 1] * w3[2 * k], (int32_t)x3[2 * k] * nw3i);

    pack_addx0(x0r, x0i, wadd(a1r, wadd(a2r, a3r)), wadd(a1i, wadd(a2i, a3i)), &y0[2 * k], &y0[2 * k + 1]);
    pack_addx0(x0r, x0i, wsub(a1i, wadd(a2r, a3i)), wsub(wsub(a3r, a2i), a1r), &y3[2 * k], &y3[2 * k + 1]);
    pack_addx0(x0r, x0i, wsub(wsub(a2r, a3r), a1r), wsub(wsub(a2i, a3i), a1i), &y2[2 * k], &y2[2 * k + 1]);
    pack_addx0(x0r, x0i, wsub(wsub(a3i, a2r), a1i), wsub(a1r, wadd(a2i, a3r)), &y1[2 * k], &y1[2 * k + 1]);
  }
}

/* ---- SIMDe baseline: the exact sequence from oai_dfts.c --------------------- */
static const int16_t reflip[16] = {1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1};

static inline void cmultc_256(simde__m256i a, simde__m256i b, simde__m256i *re32, simde__m256i *im32)
{
  const simde__m256i perm_mask = simde_mm256_set_epi8(29, 28, 31, 30, 25, 24, 27, 26, 21, 20, 23, 22, 17, 16, 19, 18,
                                                      13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2);
  *re32 = simde_mm256_madd_epi16(a, b);
  simde__m256i mmtmpb = simde_mm256_sign_epi16(b, *(const simde__m256i *)reflip);
  mmtmpb = simde_mm256_shuffle_epi8(mmtmpb, perm_mask);
  *im32 = simde_mm256_madd_epi16(a, mmtmpb);
}
static inline simde__m256i cpack_256(simde__m256i xre, simde__m256i xim)
{
  simde__m256i t1 = simde_mm256_unpacklo_epi32(xre, xim);
  simde__m256i t2 = simde_mm256_unpackhi_epi32(xre, xim);
  return simde_mm256_packs_epi32(simde_mm256_srai_epi32(t1, 15), simde_mm256_srai_epi32(t2, 15));
}
static inline void ibfly4_256(simde__m256i *x0, simde__m256i *x1, simde__m256i *x2, simde__m256i *x3,
                              simde__m256i *y0, simde__m256i *y1, simde__m256i *y2, simde__m256i *y3,
                              simde__m256i *tw1, simde__m256i *tw2, simde__m256i *tw3)
{
  simde__m256i x1r_2, x1i_2, x2r_2, x2i_2, x3r_2, x3i_2, dy0r, dy0i, dy1r, dy1i, dy2r, dy2i, dy3r, dy3i;
  cmultc_256(*x1, *tw1, &x1r_2, &x1i_2);
  cmultc_256(*x2, *tw2, &x2r_2, &x2i_2);
  cmultc_256(*x3, *tw3, &x3r_2, &x3i_2);
  dy0r = simde_mm256_add_epi32(x1r_2, simde_mm256_add_epi32(x2r_2, x3r_2));
  dy0i = simde_mm256_add_epi32(x1i_2, simde_mm256_add_epi32(x2i_2, x3i_2));
  *y0 = simde_mm256_add_epi16(*x0, cpack_256(dy0r, dy0i));
  dy3r = simde_mm256_sub_epi32(x1i_2, simde_mm256_add_epi32(x2r_2, x3i_2));
  dy3i = simde_mm256_sub_epi32(simde_mm256_sub_epi32(x3r_2, x2i_2), x1r_2);
  *y3 = simde_mm256_add_epi16(*x0, cpack_256(dy3r, dy3i));
  dy2r = simde_mm256_sub_epi32(simde_mm256_sub_epi32(x2r_2, x3r_2), x1r_2);
  dy2i = simde_mm256_sub_epi32(simde_mm256_sub_epi32(x2i_2, x3i_2), x1i_2);
  *y2 = simde_mm256_add_epi16(*x0, cpack_256(dy2r, dy2i));
  dy1r = simde_mm256_sub_epi32(simde_mm256_sub_epi32(x3i_2, x2r_2), x1i_2);
  dy1i = simde_mm256_sub_epi32(x1r_2, simde_mm256_add_epi32(x2i_2, x3r_2));
  *y1 = simde_mm256_add_epi16(*x0, cpack_256(dy1r, dy1i));
}
static void ibfly4_simde(const int16_t *x0, const int16_t *x1, const int16_t *x2, const int16_t *x3,
                         int16_t *y0, int16_t *y1, int16_t *y2, int16_t *y3,
                         const int16_t *w1, const int16_t *w2, const int16_t *w3)
{
  simde__m256i vx0 = simde_mm256_loadu_si256((const simde__m256i *)x0);
  simde__m256i vx1 = simde_mm256_loadu_si256((const simde__m256i *)x1);
  simde__m256i vx2 = simde_mm256_loadu_si256((const simde__m256i *)x2);
  simde__m256i vx3 = simde_mm256_loadu_si256((const simde__m256i *)x3);
  simde__m256i vw1 = simde_mm256_loadu_si256((const simde__m256i *)w1);
  simde__m256i vw2 = simde_mm256_loadu_si256((const simde__m256i *)w2);
  simde__m256i vw3 = simde_mm256_loadu_si256((const simde__m256i *)w3);
  simde__m256i vy0, vy1, vy2, vy3;
  ibfly4_256(&vx0, &vx1, &vx2, &vx3, &vy0, &vy1, &vy2, &vy3, &vw1, &vw2, &vw3);
  simde_mm256_storeu_si256((simde__m256i *)y0, vy0);
  simde_mm256_storeu_si256((simde__m256i *)y1, vy1);
  simde_mm256_storeu_si256((simde__m256i *)y2, vy2);
  simde_mm256_storeu_si256((simde__m256i *)y3, vy3);
}

/* ---- RVV port -------------------------------------------------------------- */
#if defined(__riscv) && defined(__riscv_vector)
/* Radix-4 inverse butterfly over n complex lanes, processed full-width with a
 * vsetvl loop (NOT a fixed vl=8 per block). Full-width matters two ways:
 *  - it exploits any VLEN (K3 little 256b -> A100 1024b) with one kernel, and
 *  - it dodges a spacemit-GCC -O2 codegen bug: a fixed small AVL (vl=8) let a
 *    vsseg2e16 emit at VLMAX, stack-smashing on VLEN=1024. The chcomp/idft
 *    orchestration hands the butterfly contiguous runs (x1 = x0 + sub-DFT
 *    size, etc.), so one call covers a whole stage loop. */
static inline void ibfly4_rvv(const int16_t *x0, const int16_t *x1, const int16_t *x2, const int16_t *x3,
                              int16_t *y0, int16_t *y1, int16_t *y2, int16_t *y3,
                              const int16_t *w1, const int16_t *w2, const int16_t *w3, size_t n)
{
  for (size_t c = 0; c < n;) {
    size_t vl = __riscv_vsetvl_e16m1(n - c); /* complex lanes this iteration */
    size_t o = 2 * c;                        /* int16 offset (re,im interleaved) */
    vint16m1x2_t X0 = __riscv_vlseg2e16_v_i16m1x2(x0 + o, vl);
    vint16m1_t x0r = __riscv_vget_v_i16m1x2_i16m1(X0, 0);
    vint16m1_t x0i = __riscv_vget_v_i16m1x2_i16m1(X0, 1);

    vint16m1x2_t X1 = __riscv_vlseg2e16_v_i16m1x2(x1 + o, vl), W1 = __riscv_vlseg2e16_v_i16m1x2(w1 + o, vl);
    vint16m1x2_t X2 = __riscv_vlseg2e16_v_i16m1x2(x2 + o, vl), W2 = __riscv_vlseg2e16_v_i16m1x2(w2 + o, vl);
    vint16m1x2_t X3 = __riscv_vlseg2e16_v_i16m1x2(x3 + o, vl), W3 = __riscv_vlseg2e16_v_i16m1x2(w3 + o, vl);
    vint16m1_t x1r = __riscv_vget_v_i16m1x2_i16m1(X1, 0), x1i = __riscv_vget_v_i16m1x2_i16m1(X1, 1);
    vint16m1_t w1r = __riscv_vget_v_i16m1x2_i16m1(W1, 0), w1i = __riscv_vget_v_i16m1x2_i16m1(W1, 1);
    vint16m1_t x2r = __riscv_vget_v_i16m1x2_i16m1(X2, 0), x2i = __riscv_vget_v_i16m1x2_i16m1(X2, 1);
    vint16m1_t w2r = __riscv_vget_v_i16m1x2_i16m1(W2, 0), w2i = __riscv_vget_v_i16m1x2_i16m1(W2, 1);
    vint16m1_t x3r = __riscv_vget_v_i16m1x2_i16m1(X3, 0), x3i = __riscv_vget_v_i16m1x2_i16m1(X3, 1);
    vint16m1_t w3r = __riscv_vget_v_i16m1x2_i16m1(W3, 0), w3i = __riscv_vget_v_i16m1x2_i16m1(W3, 1);

    /* ak = xk * conj(wk): re = xr*wr + xi*wi ; im = xi*wr + xr*neg16(wi).
     * neg16 is WRAPPING (vneg: -(-32768)=-32768) to match simde sign_epi16. */
    vint16m1_t nw1i = __riscv_vneg_v_i16m1(w1i, vl);
    vint16m1_t nw2i = __riscv_vneg_v_i16m1(w2i, vl);
    vint16m1_t nw3i = __riscv_vneg_v_i16m1(w3i, vl);
    vint32m2_t a1r = __riscv_vwmacc_vv_i32m2(__riscv_vwmul_vv_i32m2(x1r, w1r, vl), x1i, w1i, vl);
    vint32m2_t a1i = __riscv_vwmacc_vv_i32m2(__riscv_vwmul_vv_i32m2(x1i, w1r, vl), x1r, nw1i, vl);
    vint32m2_t a2r = __riscv_vwmacc_vv_i32m2(__riscv_vwmul_vv_i32m2(x2r, w2r, vl), x2i, w2i, vl);
    vint32m2_t a2i = __riscv_vwmacc_vv_i32m2(__riscv_vwmul_vv_i32m2(x2i, w2r, vl), x2r, nw2i, vl);
    vint32m2_t a3r = __riscv_vwmacc_vv_i32m2(__riscv_vwmul_vv_i32m2(x3r, w3r, vl), x3i, w3i, vl);
    vint32m2_t a3i = __riscv_vwmacc_vv_i32m2(__riscv_vwmul_vv_i32m2(x3i, w3r, vl), x3r, nw3i, vl);

    /* compute + pack + store each leg in its own scope (low live-reg pressure) */
#define PACK(D) __riscv_vnclip_wx_i16m1((D), 15, __RISCV_VXRM_RDN, vl)
#define STORE_LEG(P, DR, DI) \
  __riscv_vsseg2e16_v_i16m1x2((P) + o, \
      __riscv_vcreate_v_i16m1x2(__riscv_vadd_vv_i16m1(x0r, PACK(DR), vl), \
                                __riscv_vadd_vv_i16m1(x0i, PACK(DI), vl)), vl)
    { vint32m2_t d0r = __riscv_vadd_vv_i32m2(a1r, __riscv_vadd_vv_i32m2(a2r, a3r, vl), vl);
      vint32m2_t d0i = __riscv_vadd_vv_i32m2(a1i, __riscv_vadd_vv_i32m2(a2i, a3i, vl), vl);
      STORE_LEG(y0, d0r, d0i); }
    { vint32m2_t d3r = __riscv_vsub_vv_i32m2(a1i, __riscv_vadd_vv_i32m2(a2r, a3i, vl), vl);
      vint32m2_t d3i = __riscv_vsub_vv_i32m2(__riscv_vsub_vv_i32m2(a3r, a2i, vl), a1r, vl);
      STORE_LEG(y3, d3r, d3i); }
    { vint32m2_t d2r = __riscv_vsub_vv_i32m2(__riscv_vsub_vv_i32m2(a2r, a3r, vl), a1r, vl);
      vint32m2_t d2i = __riscv_vsub_vv_i32m2(__riscv_vsub_vv_i32m2(a2i, a3i, vl), a1i, vl);
      STORE_LEG(y2, d2r, d2i); }
    { vint32m2_t d1r = __riscv_vsub_vv_i32m2(__riscv_vsub_vv_i32m2(a3i, a2r, vl), a1i, vl);
      vint32m2_t d1i = __riscv_vsub_vv_i32m2(a1r, __riscv_vadd_vv_i32m2(a2i, a3r, vl), vl);
      STORE_LEG(y1, d1r, d1i); }
#undef PACK
#undef STORE_LEG
    c += vl;
  }
}
#endif

/* ---- test driver ----------------------------------------------------------- */
static uint32_t rng = 0x1234567u;
static int16_t rnd16(void)
{
  rng = rng * 1103515245u + 12345u;
  return (int16_t)(rng >> 16);
}

int main(int argc, char **argv)
{
  int cpu = (argc > 1) ? atoi(argv[1]) : -1;
  if (cpu >= 0) {
    if (cpu > 7)
      use_ai_core();
    if (pin_to_cpu(cpu) != 0)
      fprintf(stderr, "warning: could not pin to cpu %d\n", cpu);
  }

  enum { NC = 64, NI = 2 * NC, NB = NC / 8 }; /* NC complex = NB blocks of 8 */
  enum { N = 30000 };
  /* correctness: N random NC-complex butterfly runs, three-way byte-exact.
   * scalar/simde process one 8-complex block at a time; rvv processes all NC
   * complex full-width in one call (vsetvl loop) -- exercising vl=VLMAX. */
  int16_t x0[NI], x1[NI], x2[NI], x3[NI], w1[NI], w2[NI], w3[NI];
  int16_t s0[NI], s1[NI], s2[NI], s3[NI]; /* scalar */
  int16_t m0[NI], m1[NI], m2[NI], m3[NI]; /* simde  */
  long mism_sm = 0, mism_rv = 0;
#if defined(__riscv) && defined(__riscv_vector)
  int16_t r0[NI], r1[NI], r2[NI], r3[NI]; /* rvv */
#endif
  for (int t = 0; t < N; t++) {
    for (int i = 0; i < NI; i++) {
      x0[i] = rnd16(); x1[i] = rnd16(); x2[i] = rnd16(); x3[i] = rnd16();
      w1[i] = rnd16(); w2[i] = rnd16(); w3[i] = rnd16();
    }
    for (int b = 0; b < NB; b++) {
      int o = 16 * b;
      ibfly4_scalar(x0+o, x1+o, x2+o, x3+o, s0+o, s1+o, s2+o, s3+o, w1+o, w2+o, w3+o);
      ibfly4_simde(x0+o, x1+o, x2+o, x3+o, m0+o, m1+o, m2+o, m3+o, w1+o, w2+o, w3+o);
    }
    if (memcmp(s0, m0, NI*2) || memcmp(s1, m1, NI*2) || memcmp(s2, m2, NI*2) || memcmp(s3, m3, NI*2))
      mism_sm++;
#if defined(__riscv) && defined(__riscv_vector)
    ibfly4_rvv(x0, x1, x2, x3, r0, r1, r2, r3, w1, w2, w3, NC);
    if (memcmp(s0, r0, NI*2) || memcmp(s1, r1, NI*2) || memcmp(s2, r2, NI*2) || memcmp(s3, r3, NI*2))
      mism_rv++;
#endif
  }
  printf("correctness (%d runs x %d complex):\n", N, NC);
  printf("  scalar vs simde : %s (%ld mismatches)\n", mism_sm ? "FAIL" : "OK", mism_sm);
#if defined(__riscv) && defined(__riscv_vector)
  printf("  scalar vs rvv   : %s (%ld mismatches)\n", mism_rv ? "FAIL" : "OK", mism_rv);
#else
  printf("  rvv             : (not built -- no __riscv_vector)\n");
#endif

  /* benchmark: ns per radix-4 butterfly (NC complex = NB blocks per call) */
  enum { REP = 300000 };
  volatile int16_t sink = 0;
  uint64_t t0 = now_ns();
  for (int t = 0; t < REP; t++) {
    for (int b = 0; b < NB; b++) { int o = 16*b; ibfly4_simde(x0+o,x1+o,x2+o,x3+o,m0+o,m1+o,m2+o,m3+o,w1+o,w2+o,w3+o); }
    sink ^= m0[0];
  }
  uint64_t t1 = now_ns();
  double simde_ns = (double)(t1 - t0) / REP / NB;
#if defined(__riscv) && defined(__riscv_vector)
  t0 = now_ns();
  for (int t = 0; t < REP; t++) { ibfly4_rvv(x0, x1, x2, x3, r0, r1, r2, r3, w1, w2, w3, NC); sink ^= r0[0]; }
  t1 = now_ns();
  double rvv_ns = (double)(t1 - t0) / REP / NB;
  printf("benchmark (ns per radix-4 butterfly of 8 complex, %d reps x %d blocks):\n", REP, NB);
  printf("  simde : %.2f ns\n", simde_ns);
  printf("  rvv   : %.2f ns   (%.2fx vs simde)\n", rvv_ns, simde_ns / rvv_ns);
#else
  printf("benchmark: simde %.2f ns/butterfly\n", simde_ns);
#endif
  (void)sink;
  return (mism_sm || mism_rv) ? 1 : 0;
}
