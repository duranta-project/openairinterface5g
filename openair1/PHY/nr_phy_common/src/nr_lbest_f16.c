/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * FP16 2-layer receivers: the L-best / full ML of the fp32 kernels (nr_lbest_simd.c: seed, 3x3 window, adaptive
 * full grid, graded fallback, LLR units) with the candidate metric taken relative to the seed point, and a fused
 * inner RX that also runs the MRC and Gram in FP16. Written once against a small FP16 vector layer: ARMv8.2 FP16
 * (float16x8_t, 8 REs per vector) and AVX512-FP16 (__m512h, 32 REs per vector); the build compiles this file with
 * the matching -march flags and defines NR_LBEST_F16 (nr_compute_llr.c selects it at run time).
 *
 * Metric: with x0 the seed point (window centre and its conditional nuisance slice), r = z - G x0 once per RE and
 * per candidate x0 + d: dM = Re{d^H r} - 1/2 d^H G d. The max-log differences of M are unchanged, but the per-
 * candidate terms are small (d: a few level steps), which FP16's 11-bit significand resolves: the absolute metric
 * Re{x^H z} - 1/2 x^H G x loses 25-35% more slots at 256QAM in FP16, the relative one matches fp32 (nr_ulsim,
 * TDL-A, 2/4 Rx). Each RE is scaled by its own powers of two (max(P0, P1) in [8, 16), far from the FP16 overflow at
 * 65504) and its LLRs are converted back in fp32, so the REs are independent of their vector.
 */

#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "nr_lbest_f16.h"
#include "nr_compute_llr.h"
#include "nr_lbest_simd.h"
// the SIMD per-RE LLR store of the int16 kernels (nrlbw_store_llr) at the vector width of this file
#if defined(__x86_64__)
#include <simde/x86/avx512.h>
#define NRLB_W 512
#else
#define NRLB_W 128
#endif
#include "nr_lbest_simd_width.h"

// ---------------------------------------------------------------------------------------------------------------------
// FP16 vector layer: hv = FP16 values, hm = lane masks, hi = int16 values, NRH_N lanes (REs) per vector
// ---------------------------------------------------------------------------------------------------------------------
#if defined(__aarch64__) && defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
#include <arm_neon.h>
#define NRH_N 8
typedef float16x8_t hv;
typedef uint16x8_t hm;
typedef int16x8_t hi;
static inline hv hv_set1(float x) { return vdupq_n_f16(x); }
static inline hv hv_add(hv a, hv b) { return vaddq_f16(a, b); }
static inline hv hv_sub(hv a, hv b) { return vsubq_f16(a, b); }
static inline hv hv_mul(hv a, hv b) { return vmulq_f16(a, b); }
static inline hv hv_div(hv a, hv b) { return vdivq_f16(a, b); }
static inline hv hv_fma(hv a, hv b, hv c) { return vfmaq_f16(a, b, c); } // a + b c
static inline hv hv_fms(hv a, hv b, hv c) { return vfmsq_f16(a, b, c); } // a - b c
static inline hv hv_max(hv a, hv b) { return vmaxq_f16(a, b); }
static inline hv hv_min(hv a, hv b) { return vminq_f16(a, b); }
static inline hv hv_abs(hv a) { return vabsq_f16(a); }
static inline hv hv_neg(hv a) { return vnegq_f16(a); }
static inline hv hv_rndn(hv a) { return vrndnq_f16(a); } // to nearest, ties to even
static inline hm hv_lt(hv a, hv b) { return vcltq_f16(a, b); }
static inline hm hv_gt(hv a, hv b) { return vcgtq_f16(a, b); }
static inline hm hv_eq(hv a, hv b) { return vceqq_f16(a, b); }
static inline hm hm_or(hm a, hm b) { return vorrq_u16(a, b); }
static inline hm hm_andnot(hm a, hm w) { return vbicq_u16(a, w); } // a & ~w
static inline int hm_any(hm a) { return vmaxvq_u16(a) != 0; }
static inline hv hv_sel(hm m, hv a, hv b) { return vbslq_f16(m, a, b); } // m ? a : b
static inline hi hi_sel(hm m, hi a, hi b) { return vbslq_s16(m, a, b); }
static inline hi hi_set1(int16_t x) { return vdupq_n_s16(x); }
static inline hi hi_add(hi a, hi b) { return vaddq_s16(a, b); }
static inline hi hi_sub(hi a, hi b) { return vsubq_s16(a, b); }
static inline hi hi_min(hi a, hi b) { return vminq_s16(a, b); }
static inline hi hi_max(hi a, hi b) { return vmaxq_s16(a, b); }
static inline hi hi_shr1(hi a) { return vshrq_n_s16(a, 1); }
static inline hi hi_shl1(hi a) { return vshlq_n_s16(a, 1); }
static inline hv hv_cvt(hi a) { return vcvtq_f16_s16(a); }
static inline hi hv_expo(hv a) { return vshrq_n_s16(vreinterpretq_s16_f16(a), 10); } // exponent field (a >= 0)
static inline hv hv_pow2(hi k) // 2^k, k in the normal FP16 range
{
  return vreinterpretq_f16_s16(vshlq_n_s16(vmaxq_s16(vminq_s16(vaddq_s16(k, vdupq_n_s16(15)), vdupq_n_s16(30)), vdupq_n_s16(1)), 10));
}
static inline void hi_load_c16(const c16_t *p, hi *re, hi *im) // NRH_N complex, deinterleaved
{
  const int16x8x2_t v = vld2q_s16(&p->r);
  *re = v.val[0];
  *im = v.val[1];
}
static inline simde__m128i hi_simde(hi a) { return simde__m128i_from_neon_i16(a); }
// per-RE fp32 scales 2^k (low / high lanes)
typedef struct {
  float32x4_t lo, hi;
} hs;
static inline hs hs_pow2(hi k)
{
  return (hs){vreinterpretq_f32_s32(vshlq_n_s32(vaddq_s32(vmovl_s16(vget_low_s16(k)), vdupq_n_s32(127)), 23)),
              vreinterpretq_f32_s32(vshlq_n_s32(vaddq_s32(vmovl_high_s16(k), vdupq_n_s32(127)), 23))};
}
static inline hi hv_trunc16(hv a) { return vcvtq_s16_f16(a); } // toward zero
// lambda * up as FP16 (fp32, clamped below the FP16 overflow)
static inline hv hv_lam(float lam, const hs *up)
{
  const float32x4_t mx = vdupq_n_f32(30000.0f);
  return vcombine_f16(vcvt_f16_f32(vminq_f32(vmulq_n_f32(up->lo, lam), mx)), vcvt_f16_f32(vminq_f32(vmulq_n_f32(up->hi, lam), mx)));
}

