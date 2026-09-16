/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Implementation of RU procedures
 */

#include "PHY/defs_gNB.h"
#include "common/platform_types.h"
#include "nr/nr_common.h"
#include "sched_nr.h"
#include "PHY/MODULATION/phy_ofdm_mod.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "openair1/PHY/defs_nr_common.h"
#include "common/utils/LOG/log.h"
#include "common/utils/system.h"

#include "T.h"

#include "assertions.h"

#include <time.h>

// Looks up the DBT entry for a given FAPI beam index via ru->dbt_lut (built once in
// build_dbt_lut() when the DBT is loaded), a direct array access instead of a scan over
// dig_beam_list (whose entries' beam_idx is arbitrary, not tied to their list position).
static const nfapi_nr_dig_beam_t *find_dig_beam(const RU_t *ru, uint16_t beam_idx)
{
  AssertFatal(beam_idx < NFAPI_NR_MAX_DBT_BEAM_IDX, "beam_idx %u exceeds OAI's supported max %u\n", beam_idx, NFAPI_NR_MAX_DBT_BEAM_IDX);
  return ru->dbt_lut[beam_idx];
}

// dig_beam_weight_Re/Im are uint16_t only because that is the wire type in the SCF nFAPI struct
static c16_t dig_beam_weight(const nfapi_nr_dig_beam_t *beam, int txru)
{
  const nfapi_nr_txru_t *w = &beam->txru_list[txru];
  return (c16_t){.r = (int16_t)w->dig_beam_weight_Re, .i = (int16_t)w->dig_beam_weight_Im};
}

// RU OFDM Modulator gNodeB
// OFDM modulation core routine, generates a first_symbol to first_symbol+num_symbols on a particular slot and TX antenna port
void nr_feptx0(RU_t *ru, int tti_tx, int first_symbol, int num_symbols, int aa)
{
  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;

  unsigned int slot_offset,slot_offsetF;
  int slot = tti_tx;

  if (aa == 0 && first_symbol == 0)
    start_meas(&ru->ofdm_mod_stats);
  slot_offset = get_samples_slot_timestamp(fp, slot);
  slot_offsetF = first_symbol * fp->ofdm_symbol_size;

  int abs_first_symbol = slot * fp->symbols_per_slot;

  for (int idx_sym = abs_first_symbol; idx_sym < abs_first_symbol + first_symbol; idx_sym++)
    slot_offset += (idx_sym % (0x7 << fp->numerology_index)) ? fp->nb_prefix_samples : fp->nb_prefix_samples0;

  slot_offset += fp->ofdm_symbol_size * first_symbol;

  LOG_D(PHY,
        "SFN/SF:RU:TX:%d/%d aa %d Generating slot %d (first_symbol %d num_symbols %d) slot_offset %d, slot_offsetF %d\n",
        ru->proc.frame_tx,
        ru->proc.tti_tx,
        aa,
        slot,
        first_symbol,
        num_symbols,
        slot_offset,
        slot_offsetF);
  
  if (fp->Ncp == 1) {
    PHY_ofdm_mod(&ru->common.txdataF_BF[aa][slot_offsetF],
                 (int*)&ru->common.txdata[aa][slot_offset],
                 fp->ofdm_symbol_size,
                 num_symbols,
                 fp->nb_prefix_samples,
                 CYCLIC_PREFIX);
  } else {
    if (fp->numerology_index != 0) {
      
      if (!(slot%(fp->slots_per_subframe/2))&&(first_symbol==0)) { // case where first symbol in slot has longer prefix
        PHY_ofdm_mod(&ru->common.txdataF_BF[aa][slot_offsetF],
                     (int*)&ru->common.txdata[aa][slot_offset],
                     fp->ofdm_symbol_size,
                     1,
                     fp->nb_prefix_samples0,
                     CYCLIC_PREFIX);

        PHY_ofdm_mod(&ru->common.txdataF_BF[aa][slot_offsetF+fp->ofdm_symbol_size],
                     (int*)&ru->common.txdata[aa][slot_offset+fp->nb_prefix_samples0+fp->ofdm_symbol_size],
                     fp->ofdm_symbol_size,
                     num_symbols-1,
                     fp->nb_prefix_samples,
                     CYCLIC_PREFIX);
      }
      else { // all symbols in slot have shorter prefix
        PHY_ofdm_mod(&ru->common.txdataF_BF[aa][slot_offsetF],
                     (int*)&ru->common.txdata[aa][slot_offset],
                     fp->ofdm_symbol_size,
                     num_symbols,
                     fp->nb_prefix_samples,
                     CYCLIC_PREFIX);
      }
    } // numerology_index!=0
    else { //numerology_index == 0
      for (int idx_sym = abs_first_symbol; idx_sym < abs_first_symbol+num_symbols; idx_sym++) {
        if (idx_sym % 0x7) {
          PHY_ofdm_mod(&ru->common.txdataF_BF[aa][slot_offsetF],
                       (int*)&ru->common.txdata[aa][slot_offset],
                       fp->ofdm_symbol_size,
                       1,
                       fp->nb_prefix_samples,
                       CYCLIC_PREFIX);
          slot_offset += fp->nb_prefix_samples+fp->ofdm_symbol_size;
          slot_offsetF += fp->ofdm_symbol_size;
        }
        else {
          PHY_ofdm_mod(&ru->common.txdataF_BF[aa][slot_offsetF],
                       (int*)&ru->common.txdata[aa][slot_offset],
                       fp->ofdm_symbol_size,
                       1,
                       fp->nb_prefix_samples0,
                       CYCLIC_PREFIX);
          slot_offset += fp->nb_prefix_samples0+fp->ofdm_symbol_size;
          slot_offsetF += fp->ofdm_symbol_size;
        }
      } // for(idx_symbol..
    } //  numerology 0
  }

  if (aa == 0 && first_symbol == 0)
    stop_meas(&ru->ofdm_mod_stats);
}

