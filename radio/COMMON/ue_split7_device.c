/**
 * @file ue_split7_device.c
 * @brief Implementation of the UE-centric 7.1 functional split interface.
 */

#include "ue_split7_interface.h"
#include "common_lib.h"
#include "PHY/TOOLS/tools_defs.h"
#include "PHY/NR_REFSIG/pss_nr.h"
#include "common/utils/LOG/log.h"
#include "PHY/defs_nr_UE.h"
#include "PHY/impl_defs_nr.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "executables/nr-uesoftmodem.h"
#include "common/utils/nr/nr_common.h"
#include "common/utils/utils.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <time.h>
#include "common/utils/threadPool/thread-pool.h"

// Monotonic: UNSYNCED -> DL_SYNCED (read_symbols valid) -> UL_READY (write_symbols/
// skip_symbols/write_prach valid, indefinitely -- with multiple co-located clients
// (see UE_SPLIT7_MAX_CLIENTS) possibly at different points in their own connection
// lifecycle at once, PRACH and normal UL symbols must both stay available for the
// life of the device: a client transmitting regular UL data must never block a
// later-syncing client that still needs to PRACH. Never backward.
typedef enum {
  UE_SPLIT7_STATE_UNSYNCED = 0,
  UE_SPLIT7_STATE_DL_SYNCED = 1,
  UE_SPLIT7_STATE_UL_READY = 2,
} ue_split7_state_t;

// Hard cap on RX/TX antenna count: several hot-path functions (read_symbols's
// non-threaded fallback, write_symbols, sync_task_func) use fixed-size stack
// arrays sized to this bound and indexed directly by config.num_rx/tx_antennas,
// so ue_split7_configure() must reject any config exceeding it.
#define UE_SPLIT7_MAX_ANT 8

// Hard cap on the number of independent, spatially co-located UE instances
// ("clients") one device serves; see register_client()/seed_slot_tracking() in
// ue_split7_interface.h. synced_mask/tx_checked_in_mask/tx_expected_mask are all
// bitmasks over client ids, so this must not exceed 32.
#define UE_SPLIT7_MAX_CLIENTS 8

// Small fixed-depth queue of PRACH waveforms awaiting overlay onto whatever
// write_symbols()/skip_symbols()-combined symbol they land on in time; see
// write_prach()/ue_split7_overlay_prach().
#define UE_SPLIT7_MAX_PRACH_QUEUE 4

typedef struct {
  bool active;
  uint64_t abs_start_sample; // RF sample timestamp (post RX-to-TX lead, TA) the waveform starts at
  uint32_t cp_len; // one-time cyclic prefix length, samples[0..cp_len) in the buffer below
  uint32_t fft_size; // one repetition period, samples[cp_len..cp_len+fft_size) in the buffer below
  uint32_t total_samples; // cp_len + fft_size * repetition_count -- the full waveform's duration
  uint32_t emitted_samples; // how much of total_samples has already been streamed out
  int16_t *samples; // owned; ONE repetition only: [CP][one fft_size cycle], length (cp_len+fft_size)*2 int16s.
                    // Streamed out (with the fft_size portion cycled via modulo) symbol by symbol as
                    // ue_split7_overlay_prach() is called, rather than matched against a fixed absolute
                    // window -- robust to TA/anchor drift between when this was queued and when later,
                    // still-overlapping symbols actually get finalized (see overlay_prach's comment).
} ue_split7_prach_queue_entry_t;

typedef struct {
  openair0_device_t openair0_dev;
  openair0_config_t openair0_cfg;
  _Atomic uint32_t ta_samples; // written by set_timing_advance() (FD-UE thread),
                               // read by write_symbols()/write_prach() (dl_actors worker thread)
  bool is_started;
  _Atomic int state; // ue_split7_state_t

  // Intermediate buffers for FFT/IFFT processing
  int16_t *rx_time_buf; // intermediate time-domain buffer for read_symbols
  int16_t *tx_time_buf; // intermediate time-domain buffer for write_symbols
  uint32_t buf_size_samples;

  // Time-domain RX ring, addressed directly by RF sample timestamp (ring position ==
  // ts % circ_buf_size), not by an independently-tracked append count.
  int16_t **rx_circ_buf; // rx_circ_buf[channel][sample_index * 2] for complex samples
  uint32_t circ_buf_size; // size in complex samples
  _Atomic uint64_t latest_write_ts; // ts + n of the most recent ring write; read thread writes, others wait on it
  // Consumer-owned read cursor (absolute RF timestamp), touched only from the FD-UE
  // thread; the state acquire/release below (not atomicity) makes the sync task's seed visible.
  uint64_t next_read_ts;

  // Device-owned RX FD ring: read_symbols() DFTs directly into rx_fd_buf[antenna][ring_sym * fft_size]
  // Sized to rx_fd_ring_symbols * dev->config.fft_size elements.
  uint32_t rx_fd_ring_symbols;
  c16_t **rx_fd_buf; // rx_fd_buf[antenna][rx_fd_ring_symbols * fft_size]

  // Device-owned RX TD frame buffer: 2 frames worth of TD samples
  uint32_t rx_td_buf_samples;
  c16_t **rx_td_buf; // rx_td_buf[antenna][rx_td_buf_samples]

  // FFT completion tracking per symbol in FD space (rx_fd_ring_symbols elements)
  bool *symbol_fft_done;
  uint32_t *symbol_frame_number;
  pthread_mutex_t rx_mutex;

  // Device-owned TX TD frame buffer: 2 frames worth of TD samples, mirroring
  // rx_td_buf -- write_symbols() computes each call's (frame, slot, start_symbol)
  // offset into this buffer instead of malloc'ing/freeing a fresh one every call.
  // Protected by write_mutex (held across the fill, not just the trx_write_func()
  // call) since it's shared across calls the way rx_td_buf is across read_symbols().
  c16_t **tx_td_buf; // tx_td_buf[antenna][rx_td_buf_samples] -- same sizing as rx_td_buf

  // Read thread management
  pthread_t read_thread;
  atomic_bool read_running;

  // Broadcast by the read thread after every write so read_symbols()/wait_next_slot()
  // wake instead of busy-polling; the read thread itself never waits on this.
  pthread_mutex_t data_mutex;
  pthread_cond_t data_cond;

  // sync_task_func() owns its whole capture-retry loop internally (one
  // pushTpool() call runs to success/timeout), so no capture position is
  // shared across threads here.
  atomic_bool sync_active;
  // True for the duration of one sync_task_func() run; stop_sync() polls
  // this (not sync_active, which only requests cancellation) to know when
  // the task has actually stopped.
  atomic_bool sync_task_running;
  tpool_t *sync_pool;
  struct timespec sync_start_time;
  ue_split7_sync_config_t sync_cfg;
  ue_split7_sync_callback_t sync_cb;
  void *sync_user_data;

  pthread_mutex_t write_mutex;

  // Slot-timing tracker for wait_next_slot()/seed_slot_tracking(): frame/slot
  // reported by the NEXT call. The sample position its slot starts at is
  // next_read_ts itself (wait_next_slot() is the sole caller of
  // ue_split7_rx_time_domain() now, so there is only one RX cursor to track).
  // The sole authoritative walk; the host has no independent slot counter.
  bool slot_tracking_seeded;
  uint32_t next_frame_number;
  uint16_t next_slot_number;

  // (frame, slot) most recently primed by wait_next_slot()'s TD RX -- lets
  // read_symbols() tell "TD already resident, only FFT is needed" apart from
  // "no one has RX'd this slot yet, do it myself" (standalone/test callers that
  // never call wait_next_slot()). UINT32_MAX frame number = nothing primed yet.
  uint32_t td_rx_done_frame_number;
  uint16_t td_rx_done_slot_number;

  // Fixed reference point for write_symbols()'s TX timestamp derivation (see
  // ue_split7_ts_from_frame_slot_symbol()): set once by seed_slot_tracking() and
  // shifted in lockstep with next_read_ts by adjust_rx_timing(), unlike
  // next_frame_number/next_slot_number above, which wait_next_slot() continuously
  // advances/skips for its own RX cursor.
  uint32_t tx_anchor_frame_number;
  uint64_t tx_anchor_sample_pos;

  // Frequency offset (Hz) measured during last successful sync.
  // Applied to the RF LO so subsequent symbol reads are already corrected.
  int32_t last_freq_offset_hz;

  // Rotation tables (38.211 §5.3 symbol-phase correction) to use: config.frame_parms
  // when the host provided one (shares nr_init.c's exact tables), else &frame_parms,
  // a locally-derived fallback for standalone/unit-test use.
  const NR_DL_FRAME_PARMS *fp;
  NR_DL_FRAME_PARMS frame_parms;

  // RX FFT-window offset into the CP (nr_slot_fep()'s ISI-avoidance convention);
  // nonzero only when fp == config.frame_parms, since the fallback path samples
  // exactly at the symbol boundary instead.
  uint32_t rot_sample_offset;

  // RX/TX dynamic timing offset adjustment
  _Atomic int32_t rx_timing_offset;
  _Atomic int32_t tx_timing_offset;
  int16_t *prev_rx_buf[UE_SPLIT7_MAX_ANT];
  uint32_t prev_rx_buf_samples;

  // Absolute frame number of the slot most recently reported by wait_next_slot();
  // the reference point ue_split7_resolve_abs_frame() unwraps caller-supplied
  // frame numbers (standard 3GPP wrapped 0..1023, or already absolute -- either
  // works) against, per read_symbols()/write_symbols()/write_prach() call.
  uint32_t last_rx_abs_frame;
  uint16_t last_rx_slot;

  // ------------------ Multi-client UL combining ------------------
  // See register_client()/seed_slot_tracking()/write_symbols()/skip_symbols()
  // in ue_split7_interface.h. RX/DL (everything above) stays single-caller/
  // shared, unaffected by any of this.
  uint32_t num_clients; // also doubles as the next id register_client() hands out
  uint32_t synced_mask; // bit i set once client i has called seed_slot_tracking() at least once
  bool anchor_seeded; // true once tx_anchor_*/next_frame_number/last_rx_* have been seeded
                      // (by whichever client's seed_slot_tracking() call happens to be first)

  pthread_mutex_t tx_combine_mutex; // guards synced_mask, tx_checked_in_mask/tx_expected_mask, tx_fd_accum
  // Per ring position (rx_fd_ring_symbols elements, same indexing as symbol_fft_done):
  // tx_expected_mask[s] == 0 means nobody has touched this ring slot yet this
  // round; the first touch snapshots synced_mask into it and zeroes the
  // accumulator. Finalize (see ue_split7_finalize_tx_symbol()) runs -- and both
  // masks reset to 0 -- the instant tx_checked_in_mask[s] == tx_expected_mask[s].
  uint32_t *tx_checked_in_mask;
  uint32_t *tx_expected_mask;
  // Shared FD accumulator every client's write_symbols() sums its REs into;
  // sized/indexed exactly like rx_fd_buf but owned by num_tx_antennas.
  c16_t **tx_fd_accum;

  pthread_mutex_t prach_queue_mutex;
  ue_split7_prach_queue_entry_t prach_queue[UE_SPLIT7_MAX_PRACH_QUEUE];

  // Set whenever a nonzero rx_timing_offset trash/copy adjustment inside
  // ue_split7_rx_time_domain() is about to move next_read_ts by something
  // OTHER than exactly one slot's worth of samples -- the one place next_read_ts
  // and next_frame_number/next_slot_number can genuinely fall out of the
  // sample-position <-> (frame,slot) mapping the TX anchor (tx_anchor_frame_number/
  // tx_anchor_sample_pos, below) depends on. wait_next_slot()'s "skip ahead to
  // live" catch-up does NOT set this: its loop advances next_read_ts and
  // next_slot_number/next_frame_number in lockstep via the same
  // get_samples_per_slot() the anchor formula itself is built on, so that
  // mapping stays exact through any number of iterations (see the loop's own
  // comment). ue_split7_rx_slot_td() checks-and-clears this flag to decide
  // whether the anchor needs refreshing: only on an actual clock
  // discontinuity, never unconditionally -- a pure arithmetic reconstruction
  // from one fixed anchor is exact for as long as the real clock advances by
  // exactly one slot's worth of samples per slot (the duration functions
  // telescope: get_samples_slot_duration(fp,0,N) is by construction the sum
  // of the per-slot durations the cursor actually advances by), so an
  // unconditional refresh would be redundant work with no benefit -- and,
  // worse, a hazard: write_prach() reads the anchor once, when a PRACH burst
  // is enqueued, while finalize_tx_symbol() re-reads it fresh, per symbol,
  // possibly several times later, on a different thread, while streaming that
  // same burst; refreshing the anchor between those reads would let the two
  // ends of one burst's timing computation disagree, corrupting the burst's
  // internal sample continuity.
  _Atomic bool clock_jumped;
} ue_split7_device_priv_t;

static void sync_task_func(void *arg);

// 20ms SSB period + 1 slot margin: unlike nr_scan_ssb()'s frame-aligned capture, this
// search's arbitrary live-edge offset can otherwise truncate the SSB occasion by phase.
static uint32_t split7_sync_capture_samples(const NR_DL_FRAME_PARMS *fp)
{
  return 2 * fp->samples_per_frame + get_samples_per_slot(0, fp); // 20ms (TS 38.213 §4.1) + 1 slot margin
}

