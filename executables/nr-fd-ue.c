/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * UE main thread for Split 7.1 (frequency-domain I/O): mirrors nr-ue.c but
 * replaces time-domain RF I/O with ue_split7_device_t calls. Two PHY hooks
 * bypass FFT/IFFT: fd_rxdataF_ring (nr_slot_fep() memcpys from it instead of
 * computing the DFT) and fd_tx_cb/fd_tx_cb_data (called from
 * phy_procedures_nrUE_TX() in place of nr_tx_rotation_and_ofdm_mod()).
 */

#define _GNU_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>

#include "PHY/defs_nr_common.h"
#include "PHY/defs_nr_UE.h"
#include "PHY/INIT/nr_phy_init.h"
#include "PHY/TOOLS/tools_defs.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "SCHED_NR_UE/defs.h"
#include "NR_MAC_UE/mac_proto.h"
#include "NR_UE_PHY_INTERFACE/NR_IF_Module.h"
#include "RRC/NR_UE/rrc_proto.h"
#include "RRC/NR_UE/L2_interface_ue.h"
#include "executables/nr-ue-ru.h"
#include "executables/nr-uesoftmodem.h"
#include "executables/softmodem-common.h"
#include "radio/COMMON/ue_split7_interface.h"
#include "common/utils/LOG/log.h"
#include "common/utils/threadPool/notified_fifo.h"
#include "common/utils/time_manager/time_manager.h"
#include "common/utils/barrier/barrier.h"
#include "nr_phy_common.h"

/* Non-static functions from nr-ue.c accessible via external linkage. */
extern void UE_dl_processing(void *arg);
extern int UE_dl_preprocessing(PHY_VARS_NR_UE *UE,
                               const UE_nr_rxtx_proc_t *proc,
                               int *tx_wait_for_dlsch,
                               nr_phy_data_t *phy_data,
                               bool *stats_printed);
extern size_t dump_L1_UE_meas_stats(PHY_VARS_NR_UE *ue, char *output, size_t max_len);
extern void *nrL1_UE_stats_thread(void *param);
extern int nr_ue_slot_select(const fapi_nr_config_request_t *cfg, int nr_slot);
extern int determine_N_TA_offset(PHY_VARS_NR_UE *ue);
extern void processSlotTX(void *arg);
extern void start_process_slot_tx(void *arg);

typedef struct {
  ue_split7_device_t *dev;
} fd_tx_ctx_t;

static void fd_prach_tx_cb_impl(PHY_VARS_NR_UE *ue,
                                int frame,
                                int slot,
                                openair0_timestamp_t timestamp_tx,
                                const c16_t *prachF,
                                int dftlen,
                                int Ncp,
                                int prach_start,
                                int copies,
                                void *userdata)
{
  fd_tx_ctx_t *ctx = (fd_tx_ctx_t *)userdata;
  ue_split7_prach_tx_params_t params;
  memset(&params, 0, sizeof(params));

  params.samples = (const ue_split7_iq_t *)prachF;
  params.num_samples = dftlen;
  params.fft_size = dftlen;
  params.time_offset_samples = prach_start;
  params.cp_len_samples = Ncp;
  params.timestamp_samples = (uint64_t)timestamp_tx; // same clock domain as curMsg.proc.timestamp_tx

  params.slot_number = slot; // slot-in-frame, per params' documented contract
  params.symbol_number = 0;
  params.repetition_count = copies;
  params.antenna_port = 0;

  ue_split7_status_t rc = ctx->dev->write_prach(ctx->dev, &params);
  if (rc != UE_SPLIT7_SUCCESS)
    LOG_E(NR_PHY, "[fd-ue] write_prach failed (%d) for frame %d slot %d\n", (int)rc, frame, slot);
}

