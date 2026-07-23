/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**********************************************************************
*
* FILENAME    :  sss_nr.c
*
* MODULE      :  Functions for secundary synchronisation signal
*
* DESCRIPTION :  generation of sss
*                3GPP TS 38.211 7.4.2.3 Secondary synchronisation signal
*
************************************************************************/

#include <stdio.h>
#include <assert.h>
#include <errno.h>

#include "PHY/defs_nr_UE.h"
#include "PHY/MODULATION/modulation_UE.h"
#include "executables/softmodem-common.h"
#include "PHY/NR_REFSIG/ss_pbch_nr.h"

#define DEFINE_VARIABLES_SSS_NR_H
#include "PHY/NR_REFSIG/sss_nr.h"
#undef DEFINE_VARIABLES_SSS_NR_H

/*******************************************************************
*
* NAME :         init_context_sss_nr
*
* PARAMETERS :   N_ID_2 : element 2 of physical layer cell identity
*                value : { 0, 1, 2 }
*
* RETURN :       generate binary sss sequence (this is a m-sequence)
*                d_sss is a third dimension array depending on
*                Cell identity elements:
*                - N_ID_1 : value from 0 to 335
*                - N_ID_2 : value from 0 to 2
*
* DESCRIPTION :  3GPP TS 38.211 7.4.2.3 Secundary synchronisation signal
*                Sequence generation
*
*********************************************************************/

#define INITIAL_SSS_NR    (7)

static void init_context_sss_nr(int amp, int N_ID_2, int16_t d_sss[N_ID_1_NUMBER][LENGTH_SSS_NR])
{
  int16_t x0[LENGTH_SSS_NR];
  int16_t x1[LENGTH_SSS_NR];
  int16_t dss_current;
  int m0, m1;

  const int x0_initial[INITIAL_SSS_NR] = { 1, 0, 0, 0, 0, 0, 0 };
  const int x1_initial[INITIAL_SSS_NR] = { 1, 0, 0, 0, 0, 0, 0 };
  for (int i = 0; i < INITIAL_SSS_NR; i++) {
    x0[i] = x0_initial[i];
    x1[i] = x1_initial[i];
  }
  for (int i = 0; i < (LENGTH_SSS_NR - INITIAL_SSS_NR); i++) {
    x0[i + 7] = (x0[i + 4] + x0[i]) % (2);
    x1[i + 7] = (x1[i + 1] + x1[i]) % (2);
  }

  for (int N_ID_1 = 0; N_ID_1 < N_ID_1_NUMBER; N_ID_1++) {
    m0 = 15 * (N_ID_1 / 112) + (5 * N_ID_2);
    m1 = N_ID_1 % 112;
    for (int n = 0; n < LENGTH_SSS_NR; n++) {
      dss_current = (1 - 2 * x0[(n + m0) % (LENGTH_SSS_NR)]) * (1 - 2 * x1[(n + m1) % (LENGTH_SSS_NR)]);
      /* Modulation of SSS is a BPSK TS 36.211 chapter 5.1.2 BPSK */
#if 1
      d_sss[N_ID_1][n] = dss_current; // * amp;
      (void)amp;
#else
      (void)amp;
      d_sss[N_ID_1][n] = dss_current((1U << SCALING_PSS_NR) - 1);
#endif
    }
  }

#if 0
  for (int i = 0; i < LENGTH_SSS_NR; i++) {
    printf("sss ref[%i] : %d %d \n", i, d_sss[0][0][i], d_sss[0][0][i]);
  }
#endif
}

// #define DEBUG_SSS_NR
// #define DEBUG_PLOT_SSS

/*******************************************************************
*
* NAME :         pss_ch_est
*
* PARAMETERS :   none
*
* RETURN :       none
*
* DESCRIPTION :  pss channel estimation
*
*********************************************************************/