#elif defined(__x86_64__) && defined(__AVX512FP16__)
#include <immintrin.h>
#define NRH_N 32
typedef __m512h hv;
typedef __mmask32 hm;
typedef __m512i hi;
static inline hv hv_set1(float x) { return _mm512_set1_ph((_Float16)x); }
static inline hv hv_add(hv a, hv b) { return _mm512_add_ph(a, b); }
static inline hv hv_sub(hv a, hv b) { return _mm512_sub_ph(a, b); }
static inline hv hv_mul(hv a, hv b) { return _mm512_mul_ph(a, b); }
static inline hv hv_div(hv a, hv b) { return _mm512_div_ph(a, b); }
static inline hv hv_fma(hv a, hv b, hv c) { return _mm512_fmadd_ph(b, c, a); } // a + b c
static inline hv hv_fms(hv a, hv b, hv c) { return _mm512_fnmadd_ph(b, c, a); } // a - b c
static inline hv hv_max(hv a, hv b) { return _mm512_max_ph(a, b); }
static inline hv hv_min(hv a, hv b) { return _mm512_min_ph(a, b); }
static inline hv hv_abs(hv a) { return _mm512_abs_ph(a); }
static inline hv hv_neg(hv a) { return _mm512_castsi512_ph(_mm512_xor_si512(_mm512_castph_si512(a), _mm512_set1_epi16((short)0x8000))); }
static inline hv hv_rndn(hv a) { return _mm512_roundscale_ph(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC); }
static inline hm hv_lt(hv a, hv b) { return _mm512_cmp_ph_mask(a, b, _CMP_LT_OQ); }
static inline hm hv_gt(hv a, hv b) { return _mm512_cmp_ph_mask(a, b, _CMP_GT_OQ); }
static inline hm hv_eq(hv a, hv b) { return _mm512_cmp_ph_mask(a, b, _CMP_EQ_OQ); }
static inline hm hm_or(hm a, hm b) { return a | b; }
static inline hm hm_andnot(hm a, hm w) { return a & ~w; }
static inline int hm_any(hm a) { return a != 0; }
static inline hv hv_sel(hm m, hv a, hv b) { return _mm512_mask_blend_ph(m, b, a); }
static inline hi hi_sel(hm m, hi a, hi b) { return _mm512_mask_blend_epi16(m, b, a); }
static inline hi hi_set1(int16_t x) { return _mm512_set1_epi16(x); }
static inline hi hi_add(hi a, hi b) { return _mm512_add_epi16(a, b); }
static inline hi hi_sub(hi a, hi b) { return _mm512_sub_epi16(a, b); }
static inline hi hi_min(hi a, hi b) { return _mm512_min_epi16(a, b); }
static inline hi hi_max(hi a, hi b) { return _mm512_max_epi16(a, b); }
static inline hi hi_shr1(hi a) { return _mm512_srai_epi16(a, 1); }
static inline hi hi_shl1(hi a) { return _mm512_slli_epi16(a, 1); }
static inline hv hv_cvt(hi a) { return _mm512_cvtepi16_ph(a); }
static inline hi hv_expo(hv a) { return _mm512_srai_epi16(_mm512_castph_si512(a), 10); }
static inline hv hv_pow2(hi k)
{
  return _mm512_castsi512_ph(_mm512_slli_epi16(_mm512_max_epi16(_mm512_min_epi16(_mm512_add_epi16(k, _mm512_set1_epi16(15)), _mm512_set1_epi16(30)), _mm512_set1_epi16(1)), 10));
}
// even / odd int16 of two vectors (the re / im of 32 interleaved complex)
static inline hi hi_even(hi a, hi b)
{
  const hi idx = _mm512_set_epi16(62, 60, 58, 56, 54, 52, 50, 48, 46, 44, 42, 40, 38, 36, 34, 32, 30, 28, 26, 24, 22, 20, 18, 16, 14, 12, 10, 8, 6, 4, 2, 0);
  return _mm512_permutex2var_epi16(a, idx, b);
}
static inline hi hi_odd(hi a, hi b)
{
  const hi idx = _mm512_set_epi16(63, 61, 59, 57, 55, 53, 51, 49, 47, 45, 43, 41, 39, 37, 35, 33, 31, 29, 27, 25, 23, 21, 19, 17, 15, 13, 11, 9, 7, 5, 3, 1);
  return _mm512_permutex2var_epi16(a, idx, b);
}
static inline void hi_load_c16(const c16_t *p, hi *re, hi *im)
{
  const hi a = _mm512_loadu_si512(p), b = _mm512_loadu_si512(p + 16);
  *re = hi_even(a, b);
  *im = hi_odd(a, b);
}
static inline simde__m512i hi_simde(hi a) { return a; }
typedef struct {
  __m512 lo, hi;
} hs;
static inline __m512 f32_pow2(__m256i k) { return _mm512_castsi512_ps(_mm512_slli_epi32(_mm512_add_epi32(_mm512_cvtepi16_epi32(k), _mm512_set1_epi32(127)), 23)); }
static inline hs hs_pow2(hi k) { return (hs){f32_pow2(_mm512_castsi512_si256(k)), f32_pow2(_mm512_extracti64x4_epi64(k, 1))}; }
static inline hi hv_trunc16(hv a) { return _mm512_cvttph_epi16(a); }
static inline hv hv_lam(float lam, const hs *up)
{
  const __m512 mx = _mm512_set1_ps(30000.0f), l = _mm512_set1_ps(lam);
  const __m256h a = _mm512_cvtxps_ph(_mm512_min_ps(_mm512_mul_ps(up->lo, l), mx));
  const __m256h b = _mm512_cvtxps_ph(_mm512_min_ps(_mm512_mul_ps(up->hi, l), mx));
  return _mm512_castsi512_ph(_mm512_inserti64x4(_mm512_castsi256_si512(_mm256_castph_si256(a)), _mm256_castph_si256(b), 1));
}
#else
#error "nr_lbest_f16.c needs ARMv8.2 FP16 vector arithmetic or AVX512-FP16"
#endif

// ---------------------------------------------------------------------------------------------------------------------
// detector core
// ---------------------------------------------------------------------------------------------------------------------
static inline hv nrh_round_odd(hv x) // 2 * round((x - 1) / 2) + 1
{
  const hv one = hv_set1(1.0f);
  return hv_fma(one, hv_rndn(hv_mul(hv_sub(x, one), hv_set1(0.5f))), hv_set1(2.0f));
}

static inline hv nrh_clamp(hv x, hv lim) // to [-lim, lim]
{
  return hv_min(hv_max(x, hv_neg(lim)), lim);
}

static inline hv nrh_clamp2(hv x, float lo, float hi_)
{
  return hv_min(hv_max(x, hv_set1(lo)), hv_set1(hi_));
}

static inline hv nrh_dsq(hv lvl, hv ref)
{
  const hv d = hv_sub(lvl, ref);
  return hv_mul(d, d);
}

// LLRs of the vector's units to int16 at 2^-k per RE: two exact power-of-two factors (-k = a + b, each in the FP16
// exponent range), a clamp below the int16 limit (32752: the FP16 value under 32767) and the truncating conversion
typedef struct {
  hv a, b;
} hsc;
static inline hsc nrh_llr_scale(hi k)
{
  const hi nk = hi_sub(hi_set1(0), k);
  const hi a = hi_max(hi_min(nk, hi_set1(15)), hi_set1(-14));
  return (hsc){hv_pow2(a), hv_pow2(hi_sub(nk, a))};
}
static inline hi nrh_llr16(hv l, const hsc *sc)
{
  return hv_trunc16(nrh_clamp(hv_mul(hv_mul(l, sc->a), sc->b), hv_set1(32752.0f)));
}

