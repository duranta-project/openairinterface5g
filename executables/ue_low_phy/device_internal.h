/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Device internals shared with the anchor services (sync.c) and the tests. */

#ifndef UE_LOW_PHY_DEVICE_INTERNAL_H
#define UE_LOW_PHY_DEVICE_INTERNAL_H

#include <pthread.h>
#include "device.h"

/// Finds an anchor and calls ue_low_phy_device_set_anchor(); returns 0, or < 0 on failure.
/// Gives up once ue_low_phy_device_service_cancelled().
typedef int (*ue_low_phy_anchor_service_t)(ue_low_phy_device_t *dev, void *ctx, int expected_pci, uint32_t timeout_ms);

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
} ue_low_phy_member_t;

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
} ue_low_phy_group_t;

#define UE_LOW_PHY_MAX_BURSTS 64

typedef struct {
  int64_t start;
  int64_t end;
  bool opened; ///< its first samples were already sent (TX_BURST_START given)
} ue_low_phy_burst_t;

struct ue_low_phy_device {
  NR_DL_FRAME_PARMS fp;
  enum nr_Link rx_link;
  enum nr_Link tx_link;
  ue_low_phy_port_t *port;
  ue_low_phy_port_mem_t *mem;
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
  ue_low_phy_device_stats_t stats;
  pthread_t rx_thread;
  pthread_t tx_thread;

  // RX: time-domain ring indexed ts % rx_size, written by the RX thread only.
  c16_t *rx_td[UE_LOW_PHY_MAX_ANT];
  uint64_t rx_size;
  int rx_chunk; ///< samples per radio read
  ue_low_phy_progress_t rx_time; ///< end of the last read: the RX ring holds samples up to here
  c16_t *rx_sym[UE_LOW_PHY_MAX_ANT]; ///< one symbol with its CP, per antenna

  // Timeline: written by set_anchor() and by the RX thread (timing steering).
  pthread_mutex_t tb_mutex;
  ue_low_phy_timebase_t tb;
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
  c16_t *tx_td[UE_LOW_PHY_MAX_ANT];
  uint64_t tx_size;
  int64_t tx_base;
  int64_t tx_next;
  ue_low_phy_progress_t tx_sent; ///< tx_next as seen by other threads: radio time up to which TX is final
  ue_low_phy_device_stats_t tx_stats; ///< counted by the TX thread, added to stats once per pass
  // Content in the playout ring not transmitted yet, sorted, touching intervals
  // merged: only these samples go to the radio, each run as one burst.
  ue_low_phy_burst_t bursts[UE_LOW_PHY_MAX_BURSTS];
  int num_bursts;
  bool open_burst; ///< the last write ended a burst's samples at tx_next without ending the burst
  bool streaming; ///< lockstep: the continuous TX stream has started
  ue_low_phy_member_t members[UE_LOW_PHY_MAX_CLIENTS];
  ue_low_phy_group_t group;
  c16_t *mod_buf;
  c16_t *fd_sum[UE_LOW_PHY_MAX_ANT]; ///< frequency-domain sum of one (symbol, TA) group, ofdm_symbol_size bins
  c16_t *prach_buf;
  c16_t *prach_scratch;

  // Anchor service (UE: cell search), run on its own thread per request.
  ue_low_phy_anchor_service_t service;
  void *service_ctx;
  pthread_t service_thread;
  bool service_thread_valid;
  uint32_t served_req;
  uint32_t service_req;
  int service_pci;
  uint32_t service_timeout_ms;
};

void ue_low_phy_device_set_anchor_service(ue_low_phy_device_t *dev, ue_low_phy_anchor_service_t service, void *ctx);
bool ue_low_phy_device_service_cancelled(ue_low_phy_device_t *dev);

#endif