static void pss_ch_est_nr(int nb_antennas_rx,
                          int nid2,
                          c16_t pss_ext[nb_antennas_rx][LENGTH_PSS_NR],
                          c16_t sss_ext[nb_antennas_rx][LENGTH_SSS_NR],
                          c16_t sss_comp[LENGTH_SSS_NR])
{
  int16_t pss[LENGTH_PSS_NR];
  generate_pss_nr(nid2, pss);
  for (int aarx = 0; aarx < nb_antennas_rx; aarx++) {
    c16_t *sss_ext2 = sss_ext[aarx];
    c16_t *pss_ext2 = pss_ext[aarx];
    for (int i = 0; i < LENGTH_PSS_NR; i++) {
      // This is H*(PSS) = R* \cdot PSS
      const c16_t tmp = (c16_t){pss_ext2[i].r * pss[i], -pss_ext2[i].i * pss[i]};
      const int32_t amp = squaredMod(tmp);
      const int shift = log2_approx(amp) / 2;
      // This is R(SSS) \cdot H*(PSS)
      const c16_t new_sss = c16mulShift(tmp, sss_ext2[i], shift);
      sss_comp[i] = c16add(sss_comp[i], new_sss);
    }
  }
}

/*******************************************************************
 *
 * NAME :         pss_sss_extract_nr
 *
 * PARAMETERS :   none
 *
 * RETURN :       none
 *
 * DESCRIPTION : it allows extracting sss from samples buffer
 *
 *********************************************************************
 */

static void pss_sss_extract_nr(
    nr_sss_params_t *params,
    c16_t pss_ext[params->nb_antennas_rx][LENGTH_PSS_NR],
    c16_t sss_ext[params->nb_antennas_rx][LENGTH_SSS_NR],
    const c16_t rxdataF[NR_N_SYMBOLS_SSB][params->nb_antennas_rx]
                       [params->ofdm_symbol_size]) // add flag to indicate extracting only PSS, only SSS, or both
{
  AssertFatal(params->nb_antennas_rx > 0, "nb antennas as sss_ext is not set to any value\n");
  const int pss_symbol = 0;
  const int sss_symbol =
      get_softmodem_params()->sl_mode == 0 ? (SSS_SYMBOL_NB - PSS_SYMBOL_NB) : (SSS0_SL_SYMBOL_NB - PSS0_SL_SYMBOL_NB);

  for (int aarx = 0; aarx < params->nb_antennas_rx; aarx++) {
    const c16_t *pss_rxF = rxdataF[pss_symbol][aarx];
    const c16_t *sss_rxF = rxdataF[sss_symbol][aarx];
    c16_t *pss_rxF_ext = pss_ext[aarx];
    c16_t *sss_rxF_ext = sss_ext[aarx];
    unsigned int k = params->first_carrier_offset + params->ssb_start_subcarrier
                     + ((get_softmodem_params()->sl_mode == 0) ? PSS_SSS_SUB_CARRIER_START : PSS_SSS_SUB_CARRIER_START_SL);

    for (int i=0; i < LENGTH_PSS_NR; i++) {
      if (k >= params->ofdm_symbol_size)
        k -= params->ofdm_symbol_size;
      pss_rxF_ext[i] = pss_rxF[k];
      sss_rxF_ext[i] = sss_rxF[k];
      k++;
    }
  }
}

static bool skip_pci(int Nid1, int Nid2, const uint16_t *exclude_nid_cells, int num_exclude_nid_cells)
{
  int current_pci = Nid2 + (3 * Nid1);
  for (int i = 0; i < num_exclude_nid_cells; i++) {
    if (current_pci == exclude_nid_cells[i]) {
      return true;
    }
  }
  return false;
}

/*******************************************************************
 *
 * NAME :         rx_sss_nr
 *
 * PARAMETERS :   none
 *
 * RETURN :       Set Nid_cell in ue context, return true if cell detected
 *
 * DESCRIPTION :  Determine element Nid1 of cell identity
 *                so Nid_cell in ue context is set according to Nid1 & Nid2
 *
 *********************************************************************/
