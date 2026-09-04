/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef GNB_SCHEDULER_ULSCH_DEFAULT_POLICIES_H
#define GNB_SCHEDULER_ULSCH_DEFAULT_POLICIES_H

#include "LAYER2/NR_MAC_gNB/nr_mac_gNB.h"

void nr_ul_ri_tpmi_select_default(gNB_MAC_INST *mac, nr_ul_candidate_t *cands, int n_cand);

int nr_ul_tda_select_default(gNB_MAC_INST *mac,
                             nr_cell_sched_t *cell,
                             nr_ul_candidate_t *cands,
                             int n_cand,
                             frame_t sched_frame,
                             slot_t sched_slot,
                             int k2);

int nr_ul_beam_select_default(NR_beam_info_t *beam_info,
                              const int16_t *beam_index_list,
                              nr_ul_candidate_t *candidates,
                              int n_candidates,
                              frame_t frame,
                              slot_t slot,
                              frame_t sched_frame,
                              slot_t sched_slot,
                              int slots_per_frame);

void nr_ul_mcs_select_default(const nr_cell_sched_t *cell, nr_ul_candidate_t *candidates, int n_candidates);

int nr_ul_proportional_fair(const nr_ul_sched_params_t *params, nr_ul_candidate_t *candidates, int n_candidates);

#endif /* GNB_SCHEDULER_ULSCH_DEFAULT_POLICIES_H */
