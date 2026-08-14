/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 * Width-parameterized 2-layer 64QAM L-best SIMD kernel body. Included by
 * nr_compute_llr.c once per NRLB_W (128/256) after nr_lbest_simd_width.h.
 * Elementwise ops via NRLB_MM(); cross-lane via nrlbw_* helpers. Do not edit
 * per-width copies -- this single source is the contract.
 */
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
  const NRLB_VI mag = NRLB_MM(adds_epi16)(NRLB_MM(set1_epi16)(1),
                                                  NRLB_MM(slli_epi16)(NRLB_MM(adds_epi16)(NRLB_MM(adds_epi16)(g2, g4), g6), 1));
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

// ZF-seed nearest-level centers for 16 REs (int16 out). The 2x2 solve is done in
// FLOAT, which is robust to the wide per-RE dynamic range of real channels (TDL
// deep fades) where any fixed int pre-shift either loses precision or underflows.
// Also emits dirImask/dirQmask: int16 mask, all-ones where the seed lies on the

static inline void NRLB_NAME(nrlbw_seed)(NRLB_VI z1r16, NRLB_VI z1i16, NRLB_VI z2r16, NRLB_VI z2i16,
                                        NRLB_VI rr16, NRLB_VI ri16, NRLB_VI cm0_16, NRLB_VI cm1_16,
                                        NRLB_VI *centerI, NRLB_VI *centerQ,
                                        NRLB_VI *dirImask, NRLB_VI *dirQmask)
{
  const NRLB_VI SQRT42_OVER4 = NRLB_MM(set1_epi32)(NR_LBEST_Q_SQRT42_OVER4_Q14);
  const NRLB_VF SQRT42 = NRLB_MM(set1_ps)(6.48074069840786f);
  const NRLB_VF HALF = NRLB_MM(set1_ps)(0.5f), ONE = NRLB_MM(set1_ps)(1.0f), TWO = NRLB_MM(set1_ps)(2.0f);
  const NRLB_VF P7 = NRLB_MM(set1_ps)(7.0f), M7 = NRLB_MM(set1_ps)(-7.0f);
  NRLB_VI cI[2], cQ[2], dI[2], dQ[2];
  for (int h = 0; h < 2; h++) {
    const NRLB_VF z1r = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z1r16, h)), z1i = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z1i16, h));
    const NRLB_VF z2r = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z2r16, h)), z2i = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z2i16, h));
    const NRLB_VF rr = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(rr16, h)), ri = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(ri16, h));
    const NRLB_VF Pp0 = NRLB_MM(cvtepi32_ps)(NRLB_MM(srai_epi32)(NRLB_MM(mullo_epi32)(NRLB_NAME(nrlbw_widen)(cm0_16, h), SQRT42_OVER4), 14));
    const NRLB_VF Pp1 = NRLB_MM(cvtepi32_ps)(NRLB_MM(srai_epi32)(NRLB_MM(mullo_epi32)(NRLB_NAME(nrlbw_widen)(cm1_16, h), SQRT42_OVER4), 14));
    // ZF 2x2 solve (float): x_hat1 = ((Pp1) z1 - rho z2) / det,  det = Pp0 Pp1 - |rho|^2
    NRLB_VF det = NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(Pp0, Pp1), NRLB_MM(add_ps)(NRLB_MM(mul_ps)(rr, rr), NRLB_MM(mul_ps)(ri, ri)));
    det = NRLB_MM(max_ps)(det, ONE); // guard near-singular (high correlation)
    const NRLB_VF numr = NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(Pp1, z1r), NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(rr, z2r), NRLB_MM(mul_ps)(ri, z2i)));
    const NRLB_VF numi = NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(Pp1, z1i), NRLB_MM(add_ps)(NRLB_MM(mul_ps)(rr, z2i), NRLB_MM(mul_ps)(ri, z2r)));
    // soft level estimate est = x_hat1 * sqrt(42)  (~ +-1..7)
    const NRLB_VF estI = NRLB_MM(mul_ps)(NRLB_MM(div_ps)(numr, det), SQRT42);
    const NRLB_VF estQ = NRLB_MM(mul_ps)(NRLB_MM(div_ps)(numi, det), SQRT42);
    // nearest odd level: o = 2*round((est-1)/2)+1, clamped to [-7,7]
    NRLB_VF oI = NRLB_MM(add_ps)(NRLB_MM(mul_ps)(NRLB_ROUNDPS(NRLB_MM(mul_ps)(NRLB_MM(sub_ps)(estI, ONE), HALF), SIMDE_MM_FROUND_TO_NEAREST_INT | SIMDE_MM_FROUND_NO_EXC), TWO), ONE);
    NRLB_VF oQ = NRLB_MM(add_ps)(NRLB_MM(mul_ps)(NRLB_ROUNDPS(NRLB_MM(mul_ps)(NRLB_MM(sub_ps)(estQ, ONE), HALF), SIMDE_MM_FROUND_TO_NEAREST_INT | SIMDE_MM_FROUND_NO_EXC), TWO), ONE);
    oI = NRLB_MM(min_ps)(NRLB_MM(max_ps)(oI, M7), P7);
    oQ = NRLB_MM(min_ps)(NRLB_MM(max_ps)(oQ, M7), P7);
    cI[h] = NRLB_MM(cvtps_epi32)(oI);
    cQ[h] = NRLB_MM(cvtps_epi32)(oQ);
    dI[h] = NRLB_CMPPS_SI(estI, oI, SIMDE_CMP_GT_OQ); // toward-seed = +2 side
    dQ[h] = NRLB_CMPPS_SI(estQ, oQ, SIMDE_CMP_GT_OQ);
  }
  *centerI = NRLB_NAME(nrlbw_pack32)(cI[0], cI[1]);
  *centerQ = NRLB_NAME(nrlbw_pack32)(cQ[0], cQ[1]);
  *dirImask = NRLB_NAME(nrlbw_pack32)(dI[0], dI[1]);
  *dirQmask = NRLB_NAME(nrlbw_pack32)(dQ[0], dQ[1]);
}

