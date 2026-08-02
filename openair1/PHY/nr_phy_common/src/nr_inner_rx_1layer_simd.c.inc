/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Width-parameterized single-layer fused inner RX (MRC compensation + per-RE LLR, tiled).
 * Included once per width (define NRLB_W and include nr_lbest_simd_width.h first). The compensation
 * uses NRLB_* ops; the LLR kernels (nr_XXqam_llr) are width-agnostic (handle any nb_re internally).
 * Emits nr_inner_rx_1layer_w128 / _w256.
 */
static void NRLB_NAME(nr_inner_rx_1layer)(uint32_t length,
                                          uint32_t buffer_length,
                                          int nb_rx_ant,
                                          c16_t rxFext[nb_rx_ant][buffer_length],
                                          c16_t chFext[nb_rx_ant][buffer_length],
                                          int mod_order,
                                          int output_shift,
                                          int16_t *llr)
{
  NRLB_VI QAM_ampa = NRLB_SETZERO();
  NRLB_VI QAM_ampb = NRLB_SETZERO();
  NRLB_VI QAM_ampc = NRLB_SETZERO();

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

  // L1-resident per-tile scratch (TILE multiple of 16 -> whole SIMD blocks at any width).
  enum { TILE = 96 };
  __attribute__((aligned(32))) c16_t rxComp_t[TILE];
  __attribute__((aligned(32))) c16_t maga_t[TILE];
  __attribute__((aligned(32))) c16_t magb_t[TILE];
  __attribute__((aligned(32))) c16_t magc_t[TILE];

  NRLB_VI *rxComp_v = (NRLB_VI *)rxComp_t;
  NRLB_VI *maga_v = (NRLB_VI *)maga_t;
  NRLB_VI *magb_v = (NRLB_VI *)magb_t;
  NRLB_VI *magc_v = (NRLB_VI *)magc_t;

  for (uint32_t base = 0; base < length; base += TILE) {
    const uint32_t tre = (length - base < TILE) ? (length - base) : TILE; // valid REs (LLR)
    uint32_t hi = base + TILE;
    if (hi > buffer_length)
      hi = buffer_length;
    const uint32_t nblk = (hi - base) / NRLB_N_RE; // SIMD blocks, never reading past buffer_length

    // First Rx antenna: direct store (no pre-memset of the tile scratch needed)
    {
      NRLB_VI *rxF = (NRLB_VI *)&rxFext[0][base];
      NRLB_VI *chF = (NRLB_VI *)&chFext[0][base];
      for (uint32_t i = 0; i < nblk; i++) {
        rxComp_v[i] = NRLB_OAI(cpx_mult_conj)(chF[i], rxF[i], output_shift);
        if (mod_order > 2) {
          NRLB_VI mag = NRLB_OAI(smadd)(chF[i], chF[i], output_shift);
          mag = NRLB_MM(packs_epi32)(mag, mag);
          mag = NRLB_MM(unpacklo_epi16)(mag, mag);
          maga_v[i] = NRLB_MM(mulhrs_epi16)(mag, QAM_ampa);
          if (mod_order > 4)
            magb_v[i] = NRLB_MM(mulhrs_epi16)(mag, QAM_ampb);
          if (mod_order > 6)
            magc_v[i] = NRLB_MM(mulhrs_epi16)(mag, QAM_ampc);
        }
      }
    }

    // Remaining Rx antennas: accumulate (MRC)
    for (int aarx = 1; aarx < nb_rx_ant; aarx++) {
      NRLB_VI *rxF = (NRLB_VI *)&rxFext[aarx][base];
      NRLB_VI *chF = (NRLB_VI *)&chFext[aarx][base];
      for (uint32_t i = 0; i < nblk; i++) {
        rxComp_v[i] = NRLB_MM(add_epi16)(rxComp_v[i], NRLB_OAI(cpx_mult_conj)(chF[i], rxF[i], output_shift));
        if (mod_order > 2) {
          NRLB_VI mag = NRLB_OAI(smadd)(chF[i], chF[i], output_shift);
          mag = NRLB_MM(packs_epi32)(mag, mag);
          mag = NRLB_MM(unpacklo_epi16)(mag, mag);
          maga_v[i] = NRLB_MM(add_epi16)(maga_v[i], NRLB_MM(mulhrs_epi16)(mag, QAM_ampa));
          if (mod_order > 4)
            magb_v[i] = NRLB_MM(add_epi16)(magb_v[i], NRLB_MM(mulhrs_epi16)(mag, QAM_ampb));
          if (mod_order > 6)
            magc_v[i] = NRLB_MM(add_epi16)(magc_v[i], NRLB_MM(mulhrs_epi16)(mag, QAM_ampc));
        }
      }
    }

    // Per-RE LLR for this tile's valid REs, straight from the L1 scratch (kernels are width-agnostic)
    int16_t *llr_off = llr + (size_t)base * mod_order;
    switch (mod_order) {
      case 2: nr_qpsk_llr(rxComp_t, llr_off, tre); break;
      case 4: nr_16qam_llr(rxComp_t, maga_t, llr_off, tre); break;
      case 6: nr_64qam_llr(rxComp_t, maga_t, magb_t, llr_off, tre); break;
      case 8: nr_256qam_llr(rxComp_t, maga_t, magb_t, magc_t, llr_off, tre); break;
      default: break;
    }
  }
}
