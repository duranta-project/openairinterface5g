/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * UE main thread for Split 7.1 (frequency-domain I/O): mirrors nr-ue.c but
 * replaces time-domain RF I/O with ue_split7_device_t calls. Two PHY hooks
 * bypass FFT/IFFT: fd_rxdataF_ring (nr_slot_fep() memcpys from it instead of
 * computing the DFT) and fd_tx_cb/fd_tx_cb_data (called from
 * phy_procedures_nrUE_TX() in place of nr_tx_rotation_and_ofdm_mod()).
 *
 * ONE thread here drives ALL configured UE instances against ONE shared
 * split7 device (one physical RF chain) -- not one thread+device per
 * instance. Each instance keeps its own independent RRC/MAC/PHY state (own
 * sync progress, own dl_actors/ul_actors, own HARQ) and registers its own
 * "client" with the device (see register_client() in ue_split7_interface.h);
 * the device internally sums every synced client's UL REs into one combined
 * transmission per symbol, since multiple co-located UEs' signals literally
 * sum in the air before reaching one antenna. DL/RX is genuinely shared (one
 * downlink signal every instance observes identically), so wait_next_slot()/
 * read_symbols() are called exactly once per slot on behalf of all instances,
 * not once per instance.
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
  uint32_t client_id;
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
  params.frame_number = (uint32_t)frame;
  params.slot_number = slot;
  params.symbol_number = 0;
  params.repetition_count = copies;
  params.antenna_port = 0;

  ue_split7_status_t rc = ctx->dev->write_prach(ctx->dev, ctx->client_id, &params);
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
  const NR_DL_FRAME_PARMS *fp = &ue->frame_parms;

  // generate_nr_prach() detects ue->fd_prach_tx_cb and routes the preamble to
  // write_prach() instead of the time-domain path; txdataF/was_symbol_used are
  // meaningless here. write_prach() is fire-and-forget and doesn't check in on
  // its own (see ue_split7_interface.h) -- with multiple clients sharing the
  // device, this client still owes every symbol of this slot a check-in (via
  // skip_symbols()), or whichever other client's write_symbols() would
  // complete the combine set for these symbols hangs forever waiting for it.
  NR_UE_PRACH *prach_var = ue->prach_vars[proc->gNB_id];
  if (prach_var->active) {
    nr_ue_prach_procedures(ue, proc, NULL);
    ue_split7_status_t rc = ctx->dev->skip_symbols(ctx->dev,
                                                   ctx->client_id,
                                                   (uint32_t)proc->frame_tx,
                                                   (uint16_t)proc->nr_slot_tx,
                                                   0,
                                                   (uint8_t)fp->symbols_per_slot);
    if (rc != UE_SPLIT7_SUCCESS)
      LOG_E(NR_PHY, "[fd-ue] skip_symbols (PRACH slot) failed (%d) for slot %d\n", (int)rc, proc->nr_slot_tx);
    return;
  }

  // Every symbol of the slot must check in exactly once: write_symbols() for a
  // contiguous run with real data, skip_symbols() for a run without -- an
  // "unused" run can't be silently dropped, since another client sharing the
  // device may be waiting on this client's check-in to complete its own
  // combine set for these same symbols.
  // txdataF[a] is already symbol-major/contiguous across the whole slot, matching
  // write_symbols()'s buffer convention.
  int sym = 0;
  while (sym < fp->symbols_per_slot) {
    const bool used = was_symbol_used[sym];
    const int run_start = sym;
    do {
      sym++;
    } while (sym < fp->symbols_per_slot && was_symbol_used[sym] == used);
    const int num_symbols = sym - run_start;

    ue_split7_status_t rc;
    if (used) {
      c16_t *bufs[8];
      for (int a = 0; a < nb_ant_tx; a++)
        bufs[a] = &txdataF[a][run_start * (int)fp->ofdm_symbol_size];

      rc = ctx->dev->write_symbols(ctx->dev,
                                   ctx->client_id,
                                   (uint32_t)proc->frame_tx,
                                   (uint16_t)proc->nr_slot_tx,
                                   (uint8_t)run_start,
                                   (uint8_t)num_symbols,
                                   bufs,
                                   (uint16_t)nb_ant_tx);
    } else {
      rc = ctx->dev->skip_symbols(ctx->dev,
                                  ctx->client_id,
                                  (uint32_t)proc->frame_tx,
                                  (uint16_t)proc->nr_slot_tx,
                                  (uint8_t)run_start,
                                  (uint8_t)num_symbols);
    }
    if (rc != UE_SPLIT7_SUCCESS)
      LOG_E(NR_PHY,
            "[fd-ue] write/skip_symbols failed (%d) for slot %d symbols [%d..%d)\n",
            (int)rc,
            proc->nr_slot_tx,
            run_start,
            sym);
  }
}

