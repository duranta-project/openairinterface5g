/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * 2-layer 64QAM and 256QAM L-best SIMD LLR kernels, written once against the width abstraction
 * in nr_lbest_simd_width.h. The build compiles this file once per vector width, with NRLB_W
 * set to 128 (SSE / NEON), 256 (AVX2) and 512 (AVX-512 only), into separate objects; each
 * defines the _w<NRLB_W> entries declared in nr_lbest_simd.h. All widths are bit-exact
 * (test_llr_2layer); nr_compute_llr.c picks the arch-native one.
 */
#include "nr_lbest_simd.h"
#include "nr_compute_llr.h"

#ifndef NRLB_W
#error "NRLB_W must be defined (128, 256 or 512)"
#endif

// The 512-bit instantiation exists only when AVX-512 is a real compile target (SIMDe would
// otherwise emulate it as 2x256/4x128, slower than the native widths).
#if NRLB_W != 512 || (defined(__AVX512BW__) && defined(__AVX512VL__) && defined(__AVX512F__))
#if NRLB_W == 512
#include <simde/x86/avx512.h>
#endif
#include "nr_lbest_simd_width.h"

// Float helpers shared by the 64QAM and 256QAM graded per-axis LLRs and the ZF seed.
// nearest odd integer: 2 * round((x - 1) / 2) + 1
static inline NRLB_VF NRLB_NAME(nrlbw_round_odd)(NRLB_VF x)
{
  const NRLB_VF one = NRLB_MM(set1_ps)(1.0f);
  const NRLB_VF h = NRLB_MM(mul_ps)(NRLB_MM(sub_ps)(x, one), NRLB_MM(set1_ps)(0.5f));
  const NRLB_VF r = NRLB_ROUNDPS(h, SIMDE_MM_FROUND_TO_NEAREST_INT | SIMDE_MM_FROUND_NO_EXC);
  return NRLB_MM(add_ps)(NRLB_MM(mul_ps)(r, NRLB_MM(set1_ps)(2.0f)), one);
}

static inline NRLB_VF NRLB_NAME(nrlbw_clamp)(NRLB_VF x, float lo, float hi)
{
  return NRLB_MM(min_ps)(NRLB_MM(max_ps)(x, NRLB_MM(set1_ps)(lo)), NRLB_MM(set1_ps)(hi));
}

// squared distance (lvl - ref)^2
static inline NRLB_VF NRLB_NAME(nrlbw_dsq)(NRLB_VF lvl, NRLB_VF ref)
{
  const NRLB_VF d = NRLB_MM(sub_ps)(lvl, ref);
  return NRLB_MM(mul_ps)(d, d);
}

// float -> int32, rounded and saturated to the int16 range first: the x86 conversion returns
// INT_MIN for out-of-range input (whatever its sign) while NEON saturates, so clamp to keep the
// widths bit-exact; the int16 pack that follows saturates in range.
static inline NRLB_VI NRLB_NAME(nrlbw_cvt_sat)(NRLB_VF x)
{
  return NRLB_MM(cvtps_epi32)(NRLB_NAME(nrlbw_clamp)(x, -32768.0f, 32767.0f));
}

// ============================================================================
// int16/16-lane variant of the AVX2 L-best kernel. The dominant per-candidate
// work (metric, conditional slice, max-log reduction) runs 16 REs/vector in
// int16; only the ZF seed and z2*sqrt(42) (which overflow int16) use int32 half-
// vectors, once per RE-vector. Metric is carried at scale (8*sqrt(42))/512 so all
// terms fit int16. Single simde-256 source -> NEON on aarch64.
// ============================================================================
static inline NRLB_VI NRLB_NAME(nrlbw_level)(NRLB_VI num, NRLB_VI thunit)
{
  const NRLB_VI z = NRLB_SETZERO();
  const NRLB_VI a = NRLB_MM(abs_epi16)(num);
  const NRLB_VI t2 = NRLB_MM(slli_epi16)(thunit, 1);
  const NRLB_VI t4 = NRLB_MM(slli_epi16)(thunit, 2);
  const NRLB_VI t6 = NRLB_MM(adds_epi16)(t2, t4);
  const NRLB_VI g2 = NRLB_MM(sub_epi16)(z, NRLB_CMPGT16(a, t2));
  const NRLB_VI g4 = NRLB_MM(sub_epi16)(z, NRLB_CMPGT16(a, t4));
  const NRLB_VI g6 = NRLB_MM(sub_epi16)(z, NRLB_CMPGT16(a, t6));
  const NRLB_VI mag =
      NRLB_MM(adds_epi16)(NRLB_MM(set1_epi16)(1), NRLB_MM(slli_epi16)(NRLB_MM(adds_epi16)(NRLB_MM(adds_epi16)(g2, g4), g6), 1));
  const NRLB_VI neg = NRLB_CMPGT16(z, num);
  return NRLB_BLENDV(mag, NRLB_MM(sub_epi16)(z, mag), neg);
}

static inline NRLB_VI NRLB_NAME(nrlbw_z42d32)(NRLB_VI z16)
{
  const NRLB_VI C = NRLB_MM(set1_epi32)(NR_LBEST_Q_SQRT42_Q12);
  const NRLB_VI lo = NRLB_MM(srai_epi32)(NRLB_MM(mullo_epi32)(NRLB_NAME(nrlbw_widen)(z16, 0), C), 17); // >>12 (Q12) >>5 (/32)
  const NRLB_VI hi = NRLB_MM(srai_epi32)(NRLB_MM(mullo_epi32)(NRLB_NAME(nrlbw_widen)(z16, 1), C), 17);
  return NRLB_NAME(nrlbw_pack32)(lo, hi);
}

// MMSE-seed nearest-level centers for 16 REs (int16 out). The 2x2 solve is done in
// FLOAT, which is robust to the wide per-RE dynamic range of real channels (TDL
// deep fades) where any fixed int pre-shift either loses precision or underflows.
// Also emits dirImask/dirQmask: int16 mask, all-ones where the seed lies on the
// +2 side of the nearest level, and axisLLR[6] = graded per-axis PAM-8 LLRs (interleaved
// {I0,Q0,I1,Q1,I2,Q2}, final LLR units) used for bits the reduced search cannot span.

// Graded per-axis PAM-8 LLR (SIMD float): out[b] = g*(min dist^2 with bit b=1 - with b=0),
// g = 0.5*Peff/42, Peff = det/P1 the post-ZF energy (float reference: nr_lbest_pam8_axis_llr). Closed form via clamps of the
// nearest odd level. Only b0/b1 are computed: b2 is ALWAYS spanned by the 3x3 / plus windows (the centre and its +-2 neighbours
// always contain both {3,5} and {1,7} magnitudes), so out[2] is never read.
static inline void NRLB_NAME(nrlbw_axis_llr64)(NRLB_VF est, NRLB_VF g, NRLB_VF out[3])
{
  const NRLB_VF a = NRLB_MM(andnot_ps)(NRLB_MM(set1_ps)(-0.0f), est);
  const NRLB_VF o = NRLB_NAME(nrlbw_round_odd)(est), oa = NRLB_NAME(nrlbw_round_odd)(a);
  // b0 (sign): pos levels [1,7] (bit 0) vs neg [-7,-1] (bit 1)
  out[0] = NRLB_MM(mul_ps)(g,
                           NRLB_MM(sub_ps)(NRLB_NAME(nrlbw_dsq)(NRLB_NAME(nrlbw_clamp)(o, -7.0f, -1.0f), est),
                                           NRLB_NAME(nrlbw_dsq)(NRLB_NAME(nrlbw_clamp)(o, 1.0f, 7.0f), est)));
  // b1 (|L|>4): |L| in {1,3} (bit 0) vs {5,7} (bit 1); by symmetry work in a=|est|
  out[1] = NRLB_MM(mul_ps)(g,
                           NRLB_MM(sub_ps)(NRLB_NAME(nrlbw_dsq)(NRLB_NAME(nrlbw_clamp)(oa, 5.0f, 7.0f), a),
                                           NRLB_NAME(nrlbw_dsq)(NRLB_NAME(nrlbw_clamp)(oa, 1.0f, 3.0f), a)));
  out[2] = NRLB_MM(setzero_ps)(); // b2 always spanned by the search window -> fallback never read
}

static inline void NRLB_NAME(nrlbw_seed)(NRLB_VI z1r16,
                                         NRLB_VI z1i16,
                                         NRLB_VI z2r16,
                                         NRLB_VI z2i16,
                                         NRLB_VI rr16,
                                         NRLB_VI ri16,
                                         NRLB_VI cm0_16,
                                         NRLB_VI cm1_16,
                                         NRLB_VF lam,
                                         NRLB_VF epsh,
                                         NRLB_VI *centerI,
                                         NRLB_VI *centerQ,
                                         NRLB_VI *dirImask,
                                         NRLB_VI *dirQmask,
                                         NRLB_VI axisLLR[6])
{
  NRLB_VF aL[2][6]; // [half][ {I0,Q0,I1,Q1,I2,Q2} ]
  const NRLB_VI SQRT42_OVER4 = NRLB_MM(set1_epi32)(NR_LBEST_Q_SQRT42_OVER4_Q14);
  const NRLB_VF SQRT42 = NRLB_MM(set1_ps)(6.48074069840786f);
  const NRLB_VF ONE = NRLB_MM(set1_ps)(1.0f);
  NRLB_VI cI[2], cQ[2], dI[2], dQ[2];
  for (int h = 0; h < 2; h++) {
    const NRLB_VF z1r = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z1r16, h)),
                  z1i = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z1i16, h));
    const NRLB_VF z2r = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z2r16, h)),
                  z2i = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z2i16, h));
    const NRLB_VF rr = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(rr16, h)),
                  ri = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(ri16, h));
    const NRLB_VF Pp0 =
        NRLB_MM(cvtepi32_ps)(NRLB_MM(srai_epi32)(NRLB_MM(mullo_epi32)(NRLB_NAME(nrlbw_widen)(cm0_16, h), SQRT42_OVER4), 14));
    const NRLB_VF Pp1 =
        NRLB_MM(cvtepi32_ps)(NRLB_MM(srai_epi32)(NRLB_MM(mullo_epi32)(NRLB_NAME(nrlbw_widen)(cm1_16, h), SQRT42_OVER4), 14));
    // unbiased MMSE seed: ZF with Pp1 -> Pp1 + lambda_eff, lambda_eff = lambda + eps (Pp0 + Pp1) / 2 (noise + CSI error)
    const NRLB_VF Pp1s = NRLB_MM(add_ps)(NRLB_MM(add_ps)(Pp1, lam), NRLB_MM(mul_ps)(epsh, NRLB_MM(add_ps)(Pp0, Pp1)));
    // 2x2 solve (float): x_hat1 = ((Pp1 + lambda) z1 - rho z2) / det,  det = Pp0 (Pp1 + lambda) - |rho|^2
    NRLB_VF det = NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(Pp0, Pp1s), NRLB_MM(add_ps)(NRLB_MM(mul_ps)(rr, rr), NRLB_MM(mul_ps)(ri, ri)));
    det = NRLB_MM(max_ps)(det, ONE); // guard near-singular (high correlation)
    const NRLB_VF numr =
        NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(Pp1s, z1r), NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(rr, z2r), NRLB_MM(mul_ps)(ri, z2i)));
    const NRLB_VF numi =
        NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(Pp1s, z1i), NRLB_MM(add_ps)(NRLB_MM(mul_ps)(rr, z2i), NRLB_MM(mul_ps)(ri, z2r)));
    // soft level estimate est = x_hat1 * sqrt(42)  (~ +-1..7)
    const NRLB_VF estI = NRLB_MM(mul_ps)(NRLB_MM(div_ps)(numr, det), SQRT42);
    const NRLB_VF estQ = NRLB_MM(mul_ps)(NRLB_MM(div_ps)(numi, det), SQRT42);
    // graded per-axis PAM-8 LLRs from the soft estimate (g = 0.5*Peff/42, Peff = det/(Pp1 + lambda), the post-MMSE energy)
    const NRLB_VF glin = NRLB_MM(mul_ps)(NRLB_MM(set1_ps)(0.5f / 42.0f), NRLB_MM(div_ps)(det, NRLB_MM(max_ps)(Pp1s, ONE)));
    NRLB_VF aI3[3], aQ3[3];
    NRLB_NAME(nrlbw_axis_llr64)(estI, glin, aI3);
    NRLB_NAME(nrlbw_axis_llr64)(estQ, glin, aQ3);
    for (int b = 0; b < 3; b++) {
      aL[h][2 * b] = aI3[b];
      aL[h][2 * b + 1] = aQ3[b];
    }
    // nearest odd level: o = 2*round((est-1)/2)+1, clamped to [-7,7]
    const NRLB_VF oI = NRLB_NAME(nrlbw_clamp)(NRLB_NAME(nrlbw_round_odd)(estI), -7.0f, 7.0f);
    const NRLB_VF oQ = NRLB_NAME(nrlbw_clamp)(NRLB_NAME(nrlbw_round_odd)(estQ), -7.0f, 7.0f);
    cI[h] = NRLB_MM(cvtps_epi32)(oI);
    cQ[h] = NRLB_MM(cvtps_epi32)(oQ);
    dI[h] = NRLB_CMPPS_SI(estI, oI, SIMDE_CMP_GT_OQ); // toward-seed = +2 side
    dQ[h] = NRLB_CMPPS_SI(estQ, oQ, SIMDE_CMP_GT_OQ);
  }
  *centerI = NRLB_NAME(nrlbw_pack32)(cI[0], cI[1]);
  *centerQ = NRLB_NAME(nrlbw_pack32)(cQ[0], cQ[1]);
  *dirImask = NRLB_NAME(nrlbw_pack32)(dI[0], dI[1]);
  *dirQmask = NRLB_NAME(nrlbw_pack32)(dQ[0], dQ[1]);
  for (int p = 0; p < 6; p++) // round to nearest int16, saturating pack (final LLR units)
    axisLLR[p] = NRLB_NAME(nrlbw_pack32)(NRLB_NAME(nrlbw_cvt_sat)(aL[0][p]), NRLB_NAME(nrlbw_cvt_sat)(aL[1][p]));
}