static void fd_tx_cb_impl(PHY_VARS_NR_UE *ue,
                          const UE_nr_rxtx_proc_t *proc,
                          int nb_ant_tx,
                          c16_t **txdataF,
                          const bool *was_symbol_used,
                          void *userdata)
{
  fd_tx_ctx_t *ctx = (fd_tx_ctx_t *)userdata;

  // generate_nr_prach() detects ue->fd_prach_tx_cb and routes the preamble to
  // write_prach() instead of the time-domain path; txData is unused here.
  NR_UE_PRACH *prach_var = ue->prach_vars[proc->gNB_id];
  if (prach_var->active) {
    nr_ue_prach_procedures(ue, proc, NULL);
    return;
  }

  const NR_DL_FRAME_PARMS *fp = &ue->frame_parms;
  ue_split7_symbol_buffer_t bufs[8];

  openair0_timestamp_t sym_ts = proc->timestamp_tx; // symbol 0's CP starts here

  for (int sym = 0; sym < fp->symbols_per_slot; sym++) {
    const int sym_off = sym * (int)fp->ofdm_symbol_size;
    if (was_symbol_used[sym]) {
      for (int a = 0; a < nb_ant_tx; a++) {
        bufs[a].meta.symbol_number = (uint8_t)sym;
        bufs[a].meta.slot_number = (uint16_t)proc->nr_slot_tx;
        bufs[a].meta.timestamp_samples = sym_ts;
        bufs[a].num_subcarriers = fp->ofdm_symbol_size;
        bufs[a].re_buffer = (ue_split7_iq_t *)&txdataF[a][sym_off];
      }
      ue_split7_status_t rc = ctx->dev->write_symbol(ctx->dev, bufs, (uint16_t)nb_ant_tx);
      if (rc != UE_SPLIT7_SUCCESS)
        LOG_E(NR_PHY, "[fd-ue] write_symbol failed (%d) for slot %d symbol %d\n", (int)rc, proc->nr_slot_tx, sym);
    }
    sym_ts += (openair0_timestamp_t)get_samples_symbol_duration(fp, proc->nr_slot_tx, sym, 1);
  }
}

// Ring index must match nr_slot_fep()'s read-back formula (slot_fep_nr.c):
// ((frame % 2) * slots_per_frame + slot) * symbols_per_slot + symbol
static openair0_timestamp_t fd_read_slot(ue_split7_device_t *dev,
                                         PHY_VARS_NR_UE *UE,
                                         const NR_DL_FRAME_PARMS *fp,
                                         int frame,
                                         int slot_nr)
{
  ue_split7_symbol_buffer_t bufs[8];
  openair0_timestamp_t first_ts = 0;

  for (int sym = 0; sym < fp->symbols_per_slot; sym++) {
    const uint32_t ring_symbol = ((frame % 2) * fp->slots_per_frame + slot_nr) * fp->symbols_per_slot + sym;
    for (int a = 0; a < fp->nb_antennas_rx; a++) {
      bufs[a].meta.symbol_number = (uint8_t)sym;
      bufs[a].meta.slot_number = (uint16_t)slot_nr;
    }
    // read_symbol() fills bufs[a].re_buffer with a pointer into its own buffer,
    // valid only until the next call -- copy it into our ring right away.
    ue_split7_status_t rc = dev->read_symbol(dev, bufs, (uint16_t)fp->nb_antennas_rx);
    if (rc != UE_SPLIT7_SUCCESS) {
      // bufs[].re_buffer is not valid on failure -- don't copy it out.
      LOG_E(NR_PHY, "[fd-ue] read_symbol failed (%d) for slot %d symbol %d\n", (int)rc, slot_nr, sym);
      continue;
    }
    for (int a = 0; a < fp->nb_antennas_rx; a++) {
      memcpy(&UE->fd_rxdataF_ring[a][ring_symbol * fp->ofdm_symbol_size], bufs[a].re_buffer, fp->ofdm_symbol_size * sizeof(c16_t));
    }
    if (sym == 0)
      first_ts = bufs[0].meta.timestamp_samples;
  }
  return first_ts;
}

typedef struct {
  PHY_VARS_NR_UE *UE;
  ue_split7_sync_result_t sync_result;
  volatile bool sync_done;
} fd_sync_ctx_t;

