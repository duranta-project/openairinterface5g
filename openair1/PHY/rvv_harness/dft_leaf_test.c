/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV port harness -- kernel #13: the idft64 LEAF primitives.
 *
 * idft64 = input-reorder + 2x idft16_simd256 + 2x ibfly4_16_256 + >>3 scale.
 * This harness ports and byte-exact-validates (vs the SIMDe/AVX2 reference that
 * OAI ships) the four pieces:
 *   1. packed_cmult2_256  : cpack(madd(a,b), madd(a,b2))         -- twiddle mult
 *   2. ibfly4_16_256      : radix-4 butterfly (packed layout)    -- elementwise
 *   3. idft16_simd256     : 16-pt, 2 packed DFTs, 2 radix-4 stages + 2 reorders
 *   4. idft64 input reorder (permutevar8x32 + unpacklo/hi_epi64) -- permutation
 *
 * RVV strategy: vlseg2e16 gives re/im as separate fields, so the complex_shuffle
 * "flip" (sign_epi16(conjugatedft) then swap re/im) is free: flip = (im, -re)
 * with -re a WRAPPING vneg (matches sign_epi16: -(-32768)=-32768). madd = vwmul+
 * vwmacc (int16->int32, wrapping); cpack = vnclip_wx(_,15,RDN) (== srai(15)+packs).
 * adds/subs_epi16 = vsadd/vssub. The three permutations (idft16's two internal
 * unpack reorders and idft64's input reorder) reduce to fixed complex-index
 * gathers (derived below) done with vluxei32.
 *
 * conjugatedft = {-1,1,...}: negates the real lane; after the re<->im swap the
 * flip is (re'=im, im'=-re).
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

static int use_ai_core(void)
{
  FILE *fp = fopen("/proc/set_ai_thread", "w");
  if (!fp) return -1;
  int rc = fprintf(fp, "%ld\n", (long)getpid());
  return (rc < 0 || fclose(fp) != 0) ? -1 : 0;
}
static int pin_to_cpu(int cpu)
{
  cpu_set_t set; CPU_ZERO(&set); CPU_SET(cpu, &set);
  return sched_setaffinity(0, sizeof(set), &set);
}
static inline uint64_t now_ns(void)
{
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}
static uint32_t rng = 0x1234567u;
static int16_t rnd16(void) { rng = rng * 1103515245u + 12345u; return (int16_t)(rng >> 16); }

static const int16_t conjugatedft[16] = {-1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1};
static const int16_t tw16rep[48] = {
  32767,0,30272,-12540,23169,-23170,12539,-30273,32767,0,30272,-12540,23169,-23170,12539,-30273,
  32767,0,23169,-23170,0,-32767,-23170,-23170,32767,0,23169,-23170,0,-32767,-23170,-23170,
  32767,0,12539,-30273,-23170,-23170,-30273,12539,32767,0,12539,-30273,-23170,-23170,-30273,12539};
static const int16_t tw16crep[48] = {
  0,32767,12540,30272,23170,23169,30273,12539,0,32767,12540,30272,23170,23169,30273,12539,
  0,32767,23170,23169,32767,0,23170,-23170,0,32767,23170,23169,32767,0,23170,-23170,
  0,32767,30273,12539,23170,-23170,-12539,-30273,0,32767,30273,12539,23170,-23170,-12539,-30273};

/* ================= SIMDe references (exact OAI sequences) ================= */
static inline simde__m256i cpack_256(simde__m256i xre, simde__m256i xim)
{
  simde__m256i t1 = simde_mm256_unpacklo_epi32(xre, xim);
  simde__m256i t2 = simde_mm256_unpackhi_epi32(xre, xim);
  return simde_mm256_packs_epi32(simde_mm256_srai_epi32(t1, 15), simde_mm256_srai_epi32(t2, 15));
}
static inline simde__m256i packed_cmult2_256(simde__m256i a, simde__m256i b, simde__m256i b2)
{
  return cpack_256(simde_mm256_madd_epi16(a, b), simde_mm256_madd_epi16(a, b2));
}
static const int8_t cshuf[32] = {2,3,0,1,6,7,4,5,10,11,8,9,14,15,12,13,18,19,16,17,22,23,20,21,26,27,24,25,30,31,28,29};
static inline simde__m256i cflip(simde__m256i x)
{
  simde__m256i s = simde_mm256_sign_epi16(x, simde_mm256_loadu_si256((const simde__m256i *)conjugatedft));
  return simde_mm256_shuffle_epi8(s, simde_mm256_loadu_si256((const simde__m256i *)cshuf));
}
static void ibfly4_16_simde(const simde__m256i *x0, const simde__m256i *x1, const simde__m256i *x2, const simde__m256i *x3,
                            simde__m256i *y0, simde__m256i *y1, simde__m256i *y2, simde__m256i *y3,
                            const simde__m256i *tw1, const simde__m256i *tw2, const simde__m256i *tw3,
                            const simde__m256i *tw1b, const simde__m256i *tw2b, const simde__m256i *tw3b)
{
  simde__m256i x1t = packed_cmult2_256(*x1, *tw1, *tw1b);
  simde__m256i x2t = packed_cmult2_256(*x2, *tw2, *tw2b);
  simde__m256i x3t = packed_cmult2_256(*x3, *tw3, *tw3b);
  simde__m256i x02t = simde_mm256_adds_epi16(*x0, x2t);
  simde__m256i x13t = simde_mm256_adds_epi16(x1t, x3t);
  *y0 = simde_mm256_adds_epi16(x02t, x13t);
  *y2 = simde_mm256_subs_epi16(x02t, x13t);
  simde__m256i x1f = cflip(x1t), x3f = cflip(x3t);
  x02t = simde_mm256_subs_epi16(*x0, x2t);
  x13t = simde_mm256_subs_epi16(x1f, x3f);
  *y3 = simde_mm256_adds_epi16(x02t, x13t);
  *y1 = simde_mm256_subs_epi16(x02t, x13t);
}
static void idft16_simde(const int16_t *x, int16_t *y)
{
  const simde__m256i *tw16a = (const simde__m256i *)tw16rep, *tw16b = (const simde__m256i *)tw16crep;
  const simde__m256i *x256 = (const simde__m256i *)x;
  simde__m256i *y256 = (simde__m256i *)y;
  simde__m256i x02t, x13t, x1f, x3f, xt0, xt1, xt2, xt3, yt0, yt1, yt2, yt3;
  x02t = simde_mm256_adds_epi16(x256[0], x256[2]);
  x13t = simde_mm256_adds_epi16(x256[1], x256[3]);
  xt0 = simde_mm256_adds_epi16(x02t, x13t);
  xt2 = simde_mm256_subs_epi16(x02t, x13t);
  x1f = cflip(x256[1]); x3f = cflip(x256[3]);
  x02t = simde_mm256_subs_epi16(x256[0], x256[2]);
  x13t = simde_mm256_subs_epi16(x1f, x3f);
  xt3 = simde_mm256_adds_epi16(x02t, x13t);
  xt1 = simde_mm256_subs_epi16(x02t, x13t);
  yt0 = simde_mm256_unpacklo_epi32(xt0, xt1);
  yt1 = simde_mm256_unpackhi_epi32(xt0, xt1);
  yt2 = simde_mm256_unpacklo_epi32(xt2, xt3);
  yt3 = simde_mm256_unpackhi_epi32(xt2, xt3);
  xt0 = simde_mm256_unpacklo_epi64(yt0, yt2);
  xt1 = simde_mm256_unpackhi_epi64(yt0, yt2);
  xt2 = simde_mm256_unpacklo_epi64(yt1, yt3);
  xt3 = simde_mm256_unpackhi_epi64(yt1, yt3);
  xt1 = packed_cmult2_256(xt1, tw16a[0], tw16b[0]);
  xt2 = packed_cmult2_256(xt2, tw16a[1], tw16b[1]);
  xt3 = packed_cmult2_256(xt3, tw16a[2], tw16b[2]);
  x02t = simde_mm256_adds_epi16(xt0, xt2);
  x13t = simde_mm256_adds_epi16(xt1, xt3);
  yt0 = simde_mm256_adds_epi16(x02t, x13t);
  yt2 = simde_mm256_subs_epi16(x02t, x13t);
  x1f = cflip(xt1); x3f = cflip(xt3);
  x02t = simde_mm256_subs_epi16(xt0, xt2);
  x13t = simde_mm256_subs_epi16(x1f, x3f);
  yt3 = simde_mm256_adds_epi16(x02t, x13t);
  yt1 = simde_mm256_subs_epi16(x02t, x13t);
  y256[0] = simde_mm256_insertf128_si256(yt0, simde_mm256_extracti128_si256(yt1, 0), 1);
  y256[1] = simde_mm256_insertf128_si256(yt2, simde_mm256_extracti128_si256(yt3, 0), 1);
  y256[2] = simde_mm256_insertf128_si256(yt1, simde_mm256_extracti128_si256(yt0, 1), 0);
  y256[3] = simde_mm256_insertf128_si256(yt3, simde_mm256_extracti128_si256(yt2, 1), 0);
}
/* idft64 input reorder: permutevar8x32(perm={7,3,5,1,6,2,4,0}) + unpacklo/hi_epi64
 * over x256[0..7] -> xtmp[0..7] */
static void reorder64_simde(const int16_t *x, int16_t *xtmp)
{
  const simde__m256i *x256 = (const simde__m256i *)x;
  simde__m256i *xt = (simde__m256i *)xtmp;
  const simde__m256i pm = simde_mm256_set_epi32(7, 3, 5, 1, 6, 2, 4, 0);
  simde__m256i xi[8];
  for (int i = 0; i < 8; i++) xi[i] = simde_mm256_permutevar8x32_epi32(x256[i], pm);
  xt[0] = simde_mm256_unpacklo_epi64(xi[0], xi[1]);
  xt[4] = simde_mm256_unpackhi_epi64(xi[0], xi[1]);
  xt[1] = simde_mm256_unpacklo_epi64(xi[2], xi[3]);
  xt[5] = simde_mm256_unpackhi_epi64(xi[2], xi[3]);
  xt[2] = simde_mm256_unpacklo_epi64(xi[4], xi[5]);
  xt[6] = simde_mm256_unpackhi_epi64(xi[4], xi[5]);
  xt[3] = simde_mm256_unpacklo_epi64(xi[6], xi[7]);
  xt[7] = simde_mm256_unpackhi_epi64(xi[6], xi[7]);
}

/* ================= RVV ports ================= */
#if defined(__riscv) && defined(__riscv_vector)
/* packed_cmult2: cre=madd(a,b), cim=madd(a,b2); out=(sat(cre>>15),sat(cim>>15)) */
static void packed_cmult2_rvv(const int16_t *a, const int16_t *b, const int16_t *b2, int16_t *out, size_t n)
{
  for (size_t c = 0; c < n;) {
    size_t vl = __riscv_vsetvl_e16m1(n - c); size_t o = 2 * c;
    vint16m1x2_t A = __riscv_vlseg2e16_v_i16m1x2(a + o, vl);
    vint16m1x2_t B = __riscv_vlseg2e16_v_i16m1x2(b + o, vl);
    vint16m1x2_t B2 = __riscv_vlseg2e16_v_i16m1x2(b2 + o, vl);
    vint16m1_t ar = __riscv_vget_v_i16m1x2_i16m1(A, 0), ai = __riscv_vget_v_i16m1x2_i16m1(A, 1);
    vint32m2_t cre = __riscv_vwmacc_vv_i32m2(__riscv_vwmul_vv_i32m2(ar, __riscv_vget_v_i16m1x2_i16m1(B, 0), vl),
                                             ai, __riscv_vget_v_i16m1x2_i16m1(B, 1), vl);
    vint32m2_t cim = __riscv_vwmacc_vv_i32m2(__riscv_vwmul_vv_i32m2(ar, __riscv_vget_v_i16m1x2_i16m1(B2, 0), vl),
                                             ai, __riscv_vget_v_i16m1x2_i16m1(B2, 1), vl);
    __riscv_vsseg2e16_v_i16m1x2(out + o, __riscv_vcreate_v_i16m1x2(
        __riscv_vnclip_wx_i16m1(cre, 15, __RISCV_VXRM_RDN, vl),
        __riscv_vnclip_wx_i16m1(cim, 15, __RISCV_VXRM_RDN, vl)), vl);
    c += vl;
  }
}
/* helper: load twiddle-mult result of xk as re/im (packed_cmult2 in registers) */
#define CMUL2(XP, BP, B2P, RE, IM) do { \
  vint16m1x2_t X_ = __riscv_vlseg2e16_v_i16m1x2((XP) + o, vl); \
  vint16m1x2_t B_ = __riscv_vlseg2e16_v_i16m1x2((BP) + o, vl); \
  vint16m1x2_t C_ = __riscv_vlseg2e16_v_i16m1x2((B2P) + o, vl); \
  vint16m1_t xr_ = __riscv_vget_v_i16m1x2_i16m1(X_, 0), xi_ = __riscv_vget_v_i16m1x2_i16m1(X_, 1); \
  vint32m2_t re_ = __riscv_vwmacc_vv_i32m2(__riscv_vwmul_vv_i32m2(xr_, __riscv_vget_v_i16m1x2_i16m1(B_,0), vl), xi_, __riscv_vget_v_i16m1x2_i16m1(B_,1), vl); \
  vint32m2_t im_ = __riscv_vwmacc_vv_i32m2(__riscv_vwmul_vv_i32m2(xr_, __riscv_vget_v_i16m1x2_i16m1(C_,0), vl), xi_, __riscv_vget_v_i16m1x2_i16m1(C_,1), vl); \
  RE = __riscv_vnclip_wx_i16m1(re_, 15, __RISCV_VXRM_RDN, vl); \
  IM = __riscv_vnclip_wx_i16m1(im_, 15, __RISCV_VXRM_RDN, vl); \
} while (0)

