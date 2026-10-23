/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Split 7.1 Low-PHY engine: owns the radio and serves one split7 port.
 *
 * RX: one thread reads the radio into a time-domain ring and, once anchored,
 * demodulates every symbol into the port's DL ring.
 * TX: one thread keeps a time-domain playout ring and writes a stretch of it to
 * the radio only once every member has completed the slots covering it (see
 * split7_port.h); the members form one co-located group with the leader's Timing
 * Advance, their symbols summed in frequency domain and modulated once. There is
 * no deadline: a late member makes the whole uplink late.
 * Anchor: either a cell search (split7_engine_enable_cell_search(), UE role) or
 * split7_engine_set_anchor() from the host (e.g. an O-RU's PTP/UTC time).
 */

#ifndef SPLIT7_ENGINE_H
#define SPLIT7_ENGINE_H

#include "common_lib.h"
#include "split7_port.h"
#include "split7_timebase.h"
#include "PHY/defs_nr_common.h"

typedef struct {
  const NR_DL_FRAME_PARMS *fp; ///< copied; rotation tables must be initialized
  enum nr_Link rx_link; ///< link_type_dl for a UE, link_type_ul for an O-RU
  enum nr_Link tx_link;
  openair0_config_t radio_cfg; ///< used to load the radio unless radio is set
  openair0_device_t *radio; ///< optional: an already loaded/fake radio (tests)
} split7_engine_cfg_t;

typedef struct split7_engine split7_engine_t;

split7_engine_t *split7_engine_create(const split7_engine_cfg_t *cfg);
int split7_engine_start(split7_engine_t *e);
/// Stops the threads and the radio, releases the port. Clients must be closed.
void split7_engine_destroy(split7_engine_t *e);
split7_port_t *split7_engine_port(split7_engine_t *e);
const NR_DL_FRAME_PARMS *split7_engine_fp(const split7_engine_t *e);

/// Anchors the timeline: anchor_ts is the sample timestamp of the start of abs_frame.
/// Demodulation starts with the slot being received; cell (may be NULL) is published to clients.
void split7_engine_set_anchor(split7_engine_t *e, int64_t anchor_ts, uint64_t abs_frame, const split7_cell_t *cell);

/// Latest radio timestamp received (end of the last read).
int64_t split7_engine_rx_time(split7_engine_t *e);
/// Copies num_samples of antenna ant starting at ts out of the RX ring; false if no longer there.
bool split7_engine_rx_copy(split7_engine_t *e, int ant, int64_t ts, int num_samples, c16_t *dst);

typedef struct {
  uint64_t tx_samples;
  uint64_t tx_idft; ///< UL IDFTs run (one per symbol and antenna)
  uint64_t tx_late_writes; ///< stretches whose start was behind the radio when written
  uint64_t tx_write_errors; ///< radio writes that failed or took fewer samples than given
  uint64_t burst_end_markers; ///< bursts ended by a separate write (the next slot completed after them)
  uint64_t burst_end_late; ///< ... of which after the radio passed the burst's end
  uint64_t rx_overruns; ///< symbols lost because demodulation fell behind the RX ring
} split7_engine_stats_t;

split7_engine_stats_t split7_engine_stats(split7_engine_t *e);

/// UE role: answer client sync requests with a PSS/SSS/PBCH search (split7_sync.c): over
/// the GSCNs of nr_band within the carrier if nr_band > 0, else at ssb_first_sc only.
void split7_engine_enable_cell_search(split7_engine_t *e, int nr_band, int ssb_first_sc);

#endif