static void fd_sync_cb(ue_split7_device_t *dev, ue_split7_status_t status, const ue_split7_sync_result_t *result, void *userdata)
{
  (void)dev;
  fd_sync_ctx_t *ctx = (fd_sync_ctx_t *)userdata;

  if (status == UE_SPLIT7_SUCCESS && result) {
    ctx->sync_result = *result;
    PHY_VARS_NR_UE *UE = ctx->UE;
    NR_DL_FRAME_PARMS *fp = &UE->frame_parms;

    UE->common_vars.freq_offset = (int)result->freq_offset_hz;

    // nr_fill_rx_indication()'s FAPI_NR_RX_PDU_TYPE_SSB case reads these off
    // ue->frame_parms directly, so they must be set before dl_indication below.
    fp->Nid_cell = result->physical_cell_id;
    fp->ssb_start_subcarrier = result->ssb_start_subcarrier;
    fp->half_frame_bit = (uint8_t)result->half_frame_bit;
    fp->ssb_index = result->best_ssb_index;
    UE->symbol_offset = (uint16_t)result->symbol_offset;

    UE->is_synchronized = 1;

    LOG_A(NR_PHY,
          "[fd-ue] Cell sync'd: PCI %u, timing_offset %lld samples, "
          "freq_offset %d Hz, RSRP %.1f dBm\n",
          result->physical_cell_id,
          (long long)result->timing_offset_samples,
          (int)result->freq_offset_hz,
          result->ssb_rsrp_dbm);

    // Pushes onto mac->input_nf, which UE_fd_thread's post-sync pullNotifiedFIFO()
    // below waits on; without this it blocks forever after a successful decode.
    if (result->mib_decoded) {
      fapiPbch_t pbch_result;
      memset(&pbch_result, 0, sizeof(pbch_result));
      memcpy(pbch_result.decoded_output, result->mib_payload, sizeof(pbch_result.decoded_output));
      pbch_result.xtra_byte = result->mib_additional_bits;

      UE_nr_rxtx_proc_t dummy_proc = {0};
      nr_downlink_indication_t dl_indication;
      fapi_nr_rx_indication_t rx_ind = {0};
      nr_fill_dl_indication(&dl_indication, NULL, &rx_ind, &dummy_proc, UE, NULL);
      nr_fill_rx_indication(&rx_ind, FAPI_NR_RX_PDU_TYPE_SSB, UE, 0, 0, NULL, &dummy_proc, &pbch_result);

      if (UE->if_inst && UE->if_inst->dl_indication)
        UE->if_inst->dl_indication(&dl_indication);
    }
  } else {
    ctx->UE->is_synchronized = 0;
    LOG_W(NR_PHY, "[fd-ue] Sync failed (status %d)\n", (int)status);
  }
  ctx->sync_done = true;
}

// TX reuses nr-ue.c's processSlotTX()/start_process_slot_tx() as-is: RU_write()
// already no-ops when writeBlockSize == 0, which the call site below sets.
typedef struct {
  PHY_VARS_NR_UE *UE;
  ue_split7_device_t *dev;
} fd_thread_args_t;