/* ibfly4_16: x1t/x2t/x3t = cmult2(xk,twk,twkb); radix-4 with flip=(im,-re). */
static void ibfly4_16_rvv(const int16_t *x0, const int16_t *x1, const int16_t *x2, const int16_t *x3,
                          int16_t *y0, int16_t *y1, int16_t *y2, int16_t *y3,
                          const int16_t *tw1, const int16_t *tw2, const int16_t *tw3,
                          const int16_t *tw1b, const int16_t *tw2b, const int16_t *tw3b, size_t n)
{
  for (size_t c = 0; c < n;) {
    size_t vl = __riscv_vsetvl_e16m1(n - c); size_t o = 2 * c;
    vint16m1x2_t X0 = __riscv_vlseg2e16_v_i16m1x2(x0 + o, vl);
    vint16m1_t x0r = __riscv_vget_v_i16m1x2_i16m1(X0, 0), x0i = __riscv_vget_v_i16m1x2_i16m1(X0, 1);
    vint16m1_t x1r, x1i, x2r, x2i, x3r, x3i;
    CMUL2(x1, tw1, tw1b, x1r, x1i);
    CMUL2(x2, tw2, tw2b, x2r, x2i);
    CMUL2(x3, tw3, tw3b, x3r, x3i);
    /* y0 = (x0+x2t)+(x1t+x3t) ; y2 = (x0+x2t)-(x1t+x3t) (saturating) */
    vint16m1_t ar = __riscv_vsadd_vv_i16m1(x0r, x2r, vl), ai = __riscv_vsadd_vv_i16m1(x0i, x2i, vl);
    vint16m1_t br = __riscv_vsadd_vv_i16m1(x1r, x3r, vl), bi = __riscv_vsadd_vv_i16m1(x1i, x3i, vl);
    __riscv_vsseg2e16_v_i16m1x2(y0 + o, __riscv_vcreate_v_i16m1x2(__riscv_vsadd_vv_i16m1(ar, br, vl), __riscv_vsadd_vv_i16m1(ai, bi, vl)), vl);
    __riscv_vsseg2e16_v_i16m1x2(y2 + o, __riscv_vcreate_v_i16m1x2(__riscv_vssub_vv_i16m1(ar, br, vl), __riscv_vssub_vv_i16m1(ai, bi, vl)), vl);
    /* flip(xk) = (xk.im, -xk.re) [-re wrapping]. y3=(x0-x2t)+(x1f-x3f); y1=(x0-x2t)-(x1f-x3f) */
    vint16m1_t x1fr = x1i, x1fi = __riscv_vneg_v_i16m1(x1r, vl);
    vint16m1_t x3fr = x3i, x3fi = __riscv_vneg_v_i16m1(x3r, vl);
    vint16m1_t cr = __riscv_vssub_vv_i16m1(x0r, x2r, vl), ci = __riscv_vssub_vv_i16m1(x0i, x2i, vl);
    vint16m1_t dr = __riscv_vssub_vv_i16m1(x1fr, x3fr, vl), di = __riscv_vssub_vv_i16m1(x1fi, x3fi, vl);
    __riscv_vsseg2e16_v_i16m1x2(y3 + o, __riscv_vcreate_v_i16m1x2(__riscv_vsadd_vv_i16m1(cr, dr, vl), __riscv_vsadd_vv_i16m1(ci, di, vl)), vl);
    __riscv_vsseg2e16_v_i16m1x2(y1 + o, __riscv_vcreate_v_i16m1x2(__riscv_vssub_vv_i16m1(cr, dr, vl), __riscv_vssub_vv_i16m1(ci, di, vl)), vl);
    c += vl;
  }
}