// Per-candidate evaluation (slice + metric + max-log reduction), updating max0/max1.
// Captures the per-RE-vector operands from the enclosing scope; the per-candidate
// I-only/Q-only terms (bits, corr0/energy0 parts, slice operands) are passed in.
// Store Qm LLR vectors (res[Qm], 16 REs across the lanes) to the interleaved decoder layout
// stream0_out[(re+r)*Qm + p] via an 8x8 int16 transpose (padded from Qm), replacing the scalar

#define NR_LBEST_METRIC16(MET_, I1_, Q1_, C0I_, C0Q_, EI_, EQ_, RRI_, RII_, RRQ_, RIQ_)                                            \
  do {                                                                                                                             \
    const NRLB_VI A2I = NRLB_MM(subs_epi16)(z2r42, NRLB_MM(adds_epi16)((RRI_), (RIQ_)));                                           \
    const NRLB_VI A2Q = NRLB_MM(subs_epi16)(z2i42, NRLB_MM(subs_epi16)((RRQ_), (RII_)));                                           \
    const NRLB_VI I2 = NRLB_NAME(nrlbw_level)(A2I, Pp1_5);                                                                         \
    const NRLB_VI Q2 = NRLB_NAME(nrlbw_level)(A2Q, Pp1_5);                                                                         \
    const NRLB_VI corr0 = NRLB_MM(adds_epi16)((C0I_), (C0Q_));                                                                     \
    const NRLB_VI energy0 = NRLB_MM(adds_epi16)((EI_), (EQ_));                                                                     \
    const NRLB_VI corr1 = NRLB_MM(adds_epi16)(NRLB_MM(mulhi_epi16)(z2r, NRLB_MM(mullo_epi16)(I2, C13)),                            \
                                              NRLB_MM(mulhi_epi16)(z2i, NRLB_MM(mullo_epi16)(Q2, C13)));                           \
    const NRLB_VI energy1 = NRLB_MM(mulhi_epi16)(                                                                                  \
        cm1,                                                                                                                       \
        NRLB_MM(mullo_epi16)(NRLB_MM(adds_epi16)(NRLB_MM(mullo_epi16)(I2, I2), NRLB_MM(mullo_epi16)(Q2, Q2)), CE));                \
    const NRLB_VI aa = NRLB_MM(adds_epi16)(NRLB_MM(mullo_epi16)((I1_), I2), NRLB_MM(mullo_epi16)((Q1_), Q2));                      \
    const NRLB_VI bb = NRLB_MM(subs_epi16)(NRLB_MM(mullo_epi16)((Q1_), I2), NRLB_MM(mullo_epi16)((I1_), Q2));                      \
    const NRLB_VI cross = NRLB_MM(adds_epi16)(NRLB_MM(mulhi_epi16)(rr, NRLB_MM(mullo_epi16)(aa, CX)),                              \
                                              NRLB_MM(mulhi_epi16)(ri, NRLB_MM(mullo_epi16)(bb, CX)));                             \
    (MET_) = NRLB_MM(max_epi16)(                                                                                                   \
        NRLB_MM(subs_epi16)(NRLB_MM(adds_epi16)(NRLB_MM(subs_epi16)(corr0, energy0), NRLB_MM(subs_epi16)(corr1, energy1)), cross), \
        FLOORM);                                                                                                                   \
  } while (0)

// Per-candidate metric + streaming max-log reduction into max0/max1 (for non-grid patterns).

#define NR_LBEST_EVAL16(I1_, Q1_, BI0, BI1, BI2, BQ0, BQ1, BQ2, C0I_, C0Q_, EI_, EQ_, RRI_, RII_, RRQ_, RIQ_) \
  do {                                                                                                        \
    NRLB_VI metric;                                                                                           \
    NR_LBEST_METRIC16(metric, I1_, Q1_, C0I_, C0Q_, EI_, EQ_, RRI_, RII_, RRQ_, RIQ_);                        \
    const NRLB_VI bm[6] = {(BI0), (BQ0), (BI1), (BQ1), (BI2), (BQ2)};                                         \
    for (int p = 0; p < 6; p++) {                                                                             \
      max0[p] = NRLB_MM(max_epi16)(max0[p], NRLB_BLENDV(metric, SENT, bm[p]));                                \
      max1[p] = NRLB_MM(max_epi16)(max1[p], NRLB_BLENDV(SENT, metric, bm[p]));                                \
    }                                                                                                         \
  } while (0)

// pattern: 0 = 3x3 (9 cand, full BLER), 1 = 6-cand (plus + toward-seed diagonal),

#ifndef NRLB_F16 // the FP16 object builds only its own kernel
void NRLB_NAME(nr_qam64_llr_2layer_lbest_q15_simd)(c16_t *stream0_in,
                                                   c16_t *stream1_in,
                                                   c16_t *ch_mag,
                                                   c16_t *ch_mag_i,
                                                   int16_t *stream0_out,
                                                   c16_t *rho01,
                                                   uint32_t length,
                                                   int pattern,
                                                   float lambda)
{
  const NRLB_VF lam = NRLB_MM(set1_ps)(lambda), epsh = NRLB_MM(set1_ps)(0.5f * NR_2LAYER_CSI_EPS);
  const NRLB_VI SENT = NRLB_MM(set1_epi16)(NR_LBEST_SIMD16_SENT);
  const NRLB_VI FLOORM = NRLB_MM(set1_epi16)(-32000); // keep real metrics above SENT
  const NRLB_VI V1 = NRLB_MM(set1_epi16)(1), V4 = NRLB_MM(set1_epi16)(4);
  const NRLB_VI V7 = NRLB_MM(set1_epi16)(7), Vm7 = NRLB_MM(set1_epi16)(-7);
  const NRLB_VI z = NRLB_SETZERO();

  // When length is not a multiple of NRLB_N, the last SIMD block is shifted back to end exactly
  // at length: REs are independent, so the overlap just rewrites identical LLRs. This replaces the
  // scalar reference for the remainder (~1 us/RE, it dominated the 106/273-PRB cost), which now
  // only runs when length < NRLB_N.
  uint32_t re = 0;
  for (int last = length < NRLB_N; !last; re += NRLB_N) {
    if (re + NRLB_N >= length) {
      re = length - NRLB_N;
      last = 1;
    }
    NRLB_VI z1r, z1i, z2r, z2i, rr, ri, cm0, cm1, dummy;
    NRLB_NAME(nrlbw_load)(&stream0_in[re], &z1r, &z1i);
    NRLB_NAME(nrlbw_load)(&stream1_in[re], &z2r, &z2i);
    NRLB_NAME(nrlbw_load)(&rho01[re], &rr, &ri);
    NRLB_NAME(nrlbw_load)(&ch_mag[re], &cm0, &dummy);
    NRLB_NAME(nrlbw_load)(&ch_mag_i[re], &cm1, &dummy);

    NRLB_VI centerI, centerQ, dirImask, dirQmask, axisLLR[6];
    NRLB_NAME(nrlbw_seed)(z1r, z1i, z2r, z2i, rr, ri, cm0, cm1, lam, epsh, &centerI, &centerQ, &dirImask, &dirQmask, axisLLR);
    (void)dirImask;
    (void)dirQmask;

    // per-RE precompute (int16): conditional-slice operands (/32 scale)
    const NRLB_VI z2r42 = NRLB_NAME(nrlbw_z42d32)(z2r), z2i42 = NRLB_NAME(nrlbw_z42d32)(z2i);
    const NRLB_VI rr5 = NRLB_MM(srai_epi16)(rr, 5), ri5 = NRLB_MM(srai_epi16)(ri, 5);
    const NRLB_VI Pp1_5 = NRLB_MM(mulhi_epi16)(cm1, NRLB_MM(set1_epi16)(NR_LBEST_Q_PP1_OVER32_Q16));
    // metric Q-constants (normal/8 scale; precision-preserving via mulhi, not pre-shift)
    const NRLB_VI C13 = NRLB_MM(set1_epi16)(1264); // 2^13/sqrt(42): mulhi(z, I*C13) = z*I/(8 sqrt42)
    const NRLB_VI CE = NRLB_MM(set1_epi16)(158); // 2^16/(64 sqrt42): energy = cm*|X|^2/(64 sqrt42)
    const NRLB_VI CX = NRLB_MM(set1_epi16)(195); // 2^16/336: cross = rec/336

    NRLB_VI max0[6], max1[6];
    for (int p = 0; p < 6; p++) {
      max0[p] = SENT;
      max1[p] = SENT;
    }

    // 3x3 candidate block, HOISTED (full BLER). Precompute the I-only / Q-only terms
    // for the 3 distinct I-levels and 3 Q-levels, then combine in the 9-pair loop.
    // corr1/energy1/cross + the two level16 slicers stay per-pair (depend on the
    // conditionally-sliced I2,Q2, which couple I and Q).
    static const int offs3[3] = {-2, 0, 2};
    NRLB_VI Iv[3], Qv[3];
    NRLB_VI bIv[3][3], bQv[3][3]; // [level][bit 0..2]
    NRLB_VI c0I[3], c0Q[3], eI[3], eQ[3]; // corr0 / energy0 parts
    NRLB_VI rrI[3], riI[3], rrQ[3], riQ[3]; // slice operands
    for (int k = 0; k < 3; k++) {
      const NRLB_VI I1 =
          NRLB_MM(min_epi16)(NRLB_MM(max_epi16)(NRLB_MM(adds_epi16)(centerI, NRLB_MM(set1_epi16)(offs3[k])), Vm7), V7);
      const NRLB_VI Q1 =
          NRLB_MM(min_epi16)(NRLB_MM(max_epi16)(NRLB_MM(adds_epi16)(centerQ, NRLB_MM(set1_epi16)(offs3[k])), Vm7), V7);
      Iv[k] = I1;
      Qv[k] = Q1;
      const NRLB_VI aI = NRLB_MM(abs_epi16)(I1), aQ = NRLB_MM(abs_epi16)(Q1);
      bIv[k][0] = NRLB_CMPGT16(z, I1);
      bIv[k][1] = NRLB_CMPGT16(aI, V4);
      bIv[k][2] = NRLB_OR(NRLB_CMPEQ16(aI, V1), NRLB_CMPEQ16(aI, V7));
      bQv[k][0] = NRLB_CMPGT16(z, Q1);
      bQv[k][1] = NRLB_CMPGT16(aQ, V4);
      bQv[k][2] = NRLB_OR(NRLB_CMPEQ16(aQ, V1), NRLB_CMPEQ16(aQ, V7));
      c0I[k] = NRLB_MM(mulhi_epi16)(z1r, NRLB_MM(mullo_epi16)(I1, C13));
      c0Q[k] = NRLB_MM(mulhi_epi16)(z1i, NRLB_MM(mullo_epi16)(Q1, C13));
      eI[k] = NRLB_MM(mulhi_epi16)(cm0, NRLB_MM(mullo_epi16)(NRLB_MM(mullo_epi16)(I1, I1), CE));
      eQ[k] = NRLB_MM(mulhi_epi16)(cm0, NRLB_MM(mullo_epi16)(NRLB_MM(mullo_epi16)(Q1, Q1), CE));
      rrI[k] = NRLB_MM(mullo_epi16)(rr5, I1);
      riI[k] = NRLB_MM(mullo_epi16)(ri5, I1);
      rrQ[k] = NRLB_MM(mullo_epi16)(rr5, Q1);
      riQ[k] = NRLB_MM(mullo_epi16)(ri5, Q1);
    }
    if (pattern == 0) { // 3x3 full grid: reduce per-axis. Bits are per-axis, so the max over a
      // bit-subset factorises: max over candidates with I-bit=b = max over I-levels-with-b of
      // (max over Q). Computing maxI[i]=max_Q met[i][.], maxQ[q]=max_I met[.][q] then blending over
      // 3 values/axis replaces 9*6*2 per-candidate blends with 3*3*4 (~2.6x fewer). Bit-exact.
      NRLB_VI met[3][3];
      for (int iI = 0; iI < 3; iI++)
        for (int iQ = 0; iQ < 3; iQ++)
          NR_LBEST_METRIC16(met[iI][iQ], Iv[iI], Qv[iQ], c0I[iI], c0Q[iQ], eI[iI], eQ[iQ], rrI[iI], riI[iI], rrQ[iQ], riQ[iQ]);
      NRLB_VI maxI[3], maxQ[3];
      for (int i = 0; i < 3; i++)
        maxI[i] = NRLB_MM(max_epi16)(met[i][0], NRLB_MM(max_epi16)(met[i][1], met[i][2]));
      for (int q = 0; q < 3; q++)
        maxQ[q] = NRLB_MM(max_epi16)(met[0][q], NRLB_MM(max_epi16)(met[1][q], met[2][q]));
      for (int k = 0; k < 3; k++) // I-bit k -> output p=2k ; Q-bit k -> p=2k+1
        for (int i = 0; i < 3; i++) {
          max0[2 * k] = NRLB_MM(max_epi16)(max0[2 * k], NRLB_BLENDV(maxI[i], SENT, bIv[i][k]));
          max1[2 * k] = NRLB_MM(max_epi16)(max1[2 * k], NRLB_BLENDV(SENT, maxI[i], bIv[i][k]));
          max0[2 * k + 1] = NRLB_MM(max_epi16)(max0[2 * k + 1], NRLB_BLENDV(maxQ[i], SENT, bQv[i][k]));
          max1[2 * k + 1] = NRLB_MM(max_epi16)(max1[2 * k + 1], NRLB_BLENDV(SENT, maxQ[i], bQv[i][k]));
        }
    } else { // 5-plus: center (1,1) + I+-2 + Q+-2
      static const int pI[5] = {1, 0, 2, 1, 1}, pQ[5] = {1, 1, 1, 0, 2};
      for (int c = 0; c < 5; c++) {
        const int iI = pI[c], iQ = pQ[c];
        NR_LBEST_EVAL16(Iv[iI],
                        Qv[iQ],
                        bIv[iI][0],
                        bIv[iI][1],
                        bIv[iI][2],
                        bQv[iQ][0],
                        bQv[iQ][1],
                        bQv[iQ][2],
                        c0I[iI],
                        c0Q[iQ],
                        eI[iI],
                        eQ[iQ],
                        rrI[iI],
                        riI[iI],
                        rrQ[iQ],
                        riQ[iQ]);
      }
      if (pattern == 1) { // 6th: toward-seed diagonal, per-lane select corner 0 vs 2
#define NR_LBEST_SI(a) NRLB_BLENDV((a)[0], (a)[2], dirImask)
#define NR_LBEST_SQ(a) NRLB_BLENDV((a)[0], (a)[2], dirQmask)
        NR_LBEST_EVAL16(NR_LBEST_SI(Iv),
                        NR_LBEST_SQ(Qv),
                        NRLB_BLENDV(bIv[0][0], bIv[2][0], dirImask),
                        NRLB_BLENDV(bIv[0][1], bIv[2][1], dirImask),
                        NRLB_BLENDV(bIv[0][2], bIv[2][2], dirImask),
                        NRLB_BLENDV(bQv[0][0], bQv[2][0], dirQmask),
                        NRLB_BLENDV(bQv[0][1], bQv[2][1], dirQmask),
                        NRLB_BLENDV(bQv[0][2], bQv[2][2], dirQmask),
                        NR_LBEST_SI(c0I),
                        NR_LBEST_SQ(c0Q),
                        NR_LBEST_SI(eI),
                        NR_LBEST_SQ(eQ),
                        NR_LBEST_SI(rrI),
                        NR_LBEST_SI(riI),
                        NR_LBEST_SQ(rrQ),
                        NR_LBEST_SQ(riQ));
#undef NR_LBEST_SI
#undef NR_LBEST_SQ
      }
    }

    NRLB_VI res[6];
    for (int p = 0; p < 6; p++) {
      const NRLB_VI diff = NRLB_MM(subs_epi16)(max0[p], max1[p]);
      // metric is normal/8 scale -> LLR = diff*8 (== float-ref / SIMD scale); saturate via int32
      const NRLB_VI lo = NRLB_MM(slli_epi32)(NRLB_NAME(nrlbw_widen)(diff, 0), 3);
      const NRLB_VI hi = NRLB_MM(slli_epi32)(NRLB_NAME(nrlbw_widen)(diff, 1), 3);
      const NRLB_VI r = NRLB_NAME(nrlbw_pack32)(lo, hi);
      // unspanned bit (one side has no candidate): graded per-axis seed LLR, not +-LLR_SAT
      const NRLB_VI unspanned = NRLB_OR(NRLB_CMPEQ16(max0[p], SENT), NRLB_CMPEQ16(max1[p], SENT));
      res[p] = NRLB_BLENDV(r, axisLLR[p], unspanned);
    }
    NRLB_NAME(nrlbw_store_llr)(stream0_out, re, res, 6);
  }

  if (re < length)
    nr_qam64_llr_2layer_lbest_q15(&stream0_in[re],
                                  &stream1_in[re],
                                  &ch_mag[re],
                                  &ch_mag_i[re],
                                  &stream0_out[re * 6],
                                  &rho01[re],
                                  length - re,
                                  9,
                                  lambda);
}
#endif

