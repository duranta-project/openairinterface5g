/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Routines for initial UE synchronization procedure (PSS,SSS,PBCH and frame format detection)
 */
#include "PHY/defs_nr_UE.h"
#include "PHY/MODULATION/modulation_UE.h"
#include "nr_transport_proto_ue.h"
#include "PHY/NR_UE_ESTIMATION/nr_estimation.h"
#include "SCHED_NR_UE/defs.h"
#include "common/utils/nr/nr_common.h"

#include <math.h>

#include "PHY/NR_REFSIG/pss_nr.h"
#include "PHY/NR_REFSIG/sss_nr.h"
#include "PHY/NR_REFSIG/nr_refsig.h"
#include "PHY/TOOLS/tools_defs.h"
#include "nr_pss_cfo_search.h"
#include "nr-uesoftmodem.h"

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
                              nr_initial_sync_t *sync_res,
                              const c16_t rxdataF[NR_N_SYMBOLS_SSB][frame_parms->nb_antennas_rx][frame_parms->ofdm_symbol_size])
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
  sync_res->trace.pbch_attempted = true;
  sync_res->trace.pbch_success = false;
  sync_res->trace.pbch_dmrs_best_metric = best_ssb[0].metric;
  sync_res->trace.pbch_dmrs_second_metric = N_L * N_hf > 1 ? best_ssb[1].metric : 0;
  sync_res->trace.pbch_decode_attempts = 0;
  sync_res->trace.failure_reason = NR_SYNC_FAILURE_PBCH;

  const int nb_ant = frame_parms->nb_antennas_rx;
  const int estimateSz = frame_parms->ofdm_symbol_size;
  for (NR_UE_SSB *ssb = best_ssb; ssb < best_ssb + N_L * N_hf; ssb++) {
    sync_res->trace.pbch_decode_attempts++;
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
      sync_res->trace.pbch_success = true;
      sync_res->trace.failure_reason = NR_SYNC_FAILURE_NONE;
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

/* rxdataF should be 16 bytes aligned */
static void generate_table(nr_ssb_search_params_t *params,
                           c16_t timeshift_symbol_rotation[params->ofdm_symbol_size],
                           c16_t symbol_rotation[224])
{
  init_timeshift_rotation(params->ofdm_symbol_size,
                          params->nb_prefix_samples,
                          params->ofdm_offset_divisor,
                          timeshift_symbol_rotation);
  perform_symbol_rotation(params->symbols_per_slot * params->slots_per_frame / 10,
                          params->numerology_index,
                          params->dl_CarrierFreq,
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

  for (int symb = 0; symb < NR_N_SYMBOLS_SSB; symb++) {
    // For Sidelink 16 frames worth of samples is processed to find SSB, for 5G-NR 2.
    unsigned int rx_offset = sample_offset + params->nb_prefix_samples;
    rx_offset += symb * (params->nb_prefix_samples + params->ofdm_symbol_size);
    // use OFDM symbol from within 1/8th of the CP to avoid ISI
    rx_offset -= params->nb_prefix_samples / params->ofdm_offset_divisor;
    for (unsigned char aa = 0; aa < params->nb_antennas_rx; aa++) {
      c16_t *rxF = rxdataF[symb][aa];
      dft(dftsize, (int16_t *)&params->rxdata[aa][rx_offset], (int16_t *)rxF, 1);
      apply_nr_rotation_symbol_RX(params->symbols_per_slot,
                                  params->slots_per_subframe,
                                  timeshift_symbol_rotation,
                                  params->first_carrier_offset,
                                  rxF,
                                  symbol_rotation,
                                  params->N_RB_DL,
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
  params->failure_reason = NR_SYNC_FAILURE_PSS;
  const uint32_t pssTime_sz = params->ofdm_symbol_size;
  c16_t(*pssTime)[pssTime_sz] = (c16_t(*)[pssTime_sz])params->pssTime;

  // Perform PSS search
  pss_search_t p_pss = (pss_search_t){.rxdata = params->rxdata,
                                      .nb_antennas_rx = params->nb_antennas_rx,
                                      .rxdata_length = params->rxdata_size,
                                      .ofdm_symbol_size = params->ofdm_symbol_size,
                                      .subcarrier_spacing = params->subcarrier_spacing,
                                      .fo_flag = params->fo_flag,
                                      .target_Nid_cell = params->target_nid_cell,
                                      .pssTime = (c16_t *)pssTime};
  if (params->pss_cfo_search.enabled && params->target_nid_cell == -1) {
    if (!params->apply_freq_offset) {
      LOG_E(PHY, "CFO-PSS search requires frequency-offset compensation\n");
      params->pss_res = (pss_detection_result_t){.success = false, .pos = -1};
      return false;
    }

    const c16_t *rxdata[params->nb_antennas_rx];
    const c16_t *pss_references[NUMBER_PSS_SEQUENCE];
    for (int antenna = 0; antenna < params->nb_antennas_rx; ++antenna)
      rxdata[antenna] = params->rxdata[antenna];
    for (int nid2 = 0; nid2 < NUMBER_PSS_SEQUENCE; ++nid2)
      pss_references[nid2] = pssTime[nid2];

    const nr_pss_cfo_coarse_search_t stage1_search = {
        .rxdata = rxdata,
        .nb_antennas_rx = params->nb_antennas_rx,
        .rxdata_length = params->rxdata_size,
        .ofdm_symbol_size = params->ofdm_symbol_size,
        .pss_time = {pss_references[0], pss_references[1], pss_references[2]},
        .sampling_rate_hz = params->sampling_rate,
    };
    const nr_pss_cfo_search_result_t stage1 = nr_pss_cfo_search_stage1(&stage1_search, &params->pss_cfo_search);
    params->pss_cfo_search_used = true;
    params->pss_cfo_coarse_hz = stage1.coarse_cfo_hz;
    params->pss_cfo_fine_hz = stage1.fine_cfo_hz;
    params->pss_second_timing_peak_raw = stage1.second_timing_peak_raw;
    params->pss_cfo_coarse_bins = stage1.coarse_bins_evaluated;
    params->pss_cfo_fine_bins = stage1.fine_bins_evaluated;
    params->pss_cfo_diagnostic_passes = stage1.diagnostic_passes_evaluated;
    params->pss_res = stage1.pss;
    if (stage1.status != NR_PSS_CFO_SEARCH_OK) {
      LOG_D(PHY, "CFO-PSS Stage 1 finished with status %s\n", nr_pss_cfo_search_status_name(stage1.status));
      return false;
    }
    LOG_D(PHY,
          "CFO-PSS Stage 1 selected coarse %d Hz, fine %d Hz, total %d Hz, NID2 %d, position %d\n",
          stage1.coarse_cfo_hz,
          stage1.fine_cfo_hz,
          stage1.pss.freq_offset,
          stage1.pss.nid2,
          stage1.pss.pos);
  } else {
    if (params->pss_cfo_search.enabled)
      LOG_D(PHY, "Using original PSS detector for targeted cell search\n");
    params->pss_res = pss_search_time_nr(&p_pss);
  }

  if (!params->pss_res.success)
    return false;

  const int ssb_time_offset = params->pss_res.pos - params->nb_prefix_samples;

#ifdef DEBUG_INITIAL_SYNCH
  LOG_I(PHY, "Initial sync : Estimated PSS position %d, Nid2 %d, ssb offset %d\n", sync_pos, nid2, ssb_offset);
#endif

  // Check that SSB fits within buffer
  if (ssb_time_offset + NR_N_SYMBOLS_SSB * (params->ofdm_symbol_size + params->nb_prefix_samples) >= params->rxdata_size) {
    params->failure_reason = NR_SYNC_FAILURE_SSB_BOUNDARY;
    LOG_D(PHY,
          "SSB extends beyond buffer boundary (sync_pos %d, ssb_offset %d, buffer_size %d)\n",
          params->pss_res.pos,
          ssb_time_offset,
          params->rxdata_size);
    return false;
  }

  // Apply frequency offset compensation if requested
  if (params->apply_freq_offset && params->pss_res.freq_offset != 0) {
    compensate_freq_offset(params->rxdata, params->nb_antennas_rx, params->rxdata_size, params->pss_res.freq_offset, params->sampling_rate);
  }

  // Extract SSB symbols to frequency domain
  // Symbol ordering: 0=PSS, 1=PBCH, 2=SSS, 3=PBCH
  do_time_to_freq(params, ssb_time_offset);

  // Perform SSS detection
  nr_sss_params_t p_sss = (nr_sss_params_t){.nb_antennas_rx = params->nb_antennas_rx,
                                            .samples_per_slot_wCP = params->samples_per_slot_wCP,
                                            .ofdm_symbol_size = params->ofdm_symbol_size,
                                            .first_carrier_offset = params->first_carrier_offset,
                                            .ssb_start_subcarrier = params->ssb_start_subcarrier,
                                            .subcarrier_spacing = params->subcarrier_spacing};

  c16_t(*rxdataF)[params->nb_antennas_rx][params->ofdm_symbol_size] =
      (c16_t(*)[params->nb_antennas_rx][params->ofdm_symbol_size])params->rxdataF;
  params->sss_res = rx_sss_nr(&p_sss, &params->pss_res, -1, rxdataF);

  if (!params->sss_res.success || params->sss_res.nid_cell < 0) {
    params->failure_reason = NR_SYNC_FAILURE_SSS;
    return false;
  }

  // Check if we should exclude the serving cell
  if (params->exclude_nid_cell >= 0 && params->sss_res.nid_cell == params->exclude_nid_cell) {
    params->failure_reason = NR_SYNC_FAILURE_EXCLUDED_PCI;
    return false;
  }

  params->failure_reason = NR_SYNC_FAILURE_NONE;
  return true;
}

static void copy_search_trace(nr_initial_sync_t *sync_res, const nr_ssb_search_params_t *search_params, int initial_freq_offset)
{
  sync_res->trace.failure_reason = search_params->failure_reason;
  sync_res->trace.pss_success = search_params->pss_res.success;
  sync_res->trace.pss_nid2 = search_params->pss_res.nid2;
  sync_res->trace.pss_position = search_params->pss_res.pos;
  sync_res->trace.pss_peak_db = search_params->pss_res.peak;
  sync_res->trace.pss_avg_db = search_params->pss_res.avg;
  sync_res->trace.pss_peak_raw = search_params->pss_res.peak_raw;
  sync_res->trace.pss_avg_raw = search_params->pss_res.avg_raw;
  sync_res->trace.pss_second_sequence_peak_raw = search_params->pss_res.second_sequence_peak_raw;
  sync_res->trace.pss_cfo_search_used = search_params->pss_cfo_search_used;
  sync_res->trace.pss_cfo_coarse_hz = search_params->pss_cfo_coarse_hz;
  sync_res->trace.pss_cfo_fine_hz = search_params->pss_cfo_fine_hz;
  sync_res->trace.pss_second_timing_peak_raw = search_params->pss_second_timing_peak_raw;
  sync_res->trace.pss_cfo_coarse_bins = search_params->pss_cfo_coarse_bins;
  sync_res->trace.pss_cfo_fine_bins = search_params->pss_cfo_fine_bins;
  sync_res->trace.pss_cfo_diagnostic_passes = search_params->pss_cfo_diagnostic_passes;
  sync_res->trace.pss_freq_offset = search_params->pss_res.freq_offset;
  sync_res->trace.sss_success = search_params->sss_res.success;
  sync_res->trace.sss_nid_cell = search_params->sss_res.nid_cell;
  sync_res->trace.sss_metric = search_params->sss_res.metric;
  sync_res->trace.sss_second_metric = search_params->sss_res.second_metric;
  sync_res->trace.sss_phase = search_params->sss_res.phase;
  sync_res->trace.sss_freq_offset = search_params->sss_res.freq_offset;
  sync_res->trace.initial_freq_offset = initial_freq_offset;
  sync_res->trace.total_freq_offset = initial_freq_offset + search_params->pss_res.freq_offset + search_params->sss_res.freq_offset;
}

static int sync_trace_rank(const nr_initial_sync_t *sync_res)
{
  if (sync_res->trace.pbch_attempted)
    return 3;
  if (sync_res->trace.sss_success)
    return 2;
  if (sync_res->trace.pss_success)
    return 1;
  return 0;
}

static bool sync_trace_is_better(const nr_initial_sync_t *candidate, const nr_initial_sync_t *current)
{
  const int candidate_rank = sync_trace_rank(candidate);
  const int current_rank = sync_trace_rank(current);
  return candidate_rank > current_rank
         || (candidate_rank == current_rank && candidate->trace.pss_peak_raw > current->trace.pss_peak_raw);
}

static void nr_scan_ssb(void *arg)
{
  /*   Initial synchronisation
   *
   *                                 1 radio frame = 10 ms
   *     <--------------------------------------------------------------------------->
   *     -----------------------------------------------------------------------------
   *     |                                 Received UE data buffer                    |
   *     ----------------------------------------------------------------------------
   *                     --------------------------
   *     <-------------->| pss | pbch | sss | pbch |
   *                     --------------------------
   *          sync_pos            SS/PBCH block
   */

  nr_ue_ssb_scan_t *ssbInfo = (nr_ue_ssb_scan_t *)arg;
  c16_t **rxdata = ssbInfo->rxdata;
  const NR_DL_FRAME_PARMS *fp = ssbInfo->fp;

  // Generate PSS time signal for this GSCN.
  __attribute__((aligned(32))) c16_t pssTime[NUMBER_PSS_SEQUENCE][fp->ofdm_symbol_size];
  const int pss_sequence = get_softmodem_params()->sl_mode == 0 ? NUMBER_PSS_SEQUENCE : NUMBER_PSS_SEQUENCE_SL;
  for (int nid2 = 0; nid2 < pss_sequence; nid2++)
    generate_pss_nr_time(fp->ofdm_symbol_size, fp->first_carrier_offset, nid2, ssbInfo->gscnInfo.ssbFirstSC, pssTime[nid2]);

  __attribute__((aligned(32))) c16_t rxdataF[NR_N_SYMBOLS_SSB][fp->nb_antennas_rx][fp->ofdm_symbol_size];

  // initial sync performed on two successive frames, if pbch passes on first frame, no need to process second frame
  // only one frame is used for simulation tools
  if (ssbInfo->freqOffset)
    compensate_freq_offset(rxdata, fp->nb_antennas_rx, ssbInfo->rxdata_sz, ssbInfo->freqOffset, fp->samples_per_subframe * 1000);

  for (int frame_id = 0; frame_id < ssbInfo->nFrames && !ssbInfo->syncRes.cell_detected; frame_id++) {
    c16_t *rxdataShift[fp->nb_antennas_rx];
    for (int i = 0; i < fp->nb_antennas_rx; i++)
      rxdataShift[i] = rxdata[i] + fp->samples_per_frame * frame_id;

    nr_ssb_search_params_t search_params = {
        .dl_CarrierFreq = fp->dl_CarrierFreq,
        .sampling_rate = fp->samples_per_subframe * 1000,
        .slots_per_frame = fp->slots_per_frame,
        .slots_per_subframe = fp->slots_per_subframe,
        .numerology_index = fp->numerology_index,
        .ofdm_symbol_size = fp->ofdm_symbol_size,
        .ofdm_offset_divisor = fp->ofdm_offset_divisor,
        .nb_antennas_rx = fp->nb_antennas_rx,
        .symbols_per_slot = fp->symbols_per_slot,
        .first_carrier_offset = fp->first_carrier_offset,
        .N_RB_DL = fp->N_RB_DL,
        .rxdata_size = fp->samples_per_frame,
        .rxdata = rxdataShift,
        .nb_prefix_samples = fp->nb_prefix_samples,
        .nb_prefix_samples0 = fp->nb_prefix_samples0,
        .ssb_start_subcarrier = ssbInfo->gscnInfo.ssbFirstSC,
        .subcarrier_spacing = fp->subcarrier_spacing,
        .samples_per_slot_wCP = fp->samples_per_slot_wCP,
        .target_nid_cell = ssbInfo->targetNidCell,
        .exclude_nid_cell = -1, // No exclusion for initial sync
        .apply_freq_offset = ssbInfo->foFlag,
        .fo_flag = ssbInfo->foFlag,
        .pss_cfo_search = ssbInfo->pssCfoSearch,
        .rxdataF = rxdataF,
        .pssTime = pssTime,
    };

    nr_initial_sync_t frame_res = {
        .cell_detected = false,
        .frame_id = frame_id,
        .trace.failure_reason = NR_SYNC_FAILURE_PSS,
    };
    frame_res.cell_detected = nr_search_ssb_common(&search_params);
    copy_search_trace(&frame_res, &search_params, ssbInfo->freqOffset);

    if (!frame_res.cell_detected) {
      if (sync_trace_is_better(&frame_res, &ssbInfo->syncRes))
        ssbInfo->syncRes = frame_res;
      continue;
    }

    ssbInfo->pssCorrAvgPower = search_params.pss_res.avg;
    ssbInfo->pssCorrPeakPower = search_params.pss_res.peak;
    ssbInfo->ssbOffset = search_params.pss_res.pos - search_params.nb_prefix_samples;
    ssbInfo->nidCell = search_params.sss_res.nid_cell;

#ifdef DEBUG_INITIAL_SYNCH
    LOG_I(PHY,
          "TDD Normal prefix: sss detection result; %d, CellId %d metric %d, phase %d, measured offset %d\n",
          frame_res.cell_detected,
          ssbInfo->nidCell,
          sss_metric,
          sss_phase,
          frame_res.rx_offset);
#endif
    ssbInfo->freqOffset += search_params.pss_res.freq_offset + search_params.sss_res.freq_offset;
    frame_res.trace.total_freq_offset = ssbInfo->freqOffset;

    if (frame_res.cell_detected) { // we got sss channel
      frame_res.cell_detected = nr_pbch_detection(ssbInfo->proc,
                                                   ssbInfo->fp,
                                                   ssbInfo->nidCell,
                                                   1,
                                                   ssbInfo->gscnInfo.ssbFirstSC,
                                                   &ssbInfo->halfFrameBit,
                                                   &ssbInfo->ssbIndex,
                                                   &ssbInfo->symbolOffset,
                                                   &ssbInfo->pbchResult,
                                                   &frame_res,
                                                   rxdataF); // start pbch detection at first symbol after pss
      if (frame_res.cell_detected || sync_trace_is_better(&frame_res, &ssbInfo->syncRes))
        ssbInfo->syncRes = frame_res;
      if (frame_res.cell_detected) {
        uint32_t rsrp_avg = nr_ue_calculate_ssb_rsrp(ssbInfo->fp, rxdataF[2], ssbInfo->gscnInfo.ssbFirstSC);
        int rsrp_db_per_re = 10 * log10(rsrp_avg);
        ssbInfo->adjust_rxgain = TARGET_RX_POWER - rsrp_db_per_re;
        LOG_I(PHY, "pbch rx ok. rsrp:%d dB/RE, adjust_rxgain:%d dB\n", rsrp_db_per_re, ssbInfo->adjust_rxgain);
      }
    }
  }

  completed_task_ans(ssbInfo->ans);
}

nr_initial_sync_t nr_initial_sync(UE_nr_rxtx_proc_t *proc,
                                  PHY_VARS_NR_UE *ue,
                                  int n_frames,
                                  nr_gscn_info_t gscnInfo[MAX_GSCN_BAND],
                                  int numGscn)
{
  NR_DL_FRAME_PARMS *fp = &ue->frame_parms;

  // Perform SSB scanning in parallel. One GSCN per thread.
  LOG_I(NR_PHY,
        "Starting cell search with center freq: %ld, bandwidth: %d. Scanning for %d number of GSCN.\n",
        fp->dl_CarrierFreq,
        fp->N_RB_DL,
        numGscn);
  DevAssert(numGscn);
  task_ans_t ans;
  init_task_ans(&ans, numGscn);
  nr_ue_ssb_scan_t ssb_info[numGscn];
  for (int s = 0; s < numGscn; s++) {
    nr_ue_ssb_scan_t *ssbInfo = &ssb_info[s];
    *ssbInfo = (nr_ue_ssb_scan_t){.gscnInfo = gscnInfo[s],
                                  .fp = &ue->frame_parms,
                                  .proc = proc,
                                  .syncRes.cell_detected = false,
                                  .syncRes.trace.failure_reason = NR_SYNC_FAILURE_PSS,
                                  .nFrames = n_frames,
                                  .foFlag = ue->UE_fo_compensation,
                                  .pssCfoSearch = ue->pss_cfo_search,
                                  .freqOffset = ue->initial_fo,
                                  .targetNidCell = ue->target_Nid_cell};
    ssbInfo->rxdata = malloc16_clear(fp->nb_antennas_rx * sizeof(c16_t *));
    for (int ant = 0; ant < fp->nb_antennas_rx; ant++) {
      ssbInfo->rxdata[ant] = malloc16(sizeof(c16_t) * (fp->samples_per_frame * n_frames + fp->ofdm_symbol_size));
      memcpy(ssbInfo->rxdata[ant], ue->common_vars.rxdata[ant], sizeof(c16_t) * fp->samples_per_frame * n_frames);
      memset(ssbInfo->rxdata[ant] + fp->samples_per_frame * n_frames, 0, fp->ofdm_symbol_size * sizeof(c16_t));
      ssbInfo->rxdata_sz = fp->samples_per_frame * n_frames + fp->ofdm_symbol_size;
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
  nr_initial_sync_t best_failure = {.cell_detected = false, .trace.failure_reason = NR_SYNC_FAILURE_PSS};
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
    } else {
      if (sync_trace_is_better(&ssbInfo->syncRes, &best_failure))
        best_failure = ssbInfo->syncRes;
    }
    for (int ant = 0; ant < fp->nb_antennas_rx; ant++) {
      free(ssbInfo->rxdata[ant]);
    }
    free(ssbInfo->rxdata);
    ssbInfo->rxdata = NULL;
  }

  // Set globals based on detected cell
  if (res) {
    fp->Nid_cell = res->nidCell;
    fp->ssb_start_subcarrier = res->gscnInfo.ssbFirstSC;
    fp->half_frame_bit = res->halfFrameBit;
    fp->ssb_index = res->ssbIndex;
    ue->symbol_offset = res->symbolOffset;
    ue->common_vars.freq_offset = res->freqOffset;
    ue->adjust_rxgain = res->adjust_rxgain;
  }

  // In initial sync, we indicate PBCH to MAC after the scan is complete.
  if (ue->if_inst && ue->if_inst->dl_indication) {
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

  LOG_D(PHY, "nr_initial sync ue RB_DL %d\n", fp->N_RB_DL);

  if (res) {
    // sync at symbol ue->symbol_offset
    // computing the offset wrt the beginning of the frame
    int mu = fp->numerology_index;
    // number of symbols with different prefix length
    // every 7*(1<<mu) symbols there is a different prefix length (38.211 5.3.1)
    int n_symb_prefix0 = (res->symbolOffset / (7 * (1 << mu))) + 1;
    const int sync_pos_frame = n_symb_prefix0 * (fp->ofdm_symbol_size + fp->nb_prefix_samples0)
                               + (res->symbolOffset - n_symb_prefix0) * (fp->ofdm_symbol_size + fp->nb_prefix_samples);
    // for a correct computation of frame number to sync with the one decoded at MIB we need to take into account in which of
    // the n_frames we got sync
    ue->init_sync_frame = n_frames - 1 - res->syncRes.frame_id;

    // we also need to take into account the shift by samples_per_frame in case the if is true
    if (res->ssbOffset < sync_pos_frame) {
      res->syncRes.rx_offset = fp->samples_per_frame - sync_pos_frame + res->ssbOffset;
      ue->init_sync_frame += 1;
    } else {
      res->syncRes.rx_offset = res->ssbOffset - sync_pos_frame;
    }

    LOG_I(PHY, "[UE%d] In synch, rx_offset %d samples\n", ue->Mod_id, res->syncRes.rx_offset);
    LOG_I(PHY, "[UE %d] Measured Carrier Frequency offset %d Hz\n", ue->Mod_id, res->freqOffset);
    LOG_A(PHY, "Initial sync successful, PCI: %d\n", fp->Nid_cell);
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
    return best_failure;
  }
}
