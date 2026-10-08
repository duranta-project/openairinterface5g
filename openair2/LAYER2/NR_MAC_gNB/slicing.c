/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdlib.h>

#include "slicing.h"
#include "nr_mac_gNB.h"
#include "gNB_scheduler_dlsch_default_policies.h"
#include "gNB_scheduler_ulsch_default_policies.h"

static const char *resource_type_str[] = {"both", "dl", "ul"};

static bool applies_to(nr_slice_resource_type_t rt, nr_slice_resource_type_t dir)
{
  return rt == NR_SLICE_RESOURCE_BOTH || rt == dir;
}

/** @brief Admission policy, cf. TS 28.541 RRMPolicyRatio: dedicatedRatio <= minRatio <= maxRatio, and
 * the sum of dedicatedRatio and of minRatio of all slices per direction must not exceed 100 */
static bool admit_slice(const seq_arr_t *slices, const nr_slice_policy_t *p)
{
  if (p->num_members == 0) {
    LOG_W(NR_MAC, "slice rejected: no policy member\n");
    return false;
  }
  if (p->dedicated_ratio > p->min_ratio || p->min_ratio > p->max_ratio) {
    LOG_W(NR_MAC,
          "slice rejected: need dedicatedRatio (%.2f) <= minRatio (%.2f) <= maxRatio (%.2f)\n",
          p->dedicated_ratio,
          p->min_ratio,
          p->max_ratio);
    return false;
  }
  const nr_slice_resource_type_t dirs[] = {NR_SLICE_RESOURCE_DL, NR_SLICE_RESOURCE_UL};
  for (int d = 0; d < sizeofArray(dirs); ++d) {
    if (!applies_to(p->resource_type, dirs[d]))
      continue;
    float sum_dedicated = p->dedicated_ratio;
    float sum_min = p->min_ratio;
    for (int i = 0; i < seq_arr_size(slices); ++i) {
      const nr_slice_policy_t *s = seq_arr_at(slices, i);
      if (!applies_to(s->resource_type, dirs[d]))
        continue;
      sum_dedicated += s->dedicated_ratio;
      sum_min += s->min_ratio;
    }
    if (sum_dedicated > 100.0f || sum_min > 100.0f) {
      LOG_W(NR_MAC,
            "slice rejected: sum of %s dedicatedRatio %.2f or minRatio %.2f exceeds 100\n",
            resource_type_str[dirs[d]],
            sum_dedicated,
            sum_min);
      return false;
    }
  }
  return true;
}

/** @brief Add a slice to the cell, if admitted
 * @return true if the slice has been admitted and added, false otherwise */
bool add_slice(nr_cell_sched_t *cell, const nr_slice_policy_t *policy)
{
  if (!admit_slice(&cell->slices, policy))
    return false;

  nr_slice_policy_t s = *policy;
  s.id = ++cell->last_slice_id;
  // slices are sorted by ID for get_slice_by_id()
  const size_t n = seq_arr_size(&cell->slices);
  DevAssert(n == 0 || ((nr_slice_policy_t *)seq_arr_at(&cell->slices, n - 1))->id < s.id);
  seq_arr_push_back(&cell->slices, &s, sizeof(s));

  LOG_I(NR_MAC,
        "added slice %d: resource type %s, dedicated %.2f%% min %.2f%% max %.2f%%\n",
        s.id,
        resource_type_str[s.resource_type],
        s.dedicated_ratio,
        s.min_ratio,
        s.max_ratio);
  for (int m = 0; m < s.num_members; ++m) {
    const nr_slice_policy_member_t *pm = &s.members[m];
    LOG_I(NR_MAC,
          "  member %d: PLMN %03d.%0*d SST %d SD 0x%06x\n",
          m,
          pm->plmn.mcc,
          pm->plmn.mnc_digit_length,
          pm->plmn.mnc,
          pm->nssai.sst,
          pm->nssai.sd);
  }
  return true;
}

