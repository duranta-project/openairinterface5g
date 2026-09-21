/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Routines for initial UE synchronization procedure (PSS,SSS,PBCH and frame format detection)
 */
#include "PHY/NR_REFSIG/ss_pbch_nr.h"
#include "PHY/defs_nr_UE.h"
#include "PHY/MODULATION/modulation_UE.h"
#include "PHY/defs_nr_common.h"
#include "assertions.h"
#include "nr_transport_proto_ue.h"
#include "PHY/NR_UE_ESTIMATION/nr_estimation.h"
#include "SCHED_NR_UE/defs.h"
#include "common/utils/nr/nr_common.h"

#include <math.h>

#include "PHY/NR_REFSIG/pss_nr.h"
#include "PHY/NR_REFSIG/sss_nr.h"
#include "PHY/NR_REFSIG/nr_refsig.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "PHY/TOOLS/tools_defs.h"
#include "nr-uesoftmodem.h"
#include "nfapi/open-nFAPI/nfapi/public_inc/fapi_nr_ue_interface.h"

//#define DEBUG_INITIAL_SYNCH
#define DUMP_PBCH_CH_ESTIMATES 0

// structure used for multiple SSB detection
typedef struct NR_UE_SSB {
  uint i_ssb; // i_ssb between 0 and 7 (it corresponds to ssb_index only for Lmax=4,8)
  uint n_hf; // n_hf = 0,1 for Lmax =4 or n_hf = 0 for Lmax =8,64
  double metric; // metric to order SSB hypothesis
} NR_UE_SSB;

static int ssb_sort(const void *a, const void *b)
{
  return ((NR_UE_SSB *)b)->metric - ((NR_UE_SSB *)a)->metric;
}

static bool nr_pbch_detection(const UE_nr_rxtx_proc_t *proc,
                              const NR_DL_FRAME_PARMS *frame_parms,
                              int Nid_cell,
                              int pbch_initial_symbol,
                              int ssb_start_subcarrier,
                              int *half_frame_bit,
                              int *ssb_index,
                              int *symbol_offset,
                              fapiPbch_t *result,
                              const c16_t rxdataF[][frame_parms->nb_antennas_rx][frame_parms->ofdm_symbol_size])
{
  const int N_L = (frame_parms->Lmax == 4) ? 4 : 8;
  const int N_hf = (frame_parms->Lmax == 4) ? 2 : 1;
  NR_UE_SSB best_ssb[N_L * N_hf];
  NR_UE_SSB *current_ssb = best_ssb;
  // loops over possible pbch dmrs cases to retrieve best estimated i_ssb (and n_hf for Lmax=4) for multiple ssb detection
  for (int hf = 0; hf < N_hf; hf++) {
    for (int l = 0; l < N_L; l++) {
      // computing correlation between received DMRS symbols and transmitted sequence for current i_ssb and n_hf
      cd_t cumul = {0};
      for (int i = pbch_initial_symbol; i < pbch_initial_symbol + 3; i++) {
        c32_t meas = nr_pbch_dmrs_correlation(frame_parms,
                                              i,
                                              i - pbch_initial_symbol,
                                              Nid_cell,
                                              ssb_start_subcarrier,
                                              nr_gold_pbch(frame_parms->Lmax, Nid_cell, hf, l),
                                              rxdataF[i]);
        csum(cumul, cumul, meas);
      }
      *current_ssb = (NR_UE_SSB){.i_ssb = l, .n_hf = hf, .metric = squaredMod(cumul)};
      current_ssb++;
    }
  }
  qsort(best_ssb, N_L * N_hf, sizeof(NR_UE_SSB), ssb_sort);

  const int nb_ant = frame_parms->nb_antennas_rx;
  const int estimateSz = frame_parms->ofdm_symbol_size;
  for (NR_UE_SSB *ssb = best_ssb; ssb < best_ssb + N_L * N_hf; ssb++) {
    // computing channel estimation for selected best ssb
    int16_t pbch_e_rx[NR_POLAR_PBCH_E];

    uint8_t log2_maxh = 0;
    for (int i = pbch_initial_symbol; i < pbch_initial_symbol + 3; i++) {
      __attribute__((aligned(32))) c16_t dl_ch_estimates[nb_ant][estimateSz];
      for (int aarx = 0; aarx < nb_ant; aarx++) {
        nr_pbch_channel_estimation(frame_parms,
                                   NULL,
                                   dl_ch_estimates[aarx],
                                   proc,
                                   i - pbch_initial_symbol,
                                   ssb->i_ssb,
                                   ssb->n_hf,
                                   ssb_start_subcarrier,
                                   rxdataF[i][aarx],
                                   false,
                                   Nid_cell);
      }
      if (DUMP_PBCH_CH_ESTIMATES) {
        char varName[30] = "";
        snprintf(varName, sizeof(varName), "pbch_ch_estimates_symbol_%d", i);
        LOG_MM("pbch_ch_estimates", varName, dl_ch_estimates, nb_ant * estimateSz, 1, 1);
      }
      nr_generate_pbch_llr(NULL,
                           proc,
                           frame_parms,
                           i,
                           ssb->i_ssb,
                           Nid_cell,
                           ssb_start_subcarrier,
                           rxdataF[i],
                           dl_ch_estimates,
                           pbch_e_rx,
                           &log2_maxh);
    }

    if (0
        == nr_pbch_decode(NULL,
                          frame_parms,
                          proc,
                          ssb->i_ssb,
                          Nid_cell,
                          pbch_e_rx,
                          half_frame_bit,
                          ssb_index,
                          symbol_offset,
                          result)) {
      LOG_A(PHY, "Initial sync: pbch decoded sucessfully, ssb index %d\n", *ssb_index);
      return true;
    }
  }

  LOG_W(PHY, "Initial sync: pbch not decoded, ssb index %d\n", frame_parms->ssb_index);
  return false;
}

