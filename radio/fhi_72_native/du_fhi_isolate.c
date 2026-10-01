/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "du_fhi_isolate.h"
#include "du_fhi_config.h"
#include "du_fhi_prach.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "common/utils/utils.h"
#include "common/utils/LOG/log.h"
#include "common/utils/nr/nr_common.h"

#define DU_FHI_MAX_ANTENNAS 4
#define DU_FHI_MAX_SCRATCH_PRB 275

// Single active instance: RU_t/ru_thread's fh_south_in/fh_south_out contract carries no
// user-data pointer (mirrors radio/fhi_72/oran_isolate.c's own use of file-scope statics), and
// OAI does not support more than one fronthaul transport instance per process today.
static du_fhi_state_t *g_state = NULL;
static du_fhi_prach_config_t g_prach_cfg;

du_fhi_state_t *du_fhi_init_from_config(du_fh_config_t *cfg)
{
  du_fhi_state_t *st = calloc_or_fail(1, sizeof(*st));
  st->cfg = *cfg;

  int slot_duration_uS = 1000 >> cfg->numerology;
  st->ul_lookahead_slots = (cfg->T1a_cp_ul_max_uS / slot_duration_uS) + 2;

  st->du_fh_handle = du_fh_init(cfg);
  if (!st->du_fh_handle) {
    free(st);
    return NULL;
  }
  return st;
}

void du_fhi_cleanup(du_fhi_state_t *st)
{
  if (!st)
    return;
  if (st->du_fh_handle)
    du_fh_cleanup(st->du_fh_handle);
  free(st);
}

void du_fhi_south_out(RU_t *ru, int frame, int slot, uint64_t timestamp)
{
  du_fhi_state_t *st = g_state;
  if (!st)
    return;

  int nb_rx = ru->nb_rx;
  uint64_t hyper_frame = du_fhi_query_hyper_frame(st->du_fh_handle, frame);

  uint64_t prev_target = st->have_scheduled ? st->last_scheduled_absolute_slot : 0;
  bool had_scheduled_before = st->have_scheduled;
  uint64_t target = du_fhi_advance_ul_schedule_lookahead(st, frame, slot, nb_rx);
  uint64_t start = had_scheduled_before ? prev_target + 1 : target;
  du_fhi_advance_prach_lookahead(st, &g_prach_cfg, start, target, nb_rx);

  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;
  int ofdm_symbol_size = fp->ofdm_symbol_size;
  int num_prb = st->cfg.num_prbs;
  int nb_tx = ru->nb_tx > DU_FHI_MAX_ANTENNAS ? DU_FHI_MAX_ANTENNAS : ru->nb_tx;

  // du_fh_tx_send_dl_iq() takes one section list per call, shared across every antenna in
  // that call (see fronthaul/du/du_tx_scheduler.c's du_tx_schedule_dl_iq: the C-Plane loop
  // reuses the same `sections` for every antenna index) -- there is no per-antenna beam
  // parameter in the current API, so we take antenna 0's beam as representative for the
  // whole symbol's DL section.
  du_tx_dl_section_t section = {.beam_id = 0, .start_prb = 0, .num_prb = num_prb, .section_id = 0};

  for (int symbol = 0; symbol < NR_SYMBOLS_PER_SLOT; symbol++) {
    if (!du_fhi_is_dl_symbol(&st->cfg, slot, symbol))
      continue;

    // beam_id is only allocated when analog beamforming is enabled
    uint16_t **beam_id = ru->gNB_list[0]->common_vars.beam_id;
    section.beam_id = beam_id ? beam_id[slot * NR_SYMBOLS_PER_SLOT + symbol][0] : 0;

    // txdataF_BF holds each symbol in wire order already (first negative subcarrier first)
    uint32_t *symbol_ptrs[DU_FHI_MAX_ANTENNAS];
    for (int ant = 0; ant < nb_tx; ant++)
      symbol_ptrs[ant] = (uint32_t *)&ru->common.txdataF_BF[ant][symbol * ofdm_symbol_size];

    du_fh_tx_send_dl_iq(st->du_fh_handle, symbol_ptrs, nb_tx, hyper_frame, frame, slot, symbol, &section, 1);
  }
}