// Ring index must match nr_slot_fep()'s read-back formula (slot_fep_nr.c):
// ((frame % 2) * slots_per_frame + slot) * symbols_per_slot + symbol
// Shared across every UE instance -- called once per slot by the orchestrator,
// not once per instance (DL is the same signal for all of them).
static ue_split7_status_t fd_read_slot(ue_split7_device_t *dev, const NR_DL_FRAME_PARMS *fp, uint32_t frame, int slot_nr)
{
  ue_split7_status_t rc = dev->read_symbols(dev,
                                            frame,
                                            (uint16_t)slot_nr,
                                            0,
                                            (uint8_t)fp->symbols_per_slot,
                                            NULL,
                                            (uint16_t)fp->nb_antennas_rx);
  if (rc != UE_SPLIT7_SUCCESS) {
    LOG_E(NR_PHY, "[fd-ue] read_symbols failed (%d) for frame %u slot %d\n", (int)rc, frame, slot_nr);
  }
  return rc;
}

typedef struct {
  ue_split7_status_t status;
  ue_split7_sync_result_t sync_result;
  volatile bool sync_done;
} fd_sync_ctx_t;

// Device-level callback: stores the result for the orchestrator to apply to
// whichever UE instance triggered this search (see fd_apply_sync_result()) --
// unlike the single-client design this started as, it can't reach into a
// specific UE's state directly, since the device has no notion of "whose"
// search this is (sync is a shared, cell-wide resource, not a per-client one).
static void fd_sync_cb(ue_split7_device_t *dev, ue_split7_status_t status, const ue_split7_sync_result_t *result, void *userdata)
{
  (void)dev;
  fd_sync_ctx_t *ctx = (fd_sync_ctx_t *)userdata;
  ctx->status = status;
  if (status == UE_SPLIT7_SUCCESS && result)
    ctx->sync_result = *result;
  ctx->sync_done = true;
}

// Applies a successful device-level sync result to one UE instance's own
// RRC/MAC/PHY state -- kept separate from fd_sync_cb() since each instance
// needs this run independently (each instance does its own start_sync()
// call/PBCH decode; see the sync loop in UE_fd_thread() below for why this
// isn't shared across instances despite being the same cell).
static void fd_apply_sync_result(PHY_VARS_NR_UE *UE, const ue_split7_sync_result_t *result)
{
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
        "[fd-ue] UE %d cell sync'd: PCI %u, timing_offset %lld samples, "
        "freq_offset %d Hz, RSRP %.1f dBm\n",
        UE->Mod_id,
        result->physical_cell_id,
        (long long)result->timing_offset_samples,
        (int)result->freq_offset_hz,
        result->ssb_rsrp_dbm);

  // Pushes onto mac->input_nf, which the sync loop below's post-sync
  // pullNotifiedFIFO() waits on; without this it blocks forever after a
  // successful decode.
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
}

// Per-instance state: one of these per co-located UE instance, driven by the
// shared orchestrator loop in UE_fd_thread() below.
typedef struct {
  PHY_VARS_NR_UE *UE;
  uint32_t client_id; // from register_client(); stable for the instance's lifetime
  fd_tx_ctx_t *tx_ctx; // heap-allocated: must outlive any TX actor still queued
  // True from this instance's first successful seed_slot_tracking() call onward,
  // for the rest of the process's life -- the device's per-symbol combine set
  // (see ue_split7_interface.h) has expected this client's check-in ever since,
  // and keeps expecting it through a later re-sync (UE->is_synchronized toggling
  // back to 0 there is NOT the same as leaving the combine set -- clients only
  // ever join, never leave, so a re-syncing instance still owes every symbol a
  // check-in; see the keep-alive skip_symbols() call below for exactly that case).
  bool ever_synced;
  enum stream_status_e stream_status;
  int decoded_frame_rx;
  int tx_wait_for_dlsch[NR_MAX_SLOTS_PER_FRAME];
  bool stats_printed;
  uint32_t last_ta_samples;
  int shiftForNextFrame;
  fd_sync_ctx_t sync_ctx;
} fd_ue_instance_t;