// Writes at ts % circ_buf_size, i.e. directly at the position the RF device's own
// timestamp implies, so the ring can never desync from the device's sample clock.
static void write_ring_at_ts(ue_split7_device_priv_t *priv,
                             openair0_timestamp_t ts,
                             int16_t **src,
                             uint32_t num_samples,
                             uint16_t num_channels)
{
  uint64_t write_pos = (uint64_t)ts % priv->circ_buf_size;
  uint32_t space_to_end = priv->circ_buf_size - write_pos;

  for (uint16_t ch = 0; ch < num_channels; ch++) {
    if (num_samples <= space_to_end) {
      memcpy(&priv->rx_circ_buf[ch][write_pos * 2], src[ch], num_samples * 2 * sizeof(int16_t));
    } else {
      memcpy(&priv->rx_circ_buf[ch][write_pos * 2], src[ch], space_to_end * 2 * sizeof(int16_t));
      memcpy(&priv->rx_circ_buf[ch][0], &src[ch][space_to_end * 2], (num_samples - space_to_end) * 2 * sizeof(int16_t));
    }
  }
  atomic_store_explicit(&priv->latest_write_ts, (uint64_t)ts + num_samples, memory_order_release);
}

static void read_channel_at_ts(ue_split7_device_priv_t *priv,
                               int16_t *dst,
                               uint16_t channel,
                               openair0_timestamp_t ts,
                               uint32_t num_samples)
{
  uint64_t read_pos = (uint64_t)ts % priv->circ_buf_size;
  uint32_t space_to_end = priv->circ_buf_size - read_pos;

  if (num_samples <= space_to_end) {
    memcpy(dst, &priv->rx_circ_buf[channel][read_pos * 2], num_samples * 2 * sizeof(int16_t));
  } else {
    memcpy(dst, &priv->rx_circ_buf[channel][read_pos * 2], space_to_end * 2 * sizeof(int16_t));
    memcpy(&dst[space_to_end * 2], &priv->rx_circ_buf[channel][0], (num_samples - space_to_end) * 2 * sizeof(int16_t));
  }
}

static void *read_thread_func(void *arg)
{
  struct ue_split7_device *dev = (struct ue_split7_device *)arg;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;

  uint16_t num_rx = dev->config.num_rx_antennas;
  uint32_t chunk_size = dev->config.fft_size > 0 ? dev->config.fft_size : 2048;

  int16_t **temp_bufs = (int16_t **)malloc_or_fail(num_rx * sizeof(int16_t *));
  void **rx_ptrs = (void **)malloc_or_fail(num_rx * sizeof(void *));

  for (int i = 0; i < num_rx; i++) {
    temp_bufs[i] = (int16_t *)malloc_or_fail(chunk_size * 2 * sizeof(int16_t));
  }

  while (priv->read_running) {
    openair0_timestamp_t ts = 0;
    for (int i = 0; i < num_rx; i++) {
      rx_ptrs[i] = temp_bufs[i];
    }

    // Never blocks on a consumer: a slow FD-UE side just sees a staleness warning
    // at the point of use (read_symbols()/wait_next_slot()) instead of stalling ingestion.
    int read_samples = priv->openair0_dev.trx_read_func(&priv->openair0_dev, &ts, rx_ptrs, chunk_size, num_rx);
    if (read_samples < 0) {
      usleep(1000);
      continue;
    }
    // OAI's RF-device contract is exact-count-or-error (see nr-ru.c/nr-ue-ru.c); a
    // partial read would desync the ring's ts-addressed writes from real device time.
    AssertFatal((uint32_t)read_samples == chunk_size,
                "[split7] rfdevice trx_read_func returned %d samples, expected %u\n",
                read_samples,
                chunk_size);

    write_ring_at_ts(priv, ts, temp_bufs, chunk_size, num_rx);

    pthread_mutex_lock(&priv->data_mutex);
    pthread_cond_broadcast(&priv->data_cond);
    pthread_mutex_unlock(&priv->data_mutex);
  }

  for (int i = 0; i < num_rx; i++) {
    free(temp_bufs[i]);
  }
  free(temp_bufs);
  free(rx_ptrs);
  return NULL;
}

static ue_split7_status_t ue_split7_configure(struct ue_split7_device *dev, const ue_split7_config_t *config)
{
  if (!dev || !config)
    return UE_SPLIT7_ERR_INVALID_PARAM;
  if (config->num_rx_antennas == 0 || config->num_rx_antennas > UE_SPLIT7_MAX_ANT || config->num_tx_antennas == 0
      || config->num_tx_antennas > UE_SPLIT7_MAX_ANT) {
    LOG_E(PHY,
          "ue_split7_configure: invalid antenna count (rx=%u tx=%u); must be 1..%d\n",
          config->num_rx_antennas,
          config->num_tx_antennas,
          UE_SPLIT7_MAX_ANT);
    return UE_SPLIT7_ERR_INVALID_PARAM;
  }
  if (!config->frame_parms) {
    LOG_E(PHY, "ue_split7_configure: config->frame_parms is required (no standalone fallback)\n");
    return UE_SPLIT7_ERR_INVALID_PARAM;
  }
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;

  // Reconfiguring while started would free buffers still in use -- stop() first.
  if (priv->is_started) {
    LOG_E(PHY, "ue_split7_configure: cannot reconfigure while device is started\n");
    return UE_SPLIT7_ERR_STATE;
  }

  // Load OAI DFT library
  if (load_dftslib() < 0) {
    LOG_E(PHY, "Failed to load OAI DFT library\n");
    return UE_SPLIT7_ERR_GENERIC;
  }

  // Free existing buffers if reconfiguring
  if (priv->rx_circ_buf) {
    for (int i = 0; i < dev->config.num_rx_antennas; i++) {
      if (priv->rx_circ_buf[i])
        free(priv->rx_circ_buf[i]);
    }
    free(priv->rx_circ_buf);
    priv->rx_circ_buf = NULL;
  }
  if (priv->rx_fd_buf) {
    for (int i = 0; i < dev->config.num_rx_antennas; i++) {
      if (priv->rx_fd_buf[i])
        free(priv->rx_fd_buf[i]);
    }
    free(priv->rx_fd_buf);
    priv->rx_fd_buf = NULL;
    dev->rx_fd_buffers = NULL;
  }

  dev->config = *config;

  // Setup openair0 config
  memset(&priv->openair0_cfg, 0, sizeof(priv->openair0_cfg));
  priv->openair0_cfg.nr_flag = 1;
  priv->openair0_cfg.sample_rate = config->sample_rate_hz;
  nrUE_params_t *ue_params = get_nrUE_params();
  if (ue_params) {
    priv->openair0_cfg.num_rb_dl = ue_params->N_RB_DL > 0 ? ue_params->N_RB_DL : 106;
    priv->openair0_cfg.sdr_addrs = ue_params->usrp_args;
    priv->openair0_cfg.tx_subdev = ue_params->tx_subdev;
    priv->openair0_cfg.rx_subdev = ue_params->rx_subdev;
  } else {
    priv->openair0_cfg.num_rb_dl = 106;
  }
  priv->openair0_cfg.rx_num_channels = config->num_rx_antennas;
  priv->openair0_cfg.tx_num_channels = config->num_tx_antennas;
  priv->openair0_cfg.rx_freq[0] = config->dl_carrier_freq_hz;
  priv->openair0_cfg.tx_freq[0] = config->ul_carrier_freq_hz;
  priv->openair0_cfg.rx_bw = config->sample_rate_hz * 0.8;
  priv->openair0_cfg.tx_bw = config->sample_rate_hz * 0.8;
  priv->openair0_cfg.clock_source = internal;
  priv->openair0_cfg.time_source = internal;

  // Initialize openair0 device (loading rfsimulator or other driver)
  if (openair0_device_load(&priv->openair0_dev, &priv->openair0_cfg) < 0) {
    LOG_E(PHY, "Failed to load openair0 backend device\n");
    return UE_SPLIT7_ERR_GENERIC;
  }

  // Free existing FFT/IFFT intermediate buffers if reconfiguring (mirrors the
  // rx_circ_buf/rx_fd_buf free-before-reassign handling above -- otherwise every
  // repeat configure() call leaks the previous pair of aligned buffers).
  if (priv->rx_time_buf) {
    free(priv->rx_time_buf);
    priv->rx_time_buf = NULL;
  }
  if (priv->tx_time_buf) {
    free(priv->tx_time_buf);
    priv->tx_time_buf = NULL;
  }

  // Allocate intermediate FFT/IFFT time-domain buffers
  priv->buf_size_samples = config->fft_size + config->cp_len_symbol0;
  // Each sample is complex (2 * int16_t)
  priv->rx_time_buf = (int16_t *)malloc16(priv->buf_size_samples * 2 * sizeof(int16_t));
  priv->tx_time_buf = (int16_t *)malloc16(priv->buf_size_samples * 2 * sizeof(int16_t));

  if (!priv->rx_time_buf || !priv->tx_time_buf) {
    LOG_E(PHY, "Out of memory allocating FFT/IFFT intermediate buffers\n");
    if (priv->rx_time_buf) {
      free(priv->rx_time_buf);
      priv->rx_time_buf = NULL;
    }
    if (priv->tx_time_buf) {
      free(priv->tx_time_buf);
      priv->tx_time_buf = NULL;
    }
    return UE_SPLIT7_ERR_NO_MEMORY;
  }

  memset(priv->rx_time_buf, 0, priv->buf_size_samples * 2 * sizeof(int16_t));
  memset(priv->tx_time_buf, 0, priv->buf_size_samples * 2 * sizeof(int16_t));

  // Private copy, not a pointer alias: the host may keep mutating its own frame_parms
  // (RRC/MAC config updates) without the device racing on it. The host is required to
  // have already run it through nr_init_frame_parms_ue()/init_symbol_rotation() (see
  // frame_parms's doc comment in ue_split7_interface.h) -- this device does not derive
  // a standalone fallback.
  priv->frame_parms = *config->frame_parms;
  // Matches nr_slot_fep()'s early-into-CP sampling so timeshift_symbol_rotation applies correctly.
  priv->rot_sample_offset =
      priv->frame_parms.ofdm_offset_divisor > 0 ? priv->frame_parms.nb_prefix_samples / priv->frame_parms.ofdm_offset_divisor : 0;
  priv->fp = &priv->frame_parms;

  // Allocate circular buffer
  priv->circ_buf_size = config->sample_rate_hz * 0.2; // 200ms buffer
  if (priv->circ_buf_size < 2 * (config->fft_size + config->cp_len_symbol0)) {
    priv->circ_buf_size = 2 * (config->fft_size + config->cp_len_symbol0);
  }

  priv->rx_circ_buf = (int16_t **)malloc_or_fail(config->num_rx_antennas * sizeof(int16_t *));

  for (int i = 0; i < config->num_rx_antennas; i++) {
    priv->rx_circ_buf[i] = (int16_t *)malloc16(priv->circ_buf_size * 2 * sizeof(int16_t));
    if (!priv->rx_circ_buf[i]) {
      for (int k = 0; k < i; k++) {
        free(priv->rx_circ_buf[k]);
      }
      free(priv->rx_circ_buf);
      priv->rx_circ_buf = NULL;
      LOG_E(PHY, "Out of memory allocating circular buffer channel\n");
      return UE_SPLIT7_ERR_NO_MEMORY;
    }
    memset(priv->rx_circ_buf[i], 0, priv->circ_buf_size * 2 * sizeof(int16_t));
  }

  uint32_t slots_per_frame = priv->fp->slots_per_frame > 0 ? priv->fp->slots_per_frame : 10;
  uint32_t symbols_per_slot = priv->fp->symbols_per_slot > 0 ? priv->fp->symbols_per_slot : 14;
  priv->rx_fd_ring_symbols = 2 * slots_per_frame * symbols_per_slot;
  priv->rx_td_buf_samples = 2 * priv->fp->samples_per_frame + priv->fp->ofdm_symbol_size;

  if (priv->rx_td_buf) {
    for (int i = 0; i < dev->config.num_rx_antennas; i++) {
      if (priv->rx_td_buf[i])
        free(priv->rx_td_buf[i]);
    }
    free(priv->rx_td_buf);
    priv->rx_td_buf = NULL;
    dev->rx_td_buffers = NULL;
  }

  priv->rx_td_buf = (c16_t **)malloc_or_fail(config->num_rx_antennas * sizeof(c16_t *));
  for (int i = 0; i < config->num_rx_antennas; i++) {
    priv->rx_td_buf[i] = (c16_t *)malloc16(priv->rx_td_buf_samples * sizeof(c16_t));
    if (!priv->rx_td_buf[i]) {
      for (int k = 0; k < i; k++) {
        free(priv->rx_td_buf[k]);
      }
      free(priv->rx_td_buf);
      priv->rx_td_buf = NULL;
      dev->rx_td_buffers = NULL;
      LOG_E(PHY, "Out of memory allocating TD-frame buffer\n");
      return UE_SPLIT7_ERR_NO_MEMORY;
    }
    memset(priv->rx_td_buf[i], 0, priv->rx_td_buf_samples * sizeof(c16_t));
  }
  dev->rx_td_buffers = priv->rx_td_buf;

  // TX mirror of rx_td_buf above -- same per-antenna sizing, sized by num_tx_antennas.
  if (priv->tx_td_buf) {
    for (int i = 0; i < dev->config.num_tx_antennas; i++) {
      if (priv->tx_td_buf[i])
        free(priv->tx_td_buf[i]);
    }
    free(priv->tx_td_buf);
    priv->tx_td_buf = NULL;
  }

  priv->tx_td_buf = (c16_t **)malloc_or_fail(config->num_tx_antennas * sizeof(c16_t *));
  for (int i = 0; i < config->num_tx_antennas; i++) {
    priv->tx_td_buf[i] = (c16_t *)malloc16(priv->rx_td_buf_samples * sizeof(c16_t));
    if (!priv->tx_td_buf[i]) {
      for (int k = 0; k < i; k++) {
        free(priv->tx_td_buf[k]);
      }
      free(priv->tx_td_buf);
      priv->tx_td_buf = NULL;
      LOG_E(PHY, "Out of memory allocating TX TD-frame buffer\n");
      return UE_SPLIT7_ERR_NO_MEMORY;
    }
    memset(priv->tx_td_buf[i], 0, priv->rx_td_buf_samples * sizeof(c16_t));
  }

  if (priv->rx_fd_buf) {
    for (int i = 0; i < dev->config.num_rx_antennas; i++) {
      if (priv->rx_fd_buf[i])
        free(priv->rx_fd_buf[i]);
    }
    free(priv->rx_fd_buf);
    priv->rx_fd_buf = NULL;
    dev->rx_fd_buffers = NULL;
  }

  priv->rx_fd_buf = (c16_t **)malloc_or_fail(config->num_rx_antennas * sizeof(c16_t *));
  for (int i = 0; i < config->num_rx_antennas; i++) {
    priv->rx_fd_buf[i] = (c16_t *)malloc16(priv->rx_fd_ring_symbols * config->fft_size * sizeof(c16_t));
    if (!priv->rx_fd_buf[i]) {
      for (int k = 0; k < i; k++) {
        free(priv->rx_fd_buf[k]);
      }
      free(priv->rx_fd_buf);
      priv->rx_fd_buf = NULL;
      dev->rx_fd_buffers = NULL;
      LOG_E(PHY, "Out of memory allocating FD-symbol ring buffer\n");
      return UE_SPLIT7_ERR_NO_MEMORY;
    }
    memset(priv->rx_fd_buf[i], 0, priv->rx_fd_ring_symbols * config->fft_size * sizeof(c16_t));
  }
  dev->rx_fd_buffers = priv->rx_fd_buf;

  if (priv->symbol_fft_done) {
    free(priv->symbol_fft_done);
    priv->symbol_fft_done = NULL;
  }
  if (priv->symbol_frame_number) {
    free(priv->symbol_frame_number);
    priv->symbol_frame_number = NULL;
  }

  priv->symbol_fft_done = (bool *)calloc(priv->rx_fd_ring_symbols, sizeof(bool));
  priv->symbol_frame_number = (uint32_t *)malloc_or_fail(priv->rx_fd_ring_symbols * sizeof(uint32_t));
  for (uint32_t s = 0; s < priv->rx_fd_ring_symbols; s++) {
    priv->symbol_frame_number[s] = UINT32_MAX;
    priv->symbol_fft_done[s] = false;
  }
  // No slot primed by wait_next_slot() yet -- read_symbols() must RX for itself
  // until the first wait_next_slot() call (or forever, for standalone/test callers).
  priv->td_rx_done_frame_number = UINT32_MAX;
  priv->td_rx_done_slot_number = 0;

  // Multi-client UL combining: fresh device, nobody registered/synced/pending yet.
  priv->num_clients = 0;
  priv->synced_mask = 0;
  priv->anchor_seeded = false;

  if (priv->tx_checked_in_mask) {
    free(priv->tx_checked_in_mask);
    priv->tx_checked_in_mask = NULL;
  }
  if (priv->tx_expected_mask) {
    free(priv->tx_expected_mask);
    priv->tx_expected_mask = NULL;
  }
  priv->tx_checked_in_mask = (uint32_t *)calloc(priv->rx_fd_ring_symbols, sizeof(uint32_t));
  priv->tx_expected_mask = (uint32_t *)calloc(priv->rx_fd_ring_symbols, sizeof(uint32_t));

  if (priv->tx_fd_accum) {
    for (int i = 0; i < dev->config.num_tx_antennas; i++) {
      if (priv->tx_fd_accum[i])
        free(priv->tx_fd_accum[i]);
    }
    free(priv->tx_fd_accum);
    priv->tx_fd_accum = NULL;
  }
  priv->tx_fd_accum = (c16_t **)malloc_or_fail(config->num_tx_antennas * sizeof(c16_t *));
  for (int i = 0; i < config->num_tx_antennas; i++) {
    priv->tx_fd_accum[i] = (c16_t *)malloc16(priv->rx_fd_ring_symbols * config->fft_size * sizeof(c16_t));
    if (!priv->tx_fd_accum[i]) {
      for (int k = 0; k < i; k++)
        free(priv->tx_fd_accum[k]);
      free(priv->tx_fd_accum);
      priv->tx_fd_accum = NULL;
      LOG_E(PHY, "Out of memory allocating TX FD accumulator\n");
      return UE_SPLIT7_ERR_NO_MEMORY;
    }
    memset(priv->tx_fd_accum[i], 0, priv->rx_fd_ring_symbols * config->fft_size * sizeof(c16_t));
  }

  for (int q = 0; q < UE_SPLIT7_MAX_PRACH_QUEUE; q++) {
    if (priv->prach_queue[q].active && priv->prach_queue[q].samples)
      free(priv->prach_queue[q].samples);
    priv->prach_queue[q].active = false;
    priv->prach_queue[q].samples = NULL;
  }

  for (int i = 0; i < UE_SPLIT7_MAX_ANT; i++) {
    if (priv->prev_rx_buf[i]) {
      free(priv->prev_rx_buf[i]);
      priv->prev_rx_buf[i] = NULL;
    }
  }
  for (int i = 0; i < config->num_rx_antennas; i++) {
    priv->prev_rx_buf[i] = (int16_t *)malloc16(priv->buf_size_samples * 2 * sizeof(int16_t));
    if (priv->prev_rx_buf[i])
      memset(priv->prev_rx_buf[i], 0, priv->buf_size_samples * 2 * sizeof(int16_t));
  }
  priv->prev_rx_buf_samples = 0;
  atomic_store_explicit(&priv->rx_timing_offset, 0, memory_order_relaxed);
  atomic_store_explicit(&priv->tx_timing_offset, 0, memory_order_relaxed);

  return UE_SPLIT7_SUCCESS;
}