#undef NR_LBEST_METRIC16
#undef NR_LBEST_EVAL16

// ============================================================================
// int16/16-lane AVX2 L-best kernel for 2-layer 256QAM (PAM-16, 8 bits/RE).
// Mirrors nr_qam64_llr_2layer_lbest_q15_simd16 with:
//   - sqrt(170) replacing sqrt(42), /8 replacing /4 in Pp
//   - PAM-16 slicer (7 thresholds) replacing PAM-8 (3 thresholds)
//   - 4 bits per axis (b0..b3) instead of 3
//   - 5x5/3x3/5-plus candidate patterns instead of 3x3/6-cand/5-plus
//   - LLR stride 8 per RE instead of 6
// ============================================================================
static inline NRLB_VI NRLB_NAME(nrlbw_level_pam)(NRLB_VI num, NRLB_VI thunit)
{
  const NRLB_VI z = NRLB_SETZERO();
  const NRLB_VI a = NRLB_MM(abs_epi16)(num);
  const NRLB_VI t2 = NRLB_MM(slli_epi16)(thunit, 1);
  const NRLB_VI t4 = NRLB_MM(slli_epi16)(thunit, 2);
  const NRLB_VI t8 = NRLB_MM(slli_epi16)(thunit, 3);
  const NRLB_VI t6 = NRLB_MM(adds_epi16)(t2, t4);
  const NRLB_VI t10 = NRLB_MM(adds_epi16)(t2, t8);
  const NRLB_VI t12 = NRLB_MM(adds_epi16)(t4, t8);
  const NRLB_VI t14 = NRLB_MM(adds_epi16)(t6, t8);
  const NRLB_VI g2 = NRLB_MM(sub_epi16)(z, NRLB_CMPGT16(a, t2));
  const NRLB_VI g4 = NRLB_MM(sub_epi16)(z, NRLB_CMPGT16(a, t4));
  const NRLB_VI g6 = NRLB_MM(sub_epi16)(z, NRLB_CMPGT16(a, t6));
  const NRLB_VI g8 = NRLB_MM(sub_epi16)(z, NRLB_CMPGT16(a, t8));
  const NRLB_VI g10 = NRLB_MM(sub_epi16)(z, NRLB_CMPGT16(a, t10));
  const NRLB_VI g12 = NRLB_MM(sub_epi16)(z, NRLB_CMPGT16(a, t12));
  const NRLB_VI g14 = NRLB_MM(sub_epi16)(z, NRLB_CMPGT16(a, t14));
  // mag = 1 + 2*(g2+g4+g6+g8+g10+g12+g14)
  const NRLB_VI sum = NRLB_MM(adds_epi16)(NRLB_MM(adds_epi16)(NRLB_MM(adds_epi16)(g2, g4), NRLB_MM(adds_epi16)(g6, g8)),
                                          NRLB_MM(adds_epi16)(NRLB_MM(adds_epi16)(g10, g12), g14));
  const NRLB_VI mag = NRLB_MM(adds_epi16)(NRLB_MM(set1_epi16)(1), NRLB_MM(slli_epi16)(sum, 1));
  const NRLB_VI neg = NRLB_CMPGT16(z, num);
  return NRLB_BLENDV(mag, NRLB_MM(sub_epi16)(z, mag), neg);
}

// z * sqrt(170) / 16 (conditional-slice operand; /16 rather than the 64QAM kernel's /32, see rr4)
static inline NRLB_VI NRLB_NAME(nrlbw_z170d16)(NRLB_VI z16)
{
  const NRLB_VI C = NRLB_MM(set1_epi32)(NR_LBEST_Q_SQRT170_Q12);
  const NRLB_VI lo = NRLB_MM(srai_epi32)(NRLB_MM(mullo_epi32)(NRLB_NAME(nrlbw_widen)(z16, 0), C), 16);
  const NRLB_VI hi = NRLB_MM(srai_epi32)(NRLB_MM(mullo_epi32)(NRLB_NAME(nrlbw_widen)(z16, 1), C), 16);
  return NRLB_NAME(nrlbw_pack32)(lo, hi);
}

static inline void NRLB_NAME(nrlbw_axis_llr)(NRLB_VF est, NRLB_VF g, NRLB_VF out[4])
{
  // o = nearest odd to est; oa = nearest odd to |est|
  const NRLB_VF a = NRLB_MM(andnot_ps)(NRLB_MM(set1_ps)(-0.0f), est);
  const NRLB_VF o = NRLB_NAME(nrlbw_round_odd)(est), oa = NRLB_NAME(nrlbw_round_odd)(a);
  // b0 (sign): pos levels [1,15] (bit 0) vs neg [-15,-1] (bit 1)
  out[0] = NRLB_MM(mul_ps)(g,
                           NRLB_MM(sub_ps)(NRLB_NAME(nrlbw_dsq)(NRLB_NAME(nrlbw_clamp)(o, -15.0f, -1.0f), est),
                                           NRLB_NAME(nrlbw_dsq)(NRLB_NAME(nrlbw_clamp)(o, 1.0f, 15.0f), est)));
  // b1 (|L|>8): |L| in [1,7] (bit 0) vs [9,15] (bit 1); by symmetry work in a=|est|
  out[1] = NRLB_MM(mul_ps)(g,
                           NRLB_MM(sub_ps)(NRLB_NAME(nrlbw_dsq)(NRLB_NAME(nrlbw_clamp)(oa, 9.0f, 15.0f), a),
                                           NRLB_NAME(nrlbw_dsq)(NRLB_NAME(nrlbw_clamp)(oa, 1.0f, 7.0f), a)));
  // b2 (||L|-8|>4): |L| in {5,7,9,11} (bit 0) vs {1,3}U{13,15} (bit 1)
  const NRLB_VF d2_0 = NRLB_NAME(nrlbw_dsq)(NRLB_NAME(nrlbw_clamp)(oa, 5.0f, 11.0f), a);
  const NRLB_VF d2_1 = NRLB_MM(min_ps)(NRLB_NAME(nrlbw_dsq)(NRLB_NAME(nrlbw_clamp)(oa, 1.0f, 3.0f), a),
                                       NRLB_NAME(nrlbw_dsq)(NRLB_NAME(nrlbw_clamp)(oa, 13.0f, 15.0f), a));
  out[2] = NRLB_MM(mul_ps)(g, NRLB_MM(sub_ps)(d2_1, d2_0));
  out[3] = NRLB_MM(setzero_ps)(); // b3 always spanned by the search window -> fallback never read
}

static inline void NRLB_NAME(nrlbw_seed256)(NRLB_VI z1r16,
                                            NRLB_VI z1i16,
                                            NRLB_VI z2r16,
                                            NRLB_VI z2i16,
                                            NRLB_VI rr16,
                                            NRLB_VI ri16,
                                            NRLB_VI cm0_16,
                                            NRLB_VI cm1_16,
                                            NRLB_VF lam,
                                            NRLB_VF epsh,
                                            NRLB_VI *centerI,
                                            NRLB_VI *centerQ,
                                            NRLB_VI *dirImask,
                                            NRLB_VI *dirQmask,
                                            NRLB_VI axisLLR[8])
{
  NRLB_VF aL[2][8]; // [half][ {I0,Q0,I1,Q1,I2,Q2,I3,Q3} ]
  const NRLB_VI SQRT170_OVER8 = NRLB_MM(set1_epi32)(NR_LBEST_Q_SQRT170_OVER8_Q14_SIMD);
  const NRLB_VF SQRT170 = NRLB_MM(set1_ps)(13.038404810405298f);
  const NRLB_VF ONE = NRLB_MM(set1_ps)(1.0f);
  NRLB_VI cI[2], cQ[2], dI[2], dQ[2];
  for (int h = 0; h < 2; h++) {
    const NRLB_VF z1r = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z1r16, h)),
                  z1i = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z1i16, h));
    const NRLB_VF z2r = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z2r16, h)),
                  z2i = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z2i16, h));
    const NRLB_VF rr = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(rr16, h)),
                  ri = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(ri16, h));
    const NRLB_VF Pp0 =
        NRLB_MM(cvtepi32_ps)(NRLB_MM(srai_epi32)(NRLB_MM(mullo_epi32)(NRLB_NAME(nrlbw_widen)(cm0_16, h), SQRT170_OVER8), 14));
    const NRLB_VF Pp1 =
        NRLB_MM(cvtepi32_ps)(NRLB_MM(srai_epi32)(NRLB_MM(mullo_epi32)(NRLB_NAME(nrlbw_widen)(cm1_16, h), SQRT170_OVER8), 14));
    // unbiased MMSE seed: ZF with Pp1 -> Pp1 + lambda_eff, lambda_eff = lambda + eps (Pp0 + Pp1) / 2 (noise + CSI error)
    const NRLB_VF Pp1s = NRLB_MM(add_ps)(NRLB_MM(add_ps)(Pp1, lam), NRLB_MM(mul_ps)(epsh, NRLB_MM(add_ps)(Pp0, Pp1)));
    // 2x2 solve (float): x_hat1 = ((Pp1 + lambda) z1 - rho z2) / det,  det = Pp0 (Pp1 + lambda) - |rho|^2
    NRLB_VF det = NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(Pp0, Pp1s), NRLB_MM(add_ps)(NRLB_MM(mul_ps)(rr, rr), NRLB_MM(mul_ps)(ri, ri)));
    det = NRLB_MM(max_ps)(det, ONE);
    const NRLB_VF numr =
        NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(Pp1s, z1r), NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(rr, z2r), NRLB_MM(mul_ps)(ri, z2i)));
    const NRLB_VF numi =
        NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(Pp1s, z1i), NRLB_MM(add_ps)(NRLB_MM(mul_ps)(rr, z2i), NRLB_MM(mul_ps)(ri, z2r)));
    // soft level estimate ~ +-1..15
    const NRLB_VF estI = NRLB_MM(mul_ps)(NRLB_MM(div_ps)(numr, det), SQRT170);
    const NRLB_VF estQ = NRLB_MM(mul_ps)(NRLB_MM(div_ps)(numi, det), SQRT170);
    // graded per-axis PAM-16 LLRs from the soft estimate (g = 0.5*Peff/170, Peff = det/(Pp1 + lambda), the post-MMSE energy)
    const NRLB_VF glin = NRLB_MM(mul_ps)(NRLB_MM(set1_ps)(0.5f / 170.0f), NRLB_MM(div_ps)(det, NRLB_MM(max_ps)(Pp1s, ONE)));
    NRLB_VF aI4[4], aQ4[4];
    NRLB_NAME(nrlbw_axis_llr)(estI, glin, aI4);
    NRLB_NAME(nrlbw_axis_llr)(estQ, glin, aQ4);
    for (int b = 0; b < 4; b++) {
      aL[h][2 * b] = aI4[b];
      aL[h][2 * b + 1] = aQ4[b];
    }
    // nearest odd level: o = 2*round((est-1)/2)+1, clamped to [-15,15]
    const NRLB_VF oI = NRLB_NAME(nrlbw_clamp)(NRLB_NAME(nrlbw_round_odd)(estI), -15.0f, 15.0f);
    const NRLB_VF oQ = NRLB_NAME(nrlbw_clamp)(NRLB_NAME(nrlbw_round_odd)(estQ), -15.0f, 15.0f);
    cI[h] = NRLB_MM(cvtps_epi32)(oI);
    cQ[h] = NRLB_MM(cvtps_epi32)(oQ);
    dI[h] = NRLB_CMPPS_SI(estI, oI, SIMDE_CMP_GT_OQ);
    dQ[h] = NRLB_CMPPS_SI(estQ, oQ, SIMDE_CMP_GT_OQ);
  }
  *centerI = NRLB_NAME(nrlbw_pack32)(cI[0], cI[1]);
  *centerQ = NRLB_NAME(nrlbw_pack32)(cQ[0], cQ[1]);
  *dirImask = NRLB_NAME(nrlbw_pack32)(dI[0], dI[1]);
  *dirQmask = NRLB_NAME(nrlbw_pack32)(dQ[0], dQ[1]);
  for (int p = 0; p < 8; p++) // round to nearest int16, saturating pack (final LLR units)
    axisLLR[p] = NRLB_NAME(nrlbw_pack32)(NRLB_NAME(nrlbw_cvt_sat)(aL[0][p]), NRLB_NAME(nrlbw_cvt_sat)(aL[1][p]));
}

