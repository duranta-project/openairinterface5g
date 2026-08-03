/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Width-parameterized 2-layer near-ML fused inner RX (MRC + rho build + joint ML-LLR, tiled).
 * Included once per width (define NRLB_W and include nr_lbest_simd_width.h first). Compensation +
 * rho use NRLB_* ops; nr_compute_ML_llr is width-agnostic. Only mag_a is built (the ML kernel
 * derives the other thresholds). Emits nr_inner_rx_2layer_ml_w128 / _w256.
 */
static void NRLB_NAME(nr_inner_rx_2layer_ml)(uint32_t length,
                                             uint32_t buffer_length,
                                             int nb_rx_ant,
                                             c16_t rxFext[nb_rx_ant][buffer_length],
                                             c16_t chFext[2][nb_rx_ant][buffer_length],
                                             int mod_order,
                                             int output_shift,
                                             int16_t *llr0,
                                             int16_t *llr1,
                                             int16_t *llr_cw,
                                             const int16_t *scramble)
{
  NRLB_VI QAM_ampa = NRLB_SETZERO();
  if (mod_order == 4)
    QAM_ampa = NRLB_MM(set1_epi16)(QAM16_n1);
  else if (mod_order == 6)
    QAM_ampa = NRLB_MM(set1_epi16)(QAM64_n1);
  else if (mod_order == 8)
    QAM_ampa = NRLB_MM(set1_epi16)(QAM256_n1);

  enum { TILE = 96 };
  __attribute__((aligned(32))) c16_t rxC0_t[TILE], rxC1_t[TILE];
  __attribute__((aligned(32))) c16_t mag0_t[TILE], mag1_t[TILE];
  __attribute__((aligned(32))) c16_t rho01_t[TILE], rho10_t[TILE];
  // Per-layer LLR tile scratch (codeword-output mode): the detector writes here (L1-hot), then the
  // per-tile loop below reorders (layer demap) + optionally descrambles into the codeword buffer.
  __attribute__((aligned(32))) int16_t tl0[(TILE + 16) * 8], tl1[(TILE + 16) * 8]; // mod_order <= 8 (+1 SIMD block slop)

  NRLB_VI *rxC0 = (NRLB_VI *)rxC0_t, *rxC1 = (NRLB_VI *)rxC1_t;
  NRLB_VI *mag0 = (NRLB_VI *)mag0_t, *mag1 = (NRLB_VI *)mag1_t;
  NRLB_VI *rho01 = (NRLB_VI *)rho01_t, *rho10 = (NRLB_VI *)rho10_t;

  for (uint32_t base = 0; base < length; base += TILE) {
    const uint32_t tre = (length - base < TILE) ? (length - base) : TILE;
    uint32_t hi = base + TILE;
    if (hi > buffer_length)
      hi = buffer_length;
    const uint32_t nblk = (hi - base) / NRLB_N_RE;

    // First Rx antenna: direct store
    {
      NRLB_VI *rxF = (NRLB_VI *)&rxFext[0][base];
      NRLB_VI *c0 = (NRLB_VI *)&chFext[0][0][base];
      NRLB_VI *c1 = (NRLB_VI *)&chFext[1][0][base];
      for (uint32_t i = 0; i < nblk; i++) {
        rxC0[i] = NRLB_OAI(cpx_mult_conj)(c0[i], rxF[i], output_shift);
        rxC1[i] = NRLB_OAI(cpx_mult_conj)(c1[i], rxF[i], output_shift);
        rho01[i] = NRLB_OAI(cpx_mult_conj)(c0[i], c1[i], output_shift);
        rho10[i] = NRLB_OAI(cpx_mult_conj)(c1[i], c0[i], output_shift);
        if (mod_order > 2) {
          NRLB_VI m0 = NRLB_OAI(smadd)(c0[i], c0[i], output_shift);
          m0 = NRLB_MM(packs_epi32)(m0, m0);
          m0 = NRLB_MM(unpacklo_epi16)(m0, m0);
          mag0[i] = NRLB_MM(mulhrs_epi16)(m0, QAM_ampa);
          NRLB_VI m1 = NRLB_OAI(smadd)(c1[i], c1[i], output_shift);
          m1 = NRLB_MM(packs_epi32)(m1, m1);
          m1 = NRLB_MM(unpacklo_epi16)(m1, m1);
          mag1[i] = NRLB_MM(mulhrs_epi16)(m1, QAM_ampa);
        }
      }
    }

    // Remaining Rx antennas: accumulate (MRC + Gram)
    for (int aarx = 1; aarx < nb_rx_ant; aarx++) {
      NRLB_VI *rxF = (NRLB_VI *)&rxFext[aarx][base];
      NRLB_VI *c0 = (NRLB_VI *)&chFext[0][aarx][base];
      NRLB_VI *c1 = (NRLB_VI *)&chFext[1][aarx][base];
      for (uint32_t i = 0; i < nblk; i++) {
        rxC0[i] = NRLB_MM(add_epi16)(rxC0[i], NRLB_OAI(cpx_mult_conj)(c0[i], rxF[i], output_shift));
        rxC1[i] = NRLB_MM(add_epi16)(rxC1[i], NRLB_OAI(cpx_mult_conj)(c1[i], rxF[i], output_shift));
        rho01[i] = NRLB_MM(adds_epi16)(rho01[i], NRLB_OAI(cpx_mult_conj)(c0[i], c1[i], output_shift));
        rho10[i] = NRLB_MM(adds_epi16)(rho10[i], NRLB_OAI(cpx_mult_conj)(c1[i], c0[i], output_shift));
        if (mod_order > 2) {
          NRLB_VI m0 = NRLB_OAI(smadd)(c0[i], c0[i], output_shift);
          m0 = NRLB_MM(packs_epi32)(m0, m0);
          m0 = NRLB_MM(unpacklo_epi16)(m0, m0);
          mag0[i] = NRLB_MM(add_epi16)(mag0[i], NRLB_MM(mulhrs_epi16)(m0, QAM_ampa));
          NRLB_VI m1 = NRLB_OAI(smadd)(c1[i], c1[i], output_shift);
          m1 = NRLB_MM(packs_epi32)(m1, m1);
          m1 = NRLB_MM(unpacklo_epi16)(m1, m1);
          mag1[i] = NRLB_MM(add_epi16)(mag1[i], NRLB_MM(mulhrs_epi16)(m1, QAM_ampa));
        }
      }
    }

    // Joint 2-layer ML LLR for this tile (width-agnostic, RE-exact -> tile-safe)
    if (llr_cw) {
      // Codeword-output mode: detector -> L1 per-layer tile scratch, then fold the layer demap
      // (interleave l0,l1 per RE) and optional descramble (+-1 mullo) into the store. Bit-exact
      // with {nr_compute_ML_llr to per-layer + nr_layer_demapping + codeword unscrambling}.
      nr_compute_ML_llr(rxC0_t, rxC1_t, mag0_t, mag1_t, tl0, tl1, rho01_t, rho10_t, tre, mod_order);
      int16_t *cw = llr_cw + (size_t)base * 2 * mod_order;              // 2 layers/RE
      const int16_t *s = scramble ? scramble + (size_t)base * 2 * mod_order : NULL;
      for (uint32_t i = 0; i < tre; i++) {
        const int16_t *s0 = tl0 + (size_t)i * mod_order, *s1 = tl1 + (size_t)i * mod_order;
        int16_t *d0 = cw + (size_t)(i * 2 + 0) * mod_order, *d1 = cw + (size_t)(i * 2 + 1) * mod_order;
        if (s) {
          const int16_t *q0 = s + (size_t)(i * 2 + 0) * mod_order, *q1 = s + (size_t)(i * 2 + 1) * mod_order;
          for (int b = 0; b < mod_order; b++) {
            d0[b] = (int16_t)(s0[b] * q0[b]);
            d1[b] = (int16_t)(s1[b] * q1[b]);
          }
        } else {
          for (int b = 0; b < mod_order; b++) {
            d0[b] = s0[b];
            d1[b] = s1[b];
          }
        }
      }
    } else {
      nr_compute_ML_llr(rxC0_t, rxC1_t, mag0_t, mag1_t,
                        llr0 + (size_t)base * mod_order, llr1 + (size_t)base * mod_order,
                        rho01_t, rho10_t, tre, mod_order);
    }
  }
}