// Gray bits of a PAM level vector, nb bits per axis
static inline __attribute__((always_inline)) void nrh_bits(hv lvl, int nb, hm b[4])
{
  b[0] = hv_lt(lvl, hv_set1(0.0f));
  hv r = hv_abs(lvl);
  float t = (float)(1 << (nb - 1));
  for (int k = 1; k < nb; k++) {
    const hv tv = hv_set1(t);
    b[k] = hv_gt(r, tv);
    r = hv_abs(hv_sub(r, tv));
    t *= 0.5f;
  }
}

typedef struct {
  hv r1r, r1i, r2r, r2i; // residuals z - G x0
  hv rr, ri, hP0, hP1, sl; // rho, P/2, nuisance slice scale sqrt(K)/P1
  hv cI, cQ, c2I, c2Q; // x0 levels: window centre, nuisance slice at the centre
} nrh_re_t;

// candidate grid around x0: the 3x3 window (clamped as the other kernels) or all M levels; conditional nuisance
// slice and max-log per axis. llr in the vector's metric units, uns: one side of the bit has no candidate
static inline __attribute__((always_inline)) void nrh_grid(const nrh_re_t *e, int full, int nb, hv llr[8], hm uns[8])
{
  const int M = 1 << nb, nI = full ? M : 3;
  const hv LMAX = hv_set1((float)(M - 1));
  const hv NA = hv_set1(nb == 3 ? 1.0f / 6.48074069840786f : 1.0f / 13.038404810405298f);
  const hv NEG = hv_set1(-INFINITY);
  hv dI[16], dQ[16];
  hm bI[16][4], bQ[16][4];
  for (int k = 0; k < nI; k++) {
    hv lI, lQ;
    if (full) {
      lI = lQ = hv_set1((float)(2 * k - (M - 1)));
    } else {
      const hv o = hv_set1((float)(2 * k - 2));
      lI = nrh_clamp(hv_add(e->cI, o), LMAX);
      lQ = nrh_clamp(hv_add(e->cQ, o), LMAX);
    }
    nrh_bits(lI, nb, bI[k]);
    nrh_bits(lQ, nb, bQ[k]);
    dI[k] = hv_mul(hv_sub(lI, e->cI), NA); // offsets from x0 (exact level differences, one rounding)
    dQ[k] = hv_mul(hv_sub(lQ, e->cQ), NA);
  }
  hv rowMax[16], colMax[16];
  for (int k = 0; k < nI; k++)
    rowMax[k] = colMax[k] = NEG;
  for (int a = 0; a < nI; a++) {
    const hv d1r = dI[a];
    for (int b = 0; b < nI; b++) {
      const hv d1i = dQ[b];
      // Re{d1* r1} - P0 |d1|^2 / 2
      hv m = hv_fma(hv_mul(d1r, e->r1r), d1i, e->r1i);
      m = hv_fms(m, e->hP0, hv_fma(hv_mul(d1r, d1r), d1i, d1i));
      // nuisance x2 = Q(x0_2 + (r2 - conj(rho) d1) / P1)
      const hv cr = hv_fma(hv_mul(e->rr, d1r), e->ri, d1i);
      const hv ci = hv_fms(hv_mul(e->rr, d1i), e->ri, d1r);
      const hv I2 = nrh_clamp(nrh_round_odd(hv_fma(e->c2I, hv_sub(e->r2r, cr), e->sl)), LMAX);
      const hv Q2 = nrh_clamp(nrh_round_odd(hv_fma(e->c2Q, hv_sub(e->r2i, ci), e->sl)), LMAX);
      const hv d2r = hv_mul(hv_sub(I2, e->c2I), NA), d2i = hv_mul(hv_sub(Q2, e->c2Q), NA);
      m = hv_fma(m, d2r, e->r2r);
      m = hv_fma(m, d2i, e->r2i);
      m = hv_fms(m, e->hP1, hv_fma(hv_mul(d2r, d2r), d2i, d2i));
      // - Re{d1* rho d2}
      const hv cxr = hv_fma(hv_mul(d1r, d2r), d1i, d2i);
      const hv cxi = hv_fms(hv_mul(d1r, d2i), d1i, d2r);
      m = hv_sub(m, hv_fms(hv_mul(e->rr, cxr), e->ri, cxi));
      rowMax[a] = hv_max(rowMax[a], m);
      colMax[b] = hv_max(colMax[b], m);
    }
  }
  for (int k = 0; k < nb; k++) {
    hv m0I = NEG, m1I = NEG, m0Q = NEG, m1Q = NEG;
    for (int a = 0; a < nI; a++) {
      m0I = hv_max(m0I, hv_sel(bI[a][k], NEG, rowMax[a]));
      m1I = hv_max(m1I, hv_sel(bI[a][k], rowMax[a], NEG));
      m0Q = hv_max(m0Q, hv_sel(bQ[a][k], NEG, colMax[a]));
      m1Q = hv_max(m1Q, hv_sel(bQ[a][k], colMax[a], NEG));
    }
    llr[2 * k] = hv_sub(m0I, m1I);
    llr[2 * k + 1] = hv_sub(m0Q, m1Q);
    uns[2 * k] = hm_or(hv_eq(m0I, NEG), hv_eq(m1I, NEG));
    uns[2 * k + 1] = hm_or(hv_eq(m0Q, NEG), hv_eq(m1Q, NEG));
  }
}

// graded per-axis PAM LLRs of the target layer from its soft level estimate (nrlbw_axis_llr / nrlbw_axis_llr64): the
// fallback of the bits the 3x3 window does not span (the LSB is always spanned)
static inline __attribute__((always_inline)) void nrh_axis_llr(hv est, hv g, int nb, hv out[4])
{
  const hv a = hv_abs(est);
  const hv o = nrh_round_odd(est), oa = nrh_round_odd(a);
  const float L = (float)((1 << nb) - 1), H = (float)(1 << (nb - 1)); // top level, MSB-1 threshold
  out[0] = hv_mul(g, hv_sub(nrh_dsq(nrh_clamp2(o, -L, -1.0f), est), nrh_dsq(nrh_clamp2(o, 1.0f, L), est)));
  out[1] = hv_mul(g, hv_sub(nrh_dsq(nrh_clamp2(oa, H + 1.0f, L), a), nrh_dsq(nrh_clamp2(oa, 1.0f, H - 1.0f), a)));
  if (nb == 4) {
    const hv d0 = nrh_dsq(nrh_clamp2(oa, 5.0f, 11.0f), a);
    const hv d1 = hv_min(nrh_dsq(nrh_clamp2(oa, 1.0f, 3.0f), a), nrh_dsq(nrh_clamp2(oa, 13.0f, 15.0f), a));
    out[2] = hv_mul(g, hv_sub(d1, d0));
  } else {
    out[2] = hv_set1(0.0f);
  }
  out[3] = hv_set1(0.0f);
}