void du_fhi_south_in(RU_t *ru, int *frame, int *slot)
{
  du_fhi_state_t *st = g_state;
  if (!st)
    return;

  int nb_rx = ru->nb_rx > DU_FHI_MAX_ANTENNAS ? DU_FHI_MAX_ANTENNAS : ru->nb_rx;
  int num_prb = st->cfg.num_prbs;
  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;
  int ofdm_symbol_size = fp->ofdm_symbol_size;
  int slots_per_frame = 10 << st->cfg.numerology;

  // Like xran's per-slot rx callback in the vendor path: one event per OTA slot, DL or UL, once
  // its UL receive window has closed. This is the real-time clock of ru_thread.
  uint64_t abs_slot;
  int skipped = du_fh_wait_slot(st->du_fh_handle, &abs_slot);
  if (skipped > 0)
    LOG_W(HW, "DU fronthaul: TTI processing delay detected, skipped %d slot(s)\n", skipped);
  int f = (abs_slot / slots_per_frame) % 1024;
  int sl = abs_slot % slots_per_frame;

  RU_proc_t *proc = &ru->proc;
  proc->tti_rx = sl;
  proc->frame_rx = f;
  proc->tti_tx = (sl + ru->sl_ahead) % slots_per_frame;
  proc->frame_tx = (sl > (slots_per_frame - 1 - ru->sl_ahead)) ? (f + 1) & 1023 : f;
  if (proc->first_rx == 0) {
    if (f != *frame || sl != *slot)
      LOG_E(HW, "Received Time doesn't correspond to the time we think it is (received %d.%d, expected %d.%d)\n", f, sl, *frame, *slot);
  } else {
    proc->first_rx = 0;
    LOG_I(HW, "before adjusting, OAI: frame=%d slot=%d, fronthaul: frame=%d slot=%d\n", *frame, *slot, f, sl);
  }
  *frame = f;
  *slot = sl;

  // Every UL symbol job up to the end of this slot is ready; older ones belong to skipped slots.
  int rxdataF_slot_size = NR_SYMBOLS_PER_SLOT * ofdm_symbol_size;
  uint64_t first_symbol = abs_slot * NR_SYMBOLS_PER_SLOT;
  uint64_t last_symbol = first_symbol + NR_SYMBOLS_PER_SLOT - 1;
  uint32_t compact[DU_FHI_MAX_ANTENNAS][DU_FHI_MAX_SCRATCH_PRB * NR_NB_SC_PER_RB];
  uint32_t *compact_ptrs[DU_FHI_MAX_ANTENNAS];
  for (int ant = 0; ant < nb_rx; ant++)
    compact_ptrs[ant] = compact[ant];
  uint64_t symbol_abs;
  while (du_fh_read_ul_iq_upto(st->du_fh_handle, compact_ptrs, nb_rx, last_symbol, &symbol_abs)) {
    if (symbol_abs < first_symbol)
      continue;
    int symbol = symbol_abs - first_symbol;
    for (int ant = 0; ant < nb_rx; ant++) {
      // rxdataF is indexed per RU_RX_SLOT_DEPTH slot and holds symbols in wire order
      c16_t *dst = &ru->common.rxdataF[ant][(sl % RU_RX_SLOT_DEPTH) * rxdataF_slot_size + symbol * ofdm_symbol_size];
      const c16_t *src = (const c16_t *)compact[ant];
      if (st->cfg.comp_type == FH_COMP_NONE) {
        // same scaling of uncompressed UL IQ as the vendor path (radio/fhi_72/oaioran.c)
        for (int i = 0; i < num_prb * NR_NB_SC_PER_RB; i++)
          dst[i] = (c16_t){src[i].r >> 2, src[i].i >> 2};
      } else {
        memcpy(dst, src, num_prb * NR_NB_SC_PER_RB * sizeof(*dst));
      }
    }
  }

  du_fhi_south_in_prach(st->du_fh_handle, &g_prach_cfg, ru->gNB_list[0], *frame, *slot, st->cfg.numerology);
}

void *get_internal_parameter(char *name)
{
  if (!strcmp(name, "fh_if4p5_south_in"))
    return (void *)du_fhi_south_in;
  if (!strcmp(name, "fh_if4p5_south_out"))
    return (void *)du_fhi_south_out;
  return NULL;
}

static int du_fhi_trx_start(openair0_device_t *device)
{
  du_fhi_state_t *st = (du_fhi_state_t *)device->priv;
  return du_fh_start(st->du_fh_handle);
}

static int du_fhi_trx_stop(openair0_device_t *device)
{
  du_fhi_state_t *st = (du_fhi_state_t *)device->priv;
  du_fh_stop(st->du_fh_handle);
  return 0;
}

static void du_fhi_trx_end(openair0_device_t *device)
{
  du_fhi_state_t *st = (du_fhi_state_t *)device->priv;
  du_fhi_cleanup(st);
  g_state = NULL;
}

static int du_fhi_trx_get_stats(openair0_device_t *device)
{
  du_fhi_state_t *st = (du_fhi_state_t *)device->priv;
  du_fh_print_stats(st->du_fh_handle);
  return 0;
}

__attribute__((__visibility__("default"))) int transport_init(openair0_device_t *device, openair0_config_t *openair0_cfg)
{
  du_fh_config_t cfg;
  frequency_range_t prach_freq_range;
  if (get_du_fh_options(&cfg, &openair0_cfg[0], &prach_freq_range) < 0) {
    LOG_E(HW, "native DU fronthaul: failed to read configuration\n");
    return -1;
  }

  g_prach_cfg.prach_config_index = (uint8_t)openair0_cfg[0].split7.prach_index;
  g_prach_cfg.prach_info = get_nr_prach_occasion_info_from_index(g_prach_cfg.prach_config_index, prach_freq_range, cfg.fdd_mode ? 0 : 1);
  g_prach_cfg.freq_start = openair0_cfg[0].split7.prach_freq_start;
  g_prach_cfg.fft_size = openair0_cfg[0].split7.prach_fftSize;

  du_fhi_state_t *st = du_fhi_init_from_config(&cfg);
  if (!st) {
    LOG_E(HW, "native DU fronthaul: du_fhi_init_from_config failed\n");
    return -1;
  }

  g_state = st;

  device->host_type = RAU_HOST;
  device->transp_type = ETHERNET_TP;
  device->trx_start_func = du_fhi_trx_start;
  device->trx_stop_func = du_fhi_trx_stop;
  device->trx_end_func = du_fhi_trx_end;
  device->trx_get_stats_func = du_fhi_trx_get_stats;
  device->get_internal_parameter = get_internal_parameter;
  device->priv = st;
  device->openair0_cfg = &openair0_cfg[0];

  return 0;
}