static void compensate_freq_offset(c16_t **x, const int nb_antennas_rx, const int x_len, const int offset, const int sampling_rate)
{
  double s_time = 1.0 / sampling_rate; // sampling time
  double off_angle = -2 * M_PI * s_time * (offset); // offset rotation angle compensation per sample

  for (int n = 0; n < x_len; n++) {
    for (int ar = 0; ar < nb_antennas_rx; ar++) {
      const double re = x[ar][n].r;
      const double im = x[ar][n].i;
      x[ar][n].r = (short)(round(re * cos(n * off_angle) - im * sin(n * off_angle)));
      x[ar][n].i = (short)(round(re * sin(n * off_angle) + im * cos(n * off_angle)));
    }
  }
}

/* Cyclic prefix of symbol `symb` of an SS/PBCH block, counted from the first symbol of the
   block. The first symbol of every half subframe carries a longer prefix (TS 38.211 5.3.1).
   Only the sidelink block can contain such a symbol: it always starts on the first symbol of
   a slot, whereas the downlink block never does. */
static int ssb_symbol_prefix_samples(const nr_ssb_search_params_t *params, int symb)
{
  if (params->sidelink && (symb % (7 << params->numerology_index)) == 0)
    return params->nb_prefix_samples0;
  return params->nb_prefix_samples;
}

/* Offset of the cyclic prefix of symbol `symb` from the first sample of the block. Called
   with symb == ssb_num_symbols, it gives the size of the whole block. */
static int ssb_symbol_offset(const nr_ssb_search_params_t *params, int symb)
{
  int offset = 0;
  for (int s = 0; s < symb; s++)
    offset += ssb_symbol_prefix_samples(params, s) + params->ofdm_symbol_size;
  return offset;
}