/** @brief Remove a slice from the cell, if no connected UE is associated to it anymore
 * @param slice pointer to the slice in cell->slices
 * @return true if the slice has been removed, false otherwise */
bool remove_slice(nr_cell_sched_t *cell, nr_slice_policy_t *slice)
{
  DevAssert(seq_arr_dist(&cell->slices, seq_arr_front(&cell->slices), slice) < seq_arr_size(&cell->slices));
  LOG_I(NR_MAC, "removing slice %d\n", slice->id);
  seq_arr_erase(&cell->slices, slice);
  return true;
}

void num_associated_ues_slice(NR_UEs_t *UEs, const nr_slice_policy_t *slice, int *num_dl, int *num_ul)
{
  *num_dl = 0;
  *num_ul = 0;
  FOR_EACH_CONNECTED_UE(UEs, UE) {
    if (UE->dl_slice_id == slice->id)
      (*num_dl)++;
    if (UE->ul_slice_id == slice->id)
      (*num_ul)++;
  }
}

static bool eq_plmn(const plmn_id_t *a, const plmn_id_t *b)
{
  return a->mcc == b->mcc && a->mnc == b->mnc && a->mnc_digit_length == b->mnc_digit_length;
}

/** @brief Get the slice for PLMN and S-NSSAI in direction dir (DL or UL)
 * @return the first slice containing a matching policy member, or NULL if none */
const nr_slice_policy_t *get_slice(const nr_cell_sched_t *cell,
                                   const plmn_id_t *plmn,
                                   const nssai_t *nssai,
                                   nr_slice_resource_type_t dir)
{
  DevAssert(dir == NR_SLICE_RESOURCE_DL || dir == NR_SLICE_RESOURCE_UL);
  for (int i = 0; i < seq_arr_size(&cell->slices); ++i) {
    const nr_slice_policy_t *s = seq_arr_at(&cell->slices, i);
    if (!applies_to(s->resource_type, dir))
      continue;
    for (int m = 0; m < s->num_members; ++m) {
      const nr_slice_policy_member_t *pm = &s->members[m];
      if (eq_plmn(&pm->plmn, plmn) && pm->nssai.sst == nssai->sst && pm->nssai.sd == nssai->sd)
        return s;
    }
  }
  return NULL;
}

static int cmp_slice_id(const void *a, const void *b)
{
  const uint32_t id_a = ((const nr_slice_policy_t *)a)->id;
  const uint32_t id_b = ((const nr_slice_policy_t *)b)->id;
  return (id_a > id_b) - (id_a < id_b);
}

/** @brief Get the slice with ID id
 * @return the slice, or NULL if none */
const nr_slice_policy_t *get_slice_by_id(const nr_cell_sched_t *cell, uint32_t id)
{
  const nr_slice_policy_t key = {.id = id};
  return bsearch(&key, seq_arr_front(&cell->slices), seq_arr_size(&cell->slices), sizeof(key), cmp_slice_id);
}

// /** @brief DL RB allocation with fixed slice partitioning: each DL slice reserves minRatio of
//  * the RBs in every slot, in order of slices; UEs without slice share the remaining RBs. Within
//  * each partition, nr_dl_proportional_fair() allocates RBs to the slice's UEs. */

static const nr_slice_policy_t no_slice = {.id = 0, .dedicated_ratio = 0, .min_ratio = 0, .max_ratio = 100};

/* print per-slice PRB sums about once per second (every 100 frames), then reset */
static void log_prbs(const char *dir, const seq_arr_t *slices, uint64_t *prb_sum, int *n_slots, int *last_frame, int frame)
{
  if (frame % 100 != 0 || frame == *last_frame)
    return;
  *last_frame = frame;
  const int n_slices = seq_arr_size(slices);
  for (int i = 0; i <= n_slices; i++) {
    const nr_slice_policy_t *s = i < n_slices ? seq_arr_at(slices, i) : &no_slice;
    LOG_I(NR_MAC, "%s slice %u PRBs: %lu in %d slots\n", dir, s->id, (unsigned long)prb_sum[i], *n_slots);
    prb_sum[i] = 0;
  }
  *n_slots = 0;
}