#define NR_LBEST_METRIC16_256(MET_, I1_, Q1_, C0I_, C0Q_, EI_, EQ_, RRI_, RII_, RRQ_, RIQ_)                                        \
  do {                                                                                                                             \
    const NRLB_VI A2I = NRLB_MM(subs_epi16)(z2r170, NRLB_MM(adds_epi16)((RRI_), (RIQ_)));                                          \
    const NRLB_VI A2Q = NRLB_MM(subs_epi16)(z2i170, NRLB_MM(subs_epi16)((RRQ_), (RII_)));                                          \
    const NRLB_VI I2 = NRLB_NAME(nrlbw_level_pam)(A2I, Pp1_4_256);                                                                 \
    const NRLB_VI Q2 = NRLB_NAME(nrlbw_level_pam)(A2Q, Pp1_4_256);                                                                 \
    const NRLB_VI corr0 = NRLB_MM(adds_epi16)((C0I_), (C0Q_));                                                                     \
    const NRLB_VI energy0 = NRLB_MM(adds_epi16)((EI_), (EQ_));                                                                     \
    const NRLB_VI corr1 = NRLB_MM(adds_epi16)(NRLB_MM(mulhi_epi16)(z2rM, NRLB_MM(mullo_epi16)(I2, C13_256)),                       \
                                              NRLB_MM(mulhi_epi16)(z2iM, NRLB_MM(mullo_epi16)(Q2, C13_256)));                      \
    const NRLB_VI energy1 = NRLB_MM(mulhrs_epi16)(                                                                                  \
        cm1M,                                                                                                                      \
        NRLB_MM(mullo_epi16)(NRLB_MM(adds_epi16)(NRLB_MM(mullo_epi16)(I2, I2), NRLB_MM(mullo_epi16)(Q2, Q2)), CE_256));            \
    const NRLB_VI aa = NRLB_MM(adds_epi16)(NRLB_MM(mullo_epi16)((I1_), I2), NRLB_MM(mullo_epi16)((Q1_), Q2));                      \
    const NRLB_VI bb = NRLB_MM(subs_epi16)(NRLB_MM(mullo_epi16)((Q1_), I2), NRLB_MM(mullo_epi16)((I1_), Q2));                      \
    const NRLB_VI cross = NRLB_MM(adds_epi16)(NRLB_MM(mulhrs_epi16)(rrM, NRLB_MM(mullo_epi16)(aa, CX_256)),                         \
                                              NRLB_MM(mulhrs_epi16)(riM, NRLB_MM(mullo_epi16)(bb, CX_256)));                        \
    (MET_) = NRLB_MM(max_epi16)(                                                                                                   \
        NRLB_MM(subs_epi16)(NRLB_MM(adds_epi16)(NRLB_MM(subs_epi16)(corr0, energy0), NRLB_MM(subs_epi16)(corr1, energy1)), cross), \
        FLOORM);                                                                                                                   \
  } while (0)

#define NR_LBEST_EVAL16_256(I1_, Q1_, BI0, BI1, BI2, BI3, BQ0, BQ1, BQ2, BQ3, C0I_, C0Q_, EI_, EQ_, RRI_, RII_, RRQ_, RIQ_) \
  do {                                                                                                                      \
    NRLB_VI metric;                                                                                                         \
    NR_LBEST_METRIC16_256(metric, I1_, Q1_, C0I_, C0Q_, EI_, EQ_, RRI_, RII_, RRQ_, RIQ_);                                  \
    const NRLB_VI bm[8] = {(BI0), (BQ0), (BI1), (BQ1), (BI2), (BQ2), (BI3), (BQ3)};                                         \
    for (int p = 0; p < 8; p++) {                                                                                           \
      max0[p] = NRLB_MM(max_epi16)(max0[p], NRLB_BLENDV(metric, SENT, bm[p]));                                              \
      max1[p] = NRLB_MM(max_epi16)(max1[p], NRLB_BLENDV(SENT, metric, bm[p]));                                              \
    }                                                                                                                       \
  } while (0)