int nr_ssb_block_size(const NR_DL_FRAME_PARMS *fp, bool sidelink)
{
  const nr_ssb_search_params_t params = {.ofdm_symbol_size = fp->ofdm_symbol_size,
                                         .nb_prefix_samples = fp->nb_prefix_samples,
                                         .nb_prefix_samples0 = fp->nb_prefix_samples0,
                                         .numerology_index = fp->numerology_index,
                                         .sidelink = sidelink};
  return ssb_symbol_offset(&params, sidelink ? SL_N_SYMBOLS_SSB : NR_N_SYMBOLS_SSB);
}

/* rxdataF should be 16 bytes aligned */
static void generate_table(nr_ssb_search_params_t *params,
                           c16_t timeshift_symbol_rotation[params->ofdm_symbol_size],
                           c16_t symbol_rotation[224])
{
  init_timeshift_rotation(params->ofdm_symbol_size,
                          params->N_RB * NR_NB_SC_PER_RB,
                          params->nb_prefix_samples,
                          params->ofdm_offset_divisor,
                          timeshift_symbol_rotation);
  perform_symbol_rotation(params->symbols_per_slot * params->slots_per_frame / 10,
                          params->numerology_index,
                          params->carrier_freq,
                          symbol_rotation);
}

static void do_time_to_freq(nr_ssb_search_params_t *params, uint32_t sample_offset)
{
  c16_t timeshift_symbol_rotation[params->ofdm_symbol_size];
  c16_t symbol_rotation[224];
  generate_table(params, timeshift_symbol_rotation, symbol_rotation);

  c16_t(*rxdataF)[params->nb_antennas_rx][params->ofdm_symbol_size] =
      (c16_t(*)[params->nb_antennas_rx][params->ofdm_symbol_size])params->rxdataF;
  dft_size_idx_t dftsize = get_dft(params->ofdm_symbol_size);

  for (int symb = 0; symb < params->ssb_num_symbols; symb++) {
    unsigned int rx_offset = sample_offset + ssb_symbol_offset(params, symb) + ssb_symbol_prefix_samples(params, symb);
    // use OFDM symbol from within 1/8th of the CP to avoid ISI
    rx_offset -= params->nb_prefix_samples / params->ofdm_offset_divisor;
    for (unsigned char aa = 0; aa < params->nb_antennas_rx; aa++) {
      c16_t *rxF = rxdataF[symb][aa];
      // OFDM Demod
      dft(dftsize, (int16_t *)&params->rxdata[aa][rx_offset], (int16_t *)rxF, 1);
      // FFT-shift
      fftshift_inplace(rxF, params->N_RB * NR_NB_SC_PER_RB, params->ofdm_symbol_size);
      // Phase compensation
      apply_nr_rotation_symbol_fftshifted_RX(params->symbols_per_slot,
                                             params->slots_per_subframe,
                                             timeshift_symbol_rotation,
                                             rxF,
                                             symbol_rotation,
                                             params->N_RB,
                                             0,
                                             symb);
    }
  }
}

/*
 * Common SSB search function used by both initial sync and neighbor cell search
 */
