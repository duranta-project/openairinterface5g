/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV port harness -- kernel #3: single-layer LLR demappers.
 *
 * nr_16qam_llr / nr_64qam_llr / nr_256qam_llr from
 * openair1/PHY/nr_phy_common/src/nr_phy_common.c.
 *
 * Each is a per-RE chain: llr[0]=rxF.r, llr[1]=rxF.i, then for each channel
 * magnitude m: llr += (m - protected_abs(prev)) (saturating). The soft bits
 * for one RE are stored contiguously, so the natural RVV form is a segment
 * store of 4 / 6 / 8 fields (vsseg4/6/8e16) fed by segment loads (vlseg2e16).
 *
 * protected_abs(x) = abs(x) except INT16_MIN -> INT16_MAX (saturating abs).
 *
 * Three implementations checked BYTE-FOR-BYTE and benchmarked:
 *   scalar : plain-C ground truth
 *   simde  : OAI's exact 256-bit vector loop (what ships today)
 *   rvv    : hand-written RVV segment load/store
 * nb_re is a multiple of 8 so only the SIMDe 256-bit path runs (no tail; the
 * scalar tail in OAI differs from its own vector path by omitting the abs).
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
static inline int16_t ssub(int16_t a, int16_t b)
{
  int32_t v = (int32_t)a - (int32_t)b;
  return v > 32767 ? 32767 : (v < -32768 ? -32768 : (int16_t)v);
}
static inline int16_t pabs(int16_t x)
{
  return x == -32768 ? 32767 : (x < 0 ? -x : x);
}

static void llr16_scalar(const c16_t *rxF, const c16_t *ma, int16_t *llr, uint32_t n)
{
  for (uint32_t k = 0; k < n; k++) {
    *llr++ = rxF[k].r;
    *llr++ = rxF[k].i;
    *llr++ = ssub(ma[k].r, pabs(rxF[k].r));
    *llr++ = ssub(ma[k].i, pabs(rxF[k].i));
  }
}
static void llr64_scalar(const c16_t *rxF, const c16_t *ma, const c16_t *mb, int16_t *llr, uint32_t n)
{
  for (uint32_t k = 0; k < n; k++) {
    int16_t x1r = ssub(ma[k].r, pabs(rxF[k].r)), x1i = ssub(ma[k].i, pabs(rxF[k].i));
    int16_t x2r = ssub(mb[k].r, pabs(x1r)), x2i = ssub(mb[k].i, pabs(x1i));
    *llr++ = rxF[k].r;
    *llr++ = rxF[k].i;
    *llr++ = x1r;
    *llr++ = x1i;
    *llr++ = x2r;
    *llr++ = x2i;
  }
}
static void llr256_scalar(const c16_t *rxF, const c16_t *ma, const c16_t *mb, const c16_t *mc, int16_t *llr, uint32_t n)
{
  for (uint32_t k = 0; k < n; k++) {
    int16_t x1r = ssub(ma[k].r, pabs(rxF[k].r)), x1i = ssub(ma[k].i, pabs(rxF[k].i));
    int16_t x2r = ssub(mb[k].r, pabs(x1r)), x2i = ssub(mb[k].i, pabs(x1i));
    int16_t x3r = ssub(mc[k].r, pabs(x2r)), x3i = ssub(mc[k].i, pabs(x2i));
    *llr++ = rxF[k].r;
    *llr++ = rxF[k].i;
    *llr++ = x1r;
    *llr++ = x1i;
    *llr++ = x2r;
    *llr++ = x2i;
    *llr++ = x3r;
    *llr++ = x3i;
  }
}

