/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV port harness -- kernel #2: nr_channel_compensation inner loop.
 *
 * Models the hot inner loop of nr_channel_compensation() (16QAM, single Rx
 * antenna, no rho) which produces, per resource element:
 *
 *   rxComp = conj(chF) * rxF  >> shift            (matched filter, saturating)
 *   mag    = (|chF|^2 >> shift), saturated to int16
 *   ch_mag = mulhrs(mag, QAM16_n1)                (fixed-point Q15 round-mul)
 *
 * Three implementations are compared BYTE-FOR-BYTE and benchmarked:
 *   scalar : plain C ground truth
 *   simde  : the exact simde intrinsic sequence OAI ships today (SIMDe->scalar
 *            or autovectorized) -- the honest "before"
 *   rvv    : hand-written RVV with unit-stride segment load/store
 *
 * The MRC accumulate across Rx antennas and the rho term are just add/adds
 * variants of these same two primitives; validating the primitives validates
 * the function.
 *
 * Bit-exactness notes (the whole point of this harness):
 *  - madd/vwmacc accumulate in 32 bits and WRAP; the one overflow case
 *    (|chF|^2 with chF=(-32768,-32768) = 2^31) must wrap identically.
 *  - >> shift is an arithmetic (truncating) shift.
 *  - packs_epi32 saturates 32->16; mulhrs does NOT saturate (it truncates to
 *    16 bits), so its RVV narrow uses vnsra (truncating), not vnclip.
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

#define QAM16_n1 20724 /* from openair1/PHY/impl_defs_top.h */

/* ---- heterogeneous-core affinity (see rvv_cpx_mult_test.c) ----------------- */
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
/* _mm_mulhrs_epi16: round( a*b / 2^15 ), truncated to 16 bits (no saturation) */
static inline int16_t mulhrs(int16_t a, int16_t b)
{
  int32_t p = (int32_t)a * (int32_t)b;
  return (int16_t)((((p >> 14) + 1) >> 1) & 0xFFFF);
}

static void chcomp_scalar(const c16_t *chF, const c16_t *rxF, c16_t *rxComp, c16_t *chMag, size_t n, int shift)
{
  for (size_t k = 0; k < n; k++) {
    int32_t re = wadd((int32_t)chF[k].r * rxF[k].r, (int32_t)chF[k].i * rxF[k].i) >> shift;
    /* conj negates chF.i as a WRAPPING int16 (sign_epi16): -(-32768) = -32768.
     * im = neg16(chF.i)*rxF.r + chF.r*rxF.i, matching oai_mm256_cpx_mult_conj. */
    int16_t nci = (int16_t)(-chF[k].i);
    int32_t im = wadd((int32_t)nci * rxF[k].r, (int32_t)chF[k].r * rxF[k].i) >> shift;
    rxComp[k].r = sat16(re);
    rxComp[k].i = sat16(im);
    int32_t mag = wadd((int32_t)chF[k].r * chF[k].r, (int32_t)chF[k].i * chF[k].i) >> shift;
    int16_t mag16 = sat16(mag);
    int16_t m = mulhrs(mag16, QAM16_n1);
    chMag[k].r = m;
    chMag[k].i = m;
  }
}

/* ---- SIMDe baseline: the exact sequence from nr_channel_compensation() ------ */
static const int16_t conj_mask_i16[16] = {1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1};
static const int8_t swap_mask_i8[32] = {2,  3,  0,  1,  6,  7,  4,  5,  10, 11, 8,  9,  14, 15, 12, 13,
                                        18, 19, 16, 17, 22, 23, 20, 21, 26, 27, 24, 25, 30, 31, 28, 29};

static inline simde__m256i mm256_conj(simde__m256i a)
{
  return simde_mm256_sign_epi16(a, simde_mm256_loadu_si256((const simde__m256i *)conj_mask_i16));
}
static inline simde__m256i mm256_swap(simde__m256i a)
{
  return simde_mm256_shuffle_epi8(a, simde_mm256_loadu_si256((const simde__m256i *)swap_mask_i8));
}
static inline simde__m256i smadd(simde__m256i a, simde__m256i b, int shift)
{
  return simde_mm256_srai_epi32(simde_mm256_madd_epi16(a, b), shift);
}
static inline simde__m256i pack(simde__m256i a, simde__m256i b)
{
  return simde_mm256_packs_epi32(simde_mm256_unpacklo_epi32(a, b), simde_mm256_unpackhi_epi32(a, b));
}
static inline simde__m256i cpx_mult_conj(simde__m256i a, simde__m256i b, int shift)
{
  return pack(smadd(a, b, shift), smadd(mm256_swap(mm256_conj(a)), b, shift));
}

