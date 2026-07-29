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

#define CROSS_CORRELATION_THRESHOLD 0.4
#define MU_MIMO_PF_BIAS 1.5f

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

// Get the SRS channel estimates age in slots
static int get_srs_age_slots(const nr_srs_eff_channel_info_t *sig, frame_t f, slot_t s, int slots_per_frame)
{
  int now = f * slots_per_frame + s;
  int then = sig->frame * slots_per_frame + sig->slot;
  int age = now - then;
  // SFN wrapped
  if (age < 0)
    age += 1024 * slots_per_frame;
  return age;
}

// Checks if the UE pair can be grouped together for MU-MIMO
bool nr_srs_orthogonality_check(const nr_srs_eff_channel_info_t *a,
                                const nr_srs_eff_channel_info_t *b,
                                frame_t sched_frame,
                                slot_t sched_slot,
                                int slots_per_frame,
                                int max_age_slots)
{
  if (!a->valid || !b->valid)
    return false;

  if (get_srs_age_slots(a, sched_frame, sched_slot, slots_per_frame) > max_age_slots)
    return false;
  if (get_srs_age_slots(b, sched_frame, sched_slot, slots_per_frame) > max_age_slots)
    return false;

  const float rho = nr_srs_pair_correlation(a, b);
  return rho < CROSS_CORRELATION_THRESHOLD;
}

static bool mu_orthogonality_check(const nr_ul_candidate_t *a,
                                   const nr_ul_candidate_t *b,
                                   frame_t sched_frame,
                                   slot_t sched_slot,
                                   int slots_per_frame,
                                   int max_age_slots)
{
  const nr_srs_eff_channel_info_t *sig_a = &a->UE->UE_sched_ctrl.srs_eff_channel_info;
  const nr_srs_eff_channel_info_t *sig_b = &b->UE->UE_sched_ctrl.srs_eff_channel_info;

  /* spatial feasibility: upsilon_a + upsilon_b <= N_r */
  if (a->sched_pusch.nrOfLayers + b->sched_pusch.nrOfLayers > sig_a->num_rx)
    return false;

  /* age + rho < tau (validity re-checked inside) */
  return nr_srs_orthogonality_check(sig_a, sig_b, sched_frame, sched_slot, slots_per_frame, max_age_slots);
}

static int compare_ul_mu_pf_rb_ptrs(const void *a, const void *b)
{
  const nr_ul_candidate_t *ca = *(const nr_ul_candidate_t *const *)a;
  const nr_ul_candidate_t *cb = *(const nr_ul_candidate_t *const *)b;
  /* retx first, then highest PF weight (uses sched_pusch.mcs, which is set by mcs_select) */
  float wa = ca->is_retx ? INFINITY
                         : ul_pf_weight(ca->sched_pusch.mcs, ca->mcs_table, ca->sched_pusch.nrOfLayers, ca->avg_throughput)
                               * (ca->has_mu_partner ? MU_MIMO_PF_BIAS : 1.0f);
  float wb = cb->is_retx ? INFINITY
                         : ul_pf_weight(cb->sched_pusch.mcs, cb->mcs_table, cb->sched_pusch.nrOfLayers, cb->avg_throughput)
                               * (cb->has_mu_partner ? MU_MIMO_PF_BIAS : 1.0f);
  return (wa < wb) - (wa > wb);
}

/* Unary MU eligibility: rank-1, valid SRS. Freshness (age) is enforced
 * per-pair in nr_srs_orthogonality_check. retx/inactive UEs never pair. */
static bool is_mu_eligible(const nr_ul_candidate_t *c)
{
  if (c->is_retx || c->sched_inactive)
    return false;
  if (c->sched_pusch.nrOfLayers != 1) // rank-1 only for now
    return false;
  return c->UE->UE_sched_ctrl.srs_eff_channel_info.valid;
}