// one layer of a vector of REs: z_t / z_o target / other MRC, P_t / P_o, rho = h_t^H h_o, lam: lambda, all in the
// vector's units (= the int16 compensation units 2^k per RE); int16 LLRs (per-layer order) in the compensation units
static inline __attribute__((always_inline)) void nrh_layer(hv ztr, hv zti, hv zor, hv zoi, hv Pt, hv Po, hv rr, hv ri, hv lam, int full, int nb, const hsc *inv, hi res[8])
{
  const int M = 1 << nb, Qm = 2 * nb;
  const float sqK = nb == 3 ? 6.48074069840786f : 13.038404810405298f;
  const hv SK = hv_set1(sqK), NA = hv_set1(1.0f / sqK), LMAX = hv_set1((float)(M - 1));
  const hv ZERO = hv_set1(0.0f), HALF = hv_set1(0.5f);
  const hv FLOOR = hv_set1(1.0f / 256.0f); // det / P1s floors (P in [8, 16): ~1e-4 relative)
  // unbiased MMSE seed: x1 = ((P_o + lame) z_t - rho z_o) / det, lame = lambda + eps (P_t + P_o) / 2
  const hv lame = hv_fma(lam, hv_set1(0.5f * NR_2LAYER_CSI_EPS), hv_add(Pt, Po));
  const hv P1s = hv_add(Po, lame);
  const hv det = hv_max(hv_sub(hv_mul(Pt, P1s), hv_fma(hv_mul(rr, rr), ri, ri)), FLOOR);
  const hv idet = hv_div(SK, det);
  const hv estI = hv_mul(hv_sub(hv_mul(P1s, ztr), hv_fms(hv_mul(rr, zor), ri, zoi)), idet);
  const hv estQ = hv_mul(hv_sub(hv_mul(P1s, zti), hv_fma(hv_mul(rr, zoi), ri, zor)), idet);
  nrh_re_t e;
  e.rr = rr;
  e.ri = ri;
  e.hP0 = hv_mul(HALF, Pt);
  e.hP1 = hv_mul(HALF, Po);
  e.sl = hv_sel(hv_eq(Po, ZERO), ZERO, hv_div(SK, Po));
  e.cI = nrh_clamp(nrh_round_odd(estI), LMAX);
  e.cQ = nrh_clamp(nrh_round_odd(estQ), LMAX);
  const hv x1r = hv_mul(e.cI, NA), x1i = hv_mul(e.cQ, NA);
  const hv cr = hv_fma(hv_mul(rr, x1r), ri, x1i), ci = hv_fms(hv_mul(rr, x1i), ri, x1r);
  e.c2I = nrh_clamp(nrh_round_odd(hv_mul(hv_sub(zor, cr), e.sl)), LMAX);
  e.c2Q = nrh_clamp(nrh_round_odd(hv_mul(hv_sub(zoi, ci), e.sl)), LMAX);
  const hv x2r = hv_mul(e.c2I, NA), x2i = hv_mul(e.c2Q, NA);
  e.r1r = hv_sub(hv_fms(ztr, Pt, x1r), hv_fms(hv_mul(rr, x2r), ri, x2i));
  e.r1i = hv_sub(hv_fms(zti, Pt, x1i), hv_fma(hv_mul(rr, x2i), ri, x2r));
  e.r2r = hv_fms(hv_sub(zor, cr), Po, x2r);
  e.r2i = hv_fms(hv_sub(zoi, ci), Po, x2i);

  hv l[8];
  hm u[8];
  nrh_grid(&e, full, nb, l, u);
  hv axis[8];
  if (!full) {
    // adaptive window (nr_lbest_wide_re): the full grid on the low-SINR REs
    const hm wide = hv_lt(det, hv_mul(hv_set1(nb == 4 ? NR_LBEST_WIDE_SINR256 : NR_LBEST_WIDE_SINR64), hv_mul(lame, P1s)));
    if (hm_any(wide)) {
      hv lf[8];
      hm uf[8];
      nrh_grid(&e, 1, nb, lf, uf);
      for (int p = 0; p < Qm; p++) {
        l[p] = hv_sel(wide, lf[p], l[p]);
        u[p] = hm_andnot(u[p], wide);
      }
    }
    // graded fallback, g = 0.5 Peff / K, Peff = det / P1s
    const hv g = hv_mul(hv_set1(0.5f / (nb == 4 ? 170.0f : 42.0f)), hv_div(det, hv_max(P1s, FLOOR)));
    hv aI[4], aQ[4];
    nrh_axis_llr(estI, g, nb, aI);
    nrh_axis_llr(estQ, g, nb, aQ);
    for (int b = 0; b < nb; b++) {
      axis[2 * b] = aI[b];
      axis[2 * b + 1] = aQ[b];
    }
  }
  for (int p = 0; p < Qm; p++) {
    const hi v = nrh_llr16(l[p], inv);
    res[p] = full ? v : hi_sel(u[p], nrh_llr16(axis[p], inv), v);
  }
}

// per-RE rescale exponent putting max(P0, P1) (>= 0) in [8, 16): 18 - E, E the FP16 exponent field of the max
static inline hi nrh_rescale_exp(hv P0, hv P1)
{
  return hi_min(hi_sub(hi_set1(18), hv_expo(hv_max(P0, P1))), hi_set1(15));
}

// res[p] lane i -> out[i * Qm + p]: the SIMD transpose of the int16 kernels (Qm 6 / 8 only), a scalar interleave below
static inline void nrh_store(int16_t *out, const hi res[8], int Qm)
{
  NRLB_VI r[8];
  for (int p = 0; p < Qm; p++)
    r[p] = hi_simde(res[p]);
  if (Qm >= 6) {
    NRLB_NAME(nrlbw_store_llr)(out, 0, r, Qm);
    return;
  }
  int16_t t[4][NRH_N] __attribute__((aligned(64)));
  for (int p = 0; p < Qm; p++)
    NRLB_STOREU(t[p], r[p]);
  for (int i = 0; i < NRH_N; i++)
    for (int p = 0; p < Qm; p++)
      out[i * Qm + p] = t[p][i];
}

int nr_lbest_f16_lanes(void)
{
  return NRH_N;
}

