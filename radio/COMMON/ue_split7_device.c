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

// Monotonic: UNSYNCED -> DL_SYNCED (read_symbol valid) -> UL_PRACH_ONLY (write_prach
// valid) -> UL_NORMAL (first write_symbol, PRACH no longer valid). Never backward.
typedef enum {
  UE_SPLIT7_STATE_UNSYNCED = 0,
  UE_SPLIT7_STATE_DL_SYNCED = 1,
  UE_SPLIT7_STATE_UL_PRACH_ONLY = 2,
  UE_SPLIT7_STATE_UL_NORMAL = 3,
} ue_split7_state_t;

// Hard cap on RX/TX antenna count: several hot-path functions (read_symbol's
// non-threaded fallback, write_symbol, sync_task_func) use fixed-size stack
// arrays sized to this bound and indexed directly by config.num_rx/tx_antennas,
// so ue_split7_configure() must reject any config exceeding it.
#define UE_SPLIT7_MAX_ANT 8

typedef struct {
  openair0_device_t openair0_dev;
  openair0_config_t openair0_cfg;
  _Atomic uint32_t ta_samples; // written by set_timing_advance() (FD-UE thread),
                               // read by write_symbol()/write_prach() (dl_actors worker thread)
  bool is_started;
  _Atomic int state; // ue_split7_state_t

  // Intermediate buffers for FFT/IFFT processing
  int16_t *rx_time_buf; // intermediate time-domain buffer for read_symbol
  int16_t *tx_time_buf; // intermediate time-domain buffer for write_symbol
  uint32_t buf_size_samples;

  // Time-domain RX ring, addressed directly by RF sample timestamp (ring position ==
  // ts % circ_buf_size), not by an independently-tracked append count.
  int16_t **rx_circ_buf; // rx_circ_buf[channel][sample_index * 2] for complex samples
  uint32_t circ_buf_size; // size in complex samples
  _Atomic uint64_t latest_write_ts; // ts + n of the most recent ring write; read thread writes, others wait on it
  // Consumer-owned read cursor (absolute RF timestamp), touched only from the FD-UE
  // thread; the state acquire/release below (not atomicity) makes the sync task's seed visible.
  uint64_t next_read_ts;

  // Device-owned FD-symbol scratch: read_symbol() DFTs into rx_fd_buf[antenna] and returns
  // a pointer into it. Valid until the next read_symbol() call; the caller must copy out
  // before then if it needs the data to outlive that call.
  c16_t **rx_fd_buf; // rx_fd_buf[antenna][fft_size]

  // Read thread management
  pthread_t read_thread;
  atomic_bool read_running;

  // Broadcast by the read thread after every write so read_symbol()/wait_next_slot()
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
  // reported by the NEXT call, and the sample position its slot starts at.
  // The sole authoritative walk; the host has no independent slot counter.
  bool slot_tracking_seeded;
  uint32_t next_frame_number;
  uint16_t next_slot_number;
  uint64_t next_slot_sample_pos;

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
} ue_split7_device_priv_t;

static void sync_task_func(void *arg);