sss_detection_result_t rx_sss_nr(nr_sss_params_t *params,
                                 pss_detection_result_t *pss,
                                 int target_Nid_cell,
                                 c16_t rxdataF[NR_N_SYMBOLS_SSB][params->nb_antennas_rx][params->ofdm_symbol_size])
{
  c16_t pss_ext[params->nb_antennas_rx][LENGTH_PSS_NR];
  c16_t sss_ext[params->nb_antennas_rx][LENGTH_SSS_NR];
  const int Nid2 = GET_NID2(pss->nid2);
  int16_t d_sss[N_ID_1_NUMBER][LENGTH_SSS_NR];
  init_context_sss_nr(AMP, Nid2, d_sss);
  pss_sss_extract_nr(params, pss_ext, sss_ext, rxdataF); /* subframe */

#ifdef DEBUG_PLOT_SSS
  write_output("rxsig0.m","rxs0",&ue->common_vars.rxdata[0][0],ue->frame_parms.samples_per_subframe,1,1);
  write_output("rxdataF0_pss.m","rxF0_pss",&ue->common_vars.rxdataF[0][0],frame_parms->ofdm_symbol_size,1,1);
  write_output("rxdataF0_sss.m","rxF0_sss",&ue->common_vars.rxdataF[0][(SSS_SYMBOL_NB-PSS_SYMBOL_NB)*frame_parms->ofdm_symbol_size],frame_parms->ofdm_symbol_size,1,1);
  write_output("pss_ext.m", "pss_ext", pss_ext, LENGTH_PSS_NR, 1, 1);
#endif

  // get conjugated channel estimate from PSS, H* = R* \cdot PSS
  // and do channel estimation and compensation based on PSS
  c16_t sss_comp[LENGTH_SSS_NR] = {};
  pss_ch_est_nr(params->nb_antennas_rx, pss->nid2, pss_ext, sss_ext, sss_comp);

  // now do the SSS detection based on the precomputed sequences in PHY/LTE_TRANSPORT/sss.h
  sss_detection_result_t res = {.metric = -INT_MAX};

  /* for phase evaluation, one uses an array of possible phase shifts */
  /* then a correlation is done between received signal with a shift pĥase and the reference signal */
  /* Computation of signal with shift phase is based on below formula */
  /* cosinus cos(x + y) = cos(x)cos(y) - sin(x)sin(y) */
  /* sinus   sin(x + y) = sin(x)cos(y) + cos(x)sin(y) */

  int Nid1_start = 0;
  int Nid1_end = N_ID_1_NUMBER;
  if (target_Nid_cell != -1) {
    if (GET_NID2(target_Nid_cell) != pss->nid2) {
      LOG_E(PHY, "calling sss detection with incoherent context %d, %d\n", pss->nid2, target_Nid_cell);
    } else {
      Nid1_start = GET_NID1(target_Nid_cell);
      Nid1_end = Nid1_start + 1;
    }
  }

  const int phase_to_try[] = {0, 2, -2, 4, -4, 6, -6, 8, -8, 10, -10, 12, -12, 14, -14};
  for (int idx = 0; idx < sizeofArray(phase_to_try); idx++) { // phase offset between PSS and SSS
    const c64_t rot =
        (c64_t){round(cos(M_PI / 3 / 15 * (phase_to_try[idx])) * 32767), round(sin(M_PI / 3 / 15 * (phase_to_try[idx])) * 32767)};
    for (int n1 = Nid1_start; n1 < Nid1_end; n1++) { // all possible Nid1 values
      // Skip this Nid1 if the corresponding PCI is in the exclusion list
      if (skip_pci(n1, Nid2, params->exclude_nid_cells, params->num_exclude_nid_cells))
        continue;
      int64_t metric = 0;
      int16_t *d = d_sss[n1];
      for (int i = 0; i < LENGTH_SSS_NR; i++) {
        // metric is only real part because sss is a pure real signal (imaginary is 0)
        metric += d[i] * (rot.r * sss_comp[i].r - rot.i * sss_comp[i].i);
      }
      metric >>= SCALING_METRIC_SSS_NR;
      // if the current metric is better than the last save it
      if (metric > res.metric) {
        res.metric = metric;
        res.nid_cell = Nid2 + 3 * n1;
        res.phase = idx;

#ifdef DEBUG_SSS_NR
        LOG_D(PHY,
              "(phase,Nid1) (%d,%d), metric_phase %ld metric %d, phase_max %d \n",
              res.phase,
              n1,
              metric,
              res.metric,
              res.phase);
#endif
      }
    }
    // we try progressively rotation between pss and sss
    // but pss and sss are in phase at emission
    // rotation means doppler variation, or very noisy pss detection
    if (res.metric >= SSS_METRIC_FLOOR_NR)
      break;
  }

  if (res.metric < SSS_METRIC_FLOOR_NR) {
    LOG_D(PHY,
          "Failed to detect SSS after PSS, metric of SSS %d, threshold to consider SSS valid %d, detected PCI: %d\n",
          res.metric,
          SSS_METRIC_FLOOR_NR,
          res.nid_cell);
    res.success = false;
    return res;
  } else
    res.success = true;

  int Nid1 = GET_NID1(res.nid_cell);
  LOG_D(PHY, "Nid2 %d Nid1 %d metric %d, phase_max %d \n", Nid2, Nid1, res.metric, res.phase);
  int16_t *d = d_sss[Nid1];
  c32_t sig_sum = {};
  for (int i = 0; i < LENGTH_SSS_NR; i++) {
    sig_sum.r += d[i] * sss_comp[i].r;
    sig_sum.i += d[i] * sss_comp[i].i;
  }
  double ffo_sss = atan2(sig_sum.i, sig_sum.r) / M_PI / 4.3;
  res.freq_offset = (int)(ffo_sss * params->subcarrier_spacing);

  double ffo_pss = (double)pss->freq_offset / params->subcarrier_spacing;
  LOG_D(NR_PHY,
        "SSS detected, PCI: %d, ffo_pss %f (%i Hz), ffo_sss %f (%i Hz),  ffo_pss+ffo_sss %f (%i Hz), nid1: %d, nid2: %d\n",
        res.nid_cell,
        ffo_pss,
        (int)(ffo_pss * params->subcarrier_spacing),
        ffo_sss,
        res.freq_offset,
        ffo_pss + ffo_sss,
        (int)((ffo_pss + ffo_sss) * params->subcarrier_spacing),
        Nid1,
        Nid2);
  return res;
}