// ---------------------------------------------------------------------------------------------------------------------
// detector on the int16 compensation outputs (target layer)
// ---------------------------------------------------------------------------------------------------------------------
static inline __attribute__((always_inline)) void nrh_lbest_block(const c16_t *s0, const c16_t *s1, const c16_t *cm0, const c16_t *cm1, const c16_t *rho, int16_t *out, int full, float lambda, int nb)
{
  // cm = ||h||^2 K^(1/2) / (M / 2) (the QAM_n1 magnitudes)
  const hv PSC = hv_set1((nb == 3 ? 6.48074069840786f : 13.038404810405298f) / (float)(1 << (nb - 1)));
  hi z1r, z1i, z2r, z2i, rr, ri, c0, c1, dummy;
  hi_load_c16(s0, &z1r, &z1i);
  hi_load_c16(s1, &z2r, &z2i);
  hi_load_c16(rho, &rr, &ri);
  hi_load_c16(cm0, &c0, &dummy);
  hi_load_c16(cm1, &c1, &dummy);
  const hv P0 = hv_mul(hv_cvt(c0), PSC), P1 = hv_mul(hv_cvt(c1), PSC);
  const hi k = nrh_rescale_exp(P0, P1);
  const hv T = hv_pow2(k);
  const hsc inv = nrh_llr_scale(k);
  const hs up = hs_pow2(k);
  hi res[8];
  nrh_layer(hv_mul(hv_cvt(z1r), T), hv_mul(hv_cvt(z1i), T), hv_mul(hv_cvt(z2r), T), hv_mul(hv_cvt(z2i), T), hv_mul(P0, T), hv_mul(P1, T),
            hv_mul(hv_cvt(rr), T), hv_mul(hv_cvt(ri), T), hv_lam(lambda, &up), full, nb, &inv, res);
  nrh_store(out, res, 2 * nb);
}

// one specialization per modulation and candidate set (constant loop bounds in the grid)
#define NRH_LBEST_SPEC(NB, FULL)                                                                                           \
  static void nrh_lbest_##NB##_##FULL(c16_t *s0, c16_t *s1, c16_t *cm0, c16_t *cm1, int16_t *out, c16_t *rho, uint32_t length, float lambda) \
  {                                                                                                                         \
    for (uint32_t re = 0; re < length; re += NRH_N) {                                                                       \
      if (re + NRH_N > length)                                                                                              \
        re = length - NRH_N; /* overlaps the previous vector */                                                             \
      nrh_lbest_block(&s0[re], &s1[re], &cm0[re], &cm1[re], &rho[re], &out[re * 2 * NB], FULL, lambda, NB);                 \
    }                                                                                                                       \
  }
NRH_LBEST_SPEC(3, 0)
NRH_LBEST_SPEC(3, 1)
NRH_LBEST_SPEC(4, 0)
NRH_LBEST_SPEC(4, 1)
#undef NRH_LBEST_SPEC

static void nrh_lbest(c16_t *s0, c16_t *s1, c16_t *cm0, c16_t *cm1, int16_t *out, c16_t *rho, uint32_t length, int pattern, float lambda, int nb)
{
  if (length < NRH_N) { // a zero-padded vector: the REs are independent, a call gives the same LLRs whatever its length
    c16_t in[5][NRH_N] __attribute__((aligned(64))) = {0};
    int16_t o[NRH_N * 8] __attribute__((aligned(64)));
    memcpy(in[0], s0, length * sizeof(c16_t));
    memcpy(in[1], s1, length * sizeof(c16_t));
    memcpy(in[2], cm0, length * sizeof(c16_t));
    memcpy(in[3], cm1, length * sizeof(c16_t));
    memcpy(in[4], rho, length * sizeof(c16_t));
    nrh_lbest(in[0], in[1], in[2], in[3], o, in[4], NRH_N, pattern, lambda, nb);
    memcpy(out, o, length * 2 * nb * sizeof(int16_t));
    return;
  }
  const int full = pattern == 3;
  if (nb == 3)
    (full ? nrh_lbest_3_1 : nrh_lbest_3_0)(s0, s1, cm0, cm1, out, rho, length, lambda);
  else
    (full ? nrh_lbest_4_1 : nrh_lbest_4_0)(s0, s1, cm0, cm1, out, rho, length, lambda);
}

void nr_qam256_llr_2layer_lbest_f16(c16_t *stream0_in, c16_t *stream1_in, c16_t *ch_mag, c16_t *ch_mag_i, int16_t *stream0_out, c16_t *rho01, uint32_t length, int pattern, float lambda)
{
  nrh_lbest(stream0_in, stream1_in, ch_mag, ch_mag_i, stream0_out, rho01, length, pattern, lambda, 4);
}

void nr_qam64_llr_2layer_lbest_f16(c16_t *stream0_in, c16_t *stream1_in, c16_t *ch_mag, c16_t *ch_mag_i, int16_t *stream0_out, c16_t *rho01, uint32_t length, int pattern, float lambda)
{
  nrh_lbest(stream0_in, stream1_in, ch_mag, ch_mag_i, stream0_out, rho01, length, pattern, lambda, 3);
}

// ---------------------------------------------------------------------------------------------------------------------
// fused inner RX: MRC + Gram in FP16, then the detector of both layers
// ---------------------------------------------------------------------------------------------------------------------
typedef struct {
  hv z0r, z0i, z1r, z1i, P0, P1, rr, ri;
} nrh_fe_t;

