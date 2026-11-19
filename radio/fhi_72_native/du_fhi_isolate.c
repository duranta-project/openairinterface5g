/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "du_fhi_isolate.h"
#include "du_fhi_config.h"
#include "du_fhi_prach.h"
#include <string.h>
#include <stdlib.h>
#include "common/utils/utils.h"
#include "common/utils/system.h"
#include "common/utils/LOG/log.h"
#include "common/utils/nr/nr_common.h"
#include "openair1/PHY/nr_phy_common/inc/nr_phy_meas.h" // START_MEAS_FULL_SLOT
#include "executables/nr-softmodem.h" // ru_push_tx_job(), wait_free_rx_tti()

#define DU_FHI_MAX_ANTENNAS 4
#define DU_FHI_MAX_SCRATCH_PRB 275

// Single active instance: the fhi72_transport_t TX callbacks carry only split7_config_t (mirrors
// radio/fhi_72/oran_isolate.c's own use of file-scope state), and OAI does not support more than
// one fronthaul transport instance per process today.
static du_fhi_state_t *g_state = NULL;
static du_fhi_prach_config_t g_prach_cfg;
// gNB->common_vars.beam_id, set by du_fhi_cp() when analog beamforming is on
static uint16_t **g_beam_id = NULL;

du_fhi_state_t *du_fhi_init_from_config(du_fh_config_t *cfg)
{
  du_fhi_state_t *st = calloc_or_fail(1, sizeof(*st));
  st->cfg = *cfg;

  // UL grants are scheduled when a slot's UL window has closed, Ta3_max after its end, and must be
  // sent T1a_cp_ul_max before their own slot; one more slot of margin for the processing.
  int slot_duration_uS = 1000 >> cfg->numerology;
  st->ul_lookahead_slots = 1 + (cfg->Ta3_max_uS + cfg->T1a_cp_ul_max_uS + slot_duration_uS - 1) / slot_duration_uS + 1;

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

/* No digital beamforming in split 7.2: the precoded buffer is the L1 output */
static void du_fhi_prec_bf(struct PHY_VARS_gNB_s *gNB, int frame, int slot)
{
  split7_config_t *s7cfg = &gNB->fhi72_transport->s7cfg;
  START_MEAS_FULL_SLOT(&s7cfg->prec_bf, s7cfg->slot_dir[slot], NR_DOWNLINK_SLOT);
  AssertFatal(gNB->gNB_config.dbt_config.num_dig_beams == 0 || gNB->common_vars.analog_bf,
              "native DU fronthaul: digital beamforming is not supported\n");
  for (int i = 0; i < s7cfg->tx_num_channels; i++)
    memcpy(s7cfg->txdataF_BF[i], gNB->common_vars.txdataF[i], gNB->frame_parms.samples_per_slot_wCP * sizeof(int32_t));
  STOP_MEAS_FULL_SLOT(&s7cfg->prec_bf, s7cfg->slot_dir[slot], NR_DOWNLINK_SLOT);
}

/* DL C-Plane goes out with the DL U-Plane, UL C-Plane from the UL look-ahead: only keep the beams */
static void du_fhi_cp(split7_config_t *s7cfg, uint16_t **beam_id, int frame, int slot)
{
  g_beam_id = beam_id;
}

static void du_fhi_dl_up(split7_config_t *s7cfg, int frame, int slot)
{
  du_fhi_state_t *st = g_state;
  START_MEAS_FULL_SLOT(&s7cfg->dl_up, s7cfg->slot_dir[slot], NR_DOWNLINK_SLOT);
  uint64_t hyper_frame = du_fhi_query_hyper_frame(st->du_fh_handle, frame);
  int ofdm_symbol_size = 1 << s7cfg->fftSize;
  int nb_tx = min(s7cfg->tx_num_channels, DU_FHI_MAX_ANTENNAS);

  // du_fh_tx_send_dl_iq() takes one section list per call, shared by every antenna of the call: no
  // per-antenna beam, antenna 0's beam is taken for the whole symbol.
  du_tx_dl_section_t section = {.beam_id = 0, .start_prb = 0, .num_prb = st->cfg.num_prbs, .section_id = 0};
  for (int symbol = 0; symbol < NR_SYMBOLS_PER_SLOT; symbol++) {
    if (!du_fhi_is_dl_symbol(&st->cfg, slot, symbol))
      continue;
    section.beam_id = g_beam_id ? g_beam_id[slot * NR_SYMBOLS_PER_SLOT + symbol][0] : 0;
    // txdataF_BF holds each symbol in wire order already (first negative subcarrier first)
    uint32_t *symbol_ptrs[DU_FHI_MAX_ANTENNAS];
    for (int ant = 0; ant < nb_tx; ant++)
      symbol_ptrs[ant] = (uint32_t *)&s7cfg->txdataF_BF[ant][symbol * ofdm_symbol_size];
    du_fh_tx_send_dl_iq(st->du_fh_handle, symbol_ptrs, nb_tx, hyper_frame, frame, slot, symbol, &section, 1);
  }
  STOP_MEAS_FULL_SLOT(&s7cfg->dl_up, s7cfg->slot_dir[slot], NR_DOWNLINK_SLOT);
}

/* Copies every UL symbol received for abs_slot into rxdataF; older symbols belong to skipped slots */
static void read_ul_slot(du_fhi_state_t *st, const split7_config_t *s7cfg, uint64_t abs_slot, int slot)
{
  int nb_rx = min(s7cfg->rx_num_channels, DU_FHI_MAX_ANTENNAS);
  int num_sc = st->cfg.num_prbs * NR_NB_SC_PER_RB;
  int ofdm_symbol_size = 1 << s7cfg->fftSize;
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
      c16_t *dst = (c16_t *)&s7cfg->rxdataF[ant][(slot % RU_RX_SLOT_DEPTH) * rxdataF_slot_size + symbol * ofdm_symbol_size];
      const c16_t *src = (const c16_t *)compact[ant];
      if (st->cfg.comp_type == FH_COMP_NONE) {
        // same scaling of uncompressed UL IQ as the vendor path (radio/fhi_72/oaioran.c)
        for (int i = 0; i < num_sc; i++)
          dst[i] = (c16_t){src[i].r >> 2, src[i].i >> 2};
      } else {
        memcpy(dst, src, num_sc * sizeof(*dst));
      }
    }
  }
}

