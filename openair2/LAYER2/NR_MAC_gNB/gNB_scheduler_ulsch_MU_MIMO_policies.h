/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef GNB_SCHEDULER_ULSCH_MU_MIMO_POLICIES_H
#define GNB_SCHEDULER_ULSCH_MU_MIMO_POLICIES_H

#include "LAYER2/NR_MAC_gNB/nr_mac_gNB.h"

bool nr_srs_orthogonality_check(const nr_srs_eff_channel_info_t *a,
                                const nr_srs_eff_channel_info_t *b,
                                frame_t sched_frame,
                                slot_t sched_slot,
                                int slots_per_frame,
                                int max_age_slots);

int nr_ul_pf_mu_mimo(const nr_ul_sched_params_t *params, nr_ul_candidate_t *candidates, int n_candidates);

#endif // GNB_SCHEDULER_ULSCH_MU_MIMO_POLICIES_H
