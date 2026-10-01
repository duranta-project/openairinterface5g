/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "gNB_scheduler_ulsch_MU_MIMO_policies.h"
#include "LAYER2/NR_MAC_gNB/mac_proto.h"
#include "executables/softmodem-common.h"
#include "common/utils/nr/nr_common.h"
#include "utils.h"
#include <openair2/UTIL/OPT/opt.h>
#include "LAYER2/nr_rlc/nr_rlc_oai_api.h"
#include <math.h>

#define CROSS_CORRELATION_THRESHOLD 0.7
#define MU_MIMO_PF_BIAS 1.5f

typedef struct {
  nr_ul_candidate_t *alt;
  float rho;
} nr_mu_fallback_candidate_t;

// Allow up to 2 periods of SRS periodicity
static int nr_ul_mumimo_max_age_slots(const gNB_MAC_INST *mac)
{
  return 2 * mac->srs_period_slots;
}

// Computes cross correlation coefficient of a UE pair based on the SRS channel estimates
// Currently supports single layer UEs only
static float nr_srs_pair_correlation(const nr_srs_eff_channel_info_t *a, const nr_srs_eff_channel_info_t *b)
{
  // Check if the measurements are valid
  if (!a->valid || !b->valid)
    return 1.0f;

  // Supports only single layer UEs
  if (a->num_layers != 1 || b->num_layers != 1)
    return 1.0f;

  // Check for number of PRGs and receive antennas
  if (a->num_rx != b->num_rx || a->num_prg != b->num_prg) {
    LOG_E(NR_MAC, "SRS geometry mismatch (rx antennas %u/%u, PRGs %u/%u)\n", a->num_rx, b->num_rx, a->num_prg, b->num_prg);
    return 1.0f;
  }

  const int nrx = a->num_rx;
  const int nprg = a->num_prg;
  float rho_max = 0.0f;

  // Compute cross-correlation per PRG
  int num_valid_prg = 0;
  for (int p = 0; p < nprg; p++) {
    int64_t cross_re = 0, cross_im = 0;
    int64_t norm_h_a = 0, norm_h_b = 0;

    for (int r = 0; r < nrx; r++) {
      // layer index is 0: rank 1
      const c16_t h_a = a->h_srs_eff[p][0][r];
      const c16_t h_b = b->h_srs_eff[p][0][r];
      // conj(h_a) * h_b
      cross_re += (int64_t)h_a.r * h_b.r + (int64_t)h_a.i * h_b.i;
      cross_im += (int64_t)h_a.r * h_b.i - (int64_t)h_a.i * h_b.r;
      norm_h_a += (int64_t)h_a.r * h_a.r + (int64_t)h_a.i * h_a.i;
      norm_h_b += (int64_t)h_b.r * h_b.r + (int64_t)h_b.i * h_b.i;
    }

    if (norm_h_a == 0 || norm_h_b == 0)
      continue;

    num_valid_prg++;

    //       || h_a^H * h_b ||
    // rho = -----------------
    //       ||h_a|| * ||h_b||
    const double numerator = (double)cross_re * cross_re + (double)cross_im * cross_im;
    const double denominator = (double)norm_h_a * (double)norm_h_b;
    double rho2 = numerator / denominator;
    if (rho2 > 1.0)
      rho2 = 1.0;

    const float rho = sqrtf((float)rho2);
    if (rho > rho_max)
      rho_max = rho;

    // Worst-PRG check with early-out: one aligned PRG is enough to reject.
    if (rho_max > CROSS_CORRELATION_THRESHOLD)
      return rho_max;
  }

  if (num_valid_prg == 0)
    return 1.0f;

  return rho_max;
}

// Get the SRS channel estimates age gap in slots
static int get_srs_gap_slots(const nr_srs_eff_channel_info_t *a, const nr_srs_eff_channel_info_t *b, int slots_per_frame)
{
  const int total_slots = 1024 * slots_per_frame;
  int ta = a->frame * slots_per_frame + a->slot;
  int tb = b->frame * slots_per_frame + b->slot;
  int gap = ta - tb;
  if (gap < 0)
    gap = -gap;
  if (gap > total_slots / 2)
    gap = total_slots - gap;
  return gap;
}