static ue_split7_status_t ue_split7_start(struct ue_split7_device *dev)
{
  if (!dev)
    return UE_SPLIT7_ERR_INVALID_PARAM;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;

  if (priv->is_started)
    return UE_SPLIT7_SUCCESS;

  int rc = priv->openair0_dev.trx_start_func(&priv->openair0_dev);
  if (rc < 0) {
    LOG_E(PHY, "Failed to start openair0 device\n");
    return UE_SPLIT7_ERR_GENERIC;
  }

  priv->is_started = true;

  // Initialize circular buffer pointers and start read thread
  atomic_store_explicit(&priv->latest_write_ts, 0, memory_order_relaxed);
  priv->next_read_ts = 0;
  priv->read_running = true;

  int thread_rc = pthread_create(&priv->read_thread, NULL, read_thread_func, dev);
  if (thread_rc != 0) {
    LOG_E(PHY, "Failed to create read thread\n");
    priv->read_running = false;
    priv->is_started = false;
    return UE_SPLIT7_ERR_GENERIC;
  }

  return UE_SPLIT7_SUCCESS;
}

static ue_split7_status_t ue_split7_stop(struct ue_split7_device *dev)
{
  if (!dev)
    return UE_SPLIT7_ERR_INVALID_PARAM;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;

  if (!priv->is_started)
    return UE_SPLIT7_SUCCESS;

  if (priv->sync_active) {
    priv->sync_active = false;
    while (atomic_load_explicit(&priv->sync_task_running, memory_order_acquire)) {
      usleep(1000);
    }
  }

  if (priv->read_running) {
    priv->read_running = false;
    // Wake anything blocked in ue_split7_read_symbols()/ue_split7_wait_next_slot()
    // on data_cond -- otherwise a waiter would never notice read_running
    // went false until the next (now nonexistent) write signals it.
    pthread_mutex_lock(&priv->data_mutex);
    pthread_cond_broadcast(&priv->data_cond);
    pthread_mutex_unlock(&priv->data_mutex);
    pthread_join(priv->read_thread, NULL);
  }

  if (priv->openair0_dev.trx_stop_func)
    priv->openair0_dev.trx_stop_func(&priv->openair0_dev);
  if (priv->openair0_dev.trx_end_func)
    priv->openair0_dev.trx_end_func(&priv->openair0_dev);

  priv->is_started = false;
  return UE_SPLIT7_SUCCESS;
}

// Receives total_samples complex samples per antenna channel into ant_bufs.
// Applies accumulated rx_timing_offset:
//   - positive: trashes samples before receiving into time-domain buffer
//   - negative: copies previously received samples to the start and receives proportionally fewer
// Atomically subtracts the applied adjustment afterwards.
static int ue_split7_rx_time_domain(ue_split7_device_priv_t *priv,
                                    int16_t **ant_bufs,
                                    uint32_t total_samples,
                                    uint16_t num_buffers,
                                    openair0_timestamp_t *out_ts)
{
  int32_t offset = atomic_load_explicit(&priv->rx_timing_offset, memory_order_acquire);

  // A nonzero offset means next_read_ts is about to move by something other
  // than exactly total_samples -- a real clock discontinuity, not pure
  // incrementing. Flag it so ue_split7_rx_slot_td() knows the TX anchor
  // actually needs refreshing this slot (see clock_jumped's declaration).
  if (offset != 0)
    atomic_store_explicit(&priv->clock_jumped, true, memory_order_release);

  if (offset > 0) {
    uint32_t trash_samples = (uint32_t)offset;
    if (trash_samples > total_samples)
      trash_samples = total_samples;

    if (priv->read_running) {
      openair0_timestamp_t trash_target = priv->next_read_ts + trash_samples;
      pthread_mutex_lock(&priv->data_mutex);
      while (atomic_load_explicit(&priv->read_running, memory_order_acquire)
             && atomic_load_explicit(&priv->latest_write_ts, memory_order_acquire) < trash_target) {
        pthread_cond_wait(&priv->data_cond, &priv->data_mutex);
      }
      pthread_mutex_unlock(&priv->data_mutex);
      if (!priv->read_running)
        return -1;
      priv->next_read_ts += trash_samples;
    } else {
      void *trash_ptrs[UE_SPLIT7_MAX_ANT];
      for (uint16_t i = 0; i < num_buffers; i++) {
        trash_ptrs[i] = priv->rx_td_buf[i];
      }
      openair0_timestamp_t trash_ts = 0;
      int tr = priv->openair0_dev.trx_read_func(&priv->openair0_dev, &trash_ts, trash_ptrs, trash_samples, num_buffers);
      if (tr < 0)
        return -1;
    }

    if (priv->read_running) {
      openair0_timestamp_t target = priv->next_read_ts + total_samples;
      pthread_mutex_lock(&priv->data_mutex);
      while (atomic_load_explicit(&priv->read_running, memory_order_acquire)
             && atomic_load_explicit(&priv->latest_write_ts, memory_order_acquire) < target) {
        pthread_cond_wait(&priv->data_cond, &priv->data_mutex);
      }
      pthread_mutex_unlock(&priv->data_mutex);
      if (!priv->read_running)
        return -1;
      *out_ts = priv->next_read_ts;
      for (uint16_t i = 0; i < num_buffers; i++)
        read_channel_at_ts(priv, ant_bufs[i], i, *out_ts, total_samples);
      priv->next_read_ts = target;
    } else {
      void *rx_ptrs[UE_SPLIT7_MAX_ANT];
      for (uint16_t i = 0; i < num_buffers; i++)
        rx_ptrs[i] = ant_bufs[i];
      int r = priv->openair0_dev.trx_read_func(&priv->openair0_dev, out_ts, rx_ptrs, total_samples, num_buffers);
      if (r < 0)
        return -1;
    }

    uint32_t save_len = total_samples < priv->buf_size_samples ? total_samples : priv->buf_size_samples;
    for (uint16_t i = 0; i < num_buffers; i++) {
      if (priv->prev_rx_buf[i])
        memcpy(priv->prev_rx_buf[i], &ant_bufs[i][(total_samples - save_len) * 2], save_len * 2 * sizeof(int16_t));
    }
    priv->prev_rx_buf_samples = save_len;
    atomic_fetch_sub_explicit(&priv->rx_timing_offset, (int32_t)trash_samples, memory_order_acq_rel);
    return 0;
  }

  if (offset < 0) {
    uint32_t desired = (uint32_t)(-offset);
    uint32_t copy_samples = desired;
    if (copy_samples > priv->prev_rx_buf_samples)
      copy_samples = priv->prev_rx_buf_samples;
    if (copy_samples >= total_samples)
      copy_samples = total_samples - 1;

    if (copy_samples > 0) {
      for (uint16_t i = 0; i < num_buffers; i++) {
        if (priv->prev_rx_buf[i])
          memcpy(&ant_bufs[i][0],
                 &priv->prev_rx_buf[i][(priv->prev_rx_buf_samples - copy_samples) * 2],
                 copy_samples * 2 * sizeof(int16_t));
        else
          memset(&ant_bufs[i][0], 0, copy_samples * 2 * sizeof(int16_t));
      }

      uint32_t rx_needed = total_samples - copy_samples;
      if (priv->read_running) {
        openair0_timestamp_t target = priv->next_read_ts + rx_needed;
        pthread_mutex_lock(&priv->data_mutex);
        while (atomic_load_explicit(&priv->read_running, memory_order_acquire)
               && atomic_load_explicit(&priv->latest_write_ts, memory_order_acquire) < target) {
          pthread_cond_wait(&priv->data_cond, &priv->data_mutex);
        }
        pthread_mutex_unlock(&priv->data_mutex);
        if (!priv->read_running)
          return -1;
        *out_ts = priv->next_read_ts - copy_samples;
        for (uint16_t i = 0; i < num_buffers; i++)
          read_channel_at_ts(priv, &ant_bufs[i][copy_samples * 2], i, priv->next_read_ts, rx_needed);
        priv->next_read_ts = target;
      } else {
        void *rx_ptrs[UE_SPLIT7_MAX_ANT];
        for (uint16_t i = 0; i < num_buffers; i++)
          rx_ptrs[i] = &ant_bufs[i][copy_samples * 2];
        openair0_timestamp_t ts_read = 0;
        int r = priv->openair0_dev.trx_read_func(&priv->openair0_dev, &ts_read, rx_ptrs, rx_needed, num_buffers);
        if (r < 0)
          return -1;
        *out_ts = ts_read - copy_samples;
      }

      uint32_t save_len = total_samples < priv->buf_size_samples ? total_samples : priv->buf_size_samples;
      for (uint16_t i = 0; i < num_buffers; i++) {
        if (priv->prev_rx_buf[i])
          memcpy(priv->prev_rx_buf[i], &ant_bufs[i][(total_samples - save_len) * 2], save_len * 2 * sizeof(int16_t));
      }
      priv->prev_rx_buf_samples = save_len;
      atomic_fetch_sub_explicit(&priv->rx_timing_offset, -(int32_t)copy_samples, memory_order_acq_rel);
      return 0;
    }
  }

  // offset == 0 or copy_samples == 0 (no previous samples cached yet)
  if (priv->read_running) {
    openair0_timestamp_t target = priv->next_read_ts + total_samples;
    pthread_mutex_lock(&priv->data_mutex);
    while (atomic_load_explicit(&priv->read_running, memory_order_acquire)
           && atomic_load_explicit(&priv->latest_write_ts, memory_order_acquire) < target) {
      pthread_cond_wait(&priv->data_cond, &priv->data_mutex);
    }
    pthread_mutex_unlock(&priv->data_mutex);
    if (!priv->read_running)
      return -1;
    *out_ts = priv->next_read_ts;
    for (uint16_t i = 0; i < num_buffers; i++)
      read_channel_at_ts(priv, ant_bufs[i], i, *out_ts, total_samples);
    priv->next_read_ts = target;
  } else {
    void *rx_ptrs[UE_SPLIT7_MAX_ANT];
    for (uint16_t i = 0; i < num_buffers; i++)
      rx_ptrs[i] = ant_bufs[i];
    int r = priv->openair0_dev.trx_read_func(&priv->openair0_dev, out_ts, rx_ptrs, total_samples, num_buffers);
    if (r < 0)
      return -1;
  }

  uint32_t save_len = total_samples < priv->buf_size_samples ? total_samples : priv->buf_size_samples;
  for (uint16_t i = 0; i < num_buffers; i++) {
    if (priv->prev_rx_buf[i])
      memcpy(priv->prev_rx_buf[i], &ant_bufs[i][(total_samples - save_len) * 2], save_len * 2 * sizeof(int16_t));
  }
  priv->prev_rx_buf_samples = save_len;
  return 0;
}

