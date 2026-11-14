/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Detector core of the FP16 2-layer receivers, written against the FP16 vector layer (hv / hi / hm and their
 * operations) defined by the includer: nr_lbest_f16.c for the CPU vector ISAs, the OpenCL kernels for one RE per
 * work-item. No includes: the layer, NR_2LAYER_CSI_EPS and NR_LBEST_WIDE_SINR* must be defined before.
 */

#ifndef NR_LBEST_F16_CORE_H
#define NR_LBEST_F16_CORE_H

// loop unrolling hint of the includer (GPUs: constant indices keep the per-candidate arrays in registers); the CPU
// compilers unroll these constant-bound loops by themselves
#ifndef NRH_UNROLL
#define NRH_UNROLL
#endif

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
  NRH_UNROLL
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
  NRH_UNROLL
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
  NRH_UNROLL
  for (int k = 0; k < nI; k++)
    rowMax[k] = colMax[k] = NEG;
  NRH_UNROLL
  for (int a = 0; a < nI; a++) {
    const hv d1r = dI[a];
    NRH_UNROLL
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
  NRH_UNROLL
  for (int k = 0; k < nb; k++) {
    hv m0I = NEG, m1I = NEG, m0Q = NEG, m1Q = NEG;
    NRH_UNROLL
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
      NRH_UNROLL
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
    NRH_UNROLL
    for (int b = 0; b < nb; b++) {
      axis[2 * b] = aI[b];
      axis[2 * b + 1] = aQ[b];
    }
  }
  NRH_UNROLL
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

#endif