bool nr_search_ssb_common(nr_ssb_search_params_t *params)
{
  const uint32_t pssTime_sz = params->ofdm_symbol_size;
  c16_t(*pssTime)[pssTime_sz] = (c16_t(*)[pssTime_sz])params->pssTime;

  // Perform PSS search
  pss_search_t p_pss = (pss_search_t){.rxdata = params->rxdata,
                                      .nb_antennas_rx = params->nb_antennas_rx,
                                      .rxdata_length = params->rxdata_size,
                                      .ofdm_symbol_size = params->ofdm_symbol_size,
                                      .nb_prefix_samples = params->nb_prefix_samples,
                                      .subcarrier_spacing = params->subcarrier_spacing,
                                      .fo_flag = params->fo_flag,
                                      .sidelink = params->sidelink,
                                      .target_Nid_cell = params->target_nid_cell,
                                      .pssTime = (c16_t *)pssTime};
  nr_pss_info_t pss_info = pss_search_time_nr(&p_pss);

  /* Symbols of the block the correlation can have peaked on. The downlink block carries a
     single PSS on its first symbol. The sidelink block carries the same PSS on its symbols 1
     and 2, so the correlation peaks on both of them and the block may start one symbol
     earlier than the peak alone suggests: both hypotheses have to be tried. */
  const int pss_symbol[] = {params->sidelink ? PSS0_SL_SYMBOL_NB : 0, PSS1_SL_SYMBOL_NB};
  const int num_pss_symbols = params->sidelink ? 2 : 1;
  const int ssb_size = ssb_symbol_offset(params, params->ssb_num_symbols);

  // This is the frequency offset that will be applied in the compensation,
  // and it takes into account the values already applied previously during the loop.
  int f_off_to_comp = 0;

  for (int p = 0; p < nr_num_pss_sequences(params->sidelink); p++) {
    pss_detection_result_t *pss_res = &pss_info.pss_elem_info[p];
    if (!pss_res->success)
      continue;

    // Apply frequency offset compensation if requested
    if (params->apply_freq_offset && pss_res->freq_offset != 0) {
      f_off_to_comp += pss_res->freq_offset;
      compensate_freq_offset(params->rxdata, params->nb_antennas_rx, params->rxdata_size, f_off_to_comp, params->sampling_rate);
      f_off_to_comp *= -1;
    }

    for (int h = 0; h < num_pss_symbols; h++) {
      // The correlation peaks on the body of the PSS symbol, i.e. after its cyclic prefix.
      const int ssb_time_offset =
          pss_res->pos - ssb_symbol_offset(params, pss_symbol[h]) - ssb_symbol_prefix_samples(params, pss_symbol[h]);

#ifdef DEBUG_INITIAL_SYNCH
      LOG_I(PHY,
            "Initial sync : Estimated PSS position %d, Nid2 %d, ssb time offset %d\n",
            pss_res->pos,
            pss_res->nid2,
            ssb_time_offset);
#endif

      // Check that the whole block fits within the buffer
      if (ssb_time_offset < 0 || ssb_time_offset + ssb_size > params->rxdata_size) {
        LOG_D(PHY,
              "SSB does not fit in the buffer (sync_pos %d, ssb_time_offset %d, buffer_size %d)\n",
              pss_res->pos,
              ssb_time_offset,
              params->rxdata_size);
        continue;
      }

      // Extract the block symbols to frequency domain
      // Downlink symbol ordering: 0=PSS, 1=PBCH, 2=SSS, 3=PBCH
      // Sidelink symbol ordering: 0=PSBCH, 1,2=PSS, 3,4=SSS, 5..12=PSBCH
      do_time_to_freq(params, ssb_time_offset);

      // Perform SSS detection
      nr_sss_params_t p_sss = (nr_sss_params_t){.nb_antennas_rx = params->nb_antennas_rx,
                                                .samples_per_slot_wCP = params->samples_per_slot_wCP,
                                                .ofdm_symbol_size = params->ofdm_symbol_size,
                                                .ssb_start_subcarrier = params->ssb_start_subcarrier,
                                                .subcarrier_spacing = params->subcarrier_spacing,
                                                .sidelink = params->sidelink,
                                                .exclude_nid_cells = params->exclude_nid_cells,
                                                .num_exclude_nid_cells = params->num_exclude_nid_cells};

      c16_t(*rxdataF)[params->nb_antennas_rx][params->ofdm_symbol_size] =
          (c16_t(*)[params->nb_antennas_rx][params->ofdm_symbol_size])params->rxdataF;
      params->sss_res = rx_sss_nr(&p_sss, pss_res, -1, rxdataF);

      if (!params->sss_res.success || params->sss_res.nid_cell < 0)
        continue;

      params->pss_res = *pss_res;
      params->ssb_time_offset = ssb_time_offset;
      if (params->validate_candidate && !params->validate_candidate(params->validate_ctx, params))
        continue;
      return true;
    }
  }

  return false;
}