// Finds this UE's cached pairing entry for partner_rnti, or -1 none/invalid.
static int nr_mu_list_find(const nr_mu_orthogonal_list_t *list, rnti_t partner_rnti)
{
  for (int i = 0; i < list->num_partners; i++)
    if (list->partners[i].partner_rnti == partner_rnti)
      return i;
  return -1;
}

// swap the last element into the removed slot's place, then shrink.
static void nr_mu_list_remove_at(nr_mu_orthogonal_list_t *list, int idx)
{
  list->num_partners--;
  list->partners[idx] = list->partners[list->num_partners];
}

static void nr_mu_list_insert_or_update(nr_mu_orthogonal_list_t *list, rnti_t partner_rnti, bool orthogonal, float rho, int gap)
{
  int idx = nr_mu_list_find(list, partner_rnti);
  if (!orthogonal) {
    if (idx >= 0)
      nr_mu_list_remove_at(list, idx);
    return;
  }
  if (idx >= 0) {
    list->partners[idx].rho = rho;
    list->partners[idx].age_gap_slots = gap;
    return;
  }
  if (list->num_partners < NR_MU_MIMO_MAX_TRACKED_PARTNERS) {
    list->partners[list->num_partners++] =
        (nr_mu_orthogonal_partner_t){.partner_rnti = partner_rnti, .rho = rho, .age_gap_slots = gap};
    return;
  }
  int worst = 0;
  for (int i = 1; i < list->num_partners; i++)
    if (list->partners[i].rho > list->partners[worst].rho)
      worst = i;
  if (rho < list->partners[worst].rho)
    list->partners[worst] = (nr_mu_orthogonal_partner_t){.partner_rnti = partner_rnti, .rho = rho, .age_gap_slots = gap};
}

void nr_mu_update_pair_cache(gNB_MAC_INST *mac, nr_cell_sched_t *cell, NR_UE_info_t *UE)
{
  const int slots_per_frame = cell->frame_structure.numb_slots_frame;
  const int max_age_slots = nr_ul_mumimo_max_age_slots(mac);
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  const nr_srs_eff_channel_info_t *sig = &sched_ctrl->srs_eff_channel_info;

  UE_iterator (mac->UE_info.connected_ue_list, other) {
    if (other == UE || other->pcell != UE->pcell)
      continue;

    NR_UE_sched_ctrl_t *other_ctrl = &other->UE_sched_ctrl;
    const nr_srs_eff_channel_info_t *other_sig = &other_ctrl->srs_eff_channel_info;

    bool orthogonal = false;
    float rho = 1.0f;
    int gap = -1;
    if (sig->valid && other_sig->valid && sig->num_layers == 1 && other_sig->num_layers == 1) {
      gap = get_srs_gap_slots(sig, other_sig, slots_per_frame);
      if (gap <= max_age_slots) {
        rho = nr_srs_pair_correlation(sig, other_sig);
        orthogonal = rho < CROSS_CORRELATION_THRESHOLD;
      }
    }
    nr_mu_list_insert_or_update(&sched_ctrl->mu_orthogonal_partners, other->rnti, orthogonal, rho, gap);
    nr_mu_list_insert_or_update(&other_ctrl->mu_orthogonal_partners, UE->rnti, orthogonal, rho, gap);
  }
}

void nr_mu_invalidate_pair_cache(gNB_MAC_INST *mac, rnti_t disconnected_rnti)
{
  UE_iterator (mac->UE_info.connected_ue_list, other) {
    nr_mu_orthogonal_list_t *list = &other->UE_sched_ctrl.mu_orthogonal_partners;
    int idx = nr_mu_list_find(list, disconnected_rnti);
    if (idx >= 0)
      nr_mu_list_remove_at(list, idx);
  }
}

static bool mu_orthogonality_check(const nr_ul_candidate_t *a, const nr_ul_candidate_t *b, float *rho_out)
{
  if (a->sched_pusch.nrOfLayers + b->sched_pusch.nrOfLayers > a->UE->UE_sched_ctrl.srs_eff_channel_info.num_rx)
    return false;

  // Lookup if the pair is orthogonal
  const nr_mu_orthogonal_list_t *list = &a->UE->UE_sched_ctrl.mu_orthogonal_partners;
  int idx = nr_mu_list_find(list, b->UE->rnti);
  if (idx < 0)
    return false;
  *rho_out = list->partners[idx].rho;
  return true;
}