// Resolves a caller-supplied frame_number to the device's own absolute frame
// scale (the one tx_anchor_frame_number/next_frame_number/last_rx_abs_frame all
// share). frame_number >= 1024 already IS an absolute frame count -- taken as-is,
// unmodified, so a caller doing its own absolute bookkeeping loses nothing.
// frame_number < 1024 is the plain 3GPP SFN with no cycle information of its own;
// it's unwrapped to whichever absolute frame nearest reference_abs_frame agrees
// with it modulo 1024 (always within +/-512 frames, true for both RX "now" and
// the handful of slots' RX-to-TX lead the UL side reasons about).
static uint32_t ue_split7_resolve_abs_frame(uint32_t reference_abs_frame, uint32_t frame_number)
{
  if (frame_number >= 1024)
    return frame_number;
  uint32_t wrapped_delta = (frame_number - reference_abs_frame) & 1023u;
  int32_t signed_delta = (wrapped_delta >= 512u) ? (int32_t)wrapped_delta - 1024 : (int32_t)wrapped_delta;
  return (uint32_t)((int64_t)reference_abs_frame + signed_delta);
}

// Saturating add: multiple clients' REs (or a PRACH waveform) landing on the
// same bin/sample must not silently wrap int16_t the way a real ADC/summing
// junction wouldn't either -- it clips instead.
static inline int16_t ue_split7_sat_add_i16(int16_t a, int16_t b)
{
  int32_t sum = (int32_t)a + (int32_t)b;
  if (sum > INT16_MAX)
    return INT16_MAX;
  if (sum < INT16_MIN)
    return INT16_MIN;
  return (int16_t)sum;
}

static ue_split7_status_t ue_split7_register_client(struct ue_split7_device *dev, uint32_t *client_id)
{
  if (!dev || !client_id)
    return UE_SPLIT7_ERR_INVALID_PARAM;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;

  pthread_mutex_lock(&priv->tx_combine_mutex);
  if (priv->num_clients >= UE_SPLIT7_MAX_CLIENTS) {
    pthread_mutex_unlock(&priv->tx_combine_mutex);
    return UE_SPLIT7_ERR_NO_MEMORY;
  }
  *client_id = priv->num_clients++;
  pthread_mutex_unlock(&priv->tx_combine_mutex);

  return UE_SPLIT7_SUCCESS;
}

static ue_split7_status_t ue_split7_read_symbols(struct ue_split7_device *dev,
                                                 uint32_t frame_number,
                                                 uint16_t slot_number,
                                                 uint8_t start_symbol,
                                                 uint8_t num_symbols,
                                                 c16_t **buffers,
                                                 uint16_t num_buffers)
{
  if (!dev || num_buffers != dev->config.num_rx_antennas || num_symbols == 0) {
    return UE_SPLIT7_ERR_INVALID_PARAM;
  }
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;

  if (atomic_load_explicit(&priv->state, memory_order_acquire) < UE_SPLIT7_STATE_DL_SYNCED)
    return UE_SPLIT7_ERR_STATE;

  const NR_DL_FRAME_PARMS *fp = priv->fp;
  if ((uint32_t)start_symbol + num_symbols > fp->symbols_per_slot)
    return UE_SPLIT7_ERR_INVALID_PARAM;

  // frame_number may be the plain 3GPP SFN (0..1023) or already absolute; resolve
  // it to the device's own absolute scale before using it for ring addressing or
  // FFT-completion caching, so it can never alias against a different hyperframe.
  const uint32_t abs_frame = ue_split7_resolve_abs_frame(priv->last_rx_abs_frame, frame_number);

  pthread_mutex_lock(&priv->rx_mutex);

  uint32_t fft_size = dev->config.fft_size;
  uint32_t start_ring_sym =
      (((abs_frame % 2) * fp->slots_per_frame + slot_number) * fp->symbols_per_slot + start_symbol)
      % priv->rx_fd_ring_symbols;

  // Check if all requested symbols already have FFT completed for this frame
  bool all_done = true;
  for (uint8_t ls = 0; ls < num_symbols; ls++) {
    uint16_t sym = start_symbol + ls;
    uint32_t ring_sym = (((abs_frame % 2) * fp->slots_per_frame + slot_number) * fp->symbols_per_slot + sym)
                        % priv->rx_fd_ring_symbols;
    if (priv->symbol_frame_number[ring_sym] != abs_frame || !priv->symbol_fft_done[ring_sym]) {
      all_done = false;
      break;
    }
  }

  if (all_done) {
    if (buffers) {
      for (uint16_t i = 0; i < num_buffers; ++i) {
        buffers[i] = &priv->rx_fd_buf[i][start_ring_sym * fft_size];
      }
    }
    pthread_mutex_unlock(&priv->rx_mutex);
    return UE_SPLIT7_SUCCESS;
  }

  // Calculate sample offset in 2-frame TD buffer for this batch of symbols
  uint32_t frame_offset = (abs_frame % 2) * fp->samples_per_frame;
  uint32_t slot_offset = get_samples_slot_timestamp(fp, slot_number);
  uint32_t sym_offset = (start_symbol == 0) ? 0 : get_samples_symbol_duration(fp, slot_number, 0, start_symbol);
  uint32_t td_start_sample = frame_offset + slot_offset + sym_offset;

  // wait_next_slot() already RX'd this exact (frame, slot)'s TD samples for the
  // whole slot and reset every symbol's FFT-done flag -- nothing left to receive,
  // only the FFT below is needed. Otherwise (standalone/test callers that never
  // call wait_next_slot(), or a request for a slot it hasn't primed) fall back to
  // receiving exactly the requested symbol range here, as read_symbols() always did.
  bool td_ready = (priv->td_rx_done_frame_number == abs_frame && priv->td_rx_done_slot_number == slot_number);

  if (!td_ready) {
    uint32_t total_samples = get_samples_symbol_duration(fp, slot_number, start_symbol, num_symbols);

    int16_t *ant_bufs[UE_SPLIT7_MAX_ANT];
    for (uint16_t i = 0; i < num_buffers; i++) {
      ant_bufs[i] = (int16_t *)&priv->rx_td_buf[i][td_start_sample];
    }

    openair0_timestamp_t ts = 0;
    int rc = ue_split7_rx_time_domain(priv, ant_bufs, total_samples, num_buffers, &ts);
    if (rc < 0) {
      pthread_mutex_unlock(&priv->rx_mutex);
      return UE_SPLIT7_ERR_GENERIC;
    }

    // Set fft completion to false on TD data reception
    for (uint8_t ls = 0; ls < num_symbols; ls++) {
      uint16_t sym = start_symbol + ls;
      uint32_t ring_sym = (((abs_frame % 2) * fp->slots_per_frame + slot_number) * fp->symbols_per_slot + sym)
                          % priv->rx_fd_ring_symbols;
      priv->symbol_fft_done[ring_sym] = false;
      priv->symbol_frame_number[ring_sym] = abs_frame;
    }
  }

  dft_size_idx_t dft_sz = get_dft(fft_size);
  uint32_t sample_offset = 0;
  for (uint8_t ls = 0; ls < num_symbols; ls++) {
    uint16_t symbol = start_symbol + ls;
    uint32_t sym_samples = get_samples_symbol_duration(fp, slot_number, symbol, 1);
    uint32_t cp_len = sym_samples - fft_size;
    uint32_t ring_sym = (((abs_frame % 2) * fp->slots_per_frame + slot_number) * fp->symbols_per_slot + symbol)
                        % priv->rx_fd_ring_symbols;

    if (!priv->symbol_fft_done[ring_sym]) {
      for (uint16_t i = 0; i < num_buffers; ++i) {
        int16_t *time_ptr = (int16_t *)&priv->rx_td_buf[i][td_start_sample + sample_offset + cp_len - priv->rot_sample_offset];
        c16_t *fd_dest = &priv->rx_fd_buf[i][ring_sym * fft_size];
        dft(dft_sz, time_ptr, (int16_t *)fd_dest, 1);
        apply_nr_rotation_symbol_RX(priv->fp->symbols_per_slot,
                                    priv->fp->slots_per_subframe,
                                    priv->fp->timeshift_symbol_rotation,
                                    priv->fp->first_carrier_offset,
                                    fd_dest,
                                    priv->fp->symbol_rotation[link_type_dl],
                                    priv->fp->N_RB_DL,
                                    slot_number,
                                    symbol);
      }
      priv->symbol_fft_done[ring_sym] = true;
    }
    sample_offset += sym_samples;
  }

  if (buffers) {
    for (uint16_t i = 0; i < num_buffers; ++i) {
      buffers[i] = &priv->rx_fd_buf[i][start_ring_sym * fft_size];
    }
  }

  pthread_mutex_unlock(&priv->rx_mutex);
  return UE_SPLIT7_SUCCESS;
}

// Mirrors apply_nr_rotation_TX()'s non-flat-buffer branch (ofdm_mod.c), adapted for a
// buffer already offset to a single symbol (split7's write_symbols() convention).
static void split7_apply_tx_rotation(const ue_split7_device_priv_t *priv, c16_t *sym_buf, int slot, int symbol, int nb_rb)
{
  int symb_offset = (slot % priv->fp->slots_per_subframe) * priv->fp->symbols_per_slot;
  const c16_t rot = priv->fp->symbol_rotation[link_type_ul][symb_offset + symbol];

  c16_t *sym_neg = sym_buf + priv->fp->first_carrier_offset;
  if (nb_rb & 1) {
    sym_neg -= 6;
    nb_rb += 1;
  }
  rotate_cpx_vector(sym_buf, rot, sym_buf, nb_rb * 6, 15);
  rotate_cpx_vector(sym_neg, rot, sym_neg, nb_rb * 6, 15);
}

// Deterministic absolute sample position of (frame_number, slot_number, symbol),
// derived from the fixed seed_slot_tracking() anchor -- write_symbols()'s
// clock-domain counterpart to read_symbols()'s next_read_ts, decoupled from
// wait_next_slot()'s continuously-advancing RX polling cursor so it stays correct
// regardless of how far the RX/TX processing pipelines run ahead of each other.
static uint64_t ue_split7_ts_from_frame_slot_symbol(ue_split7_device_priv_t *priv,
                                                    uint32_t frame_number,
                                                    uint16_t slot_number,
                                                    uint8_t symbol)
{
  const NR_DL_FRAME_PARMS *fp = priv->fp;
  // tx_anchor_frame_number/tx_anchor_sample_pos are refreshed from the FD-UE
  // thread on a real clock discontinuity (see ue_split7_rx_slot_td()) while
  // this function is called from whichever actor/worker thread is finalizing
  // a TX symbol -- tx_combine_mutex (already used for every other piece of
  // TX-side shared state) keeps this pair from being read torn (frame_number
  // from one refresh, sample_pos from another).
  pthread_mutex_lock(&priv->tx_combine_mutex);
  uint32_t anchor_frame = priv->tx_anchor_frame_number;
  uint64_t anchor_pos = priv->tx_anchor_sample_pos;
  pthread_mutex_unlock(&priv->tx_combine_mutex);

  int64_t frame_delta = (int64_t)frame_number - (int64_t)anchor_frame;
  uint64_t ts = (uint64_t)((int64_t)anchor_pos
                + frame_delta * (int64_t)fp->samples_per_frame
                + (int64_t)get_samples_slot_duration(fp, 0, slot_number));
  if (symbol > 0)
    ts += get_samples_symbol_duration(fp, slot_number, 0, symbol);
  return ts;
}