/* ---- SIMDe baseline: OAI's exact 256-bit vector loops (nb_re % 8 == 0) ------ */
static const int16_t ones_epi16[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
static inline simde__m256i protected_abs256(const simde__m256i in)
{
  const simde__m256i x = simde_mm256_adds_epi16(simde_mm256_subs_epi16(in, *(simde__m256i *)ones_epi16), *(simde__m256i *)ones_epi16);
  return simde_mm256_abs_epi16(x);
}
static void llr16_simde(const c16_t *rxdataF_comp, const c16_t *ch_mag_in, int16_t *llr, uint32_t nb_re)
{
  simde__m256i *rxF_256 = (simde__m256i *)rxdataF_comp;
  simde__m256i *ch_mag256 = (simde__m256i *)ch_mag_in;
  int64_t *llr_64 = (int64_t *)llr;
  for (uint32_t i = 0; i < (nb_re >> 3); i++) {
    simde__m256i xmm0 = protected_abs256(*rxF_256);
    xmm0 = simde_mm256_subs_epi16(*ch_mag256, xmm0);
    simde__m256i xmm1 = simde_mm256_unpacklo_epi32(*rxF_256, xmm0);
    simde__m256i xmm2 = simde_mm256_unpackhi_epi32(*rxF_256, xmm0);
    *llr_64++ = simde_mm256_extract_epi64(xmm1, 0);
    *llr_64++ = simde_mm256_extract_epi64(xmm1, 1);
    *llr_64++ = simde_mm256_extract_epi64(xmm2, 0);
    *llr_64++ = simde_mm256_extract_epi64(xmm2, 1);
    *llr_64++ = simde_mm256_extract_epi64(xmm1, 2);
    *llr_64++ = simde_mm256_extract_epi64(xmm1, 3);
    *llr_64++ = simde_mm256_extract_epi64(xmm2, 2);
    *llr_64++ = simde_mm256_extract_epi64(xmm2, 3);
    rxF_256++;
    ch_mag256++;
  }
}
static void llr64_simde(const c16_t *rxdataF_comp, const c16_t *ch_mag, const c16_t *ch_mag2, int16_t *llr, uint32_t nb_re)
{
  simde__m256i *rxF = (simde__m256i *)rxdataF_comp;
  simde__m256i *ch_maga = (simde__m256i *)ch_mag;
  simde__m256i *ch_magb = (simde__m256i *)ch_mag2;
  int32_t *llr_32 = (int32_t *)llr;
  for (uint32_t i = 0; i < (nb_re >> 3); i++) {
    simde__m256i xmm0 = simde_mm256_loadu_si256(rxF);
    simde__m256i xmm1 = protected_abs256(xmm0);
    xmm1 = simde_mm256_subs_epi16(*ch_maga, xmm1);
    simde__m256i xmm2 = protected_abs256(xmm1);
    xmm2 = simde_mm256_subs_epi16(*ch_magb, xmm2);
    for (int l = 0; l < 8; l++) {
      *llr_32++ = simde_mm256_extract_epi32(xmm0, l);
      *llr_32++ = simde_mm256_extract_epi32(xmm1, l);
      *llr_32++ = simde_mm256_extract_epi32(xmm2, l);
    }
    rxF++;
    ch_maga++;
    ch_magb++;
  }
}
static void llr256_simde(const c16_t *rxdataF_comp, const c16_t *ch_mag, const c16_t *ch_mag2, const c16_t *ch_mag3, int16_t *llr, uint32_t nb_re)
{
  simde__m256i *rxF_256 = (simde__m256i *)rxdataF_comp;
  simde__m256i *llr256 = (simde__m256i *)llr;
  simde__m256i *ch_maga = (simde__m256i *)ch_mag;
  simde__m256i *ch_magb = (simde__m256i *)ch_mag2;
  simde__m256i *ch_magc = (simde__m256i *)ch_mag3;
  for (uint32_t i = 0; i < (nb_re >> 3); i++) {
    simde__m256i xmm0 = protected_abs256(*rxF_256);
    xmm0 = simde_mm256_subs_epi16(*ch_maga, xmm0);
    simde__m256i xmm1 = protected_abs256(xmm0);
    xmm1 = simde_mm256_subs_epi16(*ch_magb, xmm1);
    simde__m256i xmm2 = protected_abs256(xmm1);
    xmm2 = simde_mm256_subs_epi16(*ch_magc, xmm2);
    simde__m256i xmm3 = simde_mm256_unpacklo_epi32(*rxF_256, xmm0);
    simde__m256i xmm4 = simde_mm256_unpackhi_epi32(*rxF_256, xmm0);
    simde__m256i xmm5 = simde_mm256_unpacklo_epi32(xmm1, xmm2);
    simde__m256i xmm6 = simde_mm256_unpackhi_epi32(xmm1, xmm2);
    xmm0 = simde_mm256_unpacklo_epi64(xmm3, xmm5);
    xmm1 = simde_mm256_unpackhi_epi64(xmm3, xmm5);
    xmm2 = simde_mm256_unpacklo_epi64(xmm4, xmm6);
    xmm3 = simde_mm256_unpackhi_epi64(xmm4, xmm6);
    *llr256++ = simde_mm256_permute2x128_si256(xmm0, xmm1, 0x20);
    *llr256++ = simde_mm256_permute2x128_si256(xmm2, xmm3, 0x20);
    *llr256++ = simde_mm256_permute2x128_si256(xmm0, xmm1, 0x31);
    *llr256++ = simde_mm256_permute2x128_si256(xmm2, xmm3, 0x31);
    ch_magc++;
    ch_magb++;
    ch_maga++;
    rxF_256++;
  }
}

/* ---- RVV implementations --------------------------------------------------- */
#if defined(__riscv) && defined(__riscv_vector)
#include <riscv_vector.h>
#define LD2(p, vl) __riscv_vlseg2e16_v_i16m1x2((const int16_t *)(p), (vl))
#define RE(t) __riscv_vget_v_i16m1x2_i16m1((t), 0)
#define IM(t) __riscv_vget_v_i16m1x2_i16m1((t), 1)

static inline vint16m1_t rvv_pabs(vint16m1_t x, size_t vl)
{
  vint16m1_t t = __riscv_vsadd_vx_i16m1(__riscv_vssub_vx_i16m1(x, 1, vl), 1, vl);
  return __riscv_vmax_vv_i16m1(t, __riscv_vneg_v_i16m1(t, vl), vl); /* t != INT16_MIN here */
}

static void llr16_rvv(const c16_t *rxF, const c16_t *ma, int16_t *llr, uint32_t n)
{
  for (uint32_t k = 0; k < n;) {
    size_t vl = __riscv_vsetvl_e16m1(n - k);
    vint16m1x2_t rx = LD2(&rxF[k], vl), a = LD2(&ma[k], vl);
    vint16m1_t R = RE(rx), I = IM(rx);
    vint16m1_t Mr = __riscv_vssub_vv_i16m1(RE(a), rvv_pabs(R, vl), vl);
    vint16m1_t Mi = __riscv_vssub_vv_i16m1(IM(a), rvv_pabs(I, vl), vl);
    __riscv_vsseg4e16_v_i16m1x4(llr + 4 * k, __riscv_vcreate_v_i16m1x4(R, I, Mr, Mi), vl);
    k += vl;
  }
}
static void llr64_rvv(const c16_t *rxF, const c16_t *ma, const c16_t *mb, int16_t *llr, uint32_t n)
{
  for (uint32_t k = 0; k < n;) {
    size_t vl = __riscv_vsetvl_e16m1(n - k);
    vint16m1x2_t rx = LD2(&rxF[k], vl), a = LD2(&ma[k], vl), b = LD2(&mb[k], vl);
    vint16m1_t R = RE(rx), I = IM(rx);
    vint16m1_t x1r = __riscv_vssub_vv_i16m1(RE(a), rvv_pabs(R, vl), vl);
    vint16m1_t x1i = __riscv_vssub_vv_i16m1(IM(a), rvv_pabs(I, vl), vl);
    vint16m1_t x2r = __riscv_vssub_vv_i16m1(RE(b), rvv_pabs(x1r, vl), vl);
    vint16m1_t x2i = __riscv_vssub_vv_i16m1(IM(b), rvv_pabs(x1i, vl), vl);
    __riscv_vsseg6e16_v_i16m1x6(llr + 6 * k, __riscv_vcreate_v_i16m1x6(R, I, x1r, x1i, x2r, x2i), vl);
    k += vl;
  }
}
static void llr256_rvv(const c16_t *rxF, const c16_t *ma, const c16_t *mb, const c16_t *mc, int16_t *llr, uint32_t n)
{
  for (uint32_t k = 0; k < n;) {
    size_t vl = __riscv_vsetvl_e16m1(n - k);
    vint16m1x2_t rx = LD2(&rxF[k], vl), a = LD2(&ma[k], vl), b = LD2(&mb[k], vl), c = LD2(&mc[k], vl);
    vint16m1_t R = RE(rx), I = IM(rx);
    vint16m1_t x1r = __riscv_vssub_vv_i16m1(RE(a), rvv_pabs(R, vl), vl);
    vint16m1_t x1i = __riscv_vssub_vv_i16m1(IM(a), rvv_pabs(I, vl), vl);
    vint16m1_t x2r = __riscv_vssub_vv_i16m1(RE(b), rvv_pabs(x1r, vl), vl);
    vint16m1_t x2i = __riscv_vssub_vv_i16m1(IM(b), rvv_pabs(x1i, vl), vl);
    vint16m1_t x3r = __riscv_vssub_vv_i16m1(RE(c), rvv_pabs(x2r, vl), vl);
    vint16m1_t x3i = __riscv_vssub_vv_i16m1(IM(c), rvv_pabs(x2i, vl), vl);
    __riscv_vsseg8e16_v_i16m1x8(llr + 8 * k, __riscv_vcreate_v_i16m1x8(R, I, x1r, x1i, x2r, x2i, x3r, x3i), vl);
    k += vl;
  }
}
#endif

/* ---- driver ---------------------------------------------------------------- */
#define N 4096 /* multiple of 8 */
static uint64_t rng = 0x9e3779b97f4a7c15ULL;
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
static int cmp(const int16_t *a, const int16_t *b, size_t n, const char *tag)
{
  for (size_t k = 0; k < n; k++)
    if (a[k] != b[k]) {
      fprintf(stderr, "  MISMATCH [%s] idx %zu: %d vs %d\n", tag, k, a[k], b[k]);
      return 1;
    }
  return 0;
}

int main(int argc, char **argv)
{
  static c16_t rxF[N], ma[N], mb[N], mc[N];
  static int16_t s[8 * N], v[8 * N], q[8 * N];
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

  for (size_t k = 0; k < N; k++) {
    rxF[k] = (c16_t){rnd16(), rnd16()};
    /* magnitudes are non-negative in practice; test both signs for rigor */
    ma[k] = (c16_t){rnd16(), rnd16()};
    mb[k] = (c16_t){rnd16(), rnd16()};
    mc[k] = (c16_t){rnd16(), rnd16()};
  }
  rxF[0] = (c16_t){-32768, -32768};
  rxF[1] = (c16_t){32767, -32768};
  ma[0] = (c16_t){-32768, 32767};

  struct {
    const char *name;
    int qm, nfields;
    void (*sc)(void);
    void (*si)(void);
    void (*rv)(void);
  } cases[3];
  (void)cases;

#define CHECK(name, nf, scall, sicall, rvcall)                        \
  do {                                                                \
    memset(s, 0, sizeof(s));                                          \
    memset(v, 0, sizeof(v));                                          \
    memset(q, 0, sizeof(q));                                          \
    scall;                                                            \
    sicall;                                                           \
    int bad = cmp(s, v, (size_t)(nf) * N, name " simde");             \
    rvcall;                                                           \
    bad |= cmp(s, q, (size_t)(nf) * N, name " rvv");                  \
    printf("  %-6s: %s\n", name, bad ? "FAIL" : "byte-exact OK");     \
    fails += bad;                                                     \
  } while (0)

#if defined(__riscv) && defined(__riscv_vector)
  CHECK("16QAM", 4, llr16_scalar(rxF, ma, s, N), llr16_simde(rxF, ma, v, N), llr16_rvv(rxF, ma, q, N));
  CHECK("64QAM", 6, llr64_scalar(rxF, ma, mb, s, N), llr64_simde(rxF, ma, mb, v, N), llr64_rvv(rxF, ma, mb, q, N));
  CHECK("256QAM", 8, llr256_scalar(rxF, ma, mb, mc, s, N), llr256_simde(rxF, ma, mb, mc, v, N), llr256_rvv(rxF, ma, mb, mc, q, N));
#else
  (void)q;
  CHECK("16QAM", 4, llr16_scalar(rxF, ma, s, N), llr16_simde(rxF, ma, v, N), (void)0);
  CHECK("64QAM", 6, llr64_scalar(rxF, ma, mb, s, N), llr64_simde(rxF, ma, mb, v, N), (void)0);
  CHECK("256QAM", 8, llr256_scalar(rxF, ma, mb, mc, s, N), llr256_simde(rxF, ma, mb, mc, v, N), (void)0);
#endif

  /* benchmark 64QAM */
  const int iters = 20000;
  double t0 = now_ns();
  for (int it = 0; it < iters; it++)
    llr64_scalar(rxF, ma, mb, s, N);
  double ts = now_ns() - t0;
  t0 = now_ns();
  for (int it = 0; it < iters; it++)
    llr64_simde(rxF, ma, mb, v, N);
  double tsi = now_ns() - t0;
  printf("\n64QAM scalar     : %.2f ns/RE\n", ts / ((double)iters * N));
  printf("64QAM simde(OAI) : %.2f ns/RE  (%.2fx vs scalar)\n", tsi / ((double)iters * N), ts / tsi);
#if defined(__riscv) && defined(__riscv_vector)
  t0 = now_ns();
  for (int it = 0; it < iters; it++)
    llr64_rvv(rxF, ma, mb, q, N);
  double tv = now_ns() - t0;
  printf("64QAM rvv        : %.2f ns/RE  (%.2fx vs scalar, %.2fx vs simde/OAI)\n",
         tv / ((double)iters * N),
         ts / tv,
         tsi / tv);
#endif

  printf("\nRESULT: %s\n", fails ? "FAIL" : "PASS");
  return fails ? 1 : 0;
}