#ifndef NRLB_F16 // the FP16 object builds only its own kernel
void NRLB_NAME(nr_qam256_llr_2layer_lbest_q15_simd)(c16_t *stream0_in,
                                                    c16_t *stream1_in,
                                                    c16_t *ch_mag,
                                                    c16_t *ch_mag_i,
                                                    int16_t *stream0_out,
                                                    c16_t *rho01,
                                                    uint32_t length,
                                                    int pattern,
                                                    float lambda)
{
  const NRLB_VF lam = NRLB_MM(set1_ps)(lambda), epsh = NRLB_MM(set1_ps)(0.5f * NR_2LAYER_CSI_EPS);
  const NRLB_VI SENT = NRLB_MM(set1_epi16)(NR_LBEST_SIMD16_SENT);
  const NRLB_VI FLOORM = NRLB_MM(set1_epi16)(-32000);
  const NRLB_VI V15 = NRLB_MM(set1_epi16)(15), Vm15 = NRLB_MM(set1_epi16)(-15);
  const NRLB_VI z = NRLB_SETZERO();

  // 256QAM metric Q-constants. The metric runs at scale 1/4 of the float reference (twice the
  // 64QAM kernel's 1/8): at 1/8 the per-term mulhi truncations (~1 unit = 8 LLR each) add up to an
  // LLR error that floors 256QAM MCS27 BLER above MMSE at high SNR (float ML on the same inputs has
  // no floor). Headroom: I*C13 <= 18855, |metric| <~ 20000 with the 2*cm, 2*rho of the
  // mulhrs products below.
  //   C13_256 = round(2^14/sqrt(170))          => mulhi(z, I*C13) = z*I/(4*sqrt(170))
  //   CE_256  = round(2^16/(8*8*2*sqrt(170)))  => energy = (2cm)*|X|^2/(8*8*2*sqrt(170))
  //   CX_256  = round(2^16/(8*170))            => cross = mulhi(2rho, .)
  const NRLB_VI C13_256 = NRLB_MM(set1_epi16)(1257);
  const NRLB_VI CE_256 = NRLB_MM(set1_epi16)(39);
  const NRLB_VI CX_256 = NRLB_MM(set1_epi16)(48);

  // When length is not a multiple of NRLB_N, the last SIMD block is shifted back to end exactly
  // at length: REs are independent, so the overlap just rewrites identical LLRs. This replaces the
  // scalar reference for the remainder (~1 us/RE, it dominated the 106/273-PRB cost), which now
  // only runs when length < NRLB_N.
  uint32_t re = 0;
  for (int last = length < NRLB_N; !last; re += NRLB_N) {
    if (re + NRLB_N >= length) {
      re = length - NRLB_N;
      last = 1;
    }
    NRLB_VI z1r, z1i, z2r, z2i, rr, ri, cm0, cm1, dummy;
    NRLB_NAME(nrlbw_load)(&stream0_in[re], &z1r, &z1i);
    NRLB_NAME(nrlbw_load)(&stream1_in[re], &z2r, &z2i);
    NRLB_NAME(nrlbw_load)(&rho01[re], &rr, &ri);
    NRLB_NAME(nrlbw_load)(&ch_mag[re], &cm0, &dummy);
    NRLB_NAME(nrlbw_load)(&ch_mag_i[re], &cm1, &dummy);

    NRLB_VI centerI, centerQ, dirImask, dirQmask, axisLLR[8];
    NRLB_NAME(nrlbw_seed256)(z1r, z1i, z2r, z2i, rr, ri, cm0, cm1, lam, epsh, &centerI, &centerQ, &dirImask, &dirQmask, axisLLR);
    (void)dirImask;
    (void)dirQmask;

    // per-RE-vector precomputed quantities (int16 scale, /32 shift)
    // Conditional-slice operands at /16 with rounded rho (the 64QAM kernel uses /32, truncated):
    // at /32, rho>>5 (~30-170) carries up to ~15 units of interference error against PAM-16
    // decision spacings of ~100-350, mis-slicing the nuisance near boundaries (~1 dB at 256QAM).
    const NRLB_VI z2r170 = NRLB_NAME(nrlbw_z170d16)(z2r), z2i170 = NRLB_NAME(nrlbw_z170d16)(z2i);
    const NRLB_VI V8r = NRLB_MM(set1_epi16)(8);
    const NRLB_VI rr4 = NRLB_MM(srai_epi16)(NRLB_MM(adds_epi16)(rr, V8r), 4),
                  ri4 = NRLB_MM(srai_epi16)(NRLB_MM(adds_epi16)(ri, V8r), 4);
    const NRLB_VI Pp1_4_256 = NRLB_MM(mulhi_epi16)(cm1, NRLB_MM(set1_epi16)(NR_LBEST_Q_PP1_OVER16_Q16_256));
    // Metric operands: cm and rho enter through mulhrs (2*mulhi, rounded) for the 1/4 metric scale
    // (see C13_256): doubling them in int16 instead saturates for cm/rho > 16383. All of them are
    // pre-scaled by (exact constant / quantized constant), since the int16 constants are off by
    // -0.03/+0.69/+0.39% (C13/CE/CX) -- up to ~7 metric units between inner and corner points.
    // Once per vector, nothing per candidate: x' = x + mulhi(x, round(f*2^16)). The slicer operands
    // (z2r170, rr4/ri4, Pp1_4_256) keep the original values.
#define NR_FIX(x, f) NRLB_MM(adds_epi16)((x), NRLB_MM(mulhi_epi16)((x), NRLB_MM(set1_epi16)(f)))
    const NRLB_VI z1rM = NR_FIX(z1r, -21), z1iM = NR_FIX(z1i, -21), z2rM = NR_FIX(z2r, -21), z2iM = NR_FIX(z2i, -21);
    const NRLB_VI cm0M = NR_FIX(cm0, 451), cm1M = NR_FIX(cm1, 451);
    const NRLB_VI rrM = NR_FIX(rr, 257), riM = NR_FIX(ri, 257);
#undef NR_FIX

    NRLB_VI max0[8], max1[8];
    for (int p = 0; p < 8; p++) {
      max0[p] = SENT;
      max1[p] = SENT;
    }

    // ---- 5x5 block (hoisted) ----
    // Precompute 5 I-levels and 5 Q-levels, then evaluate all or a subset of pairs.
    // Offsets: {-4,-2,0,+2,+4} around seed center.
    static const int offs5[5] = {-4, -2, 0, 2, 4};
    NRLB_VI Iv[5], Qv[5];
    NRLB_VI bIv[5][4], bQv[5][4]; // [level][bit 0..3]
    NRLB_VI c0I[5], c0Q[5], eI[5], eQ[5];
    NRLB_VI rrI[5], riI[5], rrQ[5], riQ[5];

    const NRLB_VI V8 = NRLB_MM(set1_epi16)(8), V4 = NRLB_MM(set1_epi16)(4), V2 = NRLB_MM(set1_epi16)(2);

    for (int k = 0; k < 5; k++) {
      const NRLB_VI I1 =
          NRLB_MM(min_epi16)(NRLB_MM(max_epi16)(NRLB_MM(adds_epi16)(centerI, NRLB_MM(set1_epi16)(offs5[k])), Vm15), V15);
      const NRLB_VI Q1 =
          NRLB_MM(min_epi16)(NRLB_MM(max_epi16)(NRLB_MM(adds_epi16)(centerQ, NRLB_MM(set1_epi16)(offs5[k])), Vm15), V15);
      Iv[k] = I1;
      Qv[k] = Q1;

      // 256QAM Gray bit labeling per axis (b0..b3):
      //   b0 = sign (level < 0)
      //   b1 = |level| > 8
      //   b2 = ||level|-8| > 4
      //   b3 = |||level|-8|-4| > 2
      const NRLB_VI aI = NRLB_MM(abs_epi16)(I1);
      const NRLB_VI m2I = NRLB_MM(abs_epi16)(NRLB_MM(sub_epi16)(aI, V8));
      const NRLB_VI m3I = NRLB_MM(abs_epi16)(NRLB_MM(sub_epi16)(m2I, V4));
      bIv[k][0] = NRLB_CMPGT16(z, I1);
      bIv[k][1] = NRLB_CMPGT16(aI, V8);
      bIv[k][2] = NRLB_CMPGT16(m2I, V4);
      bIv[k][3] = NRLB_CMPGT16(m3I, V2);

      const NRLB_VI aQ = NRLB_MM(abs_epi16)(Q1);
      const NRLB_VI m2Q = NRLB_MM(abs_epi16)(NRLB_MM(sub_epi16)(aQ, V8));
      const NRLB_VI m3Q = NRLB_MM(abs_epi16)(NRLB_MM(sub_epi16)(m2Q, V4));
      bQv[k][0] = NRLB_CMPGT16(z, Q1);
      bQv[k][1] = NRLB_CMPGT16(aQ, V8);
      bQv[k][2] = NRLB_CMPGT16(m2Q, V4);
      bQv[k][3] = NRLB_CMPGT16(m3Q, V2);

      c0I[k] = NRLB_MM(mulhi_epi16)(z1rM, NRLB_MM(mullo_epi16)(I1, C13_256));
      c0Q[k] = NRLB_MM(mulhi_epi16)(z1iM, NRLB_MM(mullo_epi16)(Q1, C13_256));
      eI[k] = NRLB_MM(mulhrs_epi16)(cm0M, NRLB_MM(mullo_epi16)(NRLB_MM(mullo_epi16)(I1, I1), CE_256));
      eQ[k] = NRLB_MM(mulhrs_epi16)(cm0M, NRLB_MM(mullo_epi16)(NRLB_MM(mullo_epi16)(Q1, Q1), CE_256));
      rrI[k] = NRLB_MM(mullo_epi16)(rr4, I1);
      riI[k] = NRLB_MM(mullo_epi16)(ri4, I1);
      rrQ[k] = NRLB_MM(mullo_epi16)(rr4, Q1);
      riQ[k] = NRLB_MM(mullo_epi16)(ri4, Q1);
    }

    if (pattern == 0) { // 5x5: all 25 grid pairs
      for (int iI = 0; iI < 5; iI++)
        for (int iQ = 0; iQ < 5; iQ++)
          NR_LBEST_EVAL16_256(Iv[iI],
                              Qv[iQ],
                              bIv[iI][0],
                              bIv[iI][1],
                              bIv[iI][2],
                              bIv[iI][3],
                              bQv[iQ][0],
                              bQv[iQ][1],
                              bQv[iQ][2],
                              bQv[iQ][3],
                              c0I[iI],
                              c0Q[iQ],
                              eI[iI],
                              eQ[iQ],
                              rrI[iI],
                              riI[iI],
                              rrQ[iQ],
                              riQ[iQ]);
    } else if (pattern == 1) { // 3x3 full grid (center 3 levels): reduce per-axis (bit-exact, ~2.7x
      // fewer max-log ops than 9*8*2 per-candidate blends). center indices {1,2,3} = offsets {-2,0,+2}.
      static const int ci3[3] = {1, 2, 3};
      NRLB_VI met[3][3];
      for (int a = 0; a < 3; a++)
        for (int b = 0; b < 3; b++) {
          const int iI = ci3[a], iQ = ci3[b];
          NR_LBEST_METRIC16_256(met[a][b], Iv[iI], Qv[iQ], c0I[iI], c0Q[iQ], eI[iI], eQ[iQ], rrI[iI], riI[iI], rrQ[iQ], riQ[iQ]);
        }
      NRLB_VI maxI[3], maxQ[3];
      for (int i = 0; i < 3; i++)
        maxI[i] = NRLB_MM(max_epi16)(met[i][0], NRLB_MM(max_epi16)(met[i][1], met[i][2]));
      for (int q = 0; q < 3; q++)
        maxQ[q] = NRLB_MM(max_epi16)(met[0][q], NRLB_MM(max_epi16)(met[1][q], met[2][q]));
      for (int k = 0; k < 4; k++) // I-bit k -> output p=2k ; Q-bit k -> p=2k+1
        for (int i = 0; i < 3; i++) {
          const NRLB_VI bI = bIv[ci3[i]][k], bQ = bQv[ci3[i]][k];
          max0[2 * k] = NRLB_MM(max_epi16)(max0[2 * k], NRLB_BLENDV(maxI[i], SENT, bI));
          max1[2 * k] = NRLB_MM(max_epi16)(max1[2 * k], NRLB_BLENDV(SENT, maxI[i], bI));
          max0[2 * k + 1] = NRLB_MM(max_epi16)(max0[2 * k + 1], NRLB_BLENDV(maxQ[i], SENT, bQ));
          max1[2 * k + 1] = NRLB_MM(max_epi16)(max1[2 * k + 1], NRLB_BLENDV(SENT, maxQ[i], bQ));
        }
    } else if (pattern == 2) { // 5-plus: center + +-4 each axis (5 cand)
      // indices: (2,2)=center, (0,2)=I-4, (4,2)=I+4, (2,0)=Q-4, (2,4)=Q+4
      static const int pI5[5] = {2, 0, 4, 2, 2}, pQ5[5] = {2, 2, 2, 0, 4};
      for (int c = 0; c < 5; c++) {
        const int iI = pI5[c], iQ = pQ5[c];
        NR_LBEST_EVAL16_256(Iv[iI],
                            Qv[iQ],
                            bIv[iI][0],
                            bIv[iI][1],
                            bIv[iI][2],
                            bIv[iI][3],
                            bQv[iQ][0],
                            bQv[iQ][1],
                            bQv[iQ][2],
                            bQv[iQ][3],
                            c0I[iI],
                            c0Q[iQ],
                            eI[iI],
                            eQ[iQ],
                            rrI[iI],
                            riI[iI],
                            rrQ[iQ],
                            riQ[iQ]);
      }
    } else { // pattern == 3: FULL exact ML — all 256 points (16x16 grid, conditional slice)
      // Enumerate every PAM-16 level on both axes (no seed centering). Per-level quantities
      // that depend only on the level (Gray bits, |x|^2 energy, rho5*level) are identical
      // for the I- and Q-candidate roles; only the correlation term differs (z1r vs z1i).
      static const int lv16[16] = {-15, -13, -11, -9, -7, -5, -3, -1, 1, 3, 5, 7, 9, 11, 13, 15};
      NRLB_VI fI[16], fbI[16][4], fc0I[16], fc0Q[16], feI[16], frrI[16], friI[16];
      for (int k = 0; k < 16; k++) {
        const NRLB_VI I1 = NRLB_MM(set1_epi16)(lv16[k]);
        fI[k] = I1;
        const NRLB_VI aI = NRLB_MM(abs_epi16)(I1);
        const NRLB_VI m2I = NRLB_MM(abs_epi16)(NRLB_MM(sub_epi16)(aI, V8));
        const NRLB_VI m3I = NRLB_MM(abs_epi16)(NRLB_MM(sub_epi16)(m2I, V4));
        fbI[k][0] = NRLB_CMPGT16(z, I1);
        fbI[k][1] = NRLB_CMPGT16(aI, V8);
        fbI[k][2] = NRLB_CMPGT16(m2I, V4);
        fbI[k][3] = NRLB_CMPGT16(m3I, V2);
        fc0I[k] = NRLB_MM(mulhi_epi16)(z1rM, NRLB_MM(mullo_epi16)(I1, C13_256));
        fc0Q[k] = NRLB_MM(mulhi_epi16)(z1iM, NRLB_MM(mullo_epi16)(I1, C13_256));
        feI[k] = NRLB_MM(mulhrs_epi16)(cm0M, NRLB_MM(mullo_epi16)(NRLB_MM(mullo_epi16)(I1, I1), CE_256));
        frrI[k] = NRLB_MM(mullo_epi16)(rr4, I1);
        friI[k] = NRLB_MM(mullo_epi16)(ri4, I1);
      }
      for (int iI = 0; iI < 16; iI++)
        for (int iQ = 0; iQ < 16; iQ++)
          NR_LBEST_EVAL16_256(fI[iI],
                              fI[iQ],
                              fbI[iI][0],
                              fbI[iI][1],
                              fbI[iI][2],
                              fbI[iI][3],
                              fbI[iQ][0],
                              fbI[iQ][1],
                              fbI[iQ][2],
                              fbI[iQ][3],
                              fc0I[iI],
                              fc0Q[iQ],
                              feI[iI],
                              feI[iQ],
                              frrI[iI],
                              friI[iI],
                              frrI[iQ],
                              friI[iQ]);
    }

    // ~35-50% of these LLRs are |.|>=128 and clip at the int8 LDPC input; that is harmless. What is not
    // is the 1/4-scale metric resolution: small-|LLR| bits take the wrong sign often enough to cost
    // 0.2-1.4 dB (see nr_compute_ML_llr); nr_qam256_llr_2layer_lbest_f32_simd is the fp32 alternative.
    NRLB_VI res[8];
    for (int p = 0; p < 8; p++) {
      const NRLB_VI diff = NRLB_MM(subs_epi16)(max0[p], max1[p]);
      // metric is at 1/4 scale -> LLR = diff*4; saturate via int32 widening
      const NRLB_VI lo = NRLB_MM(slli_epi32)(NRLB_NAME(nrlbw_widen)(diff, 0), 2);
      const NRLB_VI hi = NRLB_MM(slli_epi32)(NRLB_NAME(nrlbw_widen)(diff, 1), 2);
      NRLB_VI r = NRLB_NAME(nrlbw_pack32)(lo, hi);
      // unspanned bit (one side has no candidate): use the graded per-axis seed LLR instead of
      // the hard +-LLR_SAT (which wrecks the coarse/MSB bits a local search cannot span).
      const NRLB_VI unspanned = NRLB_OR(NRLB_CMPEQ16(max0[p], SENT), NRLB_CMPEQ16(max1[p], SENT));
      res[p] = NRLB_BLENDV(r, axisLLR[p], unspanned);
    }
    NRLB_NAME(nrlbw_store_llr)(stream0_out, re, res, 8);
  }

  if (re < length)
    // scalar tail: full search (pattern 3) for ML, else the 5x5 candidate set (pattern 0)
    nr_qam256_llr_2layer_lbest_q15(&stream0_in[re],
                                   &stream1_in[re],
                                   &ch_mag[re],
                                   &ch_mag_i[re],
                                   &stream0_out[re * 8],
                                   &rho01[re],
                                   length - re,
                                   pattern == 3 ? 3 : 0,
                                   lambda);
}
#endif

#undef NR_LBEST_METRIC16_256
#undef NR_LBEST_EVAL16_256

// ============================================================================
// fp32-metric variant of the 2-layer 64QAM and 256QAM L-best / full-ML kernels. Same seed, candidate set
// (pattern 1 = 3x3 window, 3 = full grid), conditional nuisance slice and graded fallback as the int16
// kernels, plus the adaptive window (pattern 1: full grid on the REs below NR_LBEST_WIDE_SINR*), but the candidate metric and the max-log reduction run in fp32 on half-width vectors,
// following nr_qam{64,256}_llr_2layer_lbest (float references) term by term. The int16 metrics need a
// coarse scale (1/8 for 64QAM, 1/4 for 256QAM) for their dynamic range, plus truncations: bits with a
// small |LLR| (the ones a high-rate code has to resolve) then take the wrong sign often enough to cost
// ~1.2-1.4 dB at 70% throughput for 256QAM MCS 27 (4 RX, TDL-A), and for 64QAM with 2 RX at high SNR.
// ============================================================================
static inline NRLB_VF NRLB_NAME(nrlbw_sel_ps)(NRLB_VI mask, NRLB_VF a, NRLB_VF b) // mask ? a : b (32-bit lanes)
{
  return NRLB_CASTSI_PS(NRLB_BLENDV(NRLB_CASTPS_SI(b), NRLB_CASTPS_SI(a), mask));
}

// Gray bits (38.211 5.1) of a float PAM level vector, nb bits per axis (all-ones 32-bit lanes where the bit is
// 1): b0 = sign, then |L| against halving thresholds M/2, M/4, ...
static inline void NRLB_NAME(nrlbw_bits_ps)(NRLB_VF lvl, int nb, NRLB_VI b[4])
{
  const NRLB_VF nz = NRLB_MM(set1_ps)(-0.0f);
  b[0] = NRLB_CMPPS_SI(lvl, NRLB_MM(setzero_ps)(), SIMDE_CMP_LT_OQ);
  NRLB_VF r = NRLB_MM(andnot_ps)(nz, lvl);
  float t = (float)(1 << (nb - 1));
  for (int k = 1; k < nb; k++) {
    const NRLB_VF tv = NRLB_MM(set1_ps)(t);
    b[k] = NRLB_CMPPS_SI(r, tv, SIMDE_CMP_GT_OQ);
    r = NRLB_MM(andnot_ps)(nz, NRLB_MM(sub_ps)(r, tv));
    t *= 0.5f;
  }
}