/* gather 8 complex per output vector via vluxei32, index in bytes (=4*complex) */
static inline void gather4(const uint32_t *src, uint32_t *dst, const uint16_t *bidx /*32 byte-offsets*/)
{
  size_t vl = __riscv_vsetvl_e32m1(8);
  for (int v = 0; v < 4; v++) {
    vuint16mf2_t idx = __riscv_vle16_v_u16mf2(bidx + 8 * v, vl); /* byte offsets (EMUL=mf2) */
    vuint32m1_t g = __riscv_vluxei16_v_u32m1(src, idx, vl);
    __riscv_vse32_v_u32m1(dst + 8 * v, g, vl);
  }
}

/* idft16 in RVV: stage1 (elementwise) -> reorder1 -> stage2 (twiddle+bfly) -> reorder2.
 * Reorders are fixed complex-index gathers (derived from the SIMDe unpack seq). */
static const uint16_t idx_r1[32] = { /* bytes: 4*flat_src, flat=8*vec+pos */
  0,32,64,96,16,48,80,112, 4,36,68,100,20,52,84,116, 8,40,72,104,24,56,88,120, 12,44,76,108,28,60,92,124};
static const uint16_t idx_r2[32] = {
  0,4,8,12,32,36,40,44, 64,68,72,76,96,100,104,108, 16,20,24,28,48,52,56,60, 80,84,88,92,112,116,120,124};
