/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __NR_ORU_H__
#define __NR_ORU_H__
#include "openair1/PHY/defs_RU.h"
#include <pthread.h>
#include "oru_fh.h"
#include "common/utils/symbol_reorder/symbol_reorder.h"
#include "openair2/LAYER2/NR_MAC_COMMON/nr_prach_config.h"
#include "openair1/PHY/defs_gNB.h"
#include "oru_beamforming.h"

#define MAX_DL_READ_THREADS 8
#define MAX_ORU_UL_WORKERS 8

typedef struct {
  RU_t *ru;
  /// tx carrier
  uint64_t carrier_freq_tx[MAX_BANDS_PER_RRU];
  /// rx carrier
  uint64_t carrier_freq_rx[MAX_BANDS_PER_RRU];
  /// tx BW in PRBs
  int bw_tx[MAX_BANDS_PER_RRU];
  /// rx BW in PRBs
  int bw_rx[MAX_BANDS_PER_RRU];
  /// 3GPP FRAME Type FDD/TDD
  int frame_type;
  /// 3GPP PRACH configuration index
  int prach_config_index;
  /// 3GPP MSG1 Start frequency
  int prach_msg1_freq;
  /// 3GPP TDD periodicity (0.5 ms, 1 0.625ms, 2 1ms, 3 1.25ms, 4 2ms,5 2.5ms, 6 5ms, 7 10ms, 8 3ms, 9 4ms
  int tdd_period;
  /// number of DL slots
  int num_DL_slots;
  /// number of UL slots
  int num_UL_slots;
  /// number of DL symbols
  int num_DL_symbols;
  /// number of UL symbols
  int num_UL_symbols;
  int numerology;

  int north_cores[MAX_DL_READ_THREADS]; ///< affinity for DL reader i (-1 = unpinned); length sets num_dl_read_threads
  int south_core;
  int ul_worker_cores[MAX_ORU_UL_WORKERS];
  int num_ul_worker_cores;

  int num_dl_read_threads;
  pthread_t dl_read_threads[MAX_DL_READ_THREADS];
  pthread_t south_read_thread;
  pthread_t south_write_thread;

  // South (Split 8) write thread: CPU affinity, and the mutex/cond pair used by the DL reader
  // threads to publish the shared TX timing anchor to oru_south_write_thread() at startup.
  struct {
    int core;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int64_t start_timestamp;
    uint64_t start_hyper_frame;
    uint64_t start_symbol_index;
    bool initialized;
  } tx_write;

  symbol_reorder_t *dl_reorder;
  oru_fh_config_t fh_config;
  void *fronthaul;

  // PRACH related
  nr_prach_info_t prach_info;
  time_stats_t rx_prach;
  time_stats_t rx;
  prach_item_t prach_item;
  oru_codebook_t codebook;
  bool threequarter_fs;

  // Real-time Self-diagnosis metrics
  uint64_t dl_packed_stats; // upper 32 bits: count, lower 32 bits: total_time_us in SQ4
  uint64_t dl_symbol_time_max_us; // in SQ4

  uint64_t ul_packed_stats; // upper 32 bits: count, lower 32 bits: total_time_us in SQ4
  uint64_t ul_ant_time_max_us; // in SQ4
  _Atomic(uint64_t) ul_dropped_jobs;
  _Atomic(uint64_t) ul_symbols_missed;
} ORU_t;

int get_oru_options(ORU_t *oru);
void oru_init_frame_parms(ORU_t *oru);
void *oru_north_read_worker(void *arg);
void *oru_south_read_thread(void *arg);
void *oru_south_write_thread(void *arg);
void prepare_prach_item(ORU_t *oru);
void oru_self_diagnosis(ORU_t *oru);

#endif