static int compare_ul_mu_pf(const nr_ul_candidate_t *ca, const nr_ul_candidate_t *cb)
{
  if (ca->is_retx != cb->is_retx)
    return ca->is_retx ? -1 : 1;
  if (ca->is_retx)
    return (ca->retx_rbSize < cb->retx_rbSize) - (ca->retx_rbSize > cb->retx_rbSize);

  const bool dg_a = needs_default_grant(ca);
  const bool dg_b = needs_default_grant(cb);
  if (dg_a != dg_b)
    return dg_a ? -1 : 1;

  /* most SRs first: see compare_ul_pf()'s identical comment. */
  if (ca->sr_cnt != cb->sr_cnt)
    return (ca->sr_cnt < cb->sr_cnt) - (ca->sr_cnt > cb->sr_cnt);

  /* default grants are all min_rb: nothing else to order them by */
  if (dg_a)
    return 0;

  /* Finally the UEs with data, highest PF weight first (paired UEs get an MU-MIMO bias). */
  const float wa = ul_pf_weight(ca->sched_pusch.mcs, ca->mcs_table, ca->sched_pusch.nrOfLayers, ca->avg_throughput)
                   * (ca->mu_partner >= 0 ? MU_MIMO_PF_BIAS : 1.0f);
  const float wb = ul_pf_weight(cb->sched_pusch.mcs, cb->mcs_table, cb->sched_pusch.nrOfLayers, cb->avg_throughput)
                   * (cb->mu_partner >= 0 ? MU_MIMO_PF_BIAS : 1.0f);
  return (wa < wb) - (wa > wb);
}

static int compare_ul_mu_pf_rb_ptrs(const void *a, const void *b)
{
  const nr_ul_candidate_t *ca = *(const nr_ul_candidate_t *const *)a;
  const nr_ul_candidate_t *cb = *(const nr_ul_candidate_t *const *)b;
  return compare_ul_mu_pf(ca, cb);
}

/* Unary MU eligibility: rank-1, valid SRS. Freshness (age) is enforced
 * per-pair in nr_srs_orthogonality_check. retx/inactive UEs never pair. */
static bool is_mu_eligible(const nr_ul_candidate_t *c)
{
  if (c->is_retx || needs_default_grant(c))
    return false;
  if (c->sched_pusch.nrOfLayers != 1) // rank-1 only for now
    return false;
  return c->UE->UE_sched_ctrl.srs_eff_channel_info.valid;
}

static void find_mu_mimo_pairs(nr_ul_candidate_t *candidates, int n_candidates)
{
  for (int i = 0; i < n_candidates; i++)
    candidates[i].mu_partner = -1;

  for (int i = 0; i < n_candidates; i++) {
    if (candidates[i].mu_partner >= 0 || !is_mu_eligible(&candidates[i]))
      continue;
    int best_j = -1;
    float best_rho = CROSS_CORRELATION_THRESHOLD;
    for (int j = i + 1; j < n_candidates; j++) {
      if (candidates[j].mu_partner >= 0 || !is_mu_eligible(&candidates[j]))
        continue;
      float rho;
      if (mu_orthogonality_check(&candidates[i], &candidates[j], &rho) && rho < best_rho) {
        best_rho = rho;
        best_j = j;
      }
    }
    if (best_j >= 0) {
      candidates[i].mu_partner = best_j;
      candidates[best_j].mu_partner = i;
    }
  }
}