static void stage_bfly_rvv(const int16_t *x0, const int16_t *x1, const int16_t *x2, const int16_t *x3,
                           int16_t *y0, int16_t *y1, int16_t *y2, int16_t *y3, size_t n)
{ /* radix-4 with NO input twiddles (stage 1) */
  for (size_t c = 0; c < n;) {
    size_t vl = __riscv_vsetvl_e16m1(n - c); size_t o = 2 * c;
    vint16m1x2_t X0 = __riscv_vlseg2e16_v_i16m1x2(x0 + o, vl), X1 = __riscv_vlseg2e16_v_i16m1x2(x1 + o, vl);
    vint16m1x2_t X2 = __riscv_vlseg2e16_v_i16m1x2(x2 + o, vl), X3 = __riscv_vlseg2e16_v_i16m1x2(x3 + o, vl);
    vint16m1_t x0r = __riscv_vget_v_i16m1x2_i16m1(X0,0), x0i = __riscv_vget_v_i16m1x2_i16m1(X0,1);
    vint16m1_t x1r = __riscv_vget_v_i16m1x2_i16m1(X1,0), x1i = __riscv_vget_v_i16m1x2_i16m1(X1,1);
    vint16m1_t x2r = __riscv_vget_v_i16m1x2_i16m1(X2,0), x2i = __riscv_vget_v_i16m1x2_i16m1(X2,1);
    vint16m1_t x3r = __riscv_vget_v_i16m1x2_i16m1(X3,0), x3i = __riscv_vget_v_i16m1x2_i16m1(X3,1);
    vint16m1_t ar = __riscv_vsadd_vv_i16m1(x0r, x2r, vl), ai = __riscv_vsadd_vv_i16m1(x0i, x2i, vl);
    vint16m1_t br = __riscv_vsadd_vv_i16m1(x1r, x3r, vl), bi = __riscv_vsadd_vv_i16m1(x1i, x3i, vl);
    __riscv_vsseg2e16_v_i16m1x2(y0 + o, __riscv_vcreate_v_i16m1x2(__riscv_vsadd_vv_i16m1(ar,br,vl), __riscv_vsadd_vv_i16m1(ai,bi,vl)), vl);
    __riscv_vsseg2e16_v_i16m1x2(y2 + o, __riscv_vcreate_v_i16m1x2(__riscv_vssub_vv_i16m1(ar,br,vl), __riscv_vssub_vv_i16m1(ai,bi,vl)), vl);
    vint16m1_t x1fr = x1i, x1fi = __riscv_vneg_v_i16m1(x1r, vl);
    vint16m1_t x3fr = x3i, x3fi = __riscv_vneg_v_i16m1(x3r, vl);
    vint16m1_t cr = __riscv_vssub_vv_i16m1(x0r, x2r, vl), ci = __riscv_vssub_vv_i16m1(x0i, x2i, vl);
    vint16m1_t dr = __riscv_vssub_vv_i16m1(x1fr, x3fr, vl), di = __riscv_vssub_vv_i16m1(x1fi, x3fi, vl);
    __riscv_vsseg2e16_v_i16m1x2(y3 + o, __riscv_vcreate_v_i16m1x2(__riscv_vsadd_vv_i16m1(cr,dr,vl), __riscv_vsadd_vv_i16m1(ci,di,vl)), vl);
    __riscv_vsseg2e16_v_i16m1x2(y1 + o, __riscv_vcreate_v_i16m1x2(__riscv_vssub_vv_i16m1(cr,dr,vl), __riscv_vssub_vv_i16m1(ci,di,vl)), vl);
    c += vl;
  }
}
static void idft16_rvv(const int16_t *x, int16_t *y)
{
  int16_t s1[64], r1[64], s2[64]; /* 32 complex each */
  /* stage 1: x256[0..3] -> s1[0..3] (each 8 complex) */
  stage_bfly_rvv(x + 0, x + 16, x + 32, x + 48, s1 + 0, s1 + 16, s1 + 32, s1 + 48, 8);
  /* reorder1: s1 -> r1 (complex gather) */
  gather4((const uint32_t *)s1, (uint32_t *)r1, idx_r1);
  /* stage 2: twiddle mult on r1[1],r1[2],r1[3] then radix-4 */
  ibfly4_16_rvv(r1 + 0, r1 + 16, r1 + 32, r1 + 48, s2 + 0, s2 + 16, s2 + 32, s2 + 48,
                tw16rep + 0, tw16rep + 16, tw16rep + 32, tw16crep + 0, tw16crep + 16, tw16crep + 32, 8);
  /* reorder2: s2 -> y (complex gather) */
  gather4((const uint32_t *)s2, (uint32_t *)y, idx_r2);
}

