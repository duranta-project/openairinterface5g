/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 * Width-parameterized 2-layer 256QAM L-best SIMD kernel body. Included by
 * nr_compute_llr.c once per NRLB_W (128/256) after nr_lbest_simd_width.h (which the
 * 64QAM .inc's include already brought in for this width). Contract: match the float
 * reference via OAI_LBEST_DBG256.
 */
static inline NRLB_VI NRLB_NAME(nrlbw_level_pam)(NRLB_VI num, NRLB_VI thunit)
{
  const NRLB_VI z   = NRLB_SETZERO();
  const NRLB_VI a   = NRLB_MM(abs_epi16)(num);
  const NRLB_VI t2  = NRLB_MM(slli_epi16)(thunit, 1);
  const NRLB_VI t4  = NRLB_MM(slli_epi16)(thunit, 2);
  const NRLB_VI t8  = NRLB_MM(slli_epi16)(thunit, 3);
  const NRLB_VI t6  = NRLB_MM(adds_epi16)(t2, t4);
  const NRLB_VI t10 = NRLB_MM(adds_epi16)(t2, t8);
  const NRLB_VI t12 = NRLB_MM(adds_epi16)(t4, t8);
  const NRLB_VI t14 = NRLB_MM(adds_epi16)(t6, t8);
  const NRLB_VI g2  = NRLB_MM(sub_epi16)(z, NRLB_CMPGT16(a, t2));
  const NRLB_VI g4  = NRLB_MM(sub_epi16)(z, NRLB_CMPGT16(a, t4));
  const NRLB_VI g6  = NRLB_MM(sub_epi16)(z, NRLB_CMPGT16(a, t6));
  const NRLB_VI g8  = NRLB_MM(sub_epi16)(z, NRLB_CMPGT16(a, t8));
  const NRLB_VI g10 = NRLB_MM(sub_epi16)(z, NRLB_CMPGT16(a, t10));
  const NRLB_VI g12 = NRLB_MM(sub_epi16)(z, NRLB_CMPGT16(a, t12));
  const NRLB_VI g14 = NRLB_MM(sub_epi16)(z, NRLB_CMPGT16(a, t14));
  // mag = 1 + 2*(g2+g4+g6+g8+g10+g12+g14)
  const NRLB_VI sum = NRLB_MM(adds_epi16)(
      NRLB_MM(adds_epi16)(NRLB_MM(adds_epi16)(g2, g4), NRLB_MM(adds_epi16)(g6, g8)),
      NRLB_MM(adds_epi16)(NRLB_MM(adds_epi16)(g10, g12), g14));
  const NRLB_VI mag = NRLB_MM(adds_epi16)(NRLB_MM(set1_epi16)(1),
                                                  NRLB_MM(slli_epi16)(sum, 1));
  const NRLB_VI neg = NRLB_CMPGT16(z, num);
  return NRLB_BLENDV(mag, NRLB_MM(sub_epi16)(z, mag), neg);
}

static inline NRLB_VI NRLB_NAME(nrlbw_z170d32)(NRLB_VI z16)
{
  const NRLB_VI C = NRLB_MM(set1_epi32)(NR_LBEST_Q_SQRT170_Q12);
  const NRLB_VI lo = NRLB_MM(srai_epi32)(NRLB_MM(mullo_epi32)(NRLB_NAME(nrlbw_widen)(z16, 0), C), 17);
  const NRLB_VI hi = NRLB_MM(srai_epi32)(NRLB_MM(mullo_epi32)(NRLB_NAME(nrlbw_widen)(z16, 1), C), 17);
  return NRLB_NAME(nrlbw_pack32)(lo, hi);
}

static inline void NRLB_NAME(nrlbw_axis_llr)(NRLB_VF est, NRLB_VF g, NRLB_VF out[4])
{
  const NRLB_VF ONE = NRLB_MM(set1_ps)(1.0f), TWO = NRLB_MM(set1_ps)(2.0f), HALF = NRLB_MM(set1_ps)(0.5f);
  // o = nearest odd to est; oa = nearest odd to |est|
  const NRLB_VF a = NRLB_MM(andnot_ps)(NRLB_MM(set1_ps)(-0.0f), est);
#define NR_RODD(x) NRLB_MM(add_ps)(NRLB_MM(mul_ps)(NRLB_ROUNDPS(NRLB_MM(mul_ps)(NRLB_MM(sub_ps)((x), ONE), HALF), SIMDE_MM_FROUND_TO_NEAREST_INT | SIMDE_MM_FROUND_NO_EXC), TWO), ONE)
  const NRLB_VF o = NR_RODD(est), oa = NR_RODD(a);
#define NR_CL(x, lo, hi) NRLB_MM(min_ps)(NRLB_MM(max_ps)((x), NRLB_MM(set1_ps)(lo)), NRLB_MM(set1_ps)(hi))
#define NR_DSQ(lvl, ref) ({ const NRLB_VF _d = NRLB_MM(sub_ps)((lvl), (ref)); NRLB_MM(mul_ps)(_d, _d); })
  // b0 (sign): pos levels [1,15] (bit 0) vs neg [-15,-1] (bit 1)
  out[0] = NRLB_MM(mul_ps)(g, NRLB_MM(sub_ps)(NR_DSQ(NR_CL(o, -15.0f, -1.0f), est), NR_DSQ(NR_CL(o, 1.0f, 15.0f), est)));
  // b1 (|L|>8): |L| in [1,7] (bit 0) vs [9,15] (bit 1); by symmetry work in a=|est|
  out[1] = NRLB_MM(mul_ps)(g, NRLB_MM(sub_ps)(NR_DSQ(NR_CL(oa, 9.0f, 15.0f), a), NR_DSQ(NR_CL(oa, 1.0f, 7.0f), a)));
  // b2 (||L|-8|>4): |L| in {5,7,9,11} (bit 0) vs {1,3}U{13,15} (bit 1)
  const NRLB_VF d2_0 = NR_DSQ(NR_CL(oa, 5.0f, 11.0f), a);
  const NRLB_VF d2_1 = NRLB_MM(min_ps)(NR_DSQ(NR_CL(oa, 1.0f, 3.0f), a), NR_DSQ(NR_CL(oa, 13.0f, 15.0f), a));
  out[2] = NRLB_MM(mul_ps)(g, NRLB_MM(sub_ps)(d2_1, d2_0));
  out[3] = NRLB_MM(setzero_ps)(); // b3 always spanned by the search window -> fallback never read
#undef NR_RODD
#undef NR_CL
#undef NR_DSQ
}

static inline void NRLB_NAME(nrlbw_seed256)(NRLB_VI z1r16, NRLB_VI z1i16,
                                            NRLB_VI z2r16, NRLB_VI z2i16,
                                            NRLB_VI rr16,  NRLB_VI ri16,
                                            NRLB_VI cm0_16, NRLB_VI cm1_16,
                                            NRLB_VI *centerI, NRLB_VI *centerQ,
                                            NRLB_VI *dirImask, NRLB_VI *dirQmask,
                                            NRLB_VI axisLLR[8])
{
  NRLB_VF aL[2][8]; // [half][ {I0,Q0,I1,Q1,I2,Q2,I3,Q3} ]
  const NRLB_VI SQRT170_OVER8 = NRLB_MM(set1_epi32)(NR_LBEST_Q_SQRT170_OVER8_Q14_SIMD);
  const NRLB_VF SQRT170 = NRLB_MM(set1_ps)(13.038404810405298f);
  const NRLB_VF HALF = NRLB_MM(set1_ps)(0.5f), ONE = NRLB_MM(set1_ps)(1.0f), TWO = NRLB_MM(set1_ps)(2.0f);
  const NRLB_VF P15 = NRLB_MM(set1_ps)(15.0f), M15 = NRLB_MM(set1_ps)(-15.0f);
  NRLB_VI cI[2], cQ[2], dI[2], dQ[2];
  for (int h = 0; h < 2; h++) {
    const NRLB_VF z1r = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z1r16, h)), z1i = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z1i16, h));
    const NRLB_VF z2r = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z2r16, h)), z2i = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(z2i16, h));
    const NRLB_VF rr  = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(rr16,  h)), ri  = NRLB_MM(cvtepi32_ps)(NRLB_NAME(nrlbw_widen)(ri16,  h));
    const NRLB_VF Pp0 = NRLB_MM(cvtepi32_ps)(NRLB_MM(srai_epi32)(NRLB_MM(mullo_epi32)(NRLB_NAME(nrlbw_widen)(cm0_16, h), SQRT170_OVER8), 14));
    const NRLB_VF Pp1 = NRLB_MM(cvtepi32_ps)(NRLB_MM(srai_epi32)(NRLB_MM(mullo_epi32)(NRLB_NAME(nrlbw_widen)(cm1_16, h), SQRT170_OVER8), 14));
    // ZF 2x2 solve (float): x_hat1 = (Pp1*z1 - rho*z2) / det
    NRLB_VF det = NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(Pp0, Pp1),
                                         NRLB_MM(add_ps)(NRLB_MM(mul_ps)(rr, rr), NRLB_MM(mul_ps)(ri, ri)));
    det = NRLB_MM(max_ps)(det, ONE);
    const NRLB_VF numr = NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(Pp1, z1r),
                                                 NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(rr, z2r), NRLB_MM(mul_ps)(ri, z2i)));
    const NRLB_VF numi = NRLB_MM(sub_ps)(NRLB_MM(mul_ps)(Pp1, z1i),
                                                 NRLB_MM(add_ps)(NRLB_MM(mul_ps)(rr, z2i), NRLB_MM(mul_ps)(ri, z2r)));
    // soft level estimate ~ +-1..15
    const NRLB_VF estI = NRLB_MM(mul_ps)(NRLB_MM(div_ps)(numr, det), SQRT170);
    const NRLB_VF estQ = NRLB_MM(mul_ps)(NRLB_MM(div_ps)(numi, det), SQRT170);
    // graded per-axis PAM-16 LLRs from the soft estimate (g = 0.5*P0/170, P0 = Pp0)
    const NRLB_VF glin = NRLB_MM(mul_ps)(NRLB_MM(set1_ps)(0.5f / 170.0f), Pp0);
    NRLB_VF aI4[4], aQ4[4];
    NRLB_NAME(nrlbw_axis_llr)(estI, glin, aI4);
    NRLB_NAME(nrlbw_axis_llr)(estQ, glin, aQ4);
    for (int b = 0; b < 4; b++) { aL[h][2 * b] = aI4[b]; aL[h][2 * b + 1] = aQ4[b]; }
    // nearest odd level: o = 2*round((est-1)/2)+1, clamped to [-15,15]
    NRLB_VF oI = NRLB_MM(add_ps)(NRLB_MM(mul_ps)(NRLB_ROUNDPS(NRLB_MM(mul_ps)(NRLB_MM(sub_ps)(estI, ONE), HALF), SIMDE_MM_FROUND_TO_NEAREST_INT | SIMDE_MM_FROUND_NO_EXC), TWO), ONE);
    NRLB_VF oQ = NRLB_MM(add_ps)(NRLB_MM(mul_ps)(NRLB_ROUNDPS(NRLB_MM(mul_ps)(NRLB_MM(sub_ps)(estQ, ONE), HALF), SIMDE_MM_FROUND_TO_NEAREST_INT | SIMDE_MM_FROUND_NO_EXC), TWO), ONE);
    oI = NRLB_MM(min_ps)(NRLB_MM(max_ps)(oI, M15), P15);
    oQ = NRLB_MM(min_ps)(NRLB_MM(max_ps)(oQ, M15), P15);
    cI[h] = NRLB_MM(cvtps_epi32)(oI);
    cQ[h] = NRLB_MM(cvtps_epi32)(oQ);
    dI[h] = NRLB_CMPPS_SI(estI, oI, SIMDE_CMP_GT_OQ);
    dQ[h] = NRLB_CMPPS_SI(estQ, oQ, SIMDE_CMP_GT_OQ);
  }
  *centerI  = NRLB_NAME(nrlbw_pack32)(cI[0], cI[1]);
  *centerQ  = NRLB_NAME(nrlbw_pack32)(cQ[0], cQ[1]);
  *dirImask = NRLB_NAME(nrlbw_pack32)(dI[0], dI[1]);
  *dirQmask = NRLB_NAME(nrlbw_pack32)(dQ[0], dQ[1]);
  for (int p = 0; p < 8; p++) // round to nearest int16, saturating pack (final LLR units)
    axisLLR[p] = NRLB_NAME(nrlbw_pack32)(NRLB_MM(cvtps_epi32)(aL[0][p]), NRLB_MM(cvtps_epi32)(aL[1][p]));
}

