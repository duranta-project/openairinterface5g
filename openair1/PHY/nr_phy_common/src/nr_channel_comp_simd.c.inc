/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Width-parameterized MRC channel-compensation core. Included once per width
 * (define NRLB_W = 128/256/512 and include nr_lbest_simd_width.h first). Emits
 * nr_channel_compensation_w128 / _w256 / _w512. One source -> AVX2 (256),
 * SSE->NEON (128), AVX-512 (512). Bit-exact across widths: every op is per-lane
 * or per-128-lane (the mag pack packs_epi32(m,m)+unpacklo_epi16 self-duplicates
 * within each 128-lane, preserving RE order at all widths).
 */
static void NRLB_NAME(nr_channel_compensation)(uint32_t buffer_length,
                                               uint32_t pdsch_buf_size_max,
                                               int nb_rx_ant,
                                               int nb_layers,
                                               c16_t rxFext[nb_rx_ant][buffer_length],
                                               c16_t chFext[nb_layers][nb_rx_ant][buffer_length],
                                               c16_t ch_maga[nb_layers][pdsch_buf_size_max],
                                               c16_t ch_magb[nb_layers][pdsch_buf_size_max],
                                               c16_t ch_magc[nb_layers][pdsch_buf_size_max],
                                               c16_t **rxComp,
                                               c16_t (*rho)[nb_layers][pdsch_buf_size_max],
                                               int mod_order,
                                               uint32_t symbol,
                                               uint32_t output_shift)
{
  const uint32_t nvec = buffer_length / NRLB_N_RE; // SIMD vectors per symbol (NRLB_N_RE REs each)

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

  for (int aatx = 0; aatx < nb_layers; aatx++) {
    NRLB_VI *rxComp_v = (NRLB_VI *)&rxComp[aatx][symbol * buffer_length];
    NRLB_VI *ch_maga_v = (NRLB_VI *)ch_maga[aatx];
    NRLB_VI *ch_magb_v = (NRLB_VI *)ch_magb[aatx];
    NRLB_VI *ch_magc_v = (NRLB_VI *)ch_magc[aatx];

    // First Rx antenna: direct store — eliminates need to pre memset the output buffers
    {
      NRLB_VI *rxF_v = (NRLB_VI *)rxFext[0];
      NRLB_VI *chF_v = (NRLB_VI *)chFext[aatx][0];

      for (uint32_t i = 0; i < nvec; i++) {
        rxComp_v[i] = NRLB_OAI(cpx_mult_conj)(chF_v[i], rxF_v[i], output_shift);

        if (mod_order > 2) {
          NRLB_VI mag = NRLB_OAI(smadd)(chF_v[i], chF_v[i], output_shift);
          mag = NRLB_MM(packs_epi32)(mag, mag);
          mag = NRLB_MM(unpacklo_epi16)(mag, mag);
          ch_maga_v[i] = NRLB_MM(mulhrs_epi16)(mag, QAM_ampa);

          if (mod_order > 4)
            ch_magb_v[i] = NRLB_MM(mulhrs_epi16)(mag, QAM_ampb);

          if (mod_order > 6)
            ch_magc_v[i] = NRLB_MM(mulhrs_epi16)(mag, QAM_ampc);
        }
      }

      if (rho) {
        for (int atx = 0; atx < nb_layers; atx++) {
          NRLB_VI *rho_v = (NRLB_VI *)rho[aatx][atx];
          NRLB_VI *chF2_v = (NRLB_VI *)chFext[atx][0];
          for (uint32_t i = 0; i < nvec; i++)
            rho_v[i] = NRLB_OAI(cpx_mult_conj)(chF_v[i], chF2_v[i], output_shift);
        }
      }
    }

    // Remaining Rx antennas: accumulate (MRC)
    for (int aarx = 1; aarx < nb_rx_ant; aarx++) {
      NRLB_VI *rxF_v = (NRLB_VI *)rxFext[aarx];
      NRLB_VI *chF_v = (NRLB_VI *)chFext[aatx][aarx];

      for (uint32_t i = 0; i < nvec; i++) {
        NRLB_VI comp = NRLB_OAI(cpx_mult_conj)(chF_v[i], rxF_v[i], output_shift);
        rxComp_v[i] = NRLB_MM(add_epi16)(rxComp_v[i], comp);

        if (mod_order > 2) {
          NRLB_VI mag = NRLB_OAI(smadd)(chF_v[i], chF_v[i], output_shift);
          mag = NRLB_MM(packs_epi32)(mag, mag);
          mag = NRLB_MM(unpacklo_epi16)(mag, mag);
          ch_maga_v[i] = NRLB_MM(add_epi16)(ch_maga_v[i], NRLB_MM(mulhrs_epi16)(mag, QAM_ampa));

          if (mod_order > 4)
            ch_magb_v[i] = NRLB_MM(add_epi16)(ch_magb_v[i], NRLB_MM(mulhrs_epi16)(mag, QAM_ampb));

          if (mod_order > 6)
            ch_magc_v[i] = NRLB_MM(add_epi16)(ch_magc_v[i], NRLB_MM(mulhrs_epi16)(mag, QAM_ampc));
        }
      }

      if (rho) {
        for (int atx = 0; atx < nb_layers; atx++) {
          NRLB_VI *rho_v = (NRLB_VI *)rho[aatx][atx];
          NRLB_VI *chF2_v = (NRLB_VI *)chFext[atx][aarx];
          for (uint32_t i = 0; i < nvec; i++)
            rho_v[i] = NRLB_MM(adds_epi16)(rho_v[i], NRLB_OAI(cpx_mult_conj)(chF_v[i], chF2_v[i], output_shift));
        }
      }
    }
  }
}