// Streams any PRACH waveform(s) queued via write_prach() into this OFDM
// symbol's slice of tx_td_buf[0] (PRACH only ever drives antenna 0), at
// tx_start_sample -- called once per symbol as it's finalized, not matched
// against a single fixed absolute window computed once at queue time. Each
// entry tracks its own emitted_samples progress and, once it starts
// contributing (symbol_start_ts has reached its abs_start_sample), simply
// keeps consuming up to a full symbol's worth of samples on every subsequent
// call until its total_samples is exhausted. This is deliberately NOT
// re-derived from (symbol_start_ts, entry.abs_start_sample) on every call:
// TA/anchor state (folded into both the caller's tx_ts and this entry's
// abs_start_sample) can shift slightly between when write_prach() queued the
// entry and when a later, still-overlapping symbol actually gets finalized
// several symbols afterward, which would otherwise open a gap or overlap in
// coverage against a fixed absolute end. Only the entry's own buffer is
// materialized -- ONE repetition (CP + one fft_size cycle) -- and the
// fft_size portion is replayed via modulo for however many repetitions
// total_samples calls for, instead of pre-expanding the whole thing.
static void ue_split7_overlay_prach(ue_split7_device_priv_t *priv,
                                   uint64_t symbol_start_ts,
                                   uint32_t symbol_num_samples,
                                   uint32_t tx_start_sample)
{
  pthread_mutex_lock(&priv->prach_queue_mutex);

  for (int q = 0; q < UE_SPLIT7_MAX_PRACH_QUEUE; q++) {
    ue_split7_prach_queue_entry_t *e = &priv->prach_queue[q];
    if (!e->active)
      continue;

    if (e->emitted_samples >= e->total_samples) {
      // Fully streamed out -- nothing left to overlay, free it.
      free(e->samples);
      e->active = false;
      continue;
    }

    // Not started yet: this whole symbol is still before the PRACH's start.
    if (symbol_start_ts + symbol_num_samples <= e->abs_start_sample)
      continue;

    // Once started, every subsequent symbol contributes starting at its own
    // symbol_start_ts (emitted_samples already accounts for everything sent
    // so far); only the very first contributing symbol may need to skip a
    // leading portion that's still before abs_start_sample.
    uint32_t skip_in_symbol = 0;
    if (e->emitted_samples == 0 && e->abs_start_sample > symbol_start_ts)
      skip_in_symbol = (uint32_t)(e->abs_start_sample - symbol_start_ts);
    if (skip_in_symbol >= symbol_num_samples)
      continue;

    uint32_t space_in_symbol = symbol_num_samples - skip_in_symbol;
    uint32_t remaining = e->total_samples - e->emitted_samples;
    uint32_t chunk = (space_in_symbol < remaining) ? space_in_symbol : remaining;

    uint32_t dst_off = tx_start_sample + skip_in_symbol;
    int16_t *dst = (int16_t *)&priv->tx_td_buf[0][dst_off];
    for (uint32_t k = 0; k < chunk; k++) {
      uint32_t stream_pos = e->emitted_samples + k;
      uint32_t src_idx = (stream_pos < e->cp_len) ? stream_pos : e->cp_len + ((stream_pos - e->cp_len) % e->fft_size);
      dst[2 * k] = ue_split7_sat_add_i16(dst[2 * k], e->samples[2 * src_idx]);
      dst[2 * k + 1] = ue_split7_sat_add_i16(dst[2 * k + 1], e->samples[2 * src_idx + 1]);
    }
    e->emitted_samples += chunk;
  }

  pthread_mutex_unlock(&priv->prach_queue_mutex);
}

// Runs once per symbol, for whichever write_symbols()/skip_symbols() call
// completes that symbol's expected client set (see ue_split7_tx_check_in()
// below): applies the mandatory 38.211 §5.3 rotation to the shared FD
// accumulator, IDFTs + builds the CP, overlays any overlapping queued PRACH
// waveform, then writes the result to the RF device.
static ue_split7_status_t ue_split7_finalize_tx_symbol(struct ue_split7_device *dev,
                                                       uint32_t abs_frame,
                                                       uint16_t slot_number,
                                                       uint8_t symbol,
                                                       uint32_t ring_sym)
{
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;
  const NR_DL_FRAME_PARMS *fp = priv->fp;
  uint16_t num_buffers = dev->config.num_tx_antennas;
  uint32_t fft_size = fp->ofdm_symbol_size;
  idft_size_idx_t idft_size = get_idft(fft_size);

  uint32_t sym_samples = get_samples_symbol_duration(fp, slot_number, symbol, 1);
  uint32_t cp_len = sym_samples - fft_size;

  // Offset of this symbol into the device-owned 2-frame tx_td_buf, mirroring
  // read_symbols()'s td_start_sample derivation for rx_td_buf.
  uint32_t frame_offset = (abs_frame % 2) * fp->samples_per_frame;
  uint32_t slot_offset = get_samples_slot_timestamp(fp, slot_number);
  uint32_t sym_offset = (symbol == 0) ? 0 : get_samples_symbol_duration(fp, slot_number, 0, symbol);
  uint32_t tx_start_sample = frame_offset + slot_offset + sym_offset;

  // base_ts is the absolute RF sample position of (abs_frame, slot_number, symbol)'s
  // own start, reconstructed directly from the tracking anchor -- abs_frame/slot_number
  // here are already the resolved TX slot (proc->frame_tx/proc->nr_slot_tx from the
  // caller), NOT a current RX-reference slot, so ts_from_frame_slot_symbol() can
  // compute the target slot's position directly from its own (frame, slot)
  // number; no further RX-to-TX projection is needed on top. Only TA remains
  // to subtract.
  uint64_t base_ts = ue_split7_ts_from_frame_slot_symbol(priv, abs_frame, slot_number, symbol);
  openair0_timestamp_t tx_ts =
      (openair0_timestamp_t)base_ts - atomic_load_explicit(&priv->ta_samples, memory_order_relaxed);

  // Guards tx_td_buf, shared across calls, the way rx_mutex guards rx_td_buf in
  // read_symbols(); held through the RF write below, not just the fill.
  pthread_mutex_lock(&priv->write_mutex);

  for (uint16_t i = 0; i < num_buffers; ++i) {
    c16_t *sym_buf = &priv->tx_fd_accum[i][ring_sym * fft_size];

    // Mandatory 38.211 §5.3 phase rotation, as nr_tx_rotation_and_ofdm_mod() does --
    // applied once to the accumulated sum rather than per contributing client:
    // rotation is a fixed per-RE phase multiplier, so it distributes over the sum
    // identically either way.
    split7_apply_tx_rotation(priv, sym_buf, slot_number, symbol, fp->N_RB_UL);

    idft(idft_size, (int16_t *)sym_buf, priv->tx_time_buf, 1);

    // CP: last cp_len samples of IFFT output.
    int16_t *dst = (int16_t *)&priv->tx_td_buf[i][tx_start_sample];
    memcpy(dst, &priv->tx_time_buf[(fft_size - cp_len) * 2], cp_len * 2 * sizeof(int16_t));
    memcpy(&dst[cp_len * 2], priv->tx_time_buf, fft_size * 2 * sizeof(int16_t));
  }

  // Add in any PRACH waveform(s) overlapping this symbol's nominal (pre-tx_timing_offset)
  // sample range before the dynamic timing-offset dance below, so a drift
  // correction shifts PRACH and OFDM content together, consistently.
  ue_split7_overlay_prach(priv, (uint64_t)tx_ts, sym_samples, tx_start_sample);

  int16_t *tx_ant_bufs[UE_SPLIT7_MAX_ANT];
  void *tx_ptrs[UE_SPLIT7_MAX_ANT];
  for (uint16_t i = 0; i < num_buffers; ++i) {
    tx_ant_bufs[i] = (int16_t *)&priv->tx_td_buf[i][tx_start_sample];
    tx_ptrs[i] = tx_ant_bufs[i];
  }

  uint32_t total_samples = sym_samples;
  int32_t tx_offset = atomic_load_explicit(&priv->tx_timing_offset, memory_order_acquire);
  if (tx_offset > 0) {
    uint32_t dummy_samples = (uint32_t)tx_offset;
    if (dummy_samples > total_samples)
      dummy_samples = total_samples;
    int16_t *dummy_buf = (int16_t *)calloc(dummy_samples * 2, sizeof(int16_t));
    if (dummy_buf) {
      void *dummy_ptrs[UE_SPLIT7_MAX_ANT];
      for (uint16_t i = 0; i < num_buffers; ++i)
        dummy_ptrs[i] = dummy_buf;

      priv->openair0_dev.trx_write_func(&priv->openair0_dev, tx_ts, dummy_ptrs, dummy_samples, num_buffers, 1);
      free(dummy_buf);

      tx_ts += dummy_samples;
      atomic_fetch_sub_explicit(&priv->tx_timing_offset, (int32_t)dummy_samples, memory_order_acq_rel);
    }
  }

  uint32_t write_samples = total_samples;
  if (tx_offset < 0) {
    uint32_t trim_samples = (uint32_t)(-tx_offset);
    if (trim_samples >= total_samples)
      trim_samples = total_samples - 1;
    if (trim_samples > 0) {
      for (uint16_t i = 0; i < num_buffers; ++i)
        tx_ptrs[i] = &tx_ant_bufs[i][trim_samples * 2];
      write_samples = total_samples - trim_samples;
      atomic_fetch_sub_explicit(&priv->tx_timing_offset, -(int32_t)trim_samples, memory_order_acq_rel);
    }
  }

  int rc = priv->openair0_dev.trx_write_func(&priv->openair0_dev, tx_ts, tx_ptrs, write_samples, num_buffers, 1);

  pthread_mutex_unlock(&priv->write_mutex);

  return (rc < 0) ? UE_SPLIT7_ERR_GENERIC : UE_SPLIT7_SUCCESS;
}

// Shared per-symbol step for write_symbols()/skip_symbols(): optionally sums
// buffers[*][ls*fft_size..] (NULL for skip_symbols()) into the shared FD
// accumulator, marks client_id checked-in for this ring position, and
// finalizes (rotation+IDFT+CP+PRACH-overlay+TX, see above) if that completes
// the currently-synchronized set. Never blocks: a caller that doesn't complete
// the set just returns once its own contribution (if any) is consumed.
static ue_split7_status_t ue_split7_tx_check_in(struct ue_split7_device *dev,
                                                uint32_t client_id,
                                                uint32_t abs_frame,
                                                uint16_t slot_number,
                                                uint8_t symbol,
                                                c16_t **buffers,
                                                uint8_t ls,
                                                uint16_t num_buffers)
{
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;
  const NR_DL_FRAME_PARMS *fp = priv->fp;
  uint32_t fft_size = dev->config.fft_size;
  uint32_t ring_sym = (((abs_frame % 2) * fp->slots_per_frame + slot_number) * fp->symbols_per_slot + symbol)
                      % priv->rx_fd_ring_symbols;
  const uint32_t client_bit = 1u << client_id;

  pthread_mutex_lock(&priv->tx_combine_mutex);

  if (priv->tx_expected_mask[ring_sym] == 0) {
    // First touch of this ring position this round -- snapshot who's expected
    // and clear whatever the previous occurrence (2 frames back) left behind.
    priv->tx_expected_mask[ring_sym] = priv->synced_mask;
    for (uint16_t i = 0; i < num_buffers; i++)
      memset(&priv->tx_fd_accum[i][ring_sym * fft_size], 0, fft_size * sizeof(c16_t));
  }

  if (buffers) {
    for (uint16_t i = 0; i < num_buffers; i++) {
      c16_t *acc = &priv->tx_fd_accum[i][ring_sym * fft_size];
      const c16_t *src = &buffers[i][(uint32_t)ls * fft_size];
      for (uint32_t k = 0; k < fft_size; k++) {
        acc[k].r = ue_split7_sat_add_i16(acc[k].r, src[k].r);
        acc[k].i = ue_split7_sat_add_i16(acc[k].i, src[k].i);
      }
    }
  }

  priv->tx_checked_in_mask[ring_sym] |= client_bit;
  bool finalize = (priv->tx_checked_in_mask[ring_sym] == priv->tx_expected_mask[ring_sym]);
  if (finalize) {
    priv->tx_checked_in_mask[ring_sym] = 0;
    priv->tx_expected_mask[ring_sym] = 0;
  }

  pthread_mutex_unlock(&priv->tx_combine_mutex);

  if (!finalize)
    return UE_SPLIT7_SUCCESS;

  return ue_split7_finalize_tx_symbol(dev, abs_frame, slot_number, symbol, ring_sym);
}