#define NR_LBEST_METRIC16_256(MET_, I1_, Q1_, C0I_, C0Q_, EI_, EQ_, RRI_, RII_, RRQ_, RIQ_) \
  do { \
    const NRLB_VI A2I = NRLB_MM(subs_epi16)(z2r170, NRLB_MM(adds_epi16)((RRI_), (RIQ_))); \
    const NRLB_VI A2Q = NRLB_MM(subs_epi16)(z2i170, NRLB_MM(subs_epi16)((RRQ_), (RII_))); \
    const NRLB_VI I2  = NRLB_NAME(nrlbw_level_pam)(A2I, Pp1_5_256); \
    const NRLB_VI Q2  = NRLB_NAME(nrlbw_level_pam)(A2Q, Pp1_5_256); \
    const NRLB_VI corr0   = NRLB_MM(adds_epi16)((C0I_), (C0Q_)); \
    const NRLB_VI energy0 = NRLB_MM(adds_epi16)((EI_), (EQ_)); \
    const NRLB_VI corr1   = NRLB_MM(adds_epi16)(NRLB_MM(mulhi_epi16)(z2r, NRLB_MM(mullo_epi16)(I2, C13_256)), \
                                                        NRLB_MM(mulhi_epi16)(z2i, NRLB_MM(mullo_epi16)(Q2, C13_256))); \
    const NRLB_VI energy1 = NRLB_MM(mulhi_epi16)(cm1, NRLB_MM(mullo_epi16)(NRLB_MM(adds_epi16)(NRLB_MM(mullo_epi16)(I2, I2), NRLB_MM(mullo_epi16)(Q2, Q2)), CE_256)); \
    const NRLB_VI aa = NRLB_MM(adds_epi16)(NRLB_MM(mullo_epi16)((I1_), I2), NRLB_MM(mullo_epi16)((Q1_), Q2)); \
    const NRLB_VI bb = NRLB_MM(subs_epi16)(NRLB_MM(mullo_epi16)((Q1_), I2), NRLB_MM(mullo_epi16)((I1_), Q2)); \
    const NRLB_VI cross = NRLB_MM(adds_epi16)(NRLB_MM(mulhi_epi16)(rr, NRLB_MM(mullo_epi16)(aa, CX_256)), \
                                                      NRLB_MM(mulhi_epi16)(ri, NRLB_MM(mullo_epi16)(bb, CX_256))); \
    (MET_) = NRLB_MM(max_epi16)(NRLB_MM(subs_epi16)(NRLB_MM(adds_epi16)(NRLB_MM(subs_epi16)(corr0, energy0), NRLB_MM(subs_epi16)(corr1, energy1)), cross), FLOORM); \
  } while (0)