void *UE_fd_thread(void *arg)
{
  fd_thread_args_t *init = (fd_thread_args_t *)arg;
  PHY_VARS_NR_UE *UE = init->UE;
  ue_split7_device_t *dev = init->dev;
  free(init);

  NR_DL_FRAME_PARMS *fp = &UE->frame_parms;
  const int nb_slot_frame = fp->slots_per_frame;
  const int duration_rx_to_tx = NR_UE_CAPABILITY_SLOT_RX_TO_TX;

  UE->is_synchronized = 0;
  InitSinLUT();

  // Sized to 2 frames so a lagging DL actor can't be overwritten before it reads.
  const uint32_t ring_symbols = 2 * (uint32_t)nb_slot_frame * (uint32_t)fp->symbols_per_slot;
  c16_t *rxdataF_ring[8];
  for (int a = 0; a < fp->nb_antennas_rx; a++) {
    rxdataF_ring[a] = aligned_alloc(32, ring_symbols * fp->ofdm_symbol_size * sizeof(c16_t));
    AssertFatal(rxdataF_ring[a], "[fd-ue] OOM: rxdataF_ring\n");
  }
  UE->fd_rxdataF_ring = rxdataF_ring;

  // Heap-allocated: must stay valid for any TX actor still queued after this thread exits.
  fd_tx_ctx_t *tx_ctx = malloc(sizeof(*tx_ctx));
  AssertFatal(tx_ctx, "[fd-ue] OOM: tx_ctx\n");
  tx_ctx->dev = dev;
  UE->fd_tx_cb = fd_tx_cb_impl;
  UE->fd_prach_tx_cb = fd_prach_tx_cb_impl;
  UE->fd_tx_cb_data = tx_ctx;

  fapi_nr_config_request_t *cfg = &UE->nrUE_config;
  NR_UE_MAC_INST_t *mac = get_mac_inst(UE->Mod_id);

  for (int i = 0; i < NUM_PROCESS_SLOT_TX_BARRIERS; i++)
    dynamic_barrier_init(&UE->process_slot_tx_barriers[i]);

  enum stream_status_e stream_status = STREAM_STATUS_UNSYNC;
  int absolute_slot = 0;
  int decoded_frame_rx = MAX_FRAME_NUMBER - 1;
  int frame_rx_prev = -1; // reset alongside decoded_frame_rx on every (re-)sync
  int64_t hfn_rx_wraps = 0;
  int tx_wait_for_dlsch[NR_MAX_SLOTS_PER_FRAME];
  memset(tx_wait_for_dlsch, 0, sizeof(tx_wait_for_dlsch));
  bool stats_printed = false;

  uint32_t last_ta_samples = UINT32_MAX; // impossible value so the first push always fires

  fd_sync_ctx_t sync_ctx = {.UE = UE, .sync_done = false};

  // Per-frame RX/TX drift correction, applied via dev->adjust_rx_timing() instead
  // of nr-ue.c's readBlockSize/writeBlockSize; without it PBCH/PDCCH decode
  // intermittently fails as timing slowly drifts off the true channel timing.
  int shiftForNextFrame = 0;
  UE->max_pos_acc = get_nrUE_params()->time_sync_I
                        ? get_nrUE_params()->ntn_init_time_drift * 1e-6 * fp->samples_per_frame / get_nrUE_params()->time_sync_I
                        : 0;

  while (!oai_exit) {
    // Synchronization phase
    if (!UE->is_synchronized) {
      sync_ctx.sync_done = false;

      ue_split7_sync_config_t sync_cfg;
      memset(&sync_cfg, 0, sizeof(sync_cfg));
      sync_cfg.arfcn = (uint32_t)fp->ssb_start_subcarrier; // proxy for ARFCN
      sync_cfg.scs_khz = (uint16_t)(fp->subcarrier_spacing / 1000);
      sync_cfg.timeout_ms = (uint32_t)get_nrUE_params()->split7_sync_timeout_ms;
      sync_cfg.expected_pci = UE->target_Nid_cell;

      ue_split7_status_t rc = dev->start_sync(dev, &sync_cfg, &get_nrUE_params()->Tpool, fd_sync_cb, &sync_ctx);
      if (rc != UE_SPLIT7_SUCCESS) {
        LOG_E(NR_PHY, "[fd-ue] start_sync failed (%d); retrying\n", (int)rc);
        usleep(100000);
        continue;
      }

      while (!sync_ctx.sync_done && !oai_exit)
        usleep(1000);

      if (!UE->is_synchronized)
        continue;

      // sync_task_func() already snapped circ_read_idx to slot 0 symbol 0 of the
      // SSB's frame, so the next read_symbol() is already aligned there.
      {
        const int slot_in_frame = 0;

        notifiedFIFO_elt_t *elt = pullNotifiedFIFO(&mac->input_nf);
        AssertFatal(elt, "[fd-ue] FIFO error waiting for MIB\n");
        process_msg_rcc_to_mac(NotifiedFifoData(elt), UE->Mod_id);
        delNotifiedFIFO_elt(elt);
        // The blind search can take real wall-clock time, during which
        // circ_read_idx keeps advancing on the read thread; frames_since_capture
        // is that same advance in SFN units, so bookkeeping tracks the frame the
        // device is actually sitting on, not the one the MIB was decoded from.
        // Otherwise the first PRACH TX timestamp is stale by however long the
        // search took, and gets silently dropped as expired.
        decoded_frame_rx = (mac->mib_frame + sync_ctx.sync_result.frames_since_capture) % MAX_FRAME_NUMBER;

        ue_split7_status_t seed_rc = dev->seed_slot_tracking(dev, (uint32_t)decoded_frame_rx);
        if (seed_rc != UE_SPLIT7_SUCCESS)
          LOG_E(NR_PHY, "[fd-ue] seed_slot_tracking failed (%d)\n", (int)seed_rc);
        frame_rx_prev = -1;
        hfn_rx_wraps = 0;

        LOG_A(NR_PHY,
              "[fd-ue] Aligned: frame=%d slot_in_frame=%d "
              "(mib_frame=%d frames_since_capture=%u)\n",
              decoded_frame_rx,
              slot_in_frame,
              mac->mib_frame,
              sync_ctx.sync_result.frames_since_capture);
      }

      /* Push the initial TA (N_TA_offset only; MAC TA = 0 after sync). */
      UE->N_TA_offset = determine_N_TA_offset(UE);
      last_ta_samples = (uint32_t)UE->N_TA_offset;
      ue_split7_status_t ta_rc = dev->set_timing_advance(dev, last_ta_samples);
      if (ta_rc != UE_SPLIT7_SUCCESS)
        LOG_E(NR_PHY, "[fd-ue] initial set_timing_advance failed (%d)\n", (int)ta_rc);

      stream_status = STREAM_STATUS_UNSYNC;
      memset(tx_wait_for_dlsch, 0, sizeof(tx_wait_for_dlsch));
      for (int i = 0; i < NUM_PROCESS_SLOT_TX_BARRIERS; i++)
        dynamic_barrier_reset(&UE->process_slot_tx_barriers[i]);
      continue;
    }

    // MAC-triggered re-sync request (handover / re-establish)
    if (UE->synch_request.received_synch_request == 1) {
      /* Copy target PCI so the next start_sync searches the right cell. */
      UE->target_Nid_cell = UE->synch_request.synch_req.target_Nid_cell;

      /* Update SSB start subcarrier — it changes on inter-SSB-offset HOs. */
      {
        const fapi_nr_config_request_t *cfg = &UE->nrUE_config;
        int new_ssb_sc = nr_get_ssb_start_sc(fp->numerology_index,
                                             cfg->ssb_table.ssb_offset_point_a,
                                             cfg->ssb_table.ssb_subcarrier_offset,
                                             fp->freq_range);
        if (new_ssb_sc != fp->ssb_start_subcarrier) {
          LOG_I(NR_PHY, "[fd-ue] SSB subcarrier changed %d→%d on re-sync\n", fp->ssb_start_subcarrier, new_ssb_sc);
          fp->ssb_start_subcarrier = new_ssb_sc;
        }
      }

      /* Stop any in-flight sync search (normally idle in connected state). */
      ue_split7_status_t stop_rc = dev->stop_sync(dev);
      if (stop_rc != UE_SPLIT7_SUCCESS)
        LOG_E(NR_PHY, "[fd-ue] stop_sync failed (%d)\n", (int)stop_rc);

      /* Flush in-flight PHY work and reset HARQ before leaving connected state. */
      for (int i = 0; i < get_nrUE_params()->num_dl_actors; i++)
        flush_actor(UE->dl_actors + i);
      for (int i = 0; i < get_nrUE_params()->num_ul_actors; i++)
        flush_actor(UE->ul_actors + i);
      clean_UE_harq(UE);

      UE->is_synchronized = 0;
      UE->synch_request.received_synch_request = 0;
      continue;
    }

    // The Low-PHY owns the real sample clock, so block for its authoritative
    // frame/slot rather than free-running a host-side tick that could drift.
    uint32_t frame_number;
    uint16_t slot_number;
    ue_split7_status_t wns_rc = dev->wait_next_slot(dev, &frame_number, &slot_number);
    if (wns_rc != UE_SPLIT7_SUCCESS) {
      // frame_number/slot_number are left uninitialized on failure (e.g. the device
      // was stopped concurrently) -- must not fall through into the per-slot logic
      // below with garbage values.
      if (!oai_exit)
        LOG_W(NR_PHY, "[fd-ue] wait_next_slot failed (%d)\n", (int)wns_rc);
      usleep(1000);
      continue;
    }
    time_manager_iq_samples(1, nb_slot_frame * 100);

    const int slot_nr = (int)slot_number;
    const int frame_rx = (int)frame_number;

    // Reconstructs a monotonic absolute_slot for the TX-lead/HFN math below;
    // RX identity itself always comes straight from wait_next_slot() above.
    if (frame_rx < frame_rx_prev)
      hfn_rx_wraps++;
    frame_rx_prev = frame_rx;
    absolute_slot = (int)((hfn_rx_wraps * MAX_FRAME_NUMBER + frame_rx) * nb_slot_frame + slot_nr);

    // Must happen before fd_read_slot() so it takes effect on this slot's own read.
    if (slot_nr == nb_slot_frame - 1) {
      ue_split7_status_t adj_rc = dev->adjust_rx_timing(dev, shiftForNextFrame);
      if (adj_rc != UE_SPLIT7_SUCCESS)
        LOG_E(NR_PHY, "[fd-ue] adjust_rx_timing failed (%d)\n", (int)adj_rc);
      shiftForNextFrame = -(int)round(UE->max_pos_acc * get_nrUE_params()->time_sync_I);
    }

    const openair0_timestamp_t rx_ts = fd_read_slot(dev, UE, fp, frame_rx, slot_nr);

    nr_rxtx_thread_data_t curMsg = {0};
    curMsg.UE = UE;
    curMsg.proc.nr_slot_rx = slot_nr;
    curMsg.proc.nr_slot_tx = (absolute_slot + duration_rx_to_tx) % nb_slot_frame;
    curMsg.proc.frame_rx = frame_rx;
    curMsg.proc.frame_tx = ((absolute_slot + duration_rx_to_tx) / nb_slot_frame) % MAX_FRAME_NUMBER;
    curMsg.proc.hfn_rx = (absolute_slot / nb_slot_frame) / MAX_FRAME_NUMBER;
    curMsg.proc.hfn_tx = ((absolute_slot + duration_rx_to_tx) / nb_slot_frame) / MAX_FRAME_NUMBER;

    if (UE->received_config_request) {
      curMsg.proc.rx_slot_type = nr_ue_slot_select(cfg, curMsg.proc.nr_slot_rx);
      curMsg.proc.tx_slot_type = nr_ue_slot_select(cfg, curMsg.proc.nr_slot_tx);
    } else {
      curMsg.proc.rx_slot_type = NR_DOWNLINK_SLOT;
      curMsg.proc.tx_slot_type = NR_DOWNLINK_SLOT;
    }

    // No TA/RX-to-TX lead added: the Low-PHY applies both itself.
    curMsg.proc.timestamp_tx = rx_ts;

    if (curMsg.proc.nr_slot_rx == 0)
      nr_ue_rrc_timer_trigger(UE->Mod_id, curMsg.proc.hfn_rx, curMsg.proc.frame_rx, curMsg.proc.gNB_id);

    /* DL processing: launch async into dl_actors, same as UE_thread. */
    notifiedFIFO_elt_t *newRx = newNotifiedFIFO_elt(sizeof(nr_rxtx_thread_data_t), curMsg.proc.nr_slot_tx, NULL, UE_dl_processing);
    nr_rxtx_thread_data_t *curMsgRx = NotifiedFifoData(newRx);
    *curMsgRx = (nr_rxtx_thread_data_t){.proc = curMsg.proc, .UE = UE};
    int dl_preproc_ret = UE_dl_preprocessing(UE, &curMsgRx->proc, tx_wait_for_dlsch, &curMsgRx->phy_data, &stats_printed);
    // Fresh PBCH timing measurement supersedes the drift-only estimate above.
    if (dl_preproc_ret != INT_MAX)
      shiftForNextFrame = dl_preproc_ret;

    uint32_t new_ta = (uint32_t)(UE->N_TA_offset + UE->timing_advance);
    if (new_ta != last_ta_samples) {
      ue_split7_status_t ta_rc = dev->set_timing_advance(dev, new_ta);
      if (ta_rc != UE_SPLIT7_SUCCESS)
        LOG_E(NR_PHY, "[fd-ue] set_timing_advance failed (%d)\n", (int)ta_rc);
      last_ta_samples = new_ta;
    }

    if (get_nrUE_params()->num_dl_actors > 0)
      pushNotifiedFIFO(&UE->dl_actors[curMsg.proc.nr_slot_rx % get_nrUE_params()->num_dl_actors].fifo, newRx);
    else
      newRx->processingFunc(curMsgRx);

    // writeBlockSize = 0 below makes processSlotTX()'s RU_write() a no-op.
    notifiedFIFO_elt_t *newTx = newNotifiedFIFO_elt(sizeof(nr_rxtx_thread_data_t), 0, 0, processSlotTX);
    nr_rxtx_thread_data_t *curMsgTx = NotifiedFifoData(newTx);
    memset(curMsgTx, 0, sizeof(*curMsgTx));
    curMsgTx->proc = curMsg.proc;
    curMsgTx->writeBlockSize = 0; /* no RU_write */
    curMsgTx->UE = UE;

    const int slot_tx = curMsgTx->proc.nr_slot_tx;
    const int saf = slot_tx + curMsgTx->proc.frame_tx * nb_slot_frame;
    const int next_saf = absolute_slot + duration_rx_to_tx + 1;
    const int wait_prev = (stream_status == STREAM_STATUS_SYNCED) ? 1 : 0;

    curMsgTx->next_barrier = &UE->process_slot_tx_barriers[next_saf % NUM_PROCESS_SLOT_TX_BARRIERS];
    dynamic_barrier_update(&UE->process_slot_tx_barriers[saf % NUM_PROCESS_SLOT_TX_BARRIERS],
                           tx_wait_for_dlsch[slot_tx] + wait_prev,
                           start_process_slot_tx,
                           newTx);

    stream_status = STREAM_STATUS_SYNCED;
    tx_wait_for_dlsch[slot_tx] = 0;
  }

  // Null the callbacks before freeing tx_ctx so no queued actor calls into it.
  UE->fd_rxdataF_ring = NULL;
  UE->fd_tx_cb = NULL;
  UE->fd_prach_tx_cb = NULL;
  UE->fd_tx_cb_data = NULL;
  free(tx_ctx);

  for (int a = 0; a < fp->nb_antennas_rx; a++)
    free(rxdataF_ring[a]);

  // This thread owns dev for its whole lifetime.
  if (dev->stop)
    dev->stop(dev);
  ue_split7_device_free(dev);

  LOG_W(NR_PHY, "[fd-ue] main thread ending\n");
  return NULL;
}

// Called instead of init_NR_UE_threads() when a split7 device is configured.
void init_NR_UE_fd_threads(PHY_VARS_NR_UE *UE, ue_split7_device_t *dev)
{
  fd_thread_args_t *args = malloc(sizeof(*args));
  AssertFatal(args, "[fd-ue] OOM\n");
  args->UE = UE;
  args->dev = dev;

  char name[32];
  snprintf(name, sizeof(name), "UEfd_%d", UE->Mod_id);
  threadCreate(&UE->main_thread, UE_fd_thread, args, name, -1, OAI_PRIORITY_RT_MAX);

  if (!IS_SOFTMODEM_NOSTATS) {
    snprintf(name, sizeof(name), "L1_UE_stats_fd_%d", UE->Mod_id);
    threadCreate(&UE->stat_thread, nrL1_UE_stats_thread, UE, name, -1, OAI_PRIORITY_RT_LOW);
  }
}