static ue_split7_status_t ue_split7_write_symbols(struct ue_split7_device *dev,
                                                  uint32_t client_id,
                                                  uint32_t frame_number,
                                                  uint16_t slot_number,
                                                  uint8_t start_symbol,
                                                  uint8_t num_symbols,
                                                  c16_t **buffers,
                                                  uint16_t num_buffers)
{
  if (!dev || !buffers || num_buffers != dev->config.num_tx_antennas || num_symbols == 0
      || client_id >= UE_SPLIT7_MAX_CLIENTS) {
    return UE_SPLIT7_ERR_INVALID_PARAM;
  }
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;
  const NR_DL_FRAME_PARMS *fp = priv->fp;

  if ((uint32_t)start_symbol + num_symbols > fp->symbols_per_slot)
    return UE_SPLIT7_ERR_INVALID_PARAM;

  if (atomic_load_explicit(&priv->state, memory_order_acquire) < UE_SPLIT7_STATE_UL_READY)
    return UE_SPLIT7_ERR_STATE;

  // TX timestamps derive from the seed_slot_tracking() anchor (see
  // ue_split7_ts_from_frame_slot_symbol()); without it there's no clock reference yet.
  if (!priv->slot_tracking_seeded)
    return UE_SPLIT7_ERR_STATE;

  // frame_number may be the plain 3GPP SFN (0..1023) or already absolute; resolve
  // it onto the same absolute scale as tx_anchor_frame_number before deriving a TX
  // timestamp from it (see ue_split7_ts_from_frame_slot_symbol()).
  const uint32_t abs_frame = ue_split7_resolve_abs_frame(priv->last_rx_abs_frame, frame_number);

  for (uint8_t ls = 0; ls < num_symbols; ls++) {
    uint8_t symbol = start_symbol + ls;
    ue_split7_status_t rc = ue_split7_tx_check_in(dev, client_id, abs_frame, slot_number, symbol, buffers, ls, num_buffers);
    if (rc != UE_SPLIT7_SUCCESS)
      return rc;
  }

  return UE_SPLIT7_SUCCESS;
}

static ue_split7_status_t ue_split7_skip_symbols(struct ue_split7_device *dev,
                                                 uint32_t client_id,
                                                 uint32_t frame_number,
                                                 uint16_t slot_number,
                                                 uint8_t start_symbol,
                                                 uint8_t num_symbols)
{
  if (!dev || num_symbols == 0 || client_id >= UE_SPLIT7_MAX_CLIENTS)
    return UE_SPLIT7_ERR_INVALID_PARAM;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;
  const NR_DL_FRAME_PARMS *fp = priv->fp;

  if ((uint32_t)start_symbol + num_symbols > fp->symbols_per_slot)
    return UE_SPLIT7_ERR_INVALID_PARAM;

  if (atomic_load_explicit(&priv->state, memory_order_acquire) < UE_SPLIT7_STATE_UL_READY)
    return UE_SPLIT7_ERR_STATE;

  if (!priv->slot_tracking_seeded)
    return UE_SPLIT7_ERR_STATE;

  const uint32_t abs_frame = ue_split7_resolve_abs_frame(priv->last_rx_abs_frame, frame_number);
  const uint16_t num_buffers = dev->config.num_tx_antennas;

  for (uint8_t ls = 0; ls < num_symbols; ls++) {
    uint8_t symbol = start_symbol + ls;
    ue_split7_status_t rc = ue_split7_tx_check_in(dev, client_id, abs_frame, slot_number, symbol, NULL, 0, num_buffers);
    if (rc != UE_SPLIT7_SUCCESS)
      return rc;
  }

  return UE_SPLIT7_SUCCESS;
}

static ue_split7_status_t ue_split7_set_timing_advance(struct ue_split7_device *dev, uint32_t ta_samples)
{
  if (!dev)
    return UE_SPLIT7_ERR_INVALID_PARAM;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;
  atomic_store_explicit(&priv->ta_samples, ta_samples, memory_order_relaxed);

  // First call (right after DL sync) establishes the UL offset, making write_prach()/
  // write_symbols()/skip_symbols() valid -- for every client, indefinitely; later TA
  // updates just update ta_samples without touching state.
  int expected = UE_SPLIT7_STATE_DL_SYNCED;
  atomic_compare_exchange_strong_explicit(&priv->state,
                                          &expected,
                                          UE_SPLIT7_STATE_UL_READY,
                                          memory_order_release,
                                          memory_order_relaxed);

  return UE_SPLIT7_SUCCESS;
}

static ue_split7_status_t ue_split7_adjust_rx_timing_offset(struct ue_split7_device *dev, int32_t sample_shift_samples)
{
  if (!dev)
    return UE_SPLIT7_ERR_INVALID_PARAM;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;

  if (atomic_load_explicit(&priv->state, memory_order_acquire) < UE_SPLIT7_STATE_DL_SYNCED)
    return UE_SPLIT7_ERR_STATE;

  atomic_fetch_add_explicit(&priv->rx_timing_offset, sample_shift_samples, memory_order_acq_rel);
  // RX timing shift adjusts the frame boundary, which inherently affects UL (TX) timing as well.
  atomic_fetch_add_explicit(&priv->tx_timing_offset, sample_shift_samples, memory_order_acq_rel);

  pthread_mutex_lock(&priv->rx_mutex);
  if (priv->symbol_fft_done) {
    for (uint32_t s = 0; s < priv->rx_fd_ring_symbols; s++) {
      priv->symbol_fft_done[s] = false;
      priv->symbol_frame_number[s] = UINT32_MAX;
    }
  }
  pthread_mutex_unlock(&priv->rx_mutex);

  return UE_SPLIT7_SUCCESS;
}

static ue_split7_status_t ue_split7_adjust_tx_timing_offset(struct ue_split7_device *dev, int32_t sample_shift_samples)
{
  if (!dev)
    return UE_SPLIT7_ERR_INVALID_PARAM;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;

  if (atomic_load_explicit(&priv->state, memory_order_acquire) < UE_SPLIT7_STATE_DL_SYNCED)
    return UE_SPLIT7_ERR_STATE;

  atomic_fetch_add_explicit(&priv->tx_timing_offset, sample_shift_samples, memory_order_acq_rel);
  return UE_SPLIT7_SUCCESS;
}

static ue_split7_status_t ue_split7_seed_slot_tracking(struct ue_split7_device *dev, uint32_t client_id, uint32_t frame_number)
{
  if (!dev || client_id >= UE_SPLIT7_MAX_CLIENTS)
    return UE_SPLIT7_ERR_INVALID_PARAM;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;

  if (atomic_load_explicit(&priv->state, memory_order_acquire) < UE_SPLIT7_STATE_DL_SYNCED)
    return UE_SPLIT7_ERR_STATE;

  pthread_mutex_lock(&priv->tx_combine_mutex);

  // The RF clock/TA are shared across every co-located client -- only the
  // first-ever seed_slot_tracking() call (from whichever client happens to get
  // there first) actually anchors anything; later calls just join the mask below.
  if (!priv->anchor_seeded) {
    // next_read_ts already sits at slot 0 symbol 0 of frame_number (sync_task_func()'s
    // snap); anchor the slot tracker there. It doubles as wait_next_slot()'s RX
    // cursor (see ue_split7_rx_slot_td()), so no separate sample-position field is needed.
    priv->next_frame_number = frame_number;
    priv->next_slot_number = 0;

    // write_symbols()'s fixed reference point -- unlike next_frame_number above,
    // never advanced/skipped afterwards.
    priv->tx_anchor_frame_number = priv->next_frame_number;
    priv->tx_anchor_sample_pos = priv->next_read_ts;

    // Reference point ue_split7_resolve_abs_frame() unwraps caller-supplied frame
    // numbers against, until the first wait_next_slot() call updates it for real.
    priv->last_rx_abs_frame = frame_number;
    priv->last_rx_slot = 0;

    // Nothing primed under the new anchor yet.
    priv->td_rx_done_frame_number = UINT32_MAX;
    priv->td_rx_done_slot_number = 0;

    priv->slot_tracking_seeded = true;
    priv->anchor_seeded = true;
  }

  // From now on, every write_symbols()/skip_symbols() combine round for every
  // symbol expects to hear from this client.
  priv->synced_mask |= (1u << client_id);

  pthread_mutex_unlock(&priv->tx_combine_mutex);

  return UE_SPLIT7_SUCCESS;
}

// Receives one whole slot's TD samples (all symbols_per_slot of them, not just a
// subrange) into rx_td_buf at the correct 2-frame-buffer offset for
// (frame_number, slot_number), then marks every symbol in that slot as
// FFT-not-yet-done. Called from wait_next_slot(), which now owns the RX cursor
// (ue_split7_rx_time_domain()'s ring-wait, or a direct trx_read_func() call if
// the background read thread isn't running); read_symbols() only does the FFT
// afterwards when it finds this exact (frame, slot) already primed here.
static ue_split7_status_t ue_split7_rx_slot_td(struct ue_split7_device *dev, uint32_t frame_number, uint16_t slot_number)
{
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;
  const NR_DL_FRAME_PARMS *fp = priv->fp;
  uint16_t num_buffers = dev->config.num_rx_antennas;

  uint32_t frame_offset = (frame_number % 2) * fp->samples_per_frame;
  uint32_t slot_offset = get_samples_slot_timestamp(fp, slot_number);
  uint32_t td_start_sample = frame_offset + slot_offset;
  uint32_t total_samples = get_samples_per_slot((int)slot_number, fp);

  int16_t *ant_bufs[UE_SPLIT7_MAX_ANT];
  for (uint16_t i = 0; i < num_buffers; i++)
    ant_bufs[i] = (int16_t *)&priv->rx_td_buf[i][td_start_sample];

  pthread_mutex_lock(&priv->rx_mutex);

  openair0_timestamp_t ts = 0;
  int rc = ue_split7_rx_time_domain(priv, ant_bufs, total_samples, num_buffers, &ts);
  if (rc < 0) {
    pthread_mutex_unlock(&priv->rx_mutex);
    return UE_SPLIT7_ERR_GENERIC;
  }

  uint32_t ring_base = ((frame_number % 2) * fp->slots_per_frame + slot_number) * fp->symbols_per_slot;
  for (uint16_t s = 0; s < fp->symbols_per_slot; s++) {
    uint32_t ring_sym = (ring_base + s) % priv->rx_fd_ring_symbols;
    priv->symbol_fft_done[ring_sym] = false;
    priv->symbol_frame_number[ring_sym] = frame_number;
  }

  priv->td_rx_done_frame_number = frame_number;
  priv->td_rx_done_slot_number = slot_number;
  // Reference point ue_split7_resolve_abs_frame() unwraps future caller-supplied
  // frame numbers against, until the next wait_next_slot() call updates it again.
  priv->last_rx_abs_frame = frame_number;
  priv->last_rx_slot = slot_number;

  // Re-anchor the TX timestamp basis only on an actual clock discontinuity
  // (clock_jumped set -- see its declaration for exactly which conditions
  // qualify and why an unconditional per-slot refresh would be both redundant
  // and hazardous for a multi-symbol burst in flight).
  //
  // tx_anchor_sample_pos must be the timestamp of SLOT 0 of tx_anchor_frame_number
  // (that's what ue_split7_ts_from_frame_slot_symbol() adds get_samples_slot_duration()
  // on top of) -- ts is this slot's own timestamp, which is slot_number != 0 on every
  // call but the first, so it has to be normalized back to slot 0 here.
  //
  // tx_combine_mutex (not rx_mutex, already held here for the RX-side state above)
  // is what ue_split7_ts_from_frame_slot_symbol() takes to read this pair back, on
  // whichever actor/worker thread is finalizing a TX symbol concurrently with this
  // (FD-UE thread) call -- without it, a reader could observe frame_number from one
  // refresh paired with sample_pos from another (or an earlier one still), i.e. a
  // torn read of two independently-written fields.
  if (atomic_exchange_explicit(&priv->clock_jumped, false, memory_order_acq_rel)) {
    pthread_mutex_lock(&priv->tx_combine_mutex);
    priv->tx_anchor_frame_number = frame_number;
    priv->tx_anchor_sample_pos = ts - get_samples_slot_duration(fp, 0, slot_number);
    pthread_mutex_unlock(&priv->tx_combine_mutex);
  }

  pthread_mutex_unlock(&priv->rx_mutex);
  return UE_SPLIT7_SUCCESS;
}

static ue_split7_status_t ue_split7_wait_next_slot(struct ue_split7_device *dev, uint32_t *frame_number, uint16_t *slot_number)
{
  if (!dev || !frame_number || !slot_number)
    return UE_SPLIT7_ERR_INVALID_PARAM;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;

  if (!priv->slot_tracking_seeded)
    return UE_SPLIT7_ERR_STATE;

  const NR_DL_FRAME_PARMS *fp = priv->fp;

  // If the next slot has already aged out of the ring, skip forward to whatever
  // slot is live now instead of receiving/returning stale data. Only the
  // background read thread ages the ring, so this never triggers for a
  // standalone (not started) device, where latest_write_ts never advances.
  uint64_t live_edge = atomic_load_explicit(&priv->latest_write_ts, memory_order_acquire);
  if (live_edge > priv->circ_buf_size && priv->next_read_ts < live_edge - priv->circ_buf_size) {
    uint64_t behind_samples = live_edge - priv->next_read_ts;
    LOG_W(PHY,
          "[split7] FD layer too slow: next slot (frame %u slot %u) is %llu samples "
          "behind the live edge (ring size %u) -- skipping ahead to live\n",
          priv->next_frame_number,
          priv->next_slot_number,
          (unsigned long long)behind_samples,
          priv->circ_buf_size);
    // One slot's margin inside the ring, since more samples for the current slot
    // may still be in flight. next_read_ts and next_slot_number/next_frame_number
    // advance in lockstep here, via the same get_samples_per_slot() the TX
    // anchor formula itself is built on (see clock_jumped's declaration), so
    // this loop doesn't need to flag a clock discontinuity -- it's just many
    // sample-position <-> (frame,slot)-consistent steps taken at once instead
    // of one per real-time tick.
    uint64_t target_pos = live_edge - priv->circ_buf_size + get_samples_per_slot(0, fp);
    while (priv->next_read_ts < target_pos) {
      priv->next_read_ts += get_samples_per_slot((int)priv->next_slot_number, fp);
      priv->next_slot_number++;
      if (priv->next_slot_number >= fp->slots_per_frame) {
        priv->next_slot_number = 0;
        priv->next_frame_number++;
      }
    }
  }

  const uint32_t this_frame = priv->next_frame_number;
  const uint16_t this_slot = priv->next_slot_number;

  // The actual blocking call: RX this slot's TD samples and reset its FD buffer
  // state to not-yet-processed. ue_split7_rx_time_domain() (inside) advances
  // next_read_ts itself, in lockstep with next_frame_number/next_slot_number below.
  ue_split7_status_t rc = ue_split7_rx_slot_td(dev, this_frame, this_slot);
  if (rc != UE_SPLIT7_SUCCESS)
    return rc;

  *frame_number = this_frame;
  *slot_number = this_slot;

  priv->next_slot_number++;
  if (priv->next_slot_number >= fp->slots_per_frame) {
    priv->next_slot_number = 0;
    priv->next_frame_number++;
  }

  return UE_SPLIT7_SUCCESS;
}