void sl_generate_sss(SL_NR_UE_INIT_PARAMS_t *sl_init_params, uint16_t slss_id, uint16_t scaling)
{
  int i = 0;
  int m0, m1;
  int n_sl_id1, n_sl_id2;
  int16_t *sl_sss = sl_init_params->sl_sss[slss_id];
  int16_t *sl_sss_for_sync = sl_init_params->sl_sss_for_sync[slss_id];

  int16_t x0[SL_NR_SSS_SEQUENCE_LENGTH], x1[SL_NR_SSS_SEQUENCE_LENGTH];
  const int x_initial[7] = {1, 0, 0, 0, 0, 0, 0};

  n_sl_id1 = slss_id % 336;
  n_sl_id2 = slss_id / 336;

  LOG_D(PHY, "SIDELINK INIT: SSS Generation with N_SL_id1:%d N_SL_id2:%d\n", n_sl_id1, n_sl_id2);

#ifdef SL_DEBUG_INIT
  printf("SIDELINK: SSS Generation with slss_id:%d, N_SL_id1:%d, N_SL_id2:%d\n", slss_id, n_sl_id1, n_sl_id2);
#endif

  for (i = 0; i < 7; i++) {
    x0[i] = x_initial[i];
    x1[i] = x_initial[i];
  }

  for (i = 0; i < SL_NR_SSS_SEQUENCE_LENGTH - 7; i++) {
    x0[i + 7] = (x0[i + 4] + x0[i]) % 2;
    x1[i + 7] = (x1[i + 1] + x1[i]) % 2;
  }

  m0 = 15 * (n_sl_id1 / 112) + (5 * n_sl_id2);
  m1 = n_sl_id1 % 112;

  for (i = 0; i < SL_NR_SSS_SEQUENCE_LENGTH; i++) {
    sl_sss_for_sync[i] = (1 - 2 * x0[(i + m0) % SL_NR_SSS_SEQUENCE_LENGTH]) * (1 - 2 * x1[(i + m1) % SL_NR_SSS_SEQUENCE_LENGTH]);
    sl_sss[i] = sl_sss_for_sync[i] * scaling;

#ifdef SL_DEBUG_INIT_DATA
    printf("m0:%d, m1:%d, sl_sss_for_sync[%d]:%d, sl_sss[%d]:%d\n", m0, m1, i, sl_sss_for_sync[i], i, sl_sss[i]);
#endif
  }

#ifdef SL_DUMP_PSBCH_TX_SAMPLES
  LOG_M("sl_sss_seq.m", "sl_sss", (void *)sl_sss, SL_NR_SSS_SEQUENCE_LENGTH, 1, 0);
  LOG_M("sl_sss_forsync_seq.m", "sl_sss_for_sync", (void *)sl_sss_for_sync, SL_NR_SSS_SEQUENCE_LENGTH, 1, 0);
#endif
}