static bool mu_coschedule_partner(const nr_ul_sched_params_t *params,
                                  nr_ul_candidate_t *anchor,
                                  nr_ul_candidate_t *partner,
                                  int rbStart,
                                  int rbSize)
{
  const NR_sched_pusch_t saved_pusch = partner->sched_pusch;
  const uint16_t saved_slbitmap = partner->alloc_slbitmap;

  /* Resource region + DMRS come from the anchor: the partner shares the exact
   * PRBs and sits in the anchor's CDM group on a distinct port. Set these BEFORE
   * the PHR check so it sizes the TB against the config actually transmitted.
   * Do NOT retouch the anchor — already committed against its own overhead. */
  partner->sched_pusch.rbStart = rbStart;
  partner->sched_pusch.rbSize = rbSize;
  partner->sched_pusch.tda_info = anchor->sched_pusch.tda_info;
  partner->sched_pusch.time_domain_allocation = anchor->sched_pusch.time_domain_allocation;
  partner->alloc_slbitmap = anchor->alloc_slbitmap;

  /* alloc_beam_idx already equal — same beam group (ul_rb_alloc is per beam) */

  /* compute dmrs_info from the anchor's tda_info */
  partner->sched_pusch.dmrs_info = get_ul_dmrs_params(params->scc,
                                                      &partner->UE->current_UL_BWP,
                                                      &partner->sched_pusch.tda_info,
                                                      partner->sched_pusch.nrOfLayers,
                                                      anchor->sched_pusch.dmrs_info.dmrs_ports << anchor->sched_pusch.nrOfLayers,
                                                      anchor->sched_pusch.dmrs_info.num_dmrs_cdm_grps_no_data);

  /* Partner's rbSize is locked to the anchor's region — only MCS can drop for
   * power. same_rb_min_mcs holds RBs, walks MCS down; !valid => cannot pair. */
  uint8_t mcs = partner->sched_pusch.mcs;
  nr_ul_phr_advice_t advice;
  if (partner->pcmax != 0 && !nr_ul_check_phr(params, partner, rbSize, mcs, &advice)) {
    if (!advice.same_rb_min_mcs.valid) {
      partner->sched_pusch = saved_pusch;
      partner->alloc_slbitmap = saved_slbitmap;
      return false;
    }
    mcs = advice.same_rb_min_mcs.mcs;
  }
  partner->sched_pusch.mcs = mcs;

  if (!commit_ul_alloc(params, partner)) { // partner's own CCE
    partner->sched_pusch = saved_pusch;
    partner->alloc_slbitmap = saved_slbitmap;
    return false;
  }

  partner->scheduled = true;
  anchor->UE->mac_stats.mu_coscheduled++;
  partner->UE->mac_stats.mu_coscheduled++;

  return true;
}

