/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef E3_PENDING_CTRL_H
#define E3_PENDING_CTRL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* An xApp control in flight, keyed by sequence id. Opened by the flexric thread, installed by the
 * E3 control handler, put on air by the MAC thread, which has a deadline and so takes no lock. */

#define E3_PENDING_CTRL_MAX 64

typedef struct {
  uint32_t sequence_id;
  int64_t installed_ts_ns; /* realtime; mask written into the MAC */
  int64_t on_air_ts_ns; /* realtime; first tick that put it on air */
  uint16_t sfn;
  uint16_t slot;
  bool ok;
} e3_ctrl_outcome_t;

void e3_pending_ctrl_init(void);

/* False when the table is full. */
bool e3_pending_ctrl_open(uint32_t sequence_id);

/* E3 control-handler thread. */
void e3_pending_ctrl_installed(uint32_t sequence_id, int64_t installed_ts_ns, bool ok);

/* MAC thread, scheduler critical path: no lock, no allocation. */
void e3_pending_ctrl_on_air(uint32_t sequence_id, int64_t on_air_ts_ns, uint16_t sfn, uint16_t slot);

/* MAC thread, no lock: the mask was replaced before any tick put it on air.
 * The outcome keeps its install timestamp; onAirTimestamp is absent on the wire. */
void e3_pending_ctrl_superseded(uint32_t sequence_id);

/* Relay thread. */
size_t e3_pending_ctrl_drain(e3_ctrl_outcome_t* out, size_t max);

/* Retires entries past the timeout; they come back in out[] with ok=false. */
size_t e3_pending_ctrl_sweep(e3_ctrl_outcome_t* out, size_t max);

#endif
