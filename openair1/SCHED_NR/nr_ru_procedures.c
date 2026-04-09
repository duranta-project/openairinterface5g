/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Implementation of RU procedures
 */

#include "PHY/defs_gNB.h"
#include "common/platform_types.h"
#include "sched_nr.h"
#include "PHY/MODULATION/modulation_common.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "openair1/PHY/defs_nr_common.h"
#include "PHY/phy_digital_beamforming.h"

#include "common/utils/LOG/log.h"
#include "common/utils/system.h"

#include "T.h"

#include "assertions.h"

#include <time.h>

// RU OFDM Modulator gNodeB
// OFDM modulation core routine, generates a first_symbol to first_symbol+num_symbols on a particular slot and TX antenna port
static void nr_feptx0(const NR_DL_FRAME_PARMS *fp,
                      const int *in,
                      int *out,
                      time_stats_t *stats,
                      int frame,
                      int slot,
                      int first_symbol,
                      int num_symbols,
                      int aa)
{
  unsigned int slot_offset, slot_offsetF;

  if (aa == 0 && first_symbol == 0)
    start_meas(stats);
  slot_offset = get_samples_slot_timestamp(fp, slot);
  slot_offsetF = first_symbol * fp->ofdm_symbol_size;

  int abs_first_symbol = slot * fp->symbols_per_slot;

  for (int idx_sym = abs_first_symbol; idx_sym < abs_first_symbol + first_symbol; idx_sym++)
    slot_offset += (idx_sym % (0x7 << fp->numerology_index)) ? fp->nb_prefix_samples : fp->nb_prefix_samples0;

  slot_offset += fp->ofdm_symbol_size * first_symbol;

  LOG_D(PHY,
        "SFN/SF:RU:TX:%d/%d aa %d Generating slot %d (first_symbol %d num_symbols %d) slot_offset %d, slot_offsetF %d\n",
        frame,
        slot,
        aa,
        slot,
        first_symbol,
        num_symbols,
        slot_offset,
        slot_offsetF);

  if (fp->Ncp == 1) {
    PHY_ofdm_mod(&in[slot_offsetF], &out[slot_offset], fp->ofdm_symbol_size, num_symbols, fp->nb_prefix_samples, CYCLIC_PREFIX);
  } else {
    if (fp->numerology_index != 0) {
      
      if (!(slot%(fp->slots_per_subframe/2))&&(first_symbol==0)) { // case where first symbol in slot has longer prefix
        PHY_ofdm_mod(&in[slot_offsetF], &out[slot_offset], fp->ofdm_symbol_size, 1, fp->nb_prefix_samples0, CYCLIC_PREFIX);

        PHY_ofdm_mod(&in[slot_offsetF + fp->ofdm_symbol_size],
                     &out[slot_offset + fp->nb_prefix_samples0 + fp->ofdm_symbol_size],
                     fp->ofdm_symbol_size,
                     num_symbols - 1,
                     fp->nb_prefix_samples,
                     CYCLIC_PREFIX);
      }
      else { // all symbols in slot have shorter prefix
        PHY_ofdm_mod(&in[slot_offsetF], &out[slot_offset], fp->ofdm_symbol_size, num_symbols, fp->nb_prefix_samples, CYCLIC_PREFIX);
      }
    } // numerology_index!=0
    else { //numerology_index == 0
      for (int idx_sym = abs_first_symbol; idx_sym < abs_first_symbol+num_symbols; idx_sym++) {
        if (idx_sym % 0x7) {
          PHY_ofdm_mod(&in[slot_offsetF], &out[slot_offset], fp->ofdm_symbol_size, 1, fp->nb_prefix_samples, CYCLIC_PREFIX);
          slot_offset += fp->nb_prefix_samples+fp->ofdm_symbol_size;
          slot_offsetF += fp->ofdm_symbol_size;
        }
        else {
          PHY_ofdm_mod(&in[slot_offsetF], &out[slot_offset], fp->ofdm_symbol_size, 1, fp->nb_prefix_samples0, CYCLIC_PREFIX);
          slot_offset += fp->nb_prefix_samples0+fp->ofdm_symbol_size;
          slot_offsetF += fp->ofdm_symbol_size;
        }
      } // for(idx_symbol..
    } //  numerology 0
  }

  if (aa == 0 && first_symbol == 0)
    stop_meas(stats);
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

  nr_feptx0(fp,
            (const int *)ru->common.txdataF_BF[aa],
            ru->common.txdata[aa],
            &ru->ofdm_mod_stats,
            frame_tx,
            tti_tx,
            0,
            NR_SYMBOLS_PER_SLOT,
            aa);

  LOG_D(PHY,
        "feptx_ofdm (TXPATH): frame %d, slot %d: txp (time %p) %d dB, txp (freq) %d dB\n",
        frame_tx,
        slot,
        txdata,
        dB_fixed(signal_energy((int32_t *)txdata, get_samples_per_slot(slot, fp))),
        dB_fixed(signal_energy_nodc((c16_t *)ru->common.txdataF_BF[aa], 2 * slot_sizeF)));
}

void nr_feptx_prec(RU_t *ru, int frame_tx, int slot_tx)
{
  PHY_VARS_gNB **gNB_list = ru->gNB_list;
  AssertFatal(ru->num_gNB == 1, "Cannot handle more than 1 gNB\n");
  PHY_VARS_gNB *gNB = gNB_list[0];
  nfapi_nr_config_request_scf_t *cfg = &ru->gNB_list[0]->gNB_config;

  if (nr_slot_select(cfg,frame_tx,slot_tx) == NR_UPLINK_SLOT)
    return;

  start_meas(&ru->precoding_stats);
  // Point gNB's tx grid pointer to RU
  ru->common.ru_tx_grid = gNB->common_vars.tx_grid_info;

  // Call tx beamforming interface
  tx_beamforming_if(ru);

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

  // FFT shift
  const NR_DL_FRAME_PARMS *fp = &ru->gNB_list[0]->frame_parms;
  c16_t ofdm_mod_in[fp->samples_per_slot_wCP] __attribute__((aligned(64)));
  memset(ofdm_mod_in, 0, sizeof(ofdm_mod_in));
  fft_shift((c16_t *)ru->common.txdataF_BF[aa],
            fp->ofdm_symbol_size,
            fp->N_RB_DL,
            ofdm_mod_in,
            fp->ofdm_symbol_size,
            startSymbol,
            numSymbols,
            false);

  ////////////FEPTX////////////
  nr_feptx0(fp,
            (const int *)ofdm_mod_in,
            ru->common.txdata[aa],
            &ru->ofdm_mod_stats,
            ru->proc.frame_tx,
            slot,
            startSymbol,
            numSymbols,
            aa);

  // Task completed in //
  completed_task_ans(feptx->ans);
}

// RU FEP TX using thread-pool
void nr_feptx_tp(RU_t *ru, int frame_tx, int slot)
{
  nfapi_nr_config_request_scf_t *cfg = &ru->gNB_list[0]->gNB_config;
  if (nr_slot_select(cfg, frame_tx, slot) == NR_UPLINK_SLOT)
    return;
  start_meas(&ru->ofdm_total_stats);

  const NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;
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

  for (int l = startSymbol; l <= endSymbol; l++)
    nr_symbol_fep_ul(feprx_cmd->fp,
                     feprx_cmd->rxdata,
                     &feprx_cmd->rxdataF[l * feprx_cmd->fp->ofdm_symbol_size],
                     l,
                     slot,
                     feprx_cmd->sample_offet);

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