int nr_dl_slicing_fixed(const nr_dl_sched_params_t *params, nr_dl_candidate_t *cand, int n)
{
  static uint64_t prb_sum[NR_MAX_SLICES + 1];
  static float credit[NR_MAX_SLICES + 1]; /* unspent fractional/small RB entitlement, carried to next slot */
  static int n_slots = 0, last_frame = -1;
  const int min_rb = 5; /* min_rbSize of nr_dl_proportional_fair() */
  const seq_arr_t *slices = &params->cell->slices;
  const int n_slices = seq_arr_size(slices);
  const int n_rb = params->n_rb_avail[0];
  nr_dl_sched_params_t p = *params;

  bool skipped[n];
  for (int c = 0; c < n; c++)
    skipped[c] = cand[c].skipped;

  /* slice n_slices = UEs without a slice */
  int members[n_slices + 1], reserve[n_slices + 1];
  float entitled[n_slices + 1];
  int n_active = 0, reserved = 0;
  for (int i = 0; i <= n_slices; i++) {
    const nr_slice_policy_t *s = i < n_slices ? seq_arr_at(slices, i) : &no_slice;
    members[i] = 0;
    for (int c = 0; c < n; c++)
      members[i] += !skipped[c] && cand[c].slice_id == s->id;
    n_active += members[i] > 0;
    if (members[i] == 0)
      credit[i] = 0;
    /* active slice reserves minRatio, idle slice only its dedicatedRatio (+ carried credit) */
    entitled[i] = n_rb * (members[i] > 0 ? s->min_ratio : s->dedicated_ratio) / 100 + credit[i];
    reserve[i] = entitled[i];
    reserved += reserve[i];
  }

  int rb_left = n_rb, n_sched = 0;
  for (int i = 0; i <= n_slices; i++) {
    if (members[i] == 0)
      continue; /* idle: its dedicated reservation stays blocked */
    const nr_slice_policy_t *s = i < n_slices ? seq_arr_at(slices, i) : &no_slice;

    /* own reservation + equal share of unreserved RBs, capped at maxRatio */
    reserved -= reserve[i];
    int budget = reserve[i] + (rb_left - reserve[i] - reserved) / n_active--;
    budget = min(budget, (int)(n_rb * s->max_ratio / 100 + credit[i]));

    int used = 0;
    const int ue_left = params->max_num_ue - n_sched;
    if (budget >= min_rb && ue_left > 0) { /* PF asserts on fewer RBs, divides by UEs */
      for (int c = 0; c < n; c++)
        cand[c].skipped = skipped[c] || cand[c].slice_id != s->id;
      for (int b = 0; b < p.num_beams; b++)
        p.n_rb_avail[b] = budget;
      p.max_num_ue = min(members[i], ue_left);
      n_sched += nr_dl_proportional_fair(&p, cand, n);
      for (int c = 0; c < n; c++)
        if (!cand[c].skipped && cand[c].scheduled)
          used += cand[c].sched_pdsch.rbSize;
    }
    /* carry what the slice was entitled to but did not get (bounded) */
    credit[i] = max(0.0f, min(entitled[i] - used, (float)min_rb));
    if (s->id == 1 && params->frame % 100 == 0)
      LOG_I(NR_MAC, "DBG slot %d.%d: entitled %.2f budget %d used %d credit %.2f\n",
            params->frame, params->slot, entitled[i], budget, used, credit[i]);
    /* unused dedicated RBs are lost for everybody */
    rb_left -= used + max(0, (int)(n_rb * s->dedicated_ratio / 100) - used);
    prb_sum[i] += used;
  }
  n_slots++;
  log_prbs("DL", slices, prb_sum, &n_slots, &last_frame, params->frame);

  for (int c = 0; c < n; c++) /* caller checks skipped afterwards */
    cand[c].skipped = skipped[c];
  return n_sched;
}