// Candidate grid of one half vector: nI levels per axis, the window around the seed centre (cI, cQ) or, if full,
// all M levels; conditional nuisance slice and max-log per axis. llr / uns: per-bit LLR and "one side unspanned".
static inline void NRLB_NAME(nrlbw_grid_f32)(const NRLB_VF z[4], // z1r, z1i, z2r, z2i
                                              NRLB_VF rr,
                                              NRLB_VF ri,
                                              NRLB_VF hP0,
                                              NRLB_VF hP1,
                                              NRLB_VF sl,
                                              NRLB_VF cI,
                                              NRLB_VF cQ,
                                              int full,
                                              int nb,
                                              NRLB_VI llr[8],
                                              NRLB_VI uns[8])
{
  const int M = 1 << nb, nI = full ? M : 3;
  const float Lmax = (float)(M - 1);
  const NRLB_VF NA = NRLB_MM(set1_ps)(nb == 3 ? 1.0f / 6.48074069840786f : 1.0f / 13.038404810405298f);
  const NRLB_VF NEG = NRLB_MM(set1_ps)(-1e30f);
  const NRLB_VF z1r = z[0], z1i = z[1], z2r = z[2], z2i = z[3];
  // per-axis candidate levels: the 3x3 window around the seed (clamped as the int16 kernels) or all M
  NRLB_VF lvI[16], lvQ[16];
  NRLB_VI bI[16][4], bQ[16][4];
  for (int k = 0; k < nI; k++) {
    if (full) {
      lvI[k] = lvQ[k] = NRLB_MM(set1_ps)((float)(2 * k - (M - 1)));
    } else {
      const NRLB_VF o = NRLB_MM(set1_ps)((float)(2 * k - 2));
      lvI[k] = NRLB_NAME(nrlbw_clamp)(NRLB_MM(add_ps)(cI, o), -Lmax, Lmax);
      lvQ[k] = NRLB_NAME(nrlbw_clamp)(NRLB_MM(add_ps)(cQ, o), -Lmax, Lmax);
    }
    NRLB_NAME(nrlbw_bits_ps)(lvI[k], nb, bI[k]);
    NRLB_NAME(nrlbw_bits_ps)(lvQ[k], nb, bQ[k]);
  }

  // max-log per axis: rows (I level) and columns (Q level) of the candidate grid
  NRLB_VF rowMax[16], colMax[16];
  for (int k = 0; k < nI; k++)
    rowMax[k] = colMax[k] = NEG;
  for (int a = 0; a < nI; a++) {
    const NRLB_VF x1r = NRLB_MM(mul_ps)(lvI[a], NA);
    for (int b = 0; b < nI; b++) {
      const NRLB_VF x1i = NRLB_MM(mul_ps)(lvQ[b], NA);
      // Re{x1* z1} - P0 |x1|^2 / 2
      NRLB_VF m = NRLB_MM(sub_ps)(NRLB_MM(add_ps)(NRLB_MM(mul_ps)(x1r, z1r), NRLB_MM(mul_ps)(x1i, z1i)),
                                  NRLB_MM(mul_ps)(hP0, NRLB_MM(add_ps)(NRLB_MM(mul_ps)(x1r, x1r), NRLB_MM(mul_ps)(x1i, x1i))));
      // conditional ML nuisance x2* = Q((z2 - conj(rho) x1) / P1)
      const NRLB_VF cr = NRLB_MM(add_ps)(NRLB_MM(mul_ps)(rr, x1r), NRLB_MM(mul_ps)(ri, x1i));
      const NRLB_VF ci = NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(rr, x1i), NRLB_MM(mul_ps)(ri, x1r));
      const NRLB_VF I2 = NRLB_NAME(nrlbw_clamp)(NRLB_NAME(nrlbw_round_odd)(NRLB_MM(mul_ps)(NRLB_MM(sub_ps)(z2r, cr), sl)), -Lmax, Lmax);
      const NRLB_VF Q2 = NRLB_NAME(nrlbw_clamp)(NRLB_NAME(nrlbw_round_odd)(NRLB_MM(mul_ps)(NRLB_MM(sub_ps)(z2i, ci), sl)), -Lmax, Lmax);
      const NRLB_VF x2r = NRLB_MM(mul_ps)(I2, NA), x2i = NRLB_MM(mul_ps)(Q2, NA);
      m = NRLB_MM(add_ps)(m, NRLB_MM(sub_ps)(NRLB_MM(add_ps)(NRLB_MM(mul_ps)(x2r, z2r), NRLB_MM(mul_ps)(x2i, z2i)),
                                             NRLB_MM(mul_ps)(hP1, NRLB_MM(add_ps)(NRLB_MM(mul_ps)(x2r, x2r), NRLB_MM(mul_ps)(x2i, x2i)))));
      // - Re{x1* rho x2}
      const NRLB_VF cxr = NRLB_MM(add_ps)(NRLB_MM(mul_ps)(x1r, x2r), NRLB_MM(mul_ps)(x1i, x2i));
      const NRLB_VF cxi = NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(x1r, x2i), NRLB_MM(mul_ps)(x1i, x2r));
      m = NRLB_MM(sub_ps)(m, NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(rr, cxr), NRLB_MM(mul_ps)(ri, cxi)));
      rowMax[a] = NRLB_MM(max_ps)(rowMax[a], m);
      colMax[b] = NRLB_MM(max_ps)(colMax[b], m);
    }
  }
  const NRLB_VF lim = NRLB_MM(set1_ps)(-1e29f);
  for (int k = 0; k < nb; k++) {
    NRLB_VF m0I = NEG, m1I = NEG, m0Q = NEG, m1Q = NEG;
    for (int a = 0; a < nI; a++) {
      m0I = NRLB_MM(max_ps)(m0I, NRLB_NAME(nrlbw_sel_ps)(bI[a][k], NEG, rowMax[a]));
      m1I = NRLB_MM(max_ps)(m1I, NRLB_NAME(nrlbw_sel_ps)(bI[a][k], rowMax[a], NEG));
      m0Q = NRLB_MM(max_ps)(m0Q, NRLB_NAME(nrlbw_sel_ps)(bQ[a][k], NEG, colMax[a]));
      m1Q = NRLB_MM(max_ps)(m1Q, NRLB_NAME(nrlbw_sel_ps)(bQ[a][k], colMax[a], NEG));
    }
    llr[2 * k] = NRLB_MM(cvttps_epi32)(NRLB_NAME(nrlbw_clamp)(NRLB_MM(sub_ps)(m0I, m1I), -32768.0f, 32767.0f));
    llr[2 * k + 1] = NRLB_MM(cvttps_epi32)(NRLB_NAME(nrlbw_clamp)(NRLB_MM(sub_ps)(m0Q, m1Q), -32768.0f, 32767.0f));
    uns[2 * k] = NRLB_OR(NRLB_CMPPS_SI(m0I, lim, SIMDE_CMP_LT_OQ), NRLB_CMPPS_SI(m1I, lim, SIMDE_CMP_LT_OQ));
    uns[2 * k + 1] = NRLB_OR(NRLB_CMPPS_SI(m0Q, lim, SIMDE_CMP_LT_OQ), NRLB_CMPPS_SI(m1Q, lim, SIMDE_CMP_LT_OQ));
  }
}

// pattern 1: 3x3 window, and the full grid on the REs below NR_LBEST_WIDE_SINR* (adaptive window); 3: full grid.
static inline void NRLB_NAME(nrlbw_lbest_f32)(c16_t *stream0_in,
                                               c16_t *stream1_in,
                                               c16_t *ch_mag,
                                               c16_t *ch_mag_i,
                                               int16_t *stream0_out,
                                               c16_t *rho01,
                                               uint32_t length,
                                               int pattern,
                                               float lambda,
                                               int nb)
{
  // nb = 3: 64QAM (PAM-8, cm = ||h||^2 * 4/sqrt(42)); nb = 4: 256QAM (PAM-16, cm = ||h||^2 * 8/sqrt(170))
  const float sqK = nb == 3 ? 6.48074069840786f : 13.038404810405298f;
  const int M = 1 << nb, Qm = 2 * nb;
  const float psc = sqK / (float)(M / 2); // cm -> ||h||^2
  const NRLB_VF lam = NRLB_MM(set1_ps)(lambda), epsh = NRLB_MM(set1_ps)(0.5f * NR_2LAYER_CSI_EPS);
  const NRLB_VF SK = NRLB_MM(set1_ps)(sqK);
  const NRLB_VF PSC = NRLB_MM(set1_ps)(psc);
  const NRLB_VF HALF = NRLB_MM(set1_ps)(0.5f), ZERO = NRLB_MM(setzero_ps)();
  const NRLB_VF WT = NRLB_MM(set1_ps)(nb == 4 ? NR_LBEST_WIDE_SINR256 : NR_LBEST_WIDE_SINR64);
  const int full = (pattern == 3);

  uint32_t re = 0;
  for (int last = length < NRLB_N; !last; re += NRLB_N) {
    if (re + NRLB_N >= length) {
      re = length - NRLB_N;
      last = 1;
    }
    NRLB_VI z1r16, z1i16, z2r16, z2i16, rr16, ri16, cm0, cm1, dummy;
    NRLB_NAME(nrlbw_load)(&stream0_in[re], &z1r16, &z1i16);
    NRLB_NAME(nrlbw_load)(&stream1_in[re], &z2r16, &z2i16);
    NRLB_NAME(nrlbw_load)(&rho01[re], &rr16, &ri16);
    NRLB_NAME(nrlbw_load)(&ch_mag[re], &cm0, &dummy);
    NRLB_NAME(nrlbw_load)(&ch_mag_i[re], &cm1, &dummy);

    NRLB_VI centerI, centerQ, dirImask, dirQmask, axisLLR[8];
    if (nb == 4)
      NRLB_NAME(nrlbw_seed256)(z1r16, z1i16, z2r16, z2i16, rr16, ri16, cm0, cm1, lam, epsh, &centerI, &centerQ, &dirImask, &dirQmask, axisLLR);
    else
      NRLB_NAME(nrlbw_seed)(z1r16, z1i16, z2r16, z2i16, rr16, ri16, cm0, cm1, lam, epsh, &centerI, &centerQ, &dirImask, &dirQmask, axisLLR);
    (void)dirImask;
    (void)dirQmask;

    NRLB_VI llr32[8][2], uns32[8][2];
    for (int h = 0; h < 2; h++) {
      const NRLB_VF z[4] = {NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z1r16, h)),
                            NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z1i16, h)),
                            NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z2r16, h)),
                            NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z2i16, h))};
      const NRLB_VF rr = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(rr16, h)), ri = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(ri16, h));
      const NRLB_VF P0 = NRLB_MM(mul_ps)(NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(cm0, h)), PSC);
      const NRLB_VF P1 = NRLB_MM(mul_ps)(NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(cm1, h)), PSC);
      const NRLB_VF hP0 = NRLB_MM(mul_ps)(HALF, P0), hP1 = NRLB_MM(mul_ps)(HALF, P1);
      // nuisance slice scale sqrt(K)/P1 (0 where P1 == 0, as the references)
      const NRLB_VF sl = NRLB_NAME(nrlbw_sel_ps)(NRLB_CMPPS_SI(P1, ZERO, SIMDE_CMP_NEQ_OQ), NRLB_MM(div_ps)(SK, P1), ZERO);
      const NRLB_VF cI = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(centerI, h));
      const NRLB_VF cQ = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(centerQ, h));
      NRLB_VI l[8], u[8];
      NRLB_NAME(nrlbw_grid_f32)(z, rr, ri, hP0, hP1, sl, cI, cQ, full, nb, l, u);
      if (!full) {
        // adaptive window (nr_lbest_wide_re): the full grid replaces the window on the low-SINR REs
        const NRLB_VF lame = NRLB_MM(add_ps)(lam, NRLB_MM(mul_ps)(epsh, NRLB_MM(add_ps)(P0, P1)));
        const NRLB_VF P1s = NRLB_MM(add_ps)(P1, lame);
        const NRLB_VF det = NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(P0, P1s), NRLB_MM(add_ps)(NRLB_MM(mul_ps)(rr, rr), NRLB_MM(mul_ps)(ri, ri)));
        const NRLB_VI wide = NRLB_CMPPS_SI(det, NRLB_MM(mul_ps)(WT, NRLB_MM(mul_ps)(lame, P1s)), SIMDE_CMP_LT_OQ);
        if (NRLB_ANY32(wide)) {
          NRLB_VI lf[8], uf[8];
          NRLB_NAME(nrlbw_grid_f32)(z, rr, ri, hP0, hP1, sl, cI, cQ, 1, nb, lf, uf);
          for (int p = 0; p < Qm; p++) {
            l[p] = NRLB_BLENDV(l[p], lf[p], wide);
            u[p] = NRLB_ANDNOT(wide, u[p]);
          }
        }
      }
      for (int p = 0; p < Qm; p++) {
        llr32[p][h] = l[p];
        uns32[p][h] = u[p];
      }
    }
    NRLB_VI res[8];
    for (int p = 0; p < Qm; p++) {
      const NRLB_VI r = NRLB_NAME(nrlbw_pack32)(llr32[p][0], llr32[p][1]);
      // unspanned bit (one side has no candidate): graded per-axis seed LLR, as the int16 kernels
      res[p] = NRLB_BLENDV(r, axisLLR[p], NRLB_NAME(nrlbw_pack32)(uns32[p][0], uns32[p][1]));
    }
    NRLB_NAME(nrlbw_store_llr)(stream0_out, re, res, Qm);
  }

  // fewer than one SIMD block: the float reference per RE, same candidate set (window or full grid)
  for (; re < length; re++) {
    const int wide = full || nr_lbest_wide_re(ch_mag[re].r * psc, ch_mag_i[re].r * psc, rho01[re].r, rho01[re].i, lambda, nb);
    if (nb == 4)
      nr_qam256_llr_2layer_lbest(&stream0_in[re], &stream1_in[re], &ch_mag[re], &ch_mag_i[re], &stream0_out[re * 8], &rho01[re], 1, wide ? 256 : 9, lambda);
    else
      nr_qam64_llr_2layer_lbest(&stream0_in[re], &stream1_in[re], &ch_mag[re], &ch_mag_i[re], &stream0_out[re * 6], &rho01[re], 1, wide ? 64 : 9, lambda);
  }
}

#ifndef NRLB_F16 // the FP16 object builds only its own kernel
void NRLB_NAME(nr_qam256_llr_2layer_lbest_f32_simd)(c16_t *stream0_in,
                                                    c16_t *stream1_in,
                                                    c16_t *ch_mag,
                                                    c16_t *ch_mag_i,
                                                    int16_t *stream0_out,
                                                    c16_t *rho01,
                                                    uint32_t length,
                                                    int pattern,
                                                    float lambda)
{
  NRLB_NAME(nrlbw_lbest_f32)(stream0_in, stream1_in, ch_mag, ch_mag_i, stream0_out, rho01, length, pattern, lambda, 4);
}
#endif