int nr_ul_pf_mu_mimo(const nr_ul_sched_params_t *params, nr_ul_candidate_t *candidates, int n_candidates)
{
  int n_scheduled = 0;
  const int min_rb = params->min_rb;
  const int max_rbSize = params->n_rb_avail[0]; // BW is the same across all beams, just use beam 0
  DevAssert(max_rbSize >= min_rb);

  find_mu_mimo_pairs(candidates, n_candidates);

  /* Build pointer array sorted by PF priority: retx, then default grants, then data UEs with highest weight first*/
  nr_ul_candidate_t *order[MAX_MOBILES_PER_GNB];
  int n_active = 0;
  FOR_EACH_CANDIDATE(cand, candidates, n_candidates)
  if (!cand->skipped)
    order[n_active++] = cand;
  qsort(order, n_active, sizeof(*order), compare_ul_mu_pf_rb_ptrs);

  const int dg_budget = params->max_num_ue / 2;
  if (dg_budget == 0)
    LOG_W(NR_MAC,
          "max_num_ue %d leaves no budget for the default grants (max_num_ue / 2 == 0): phase 2 never runs, UEs "
          "waiting for a default grant are served after the UEs with data\n",
          params->max_num_ue);

  /* compare_ul_mu_pf() sorted the candidates into retransmissions, then UEs waiting for a
   * default grant, then UEs with data, so every phase below walks the array on from where the
   * previous one stopped. */
  nr_ul_candidate_t **ue_it = order;
  nr_ul_candidate_t **const ue_end = order + n_active;

  /* Phase 1: HARQ retransmissions (highest priority, exact RBs) */
  for (; ue_it < ue_end && (*ue_it)->is_retx; ue_it++) {
    nr_ul_candidate_t *cand = *ue_it;

    nr_ul_port_select_default(params, cand);

    int rbStart;
    uint16_t *vrb_map = params->vrb_map_UL[cand->alloc_beam_idx];
    int block_len = find_largest_free_block(vrb_map, cand->alloc_slbitmap, cand->bwp_start, cand->bwp_size, &rbStart);
    if (block_len < cand->retx_rbSize)
      continue;

    COMMIT_UL_ALLOC(params, cand, rbStart, cand->retx_rbSize, cand->sched_pusch.mcs, n_scheduled);
  }

  /* Phase 2: default grants (no BSR data, need scheduling for TA/SR), capped at dg_budget */
  for (int n = 0; ue_it < ue_end && needs_default_grant(*ue_it) && n < dg_budget; ue_it++) {
    nr_ul_candidate_t *cand = *ue_it;

    nr_ul_port_select_default(params, cand);

    uint16_t *vrb_map = params->vrb_map_UL[cand->alloc_beam_idx];
    int rbStart;
    int block_len = find_largest_free_block(vrb_map, cand->alloc_slbitmap, cand->bwp_start, cand->bwp_size, &rbStart);
    if (block_len < min_rb)
      continue;

    COMMIT_UL_ALLOC(params, cand, rbStart, min_rb, cand->sched_pusch.mcs, n_scheduled);
    if (cand->scheduled)
      n++;
  }

  /* The UEs the share did not cover are kept for phase 4, in case the UEs with
   * data leave any of the budget unspent. */
  nr_ul_candidate_t **ue_res = ue_it;
  while (ue_it < ue_end && needs_default_grant(*ue_it))
    ue_it++;
  nr_ul_candidate_t **const ue_res_end = ue_it;

  const int n_remain_ue = params->max_num_ue - n_scheduled;

  // Count each pair as a single group, so a co-scheduled pair is charged the RB-fairness share
  // of one allocation, not two.
  int n_paired = 0;
  FOR_EACH_CANDIDATE(cand, candidates, n_candidates)
  if (cand->mu_partner >= 0)
    n_paired++;
  const int n_groups = max(1, n_remain_ue - n_paired / 2);
  // share RBs fairly between remaining allocatable groups
  const int n_rb_per_ue = max(min_rb, max_rbSize / n_groups);

  /* Phase 3: New data UEs — PF priority order, count number of RBs required,
   * store number of excess RBs. Check two additional UEs in case the first
   * ones cannot be allocated (DCI alloc fail). This is only necessary because
   * we use type-1 allocate; if we used type-0, we could fix the UEs, then give
   * iteratively the RBs as needed*/
  nr_ul_candidate_t **const ue_data = ue_it;
  uint16_t rbs_ue[MAX_MOBILES_PER_GNB] = {0};
  int excess_total_rbs = max_rbSize;
  for (int n = 0; ue_it < ue_end && n < n_remain_ue + 2; ue_it++, n++) {
    nr_ul_candidate_t *cand = *ue_it;

    nr_ul_port_select_default(params, cand);

    // calculate the number of RBs that UE would like to have. Power limitation
    // is later
    NR_pusch_dmrs_t dmrs_info = cand->sched_pusch.dmrs_info;
    NR_UE_UL_BWP_t *current_BWP = &cand->UE->current_UL_BWP;
    uint16_t Rt;
    uint8_t Qt;
    update_ul_ue_R_Qm(cand->sched_pusch.mcs, current_BWP->mcs_table, current_BWP->pusch_Config, &Rt, &Qt);
    uint32_t tb_size;
    uint16_t *want = &rbs_ue[ue_it - order];
    nr_find_nb_rb(Qt,
                  Rt,
                  current_BWP->transform_precoding,
                  cand->sched_pusch.nrOfLayers,
                  cand->sched_pusch.tda_info.nrOfSymbols,
                  dmrs_info.N_PRB_DMRS * dmrs_info.num_dmrs_symb,
                  cand->pending_bytes,
                  min_rb,
                  max_rbSize,
                  &tb_size,
                  want);
    if (n < n_remain_ue) {
      // for the first n_remain_ue UEs: account number of RBs
      // so excess RBs not used by some UEs could be given to others
      excess_total_rbs -= min(*want, n_rb_per_ue);
      excess_total_rbs = max(excess_total_rbs, 0);
    }
  }

  /* allocate up to all UEs checked above */
  for (ue_it = ue_data; ue_it < ue_end; ue_it++) {
    nr_ul_candidate_t *cand = *ue_it;
    const int j = ue_it - order;

    /* candidate as its MU-MIMO partner must not give it a second, independent grant. */
    if (cand->scheduled || rbs_ue[j] == 0)
      continue;

    // give every UE its chunk of data. If total_rbs indicates excess RBs, give
    // additionally as appropriate.
    int rb_req = min(rbs_ue[j], n_rb_per_ue);
    int excess_req = max(rbs_ue[j] - rb_req, 0);
    uint8_t mcs = cand->sched_pusch.mcs;
    // check if power is enough for rb_req + excess_req if actually received a
    // PHR (PCmax > 0, otherwise nothing is scheduled)
    nr_ul_phr_advice_t advice;
    if (cand->pcmax != 0 && !nr_ul_check_phr(params, cand, rb_req + excess_req, mcs, &advice)) {
      int lim_rb = advice.max_mcs_min_rb.rbSize;
      if (lim_rb > rb_req) {
        // enough for rb_req, but not excess_req
        excess_req = lim_rb - rb_req;
      } else {
        // not enough for rb_req
        excess_req = 0;
        rb_req = lim_rb;
      }
      mcs = advice.max_mcs_min_rb.mcs;
    }
    if (excess_total_rbs > 0 && excess_req > 0) {
      int excess_ack = min(excess_total_rbs, excess_req);
      rb_req += excess_ack;
      excess_total_rbs -= excess_ack;
      DevAssert(excess_total_rbs >= 0);
    }
    int rbStart, rbSize;
    uint16_t *vrb_map = params->vrb_map_UL[cand->alloc_beam_idx];
    if (!get_rb_alloc(min_rb, rb_req, cand->bwp_start, cand->bwp_size, vrb_map, cand->alloc_slbitmap, &rbStart, &rbSize))
      continue;
    COMMIT_UL_ALLOC(params, cand, rbStart, rbSize, mcs, n_scheduled);

    /* MU-MIMO: if the anchor committed and can pair, co-schedule it onto the same RBs */
    if (!cand->scheduled || cand->mu_partner < 0)
      continue;
    nr_ul_candidate_t *partner = &candidates[cand->mu_partner];
    if (!partner->scheduled && mu_coschedule_partner(params, cand, partner, rbStart, rbSize)) {
      n_scheduled++;
      continue;
    }
    nr_mu_fallback_candidate_t fallbacks[NR_MU_MIMO_MAX_TRACKED_PARTNERS];
    int n_fallbacks = 0;
    for (int m = 0; m < n_active && n_fallbacks < NR_MU_MIMO_MAX_TRACKED_PARTNERS; m++) {
      nr_ul_candidate_t *alt = order[m];
      if (alt == cand || alt == partner || alt->scheduled || !is_mu_eligible(alt))
        continue;
      float rho;
      if (!mu_orthogonality_check(cand, alt, &rho))
        continue;
      int pos = n_fallbacks++;
      while (pos > 0 && fallbacks[pos - 1].rho > rho) {
        fallbacks[pos] = fallbacks[pos - 1];
        pos--;
      }
      fallbacks[pos] = (nr_mu_fallback_candidate_t){.alt = alt, .rho = rho};
    }
    for (int f = 0; f < n_fallbacks; f++) {
      if (mu_coschedule_partner(params, cand, fallbacks[f].alt, rbStart, rbSize)) {
        n_scheduled++;
        break;
      }
    }
  }

  /* Phase 4: if data requests did not use all DCIs, continue with the default grants phase 2's
   * budget deferred. */
  for (; ue_res < ue_res_end; ue_res++) {
    nr_ul_candidate_t *cand = *ue_res;

    nr_ul_port_select_default(params, cand);

    uint16_t *vrb_map = params->vrb_map_UL[cand->alloc_beam_idx];
    int rbStart;
    int block_len = find_largest_free_block(vrb_map, cand->alloc_slbitmap, cand->bwp_start, cand->bwp_size, &rbStart);
    if (block_len < min_rb)
      continue;

    COMMIT_UL_ALLOC(params, cand, rbStart, min_rb, cand->sched_pusch.mcs, n_scheduled);
  }

  return n_scheduled;
}