/* Decode the (P)SBCH of a candidate block whose PSS and SSS have just been detected. This is
   what confirms the candidate: a correlation peak landing on the wrong symbol of a real
   block still passes the SSS detection, because the sidelink block repeats both PSS and SSS
   on two consecutive symbols. */
static bool nr_validate_ssb_candidate(void *ctx, const nr_ssb_search_params_t *params)
{
  nr_ue_ssb_scan_t *ssbInfo = (nr_ue_ssb_scan_t *)ctx;
  const NR_DL_FRAME_PARMS *fp = ssbInfo->fp;
  const c16_t(*rxdataF)[fp->nb_antennas_rx][fp->ofdm_symbol_size] =
      (const c16_t(*)[fp->nb_antennas_rx][fp->ofdm_symbol_size])params->rxdataF;

  ssbInfo->nidCell = params->sss_res.nid_cell;
  ssbInfo->ssbOffset = params->ssb_time_offset;

  if (ssbInfo->sidelink)
    return sl_nr_psbch_detection(ssbInfo, rxdataF);

  if (!nr_pbch_detection(ssbInfo->proc,
                         fp,
                         ssbInfo->nidCell,
                         1, // start pbch detection at first symbol after pss
                         ssbInfo->gscnInfo.ssbFirstSC,
                         &ssbInfo->halfFrameBit,
                         &ssbInfo->ssbIndex,
                         &ssbInfo->symbolOffset,
                         &ssbInfo->pbchResult,
                         rxdataF))
    return false;

  uint32_t rsrp_avg = nr_ue_calculate_ssb_rsrp(fp, rxdataF[2], ssbInfo->gscnInfo.ssbFirstSC);
  int rsrp_db_per_re = 10 * log10(rsrp_avg);
  ssbInfo->adjust_rxgain = TARGET_RX_POWER - rsrp_db_per_re;
  LOG_I(PHY, "pbch rx ok. rsrp:%d dB/RE, adjust_rxgain:%d dB\n", rsrp_db_per_re, ssbInfo->adjust_rxgain);
  return true;
}