#ifndef NRLB_F16 // the FP16 object builds only its own kernel
void NRLB_NAME(nr_qam64_llr_2layer_lbest_f32_simd)(c16_t *stream0_in,
                                                   c16_t *stream1_in,
                                                   c16_t *ch_mag,
                                                   c16_t *ch_mag_i,
                                                   int16_t *stream0_out,
                                                   c16_t *rho01,
                                                   uint32_t length,
                                                   int pattern,
                                                   float lambda)
{
  NRLB_NAME(nrlbw_lbest_f32)(stream0_in, stream1_in, ch_mag, ch_mag_i, stream0_out, rho01, length, pattern, lambda, 3);
}
#endif


// Piecewise-linear per-axis Gray-PAM LLR, nb bits per axis (M = 2^nb levels): v0 = est, v_b = M / 2^b - |v_(b-1)|,
// out[b] = g4 * v_b with g4 = 4 g in level units. Equal to max-log g * (d1 - d0) within the inner decision region
// of each bit; outside it max-log has steeper slopes, but only on bits that are already reliable (coded throughput
// unchanged on TDL-A, 2 and 4 Rx, 64/256QAM), at ~40% of the 64/256QAM MMSE kernel cost.
static inline void NRLB_NAME(nrlbw_pam_llr)(NRLB_VF est, NRLB_VF g4, int nb, NRLB_VF out[4])
{
  const NRLB_VF sgn = NRLB_MM(set1_ps)(-0.0f);
  const float M = (float)(1 << nb);
  NRLB_VF v = est;
  out[0] = NRLB_MM(mul_ps)(g4, v);
  if (nb >= 2) {
    v = NRLB_MM(sub_ps)(NRLB_MM(set1_ps)(M / 2), NRLB_MM(andnot_ps)(sgn, v));
    out[1] = NRLB_MM(mul_ps)(g4, v);
  }
  if (nb >= 3) {
    v = NRLB_MM(sub_ps)(NRLB_MM(set1_ps)(M / 4), NRLB_MM(andnot_ps)(sgn, v));
    out[2] = NRLB_MM(mul_ps)(g4, v);
  }
  if (nb >= 4) {
    v = NRLB_MM(sub_ps)(NRLB_MM(set1_ps)(M / 8), NRLB_MM(andnot_ps)(sgn, v));
    out[3] = NRLB_MM(mul_ps)(g4, v);
  }
}

// ============================================================================
// 2-layer linear MMSE: the L = 1 member of the L-best family. Unbiased MMSE estimate of the target layer (the
// L-best seed) and piecewise-linear per-axis Gray-PAM LLRs for every bit (nrlbw_pam_llr) at the max-log scale,
// with the post-MMSE energy as the reliability, g = 0.5 * Peff / K: the L-best fallback scale. fp32 throughout.
// Inputs (compensated, >> log2_maxh): z0 = target-layer MRC output, z1 = other layer, g00/g11 = Gram diagonal
// ||h||^2 (real part), rho01 = h0^H h1; lambda = noise variance (nr_2layer_seed_lambda). Qm = 2..8.
// ============================================================================
#ifndef NRLB_F16 // the FP16 object builds only its own kernel
void NRLB_NAME(nr_llr_2layer_mmse_simd)(const c16_t *z0,
                                        const c16_t *z1,
                                        const c16_t *g00,
                                        const c16_t *g11,
                                        const c16_t *rho01,
                                        int16_t *out,
                                        uint32_t length,
                                        int mod_order,
                                        float lambda)
{
  const int nb = mod_order / 2;
  static const float sqK[5] = {0, 1.4142135623730951f, 3.1622776601683795f, 6.48074069840786f, 13.038404810405298f};
  static const float K[5] = {0, 2.0f, 10.0f, 42.0f, 170.0f};
  const NRLB_VF lam = NRLB_MM(set1_ps)(lambda), ONE = NRLB_MM(set1_ps)(1.0f);
  const NRLB_VF epsh = NRLB_MM(set1_ps)(0.5f * NR_2LAYER_CSI_EPS);
  const NRLB_VF SQ = NRLB_MM(set1_ps)(sqK[nb]), GK = NRLB_MM(set1_ps)(2.0f / K[nb]); // 4 g: PWL slope
  __attribute__((aligned(64))) int16_t tmp[8][NRLB_N];

  uint32_t re = 0;
  for (int last = length < NRLB_N; !last; re += NRLB_N) {
    if (re + NRLB_N >= length) {
      re = length - NRLB_N;
      last = 1;
    }
    NRLB_VI z0r16, z0i16, z1r16, z1i16, rr16, ri16, p0_16, p1_16, dummy;
    NRLB_NAME(nrlbw_load)(&z0[re], &z0r16, &z0i16);
    NRLB_NAME(nrlbw_load)(&z1[re], &z1r16, &z1i16);
    NRLB_NAME(nrlbw_load)(&rho01[re], &rr16, &ri16);
    NRLB_NAME(nrlbw_load)(&g00[re], &p0_16, &dummy);
    NRLB_NAME(nrlbw_load)(&g11[re], &p1_16, &dummy);
    NRLB_VI llr32[8][2];
    for (int h = 0; h < 2; h++) {
      const NRLB_VF z0r = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z0r16, h)), z0i = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z0i16, h));
      const NRLB_VF z1r = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z1r16, h)), z1i = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z1i16, h));
      const NRLB_VF rr = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(rr16, h)), ri = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(ri16, h));
      const NRLB_VF P0 = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(p0_16, h));
      const NRLB_VF P1 = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(p1_16, h));
      const NRLB_VF P1s = NRLB_MM(add_ps)(NRLB_MM(add_ps)(P1, lam), NRLB_MM(mul_ps)(epsh, NRLB_MM(add_ps)(P0, P1)));
      // unbiased MMSE: x = ((P1 + lambda_eff) z0 - rho z1) / (P0 (P1 + lambda_eff) - |rho|^2), lambda_eff as the seeds
      const NRLB_VF det = NRLB_MM(max_ps)(
          NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(P0, P1s), NRLB_MM(add_ps)(NRLB_MM(mul_ps)(rr, rr), NRLB_MM(mul_ps)(ri, ri))), ONE);
      const NRLB_VF numr = NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(P1s, z0r), NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(rr, z1r), NRLB_MM(mul_ps)(ri, z1i)));
      const NRLB_VF numi = NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(P1s, z0i), NRLB_MM(add_ps)(NRLB_MM(mul_ps)(rr, z1i), NRLB_MM(mul_ps)(ri, z1r)));
      const NRLB_VF est[2] = {NRLB_MM(mul_ps)(NRLB_MM(div_ps)(numr, det), SQ), NRLB_MM(mul_ps)(NRLB_MM(div_ps)(numi, det), SQ)};
      const NRLB_VF g = NRLB_MM(mul_ps)(GK, NRLB_MM(div_ps)(det, NRLB_MM(max_ps)(P1s, ONE)));
      for (int ax = 0; ax < 2; ax++) { // I, Q
        NRLB_VF l[4];
        NRLB_NAME(nrlbw_pam_llr)(est[ax], g, nb, l);
        for (int b = 0; b < nb; b++)
          llr32[2 * b + ax][h] = NRLB_NAME(nrlbw_cvt_sat)(l[b]);
      }
    }
    // {I0,Q0,I1,Q1,...} per RE: the vector transpose store for Qm 6/8, a scalar interleave for QPSK/16QAM
    NRLB_VI res[8];
    for (int p = 0; p < mod_order; p++)
      res[p] = NRLB_NAME(nrlbw_pack32)(llr32[p][0], llr32[p][1]);
    if (mod_order >= 6) {
      NRLB_NAME(nrlbw_store_llr)(out, re, res, mod_order);
    } else {
      for (int p = 0; p < mod_order; p++)
        NRLB_STOREU(tmp[p], res[p]);
      for (int i = 0; i < NRLB_N; i++)
        for (int p = 0; p < mod_order; p++)
          out[(re + i) * mod_order + p] = tmp[p][i];
    }
  }
  if (re < length)
    nr_llr_2layer_mmse_ref(&z0[re], &z1[re], &g00[re], &g11[re], &rho01[re], &out[re * mod_order], length - re, mod_order, lambda);
}
#endif

#if defined(NRLB_F16) && NRLB_W == 128 && defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
#include <arm_neon.h>
// ============================================================================
// FP16 L-best / full ML (ARMv8.2 FP16 vector arithmetic), 8 REs per float16x8_t: the seed, candidate set,
// adaptive window and graded fallback of the fp32 kernel, with the metric taken relative to the seed point
// x0 = (window centre, its conditional nuisance slice): r = z - G x0 once per RE, then per candidate x0 + d
// dM = Re{d^H r} - 1/2 d^H G d. The max-log differences of M are unchanged, but the per-candidate terms are
// small (d: a few level steps), which FP16's 11-bit significand resolves: the absolute metric
// Re{x^H z} - 1/2 x^H G x loses 25-35% more slots at 256QAM in FP16, the relative one matches fp32 (nr_ulsim,
// TDL-A, 2/4 Rx). Each RE's inputs are scaled by a power of two so that its max(P0, P1) is in [8, 16) (P0 P1 <=
// 256, far from the FP16 overflow at 65504; per RE, so every RE stays independent of its block); the LLRs are scaled
// back in fp32, in the fp32 kernel's units.
// ============================================================================
static inline float16x8_t nrlbh_round_odd(float16x8_t x) // 2 * round((x - 1) / 2) + 1
{
  const float16x8_t one = vdupq_n_f16(1.0f), half = vdupq_n_f16(0.5f), two = vdupq_n_f16(2.0f);
  return vfmaq_f16(one, vrndnq_f16(vmulq_f16(vsubq_f16(x, one), half)), two);
}

static inline float16x8_t nrlbh_clamp(float16x8_t x, float16x8_t lim) // to [-lim, lim]
{
  return vminq_f16(vmaxq_f16(x, vnegq_f16(lim)), lim);
}

// Gray bits of a PAM level vector, nb bits per axis (all-ones 16-bit lanes where the bit is 1)
static inline void nrlbh_bits(float16x8_t lvl, int nb, uint16x8_t b[4])
{
  b[0] = vcltq_f16(lvl, vdupq_n_f16(0.0f));
  float16x8_t r = vabsq_f16(lvl);
  float t = (float)(1 << (nb - 1));
  for (int k = 1; k < nb; k++) {
    const float16x8_t tv = vdupq_n_f16(t);
    b[k] = vcgtq_f16(r, tv);
    r = vabsq_f16(vsubq_f16(r, tv));
    t *= 0.5f;
  }
}

typedef struct {
  float16x8_t r1r, r1i, r2r, r2i; // residuals z - G x0
  float16x8_t rr, ri, hP0, hP1, sl; // rho, P/2, nuisance slice scale sqrt(K)/P1
  float16x8_t cI, cQ, c2I, c2Q; // x0 levels: window centre, nuisance slice at the centre
} nrlbh_re_t;

// candidate grid around x0: the 3x3 window (clamped as the other kernels) or all M levels; conditional nuisance
// slice and max-log per axis. llr in metric units (scaled), uns: one side of the bit has no candidate
static inline void nrlbh_grid(const nrlbh_re_t *e, int full, int nb, float16x8_t llr[8], uint16x8_t uns[8])
{
  const int M = 1 << nb, nI = full ? M : 3;
  const float16x8_t LMAX = vdupq_n_f16((float)(M - 1));
  const float16x8_t NA = vdupq_n_f16(nb == 3 ? 1.0f / 6.48074069840786f : 1.0f / 13.038404810405298f);
  const float16x8_t NEG = vdupq_n_f16(-INFINITY);
  float16x8_t dI[16], dQ[16];
  uint16x8_t bI[16][4], bQ[16][4];
  for (int k = 0; k < nI; k++) {
    float16x8_t lI, lQ;
    if (full) {
      lI = lQ = vdupq_n_f16((float)(2 * k - (M - 1)));
    } else {
      const float16x8_t o = vdupq_n_f16((float)(2 * k - 2));
      lI = nrlbh_clamp(vaddq_f16(e->cI, o), LMAX);
      lQ = nrlbh_clamp(vaddq_f16(e->cQ, o), LMAX);
    }
    nrlbh_bits(lI, nb, bI[k]);
    nrlbh_bits(lQ, nb, bQ[k]);
    dI[k] = vmulq_f16(vsubq_f16(lI, e->cI), NA); // offsets from x0 (exact level differences, one rounding)
    dQ[k] = vmulq_f16(vsubq_f16(lQ, e->cQ), NA);
  }
  float16x8_t rowMax[16], colMax[16];
  for (int k = 0; k < nI; k++)
    rowMax[k] = colMax[k] = NEG;
  for (int a = 0; a < nI; a++) {
    const float16x8_t d1r = dI[a];
    for (int b = 0; b < nI; b++) {
      const float16x8_t d1i = dQ[b];
      // Re{d1* r1} - P0 |d1|^2 / 2
      float16x8_t m = vfmaq_f16(vmulq_f16(d1r, e->r1r), d1i, e->r1i);
      m = vfmsq_f16(m, e->hP0, vfmaq_f16(vmulq_f16(d1r, d1r), d1i, d1i));
      // nuisance x2 = Q(x0_2 + (r2 - conj(rho) d1) / P1)
      const float16x8_t cr = vfmaq_f16(vmulq_f16(e->rr, d1r), e->ri, d1i);
      const float16x8_t ci = vfmsq_f16(vmulq_f16(e->rr, d1i), e->ri, d1r);
      const float16x8_t I2 = nrlbh_clamp(nrlbh_round_odd(vfmaq_f16(e->c2I, vsubq_f16(e->r2r, cr), e->sl)), LMAX);
      const float16x8_t Q2 = nrlbh_clamp(nrlbh_round_odd(vfmaq_f16(e->c2Q, vsubq_f16(e->r2i, ci), e->sl)), LMAX);
      const float16x8_t d2r = vmulq_f16(vsubq_f16(I2, e->c2I), NA), d2i = vmulq_f16(vsubq_f16(Q2, e->c2Q), NA);
      m = vfmaq_f16(m, d2r, e->r2r);
      m = vfmaq_f16(m, d2i, e->r2i);
      m = vfmsq_f16(m, e->hP1, vfmaq_f16(vmulq_f16(d2r, d2r), d2i, d2i));
      // - Re{d1* rho d2}
      const float16x8_t cxr = vfmaq_f16(vmulq_f16(d1r, d2r), d1i, d2i);
      const float16x8_t cxi = vfmsq_f16(vmulq_f16(d1r, d2i), d1i, d2r);
      m = vsubq_f16(m, vfmsq_f16(vmulq_f16(e->rr, cxr), e->ri, cxi));
      rowMax[a] = vmaxq_f16(rowMax[a], m);
      colMax[b] = vmaxq_f16(colMax[b], m);
    }
  }
  for (int k = 0; k < nb; k++) {
    float16x8_t m0I = NEG, m1I = NEG, m0Q = NEG, m1Q = NEG;
    for (int a = 0; a < nI; a++) {
      m0I = vmaxq_f16(m0I, vbslq_f16(bI[a][k], NEG, rowMax[a]));
      m1I = vmaxq_f16(m1I, vbslq_f16(bI[a][k], rowMax[a], NEG));
      m0Q = vmaxq_f16(m0Q, vbslq_f16(bQ[a][k], NEG, colMax[a]));
      m1Q = vmaxq_f16(m1Q, vbslq_f16(bQ[a][k], colMax[a], NEG));
    }
    llr[2 * k] = vsubq_f16(m0I, m1I);
    llr[2 * k + 1] = vsubq_f16(m0Q, m1Q);
    uns[2 * k] = vorrq_u16(vceqq_f16(m0I, NEG), vceqq_f16(m1I, NEG));
    uns[2 * k + 1] = vorrq_u16(vceqq_f16(m0Q, NEG), vceqq_f16(m1Q, NEG));
  }
}