/* split 7.2 FH thread: one event per OTA slot once its UL receive window has closed. Schedules UL grants and
 * PRACH ahead, reads UL data of UL slots into rxdataF, then pushes the TX job. */
static void *split7_thread(void *param)
{
  PHY_VARS_gNB *gNB = param;
  split7_config_t *s7cfg = &gNB->fhi72_transport->s7cfg;
  du_fhi_state_t *st = g_state;

  wait_sync("split7_thread");

  // after wait_sync(): UL jobs released while nobody reads them would exhaust the job pool
  AssertFatal(du_fh_start(st->du_fh_handle) == 0, "Could not start the native DU fronthaul\n");

  int nb_rx = min(s7cfg->rx_num_channels, DU_FHI_MAX_ANTENNAS);
  // rxdataF slots still being read by L1 (rx_func() releases them through L1_rx_out)
  bool rx_tti_busy[RU_RX_SLOT_DEPTH] = {false};
  while (!oai_exit) {
    uint64_t abs_slot;
    int skipped = du_fh_wait_slot(st->du_fh_handle, &abs_slot);
    if (skipped < 0)
      break; // the fronthaul was stopped
    if (skipped > 0)
      LOG_W(HW, "DU fronthaul: TTI processing delay detected, skipped %d slot(s)\n", skipped);
    int frame = (abs_slot / s7cfg->slots_per_frame) % 1024;
    int slot = abs_slot % s7cfg->slots_per_frame;

    uint64_t first_slot;
    uint64_t target = du_fhi_advance_ul_schedule_lookahead(st, abs_slot, nb_rx, &first_slot);
    du_fhi_advance_prach_lookahead(st, &g_prach_cfg, first_slot, target, nb_rx);

    if (s7cfg->slot_dir[slot] & NR_UPLINK_SLOT) { // also covers NR_MIXED_SLOT (and FDD)
      if (!wait_free_rx_tti(&gNB->L1_rx_out, rx_tti_busy, frame, slot))
        break; // L1_rx_out aborted: stopping
      START_MEAS_FULL_SLOT(&s7cfg->ul_up, s7cfg->slot_dir[slot], NR_UPLINK_SLOT);
      read_ul_slot(st, s7cfg, abs_slot, slot);
      du_fhi_read_prach(st->du_fh_handle, gNB, abs_slot, st->cfg.numerology);
      STOP_MEAS_FULL_SLOT(&s7cfg->ul_up, s7cfg->slot_dir[slot], NR_UPLINK_SLOT);
    }

    int frame_tx = (slot > s7cfg->slots_per_frame - 1 - s7cfg->sl_ahead) ? (frame + 1) & 1023 : frame;
    int slot_tx = (slot + s7cfg->sl_ahead) % s7cfg->slots_per_frame;
    // no timestamp as 7.2 is a frequency domain interface
    ru_push_tx_job(gNB, frame_tx, slot_tx, frame, slot, 0);
  }

  du_fh_stop(st->du_fh_handle); // prints the statistics
  du_fhi_cleanup(st);
  g_state = NULL;
  LOG_I(HW, "Exiting split7_thread\n");
  return NULL;
}

/* Library entry point (see fhi72_init_t): configures the fronthaul, fills gNB->fhi72_transport, spawns split7_thread() */
__attribute__((__visibility__("default"))) int oran_fhi72_init(struct PHY_VARS_gNB_s *gNB)
{
  fhi72_transport_t *t = gNB->fhi72_transport;
  const split7_config_t *s7cfg = &t->s7cfg;
  du_fh_config_t cfg;
  frequency_range_t prach_freq_range;
  if (get_du_fh_options(&cfg, s7cfg, &prach_freq_range) < 0) {
    LOG_E(HW, "native DU fronthaul: failed to read configuration\n");
    return -1;
  }

  g_prach_cfg.prach_config_index = s7cfg->prach_index;
  g_prach_cfg.freq_range = prach_freq_range;
  g_prach_cfg.prach_info = get_nr_prach_occasion_info_from_index(g_prach_cfg.prach_config_index, prach_freq_range, cfg.fdd_mode ? 0 : 1);
  if (du_fhi_prach_is_long_format(g_prach_cfg.prach_info.format)) {
    // The fronthaul library carries the 139-subcarrier short sequence only
    LOG_E(HW, "native DU fronthaul: long PRACH formats (0-3) are not supported, PRACH configuration index %d\n", g_prach_cfg.prach_config_index);
    return -1;
  }
  g_prach_cfg.freq_start = s7cfg->prach_freq_start;
  g_prach_cfg.fft_size = s7cfg->prach_fftSize;

  g_state = du_fhi_init_from_config(&cfg);
  if (!g_state) {
    LOG_E(HW, "native DU fronthaul: du_fhi_init_from_config failed\n");
    return -1;
  }

  t->prec_bf = du_fhi_prec_bf;
  t->dl_up = du_fhi_dl_up;
  t->cp = du_fhi_cp;
  threadCreate(&t->pthread, split7_thread, gNB, "split7_thread", gNB->RU_list[0]->ru_thread_core, OAI_PRIORITY_RT_MAX);
  return 0;
}