// Per-candidate evaluation (slice + metric + max-log reduction), updating max0/max1.
// Captures the per-RE-vector operands from the enclosing scope; the per-candidate
// I-only/Q-only terms (bits, corr0/energy0 parts, slice operands) are passed in.
// Store Qm LLR vectors (res[Qm], 16 REs across the lanes) to the interleaved decoder layout
// stream0_out[(re+r)*Qm + p] via an 8x8 int16 transpose (padded from Qm), replacing the scalar

#define NR_LBEST_METRIC16(MET_, I1_, Q1_, C0I_, C0Q_, EI_, EQ_, RRI_, RII_, RRQ_, RIQ_) \
  do { \
    const NRLB_VI A2I = NRLB_MM(subs_epi16)(z2r42, NRLB_MM(adds_epi16)((RRI_), (RIQ_))); \
    const NRLB_VI A2Q = NRLB_MM(subs_epi16)(z2i42, NRLB_MM(subs_epi16)((RRQ_), (RII_))); \
    const NRLB_VI I2 = NRLB_NAME(nrlbw_level)(A2I, Pp1_5); \
    const NRLB_VI Q2 = NRLB_NAME(nrlbw_level)(A2Q, Pp1_5); \
    const NRLB_VI corr0 = NRLB_MM(adds_epi16)((C0I_), (C0Q_)); \
    const NRLB_VI energy0 = NRLB_MM(adds_epi16)((EI_), (EQ_)); \
    const NRLB_VI corr1 = NRLB_MM(adds_epi16)(NRLB_MM(mulhi_epi16)(z2r, NRLB_MM(mullo_epi16)(I2, C13)), NRLB_MM(mulhi_epi16)(z2i, NRLB_MM(mullo_epi16)(Q2, C13))); \
    const NRLB_VI energy1 = NRLB_MM(mulhi_epi16)(cm1, NRLB_MM(mullo_epi16)(NRLB_MM(adds_epi16)(NRLB_MM(mullo_epi16)(I2, I2), NRLB_MM(mullo_epi16)(Q2, Q2)), CE)); \
    const NRLB_VI aa = NRLB_MM(adds_epi16)(NRLB_MM(mullo_epi16)((I1_), I2), NRLB_MM(mullo_epi16)((Q1_), Q2)); \
    const NRLB_VI bb = NRLB_MM(subs_epi16)(NRLB_MM(mullo_epi16)((Q1_), I2), NRLB_MM(mullo_epi16)((I1_), Q2)); \
    const NRLB_VI cross = NRLB_MM(adds_epi16)(NRLB_MM(mulhi_epi16)(rr, NRLB_MM(mullo_epi16)(aa, CX)), NRLB_MM(mulhi_epi16)(ri, NRLB_MM(mullo_epi16)(bb, CX))); \
    (MET_) = NRLB_MM(max_epi16)(NRLB_MM(subs_epi16)(NRLB_MM(adds_epi16)(NRLB_MM(subs_epi16)(corr0, energy0), NRLB_MM(subs_epi16)(corr1, energy1)), cross), FLOORM); \
  } while (0)