// RU FEP TX OFDM modulation, single-thread
void nr_feptx_ofdm(RU_t *ru,int frame_tx,int tti_tx)
{
  nfapi_nr_config_request_scf_t *cfg = &ru->gNB_list[0]->gNB_config;
  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;

  unsigned int aa=0;
  int slot_sizeF = fp->ofdm_symbol_size * fp->symbols_per_slot;
  int slot = tti_tx;
  int *txdata = &ru->common.txdata[aa][get_samples_slot_timestamp(fp, slot)];

  if (nr_slot_select(cfg,frame_tx,slot) == NR_UPLINK_SLOT)
    return;

  nr_feptx0(ru, slot, 0, fp->symbols_per_slot, aa);

  LOG_D(PHY,
        "feptx_ofdm (TXPATH): frame %d, slot %d: txp (time %p) %d dB, txp (freq) %d dB\n",
        frame_tx,
        slot,
        txdata,
        dB_fixed(signal_energy((int32_t *)txdata, get_samples_per_slot(slot, fp))),
        dB_fixed(signal_energy_nodc((c16_t *)ru->common.txdataF_BF[aa], 2 * slot_sizeF)));
}

// Digital-beamforming precoding for one TX antenna, one slot
static void nr_feptx_prec_bf_antenna(RU_t *ru, int slot_tx, int aa)
{
  PHY_VARS_gNB *gNB = ru->gNB_list[0];
  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;

  // Digital beamforming: cfg->dbt_config is a per-antenna weight table (Category A, DU-side
  // combining - the RU/xran fronthaul just streams whatever lands in txdataF_BF)
  for (int s = 0; s < fp->symbols_per_slot; ++s) {
    uint16_t beam_idx = gNB->common_vars.beam_id[slot_tx * fp->symbols_per_slot + s][aa];
    const nfapi_nr_dig_beam_t *dig_beam = find_dig_beam(ru, beam_idx);
    AssertFatal(dig_beam != NULL, "No DBT entry for beam_id %d on antenna %d\n", beam_idx, aa);
    const c16_t w = dig_beam_weight(dig_beam, aa);
    rotate_cpx_vector(&gNB->common_vars.txdataF[aa][s * fp->ofdm_symbol_size],
                       w,
                       (c16_t *)&ru->common.txdataF_BF[aa][s * fp->ofdm_symbol_size],
                       fp->ofdm_symbol_size,
                       15);
  }
}

static void nr_feptx_prec_task(void *arg)
{
  feptx_cmd_t *cmd = (feptx_cmd_t *)arg;
  nr_feptx_prec_bf_antenna(cmd->ru, cmd->slot, cmd->aid);
  completed_task_ans(cmd->ans);
}

