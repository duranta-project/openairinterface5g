/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef DU_FHI_CORE_H
#define DU_FHI_CORE_H

#include "du_fh.h"
#include "openair1/PHY/defs_nr_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Scheduling look-ahead progress + RX/TX bookkeeping for the native compat shim. Exposed (not
// opaque) so unit tests can construct/inspect it directly without a real RU_t or config_get().
// PRACH-related fields/scheduling live in du_fhi_prach.h, kept out of this file so tests that
// only need the TDD/catch-up/look-ahead logic below don't drag in the L2 library it requires.
typedef struct {
  void *du_fh_handle;
  du_fh_config_t cfg;

  int ul_lookahead_slots; // how many slots ahead of the current OTA slot to schedule UL grants

  bool have_scheduled;
  uint64_t last_scheduled_absolute_slot; // hyper_frame*1024*slots_per_frame + frame*slots_per_frame + slot
} du_fhi_state_t;

uint64_t du_fhi_absolute_slot_number(uint64_t hyper_frame, int frame, int slot, int slots_per_frame);


// True if the given slot/symbol carries a DL (resp. UL) allocation under the configured
// TDD pattern; in FDD mode every symbol is simultaneously DL- and UL-schedulable.
bool du_fhi_is_dl_symbol(const du_fh_config_t *cfg, int slot, int symbol);
bool du_fhi_is_ul_symbol(const du_fh_config_t *cfg, int slot, int symbol);

// Number of UL symbols carried by the given slot under the configured TDD pattern (0 for a
// pure-DL slot). ru_thread expects one south_in() call to deliver a *whole slot's* worth of UL
// data (matching the vendor wrapper's per-slot notifiedFIFO granularity); du_fh's own
// read/ready API is per-symbol, so the shim needs this count to know how many consecutive
// reads make up one slot.
int du_fhi_count_ul_symbols_in_slot(const du_fh_config_t *cfg, int slot);

// Current hyper-frame number for the given (caller-observed) frame, queried fresh from du_fh's
// own fh_timer-anchored clock (du_fh_get_utc_anchor_point()) rather than tracked/estimated
// locally -- ru_thread's own frame/slot pacing is not guaranteed to stay perfectly isochronous
// with fh_timer's real-time clock over a long run, so a locally-tracked estimate (even one
// seeded correctly at bootstrap) drifts, whereas querying fresh every time never does.
uint64_t du_fhi_query_hyper_frame(void *du_fh_handle, int frame);

// The +1/0/-1 correction applied to the queried anchor's hyper_frame so it matches whichever
// side of a 1023->0 wrap the caller's own frame is actually on. Exposed for testing.
int du_fhi_hyper_frame_wrap_adjustment(uint32_t anchor_frame, int caller_frame);



// Ensures every slot up to ul_lookahead_slots ahead of (frame, slot) has had its UL grant
// C-Plane scheduling issued exactly once (no gaps, no duplicates), mirroring what libxran's
// worker threads generate internally from the same static TDD configuration. Returns the
// absolute slot number scheduling was advanced to (== st->last_scheduled_absolute_slot).
uint64_t du_fhi_advance_ul_schedule_lookahead(du_fhi_state_t *st, int frame, int slot, int nb_rx);


#ifdef __cplusplus
}
#endif

#endif /* DU_FHI_CORE_H */