static void chcomp_simde(const c16_t *chF, const c16_t *rxF, c16_t *rxComp, c16_t *chMag, size_t n, int shift)
{
  const simde__m256i *chF_256 = (const simde__m256i *)chF;
  const simde__m256i *rxF_256 = (const simde__m256i *)rxF;
  simde__m256i *rxComp_256 = (simde__m256i *)rxComp;
  simde__m256i *chMag_256 = (simde__m256i *)chMag;
  const simde__m256i ampa = simde_mm256_set1_epi16(QAM16_n1);

  for (size_t i = 0; i < n >> 3; i++) {
    rxComp_256[i] = cpx_mult_conj(chF_256[i], rxF_256[i], shift);
    simde__m256i mag = smadd(chF_256[i], chF_256[i], shift);
    mag = simde_mm256_packs_epi32(mag, mag);
    mag = simde_mm256_unpacklo_epi16(mag, mag);
    chMag_256[i] = simde_mm256_mulhrs_epi16(mag, ampa);
  }
}

/* ---- RVV implementation ---------------------------------------------------- */
#if defined(__riscv) && defined(__riscv_vector)
#include <riscv_vector.h>

static void chcomp_rvv(const c16_t *chF, const c16_t *rxF, c16_t *rxComp, c16_t *chMag, size_t n, int shift)
{
  const int16_t *pc = &chF[0].r, *px = &rxF[0].r;
  int16_t *pcomp = &rxComp[0].r, *pmag = &chMag[0].r;

  for (size_t k = 0; k < n;) {
    size_t vl = __riscv_vsetvl_e16m1(n - k);

    vint16m1x2_t sc = __riscv_vlseg2e16_v_i16m1x2(pc + 2 * k, vl);
    vint16m1x2_t sx = __riscv_vlseg2e16_v_i16m1x2(px + 2 * k, vl);
    vint16m1_t cr = __riscv_vget_v_i16m1x2_i16m1(sc, 0);
    vint16m1_t ci = __riscv_vget_v_i16m1x2_i16m1(sc, 1);
    vint16m1_t xr = __riscv_vget_v_i16m1x2_i16m1(sx, 0);
    vint16m1_t xi = __riscv_vget_v_i16m1x2_i16m1(sx, 1);

    /* rxComp = conj(chF)*rxF: re = cr*xr + ci*xi.
     * im uses the WRAPPING int16 negation of ci (matches sign_epi16):
     *   im = neg16(ci)*xr + cr*xi. */
    vint32m2_t re = __riscv_vwmacc_vv_i32m2(__riscv_vwmul_vv_i32m2(cr, xr, vl), ci, xi, vl);
    vint16m1_t nci = __riscv_vneg_v_i16m1(ci, vl);
    vint32m2_t im = __riscv_vwmacc_vv_i32m2(__riscv_vwmul_vv_i32m2(nci, xr, vl), cr, xi, vl);
    vint16m1_t re16 = __riscv_vnclip_wx_i16m1(__riscv_vsra_vx_i32m2(re, (size_t)shift, vl), 0, __RISCV_VXRM_RDN, vl);
    vint16m1_t im16 = __riscv_vnclip_wx_i16m1(__riscv_vsra_vx_i32m2(im, (size_t)shift, vl), 0, __RISCV_VXRM_RDN, vl);
    __riscv_vsseg2e16_v_i16m1x2(pcomp + 2 * k, __riscv_vcreate_v_i16m1x2(re16, im16), vl);

    /* mag = |chF|^2 >> shift, sat16; then mulhrs(mag, QAM16_n1) into re & im */
    vint32m2_t mg = __riscv_vwmacc_vv_i32m2(__riscv_vwmul_vv_i32m2(cr, cr, vl), ci, ci, vl);
    vint16m1_t mag16 = __riscv_vnclip_wx_i16m1(__riscv_vsra_vx_i32m2(mg, (size_t)shift, vl), 0, __RISCV_VXRM_RDN, vl);
    /* mulhrs: ((mag16*amp >> 14) + 1) >> 1, truncating narrow (wrap, not sat) */
    vint32m2_t p = __riscv_vwmul_vx_i32m2(mag16, QAM16_n1, vl);
    p = __riscv_vsra_vx_i32m2(__riscv_vadd_vx_i32m2(__riscv_vsra_vx_i32m2(p, 14, vl), 1, vl), 1, vl);
    vint16m1_t m16 = __riscv_vnsra_wx_i16m1(p, 0, vl);
    __riscv_vsseg2e16_v_i16m1x2(pmag + 2 * k, __riscv_vcreate_v_i16m1x2(m16, m16), vl);

    k += vl;
  }
}
#endif /* __riscv_vector */

