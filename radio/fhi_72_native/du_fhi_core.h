/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef DU_FHI_CORE_H
#define DU_FHI_CORE_H

#include "du_fh.h"
#include "openair1/PHY/defs_nr_common.h"
#include "radio/COMMON/common_lib.h" // symbol_direction_t

#ifdef __cplusplus
extern "C" {
#endif

// Scheduling look-ahead and RX/TX bookkeeping of the driver; not opaque so tests can build it.
typedef struct {
  void *du_fh_handle;
  du_fh_config_t cfg;

  int ul_lookahead_slots; // how many slots after the one whose UL window just closed to schedule UL grants

  bool have_scheduled;
  uint64_t last_scheduled_absolute_slot; // hyper_frame*1024*slots_per_frame + frame*slots_per_frame + slot
} du_fhi_state_t;

uint64_t du_fhi_absolute_slot_number(uint64_t hyper_frame, int frame, int slot, int slots_per_frame);


// True if the given slot/symbol carries a DL (resp. UL) allocation under the configured
// TDD pattern; in FDD mode every symbol is simultaneously DL- and UL-schedulable.
bool du_fhi_is_dl_symbol(const du_fh_config_t *cfg, int slot, int symbol);
bool du_fhi_is_ul_symbol(const du_fh_config_t *cfg, int slot, int symbol);

// Number of UL symbols in the given slot under the configured TDD pattern (0 for a DL slot)
int du_fhi_count_ul_symbols_in_slot(const du_fh_config_t *cfg, int slot);

// Reduces a TDD period given per symbol to du_fh_tdd_pattern_t: leading full DL slots, DL symbols
// starting the next slot, UL symbols ending the slot before the trailing full UL slots. Returns
// false if the result does not reproduce the direction of every symbol.
bool du_fhi_derive_tdd_pattern(int period_slots, const symbol_direction_t dirs[][NR_SYMBOLS_PER_SLOT], du_fh_tdd_pattern_t *tdd);

// Hyper-frame number of the given frame, read from du_fh's clock on every call so it cannot drift
uint64_t du_fhi_query_hyper_frame(void *du_fh_handle, int frame);

// The +1/0/-1 correction applied to the queried anchor's hyper_frame so it matches whichever
// side of a 1023->0 wrap the caller's own frame is actually on. Exposed for testing.
int du_fhi_hyper_frame_wrap_adjustment(uint32_t anchor_frame, int caller_frame);



// Called every slot with the slot whose UL window has just closed. Schedules the UL grant C-Plane
// of every slot up to ul_lookahead_slots ahead exactly once, none for slots already over. Returns
// the last scheduled slot; *first_slot is the first one scheduled by this call.
uint64_t du_fhi_advance_ul_schedule_lookahead(du_fhi_state_t *st, uint64_t absolute_slot, int nb_rx, uint64_t *first_slot);


#ifdef __cplusplus
}
#endif

#endif /* DU_FHI_CORE_H */