static void nr_scan_ssb(void *arg)
{
  /*   Initial synchronisation
   *
   *                          scan window = 1 subframe + one SS/PBCH block
   *     <--------------------------------------------------------------------------->
   *     -----------------------------------------------------------------------------
   *     |                                 Received UE data buffer                    |
   *     ----------------------------------------------------------------------------
   *                     --------------------------
   *     <-------------->| pss | pbch | sss | pbch |          (downlink)
   *                     --------------------------
   *      ssb_time_offset        SS/PBCH block
   *
   *                     ------------------------------------------
   *     <-------------->|psbch|pss|pss|sss|sss|psbch sym5-sym12|  (sidelink)
   *                     ------------------------------------------
   *      ssb_time_offset            SL-SSB block
   */

  nr_ue_ssb_scan_t *ssbInfo = (nr_ue_ssb_scan_t *)arg;
  c16_t **rxdata = ssbInfo->rxdata;
  const NR_DL_FRAME_PARMS *fp = ssbInfo->fp;
  const bool sl = ssbInfo->sidelink;
  const int num_symbols = sl ? SL_N_SYMBOLS_SSB : NR_N_SYMBOLS_SSB;

  // Generate PSS time signal for this GSCN.
  __attribute__((aligned(32))) c16_t pssTime[NUMBER_PSS_SEQUENCE][fp->ofdm_symbol_size];
  for (int nid2 = 0; nid2 < nr_num_pss_sequences(sl); nid2++)
    generate_pss_nr_time(fp->ofdm_symbol_size, fp->first_carrier_offset, nid2, ssbInfo->gscnInfo.ssbFirstSC, sl, pssTime[nid2]);

  __attribute__((aligned(32))) c16_t rxdataF[num_symbols][fp->nb_antennas_rx][fp->ofdm_symbol_size];

  if (ssbInfo->freqOffset)
    compensate_freq_offset(rxdata, fp->nb_antennas_rx, ssbInfo->rxdata_sz, ssbInfo->freqOffset, fp->samples_per_subframe * 1000);

  nr_ssb_search_params_t search_params = {
      .carrier_freq = sl ? fp->sl_CarrierFreq : fp->dl_CarrierFreq,
      .sampling_rate = fp->samples_per_subframe * 1000,
      .slots_per_frame = fp->slots_per_frame,
      .slots_per_subframe = fp->slots_per_subframe,
      .numerology_index = fp->numerology_index,
      .ofdm_symbol_size = fp->ofdm_symbol_size,
      .ofdm_offset_divisor = fp->ofdm_offset_divisor,
      .nb_antennas_rx = fp->nb_antennas_rx,
      .symbols_per_slot = fp->symbols_per_slot,
      .N_RB = sl ? fp->N_RB_SL : fp->N_RB_DL,
      .ssb_num_symbols = num_symbols,
      .sidelink = sl,
      .rxdata_size = ssbInfo->rxdata_sz,
      .rxdata = rxdata,
      .nb_prefix_samples = fp->nb_prefix_samples,
      .nb_prefix_samples0 = fp->nb_prefix_samples0,
      .ssb_start_subcarrier = ssbInfo->gscnInfo.ssbFirstSC,
      .subcarrier_spacing = fp->subcarrier_spacing,
      .samples_per_slot_wCP = fp->samples_per_slot_wCP,
      .target_nid_cell = ssbInfo->targetNidCell,
      .exclude_nid_cells = NULL, // No exclusion for initial sync
      .num_exclude_nid_cells = 0,
      .apply_freq_offset = ssbInfo->foFlag,
      .fo_flag = ssbInfo->foFlag,
      .rxdataF = rxdataF,
      .pssTime = pssTime,
      .validate_candidate = nr_validate_ssb_candidate,
      .validate_ctx = ssbInfo,
  };

  ssbInfo->syncRes.cell_detected = nr_search_ssb_common(&search_params);

  ssbInfo->pssCorrAvgPower = search_params.pss_res.avg;
  ssbInfo->pssCorrPeakPower = search_params.pss_res.peak;
  ssbInfo->freqOffset += search_params.pss_res.freq_offset + search_params.sss_res.freq_offset;

  completed_task_ans(ssbInfo->ans);
}