#define NR_LBEST_EVAL16_256(I1_, Q1_, BI0, BI1, BI2, BI3, BQ0, BQ1, BQ2, BQ3, C0I_, C0Q_, EI_, EQ_, RRI_, RII_, RRQ_, RIQ_) \
  do { \
    NRLB_VI metric; \
    NR_LBEST_METRIC16_256(metric, I1_, Q1_, C0I_, C0Q_, EI_, EQ_, RRI_, RII_, RRQ_, RIQ_); \
    const NRLB_VI bm[8] = {(BI0), (BQ0), (BI1), (BQ1), (BI2), (BQ2), (BI3), (BQ3)}; \
    for (int p = 0; p < 8; p++) { \
      max0[p] = NRLB_MM(max_epi16)(max0[p], NRLB_BLENDV(metric, SENT, bm[p])); \
      max1[p] = NRLB_MM(max_epi16)(max1[p], NRLB_BLENDV(SENT, metric, bm[p])); \
    } \
  } while (0)

void NRLB_NAME(nr_qam256_llr_2layer_lbest_q15_simd)(c16_t *stream0_in,
                                           c16_t *stream1_in,
                                           c16_t *ch_mag,
                                           c16_t *ch_mag_i,
                                           int16_t *stream0_out,
                                           c16_t *rho01,
                                           uint32_t length,
                                           int pattern)
{
  const NRLB_VI SENT   = NRLB_MM(set1_epi16)(NR_LBEST_SIMD16_SENT);
  const NRLB_VI FLOORM = NRLB_MM(set1_epi16)(-32000);
  const NRLB_VI V15 = NRLB_MM(set1_epi16)(15), Vm15 = NRLB_MM(set1_epi16)(-15);
  const NRLB_VI z   = NRLB_SETZERO();

  // 256QAM metric Q-constants (metric scale = 1/8, matching 64QAM SIMD kernel):
  //   C13_256 = round(2^13/sqrt(170))       => mulhi(z, I*C13) = z*I/(8*sqrt(170))
  //   CE_256  = round(2^16/(8*8*2*sqrt(170)))  => energy = cm*|X|^2/(8*8*2*sqrt(170))
  //   CX_256  = round(2^16/(8*170))         => cross term
  const NRLB_VI C13_256 = NRLB_MM(set1_epi16)(628);
  const NRLB_VI CE_256  = NRLB_MM(set1_epi16)(39);
  const NRLB_VI CX_256  = NRLB_MM(set1_epi16)(48);

  uint32_t re = 0;
  for (; re + NRLB_N <= length; re += NRLB_N) {
    NRLB_VI z1r, z1i, z2r, z2i, rr, ri, cm0, cm1, dummy;
    NRLB_NAME(nrlbw_load)(&stream0_in[re], &z1r, &z1i);
    NRLB_NAME(nrlbw_load)(&stream1_in[re], &z2r, &z2i);
    NRLB_NAME(nrlbw_load)(&rho01[re],      &rr,  &ri);
    NRLB_NAME(nrlbw_load)(&ch_mag[re],     &cm0, &dummy);
    NRLB_NAME(nrlbw_load)(&ch_mag_i[re],   &cm1, &dummy);

    NRLB_VI centerI, centerQ, dirImask, dirQmask, axisLLR[8];
    NRLB_NAME(nrlbw_seed256)(z1r, z1i, z2r, z2i, rr, ri, cm0, cm1,
                             &centerI, &centerQ, &dirImask, &dirQmask, axisLLR);
    (void)dirImask; (void)dirQmask;

    // per-RE-vector precomputed quantities (int16 scale, /32 shift)
    const NRLB_VI z2r170   = NRLB_NAME(nrlbw_z170d32)(z2r), z2i170 = NRLB_NAME(nrlbw_z170d32)(z2i);
    const NRLB_VI rr5      = NRLB_MM(srai_epi16)(rr, 5), ri5 = NRLB_MM(srai_epi16)(ri, 5);
    const NRLB_VI Pp1_5_256 = NRLB_MM(mulhi_epi16)(cm1, NRLB_MM(set1_epi16)(NR_LBEST_Q_PP1_OVER32_Q16_256));

    NRLB_VI max0[8], max1[8];
    for (int p = 0; p < 8; p++) { max0[p] = SENT; max1[p] = SENT; }

    // ---- 5x5 block (hoisted) ----
    // Precompute 5 I-levels and 5 Q-levels, then evaluate all or a subset of pairs.
    // Offsets: {-4,-2,0,+2,+4} around seed center.
    static const int offs5[5] = {-4, -2, 0, 2, 4};
    NRLB_VI Iv[5], Qv[5];
    NRLB_VI bIv[5][4], bQv[5][4];                // [level][bit 0..3]
    NRLB_VI c0I[5], c0Q[5], eI[5], eQ[5];
    NRLB_VI rrI[5], riI[5], rrQ[5], riQ[5];

    const NRLB_VI V8 = NRLB_MM(set1_epi16)(8), V4 = NRLB_MM(set1_epi16)(4), V2 = NRLB_MM(set1_epi16)(2);

    for (int k = 0; k < 5; k++) {
      const NRLB_VI I1 = NRLB_MM(min_epi16)(NRLB_MM(max_epi16)(NRLB_MM(adds_epi16)(centerI, NRLB_MM(set1_epi16)(offs5[k])), Vm15), V15);
      const NRLB_VI Q1 = NRLB_MM(min_epi16)(NRLB_MM(max_epi16)(NRLB_MM(adds_epi16)(centerQ, NRLB_MM(set1_epi16)(offs5[k])), Vm15), V15);
      Iv[k] = I1; Qv[k] = Q1;

      // 256QAM Gray bit labeling per axis (b0..b3):
      //   b0 = sign (level < 0)
      //   b1 = |level| > 8
      //   b2 = ||level|-8| > 4
      //   b3 = |||level|-8|-4| > 2
      const NRLB_VI aI  = NRLB_MM(abs_epi16)(I1);
      const NRLB_VI m2I = NRLB_MM(abs_epi16)(NRLB_MM(sub_epi16)(aI,  V8));
      const NRLB_VI m3I = NRLB_MM(abs_epi16)(NRLB_MM(sub_epi16)(m2I, V4));
      bIv[k][0] = NRLB_CMPGT16(z,   I1);
      bIv[k][1] = NRLB_CMPGT16(aI,  V8);
      bIv[k][2] = NRLB_CMPGT16(m2I, V4);
      bIv[k][3] = NRLB_CMPGT16(m3I, V2);

      const NRLB_VI aQ  = NRLB_MM(abs_epi16)(Q1);
      const NRLB_VI m2Q = NRLB_MM(abs_epi16)(NRLB_MM(sub_epi16)(aQ,  V8));
      const NRLB_VI m3Q = NRLB_MM(abs_epi16)(NRLB_MM(sub_epi16)(m2Q, V4));
      bQv[k][0] = NRLB_CMPGT16(z,   Q1);
      bQv[k][1] = NRLB_CMPGT16(aQ,  V8);
      bQv[k][2] = NRLB_CMPGT16(m2Q, V4);
      bQv[k][3] = NRLB_CMPGT16(m3Q, V2);

      c0I[k] = NRLB_MM(mulhi_epi16)(z1r, NRLB_MM(mullo_epi16)(I1, C13_256));
      c0Q[k] = NRLB_MM(mulhi_epi16)(z1i, NRLB_MM(mullo_epi16)(Q1, C13_256));
      eI[k]  = NRLB_MM(mulhi_epi16)(cm0, NRLB_MM(mullo_epi16)(NRLB_MM(mullo_epi16)(I1, I1), CE_256));
      eQ[k]  = NRLB_MM(mulhi_epi16)(cm0, NRLB_MM(mullo_epi16)(NRLB_MM(mullo_epi16)(Q1, Q1), CE_256));
      rrI[k] = NRLB_MM(mullo_epi16)(rr5, I1); riI[k] = NRLB_MM(mullo_epi16)(ri5, I1);
      rrQ[k] = NRLB_MM(mullo_epi16)(rr5, Q1); riQ[k] = NRLB_MM(mullo_epi16)(ri5, Q1);
    }

    if (pattern == 0) { // 5x5: all 25 grid pairs
      for (int iI = 0; iI < 5; iI++)
        for (int iQ = 0; iQ < 5; iQ++)
          NR_LBEST_EVAL16_256(Iv[iI], Qv[iQ],
                              bIv[iI][0], bIv[iI][1], bIv[iI][2], bIv[iI][3],
                              bQv[iQ][0], bQv[iQ][1], bQv[iQ][2], bQv[iQ][3],
                              c0I[iI], c0Q[iQ], eI[iI], eQ[iQ],
                              rrI[iI], riI[iI], rrQ[iQ], riQ[iQ]);
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
      for (int i = 0; i < 3; i++) maxI[i] = NRLB_MM(max_epi16)(met[i][0], NRLB_MM(max_epi16)(met[i][1], met[i][2]));
      for (int q = 0; q < 3; q++) maxQ[q] = NRLB_MM(max_epi16)(met[0][q], NRLB_MM(max_epi16)(met[1][q], met[2][q]));
      for (int k = 0; k < 4; k++) // I-bit k -> output p=2k ; Q-bit k -> p=2k+1
        for (int i = 0; i < 3; i++) {
          const NRLB_VI bI = bIv[ci3[i]][k], bQ = bQv[ci3[i]][k];
          max0[2 * k]     = NRLB_MM(max_epi16)(max0[2 * k],     NRLB_BLENDV(maxI[i], SENT, bI));
          max1[2 * k]     = NRLB_MM(max_epi16)(max1[2 * k],     NRLB_BLENDV(SENT, maxI[i], bI));
          max0[2 * k + 1] = NRLB_MM(max_epi16)(max0[2 * k + 1], NRLB_BLENDV(maxQ[i], SENT, bQ));
          max1[2 * k + 1] = NRLB_MM(max_epi16)(max1[2 * k + 1], NRLB_BLENDV(SENT, maxQ[i], bQ));
        }
    } else if (pattern == 2) { // 5-plus: center + +-4 each axis (5 cand)
      // indices: (2,2)=center, (0,2)=I-4, (4,2)=I+4, (2,0)=Q-4, (2,4)=Q+4
      static const int pI5[5] = {2, 0, 4, 2, 2}, pQ5[5] = {2, 2, 2, 0, 4};
      for (int c = 0; c < 5; c++) {
        const int iI = pI5[c], iQ = pQ5[c];
        NR_LBEST_EVAL16_256(Iv[iI], Qv[iQ],
                            bIv[iI][0], bIv[iI][1], bIv[iI][2], bIv[iI][3],
                            bQv[iQ][0], bQv[iQ][1], bQv[iQ][2], bQv[iQ][3],
                            c0I[iI], c0Q[iQ], eI[iI], eQ[iQ],
                            rrI[iI], riI[iI], rrQ[iQ], riQ[iQ]);
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
        const NRLB_VI aI  = NRLB_MM(abs_epi16)(I1);
        const NRLB_VI m2I = NRLB_MM(abs_epi16)(NRLB_MM(sub_epi16)(aI,  V8));
        const NRLB_VI m3I = NRLB_MM(abs_epi16)(NRLB_MM(sub_epi16)(m2I, V4));
        fbI[k][0] = NRLB_CMPGT16(z,   I1);
        fbI[k][1] = NRLB_CMPGT16(aI,  V8);
        fbI[k][2] = NRLB_CMPGT16(m2I, V4);
        fbI[k][3] = NRLB_CMPGT16(m3I, V2);
        fc0I[k] = NRLB_MM(mulhi_epi16)(z1r, NRLB_MM(mullo_epi16)(I1, C13_256));
        fc0Q[k] = NRLB_MM(mulhi_epi16)(z1i, NRLB_MM(mullo_epi16)(I1, C13_256));
        feI[k]  = NRLB_MM(mulhi_epi16)(cm0, NRLB_MM(mullo_epi16)(NRLB_MM(mullo_epi16)(I1, I1), CE_256));
        frrI[k] = NRLB_MM(mullo_epi16)(rr5, I1);
        friI[k] = NRLB_MM(mullo_epi16)(ri5, I1);
      }
      for (int iI = 0; iI < 16; iI++)
        for (int iQ = 0; iQ < 16; iQ++)
          NR_LBEST_EVAL16_256(fI[iI], fI[iQ],
                              fbI[iI][0], fbI[iI][1], fbI[iI][2], fbI[iI][3],
                              fbI[iQ][0], fbI[iQ][1], fbI[iQ][2], fbI[iQ][3],
                              fc0I[iI], fc0Q[iQ], feI[iI], feI[iQ],
                              frrI[iI], friI[iI], frrI[iQ], friI[iQ]);
    }

    // TODO(int8-clip): these (correct, ML-faithful) 256QAM LLRs are hot for the 8-bit LDPC
    // decoder input — OAI_LBEST_DBG256 measures ~35% of them |.|>=128 (clip int8), which can
    // add a small high-SNR error floor. Second-order vs the graded-fallback fix above, and
    // naive log2_maxh cooling made BLER worse (precision loss), so a proper LLR->int8 rescale
    // (matched to the linear/MMSE path's scale) is needed, not just a shift. Not yet done.
    NRLB_VI res[8];
    for (int p = 0; p < 8; p++) {
      const NRLB_VI diff = NRLB_MM(subs_epi16)(max0[p], max1[p]);
      // metric is at 1/8 scale -> LLR = diff*8; saturate via int32 widening
      const NRLB_VI lo = NRLB_MM(slli_epi32)(NRLB_NAME(nrlbw_widen)(diff, 0), 3);
      const NRLB_VI hi = NRLB_MM(slli_epi32)(NRLB_NAME(nrlbw_widen)(diff, 1), 3);
      NRLB_VI r = NRLB_NAME(nrlbw_pack32)(lo, hi);
      // unspanned bit (one side has no candidate): use the graded per-axis seed LLR instead of
      // the hard +-LLR_SAT (which wrecks the coarse/MSB bits a local search cannot span).
      const NRLB_VI unspanned = NRLB_OR(NRLB_CMPEQ16(max0[p], SENT),
                                                          NRLB_CMPEQ16(max1[p], SENT));
      res[p] = NRLB_BLENDV(r, axisLLR[p], unspanned);
    }
    NRLB_NAME(nrlbw_store_llr)(stream0_out, re, res, 8);
  }

  if (re < length)
    nr_qam256_llr_2layer_lbest_q15_layer(&stream0_in[re], &stream1_in[re], &ch_mag[re], &ch_mag_i[re],
                                         &stream0_out[re * 8], &rho01[re], length - re, (pattern == 3) ? 256 : 25);
}

#undef NR_LBEST_METRIC16_256
#undef NR_LBEST_EVAL16_256
