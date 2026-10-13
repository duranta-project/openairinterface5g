/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * The seam between the gNB MAC and the Spectrum RAN function: the only symbols
 * the scheduler is allowed to call into. Same role as the *_extern.h headers of
 * the E2 RAN functions (e.g. ran_func_rc_extern.h). Everything else about
 * sensing -- configuration, state, the scan itself -- is private to
 * ran_func_spectrum.c.
 */
#ifndef RAN_FUNC_SPECTRUM_EXTERN_H
#define RAN_FUNC_SPECTRUM_EXTERN_H

#include "LAYER2/NR_MAC_gNB/nr_mac_gNB.h"
#include "LAYER2/NR_MAC_gNB/nr_radio_config.h"
#include "openair2/E3AP/ran_func_spectrum_types.h"

/* Extra UL TDA shapes the operator asked for in E3Configuration
 * (additional_ul_tdas). Called while the cell's UL TDA list is built, so they can
 * be appended to it; returns how many, 0 if none. *out stays valid for the life
 * of the process. */
int e3_spectrum_extra_ul_tdas(const nr_ul_tda_shape_t **out);

/* Bind the Spectrum RAN function to a cell once its frame structure and UL TDA
 * list are known (end of nr_mac_config_scc()). Reads the E3Configuration keys it
 * owns and enables sensing for the cell iff slots are reserved or extra TDAs are
 * configured; when it is, installs the sensing-aware UL TDA selector on the MAC
 * instance. A no-op when nothing is configured. */
void e3_spectrum_mac_attach_cell(gNB_MAC_INST *mac, nr_cell_sched_t *cell);

/* Release what e3_spectrum_mac_attach_cell() allocated. Idempotent. */
void e3_spectrum_mac_detach_cell(nr_cell_sched_t *cell);

/* Is this (absolute) slot hard-reserved for sensing for this cell? */
bool nr_mac_ul_slot_is_sensing_reserved(const nr_cell_sched_t *cell, int slot);

/* Log rate limiter for the scheduler's graceful-skip paths: ++*counter, true for
 * the first five and then every 1000th. */
static inline bool prb_block_ratelimit(uint64_t *counter)
{
  const uint64_t count = ++(*counter);
  return count <= 5 || (count % 1000) == 0;
}

/* OR the dApp's PRB block into the current slot's VRB maps. Called once per slot,
 * after the maps are seeded and before any scheduling step reads them. */
void apply_prb_block_masks(nr_cell_sched_t *cell, frame_t frame, slot_t slot);

/* Reserve `mask` over [rb_start, rb_start+nb_rb) of a UL VRB row for a cell
 * channel (PRACH/MsgA-PUSCH/Msg3), skipping collisions with the dApp block
 * (logged once) and asserting only on a genuine conflict. */
void prb_block_reserve_ul_channel(const nr_cell_sched_t *cell,
                                  uint16_t *vrb_map_UL,
                                  int rb_start,
                                  int nb_rb,
                                  uint16_t mask,
                                  frame_t frame,
                                  slot_t slot,
                                  rnti_t rnti,
                                  const char *channel_label);

/* A UE's dedicated configuration was (re)applied / the UE is being deleted:
 * keep the record of where its periodic PUCCH/SRS/CSI-RS live up to date, and
 * report any that the current block lands on. */
void e3_spectrum_on_ue_configured(const nr_cell_sched_t *cell, NR_UE_info_t *UE);
void e3_spectrum_on_ue_deleted(const NR_UE_info_t *UE);

/* Per-slot hooks, called from gNB_dlsch_ulsch_scheduler(). reserve/restore
 * bracket the UE allocators (block the slot, then free it for the scan);
 * scan_and_publish runs the scan, the Aerial capture PUSCH and the E3 publish.
 * All three are no-ops for a cell sensing is not attached to. */
void nr_mac_sensing_reserve_ul_slot(nr_cell_sched_t *cell, int prev_slot, frame_t frame);
void nr_mac_sensing_restore_ul_slot(nr_cell_sched_t *cell, frame_t frame, slot_t slot);
void nr_mac_sensing_scan_and_publish(gNB_MAC_INST *mac, nr_cell_sched_t *cell, frame_t frame, slot_t slot);

#endif /* RAN_FUNC_SPECTRUM_EXTERN_H */