nr_initial_sync_t nr_initial_sync(UE_nr_rxtx_proc_t *proc,
                                  PHY_VARS_NR_UE *ue,
                                  int input_sz,
                                  c16_t **input,
                                  nr_gscn_info_t gscnInfo[MAX_GSCN_BAND],
                                  int numGscn)
{
  NR_DL_FRAME_PARMS *fp = nrue_frame_parms(ue);
  const bool sl = ue->sl_mode == SL_MODE2_SUPPORTED;

  /* The scan window is one subframe long, plus one block so that a block starting anywhere
     inside that subframe is entirely covered. The caller slides the window by one subframe
     between two calls, so every sample is scanned exactly once. */
  const int scan_sz = fp->samples_per_subframe + nr_ssb_block_size(fp, sl);
  AssertFatal(scan_sz <= input_sz, "sync buffer of %d samples is too small, need %d\n", input_sz, scan_sz);

  // Perform SSB scanning in parallel. One GSCN per thread.
  LOG_I(NR_PHY,
        "Starting cell search with center freq: %ld, bandwidth: %d. Scanning for %d number of GSCN.\n",
        sl ? fp->sl_CarrierFreq : fp->dl_CarrierFreq,
        sl ? fp->N_RB_SL : fp->N_RB_DL,
        numGscn);
  DevAssert(numGscn);
  task_ans_t ans;
  init_task_ans(&ans, numGscn);
  nr_ue_ssb_scan_t ssb_info[numGscn];
  for (int s = 0; s < numGscn; s++) {
    nr_ue_ssb_scan_t *ssbInfo = &ssb_info[s];
    *ssbInfo = (nr_ue_ssb_scan_t){.gscnInfo = gscnInfo[s],
                                  .fp = fp,
                                  .ue = ue,
                                  .sidelink = sl,
                                  .proc = proc,
                                  .syncRes.cell_detected = false,
                                  .foFlag = ue->UE_fo_compensation,
                                  .freqOffset = ue->initial_fo,
                                  .targetNidCell = ue->target_Nid_cell};
    /* Work on a private copy: the frequency offset compensation modifies the samples, and
       the caller keeps the tail of the window for the next scan. */
    ssbInfo->rxdata = malloc16_clear(fp->nb_antennas_rx * sizeof(c16_t *));
    for (int ant = 0; ant < fp->nb_antennas_rx; ant++) {
      ssbInfo->rxdata[ant] = malloc16(sizeof(c16_t) * scan_sz);
      memcpy(ssbInfo->rxdata[ant], input[ant], sizeof(c16_t) * scan_sz);
      ssbInfo->rxdata_sz = scan_sz;
    }
    LOG_I(NR_PHY,
          "Scanning GSCN: %d, with SSB offset: %d, SSB Freq: %lf\n",
          ssbInfo->gscnInfo.gscn,
          ssbInfo->gscnInfo.ssbFirstSC,
          ssbInfo->gscnInfo.ssRef);
    ssbInfo->ans = &ans;
    task_t t = {.func = nr_scan_ssb, .args = ssbInfo};
    pushTpool(&get_nrUE_params()->Tpool, t);
  }

  // Collect the scan results
  nr_ue_ssb_scan_t *res = NULL;
  join_task_ans(&ans);
  for (int i = 0; i < numGscn; i++) {
    nr_ue_ssb_scan_t *ssbInfo = &ssb_info[i];
    if (ssbInfo->syncRes.cell_detected) {
      LOG_I(NR_PHY,
            "Cell Detected with GSCN: %d, SSB SC offset: %d, SSB Ref: %lf, PSS Corr peak: %d dB, PSS Corr Average: %d\n",
            ssbInfo->gscnInfo.gscn,
            ssbInfo->gscnInfo.ssbFirstSC,
            ssbInfo->gscnInfo.ssRef,
            ssbInfo->pssCorrPeakPower,
            ssbInfo->pssCorrAvgPower);
      // take the first cell detected
      if (!res)
        res = ssbInfo;
    }
    for (int ant = 0; ant < fp->nb_antennas_rx; ant++) {
      free(ssbInfo->rxdata[ant]);
    }
    free(ssbInfo->rxdata);
    ssbInfo->rxdata = NULL;
  }

  // Set globals based on detected cell
  if (res) {
    ue->common_vars.freq_offset = res->freqOffset;
    ue->adjust_rxgain = res->adjust_rxgain;
    if (sl) {
      sl_nr_apply_slss_sync(ue, proc, res);
    } else {
      fp->Nid_cell = res->nidCell;
      fp->ssb_start_subcarrier = res->gscnInfo.ssbFirstSC;
      fp->half_frame_bit = res->halfFrameBit;
      fp->ssb_index = res->ssbIndex;
      ue->symbol_offset = res->symbolOffset;
    }
  }

  // In initial sync, we indicate PBCH to MAC after the scan is complete.
  if (!sl && ue->if_inst && ue->if_inst->dl_indication) {
    fapi_nr_rx_indication_t rx_ind;
    rx_ind.number_pdus = 0;
    nr_fill_rx_indication(&rx_ind, FAPI_NR_RX_PDU_TYPE_SSB, ue, 0, 0, NULL, proc, res ? (void *)&res->pbchResult : NULL);
    nr_downlink_indication_t dl_indication = (nr_downlink_indication_t){
        .gNB_index = proc->gNB_id,
        .module_id = ue->Mod_id,
        .cc_id = ue->CC_id,
        .hfn = proc->hfn_rx,
        .frame = proc->frame_rx,
        .slot = proc->nr_slot_rx,
        .rx_ind = &rx_ind,
    };
    ue->if_inst->dl_indication(&dl_indication);
  }

  LOG_D(PHY, "nr_initial sync ue N_RB %d\n", sl ? fp->N_RB_SL : fp->N_RB_DL);

  if (res) {
    /* The block starts at symbol res->symbolOffset of the frame, known from the decoded
       (P)SBCH. Turn it into the subframe the block belongs to and the position of the start
       of that subframe inside the scanned buffer, so that the caller can align its sample
       stream to a subframe boundary. */
    const int symbols_per_subframe = fp->symbols_per_slot * fp->slots_per_subframe;
    const int symbol_in_subframe = res->symbolOffset % symbols_per_subframe;
    // every 7*(1<<mu) symbols there is a longer prefix (38.211 5.3.1)
    const int long_prefix_period = 7 << fp->numerology_index;
    // number of symbols with the longer prefix before symbol_in_subframe
    const int n_symb_prefix0 = (symbol_in_subframe + long_prefix_period - 1) / long_prefix_period;
    const int sync_pos_subframe = n_symb_prefix0 * (fp->ofdm_symbol_size + fp->nb_prefix_samples0)
                                  + (symbol_in_subframe - n_symb_prefix0) * (fp->ofdm_symbol_size + fp->nb_prefix_samples);

    res->syncRes.rx_offset = res->ssbOffset - sync_pos_subframe;
    res->syncRes.sync_subframe = res->symbolOffset / symbols_per_subframe;
    if (res->syncRes.rx_offset < 0) {
      // That subframe started before the scan window: align to the next subframe instead.
      res->syncRes.rx_offset += fp->samples_per_subframe;
      res->syncRes.sync_subframe++;
    }

    LOG_I(PHY,
          "[UE%d] In synch, subframe %u, rx_offset %d samples\n",
          ue->Mod_id,
          res->syncRes.sync_subframe,
          res->syncRes.rx_offset);
    LOG_I(PHY, "[UE %d] Measured Carrier Frequency offset %d Hz\n", ue->Mod_id, res->freqOffset);
    LOG_A(PHY, "Initial sync successful, %s: %d\n", sl ? "SLSS id" : "PCI", res->nidCell);
    return res->syncRes;
  } else {
#ifdef DEBUG_INITIAL_SYNC
    LOG_I(PHY,"[UE%d] Initial sync : PBCH not ok\n",ue->Mod_id);
    LOG_I(PHY, "[UE%d] Initial sync : Estimated PSS position %d, Nid2 %d\n", ue->Mod_id, sync_pos, ue->common_vars.nid2);
    LOG_I(PHY,"[UE%d] Initial sync : Estimated Nid_cell %d, Frame_type %d\n",ue->Mod_id,
          fp->Nid_cell,fp->frame_type);
    LOG_I(PHY, "[UE%d] Initial sync failed : Estimated power: %d dB\n", ue->Mod_id, ue->measurements.rx_power_avg_dB[0]);
#endif
    // gain control
    // we are not synched, so we cannot use rssi measurement (which is based on channel estimates)
    int rx_power = 0;

    // do a measurement on the best guess of the PSS
    // for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++)
    //  rx_power += signal_energy(&ue->common_vars.rxdata[aarx][sync_pos2],
    //			frame_parms->ofdm_symbol_size+frame_parms->nb_prefix_samples);

    /*
    // do a measurement on the full frame
    for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++)
    rx_power += signal_energy(&ue->common_vars.rxdata[aarx][0],
    frame_parms->samples_per_subframe*10);
    */

    // we might add a low-pass filter here later
    ue->measurements.rx_power_avg[0] = rx_power / fp->nb_antennas_rx;
    ue->measurements.rx_power_avg_dB[0] = dB_fixed(ue->measurements.rx_power_avg[0]);
    return (nr_initial_sync_t){.cell_detected = false};
  }
}