// aligned_alloc() requires size to be a multiple of the alignment; round up.
static void *split7_aligned_alloc32(size_t size)
{
  return aligned_alloc(32, (size + 31) & ~(size_t)31);
}

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

  int16_t **temp_bufs = (int16_t **)malloc(num_rx * sizeof(int16_t *));
  void **rx_ptrs = (void **)malloc(num_rx * sizeof(void *));
  if (!temp_bufs || !rx_ptrs) {
    if (temp_bufs)
      free(temp_bufs);
    if (rx_ptrs)
      free(rx_ptrs);
    priv->read_running = false;
    return NULL;
  }

  for (int i = 0; i < num_rx; i++) {
    temp_bufs[i] = (int16_t *)malloc(chunk_size * 2 * sizeof(int16_t));
    if (!temp_bufs[i]) {
      for (int k = 0; k < i; k++) {
        free(temp_bufs[k]);
      }
      free(temp_bufs);
      free(rx_ptrs);
      priv->read_running = false;
      return NULL;
    }
  }

  while (priv->read_running) {
    openair0_timestamp_t ts = 0;
    for (int i = 0; i < num_rx; i++) {
      rx_ptrs[i] = temp_bufs[i];
    }

    // Never blocks on a consumer: a slow FD-UE side just sees a staleness warning
    // at the point of use (read_symbol()/wait_next_slot()) instead of stalling ingestion.
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
  priv->rx_time_buf = (int16_t *)split7_aligned_alloc32(priv->buf_size_samples * 2 * sizeof(int16_t));
  priv->tx_time_buf = (int16_t *)split7_aligned_alloc32(priv->buf_size_samples * 2 * sizeof(int16_t));

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

  priv->rx_circ_buf = (int16_t **)malloc(config->num_rx_antennas * sizeof(int16_t *));
  if (!priv->rx_circ_buf) {
    LOG_E(PHY, "Out of memory allocating circular buffer channel array\n");
    return UE_SPLIT7_ERR_NO_MEMORY;
  }

  for (int i = 0; i < config->num_rx_antennas; i++) {
    priv->rx_circ_buf[i] = (int16_t *)split7_aligned_alloc32(priv->circ_buf_size * 2 * sizeof(int16_t));
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

  priv->rx_fd_buf = (c16_t **)malloc(config->num_rx_antennas * sizeof(c16_t *));
  if (!priv->rx_fd_buf) {
    LOG_E(PHY, "Out of memory allocating FD-symbol buffer array\n");
    return UE_SPLIT7_ERR_NO_MEMORY;
  }
  for (int i = 0; i < config->num_rx_antennas; i++) {
    priv->rx_fd_buf[i] = (c16_t *)split7_aligned_alloc32(config->fft_size * sizeof(c16_t));
    if (!priv->rx_fd_buf[i]) {
      for (int k = 0; k < i; k++) {
        free(priv->rx_fd_buf[k]);
      }
      free(priv->rx_fd_buf);
      priv->rx_fd_buf = NULL;
      LOG_E(PHY, "Out of memory allocating FD-symbol buffer\n");
      return UE_SPLIT7_ERR_NO_MEMORY;
    }
  }

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
    // Wake anything blocked in ue_split7_read_symbol()/ue_split7_wait_next_slot()
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

static ue_split7_status_t ue_split7_read_symbol(struct ue_split7_device *dev,
                                                ue_split7_symbol_buffer_t *buffers,
                                                uint16_t num_buffers)
{
  if (!dev || !buffers || num_buffers != dev->config.num_rx_antennas) {
    return UE_SPLIT7_ERR_INVALID_PARAM;
  }
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;

  if (atomic_load_explicit(&priv->state, memory_order_acquire) < UE_SPLIT7_STATE_DL_SYNCED)
    return UE_SPLIT7_ERR_STATE;

  uint16_t symbol = buffers[0].meta.symbol_number;
  uint32_t fft_size = dev->config.fft_size;
  uint32_t total_samples = get_samples_symbol_duration(priv->fp, buffers[0].meta.slot_number, symbol, 1);
  uint32_t cp_len = total_samples - fft_size;

  openair0_timestamp_t ts = 0;
  dft_size_idx_t dft_sz = get_dft(fft_size);

  if (priv->read_running) {
    openair0_timestamp_t target = priv->next_read_ts + total_samples;

    pthread_mutex_lock(&priv->data_mutex);
    while (atomic_load_explicit(&priv->read_running, memory_order_acquire)
           && atomic_load_explicit(&priv->latest_write_ts, memory_order_acquire) < target) {
      pthread_cond_wait(&priv->data_cond, &priv->data_mutex);
    }
    pthread_mutex_unlock(&priv->data_mutex);

    if (!priv->read_running) {
      return UE_SPLIT7_ERR_GENERIC;
    }

    // Ring only holds circ_buf_size samples; if the live edge overran our target,
    // that data's already overwritten (wait_next_slot() is where skip-ahead recovery happens).
    uint64_t live_edge = atomic_load_explicit(&priv->latest_write_ts, memory_order_acquire);
    if (live_edge > priv->circ_buf_size && priv->next_read_ts < live_edge - priv->circ_buf_size) {
      LOG_W(PHY,
            "[split7] FD layer too slow: read_symbol() target ts %llu is already "
            "%llu samples behind the live edge (ring size %u) -- data already overwritten\n",
            (unsigned long long)priv->next_read_ts,
            (unsigned long long)(live_edge - priv->next_read_ts),
            priv->circ_buf_size);
    }

    ts = priv->next_read_ts;

    for (uint16_t i = 0; i < num_buffers; ++i) {
      read_channel_at_ts(priv, priv->rx_time_buf, i, ts, total_samples);
      int16_t *time_ptr = &priv->rx_time_buf[(cp_len - priv->rot_sample_offset) * 2];
      dft(dft_sz, time_ptr, (int16_t *)priv->rx_fd_buf[i], 1);
      apply_nr_rotation_symbol_RX(priv->fp->symbols_per_slot,
                                  priv->fp->slots_per_subframe,
                                  priv->fp->timeshift_symbol_rotation,
                                  priv->fp->first_carrier_offset,
                                  priv->rx_fd_buf[i],
                                  priv->fp->symbol_rotation[link_type_dl],
                                  priv->fp->N_RB_DL,
                                  buffers[i].meta.slot_number,
                                  symbol);
      buffers[i].meta.timestamp_samples = ts;
      buffers[i].num_subcarriers = fft_size;
      buffers[i].re_buffer = (ue_split7_iq_t *)priv->rx_fd_buf[i];
    }

    priv->next_read_ts = target;
  } else {
    int16_t *ant_bufs[8] = {NULL};
    void *rx_ptrs[8] = {NULL};
    bool alloc_ok = true;
    for (int i = 0; i < num_buffers; i++) {
      ant_bufs[i] = (int16_t *)split7_aligned_alloc32(total_samples * 2 * sizeof(int16_t));
      if (!ant_bufs[i]) {
        alloc_ok = false;
        break;
      }
      rx_ptrs[i] = ant_bufs[i];
    }
    int samples_read = -1;
    if (alloc_ok) {
      samples_read = priv->openair0_dev.trx_read_func(&priv->openair0_dev, &ts, rx_ptrs, total_samples, num_buffers);
    }
    if (samples_read < 0) {
      for (int i = 0; i < num_buffers; i++)
        free(ant_bufs[i]);
      return UE_SPLIT7_ERR_GENERIC;
    }
    AssertFatal((uint32_t)samples_read == total_samples,
                "[split7] rfdevice trx_read_func returned %d samples, expected %u\n",
                samples_read,
                total_samples);
    for (uint16_t i = 0; i < num_buffers; ++i) {
      int16_t *time_ptr = &ant_bufs[i][(cp_len - priv->rot_sample_offset) * 2];
      dft(dft_sz, time_ptr, (int16_t *)priv->rx_fd_buf[i], 1);
      apply_nr_rotation_symbol_RX(priv->fp->symbols_per_slot,
                                  priv->fp->slots_per_subframe,
                                  priv->fp->timeshift_symbol_rotation,
                                  priv->fp->first_carrier_offset,
                                  priv->rx_fd_buf[i],
                                  priv->fp->symbol_rotation[link_type_dl],
                                  priv->fp->N_RB_DL,
                                  buffers[i].meta.slot_number,
                                  symbol);
      buffers[i].meta.timestamp_samples = ts;
      buffers[i].num_subcarriers = fft_size;
      buffers[i].re_buffer = (ue_split7_iq_t *)priv->rx_fd_buf[i];
    }
    for (int i = 0; i < num_buffers; i++)
      free(ant_bufs[i]);
  }

  return UE_SPLIT7_SUCCESS;
}

// Reuses get_samples_slot_duration() (nr_parms.c); reconstructs the RX reference slot from
// tx_slot_number since that call needs the start slot to account for symbol-0 CP length.
static uint64_t ue_split7_slots_duration_samples(struct ue_split7_device *dev, uint32_t tx_slot_number, uint32_t num_slots)
{
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;
  const NR_DL_FRAME_PARMS *fp = priv->fp;
  uint32_t slots_per_frame = fp->slots_per_frame;
  uint32_t tx_slot = tx_slot_number % slots_per_frame;
  uint32_t rx_slot = (tx_slot + slots_per_frame - (num_slots % slots_per_frame)) % slots_per_frame;
  return get_samples_slot_duration(fp, rx_slot, num_slots);
}

// Mirrors apply_nr_rotation_TX()'s non-flat-buffer branch (ofdm_mod.c), adapted for a
// buffer already offset to a single symbol (split7's write_symbol() convention).
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

static ue_split7_status_t ue_split7_write_symbol(struct ue_split7_device *dev,
                                                 const ue_split7_symbol_buffer_t *buffers,
                                                 uint16_t num_buffers)
{
  if (!dev || !buffers || num_buffers != dev->config.num_tx_antennas) {
    return UE_SPLIT7_ERR_INVALID_PARAM;
  }
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;

  if (atomic_load_explicit(&priv->state, memory_order_acquire) < UE_SPLIT7_STATE_UL_PRACH_ONLY)
    return UE_SPLIT7_ERR_STATE;

  atomic_store_explicit(&priv->state, UE_SPLIT7_STATE_UL_NORMAL, memory_order_release);

  uint16_t symbol = buffers[0].meta.symbol_number;
  uint32_t fft_size = dev->config.fft_size;
  uint32_t total_samples = get_samples_symbol_duration(priv->fp, buffers[0].meta.slot_number, symbol, 1);
  uint32_t cp_len = total_samples - fft_size;

  idft_size_idx_t idft_size = get_idft(fft_size);

  int16_t *tx_ant_bufs[8] = {NULL};
  void *tx_ptrs[8] = {NULL};
  for (uint16_t i = 0; i < num_buffers; ++i) {
    tx_ant_bufs[i] = (int16_t *)split7_aligned_alloc32(total_samples * 2 * sizeof(int16_t));
    if (!tx_ant_bufs[i]) {
      for (uint16_t k = 0; k < i; k++)
        free(tx_ant_bufs[k]);
      return UE_SPLIT7_ERR_NO_MEMORY;
    }
    tx_ptrs[i] = tx_ant_bufs[i];

    // Mandatory 38.211 §5.3 phase rotation, as nr_tx_rotation_and_ofdm_mod() does.
    split7_apply_tx_rotation(priv, (c16_t *)buffers[i].re_buffer, buffers[i].meta.slot_number, symbol, priv->fp->N_RB_UL);

    idft(idft_size, (int16_t *)buffers[i].re_buffer, priv->tx_time_buf, 1);

    // CP: last cp_len samples of IFFT output.
    memcpy(tx_ant_bufs[i], &priv->tx_time_buf[(fft_size - cp_len) * 2], cp_len * 2 * sizeof(int16_t));
    memcpy(&tx_ant_bufs[i][cp_len * 2], priv->tx_time_buf, fft_size * 2 * sizeof(int16_t));
  }

  // timestamp_samples is the raw DL reference timestamp (rx_ts); TA and RX-to-TX
  // lead time are both added here by the Low-PHY, not upstream.
  openair0_timestamp_t tx_ts = buffers[0].meta.timestamp_samples
                               + ue_split7_slots_duration_samples(dev, buffers[0].meta.slot_number, NR_UE_CAPABILITY_SLOT_RX_TO_TX)
                               - atomic_load_explicit(&priv->ta_samples, memory_order_relaxed);

  pthread_mutex_lock(&priv->write_mutex);
  int rc = priv->openair0_dev.trx_write_func(&priv->openair0_dev, tx_ts, tx_ptrs, total_samples, num_buffers, 1);
  pthread_mutex_unlock(&priv->write_mutex);

  for (uint16_t i = 0; i < num_buffers; ++i)
    free(tx_ant_bufs[i]);

  if (rc < 0)
    return UE_SPLIT7_ERR_GENERIC;

  return UE_SPLIT7_SUCCESS;
}

static ue_split7_status_t ue_split7_set_timing_advance(struct ue_split7_device *dev, uint32_t ta_samples)
{
  if (!dev)
    return UE_SPLIT7_ERR_INVALID_PARAM;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;
  atomic_store_explicit(&priv->ta_samples, ta_samples, memory_order_relaxed);

  // First call (right after DL sync) establishes the UL offset, making write_prach()/
  // write_symbol() valid; later TA updates just update ta_samples without touching state.
  int expected = UE_SPLIT7_STATE_DL_SYNCED;
  atomic_compare_exchange_strong_explicit(&priv->state,
                                          &expected,
                                          UE_SPLIT7_STATE_UL_PRACH_ONLY,
                                          memory_order_release,
                                          memory_order_relaxed);

  return UE_SPLIT7_SUCCESS;
}

static ue_split7_status_t ue_split7_adjust_rx_timing(struct ue_split7_device *dev, int32_t sample_shift_samples)
{
  if (!dev)
    return UE_SPLIT7_ERR_INVALID_PARAM;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;

  if (atomic_load_explicit(&priv->state, memory_order_acquire) < UE_SPLIT7_STATE_DL_SYNCED)
    return UE_SPLIT7_ERR_STATE;

  // Nudging next_read_ts re-anchors both RX and TX together (write_symbol()'s TX
  // timestamp derives from it too) -- same net effect as the monolithic UE's paired
  // readBlockSize/writeBlockSize shift at a frame boundary.
  priv->next_read_ts = (uint64_t)((int64_t)priv->next_read_ts - sample_shift_samples);

  return UE_SPLIT7_SUCCESS;
}

static ue_split7_status_t ue_split7_seed_slot_tracking(struct ue_split7_device *dev, uint32_t frame_number)
{
  if (!dev)
    return UE_SPLIT7_ERR_INVALID_PARAM;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;

  if (atomic_load_explicit(&priv->state, memory_order_acquire) < UE_SPLIT7_STATE_DL_SYNCED)
    return UE_SPLIT7_ERR_STATE;

  // next_read_ts already sits at slot 0 symbol 0 of frame_number (sync_task_func()'s
  // snap); anchor the slot tracker there.
  priv->next_frame_number = frame_number % 1024;
  priv->next_slot_number = 0;
  priv->next_slot_sample_pos = priv->next_read_ts;
  priv->slot_tracking_seeded = true;

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
  // slot is live now instead of returning stale data.
  uint64_t live_edge = atomic_load_explicit(&priv->latest_write_ts, memory_order_acquire);
  if (live_edge > priv->circ_buf_size && priv->next_slot_sample_pos < live_edge - priv->circ_buf_size) {
    uint64_t behind_samples = live_edge - priv->next_slot_sample_pos;
    LOG_W(PHY,
          "[split7] FD layer too slow: next slot (frame %u slot %u) is %llu samples "
          "behind the live edge (ring size %u) -- skipping ahead to live\n",
          priv->next_frame_number,
          priv->next_slot_number,
          (unsigned long long)behind_samples,
          priv->circ_buf_size);
    // One slot's margin inside the ring, since more samples for the current slot may still be in flight.
    uint64_t target_pos = live_edge - priv->circ_buf_size + get_samples_per_slot(0, fp);
    while (priv->next_slot_sample_pos < target_pos) {
      priv->next_slot_sample_pos += get_samples_per_slot((int)priv->next_slot_number, fp);
      priv->next_slot_number++;
      if (priv->next_slot_number >= fp->slots_per_frame) {
        priv->next_slot_number = 0;
        priv->next_frame_number = (priv->next_frame_number + 1) % 1024;
      }
    }
  }

  const uint64_t slot_samples = get_samples_per_slot((int)priv->next_slot_number, fp);
  const uint64_t target = priv->next_slot_sample_pos + slot_samples;

  pthread_mutex_lock(&priv->data_mutex);
  while (atomic_load_explicit(&priv->read_running, memory_order_acquire)
         && atomic_load_explicit(&priv->latest_write_ts, memory_order_acquire) < target) {
    pthread_cond_wait(&priv->data_cond, &priv->data_mutex);
  }
  pthread_mutex_unlock(&priv->data_mutex);
  if (!atomic_load_explicit(&priv->read_running, memory_order_acquire))
    return UE_SPLIT7_ERR_GENERIC;

  *frame_number = priv->next_frame_number;
  *slot_number = priv->next_slot_number;

  priv->next_slot_sample_pos = target;
  priv->next_slot_number++;
  if (priv->next_slot_number >= fp->slots_per_frame) {
    priv->next_slot_number = 0;
    priv->next_frame_number = (priv->next_frame_number + 1) % 1024;
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
  bool alloc_failed = false;
  for (int ch = 0; ch < num_rx; ch++) {
    rx_bufs[ch] = (int16_t *)malloc(alloc_samples * 2 * sizeof(int16_t));
    if (!rx_bufs[ch]) {
      alloc_failed = true;
      break;
    }
    rx_ptrs_c16[ch] = (c16_t *)rx_bufs[ch];
  }

  c16_t *rxdataF_buf = alloc_failed ? NULL : (c16_t *)split7_aligned_alloc32(4 * num_rx * fft_size * sizeof(c16_t));
  c16_t *pssTime_buf = alloc_failed ? NULL : (c16_t *)split7_aligned_alloc32(3 * fft_size * sizeof(c16_t));
  if (alloc_failed || !rxdataF_buf || !pssTime_buf) {
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

    priv->next_read_ts = (uint64_t)frame_boundary;

    LOG_I(PHY,
          "[sync] symbol_offset=%d frame_boundary=%lld frames_elapsed=%lld\n",
          symbol_offset,
          (long long)frame_boundary,
          (long long)frames_elapsed);

    if (!priv->sync_active)
      break;

    // openair0 convention: positive freq_offset means the carrier was above nominal.
    if (freq_offset != 0 && priv->openair0_dev.trx_set_freq_func) {
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

    atomic_store_explicit(&priv->state, UE_SPLIT7_STATE_DL_SYNCED, memory_order_release);

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

static ue_split7_status_t ue_split7_write_prach(struct ue_split7_device *dev, const ue_split7_prach_tx_params_t *params)
{
  if (!dev || !params)
    return UE_SPLIT7_ERR_INVALID_PARAM;
  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)dev->priv;

  if (atomic_load_explicit(&priv->state, memory_order_acquire) != UE_SPLIT7_STATE_UL_PRACH_ONLY)
    return UE_SPLIT7_ERR_STATE;

  uint32_t fft_size = params->fft_size > 0 ? params->fft_size : dev->config.fft_size;
  idft_size_idx_t idft_size = get_idft(fft_size);

  int16_t *temp_fft_in = (int16_t *)split7_aligned_alloc32(fft_size * 2 * sizeof(int16_t));
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

  int16_t *temp_ifft_out = (int16_t *)split7_aligned_alloc32(fft_size * 2 * sizeof(int16_t));
  if (!temp_ifft_out) {
    free(temp_fft_in);
    return UE_SPLIT7_ERR_NO_MEMORY;
  }

  idft(idft_size, temp_fft_in, temp_ifft_out, 1);

  uint32_t cp_len = params->cp_len_samples;
  uint32_t repetition = params->repetition_count > 0 ? params->repetition_count : 1;
  uint32_t total_samples = cp_len + fft_size * repetition;

  int16_t *prach_time_buf = (int16_t *)malloc(total_samples * 2 * sizeof(int16_t));
  if (!prach_time_buf) {
    free(temp_fft_in);
    free(temp_ifft_out);
    return UE_SPLIT7_ERR_NO_MEMORY;
  }

  uint32_t cp_copy_len = (cp_len < fft_size) ? cp_len : fft_size;
  memcpy(prach_time_buf, &temp_ifft_out[(fft_size - cp_copy_len) * 2], cp_copy_len * 2 * sizeof(int16_t));
  if (cp_len > fft_size) {
    memset(&prach_time_buf[cp_copy_len * 2], 0, (cp_len - cp_copy_len) * 2 * sizeof(int16_t));
  }

  for (uint32_t r = 0; r < repetition; ++r) {
    memcpy(&prach_time_buf[(cp_len + r * fft_size) * 2], temp_ifft_out, fft_size * 2 * sizeof(int16_t));
  }

  // params->slot_number uses the wrapped SFN, not an absolute frame count, so it can't
  // be turned into a timestamp directly -- timestamp_samples is the real anchor.
  uint64_t base_ts = params->timestamp_samples;
  // Cumulative sample count of the preceding *normal-numerology* symbols in this slot
  // (NOT the PRACH fft_size above, which can differ from fp->ofdm_symbol_size).
  uint64_t symbol_offset =
      params->symbol_number > 0 ? get_samples_symbol_duration(priv->fp, params->slot_number, 0, params->symbol_number) : 0;

  // base_ts is the raw DL reference timestamp; TA and RX-to-TX lead time are added
  // here by the Low-PHY, matching write_symbol()'s convention.
  openair0_timestamp_t tx_ts = base_ts + symbol_offset + params->time_offset_samples
                               + ue_split7_slots_duration_samples(dev, params->slot_number, NR_UE_CAPABILITY_SLOT_RX_TO_TX)
                               - atomic_load_explicit(&priv->ta_samples, memory_order_relaxed);

  void *tx_ptrs[8];
  tx_ptrs[0] = prach_time_buf;

  pthread_mutex_lock(&priv->write_mutex);
  int rc = priv->openair0_dev.trx_write_func(&priv->openair0_dev, tx_ts, tx_ptrs, total_samples, 1, 1);
  pthread_mutex_unlock(&priv->write_mutex);

  free(temp_fft_in);
  free(temp_ifft_out);
  free(prach_time_buf);

  if (rc < 0)
    return UE_SPLIT7_ERR_GENERIC;
  return UE_SPLIT7_SUCCESS;
}

ue_split7_device_t *ue_split7_device_create(void)
{
  ue_split7_device_t *dev = (ue_split7_device_t *)malloc(sizeof(ue_split7_device_t));
  if (!dev)
    return NULL;
  memset(dev, 0, sizeof(ue_split7_device_t));

  ue_split7_device_priv_t *priv = (ue_split7_device_priv_t *)malloc(sizeof(ue_split7_device_priv_t));
  if (!priv) {
    free(dev);
    return NULL;
  }
  memset(priv, 0, sizeof(ue_split7_device_priv_t)); // state == UE_SPLIT7_STATE_UNSYNCED (0)
  pthread_mutex_init(&priv->write_mutex, NULL);
  pthread_mutex_init(&priv->data_mutex, NULL);
  pthread_cond_init(&priv->data_cond, NULL);

  dev->priv = priv;
  dev->configure = ue_split7_configure;
  dev->start = ue_split7_start;
  dev->stop = ue_split7_stop;
  dev->read_symbol = ue_split7_read_symbol;
  dev->write_symbol = ue_split7_write_symbol;
  dev->start_sync = ue_split7_start_sync;
  dev->stop_sync = ue_split7_stop_sync;
  dev->set_timing_advance = ue_split7_set_timing_advance;
  dev->adjust_rx_timing = ue_split7_adjust_rx_timing;
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
    if (priv->rx_circ_buf) {
      for (int i = 0; i < dev->config.num_rx_antennas; i++) {
        if (priv->rx_circ_buf[i])
          free(priv->rx_circ_buf[i]);
      }
      free(priv->rx_circ_buf);
    }
    if (priv->rx_fd_buf) {
      for (int i = 0; i < dev->config.num_rx_antennas; i++) {
        if (priv->rx_fd_buf[i])
          free(priv->rx_fd_buf[i]);
      }
      free(priv->rx_fd_buf);
    }
    pthread_mutex_destroy(&priv->write_mutex);
    pthread_mutex_destroy(&priv->data_mutex);
    pthread_cond_destroy(&priv->data_cond);
    free(priv);
  }
  free(dev);
}