void nr_feptx_prec(RU_t *ru, int frame_tx, int slot_tx)
{
  AssertFatal(ru->num_gNB == 1, "Cannot handle more than 1 gNB\n");
  PHY_VARS_gNB *gNB = ru->gNB_list[0];
  nfapi_nr_config_request_scf_t *cfg = &gNB->gNB_config;
  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;
  start_meas(&ru->precoding_stats);

  if (nr_slot_select(cfg,frame_tx,slot_tx) == NR_UPLINK_SLOT)
    return;

  const int nt = fp->nb_antennas_tx;

  bool apply_dbt = ru->config.dbt_config.num_dig_beams > 0 && !gNB->common_vars.analog_bf && ru->do_precoding;
  if (!apply_dbt) {
    for (int aa = 0; aa < nt; ++aa)
      memcpy(ru->common.txdataF_BF[aa], gNB->common_vars.txdataF[aa], fp->samples_per_slot_wCP * sizeof(int32_t));
  } else if (nt == 1) {
    LOG_A(PHY, "Applying digital beamforming for single antenna case\n");
    // Common case (rfsim, small test setups): dispatching to the thread pool would be pure
    // overhead with nothing to parallelize against.
    nr_feptx_prec_bf_antenna(ru, slot_tx, 0);
  } else {
    LOG_A(PHY, "Applying digital beamforming for multi-antenna case\n");
    feptx_cmd_t arr[nt - 1];
    task_ans_t ans;
    init_task_ans(&ans, nt - 1);
    for (int aa = 1; aa < nt; aa++) {
      feptx_cmd_t *cmd = &arr[aa - 1];
      cmd->ru = ru;
      cmd->slot = slot_tx;
      cmd->aid = aa;
      cmd->ans = &ans;
      task_t t = {.func = nr_feptx_prec_task, .args = cmd};
      pushTpool(ru->threadPool, t);
    }
    // Antenna 0 runs on this thread while the pool works antennas 1..nt-1 concurrently.
    nr_feptx_prec_bf_antenna(ru, slot_tx, 0);
    join_task_ans(&ans);
  }

  stop_meas(&ru->precoding_stats);
}

// core routine for FEP TX, called from threads in RU TX thread-pool 
void nr_feptx(void *arg)
{
  feptx_cmd_t *feptx = (feptx_cmd_t *)arg;

  RU_t *ru = feptx->ru;
  int slot = feptx->slot;
  int aa = feptx->aid;
  int startSymbol = feptx->startSymbol;
  int numSymbols = feptx->numSymbols;

  if (aa == 0)
    start_meas(&ru->precoding_stats);

  const NR_DL_FRAME_PARMS *fp = &ru->gNB_list[0]->frame_parms;
  bool apply_dbt = ru->config.dbt_config.num_dig_beams > 0 && !ru->gNB_list[0]->common_vars.analog_bf && ru->do_precoding;
  if (!apply_dbt) {
    // Inverse FFT shift
    for (uint s = startSymbol; s < startSymbol + numSymbols; s++)
      fftshift_inverse(ru->gNB_list[0]->common_vars.txdataF[aa] + s * fp->ofdm_symbol_size,
                       (c16_t *)ru->common.txdataF_BF[aa] + s * fp->ofdm_symbol_size,
                       fp->N_RB_DL * NR_NB_SC_PER_RB,
                       fp->ofdm_symbol_size);
  } else {
    // Digital beamforming, fused with the inverse FFT shift: apply the per-antenna DBT weight
    const int nbins = fp->N_RB_DL * NR_NB_SC_PER_RB;
    const int half = nbins / 2;
    for (uint s = startSymbol; s < startSymbol + numSymbols; s++) {
      uint16_t beam_idx = ru->gNB_list[0]->common_vars.beam_id[slot * fp->symbols_per_slot + s][aa];
      const nfapi_nr_dig_beam_t *dig_beam = find_dig_beam(ru, beam_idx);
      AssertFatal(dig_beam != NULL, "No DBT entry for beam_id %d on antenna %d\n", beam_idx, aa);
      const c16_t w = dig_beam_weight(dig_beam, aa);
      const c16_t *in = ru->gNB_list[0]->common_vars.txdataF[aa] + s * fp->ofdm_symbol_size;
      c16_t *out = (c16_t *)ru->common.txdataF_BF[aa] + s * fp->ofdm_symbol_size;
      // negative-freq half -> back
      rotate_cpx_vector(in, w, out + fp->ofdm_symbol_size - half, half, 15);
      // dc + positive-freq half -> front
      rotate_cpx_vector(in + half, w, out, half, 15);
    }
  }

  if (aa == 0)
    stop_meas(&ru->precoding_stats);

  ////////////FEPTX////////////
  nr_feptx0(ru, slot, startSymbol, numSymbols, aa);

  // Task completed in //
  completed_task_ans(feptx->ans);
}