// Per-candidate metric + streaming max-log reduction into max0/max1 (for non-grid patterns).

#define NR_LBEST_EVAL16(I1_, Q1_, BI0, BI1, BI2, BQ0, BQ1, BQ2, C0I_, C0Q_, EI_, EQ_, RRI_, RII_, RRQ_, RIQ_) \
  do { \
    NRLB_VI metric; \
    NR_LBEST_METRIC16(metric, I1_, Q1_, C0I_, C0Q_, EI_, EQ_, RRI_, RII_, RRQ_, RIQ_); \
    const NRLB_VI bm[6] = {(BI0), (BQ0), (BI1), (BQ1), (BI2), (BQ2)}; \
    for (int p = 0; p < 6; p++) { \
      max0[p] = NRLB_MM(max_epi16)(max0[p], NRLB_BLENDV(metric, SENT, bm[p])); \
      max1[p] = NRLB_MM(max_epi16)(max1[p], NRLB_BLENDV(SENT, metric, bm[p])); \
    } \
  } while (0)

// pattern: 0 = 3x3 (9 cand, full BLER), 1 = 6-cand (plus + toward-seed diagonal),

void NRLB_NAME(nr_qam64_llr_2layer_lbest_q15_simd)(c16_t *stream0_in,
                                          c16_t *stream1_in,
                                          c16_t *ch_mag,
                                          c16_t *ch_mag_i,
                                          int16_t *stream0_out,
                                          c16_t *rho01,
                                          uint32_t length,
                                          int pattern)
{
  const NRLB_VI SENT = NRLB_MM(set1_epi16)(NR_LBEST_SIMD16_SENT);
  const NRLB_VI FLOORM = NRLB_MM(set1_epi16)(-32000); // keep real metrics above SENT
  const NRLB_VI V1 = NRLB_MM(set1_epi16)(1), V4 = NRLB_MM(set1_epi16)(4);
  const NRLB_VI V7 = NRLB_MM(set1_epi16)(7), Vm7 = NRLB_MM(set1_epi16)(-7);
  const NRLB_VI z = NRLB_SETZERO();

  uint32_t re = 0;
  for (; re + NRLB_N <= length; re += NRLB_N) {
    NRLB_VI z1r, z1i, z2r, z2i, rr, ri, cm0, cm1, dummy;
    NRLB_NAME(nrlbw_load)(&stream0_in[re], &z1r, &z1i);
    NRLB_NAME(nrlbw_load)(&stream1_in[re], &z2r, &z2i);
    NRLB_NAME(nrlbw_load)(&rho01[re], &rr, &ri);
    NRLB_NAME(nrlbw_load)(&ch_mag[re], &cm0, &dummy);
    NRLB_NAME(nrlbw_load)(&ch_mag_i[re], &cm1, &dummy);

    NRLB_VI centerI, centerQ, dirImask, dirQmask;
    NRLB_NAME(nrlbw_seed)(z1r, z1i, z2r, z2i, rr, ri, cm0, cm1, &centerI, &centerQ, &dirImask, &dirQmask);
    (void)dirImask; (void)dirQmask;

    // per-RE precompute (int16): conditional-slice operands (/32 scale)
    const NRLB_VI z2r42 = NRLB_NAME(nrlbw_z42d32)(z2r), z2i42 = NRLB_NAME(nrlbw_z42d32)(z2i);
    const NRLB_VI rr5 = NRLB_MM(srai_epi16)(rr, 5), ri5 = NRLB_MM(srai_epi16)(ri, 5);
    const NRLB_VI Pp1_5 = NRLB_MM(mulhi_epi16)(cm1, NRLB_MM(set1_epi16)(NR_LBEST_Q_PP1_OVER32_Q16));
    // metric Q-constants (normal/8 scale; precision-preserving via mulhi, not pre-shift)
    const NRLB_VI C13 = NRLB_MM(set1_epi16)(1264); // 2^13/sqrt(42): mulhi(z, I*C13) = z*I/(8 sqrt42)
    const NRLB_VI CE = NRLB_MM(set1_epi16)(158);   // 2^16/(64 sqrt42): energy = cm*|X|^2/(64 sqrt42)
    const NRLB_VI CX = NRLB_MM(set1_epi16)(195);   // 2^16/336: cross = rec/336

    NRLB_VI max0[6], max1[6];
    for (int p = 0; p < 6; p++) { max0[p] = SENT; max1[p] = SENT; }

    // 3x3 candidate block, HOISTED (full BLER). Precompute the I-only / Q-only terms
    // for the 3 distinct I-levels and 3 Q-levels, then combine in the 9-pair loop.
    // corr1/energy1/cross + the two level16 slicers stay per-pair (depend on the
    // conditionally-sliced I2,Q2, which couple I and Q).
    static const int offs3[3] = {-2, 0, 2};
    NRLB_VI Iv[3], Qv[3];
    NRLB_VI bIv[3][3], bQv[3][3];                       // [level][bit 0..2]
    NRLB_VI c0I[3], c0Q[3], eI[3], eQ[3];               // corr0 / energy0 parts
    NRLB_VI rrI[3], riI[3], rrQ[3], riQ[3];             // slice operands
    for (int k = 0; k < 3; k++) {
      const NRLB_VI I1 = NRLB_MM(min_epi16)(NRLB_MM(max_epi16)(NRLB_MM(adds_epi16)(centerI, NRLB_MM(set1_epi16)(offs3[k])), Vm7), V7);
      const NRLB_VI Q1 = NRLB_MM(min_epi16)(NRLB_MM(max_epi16)(NRLB_MM(adds_epi16)(centerQ, NRLB_MM(set1_epi16)(offs3[k])), Vm7), V7);
      Iv[k] = I1; Qv[k] = Q1;
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
      rrI[k] = NRLB_MM(mullo_epi16)(rr5, I1); riI[k] = NRLB_MM(mullo_epi16)(ri5, I1);
      rrQ[k] = NRLB_MM(mullo_epi16)(rr5, Q1); riQ[k] = NRLB_MM(mullo_epi16)(ri5, Q1);
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
      for (int i = 0; i < 3; i++) maxI[i] = NRLB_MM(max_epi16)(met[i][0], NRLB_MM(max_epi16)(met[i][1], met[i][2]));
      for (int q = 0; q < 3; q++) maxQ[q] = NRLB_MM(max_epi16)(met[0][q], NRLB_MM(max_epi16)(met[1][q], met[2][q]));
      for (int k = 0; k < 3; k++) // I-bit k -> output p=2k ; Q-bit k -> p=2k+1
        for (int i = 0; i < 3; i++) {
          max0[2 * k]     = NRLB_MM(max_epi16)(max0[2 * k],     NRLB_BLENDV(maxI[i], SENT, bIv[i][k]));
          max1[2 * k]     = NRLB_MM(max_epi16)(max1[2 * k],     NRLB_BLENDV(SENT, maxI[i], bIv[i][k]));
          max0[2 * k + 1] = NRLB_MM(max_epi16)(max0[2 * k + 1], NRLB_BLENDV(maxQ[i], SENT, bQv[i][k]));
          max1[2 * k + 1] = NRLB_MM(max_epi16)(max1[2 * k + 1], NRLB_BLENDV(SENT, maxQ[i], bQv[i][k]));
        }
    } else { // 5-plus: center (1,1) + I+-2 + Q+-2
      static const int pI[5] = {1, 0, 2, 1, 1}, pQ[5] = {1, 1, 1, 0, 2};
      for (int c = 0; c < 5; c++) {
        const int iI = pI[c], iQ = pQ[c];
        NR_LBEST_EVAL16(Iv[iI], Qv[iQ], bIv[iI][0], bIv[iI][1], bIv[iI][2], bQv[iQ][0], bQv[iQ][1], bQv[iQ][2],
                        c0I[iI], c0Q[iQ], eI[iI], eQ[iQ], rrI[iI], riI[iI], rrQ[iQ], riQ[iQ]);
      }
      if (pattern == 1) { // 6th: toward-seed diagonal, per-lane select corner 0 vs 2
#define NR_LBEST_SI(a) NRLB_BLENDV((a)[0], (a)[2], dirImask)
#define NR_LBEST_SQ(a) NRLB_BLENDV((a)[0], (a)[2], dirQmask)
        NR_LBEST_EVAL16(NR_LBEST_SI(Iv), NR_LBEST_SQ(Qv),
                        NRLB_BLENDV(bIv[0][0], bIv[2][0], dirImask),
                        NRLB_BLENDV(bIv[0][1], bIv[2][1], dirImask),
                        NRLB_BLENDV(bIv[0][2], bIv[2][2], dirImask),
                        NRLB_BLENDV(bQv[0][0], bQv[2][0], dirQmask),
                        NRLB_BLENDV(bQv[0][1], bQv[2][1], dirQmask),
                        NRLB_BLENDV(bQv[0][2], bQv[2][2], dirQmask),
                        NR_LBEST_SI(c0I), NR_LBEST_SQ(c0Q), NR_LBEST_SI(eI), NR_LBEST_SQ(eQ),
                        NR_LBEST_SI(rrI), NR_LBEST_SI(riI), NR_LBEST_SQ(rrQ), NR_LBEST_SQ(riQ));
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
      NRLB_VI r = NRLB_NAME(nrlbw_pack32)(lo, hi);
      r = NRLB_BLENDV(r, NRLB_MM(set1_epi16)(-NR_LBEST_Q_LLR_SAT), NRLB_CMPEQ16(max0[p], SENT));
      r = NRLB_BLENDV(r, NRLB_MM(set1_epi16)(NR_LBEST_Q_LLR_SAT), NRLB_CMPEQ16(max1[p], SENT));
      res[p] = r;
    }
    NRLB_NAME(nrlbw_store_llr)(stream0_out, re, res, 6);
  }

  if (re < length)
    nr_qam64_llr_2layer_lbest_q15_layer(&stream0_in[re], &stream1_in[re], &ch_mag[re], &ch_mag_i[re],
                                        &stream0_out[re * 6], &rho01[re], length - re, 9);
}

#undef NR_LBEST_METRIC16
#undef NR_LBEST_EVAL16
