/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef E3_PENDING_CTRL_H
#define E3_PENDING_CTRL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* An xApp control being carried out, from the bridge forwarding it to the dApp
 * until the mask it asks for is on the air.
 *
 * Three threads touch one of these. The flexric thread that ran the E2 control
 * callback opens it; the E3 control handler records the install; the MAC thread
 * records the slot that put it on air. Only the last of those has a deadline,
 * so it does the least: one clock read and one store, no lock and no allocation
 * (see publish_on_air()).
 *
 * Keyed by the procedure's sequence id, which is what the acknowledge must name
 * for the xApp to know which of its decisions was applied. */

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

/* Open a slot for a control the bridge has just forwarded. False when the table
 * is full, which the caller answers immediately rather than deferring. */
bool e3_pending_ctrl_open(uint32_t sequence_id);

/* Record the install. Called on the E3 control-handler thread. */
void e3_pending_ctrl_installed(uint32_t sequence_id, int64_t installed_ts_ns, bool ok);

/* Record the slot that put the mask on air, and hand the finished outcome to
 * the relay. Called on the MAC thread, in the scheduler's critical path: it
 * takes no lock and does no work beyond filling a slot the relay drains. */
void e3_pending_ctrl_on_air(uint32_t sequence_id, int64_t on_air_ts_ns, uint16_t sfn, uint16_t slot);

/* Retire a control whose mask was replaced before any tick put it on the air.
 * Called on the MAC thread when a later install overwrites this one, so it does
 * the same minimal work as e3_pending_ctrl_on_air(): no lock, no allocation.
 *
 * The outcome keeps its install timestamp and carries no on-air one, which is
 * the truth about it -- it was applied to the MAC and replaced before the
 * scheduler ever ran with it. The wire says exactly that: onAirTimestamp is
 * OPTIONAL and absent. */
void e3_pending_ctrl_superseded(uint32_t sequence_id);

/* Drain up to max completed outcomes. Called by the relay thread. */
size_t e3_pending_ctrl_drain(e3_ctrl_outcome_t* out, size_t max);

/* Retire anything that has waited longer than the budget, so a dApp that never
 * answers cannot hold an xApp. The retired entries come back through out[] with
 * ok=false and no timestamps. */
size_t e3_pending_ctrl_sweep(e3_ctrl_outcome_t* out, size_t max);

#endif