// Front end of NRH_N REs from rxFext / chFext ([nb_rx][bl] / [2][nb_rx][bl]): each RE scaled by 2^ls (ls = 1 - e, e
// the bit length of its largest RX or channel component: |x| < 2), the PTRS phase error applied to the RX
// (conj(c cpe) y = conj(c) (conj(cpe) y)); returns ls
#if defined(__aarch64__)
static inline hi nrh_front_end(const c16_t *rx, const c16_t *ch, uint32_t bl, int nb_rx, uint32_t re, c16_t cpe, nrh_fe_t *f)
{
  const bool rot = !(cpe.r == 32767 && cpe.i == 0);
  const hv CR = hv_set1(cpe.r / 32768.0f), CI = hv_set1(cpe.i / 32768.0f);
  int16x8_t amax = vdupq_n_s16(1);
  for (int a = 0; a < nb_rx; a++) {
    const int16x8x2_t y = vld2q_s16(&rx[(size_t)a * bl + re].r);
    const int16x8x2_t h0 = vld2q_s16(&ch[(size_t)a * bl + re].r), h1 = vld2q_s16(&ch[(size_t)(nb_rx + a) * bl + re].r);
    amax = vmaxq_s16(amax, vmaxq_s16(vqabsq_s16(y.val[0]), vqabsq_s16(y.val[1])));
    amax = vmaxq_s16(amax, vmaxq_s16(vqabsq_s16(h0.val[0]), vqabsq_s16(h0.val[1])));
    amax = vmaxq_s16(amax, vmaxq_s16(vqabsq_s16(h1.val[0]), vqabsq_s16(h1.val[1])));
  }
  const hi ls = vsubq_s16(vclzq_s16(amax), vdupq_n_s16(15));
  const hv S = hv_pow2(ls);
  hv z0r = hv_set1(0.0f), z0i = z0r, z1r = z0r, z1i = z0r, P0 = z0r, P1 = z0r, rr = z0r, ri = z0r;
  for (int a = 0; a < nb_rx; a++) {
    const int16x8x2_t y = vld2q_s16(&rx[(size_t)a * bl + re].r);
    const int16x8x2_t h0 = vld2q_s16(&ch[(size_t)a * bl + re].r), h1 = vld2q_s16(&ch[(size_t)(nb_rx + a) * bl + re].r);
    hv yr = hv_mul(hv_cvt(y.val[0]), S), yi = hv_mul(hv_cvt(y.val[1]), S);
    if (rot) {
      const hv tr = hv_fma(hv_mul(CR, yr), CI, yi);
      yi = hv_fms(hv_mul(CR, yi), CI, yr);
      yr = tr;
    }
    const hv a0r = hv_mul(hv_cvt(h0.val[0]), S), a0i = hv_mul(hv_cvt(h0.val[1]), S);
    const hv a1r = hv_mul(hv_cvt(h1.val[0]), S), a1i = hv_mul(hv_cvt(h1.val[1]), S);
    z0r = hv_fma(hv_fma(z0r, a0r, yr), a0i, yi);
    z0i = hv_fms(hv_fma(z0i, a0r, yi), a0i, yr);
    z1r = hv_fma(hv_fma(z1r, a1r, yr), a1i, yi);
    z1i = hv_fms(hv_fma(z1i, a1r, yi), a1i, yr);
    P0 = hv_fma(hv_fma(P0, a0r, a0r), a0i, a0i);
    P1 = hv_fma(hv_fma(P1, a1r, a1r), a1i, a1i);
    rr = hv_fma(hv_fma(rr, a0r, a1r), a0i, a1i);
    ri = hv_fms(hv_fma(ri, a0r, a1i), a0i, a1r);
  }
  *f = (nrh_fe_t){z0r, z0i, z1r, z1i, P0, P1, rr, ri};
  return ls;
}
// single-layer front end: z = sum conj(c) y, P = sum |c|^2 (chF [nb_rx][bl]), the scaling of the 2-layer one
static inline hi nrh_front_end_1l(const c16_t *rx, const c16_t *ch, uint32_t bl, int nb_rx, uint32_t re, c16_t cpe, hv *zr, hv *zi, hv *P)
{
  const bool rot = !(cpe.r == 32767 && cpe.i == 0);
  const hv CR = hv_set1(cpe.r / 32768.0f), CI = hv_set1(cpe.i / 32768.0f);
  int16x8_t amax = vdupq_n_s16(1);
  for (int a = 0; a < nb_rx; a++) {
    const int16x8x2_t y = vld2q_s16(&rx[(size_t)a * bl + re].r), h = vld2q_s16(&ch[(size_t)a * bl + re].r);
    amax = vmaxq_s16(amax, vmaxq_s16(vqabsq_s16(y.val[0]), vqabsq_s16(y.val[1])));
    amax = vmaxq_s16(amax, vmaxq_s16(vqabsq_s16(h.val[0]), vqabsq_s16(h.val[1])));
  }
  const hi ls = vsubq_s16(vclzq_s16(amax), vdupq_n_s16(15));
  const hv S = hv_pow2(ls);
  hv a_zr = hv_set1(0.0f), a_zi = a_zr, a_P = a_zr;
  for (int a = 0; a < nb_rx; a++) {
    const int16x8x2_t y = vld2q_s16(&rx[(size_t)a * bl + re].r), h = vld2q_s16(&ch[(size_t)a * bl + re].r);
    hv yr = hv_mul(hv_cvt(y.val[0]), S), yi = hv_mul(hv_cvt(y.val[1]), S);
    if (rot) {
      const hv tr = hv_fma(hv_mul(CR, yr), CI, yi);
      yi = hv_fms(hv_mul(CR, yi), CI, yr);
      yr = tr;
    }
    const hv cr = hv_mul(hv_cvt(h.val[0]), S), ci = hv_mul(hv_cvt(h.val[1]), S);
    a_zr = hv_fma(hv_fma(a_zr, cr, yr), ci, yi);
    a_zi = hv_fms(hv_fma(a_zi, cr, yi), ci, yr);
    a_P = hv_fma(hv_fma(a_P, cr, cr), ci, ci);
  }
  *zr = a_zr;
  *zi = a_zi;
  *P = a_P;
  return ls;
}
#else
// AVX512-FP16: complex FMAs on the interleaved data (y conj(h) accumulate, no deinterleave on load), deinterleaved
// once at the end; two vectors of 16 complex per quantity
static inline hi nrh_front_end(const c16_t *rx, const c16_t *ch, uint32_t bl, int nb_rx, uint32_t re, c16_t cpe, nrh_fe_t *f)
{
  const bool rot = !(cpe.r == 32767 && cpe.i == 0);
  _Float16 cp[2] = {(_Float16)(cpe.r / 32768.0f), (_Float16)(cpe.i / 32768.0f)};
  int32_t cpbits;
  memcpy(&cpbits, cp, sizeof(cpbits));
  const __m512h CPE = _mm512_castsi512_ph(_mm512_set1_epi32(cpbits));
  const __m512i one = _mm512_set1_epi32(1), lo16 = _mm512_set1_epi32(0xffff);
  __m512i ls32[2];
  __m512h S[2];
  for (int h = 0; h < 2; h++) {
    __m512i m = one;
    for (int a = 0; a < nb_rx; a++) {
      const __m512i y = _mm512_loadu_si512(&rx[(size_t)a * bl + re + 16 * h]);
      const __m512i h0 = _mm512_loadu_si512(&ch[(size_t)a * bl + re + 16 * h]), h1 = _mm512_loadu_si512(&ch[(size_t)(nb_rx + a) * bl + re + 16 * h]);
      // unsigned: |-32768| stays 0x8000, which compares as 32768
      const __m512i v = _mm512_max_epu16(_mm512_max_epu16(_mm512_abs_epi16(y), _mm512_abs_epi16(h0)), _mm512_abs_epi16(h1));
      m = _mm512_max_epu16(m, v);
    }
    m = _mm512_and_si512(_mm512_max_epu16(m, _mm512_srli_epi32(m, 16)), lo16); // max(|re|, |im|) per RE
    m = _mm512_min_epu32(m, _mm512_set1_epi32(32767));
    ls32[h] = _mm512_sub_epi32(_mm512_lzcnt_epi32(m), _mm512_set1_epi32(31)); // 1 - e = (clz32 - 16) - 15
    const __m512i fbits = _mm512_slli_epi32(_mm512_add_epi32(ls32[h], _mm512_set1_epi32(15)), 10);
    S[h] = _mm512_castsi512_ph(_mm512_or_si512(fbits, _mm512_slli_epi32(fbits, 16))); // 2^ls on re and im
  }
  __m512h z0[2], z1[2], g00[2], g11[2], g01[2];
  for (int h = 0; h < 2; h++) {
    z0[h] = z1[h] = g00[h] = g11[h] = g01[h] = _mm512_setzero_ph();
    for (int a = 0; a < nb_rx; a++) {
      __m512h y = _mm512_mul_ph(_mm512_cvtepi16_ph(_mm512_loadu_si512(&rx[(size_t)a * bl + re + 16 * h])), S[h]);
      if (rot)
        y = _mm512_fcmul_pch(y, CPE); // y conj(cpe)
      const __m512h c0 = _mm512_mul_ph(_mm512_cvtepi16_ph(_mm512_loadu_si512(&ch[(size_t)a * bl + re + 16 * h])), S[h]);
      const __m512h c1 = _mm512_mul_ph(_mm512_cvtepi16_ph(_mm512_loadu_si512(&ch[(size_t)(nb_rx + a) * bl + re + 16 * h])), S[h]);
      z0[h] = _mm512_fcmadd_pch(y, c0, z0[h]); // y conj(c0) = conj(c0) y
      z1[h] = _mm512_fcmadd_pch(y, c1, z1[h]);
      g00[h] = _mm512_fcmadd_pch(c0, c0, g00[h]);
      g11[h] = _mm512_fcmadd_pch(c1, c1, g11[h]);
      g01[h] = _mm512_fcmadd_pch(c1, c0, g01[h]); // c1 conj(c0) = h0^H h1
    }
  }
#define NRH_RE(v) _mm512_castsi512_ph(hi_even(_mm512_castph_si512(v[0]), _mm512_castph_si512(v[1])))
#define NRH_IM(v) _mm512_castsi512_ph(hi_odd(_mm512_castph_si512(v[0]), _mm512_castph_si512(v[1])))
  *f = (nrh_fe_t){NRH_RE(z0), NRH_IM(z0), NRH_RE(z1), NRH_IM(z1), NRH_RE(g00), NRH_RE(g11), NRH_RE(g01), NRH_IM(g01)};
#undef NRH_RE
#undef NRH_IM
  return hi_even(ls32[0], ls32[1]);
}
static inline hi nrh_front_end_1l(const c16_t *rx, const c16_t *ch, uint32_t bl, int nb_rx, uint32_t re, c16_t cpe, hv *zr, hv *zi, hv *P)
{
  const bool rot = !(cpe.r == 32767 && cpe.i == 0);
  _Float16 cp[2] = {(_Float16)(cpe.r / 32768.0f), (_Float16)(cpe.i / 32768.0f)};
  int32_t cpbits;
  memcpy(&cpbits, cp, sizeof(cpbits));
  const __m512h CPE = _mm512_castsi512_ph(_mm512_set1_epi32(cpbits));
  __m512i ls32[2];
  __m512h z[2], g[2];
  for (int h = 0; h < 2; h++) {
    __m512i m = _mm512_set1_epi32(1);
    for (int a = 0; a < nb_rx; a++) {
      const __m512i y = _mm512_loadu_si512(&rx[(size_t)a * bl + re + 16 * h]), c = _mm512_loadu_si512(&ch[(size_t)a * bl + re + 16 * h]);
      m = _mm512_max_epu16(m, _mm512_max_epu16(_mm512_abs_epi16(y), _mm512_abs_epi16(c))); // unsigned: |-32768| = 0x8000
    }
    m = _mm512_and_si512(_mm512_max_epu16(m, _mm512_srli_epi32(m, 16)), _mm512_set1_epi32(0xffff));
    m = _mm512_min_epu32(m, _mm512_set1_epi32(32767));
    ls32[h] = _mm512_sub_epi32(_mm512_lzcnt_epi32(m), _mm512_set1_epi32(31));
    const __m512i fbits = _mm512_slli_epi32(_mm512_add_epi32(ls32[h], _mm512_set1_epi32(15)), 10);
    const __m512h S = _mm512_castsi512_ph(_mm512_or_si512(fbits, _mm512_slli_epi32(fbits, 16)));
    z[h] = g[h] = _mm512_setzero_ph();
    for (int a = 0; a < nb_rx; a++) {
      __m512h y = _mm512_mul_ph(_mm512_cvtepi16_ph(_mm512_loadu_si512(&rx[(size_t)a * bl + re + 16 * h])), S);
      if (rot)
        y = _mm512_fcmul_pch(y, CPE);
      const __m512h c = _mm512_mul_ph(_mm512_cvtepi16_ph(_mm512_loadu_si512(&ch[(size_t)a * bl + re + 16 * h])), S);
      z[h] = _mm512_fcmadd_pch(y, c, z[h]);
      g[h] = _mm512_fcmadd_pch(c, c, g[h]);
    }
  }
  *zr = _mm512_castsi512_ph(hi_even(_mm512_castph_si512(z[0]), _mm512_castph_si512(z[1])));
  *zi = _mm512_castsi512_ph(hi_odd(_mm512_castph_si512(z[0]), _mm512_castph_si512(z[1])));
  *P = _mm512_castsi512_ph(hi_even(_mm512_castph_si512(g[0]), _mm512_castph_si512(g[1])));
  return hi_even(ls32[0], ls32[1]);
}
#endif