typedef struct {
  PHY_VARS_NR_UE **UE_list;
  int num_ues;
  ue_split7_device_t *dev;
} fd_thread_args_t;

void *UE_fd_thread(void *arg)
{
  fd_thread_args_t *init = (fd_thread_args_t *)arg;
  PHY_VARS_NR_UE **UE_list = init->UE_list; // heap copy, owned by this thread now; freed at the end
  const int num_ues = init->num_ues;
  ue_split7_device_t *dev = init->dev;
  free(init);

  // Every instance must share the same numerology/cell (spatially co-located,
  // one antenna) -- instance 0's frame_parms stands in for all of them for
  // every shared (device-clock-level) computation below.
  NR_DL_FRAME_PARMS *fp = &UE_list[0]->frame_parms;
  const int nb_slot_frame = fp->slots_per_frame;
  const int duration_rx_to_tx = NR_UE_CAPABILITY_SLOT_RX_TO_TX;

  InitSinLUT();

  // The split7 device allocates and owns the 2-frame RX frequency-domain ring buffers.
  AssertFatal(dev->rx_fd_buffers, "[fd-ue] split7 device has no rx_fd_buffers allocated\n");

  fd_ue_instance_t *inst = calloc(num_ues, sizeof(*inst));
  AssertFatal(inst, "[fd-ue] OOM: inst[]\n");

  for (int u = 0; u < num_ues; u++) {
    PHY_VARS_NR_UE *UE = UE_list[u];
    inst[u].UE = UE;
    UE->is_synchronized = 0;
    UE->fd_rxdataF_ring = dev->rx_fd_buffers;

    ue_split7_status_t reg_rc = dev->register_client(dev, &inst[u].client_id);
    AssertFatal(reg_rc == UE_SPLIT7_SUCCESS, "[fd-ue] register_client failed for UE %d (%d)\n", u, (int)reg_rc);

    fd_tx_ctx_t *tx_ctx = malloc(sizeof(*tx_ctx));
    AssertFatal(tx_ctx, "[fd-ue] OOM: tx_ctx\n");
    tx_ctx->dev = dev;
    tx_ctx->client_id = inst[u].client_id;
    inst[u].tx_ctx = tx_ctx;
    UE->fd_tx_cb = fd_tx_cb_impl;
    UE->fd_prach_tx_cb = fd_prach_tx_cb_impl;
    UE->fd_tx_cb_data = tx_ctx;

    inst[u].stream_status = STREAM_STATUS_UNSYNC;
    inst[u].decoded_frame_rx = MAX_FRAME_NUMBER - 1;
    inst[u].last_ta_samples = UINT32_MAX; // impossible value so the first push always fires
    inst[u].sync_ctx.sync_done = false;

    for (int i = 0; i < NUM_PROCESS_SLOT_TX_BARRIERS; i++)
      dynamic_barrier_init(&UE->process_slot_tx_barriers[i]);

    // Per-frame RX/TX drift correction (see the shared adjust_rx_timing_offset()
    // call below) is only ever driven by instance 0 -- one shared RF clock, one
    // shared Timing Advance for every co-located instance -- so only instance 0's
    // max_pos_acc is actually consulted, but it's harmless to set for all of them.
    UE->max_pos_acc = get_nrUE_params()->time_sync_I
                          ? get_nrUE_params()->ntn_init_time_drift * 1e-6 * fp->samples_per_frame / get_nrUE_params()->time_sync_I
                          : 0;
  }

  while (!oai_exit) {
    // Synchronization phase: drive any not-yet-synchronized instance. Device-level
    // cell search is shared/serialized (only one start_sync() at a time; see
    // ue_split7_device.c), so instances needing it take turns here -- each still
    // runs its own full search/PBCH decode (simpler and safer than trying to
    // reuse one instance's result for another, even though in practice they're
    // the same cell) rather than a device-level result being broadcast.
    bool did_sync_work = false;
    for (int u = 0; u < num_ues; u++) {
      PHY_VARS_NR_UE *UE = inst[u].UE;
      if (UE->is_synchronized)
        continue;
      did_sync_work = true;

      inst[u].sync_ctx.sync_done = false;

      ue_split7_sync_config_t sync_cfg;
      memset(&sync_cfg, 0, sizeof(sync_cfg));
      sync_cfg.arfcn = (uint32_t)UE->frame_parms.ssb_start_subcarrier; // proxy for ARFCN
      sync_cfg.scs_khz = (uint16_t)(UE->frame_parms.subcarrier_spacing / 1000);
      sync_cfg.timeout_ms = (uint32_t)get_nrUE_params()->split7_sync_timeout_ms;
      sync_cfg.expected_pci = UE->target_Nid_cell;

      ue_split7_status_t rc = dev->start_sync(dev, &sync_cfg, &get_nrUE_params()->Tpool, fd_sync_cb, &inst[u].sync_ctx);
      if (rc != UE_SPLIT7_SUCCESS) {
        LOG_E(NR_PHY, "[fd-ue] start_sync failed (%d) for UE %d; retrying\n", (int)rc, u);
        usleep(100000);
        continue;
      }

      while (!inst[u].sync_ctx.sync_done && !oai_exit)
        usleep(1000);

      if (inst[u].sync_ctx.status != UE_SPLIT7_SUCCESS)
        continue;

      fd_apply_sync_result(UE, &inst[u].sync_ctx.sync_result);

      // sync_task_func() already snapped circ_read_idx to slot 0 symbol 0 of the
      // SSB's frame, so the next read_symbols() is already aligned there.
      {
        const int slot_in_frame = 0;
        NR_UE_MAC_INST_t *mac = get_mac_inst(UE->Mod_id);

        notifiedFIFO_elt_t *elt = pullNotifiedFIFO(&mac->input_nf);
        AssertFatal(elt, "[fd-ue] FIFO error waiting for MIB (UE %d)\n", u);
        process_msg_rcc_to_mac(NotifiedFifoData(elt), UE->Mod_id);
        delNotifiedFIFO_elt(elt);
        // The blind search can take real wall-clock time, during which
        // circ_read_idx keeps advancing on the read thread; frames_since_capture
        // is that same advance in SFN units, so bookkeeping tracks the frame the
        // device is actually sitting on, not the one the MIB was decoded from.
        // Otherwise the first PRACH TX timestamp is stale by however long the
        // search took, and gets silently dropped as expired.
        inst[u].decoded_frame_rx = (mac->mib_frame + inst[u].sync_ctx.sync_result.frames_since_capture) % MAX_FRAME_NUMBER;

        ue_split7_status_t seed_rc =
            dev->seed_slot_tracking(dev, inst[u].client_id, (uint32_t)inst[u].decoded_frame_rx);
        if (seed_rc != UE_SPLIT7_SUCCESS)
          LOG_E(NR_PHY, "[fd-ue] seed_slot_tracking failed (%d) for UE %d\n", (int)seed_rc, u);
        else
          inst[u].ever_synced = true; // joins the device's per-symbol combine set from here on, permanently

        LOG_A(NR_PHY,
              "[fd-ue] UE %d aligned: frame=%d slot_in_frame=%d "
              "(mib_frame=%d frames_since_capture=%u)\n",
              u,
              inst[u].decoded_frame_rx,
              slot_in_frame,
              mac->mib_frame,
              inst[u].sync_ctx.sync_result.frames_since_capture);
      }

      /* Push the initial TA (N_TA_offset only; MAC TA = 0 after sync). TA is one
       * shared value for every co-located instance (one shared RF clock/antenna --
       * see max_pos_acc's comment above), so only instance 0's measurement is
       * ever pushed to the device; a later instance's own (independently
       * measured, generally different) N_TA_offset would otherwise clobber
       * instance 0's already-converged TA with a fresh, uncorrelated value. */
      UE->N_TA_offset = determine_N_TA_offset(UE);
      inst[u].last_ta_samples = (uint32_t)UE->N_TA_offset;
      if (u == 0) {
        ue_split7_status_t ta_rc = dev->set_timing_advance(dev, inst[u].last_ta_samples);
        if (ta_rc != UE_SPLIT7_SUCCESS)
          LOG_E(NR_PHY, "[fd-ue] initial set_timing_advance failed (%d) for UE %d\n", (int)ta_rc, u);
      }

      inst[u].stream_status = STREAM_STATUS_UNSYNC;
      memset(inst[u].tx_wait_for_dlsch, 0, sizeof(inst[u].tx_wait_for_dlsch));
      for (int i = 0; i < NUM_PROCESS_SLOT_TX_BARRIERS; i++)
        dynamic_barrier_reset(&UE->process_slot_tx_barriers[i]);
    }
    if (did_sync_work)
      continue;

    // MAC-triggered re-sync request (handover / re-establish), per instance.
    bool did_resync_work = false;
    for (int u = 0; u < num_ues; u++) {
      PHY_VARS_NR_UE *UE = inst[u].UE;
      if (UE->synch_request.received_synch_request != 1)
        continue;
      did_resync_work = true;

      /* Copy target PCI so the next start_sync searches the right cell. */
      UE->target_Nid_cell = UE->synch_request.synch_req.target_Nid_cell;

      /* Update SSB start subcarrier — it changes on inter-SSB-offset HOs. */
      {
        NR_DL_FRAME_PARMS *ue_fp = &UE->frame_parms;
        const fapi_nr_config_request_t *cfg = &UE->nrUE_config;
        int new_ssb_sc = nr_get_ssb_start_sc(ue_fp->numerology_index,
                                             cfg->ssb_table.ssb_offset_point_a,
                                             cfg->ssb_table.ssb_subcarrier_offset,
                                             ue_fp->freq_range);
        if (new_ssb_sc != ue_fp->ssb_start_subcarrier) {
          LOG_I(NR_PHY, "[fd-ue] UE %d SSB subcarrier changed %d→%d on re-sync\n", u, ue_fp->ssb_start_subcarrier, new_ssb_sc);
          ue_fp->ssb_start_subcarrier = new_ssb_sc;
        }
      }

      /* Stop any in-flight sync search (normally idle in connected state). */
      ue_split7_status_t stop_rc = dev->stop_sync(dev);
      if (stop_rc != UE_SPLIT7_SUCCESS)
        LOG_E(NR_PHY, "[fd-ue] stop_sync failed (%d) for UE %d\n", (int)stop_rc, u);

      /* Flush in-flight PHY work and reset HARQ before leaving connected state. */
      for (int i = 0; i < get_nrUE_params()->num_dl_actors; i++)
        flush_actor(UE->dl_actors + i);
      for (int i = 0; i < get_nrUE_params()->num_ul_actors; i++)
        flush_actor(UE->ul_actors + i);
      clean_UE_harq(UE);

      UE->is_synchronized = 0;
      UE->synch_request.received_synch_request = 0;
    }
    if (did_resync_work)
      continue;

    // The Low-PHY owns the real sample clock, so block for its authoritative
    // frame/slot rather than free-running a host-side tick that could drift.
    // Shared across every instance -- called once per slot, not once per instance.
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
    const uint32_t abs_frame_rx = frame_number;
    const int frame_rx = (int)(abs_frame_rx % MAX_FRAME_NUMBER);

    const int absolute_slot = (int)(abs_frame_rx * nb_slot_frame + slot_nr);

    // wait_next_slot() already RX'd this slot's samples by the time it returns, so
    // this can no longer land before this slot's own read -- it takes effect
    // starting the first slot of the next frame's wait_next_slot() call instead.
    // adjust_rx_timing_offset() automatically shifts TX timing in lockstep with the
    // frame boundary. RX/TA are shared across every co-located instance, so only
    // instance 0's drift estimate is used -- see its assignment below.
    if (slot_nr == nb_slot_frame - 1) {
      ue_split7_status_t adj_rc = dev->adjust_rx_timing_offset(dev, inst[0].shiftForNextFrame);
      if (adj_rc != UE_SPLIT7_SUCCESS)
        LOG_E(NR_PHY, "[fd-ue] adjust_rx_timing_offset failed (%d)\n", (int)adj_rc);
      inst[0].shiftForNextFrame = -(int)round(inst[0].UE->max_pos_acc * get_nrUE_params()->time_sync_I);
    }

    fd_read_slot(dev, fp, (uint32_t)frame_rx, slot_nr);

    const int abs_slot_tx = absolute_slot + duration_rx_to_tx;
    const uint32_t abs_frame_tx = (uint32_t)(abs_slot_tx / nb_slot_frame);

    for (int u = 0; u < num_ues; u++) {
      PHY_VARS_NR_UE *UE = inst[u].UE;
      if (!UE->is_synchronized) {
        // Re-syncing (not never-synced -- see ever_synced's comment): this
        // instance's DL/UL processing is paused, but the device still expects
        // its check-in every symbol, forever, once joined. Keep it alive with
        // an empty contribution instead of stalling every other instance --
        // targeting the same TX slot (frame_tx/nr_slot_tx) the other, still-
        // synced instances' real write_symbols() calls target this iteration.
        if (inst[u].ever_synced) {
          const uint16_t nr_slot_tx = (uint16_t)(abs_slot_tx % nb_slot_frame);
          const uint32_t frame_tx = abs_frame_tx % MAX_FRAME_NUMBER;
          ue_split7_status_t rc =
              dev->skip_symbols(dev, inst[u].client_id, frame_tx, nr_slot_tx, 0, (uint8_t)fp->symbols_per_slot);
          if (rc != UE_SPLIT7_SUCCESS)
            LOG_E(NR_PHY, "[fd-ue] keep-alive skip_symbols failed (%d) for UE %d\n", (int)rc, u);
        }
        continue;
      }

      fapi_nr_config_request_t *cfg = &UE->nrUE_config;

      nr_rxtx_thread_data_t curMsg = {0};
      curMsg.UE = UE;
      curMsg.proc.nr_slot_rx = slot_nr;
      curMsg.proc.nr_slot_tx = abs_slot_tx % nb_slot_frame;
      curMsg.proc.frame_rx = frame_rx;
      curMsg.proc.frame_tx = (int)(abs_frame_tx % MAX_FRAME_NUMBER);
      curMsg.proc.hfn_rx = (int)(abs_frame_rx / MAX_FRAME_NUMBER);
      curMsg.proc.hfn_tx = (int)(abs_frame_tx / MAX_FRAME_NUMBER);

      if (UE->received_config_request) {
        curMsg.proc.rx_slot_type = nr_ue_slot_select(cfg, curMsg.proc.nr_slot_rx);
        curMsg.proc.tx_slot_type = nr_ue_slot_select(cfg, curMsg.proc.nr_slot_tx);
      } else {
        curMsg.proc.rx_slot_type = NR_DOWNLINK_SLOT;
        curMsg.proc.tx_slot_type = NR_DOWNLINK_SLOT;
      }

      // No TA/RX-to-TX lead added: the Low-PHY applies both itself.
      curMsg.proc.timestamp_tx = 0;

      if (curMsg.proc.nr_slot_rx == 0)
        nr_ue_rrc_timer_trigger(UE->Mod_id, curMsg.proc.hfn_rx, curMsg.proc.frame_rx, curMsg.proc.gNB_id);

      /* DL processing: launch async into dl_actors, same as UE_thread. */
      notifiedFIFO_elt_t *newRx = newNotifiedFIFO_elt(sizeof(nr_rxtx_thread_data_t), curMsg.proc.nr_slot_tx, NULL, UE_dl_processing);
      nr_rxtx_thread_data_t *curMsgRx = NotifiedFifoData(newRx);
      *curMsgRx = (nr_rxtx_thread_data_t){.proc = curMsg.proc, .UE = UE};
      int dl_preproc_ret =
          UE_dl_preprocessing(UE, &curMsgRx->proc, inst[u].tx_wait_for_dlsch, &curMsgRx->phy_data, &inst[u].stats_printed);
      // Fresh PBCH timing measurement supersedes the drift-only estimate above
      // (only meaningful for instance 0; see the shared adjust_rx_timing_offset() call).
      if (dl_preproc_ret != INT_MAX)
        inst[u].shiftForNextFrame = dl_preproc_ret;

      // TA is one shared value for the device's single RF chain (see the initial
      // push above and max_pos_acc's comment) -- only instance 0's updates are
      // ever pushed, or every other instance's own (uncorrelated) TA would keep
      // clobbering instance 0's already-converged value on essentially every slot.
      uint32_t new_ta = (uint32_t)(UE->N_TA_offset + UE->timing_advance);
      if (u == 0 && new_ta != inst[u].last_ta_samples) {
        ue_split7_status_t ta_rc = dev->set_timing_advance(dev, new_ta);
        if (ta_rc != UE_SPLIT7_SUCCESS)
          LOG_E(NR_PHY, "[fd-ue] set_timing_advance failed (%d) for UE %d\n", (int)ta_rc, u);
        inst[u].last_ta_samples = new_ta;
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
      const int wait_prev = (inst[u].stream_status == STREAM_STATUS_SYNCED) ? 1 : 0;

      curMsgTx->next_barrier = &UE->process_slot_tx_barriers[next_saf % NUM_PROCESS_SLOT_TX_BARRIERS];
      dynamic_barrier_update(&UE->process_slot_tx_barriers[saf % NUM_PROCESS_SLOT_TX_BARRIERS],
                             inst[u].tx_wait_for_dlsch[slot_tx] + wait_prev,
                             start_process_slot_tx,
                             newTx);

      inst[u].stream_status = STREAM_STATUS_SYNCED;
      inst[u].tx_wait_for_dlsch[slot_tx] = 0;
    }
  }

  for (int u = 0; u < num_ues; u++) {
    PHY_VARS_NR_UE *UE = inst[u].UE;
    // Null the callbacks before freeing tx_ctx so no queued actor calls into it.
    UE->fd_rxdataF_ring = NULL;
    UE->fd_tx_cb = NULL;
    UE->fd_prach_tx_cb = NULL;
    UE->fd_tx_cb_data = NULL;
    free(inst[u].tx_ctx);
  }
  free(inst);
  free(UE_list);

  // This thread owns dev for its whole lifetime.
  if (dev->stop)
    dev->stop(dev);
  ue_split7_device_free(dev);

  LOG_W(NR_PHY, "[fd-ue] main thread ending\n");
  return NULL;
}

// Called instead of init_NR_UE_threads() when a split7 device is configured.
// Starts ONE thread driving all num_ues instances against the ONE shared dev.
void init_NR_UE_fd_threads(PHY_VARS_NR_UE **UE_list, int num_ues, ue_split7_device_t *dev)
{
  fd_thread_args_t *args = malloc(sizeof(*args));
  AssertFatal(args, "[fd-ue] OOM\n");
  args->UE_list = malloc(sizeof(PHY_VARS_NR_UE *) * num_ues);
  AssertFatal(args->UE_list, "[fd-ue] OOM: UE_list copy\n");
  memcpy(args->UE_list, UE_list, sizeof(PHY_VARS_NR_UE *) * num_ues);
  args->num_ues = num_ues;
  args->dev = dev;

  // ONE processing thread drives every instance, so it hangs off instance 0's
  // main_thread handle -- there is only one of it now, shared. L1 stats
  // reporting stays per-instance (cheap, read-only, independent per UE), one
  // thread each, same as the single-client design.
  PHY_VARS_NR_UE *UE0 = UE_list[0];
  threadCreate(&UE0->main_thread, UE_fd_thread, args, "UEfd", -1, OAI_PRIORITY_RT_MAX);

  if (!IS_SOFTMODEM_NOSTATS) {
    for (int u = 0; u < num_ues; u++) {
      char name[32];
      snprintf(name, sizeof(name), "L1_UE_stats_fd_%d", UE_list[u]->Mod_id);
      threadCreate(&UE_list[u]->stat_thread, nrL1_UE_stats_thread, UE_list[u], name, -1, OAI_PRIORITY_RT_LOW);
    }
  }
}