// RU FEP TX using thread-pool
void nr_feptx_tp(RU_t *ru, int frame_tx, int slot)
{
  nfapi_nr_config_request_scf_t *cfg = &ru->gNB_list[0]->gNB_config;
  const NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;

  if (nr_slot_select(cfg, frame_tx, slot) == NR_UPLINK_SLOT)
    return;
  start_meas(&ru->ofdm_total_stats);

  int nt = fp->nb_antennas_tx;
  size_t const sz = nt + (ru->half_slot_parallelization > 0) * nt;
  feptx_cmd_t arr[sz];
  task_ans_t ans;
  init_task_ans(&ans, sz);

  int nbfeptx = 0;
  for (int aid = 0; aid < nt; aid++) {
    feptx_cmd_t *feptx_cmd = &arr[nbfeptx];
    feptx_cmd->ans = &ans;
    feptx_cmd->aid = aid;
    feptx_cmd->ru = ru;
    feptx_cmd->slot = slot;
    feptx_cmd->startSymbol = 0;
    feptx_cmd->numSymbols =
        (ru->half_slot_parallelization > 0) ? ru->nr_frame_parms->symbols_per_slot >> 1 : ru->nr_frame_parms->symbols_per_slot;

    task_t t = {.func = nr_feptx, .args = feptx_cmd};
    pushTpool(ru->threadPool, t);
    nbfeptx++;
    if (ru->half_slot_parallelization > 0) {
      feptx_cmd_t *feptx_cmd = &arr[nbfeptx];
      feptx_cmd->ans = &ans;
      feptx_cmd->aid = aid;
      feptx_cmd->ru = ru;
      feptx_cmd->slot = slot;
      feptx_cmd->startSymbol = ru->nr_frame_parms->symbols_per_slot >> 1;
      feptx_cmd->numSymbols = ru->nr_frame_parms->symbols_per_slot >> 1;

      task_t t = {.func = nr_feptx, .args = feptx_cmd};
      pushTpool(ru->threadPool, t);
      nbfeptx++;
    }
  }
  join_task_ans(&ans);

  stop_meas(&ru->ofdm_total_stats);
}

// core RX FEP routine, called by threads in RU thread-pool
void nr_fep(void *arg)
{
  feprx_cmd_t *feprx_cmd = (feprx_cmd_t *)arg;
  int slot = feprx_cmd->slot;
  int startSymbol = feprx_cmd->startSymbol;
  int endSymbol = feprx_cmd->endSymbol;

  const NR_DL_FRAME_PARMS *fp = feprx_cmd->fp;
  for (int l = startSymbol; l <= endSymbol; l++) {
    nr_symbol_fep_ul(fp, feprx_cmd->rxdata, &feprx_cmd->rxdataF[l * fp->ofdm_symbol_size], l, slot, feprx_cmd->sample_offet);
    fftshift_inplace(&feprx_cmd->rxdataF[l * fp->ofdm_symbol_size], fp->N_RB_UL * NR_NB_SC_PER_RB, fp->ofdm_symbol_size);
  }

  completed_task_ans(feprx_cmd->ans);
}

// RU RX FEP using thread-pool
void nr_fep_tp(RU_t *ru, int slot)
{
  int nbfeprx = 0;
  start_meas(&ru->ofdm_demod_stats);

  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;
  int nt = fp->nb_antennas_rx;
  int tasks_per_slot = (ru->half_slot_parallelization > 0) ? 2 : 1;
  size_t const sz = nt * tasks_per_slot;
  feprx_cmd_t arr[sz];
  task_ans_t ans;
  init_task_ans(&ans, sz);
  int rxdataF_offset = (slot % RU_RX_SLOT_DEPTH) * fp->symbols_per_slot * fp->ofdm_symbol_size;

  int symbols_per_task = fp->symbols_per_slot / tasks_per_slot;

  for (int task_idx = 0; task_idx < tasks_per_slot; task_idx++) {
    int start_symbol = task_idx * symbols_per_task;
    int end_symbol = (task_idx + 1) * symbols_per_task - 1;
    if (task_idx == tasks_per_slot - 1)
      end_symbol = fp->symbols_per_slot - 1;

    for (int aid = 0; aid < nt; aid++) {
      feprx_cmd_t *feprx_cmd = &arr[nbfeprx];
      feprx_cmd->ans = &ans;
      feprx_cmd->fp = fp;
      feprx_cmd->slot = ru->proc.tti_rx;
      feprx_cmd->startSymbol = start_symbol;
      feprx_cmd->endSymbol = end_symbol;
      feprx_cmd->rxdata = (const c16_t *)ru->common.rxdata[aid];
      feprx_cmd->rxdataF = (c16_t *)&ru->common.rxdataF[aid][rxdataF_offset];
      feprx_cmd->sample_offet = ru->N_TA_offset;

      task_t t = {.func = nr_fep, .args = feprx_cmd};
      pushTpool(ru->threadPool, t);
      nbfeprx++;
    }
  }
  join_task_ans(&ans);

  stop_meas(&ru->ofdm_demod_stats);
}