static inline __attribute__((always_inline)) void nrh_fused(uint32_t length,
                            uint32_t buffer_length,
                            int nb_rx_ant,
                            const c16_t *rxFext,
                            const c16_t *chFext,
                            c16_t cpe,
                            int nb,
                            int full,
                            float lambda,
                            int output_shift,
                            int16_t *llr0,
                            int16_t *llr1,
                            int16_t *llr_cw,
                            const int16_t *scramble)
{
  const int Qm = 2 * nb;
  for (uint32_t re = 0; re < length; re += NRH_N) {
    if (re + NRH_N > length)
      re = length - NRH_N; // overlaps the previous vector: the REs are independent
    nrh_fe_t f;
    const hi ls = nrh_front_end(rxFext, chFext, buffer_length, nb_rx_ant, re, cpe, &f);
    // rescale 2^lt: max(P0, P1) in [8, 16)
    const hi lt = nrh_rescale_exp(f.P0, f.P1);
    const hv T = hv_pow2(lt);
    // vector units = int16 compensation units 2^k, k = 2 ls + lt + output_shift
    const hi k = hi_add(hi_add(hi_shl1(ls), lt), hi_set1(output_shift));
    const hsc inv = nrh_llr_scale(k);
    const hs up = hs_pow2(k);
    const hv lam = hv_lam(lambda, &up);
    const hv z0r = hv_mul(f.z0r, T), z0i = hv_mul(f.z0i, T), z1r = hv_mul(f.z1r, T), z1i = hv_mul(f.z1i, T);
    const hv P0 = hv_mul(f.P0, T), P1 = hv_mul(f.P1, T), rr = hv_mul(f.rr, T), ri = hv_mul(f.ri, T);
    hi res[2][8];
    nrh_layer(z0r, z0i, z1r, z1i, P0, P1, rr, ri, lam, full, nb, &inv, res[0]);
    nrh_layer(z1r, z1i, z0r, z0i, P1, P0, rr, hv_neg(ri), lam, full, nb, &inv, res[1]);
    if (nb == 3) // 64QAM: the one-bit cooling of nr_compute_ML_llr
      for (int t = 0; t < 2; t++)
        for (int p = 0; p < Qm; p++)
          res[t][p] = hi_shr1(res[t][p]);
    if (llr_cw) { // layer demap (per RE: layer 0 then layer 1) and descrambling
      int16_t *cw = llr_cw + (size_t)re * 2 * Qm;
      int16_t tl[2][NRH_N * 8] __attribute__((aligned(64)));
      nrh_store(tl[0], res[0], Qm);
      nrh_store(tl[1], res[1], Qm);
      for (int i = 0; i < NRH_N; i++) {
        memcpy(&cw[2 * i * Qm], &tl[0][i * Qm], Qm * sizeof(int16_t));
        memcpy(&cw[(2 * i + 1) * Qm], &tl[1][i * Qm], Qm * sizeof(int16_t));
      }
      if (scramble) {
        const int16_t *s = scramble + (size_t)re * 2 * Qm;
        for (int i = 0; i < NRH_N * 2 * Qm; i++)
          cw[i] = (int16_t)(cw[i] * s[i]);
      }
    } else {
      nrh_store(llr0 + (size_t)re * Qm, res[0], Qm);
      nrh_store(llr1 + (size_t)re * Qm, res[1], Qm);
    }
  }
}

