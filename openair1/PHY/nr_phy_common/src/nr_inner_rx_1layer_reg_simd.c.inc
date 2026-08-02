/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Register-fused single-layer inner RX: per SIMD block, MRC compensation + magnitudes + LLR are
 * all computed in registers and the LLR is stored directly — NO tile scratch and NO per-tile LLR
 * function call (the "one read -> compute -> one write" form). Included once per width. Emits
 * nr_inner_rx_1layer_reg_w128 / _w256. Bit-exact with {nr_channel_compensation + nr_XXqam_llr}:
 * same per-RE math (protected_abs = abs of INT16_MIN-clamped) and the same per-RE LLR order.
 */
static void NRLB_NAME(nr_inner_rx_1layer_reg)(uint32_t length,
                                              uint32_t buffer_length,
                                              int nb_rx_ant,
                                              c16_t rxFext[nb_rx_ant][buffer_length],
                                              c16_t chFext[nb_rx_ant][buffer_length],
                                              int mod_order,
                                              int output_shift,
                                              int16_t *llr)
{
  const NRLB_VI ones_v = NRLB_MM(set1_epi16)(1);
#define NRLB_PABS(v) NRLB_MM(abs_epi16)(NRLB_MM(adds_epi16)(NRLB_MM(subs_epi16)((v), ones_v), ones_v))

  NRLB_VI QAM_ampa = NRLB_SETZERO(), QAM_ampb = NRLB_SETZERO(), QAM_ampc = NRLB_SETZERO();
  if (mod_order == 4) {
    QAM_ampa = NRLB_MM(set1_epi16)(QAM16_n1);
  } else if (mod_order == 6) {
    QAM_ampa = NRLB_MM(set1_epi16)(QAM64_n1);
    QAM_ampb = NRLB_MM(set1_epi16)(QAM64_n2);
  } else if (mod_order == 8) {
    QAM_ampa = NRLB_MM(set1_epi16)(QAM256_n1);
    QAM_ampb = NRLB_MM(set1_epi16)(QAM256_n2);
    QAM_ampc = NRLB_MM(set1_epi16)(QAM256_n3);
  }

  for (uint32_t base = 0; base < length; base += NRLB_N_RE) {
    const uint32_t blk = base / NRLB_N_RE;
    const uint32_t nvalid = (length - base < NRLB_N_RE) ? (length - base) : NRLB_N_RE;

    // MRC + magnitudes for this block, accumulated across Rx antennas
    NRLB_VI rxComp, maga = NRLB_SETZERO(), magb = NRLB_SETZERO(), magc = NRLB_SETZERO();
    for (int aarx = 0; aarx < nb_rx_ant; aarx++) {
      const NRLB_VI chF = ((const NRLB_VI *)chFext[aarx])[blk];
      const NRLB_VI rxF = ((const NRLB_VI *)rxFext[aarx])[blk];
      const NRLB_VI comp = NRLB_OAI(cpx_mult_conj)(chF, rxF, output_shift);
      rxComp = (aarx == 0) ? comp : NRLB_MM(add_epi16)(rxComp, comp);
      if (mod_order > 2) {
        NRLB_VI mag = NRLB_OAI(smadd)(chF, chF, output_shift);
        mag = NRLB_MM(packs_epi32)(mag, mag);
        mag = NRLB_MM(unpacklo_epi16)(mag, mag);
        const NRLB_VI ma = NRLB_MM(mulhrs_epi16)(mag, QAM_ampa);
        maga = (aarx == 0) ? ma : NRLB_MM(add_epi16)(maga, ma);
        if (mod_order > 4) {
          const NRLB_VI mb = NRLB_MM(mulhrs_epi16)(mag, QAM_ampb);
          magb = (aarx == 0) ? mb : NRLB_MM(add_epi16)(magb, mb);
        }
        if (mod_order > 6) {
          const NRLB_VI mc = NRLB_MM(mulhrs_epi16)(mag, QAM_ampc);
          magc = (aarx == 0) ? mc : NRLB_MM(add_epi16)(magc, mc);
        }
      }
    }

    // LLR in registers. int32-per-RE layout [v0, s1, s2, s3][re] == the bytes nr_XXqam_llr write.
    const int npr = mod_order / 2; // int32 (c16) words per RE
    NRLB_VI v0, s1 = NRLB_SETZERO(), s2 = NRLB_SETZERO(), s3 = NRLB_SETZERO();
    if (mod_order == 2) {
      v0 = NRLB_MM(srai_epi16)(rxComp, 4);
    } else {
      v0 = rxComp;
      s1 = NRLB_MM(subs_epi16)(maga, NRLB_PABS(rxComp));
      if (mod_order > 4)
        s2 = NRLB_MM(subs_epi16)(magb, NRLB_PABS(s1));
      if (mod_order > 6)
        s3 = NRLB_MM(subs_epi16)(magc, NRLB_PABS(s2));
    }
    int32_t *o = (int32_t *)&llr[(size_t)base * mod_order];
// Store one RE by lane (compile-time lane -> a register extract, no stack spill).
#define NRLB_STORE_RE(re)                                    \
  do {                                                       \
    o[(re) * npr + 0] = NRLB_MM(extract_epi32)(v0, (re));    \
    if (npr > 1)                                             \
      o[(re) * npr + 1] = NRLB_MM(extract_epi32)(s1, (re));  \
    if (npr > 2)                                             \
      o[(re) * npr + 2] = NRLB_MM(extract_epi32)(s2, (re));  \
    if (npr > 3)                                             \
      o[(re) * npr + 3] = NRLB_MM(extract_epi32)(s3, (re));  \
  } while (0)
    if (nvalid == NRLB_N_RE) { // full block: compile-time-index extracts
      NRLB_STORE_RE(0);
      NRLB_STORE_RE(1);
      NRLB_STORE_RE(2);
      NRLB_STORE_RE(3);
#if NRLB_W >= 256
      NRLB_STORE_RE(4);
      NRLB_STORE_RE(5);
      NRLB_STORE_RE(6);
      NRLB_STORE_RE(7);
#endif
    } else { // partial tail block (runtime-indexed; only ever one such block)
      const int32_t *r0 = (const int32_t *)&v0, *r1 = (const int32_t *)&s1;
      const int32_t *r2 = (const int32_t *)&s2, *r3 = (const int32_t *)&s3;
      for (uint32_t re = 0; re < nvalid; re++) {
        o[re * npr + 0] = r0[re];
        if (npr > 1)
          o[re * npr + 1] = r1[re];
        if (npr > 2)
          o[re * npr + 2] = r2[re];
        if (npr > 3)
          o[re * npr + 3] = r3[re];
      }
    }
#undef NRLB_STORE_RE
  }
#undef NRLB_PABS
}
