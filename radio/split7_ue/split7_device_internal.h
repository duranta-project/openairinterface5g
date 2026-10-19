/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Device internals shared with the anchor services (split7_sync.c) and the tests. */

#ifndef SPLIT7_DEVICE_INTERNAL_H
#define SPLIT7_DEVICE_INTERNAL_H

#include <pthread.h>
#include "split7_device.h"

/// Finds an anchor and calls split7_device_set_anchor(); returns 0, or < 0 on failure.
/// Gives up once split7_device_service_cancelled().
typedef int (*split7_anchor_service_t)(split7_device_t *dev, void *ctx, int expected_pci, uint32_t timeout_ms);

// TX-thread view of one member. Per-slot arrays are indexed slot % ring_slots and
// copied from the slot records as the member's completed slots advance.
typedef struct {
  bool member;
  uint64_t first_slot;
  uint64_t leave_slot; ///< UINT64_MAX unless leaving
  uint64_t done; ///< last completed slot
  uint32_t *used;
  int32_t *prach;
  int32_t *ta; ///< own TA per slot; a slot completed without a record (skipped) keeps the previous one
  int32_t join_ta; ///< TA before its first slot
} split7_member_t;

// The group: one timeline, each member at its own Timing Advance, assembled
// slot by slot once every participating member has completed the slot.
typedef struct {
  bool active;
  uint64_t done; ///< last slot completed by every participating member
  uint64_t cursor; ///< next symbol to assemble
  uint64_t prach_slot; ///< next slot whose PRACH windows are not handled yet
  int64_t reach; ///< radio time up to which the completed slots are final
  int32_t *ta_max; ///< per slot: the largest member TA, whose slot ends first: TX is final up to there
  int64_t *clip; ///< samples of the slot before this are dropped
  int16_t *holder; ///< per slot: member the group last waited for before the slot completed, -1 for none
  int waiting_for; ///< member the group waits for now, -1 for none
} split7_group_t;

#define SPLIT7_MAX_BURSTS 64

typedef struct {
  int64_t start;
  int64_t end;
  bool opened; ///< its first samples were already sent (TX_BURST_START given)
} split7_burst_t;

struct split7_device {
  NR_DL_FRAME_PARMS fp;
  enum nr_Link rx_link;
  enum nr_Link tx_link;
  split7_port_t *port;
  split7_port_mem_t *mem;
  openair0_config_t radio_cfg;
  openair0_device_t own_radio;
  openair0_device_t *radio;
  bool lockstep;
  bool started;
  // Guarded by state_mutex: run flags and statistics.
  pthread_mutex_t state_mutex;
  bool running;
  bool service_busy;
  bool service_cancel;
  split7_device_stats_t stats;
  pthread_t rx_thread;
  pthread_t tx_thread;

  // RX: time-domain ring indexed ts % rx_size, written by the RX thread only.
  c16_t *rx_td[SPLIT7_MAX_ANT];
  uint64_t rx_size;
  int rx_chunk; ///< samples per radio read
  split7_progress_t rx_time; ///< end of the last read: the RX ring holds samples up to here
  c16_t *rx_sym;

  // Timeline: written by set_anchor() and by the RX thread (timing steering).
  pthread_mutex_t tb_mutex;
  split7_timebase_t tb;
  uint64_t next_demod;
  uint64_t max_done; ///< first slot no member has completed (timing shifts start after it)

  // Radio clock estimate: last read and its wall time, rate measured over seconds.
  pthread_mutex_t clk_mutex;
  int64_t clk_rx;
  int64_t clk_ns;
  double clk_rate; ///< samples per ns
  int64_t clk_ref_rx;
  int64_t clk_ref_ns;

  // TX: playout ring indexed (ts - tx_base) % tx_size, TX thread only.
  c16_t *tx_td[SPLIT7_MAX_ANT];
  uint64_t tx_size;
  int64_t tx_base;
  int64_t tx_next;
  split7_progress_t tx_sent; ///< tx_next as seen by other threads: radio time up to which TX is final
  split7_device_stats_t tx_stats; ///< counted by the TX thread, added to stats once per pass
  // Content in the playout ring not transmitted yet, sorted, touching intervals
  // merged: only these samples go to the radio, each run as one burst.
  split7_burst_t bursts[SPLIT7_MAX_BURSTS];
  int num_bursts;
  bool open_burst; ///< the last write ended a burst's samples at tx_next without ending the burst
  bool streaming; ///< lockstep: the continuous TX stream has started
  split7_member_t members[SPLIT7_MAX_CLIENTS];
  split7_group_t group;
  c16_t *mod_buf;
  c16_t *mod_scratch;
  c16_t *fd_sum[SPLIT7_MAX_ANT]; ///< frequency-domain sum of one (symbol, TA) group
  c16_t *prach_buf;
  c16_t *prach_scratch;

  // Anchor service (UE: cell search), run on its own thread per request.
  split7_anchor_service_t service;
  void *service_ctx;
  pthread_t service_thread;
  bool service_thread_valid;
  uint32_t served_req;
  uint32_t service_req;
  int service_pci;
  uint32_t service_timeout_ms;
};

void split7_device_set_anchor_service(split7_device_t *dev, split7_anchor_service_t service, void *ctx);
bool split7_device_service_cancelled(split7_device_t *dev);

#endif