// per-RE scales from P = max(P0, P1) >= 1 (fp32 halves): s = 2^(4 - e) and 1 / s, e the frexp exponent of P (exponent
// field E = e + 126: s has field 257 - E, 1 / s field E - 3)
static inline void nrlbh_scale(float32x4_t p, float32x4_t *s, float32x4_t *inv)
{
  const uint32x4_t E = vshrq_n_u32(vreinterpretq_u32_f32(vmaxq_f32(p, vdupq_n_f32(1.0f))), 23);
  *s = vreinterpretq_f32_u32(vshlq_n_u32(vsubq_u32(vdupq_n_u32(257), E), 23));
  *inv = vreinterpretq_f32_u32(vshlq_n_u32(vsubq_u32(E, vdupq_n_u32(3)), 23));
}

// metric-unit FP16 LLRs (scaled by s per RE) -> int16 in the fp32 kernel's units (truncated, saturated)
static inline int16x8_t nrlbh_llr16(float16x8_t l, float32x4_t inv_lo, float32x4_t inv_hi)
{
  const float32x4_t lo = vmulq_f32(vcvt_f32_f16(vget_low_f16(l)), inv_lo);
  const float32x4_t hi = vmulq_f32(vcvt_high_f32_f16(l), inv_hi);
  const float32x4_t mn = vdupq_n_f32(-32768.0f), mx = vdupq_n_f32(32767.0f);
  return vcombine_s16(vqmovn_s32(vcvtq_s32_f32(vminq_f32(vmaxq_f32(lo, mn), mx))),
                      vqmovn_s32(vcvtq_s32_f32(vminq_f32(vmaxq_f32(hi, mn), mx))));
}

static inline void nrlbh_lbest(c16_t *stream0_in,
                               c16_t *stream1_in,
                               c16_t *ch_mag,
                               c16_t *ch_mag_i,
                               int16_t *stream0_out,
                               c16_t *rho01,
                               uint32_t length,
                               int pattern,
                               float lambda,
                               int nb)
{
  const float sqK = nb == 3 ? 6.48074069840786f : 13.038404810405298f;
  const int M = 1 << nb, Qm = 2 * nb;
  const float psc = sqK / (float)(M / 2); // cm -> ||h||^2
  const NRLB_VF lamv = NRLB_MM(set1_ps)(lambda), epshv = NRLB_MM(set1_ps)(0.5f * NR_2LAYER_CSI_EPS);
  const float16x8_t NA = vdupq_n_f16(1.0f / sqK), LMAX = vdupq_n_f16((float)(M - 1)), ZERO = vdupq_n_f16(0.0f);
  const float16x8_t EPSH = vdupq_n_f16(0.5f * NR_2LAYER_CSI_EPS), HALF = vdupq_n_f16(0.5f);
  const float16x8_t WT = vdupq_n_f16(nb == 4 ? NR_LBEST_WIDE_SINR256 : NR_LBEST_WIDE_SINR64);
  const int full = pattern == 3;

  uint32_t re = 0;
  for (int last = length < NRLB_N; !last; re += NRLB_N) {
    if (re + NRLB_N >= length) {
      re = length - NRLB_N;
      last = 1;
    }
    NRLB_VI z1r16, z1i16, z2r16, z2i16, rr16, ri16, cm0, cm1, dummy;
    NRLB_NAME(nrlbw_load)(&stream0_in[re], &z1r16, &z1i16);
    NRLB_NAME(nrlbw_load)(&stream1_in[re], &z2r16, &z2i16);
    NRLB_NAME(nrlbw_load)(&rho01[re], &rr16, &ri16);
    NRLB_NAME(nrlbw_load)(&ch_mag[re], &cm0, &dummy);
    NRLB_NAME(nrlbw_load)(&ch_mag_i[re], &cm1, &dummy);
    NRLB_VI centerI, centerQ, dirImask, dirQmask, axisLLR[8];
    if (nb == 4)
      NRLB_NAME(nrlbw_seed256)(z1r16, z1i16, z2r16, z2i16, rr16, ri16, cm0, cm1, lamv, epshv, &centerI, &centerQ, &dirImask, &dirQmask, axisLLR);
    else
      NRLB_NAME(nrlbw_seed)(z1r16, z1i16, z2r16, z2i16, rr16, ri16, cm0, cm1, lamv, epshv, &centerI, &centerQ, &dirImask, &dirQmask, axisLLR);
    (void)dirImask;
    (void)dirQmask;

    // per-RE power-of-two scale with max(P0, P1) in [8, 16)
    const int16x8_t c0 = simde__m128i_to_neon_i16(cm0), c1 = simde__m128i_to_neon_i16(cm1);
    const int16x8_t cmax = vmaxq_s16(c0, c1);
    float32x4_t s_lo, s_hi, inv_lo, inv_hi;
    nrlbh_scale(vmulq_n_f32(vcvtq_f32_s32(vmovl_s16(vget_low_s16(cmax))), psc), &s_lo, &inv_lo);
    nrlbh_scale(vmulq_n_f32(vcvtq_f32_s32(vmovl_high_s16(cmax)), psc), &s_hi, &inv_hi);
    const float16x8_t S = vcombine_f16(vcvt_f16_f32(s_lo), vcvt_f16_f32(s_hi));
#define NRLBH_IN(v) vmulq_f16(vcvtq_f16_s16(simde__m128i_to_neon_i16(v)), S)
    const float16x8_t z1r = NRLBH_IN(z1r16), z1i = NRLBH_IN(z1i16), z2r = NRLBH_IN(z2r16), z2i = NRLBH_IN(z2i16);
    nrlbh_re_t e;
    e.rr = NRLBH_IN(rr16);
    e.ri = NRLBH_IN(ri16);
#undef NRLBH_IN
    const float16x8_t PSC = vdupq_n_f16(psc);
    const float16x8_t P0 = vmulq_f16(vmulq_f16(vcvtq_f16_s16(c0), S), PSC), P1 = vmulq_f16(vmulq_f16(vcvtq_f16_s16(c1), S), PSC);
    e.hP0 = vmulq_f16(HALF, P0);
    e.hP1 = vmulq_f16(HALF, P1);
    // nuisance slice scale sqrt(K) / P1 (0 where P1 == 0, as the references)
    e.sl = vbslq_f16(vceqq_f16(P1, ZERO), ZERO, vdivq_f16(vdupq_n_f16(sqK), P1));
    e.cI = vcvtq_f16_s16(simde__m128i_to_neon_i16(centerI));
    e.cQ = vcvtq_f16_s16(simde__m128i_to_neon_i16(centerQ));
    // x0_1 = centre, x0_2 = its conditional nuisance slice; r = z - G x0
    const float16x8_t x1r = vmulq_f16(e.cI, NA), x1i = vmulq_f16(e.cQ, NA);
    const float16x8_t cr = vfmaq_f16(vmulq_f16(e.rr, x1r), e.ri, x1i), ci = vfmsq_f16(vmulq_f16(e.rr, x1i), e.ri, x1r);
    e.c2I = nrlbh_clamp(nrlbh_round_odd(vmulq_f16(vsubq_f16(z2r, cr), e.sl)), LMAX);
    e.c2Q = nrlbh_clamp(nrlbh_round_odd(vmulq_f16(vsubq_f16(z2i, ci), e.sl)), LMAX);
    const float16x8_t x2r = vmulq_f16(e.c2I, NA), x2i = vmulq_f16(e.c2Q, NA);
    e.r1r = vsubq_f16(vfmsq_f16(z1r, P0, x1r), vfmsq_f16(vmulq_f16(e.rr, x2r), e.ri, x2i));
    e.r1i = vsubq_f16(vfmsq_f16(z1i, P0, x1i), vfmaq_f16(vmulq_f16(e.rr, x2i), e.ri, x2r));
    e.r2r = vfmsq_f16(vsubq_f16(z2r, cr), P1, x2r);
    e.r2i = vfmsq_f16(vsubq_f16(z2i, ci), P1, x2i);

    float16x8_t l[8];
    uint16x8_t u[8];
    nrlbh_grid(&e, full, nb, l, u);
    if (!full) {
      // adaptive window (nr_lbest_wide_re): the full grid on the low-SINR REs
      const float16x8_t lame = vfmaq_f16(vmulq_f16(vdupq_n_f16(lambda), S), EPSH, vaddq_f16(P0, P1));
      const float16x8_t P1s = vaddq_f16(P1, lame);
      const float16x8_t det = vsubq_f16(vmulq_f16(P0, P1s), vfmaq_f16(vmulq_f16(e.rr, e.rr), e.ri, e.ri));
      const uint16x8_t wide = vcltq_f16(det, vmulq_f16(WT, vmulq_f16(lame, P1s)));
      if (vmaxvq_u16(wide)) {
        float16x8_t lf[8];
        uint16x8_t uf[8];
        nrlbh_grid(&e, 1, nb, lf, uf);
        for (int p = 0; p < Qm; p++) {
          l[p] = vbslq_f16(wide, lf[p], l[p]);
          u[p] = vbicq_u16(u[p], wide);
        }
      }
    }
    NRLB_VI res[8];
    for (int p = 0; p < Qm; p++) {
      // unspanned bit: graded per-axis seed LLR, as the other kernels
      const int16x8_t r = vbslq_s16(u[p], simde__m128i_to_neon_i16(axisLLR[p]), nrlbh_llr16(l[p], inv_lo, inv_hi));
      res[p] = simde__m128i_from_neon_i16(r);
    }
    NRLB_NAME(nrlbw_store_llr)(stream0_out, re, res, Qm);
  }

}

// FP16 L-best of any length: fewer REs than one block run as a zero-padded block (the REs are independent), so a
// call gives the same LLRs whatever its length
static void nrlbh_lbest_any(c16_t *stream0_in,
                            c16_t *stream1_in,
                            c16_t *ch_mag,
                            c16_t *ch_mag_i,
                            int16_t *stream0_out,
                            c16_t *rho01,
                            uint32_t length,
                            int pattern,
                            float lambda,
                            int nb)
{
  if (length >= NRLB_N) {
    nrlbh_lbest(stream0_in, stream1_in, ch_mag, ch_mag_i, stream0_out, rho01, length, pattern, lambda, nb);
    return;
  }
  c16_t in[5][NRLB_N] __attribute__((aligned(64))) = {0};
  int16_t out[NRLB_N * 8] __attribute__((aligned(64)));
  memcpy(in[0], stream0_in, length * sizeof(c16_t));
  memcpy(in[1], stream1_in, length * sizeof(c16_t));
  memcpy(in[2], ch_mag, length * sizeof(c16_t));
  memcpy(in[3], ch_mag_i, length * sizeof(c16_t));
  memcpy(in[4], rho01, length * sizeof(c16_t));
  nrlbh_lbest(in[0], in[1], in[2], in[3], out, in[4], NRLB_N, pattern, lambda, nb);
  memcpy(stream0_out, out, length * 2 * nb * sizeof(int16_t));
}

void NRLB_NAME(nr_qam256_llr_2layer_lbest_f16_simd)(c16_t *stream0_in,
                                                    c16_t *stream1_in,
                                                    c16_t *ch_mag,
                                                    c16_t *ch_mag_i,
                                                    int16_t *stream0_out,
                                                    c16_t *rho01,
                                                    uint32_t length,
                                                    int pattern,
                                                    float lambda)
{
  nrlbh_lbest_any(stream0_in, stream1_in, ch_mag, ch_mag_i, stream0_out, rho01, length, pattern, lambda, 4);
}

void NRLB_NAME(nr_qam64_llr_2layer_lbest_f16_simd)(c16_t *stream0_in,
                                                   c16_t *stream1_in,
                                                   c16_t *ch_mag,
                                                   c16_t *ch_mag_i,
                                                   int16_t *stream0_out,
                                                   c16_t *rho01,
                                                   uint32_t length,
                                                   int pattern,
                                                   float lambda)
{
  nrlbh_lbest_any(stream0_in, stream1_in, ch_mag, ch_mag_i, stream0_out, rho01, length, pattern, lambda, 3);
}
#endif

#endif