// Runs capture/analyze/retry to success or timeout in one pushTpool() task. Each retry
// captures from the current live edge, not a stale anchor, so a slow search can't deadlock the read thread.
static void sync_task_func(void *arg)
{
  struct ue_split7_device *dev = (struct ue_split7_device *)arg;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;

  const NR_DL_FRAME_PARMS *fp = priv->fp;
  uint32_t fft_size = fp->ofdm_symbol_size;
  int mu = fp->numerology_index;
  int N_RB_DL = fp->N_RB_DL;
  int num_rx = fp->nb_antennas_rx > 0 ? fp->nb_antennas_rx : 1;

  uint32_t capture_samples = split7_sync_capture_samples(fp);
  uint32_t alloc_samples = capture_samples + fft_size;

  // Retry-invariant setup, allocated once rather than per attempt.
  int16_t *rx_bufs[8] = {NULL};
  c16_t *rx_ptrs_c16[8] = {NULL};
  for (int ch = 0; ch < num_rx; ch++) {
    rx_bufs[ch] = (int16_t *)malloc_or_fail(alloc_samples * 2 * sizeof(int16_t));
    rx_ptrs_c16[ch] = (c16_t *)rx_bufs[ch];
  }

  c16_t *rxdataF_buf = (c16_t *)malloc16(4 * num_rx * fft_size * sizeof(c16_t));
  c16_t *pssTime_buf = (c16_t *)malloc16(3 * fft_size * sizeof(c16_t));
  if (!rxdataF_buf || !pssTime_buf) {
    if (rxdataF_buf)
      free(rxdataF_buf);
    if (pssTime_buf)
      free(pssTime_buf);
    for (int ch = 0; ch < num_rx; ch++)
      if (rx_bufs[ch])
        free(rx_bufs[ch]);
    priv->sync_cb(dev, UE_SPLIT7_ERR_NO_MEMORY, NULL, priv->sync_user_data);
    priv->sync_active = false;
    atomic_store_explicit(&priv->sync_task_running, false, memory_order_release);
    return;
  }

  nr_gscn_info_t gscn_list[MAX_GSCN_BAND];
  int num_gscn = 0;
  if (dev->config.nr_band > 0) {
    num_gscn = get_scan_ssb_first_sc((double)fp->dl_CarrierFreq, N_RB_DL, (int)dev->config.nr_band, mu, gscn_list);
  }
  if (num_gscn <= 0) {
    gscn_list[0].ssbFirstSC = 0;
    gscn_list[0].gscn = 0;
    num_gscn = 1;
  }

  nr_ssb_search_params_t search_params;
  memset(&search_params, 0, sizeof(search_params));
  search_params.dl_CarrierFreq = fp->dl_CarrierFreq;
  search_params.sampling_rate = fp->samples_per_subframe * 1000;
  search_params.slots_per_frame = fp->slots_per_frame;
  search_params.slots_per_subframe = fp->slots_per_subframe;
  search_params.numerology_index = mu;
  search_params.ofdm_symbol_size = fft_size;
  search_params.ofdm_offset_divisor = fp->ofdm_offset_divisor;
  search_params.nb_antennas_rx = num_rx;
  search_params.symbols_per_slot = fp->symbols_per_slot;
  search_params.first_carrier_offset = fp->first_carrier_offset;
  search_params.N_RB_DL = N_RB_DL;
  search_params.nb_prefix_samples = fp->nb_prefix_samples;
  search_params.nb_prefix_samples0 = fp->nb_prefix_samples0;
  search_params.subcarrier_spacing = fp->subcarrier_spacing;
  search_params.samples_per_slot_wCP = fp->samples_per_slot_wCP;
  search_params.target_nid_cell = priv->sync_cfg.expected_pci;
  // exclude_nid_cells/num_exclude_nid_cells left NULL/0: this search never excludes cells.
  search_params.apply_freq_offset = true;
  search_params.fo_flag = true;
  search_params.rxdataF = rxdataF_buf;
  search_params.pssTime = pssTime_buf;
  search_params.rxdata = rx_ptrs_c16;
  // Must equal capture_samples, not alloc_samples: pss_search_time_nr()'s correlation loop
  // reads up to fft_size samples past rxdata_size, and alloc_samples' extra fft_size of
  // headroom exists to keep that in-bounds -- using alloc_samples here would overrun it.
  search_params.rxdata_size = capture_samples;

  c16_t(*pssTime_ptr)[fft_size] = (c16_t(*)[fft_size])pssTime_buf;
  c16_t(*rxdataF_4sym)[num_rx][fft_size] = (c16_t(*)[num_rx][fft_size])rxdataF_buf;

  for (;;) {
    if (!priv->sync_active) // cooperative cancellation via stop_sync()
      break;

    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    uint64_t elapsed_ms = (current_time.tv_sec - priv->sync_start_time.tv_sec) * 1000
                          + (current_time.tv_nsec - priv->sync_start_time.tv_nsec) / 1000000;
    if (priv->sync_cfg.timeout_ms > 0 && elapsed_ms >= priv->sync_cfg.timeout_ms) {
      LOG_W(PHY, "Sync search timed out after %u ms\n", priv->sync_cfg.timeout_ms);
      priv->sync_cb(dev, UE_SPLIT7_ERR_TIMEOUT, NULL, priv->sync_user_data);
      priv->sync_active = false;
      break;
    }

    uint64_t live_edge = atomic_load_explicit(&priv->latest_write_ts, memory_order_acquire);
    openair0_timestamp_t sync_start_ts = (live_edge > alloc_samples) ? (live_edge - alloc_samples) : 0;

    for (int ch = 0; ch < num_rx; ch++)
      read_channel_at_ts(priv, rx_bufs[ch], ch, sync_start_ts, alloc_samples);

    // Mirrors nr_scan_ssb()'s ssbInfo->pbchResult/halfFrameBit/ssbIndex.
    fapiPbch_t pbch_result;
    memset(&pbch_result, 0, sizeof(pbch_result));
    int half_frame_bit = 0, ssb_index = 0, symbol_offset = 0;
    int32_t freq_offset = 0;
    int64_t timing_offset = 0;
    uint16_t cell_id = 0;

    bool ssb_found = false;
    int ssb_start_subcarrier = 0;
    for (int g = 0; g < num_gscn && !ssb_found; g++) {
      ssb_start_subcarrier = gscn_list[g].ssbFirstSC;
      for (int nid2 = 0; nid2 < 3; nid2++)
        generate_pss_nr_time(fft_size, fp->first_carrier_offset, nid2, ssb_start_subcarrier, pssTime_ptr[nid2]);
      memset(rxdataF_buf, 0, 4 * num_rx * fft_size * sizeof(c16_t));
      search_params.ssb_start_subcarrier = ssb_start_subcarrier;
      ssb_found = nr_search_ssb_common(&search_params);
      LOG_I(PHY,
            "SSB search (GSCN %d ssbFirstSC %d): ssb_found=%d pss_success=%d nid2=%d pos=%d peak=%d avg=%d\n",
            gscn_list[g].gscn,
            ssb_start_subcarrier,
            ssb_found,
            search_params.pss_res.success,
            search_params.pss_res.nid2,
            search_params.pss_res.pos,
            search_params.pss_res.peak,
            search_params.pss_res.avg);

      // Require PBCH/MIB decode too, as nr_scan_ssb() does: PSS/SSS alone gives no SFN.
      if (ssb_found) {
        const UE_nr_rxtx_proc_t dummy_proc = {0};
        ssb_found = nr_pbch_detection(&dummy_proc,
                                      priv->fp,
                                      search_params.sss_res.nid_cell,
                                      1, // pbch_initial_symbol: symbol 0 is PSS
                                      ssb_start_subcarrier,
                                      &half_frame_bit,
                                      &ssb_index,
                                      &symbol_offset,
                                      &pbch_result,
                                      rxdataF_4sym);
        LOG_I(PHY,
              "PBCH detection (GSCN %d): mib_decoded=%d ssb_index=%d half_frame_bit=%d\n",
              gscn_list[g].gscn,
              ssb_found,
              ssb_index,
              half_frame_bit);
      }
    }

    if (!ssb_found)
      continue; // immediately try again -- the next capture is already fresh

    cell_id = search_params.sss_res.nid_cell;
    // pss_res.pos is measured to the correlation peak, one CP length past the true symbol start.
    timing_offset = (int64_t)sync_start_ts + search_params.pss_res.pos - search_params.nb_prefix_samples;
    freq_offset = search_params.pss_res.freq_offset + search_params.sss_res.freq_offset;

    // Snap to slot 0 symbol 0 of the SSB's frame (38.211 §5.3.1 CP0-vs-normal CP accounting).
    const int n_symb_prefix0 = (symbol_offset / (7 * (1 << mu))) + 1;
    const int64_t sync_pos_frame = (int64_t)n_symb_prefix0 * (fft_size + fp->nb_prefix_samples0)
                                   + (int64_t)(symbol_offset - n_symb_prefix0) * (fft_size + fp->nb_prefix_samples);
    const int64_t samples_per_frame = (int64_t)fp->samples_per_frame;
    int64_t frame_boundary = (int64_t)timing_offset - sync_pos_frame;
    if (frame_boundary < 0)
      frame_boundary += samples_per_frame;

    // Correlation work took real time since capture; advance by whole frames
    // (preserving phase) to the latest boundary at or before the current live edge.
    int64_t frames_elapsed = 0;
    const uint64_t live_edge_now = atomic_load_explicit(&priv->latest_write_ts, memory_order_acquire);
    if (live_edge_now > (uint64_t)frame_boundary)
      frames_elapsed = (int64_t)((live_edge_now - (uint64_t)frame_boundary) / (uint64_t)samples_per_frame);
    frame_boundary += frames_elapsed * samples_per_frame;

    // next_read_ts is the shared RX cursor wait_next_slot() drives off of (see its
    // comment) -- once the device has already reached DL_SYNCED via an earlier
    // client's own sync, that cursor is already live and tracking real time on its
    // own; a later client's own independent measurement of "where slot 0 symbol 0
    // is" must not jump it out from under whatever's already synced to it.
    if (atomic_load_explicit(&priv->state, memory_order_acquire) < UE_SPLIT7_STATE_DL_SYNCED)
      priv->next_read_ts = (uint64_t)frame_boundary;

    LOG_I(PHY,
          "[sync] symbol_offset=%d frame_boundary=%lld frames_elapsed=%lld\n",
          symbol_offset,
          (long long)frame_boundary,
          (long long)frames_elapsed);

    if (!priv->sync_active)
      break;

    // openair0 convention: positive freq_offset means the carrier was above nominal.
    // The RF frequency is shared across every co-located client (one physical RF
    // chain): once the device has already reached DL_SYNCED via an earlier
    // client's own sync, a later client's own independent CFO measurement is just
    // residual estimation noise around that already-converged frequency, not a
    // fresh drift to correct -- retuning on it here would clobber the already-
    // synced client(s)' lock. Only the very first successful sync ever steers the LO.
    if (freq_offset != 0 && priv->openair0_dev.trx_set_freq_func
        && atomic_load_explicit(&priv->state, memory_order_acquire) < UE_SPLIT7_STATE_DL_SYNCED) {
      priv->openair0_cfg.rx_freq[0] += freq_offset;
      priv->openair0_cfg.tx_freq[0] += freq_offset;
      priv->openair0_dev.trx_set_freq_func(&priv->openair0_dev, &priv->openair0_cfg);
      LOG_I(PHY, "[sync] Applied CFO correction %d Hz; new RX freq %.3f MHz\n", freq_offset, priv->openair0_cfg.rx_freq[0] / 1e6);
    }
    priv->last_freq_offset_hz = freq_offset;

    ue_split7_sync_result_t result;
    memset(&result, 0, sizeof(result));
    result.physical_cell_id = cell_id;
    result.best_ssb_index = (uint8_t)ssb_index;
    result.timing_offset_samples = timing_offset;
    result.freq_offset_hz = freq_offset;
    result.ssb_rsrp_dbm = -75.0f;
    result.mib_decoded = true; // ssb_found already required PBCH decode above
    memcpy(result.mib_payload, pbch_result.decoded_output, sizeof(result.mib_payload));
    result.mib_additional_bits = pbch_result.xtra_byte;
    result.half_frame_bit = half_frame_bit;
    result.symbol_offset = symbol_offset;
    result.ssb_start_subcarrier = (uint16_t)ssb_start_subcarrier;
    result.frames_since_capture = (uint32_t)(frames_elapsed % 1024);

    // CAS-advance only: with multiple co-located clients, a later client's sync
    // must not regress an already-UL_READY device back to DL_SYNCED.
    int cur_state = atomic_load_explicit(&priv->state, memory_order_relaxed);
    while (cur_state < UE_SPLIT7_STATE_DL_SYNCED
           && !atomic_compare_exchange_weak_explicit(&priv->state,
                                                     &cur_state,
                                                     UE_SPLIT7_STATE_DL_SYNCED,
                                                     memory_order_release,
                                                     memory_order_relaxed)) {
    }

    priv->sync_cb(dev, UE_SPLIT7_SUCCESS, &result, priv->sync_user_data);
    priv->sync_active = false;
    break;
  }

  free(rxdataF_buf);
  free(pssTime_buf);
  for (int ch = 0; ch < num_rx; ch++)
    free(rx_bufs[ch]);

  atomic_store_explicit(&priv->sync_task_running, false, memory_order_release);
}