static void find_mu_mimo_pairs(nr_ul_candidate_t *candidates,
                               int n_candidates,
                               const frame_t sched_frame,
                               const slot_t sched_slot,
                               const int slots_per_frame,
                               const int max_age_slots,
                               bool adj[n_candidates][n_candidates])
{
  for (int i = 0; i < n_candidates; i++) {
    if (!is_mu_eligible(&candidates[i]))
      continue;
    for (int j = i + 1; j < n_candidates; j++) {
      if (!is_mu_eligible(&candidates[j]))
        continue;
      if (mu_orthogonality_check(&candidates[i], &candidates[j], sched_frame, sched_slot, slots_per_frame, max_age_slots)) {
        adj[i][j] = adj[j][i] = true;
        candidates[i].has_mu_partner = true;
        candidates[j].has_mu_partner = true;
      }
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
  return true;
}

int nr_ul_pf_mu_mimo(const nr_ul_sched_params_t *params, nr_ul_candidate_t *candidates, int n_candidates)
{
  int n_scheduled = 0;
  const int min_rb = params->min_rb;
  const frame_t sched_frame = params->frame;
  const slot_t sched_slot = params->slot;
  const int slots_per_frame = params->cell->frame_structure.numb_slots_frame;
  const int max_age_slots = nr_ul_mumimo_max_age_slots(params->mac);

  bool mu_adj[n_candidates][n_candidates];
  memset(mu_adj, 0, sizeof(mu_adj));
  find_mu_mimo_pairs(candidates, n_candidates, sched_frame, sched_slot, slots_per_frame, max_age_slots, mu_adj);

  /* Build pointer array sorted by PF priority (retx first, then highest weight) */
  nr_ul_candidate_t *order[MAX_MOBILES_PER_GNB];
  int n_active = 0;
  FOR_EACH_CANDIDATE(cand, candidates, n_candidates)
  if (!cand->skipped)
    order[n_active++] = cand;
  qsort(order, n_active, sizeof(*order), compare_ul_mu_pf_rb_ptrs);

  /* Phase 1: HARQ retransmissions (highest priority, exact RBs) */
  for (int j = 0; j < n_active; j++) {
    nr_ul_candidate_t *cand = order[j];
    if (!cand->is_retx)
      continue;

    nr_ul_port_select_default(params, cand);

    int rbStart;
    uint16_t *vrb_map = params->vrb_map_UL[cand->alloc_beam_idx];
    int block_len = find_largest_free_block(vrb_map, cand->alloc_slbitmap, cand->bwp_start, cand->bwp_size, &rbStart);
    if (block_len < cand->retx_rbSize)
      continue;

    COMMIT_UL_ALLOC(params, cand, rbStart, cand->retx_rbSize, cand->sched_pusch.mcs, n_scheduled);
  }

  /* Phase 2: Inactive UEs (no BSR data, need scheduling for TA/SR) */
  for (int j = 0; j < n_active; j++) {
    nr_ul_candidate_t *cand = order[j];
    if (cand->is_retx || !cand->sched_inactive)
      continue;

    nr_ul_port_select_default(params, cand);

    uint16_t *vrb_map = params->vrb_map_UL[cand->alloc_beam_idx];
    int rbStart;
    int block_len = find_largest_free_block(vrb_map, cand->alloc_slbitmap, cand->bwp_start, cand->bwp_size, &rbStart);
    if (block_len < min_rb)
      continue;

    COMMIT_UL_ALLOC(params, cand, rbStart, min_rb, cand->sched_pusch.mcs, n_scheduled);
  }

  /* BW is the same across all beams, just use beam 0 */
  int max_rbSize = params->n_rb_avail[0];
  DevAssert(max_rbSize >= min_rb);
  int n_remain_ue = params->max_num_ue - n_scheduled;
  // share RBs fairly between remaining allocatable UEs
  int n_rb_per_ue = max(min_rb, max_rbSize / max(1, n_remain_ue));

  /* Phase 3: New data UEs — PF priority order, count number of RBs required,
   * store number of excess RBs. Check two additional UEs in case the first
   * ones cannot be allocated (DCI alloc fail). This is only necessary because
   * we use type-1 allocate; if we used type-0, we could fix the UEs, then give
   * iteratively the RBs as needed*/
  uint16_t rbs_ue[MAX_MOBILES_PER_GNB] = {0};
  int excess_total_rbs = max_rbSize;
  for (int j = 0, n = 0; j < n_active && n < n_remain_ue + 2; j++) {
    nr_ul_candidate_t *cand = order[j];
    if (cand->is_retx || cand->sched_inactive)
      continue;

    nr_ul_port_select_default(params, cand);

    // calculate the number of RBs that UE would like to have. Power limitation
    // is later
    NR_pusch_dmrs_t dmrs_info = cand->sched_pusch.dmrs_info;
    NR_UE_UL_BWP_t *current_BWP = &cand->UE->current_UL_BWP;
    uint16_t Rt;
    uint8_t Qt;
    update_ul_ue_R_Qm(cand->sched_pusch.mcs, current_BWP->mcs_table, current_BWP->pusch_Config, &Rt, &Qt);
    uint32_t tb_size;
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
                  &rbs_ue[j]);
    if (n < n_remain_ue) {
      // for the first n_remain_ue UEs: account number of RBs
      // so excess RBs not used by some UEs could be given to others
      excess_total_rbs -= min(rbs_ue[j], n_rb_per_ue);
      excess_total_rbs = max(excess_total_rbs, 0);
    }
    n++;
  }

  /* allocate up to all UEs checked above */
  for (int j = 0; j < n_active; j++) {
    nr_ul_candidate_t *cand = order[j];
    if (cand->is_retx || cand->sched_inactive || cand->scheduled || rbs_ue[j] == 0)
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

    /* MU-MIMO: if the anchor committed and can pair, co-schedule the first
     * admissible partner (in PF order) onto the same RBs. adj already encodes
     * eligibility + orthogonality; only `scheduled` is dynamic. */
    if (!cand->scheduled || !cand->has_mu_partner)
      continue;
    const int ai = (int)(cand - candidates);
    for (int m = j + 1; m < n_active; m++) {
      nr_ul_candidate_t *partner = order[m];
      const int bi = (int)(partner - candidates);
      if (partner->scheduled || !mu_adj[ai][bi])
        continue;
      if (mu_coschedule_partner(params, cand, partner, rbStart, rbSize)) {
        n_scheduled++;
        break;
      }
    }
  }

  return n_scheduled;
}