/* idft64 input reorder: 8 vectors -> 8 xtmp vectors (fixed complex permutation) */
static const uint16_t idx_r64[64] = { /* bytes = 4*flat_src, flat = 8*vec+pos over 8 vecs */
  0,16,32,48,4,20,36,52, 64,80,96,112,68,84,100,116, 128,144,160,176,132,148,164,180, 192,208,224,240,196,212,228,244,
  8,24,40,56,12,28,44,60, 72,88,104,120,76,92,108,124, 136,152,168,184,140,156,172,188, 200,216,232,248,204,220,236,252};
static void reorder64_rvv(const int16_t *x, int16_t *xtmp)
{
  size_t vl = __riscv_vsetvl_e32m1(8);
  for (int v = 0; v < 8; v++) {
    vuint16mf2_t idx = __riscv_vle16_v_u16mf2(idx_r64 + 8 * v, vl);
    vuint32m1_t g = __riscv_vluxei16_v_u32m1((const uint32_t *)x, idx, vl);
    __riscv_vse32_v_u32m1((uint32_t *)xtmp + 8 * v, g, vl);
  }
}
#endif

/* ================= test driver ================= */
int main(int argc, char **argv)
{
  int cpu = (argc > 1) ? atoi(argv[1]) : -1;
  if (cpu >= 0) { if (cpu > 7) use_ai_core(); if (pin_to_cpu(cpu) != 0) fprintf(stderr, "warn: no pin %d\n", cpu); }
  enum { N = 100000 };
  long mc = 0, mb = 0, m16 = 0, m64 = 0;
  int16_t a[16], b[16], b2[16], out_s[16], out_r[16];
  int16_t x0[16], x1[16], x2[16], x3[16], t1[16], t2[16], t3[16], t1b[16], t2b[16], t3b[16];
  int16_t y0s[16], y1s[16], y2s[16], y3s[16], y0r[16], y1r[16], y2r[16], y3r[16];
  int16_t xi16[64], ys16[64], yr16[64];
  int16_t xi64[128], xs64[128], xr64[128];
  for (int t = 0; t < N; t++) {
    for (int i = 0; i < 16; i++) { a[i]=rnd16(); b[i]=rnd16(); b2[i]=rnd16(); x0[i]=rnd16(); x1[i]=rnd16(); x2[i]=rnd16(); x3[i]=rnd16();
      t1[i]=rnd16(); t2[i]=rnd16(); t3[i]=rnd16(); t1b[i]=rnd16(); t2b[i]=rnd16(); t3b[i]=rnd16(); }
    for (int i = 0; i < 64; i++) xi16[i] = rnd16();
    for (int i = 0; i < 128; i++) xi64[i] = rnd16();
    /* packed_cmult2 */
    simde__m256i r = packed_cmult2_256(simde_mm256_loadu_si256((simde__m256i*)a), simde_mm256_loadu_si256((simde__m256i*)b), simde_mm256_loadu_si256((simde__m256i*)b2));
    simde_mm256_storeu_si256((simde__m256i*)out_s, r);
    /* ibfly4_16 */
    ibfly4_16_simde((simde__m256i*)x0,(simde__m256i*)x1,(simde__m256i*)x2,(simde__m256i*)x3,(simde__m256i*)y0s,(simde__m256i*)y1s,(simde__m256i*)y2s,(simde__m256i*)y3s,
                    (simde__m256i*)t1,(simde__m256i*)t2,(simde__m256i*)t3,(simde__m256i*)t1b,(simde__m256i*)t2b,(simde__m256i*)t3b);
    idft16_simde(xi16, ys16);
    reorder64_simde(xi64, xs64);
#if defined(__riscv) && defined(__riscv_vector)
    packed_cmult2_rvv(a, b, b2, out_r, 8);
    if (memcmp(out_s, out_r, 32)) mc++;
    ibfly4_16_rvv(x0,x1,x2,x3,y0r,y1r,y2r,y3r,t1,t2,t3,t1b,t2b,t3b,8);
    if (memcmp(y0s,y0r,32)||memcmp(y1s,y1r,32)||memcmp(y2s,y2r,32)||memcmp(y3s,y3r,32)) mb++;
    idft16_rvv(xi16, yr16);
    if (memcmp(ys16, yr16, 128)) m16++;
    reorder64_rvv(xi64, xr64);
    if (memcmp(xs64, xr64, 256)) m64++;
#endif
  }
  printf("correctness (%d random trials):\n", N);
#if defined(__riscv) && defined(__riscv_vector)
  printf("  packed_cmult2 : %s (%ld)\n", mc ? "FAIL" : "OK", mc);
  printf("  ibfly4_16     : %s (%ld)\n", mb ? "FAIL" : "OK", mb);
  printf("  idft16        : %s (%ld)\n", m16 ? "FAIL" : "OK", m16);
  printf("  reorder64     : %s (%ld)\n", m64 ? "FAIL" : "OK", m64);
#else
  printf("  (rvv not built)\n");
#endif
  return (mc || mb || m16 || m64) ? 1 : 0;
}
