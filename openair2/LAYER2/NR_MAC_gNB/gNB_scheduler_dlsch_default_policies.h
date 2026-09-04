/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef GNB_SCHEDULER_DLSCH_DEFAULT_POLICIES_H
#define GNB_SCHEDULER_DLSCH_DEFAULT_POLICIES_H

#include "LAYER2/NR_MAC_gNB/nr_mac_gNB.h"

void nr_dl_ri_pmi_select_default(const nr_cell_sched_t *cell, nr_dl_candidate_t *candidates, int n_candidates);
void nr_dl_mcs_select_default(const nr_cell_sched_t *cell, nr_dl_candidate_t *candidates, int n_candidates);

int nr_dl_beam_select_default(NR_beam_info_t *beam_info,
                              const int16_t *beam_index_list,
                              nr_dl_candidate_t *candidates,
                              int n_candidates,
                              frame_t frame,
                              slot_t slot,
                              int slots_per_frame);

int nr_dl_tda_select_default(const gNB_MAC_INST *mac, const nr_cell_sched_t *cell, nr_dl_candidate_t *candidates, int n_candidates, frame_t frame, slot_t slot);

int nr_dl_proportional_fair(const nr_dl_sched_params_t *params, nr_dl_candidate_t *candidates, int n_candidates);

void nr_dl_lcid_alloc_default(const gNB_MAC_INST *mac,
                              const nr_dl_candidate_t *candidate,
                              int tbs_available,
                              int lcid_alloc[NR_MAX_NUM_LCID]);

#endif /* GNB_SCHEDULER_DLSCH_DEFAULT_POLICIES_H */