#define NRH_FUSED_SPEC(NB, FULL)                                                                                            \
  static void nrh_fused_##NB##_##FULL(uint32_t length, uint32_t bl, int nb_rx, const c16_t *rx, const c16_t *ch, c16_t cpe, float lambda, \
                                      int shift, int16_t *l0, int16_t *l1, int16_t *cw, const int16_t *scr)                  \
  {                                                                                                                         \
    nrh_fused(length, bl, nb_rx, rx, ch, cpe, NB, FULL, lambda, shift, l0, l1, cw, scr);                                    \
  }
NRH_FUSED_SPEC(3, 0)
NRH_FUSED_SPEC(3, 1)
NRH_FUSED_SPEC(4, 0)
NRH_FUSED_SPEC(4, 1)
#undef NRH_FUSED_SPEC

void nr_inner_rx_2layer_f16(uint32_t length,
                            uint32_t buffer_length,
                            int nb_rx_ant,
                            const c16_t *rxFext,
                            const c16_t *chFext,
                            c16_t cpe,
                            int mod_order,
                            int lbest,
                            float lambda,
                            int output_shift,
                            int16_t *llr0,
                            int16_t *llr1,
                            int16_t *llr_cw,
                            const int16_t *scramble)
{
  const int full = lbest == 0;
  if (mod_order == 6)
    (full ? nrh_fused_3_1 : nrh_fused_3_0)(length, buffer_length, nb_rx_ant, rxFext, chFext, cpe, lambda, output_shift, llr0, llr1, llr_cw, scramble);
  else
    (full ? nrh_fused_4_1 : nrh_fused_4_0)(length, buffer_length, nb_rx_ant, rxFext, chFext, cpe, lambda, output_shift, llr0, llr1, llr_cw, scramble);
}

// ---------------------------------------------------------------------------------------------------------------------
// fused single-layer inner RX: MRC in FP16, the PAM LLRs of nr_compute_llr (level 0 the MRC output, level b
// m_b - |level b-1| with m_b = ||h||^2 QAM_n_b), in the units of the int16 compensation at output_shift (QPSK: >> 4)
// ---------------------------------------------------------------------------------------------------------------------
static inline __attribute__((always_inline)) void nrh_1layer(uint32_t length,
                                                            uint32_t bl,
                                                            int nb_rx,
                                                            const c16_t *rx,
                                                            const c16_t *ch,
                                                            c16_t cpe,
                                                            int nb,
                                                            int output_shift,
                                                            int16_t *llr,
                                                            const int16_t *scramble)
{
  // QAM16_n1, QAM64_n1/n2, QAM256_n1/n2/n3 of PHY/impl_defs_top.h (Q15)
  static const float amp[5][3] = {{0},
                                  {0},
                                  {20724.0f / 32768, 0, 0},
                                  {20225.0f / 32768, 10112.0f / 32768, 0},
                                  {20106.0f / 32768, 10053.0f / 32768, 5026.0f / 32768}};
  const int Qm = 2 * nb;
  for (uint32_t re = 0; re < length; re += NRH_N) {
    if (re + NRH_N > length)
      re = length - NRH_N; // overlaps the previous vector: the REs are independent
    hv zr, zi, P;
    const hi ls = nrh_front_end_1l(rx, ch, bl, nb_rx, re, cpe, &zr, &zi, &P);
    const hi lt = nrh_rescale_exp(P, P);
    const hv T = hv_pow2(lt);
    // vector units = int16 compensation units 2^k, k = 2 ls + lt + output_shift (+ 4: the QPSK LLR is z >> 4)
    const hi k = hi_add(hi_add(hi_shl1(ls), lt), hi_set1(output_shift + (nb == 1 ? 4 : 0)));
    const hsc inv = nrh_llr_scale(k);
    hv l[8];
    l[0] = hv_mul(zr, T);
    l[1] = hv_mul(zi, T);
    P = hv_mul(P, T);
    for (int b = 1; b < nb; b++) {
      const hv m = hv_mul(P, hv_set1(amp[nb][b - 1]));
      l[2 * b] = hv_sub(m, hv_abs(l[2 * b - 2]));
      l[2 * b + 1] = hv_sub(m, hv_abs(l[2 * b - 1]));
    }
    hi res[8];
    for (int p = 0; p < Qm; p++)
      res[p] = nrh_llr16(l[p], &inv);
    int16_t *out = llr + (size_t)re * Qm;
    nrh_store(out, res, Qm);
    if (scramble) {
      const int16_t *sc = scramble + (size_t)re * Qm;
      for (int i = 0; i < NRH_N * Qm; i++)
        out[i] = (int16_t)(out[i] * sc[i]);
    }
  }
}

#define NRH_1LAYER_SPEC(NB)                                                                                                 \
  static void nrh_1layer_##NB(uint32_t length, uint32_t bl, int nb_rx, const c16_t *rx, const c16_t *ch, c16_t cpe, int shift, int16_t *llr, \
                              const int16_t *scr)                                                                           \
  {                                                                                                                         \
    nrh_1layer(length, bl, nb_rx, rx, ch, cpe, NB, shift, llr, scr);                                                       \
  }
NRH_1LAYER_SPEC(1)
NRH_1LAYER_SPEC(2)
NRH_1LAYER_SPEC(3)
NRH_1LAYER_SPEC(4)
#undef NRH_1LAYER_SPEC

void nr_inner_rx_1layer_f16(uint32_t length,
                            uint32_t buffer_length,
                            int nb_rx_ant,
                            const c16_t *rxFext,
                            const c16_t *chFext,
                            c16_t cpe,
                            int mod_order,
                            int output_shift,
                            int16_t *llr,
                            const int16_t *scramble)
{
  static void (*const f[5])(uint32_t, uint32_t, int, const c16_t *, const c16_t *, c16_t, int, int16_t *, const int16_t *) =
      {NULL, nrh_1layer_1, nrh_1layer_2, nrh_1layer_3, nrh_1layer_4};
  f[mod_order / 2](length, buffer_length, nb_rx_ant, rxFext, chFext, cpe, output_shift, llr, scramble);
}
