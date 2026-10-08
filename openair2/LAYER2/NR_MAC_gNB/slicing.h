/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_MAC_GNB_SLICING_H_
#define NR_MAC_GNB_SLICING_H_

#include <stdbool.h>
#include "common/5g_platform_types.h"

/// Maximum number of slices
#define NR_MAX_SLICES 512
/// Maximum number of policy members (PLMN + S-NSSAI) per slice
#define MAX_NUM_SLICE_MEMBERS 8

/// Resource type of an RRM policy, cf. TS 28.541 resourceType (PRB, PRB_DL, PRB_UL)
typedef enum nr_slice_resource_type_e {
  NR_SLICE_RESOURCE_BOTH,
  NR_SLICE_RESOURCE_DL,
  NR_SLICE_RESOURCE_UL,
} nr_slice_resource_type_t;

/// RRM policy member, cf. TS 28.541 RRMPolicyMember
typedef struct nr_slice_policy_member_s {
  plmn_id_t plmn;
  nssai_t nssai;
} nr_slice_policy_member_t;

/// RRM policy ratio, cf. TS 28.541 RRMPolicyRatio. Ratios in percent [0,100].
typedef struct nr_slice_policy_s {
  /// slice ID, assigned by add_slice() and unique per cell; 0 means "no slice"
  uint32_t id;
  nr_slice_resource_type_t resource_type;
  int num_members;
  nr_slice_policy_member_t members[MAX_NUM_SLICE_MEMBERS];
  float dedicated_ratio;
  float min_ratio;
  float max_ratio;
} nr_slice_policy_t;

struct nr_cell_sched_s;
struct NR_UEs_s;
struct nr_dl_sched_params;
struct nr_dl_candidate;
struct nr_ul_sched_params;
struct nr_ul_candidate;

bool add_slice(struct nr_cell_sched_s *cell, const nr_slice_policy_t *policy);
bool remove_slice(struct nr_cell_sched_s *cell, nr_slice_policy_t *slice);
const nr_slice_policy_t *get_slice(const struct nr_cell_sched_s *cell,
                                   const plmn_id_t *plmn,
                                   const nssai_t *nssai,
                                   nr_slice_resource_type_t dir);

const nr_slice_policy_t *get_slice_by_id(const struct nr_cell_sched_s *cell, uint32_t id);
void num_associated_ues_slice(struct NR_UEs_s *UEs, const nr_slice_policy_t *slice, int *num_dl, int *num_ul);

int nr_dl_slicing_fixed(const struct nr_dl_sched_params *params, struct nr_dl_candidate *candidates, int n_candidates);
int nr_ul_slicing_fixed(const struct nr_ul_sched_params *params, struct nr_ul_candidate *candidates, int n_candidates);

#endif /* NR_MAC_GNB_SLICING_H_ */