static ue_split7_status_t ue_split7_start_sync(struct ue_split7_device *dev,
                                               const ue_split7_sync_config_t *sync_config,
                                               void *pool,
                                               ue_split7_sync_callback_t callback,
                                               void *user_data)
{
  if (!dev || !sync_config || !pool || !callback)
    return UE_SPLIT7_ERR_INVALID_PARAM;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;

  if (priv->sync_active)
    return UE_SPLIT7_ERR_BUSY;

  // Guard against a just-finished task whose callback fired but whose cleanup hasn't run yet.
  while (atomic_load_explicit(&priv->sync_task_running, memory_order_acquire))
    usleep(1000);

  // Auto-start device if not started yet
  if (!priv->is_started) {
    ue_split7_status_t rc = ue_split7_start(dev);
    if (rc != UE_SPLIT7_SUCCESS)
      return rc;
  }

  priv->sync_cfg = *sync_config;
  priv->sync_pool = (tpool_t *)pool;
  priv->sync_cb = callback;
  priv->sync_user_data = user_data;
  clock_gettime(CLOCK_MONOTONIC, &priv->sync_start_time);
  priv->sync_active = true;
  atomic_store_explicit(&priv->sync_task_running, true, memory_order_release);

  task_t task = {.func = sync_task_func, .args = dev};
  pushTpool(priv->sync_pool, task);

  return UE_SPLIT7_SUCCESS;
}

static ue_split7_status_t ue_split7_stop_sync(struct ue_split7_device *dev)
{
  if (!dev)
    return UE_SPLIT7_ERR_INVALID_PARAM;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;
  priv->sync_active = false;
  while (atomic_load_explicit(&priv->sync_task_running, memory_order_acquire)) {
    usleep(1000);
  }
  return UE_SPLIT7_SUCCESS;
}

// client_id is accepted for tracing/API symmetry but not otherwise load-bearing:
// the RF clock and Timing Advance are shared across every co-located client, so
// the enqueued waveform's absolute sample position doesn't depend on who called.
static ue_split7_status_t ue_split7_write_prach(struct ue_split7_device *dev, uint32_t client_id, const ue_split7_prach_tx_params_t *params)
{
  if (!dev || !params || client_id >= UE_SPLIT7_MAX_CLIENTS)
    return UE_SPLIT7_ERR_INVALID_PARAM;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;

  if (atomic_load_explicit(&priv->state, memory_order_acquire) < UE_SPLIT7_STATE_UL_READY)
    return UE_SPLIT7_ERR_STATE;

  if (!priv->slot_tracking_seeded)
    return UE_SPLIT7_ERR_STATE;

  uint32_t fft_size = params->fft_size > 0 ? params->fft_size : dev->config.fft_size;
  idft_size_idx_t idft_size = get_idft(fft_size);

  int16_t *temp_fft_in = (int16_t *)malloc16(fft_size * 2 * sizeof(int16_t));
  if (!temp_fft_in)
    return UE_SPLIT7_ERR_NO_MEMORY;
  memset(temp_fft_in, 0, fft_size * 2 * sizeof(int16_t));

  if (params->num_samples == fft_size) {
    for (uint32_t k = 0; k < fft_size; ++k) {
      temp_fft_in[k * 2] = params->samples[k].r;
      temp_fft_in[k * 2 + 1] = params->samples[k].i;
    }
  } else {
    int32_t center_sc = params->frequency_offset_scs;
    for (uint32_t k = 0; k < params->num_samples; ++k) {
      int32_t sc_idx = (int32_t)k - (int32_t)params->num_samples / 2 + center_sc;
      while (sc_idx < 0)
        sc_idx += fft_size;
      sc_idx = sc_idx % fft_size;

      temp_fft_in[sc_idx * 2] = params->samples[k].r;
      temp_fft_in[sc_idx * 2 + 1] = params->samples[k].i;
    }
  }

  int16_t *temp_ifft_out = (int16_t *)malloc16(fft_size * 2 * sizeof(int16_t));
  if (!temp_ifft_out) {
    free(temp_fft_in);
    return UE_SPLIT7_ERR_NO_MEMORY;
  }

  idft(idft_size, temp_fft_in, temp_ifft_out, 1);

  uint32_t cp_len = params->cp_len_samples;
  uint32_t repetition = params->repetition_count > 0 ? params->repetition_count : 1;
  uint32_t total_samples = cp_len + fft_size * repetition;

  // Owned by the queue entry below (potentially outlives this call by however
  // long it takes ue_split7_overlay_prach() to stream the whole thing out,
  // symbol by symbol) -- unlike write_symbols()'s tx_td_buf, this can't reuse a
  // device-owned scratch buffer, since the combine step drives that one.
  // Only ONE repetition is materialized here -- [CP][one fft_size cycle] --
  // ue_split7_overlay_prach() replays the fft_size portion via modulo for
  // however many repetitions total_samples calls for, instead of
  // pre-expanding every repetition into one giant buffer up front.
  int16_t *prach_time_buf = (int16_t *)malloc_or_fail((cp_len + fft_size) * 2 * sizeof(int16_t));

  uint32_t cp_copy_len = (cp_len < fft_size) ? cp_len : fft_size;
  memcpy(prach_time_buf, &temp_ifft_out[(fft_size - cp_copy_len) * 2], cp_copy_len * 2 * sizeof(int16_t));
  if (cp_len > fft_size) {
    memset(&prach_time_buf[cp_copy_len * 2], 0, (cp_len - cp_copy_len) * 2 * sizeof(int16_t));
  }

  memcpy(&prach_time_buf[cp_len * 2], temp_ifft_out, fft_size * 2 * sizeof(int16_t));

  free(temp_fft_in);
  free(temp_ifft_out);

  // params->frame_number may be the plain 3GPP SFN (0..1023) or already absolute;
  // resolve it onto the same absolute scale as tx_anchor_frame_number before
  // deriving this waveform's absolute RF sample position.
  const uint32_t abs_frame = ue_split7_resolve_abs_frame(priv->last_rx_abs_frame, params->frame_number);

  // base_ts is the absolute RF sample position of (abs_frame, params->slot_number,
  // params->symbol_number)'s own start -- params->slot_number is already the resolved
  // TX slot (see ue_split7_finalize_tx_symbol()'s comment for why no further
  // RX-to-TX projection is added here), matching write_symbols()'s convention -- this
  // is the exact nominal (pre-tx_timing_offset) coordinate
  // ue_split7_overlay_prach() compares against each symbol's own tx_ts.
  uint64_t base_ts = ue_split7_ts_from_frame_slot_symbol(priv, abs_frame, params->slot_number, params->symbol_number);
  int64_t ta_now = (int64_t)atomic_load_explicit(&priv->ta_samples, memory_order_relaxed);
  uint64_t abs_start_sample = (uint64_t)((int64_t)base_ts + params->time_offset_samples - ta_now);

  pthread_mutex_lock(&priv->prach_queue_mutex);
  int slot_idx = -1;
  for (int q = 0; q < UE_SPLIT7_MAX_PRACH_QUEUE; q++) {
    if (!priv->prach_queue[q].active) {
      slot_idx = q;
      break;
    }
  }
  if (slot_idx < 0) {
    pthread_mutex_unlock(&priv->prach_queue_mutex);
    free(prach_time_buf);
    LOG_E(PHY, "ue_split7_write_prach: PRACH queue full (%d entries)\n", UE_SPLIT7_MAX_PRACH_QUEUE);
    return UE_SPLIT7_ERR_BUSY;
  }
  priv->prach_queue[slot_idx].active = true;
  priv->prach_queue[slot_idx].abs_start_sample = abs_start_sample;
  priv->prach_queue[slot_idx].cp_len = cp_len;
  priv->prach_queue[slot_idx].fft_size = fft_size;
  priv->prach_queue[slot_idx].total_samples = total_samples;
  priv->prach_queue[slot_idx].emitted_samples = 0;
  priv->prach_queue[slot_idx].samples = prach_time_buf;
  pthread_mutex_unlock(&priv->prach_queue_mutex);

  return UE_SPLIT7_SUCCESS;
}

ue_split7_device_t *ue_split7_device_create(void)
{
  ue_split7_device_t *dev = (ue_split7_device_t *)malloc_or_fail(sizeof(ue_split7_device_t));
  memset(dev, 0, sizeof(ue_split7_device_t));

  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)malloc_or_fail(sizeof(ue_split7_device_priv_t));
  memset(priv, 0, sizeof(ue_split7_device_priv_t)); // state == UE_SPLIT7_STATE_UNSYNCED (0)
  pthread_mutex_init(&priv->write_mutex, NULL);
  pthread_mutex_init(&priv->data_mutex, NULL);
  pthread_mutex_init(&priv->rx_mutex, NULL);
  pthread_cond_init(&priv->data_cond, NULL);
  pthread_mutex_init(&priv->tx_combine_mutex, NULL);
  pthread_mutex_init(&priv->prach_queue_mutex, NULL);

  dev->priv = priv;
  dev->configure = ue_split7_configure;
  dev->start = ue_split7_start;
  dev->stop = ue_split7_stop;
  dev->read_symbols = ue_split7_read_symbols;
  dev->write_symbols = ue_split7_write_symbols;
  dev->skip_symbols = ue_split7_skip_symbols;
  dev->start_sync = ue_split7_start_sync;
  dev->stop_sync = ue_split7_stop_sync;
  dev->set_timing_advance = ue_split7_set_timing_advance;
  dev->adjust_rx_timing_offset = ue_split7_adjust_rx_timing_offset;
  dev->adjust_tx_timing_offset = ue_split7_adjust_tx_timing_offset;
  dev->adjust_rx_timing = ue_split7_adjust_rx_timing_offset;
  dev->register_client = ue_split7_register_client;
  dev->seed_slot_tracking = ue_split7_seed_slot_tracking;
  dev->wait_next_slot = ue_split7_wait_next_slot;
  dev->write_prach = ue_split7_write_prach;

  return dev;
}

void ue_split7_device_free(ue_split7_device_t *dev)
{
  if (!dev)
    return;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;
  if (priv) {
    // Safety net in case the caller forgot to stop() first.
    if (dev->stop)
      dev->stop(dev);
    if (priv->rx_time_buf)
      free(priv->rx_time_buf);
    if (priv->tx_time_buf)
      free(priv->tx_time_buf);
    for (int i = 0; i < UE_SPLIT7_MAX_ANT; i++) {
      if (priv->prev_rx_buf[i])
        free(priv->prev_rx_buf[i]);
    }
    if (priv->rx_circ_buf) {
      for (int i = 0; i < dev->config.num_rx_antennas; i++) {
        if (priv->rx_circ_buf[i])
          free(priv->rx_circ_buf[i]);
      }
      free(priv->rx_circ_buf);
    }
    if (priv->rx_td_buf) {
      for (int i = 0; i < dev->config.num_rx_antennas; i++) {
        if (priv->rx_td_buf[i])
          free(priv->rx_td_buf[i]);
      }
      free(priv->rx_td_buf);
      priv->rx_td_buf = NULL;
      dev->rx_td_buffers = NULL;
    }
    if (priv->tx_td_buf) {
      for (int i = 0; i < dev->config.num_tx_antennas; i++) {
        if (priv->tx_td_buf[i])
          free(priv->tx_td_buf[i]);
      }
      free(priv->tx_td_buf);
      priv->tx_td_buf = NULL;
    }
    if (priv->rx_fd_buf) {
      for (int i = 0; i < dev->config.num_rx_antennas; i++) {
        if (priv->rx_fd_buf[i])
          free(priv->rx_fd_buf[i]);
      }
      free(priv->rx_fd_buf);
      priv->rx_fd_buf = NULL;
      dev->rx_fd_buffers = NULL;
    }
    if (priv->symbol_fft_done) {
      free(priv->symbol_fft_done);
      priv->symbol_fft_done = NULL;
    }
    if (priv->symbol_frame_number) {
      free(priv->symbol_frame_number);
      priv->symbol_frame_number = NULL;
    }
    if (priv->tx_fd_accum) {
      for (int i = 0; i < dev->config.num_tx_antennas; i++) {
        if (priv->tx_fd_accum[i])
          free(priv->tx_fd_accum[i]);
      }
      free(priv->tx_fd_accum);
      priv->tx_fd_accum = NULL;
    }
    if (priv->tx_checked_in_mask) {
      free(priv->tx_checked_in_mask);
      priv->tx_checked_in_mask = NULL;
    }
    if (priv->tx_expected_mask) {
      free(priv->tx_expected_mask);
      priv->tx_expected_mask = NULL;
    }
    for (int q = 0; q < UE_SPLIT7_MAX_PRACH_QUEUE; q++) {
      if (priv->prach_queue[q].active && priv->prach_queue[q].samples)
        free(priv->prach_queue[q].samples);
    }
    pthread_mutex_destroy(&priv->write_mutex);
    pthread_mutex_destroy(&priv->data_mutex);
    pthread_mutex_destroy(&priv->rx_mutex);
    pthread_cond_destroy(&priv->data_cond);
    pthread_mutex_destroy(&priv->tx_combine_mutex);
    pthread_mutex_destroy(&priv->prach_queue_mutex);
    free(priv);
  }
  free(dev);
}