int nr_ul_slicing_fixed(const nr_ul_sched_params_t *params, nr_ul_candidate_t *cand, int n)
{
  static uint64_t prb_sum[NR_MAX_SLICES + 1];
  static float credit[NR_MAX_SLICES + 1]; /* unspent fractional/small RB entitlement, carried to next slot */
  static int n_slots = 0, last_frame = -1;
  const int min_rb = params->min_rb;
  const seq_arr_t *slices = &params->cell->slices;
  const int n_slices = seq_arr_size(slices);
  const int n_rb = params->n_rb_avail[0];
  nr_ul_sched_params_t p = *params;

  bool skipped[n];
  for (int c = 0; c < n; c++)
    skipped[c] = cand[c].skipped;

  /* slice n_slices = UEs without a slice */
  int members[n_slices + 1], reserve[n_slices + 1];
  float entitled[n_slices + 1];
  int n_active = 0, reserved = 0;
  for (int i = 0; i <= n_slices; i++) {
    const nr_slice_policy_t *s = i < n_slices ? seq_arr_at(slices, i) : &no_slice;
    members[i] = 0;
    for (int c = 0; c < n; c++)
      members[i] += !skipped[c] && cand[c].slice_id == s->id;
    n_active += members[i] > 0;
    if (members[i] == 0)
      credit[i] = 0;
    /* active slice reserves minRatio, idle slice only its dedicatedRatio (+ carried credit) */
    entitled[i] = n_rb * (members[i] > 0 ? s->min_ratio : s->dedicated_ratio) / 100 + credit[i];
    reserve[i] = entitled[i];
    reserved += reserve[i];
  }

  int rb_left = n_rb, n_sched = 0;
  for (int i = 0; i <= n_slices; i++) {
    if (members[i] == 0)
      continue; /* idle: its dedicated reservation stays blocked */
    const nr_slice_policy_t *s = i < n_slices ? seq_arr_at(slices, i) : &no_slice;

    /* own reservation + equal share of unreserved RBs, capped at maxRatio */
    reserved -= reserve[i];
    int budget = reserve[i] + (rb_left - reserve[i] - reserved) / n_active--;
    budget = min(budget, (int)(n_rb * s->max_ratio / 100 + credit[i]));

    int used = 0;
    const int ue_left = params->max_num_ue - n_sched;
    if (budget >= min_rb && ue_left > 0) { /* PF asserts on fewer RBs, divides by UEs */
      for (int c = 0; c < n; c++)
        cand[c].skipped = skipped[c] || cand[c].slice_id != s->id;
      for (int b = 0; b < p.num_beams; b++)
        p.n_rb_avail[b] = budget;
      p.max_num_ue = min(max(members[i], 2), ue_left); /* UL PF needs >= 2 for default grants */
      n_sched += nr_ul_proportional_fair(&p, cand, n);
      for (int c = 0; c < n; c++)
        if (!cand[c].skipped && cand[c].scheduled)
          used += cand[c].sched_pusch.rbSize;
    }
    /* carry what the slice was entitled to but did not get (bounded) */
    credit[i] = max(0.0f, min(entitled[i] - used, (float)min_rb));
    /* unused dedicated RBs are lost for everybody */
    rb_left -= used + max(0, (int)(n_rb * s->dedicated_ratio / 100) - used);
    prb_sum[i] += used;
  }
  n_slots++;
  log_prbs("UL", slices, prb_sum, &n_slots, &last_frame, params->frame);

  for (int c = 0; c < n; c++) /* caller checks skipped afterwards */
    cand[c].skipped = skipped[c];
  return n_sched;
}