/* ---- driver ---------------------------------------------------------------- */
#define N 4096

static uint64_t rng = 0x1234567890abcdefULL;
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

static int cmp(const c16_t *a, const c16_t *b, size_t n, const char *tag)
{
  for (size_t k = 0; k < n; k++)
    if (a[k].r != b[k].r || a[k].i != b[k].i) {
      fprintf(stderr, "  MISMATCH [%s] idx %zu: (%d,%d) vs (%d,%d)\n", tag, k, a[k].r, a[k].i, b[k].r, b[k].i);
      return 1;
    }
  return 0;
}

int main(int argc, char **argv)
{
  static c16_t chF[N], rxF[N];
  static c16_t comp_ref[N], mag_ref[N], comp_s[N], mag_s[N], comp_v[N], mag_v[N];
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

  for (int shift = 0; shift <= 12; shift += 4) {
    for (size_t k = 0; k < N; k++) {
      chF[k] = (c16_t){rnd16(), rnd16()};
      rxF[k] = (c16_t){rnd16(), rnd16()};
    }
    chF[0] = (c16_t){-32768, -32768};
    rxF[0] = (c16_t){-32768, -32768}; /* |chF|^2 overflow + madd wrap */
    chF[1] = (c16_t){32767, -32768};
    rxF[1] = (c16_t){-32768, 32767};

    chcomp_scalar(chF, rxF, comp_ref, mag_ref, N, shift);
    chcomp_simde(chF, rxF, comp_s, mag_s, N, shift);
    int bad = cmp(comp_ref, comp_s, N, "simde rxComp") | cmp(mag_ref, mag_s, N, "simde chMag");
#if defined(__riscv) && defined(__riscv_vector)
    chcomp_rvv(chF, rxF, comp_v, mag_v, N, shift);
    bad |= cmp(comp_ref, comp_v, N, "rvv rxComp") | cmp(mag_ref, mag_v, N, "rvv chMag");
#else
    (void)comp_v;
    (void)mag_v;
#endif
    printf("  shift=%2d: %s\n", shift, bad ? "FAIL" : "byte-exact OK (simde + rvv vs scalar)");
    fails += bad;
  }

  const int shift = 8, iters = 20000;
  double t0 = now_ns();
  for (int it = 0; it < iters; it++)
    chcomp_scalar(chF, rxF, comp_ref, mag_ref, N, shift);
  double ts = now_ns() - t0;
  t0 = now_ns();
  for (int it = 0; it < iters; it++)
    chcomp_simde(chF, rxF, comp_s, mag_s, N, shift);
  double tsi = now_ns() - t0;
  printf("\nscalar     : %.2f ns/elem\n", ts / ((double)iters * N));
  printf("simde(OAI) : %.2f ns/elem  (%.2fx vs scalar)\n", tsi / ((double)iters * N), ts / tsi);
#if defined(__riscv) && defined(__riscv_vector)
  t0 = now_ns();
  for (int it = 0; it < iters; it++)
    chcomp_rvv(chF, rxF, comp_v, mag_v, N, shift);
  double tv = now_ns() - t0;
  printf("rvv        : %.2f ns/elem  (%.2fx vs scalar, %.2fx vs simde/OAI)\n",
         tv / ((double)iters * N),
         ts / tv,
         tsi / tv);
#endif

  printf("\nRESULT: %s\n", fails ? "FAIL" : "PASS");
  return fails ? 1 : 0;
}
