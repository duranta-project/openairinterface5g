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
#include "PHY/sse_intrin.h"

typedef struct {
  nr_ul_candidate_t *alt;
  float rho;
} nr_mu_fallback_candidate_t;

/* Effective channel of each layer of the UE after its precoder for the selected rank and TPMI,
 * sum over the ports u of h[u][g][p] * w[u][l], normalized per PRG. channel_matrix is the SRS
 * report, [port][gNB antenna][PRG]. */
nr_srs_sig_t *nr_srs_sig_create(const c16_t *channel_matrix,
                                int num_prg,
                                int num_rx,
                                int num_ports,
                                int ul_ri,
                                long transform_precoding,
                                uint8_t tpmi)
{
  // UL codebooks of TS 38.211 Tables 6.3.1.5-1 to 6.3.1.5-5: up to 2 layers
  if (ul_ri > 1)
    return NULL;
  DevAssert(num_prg <= MAX_BWP_SIZE);
  const int num_layers = ul_ri + 1;
  char w[num_layers][num_ports];
  for (int l = 0; l < num_layers; l++)
    for (int u = 0; u < num_ports; u++)
      w[l][u] = get_precoder_matrix_coef(ul_ri, num_ports, transform_precoding, tpmi, u, l);

  nr_srs_sig_t *sig = calloc_or_fail(1, sizeof(*sig));
  sig->num_layers = num_layers;
  sig->num_rx = num_rx;
  sig->num_prg = num_prg;
  sig->prg_stride = (num_prg + 7) & ~7;
  const size_t planes = (size_t)num_layers * num_rx * sig->prg_stride;
  // aligned for the vector loads, padding PRGs cleared
  sig->re = malloc16_clear_or_fail(2 * planes * sizeof(float));
  sig->im = sig->re + planes;
  sig->prg_valid = calloc_or_fail(num_layers * num_prg, sizeof(*sig->prg_valid));
  int num_valid = 0;
  for (int l = 0; l < num_layers; l++) {
    for (int p = 0; p < num_prg; p++) {
      int32_t eff_r[num_rx], eff_i[num_rx];
      int64_t norm = 0;
      for (int g = 0; g < num_rx; g++) {
        eff_r[g] = eff_i[g] = 0;
        for (int u = 0; u < num_ports; u++) {
          const c16_t hw = nr_h_times_w(channel_matrix[(u * num_rx + g) * num_prg + p], w[l][u]);
          eff_r[g] += hw.r;
          eff_i[g] += hw.i;
        }
        norm += (int64_t)eff_r[g] * eff_r[g] + (int64_t)eff_i[g] * eff_i[g];
      }
      uint8_t *valid = &sig->prg_valid[l * num_prg + p];
      *valid = norm > 0;
      if (!*valid)
        continue;
      num_valid++;
      const float scale = 1.0f / sqrtf((float)norm);
      for (int g = 0; g < num_rx; g++) {
        sig->re[(l * num_rx + g) * sig->prg_stride + p] = eff_r[g] * scale;
        sig->im[(l * num_rx + g) * sig->prg_stride + p] = eff_i[g] * scale;
      }
    }
  }
  if (num_valid == 0) {
    nr_srs_sig_free(sig);
    return NULL;
  }
  return sig;
}

void nr_srs_sig_free(nr_srs_sig_t *sig)
{
  if (!sig)
    return;
  free16(sig->re, 0);
  free(sig->prg_valid);
  free(sig);
}

/* Maximum over the PRGs of the cross-correlation coefficient |h_a^H h_b| / (|h_a| |h_b|): one
 * aligned PRG is enough to reject a pair. Vectorized across PRGs (the planes are 32-byte aligned
 * and prg_stride is a multiple of 8), so all of them are computed. */
float nr_srs_pair_correlation(const nr_srs_sig_t *a, const nr_srs_sig_t *b)
{
  DevAssert(a->num_layers == 1 && b->num_layers == 1);
  if (a->num_rx != b->num_rx || a->num_prg != b->num_prg) {
    LOG_E(NR_MAC, "SRS geometry mismatch (rx antennas %u/%u, PRGs %u/%u)\n", a->num_rx, b->num_rx, a->num_prg, b->num_prg);
    return 1.0f;
  }
  const int num_blocks = a->prg_stride / 8;
  simde__m256 cross_re[NR_SRS_SIG_MAX_STRIDE / 8], cross_im[NR_SRS_SIG_MAX_STRIDE / 8];
  for (int k = 0; k < num_blocks; k++)
    cross_re[k] = cross_im[k] = simde_mm256_setzero_ps();
  for (int r = 0; r < a->num_rx; r++) {
    const simde__m256 *ar = (const simde__m256 *)(a->re + r * a->prg_stride);
    const simde__m256 *ai = (const simde__m256 *)(a->im + r * a->prg_stride);
    const simde__m256 *br = (const simde__m256 *)(b->re + r * b->prg_stride);
    const simde__m256 *bi = (const simde__m256 *)(b->im + r * b->prg_stride);
    // conj(h_a) * h_b, 8 PRGs at a time
    for (int k = 0; k < num_blocks; k++) {
      cross_re[k] =
          simde_mm256_add_ps(cross_re[k], simde_mm256_add_ps(simde_mm256_mul_ps(ar[k], br[k]), simde_mm256_mul_ps(ai[k], bi[k])));
      cross_im[k] =
          simde_mm256_add_ps(cross_im[k], simde_mm256_sub_ps(simde_mm256_mul_ps(ar[k], bi[k]), simde_mm256_mul_ps(ai[k], br[k])));
    }
  }
  const float *cr = (const float *)cross_re, *ci = (const float *)cross_im;
  float rho2_max = 0.0f;
  int num_valid_prg = 0;
  for (int p = 0; p < a->num_prg; p++) {
    if (!a->prg_valid[p] || !b->prg_valid[p])
      continue;
    num_valid_prg++;
    const float rho2 = cr[p] * cr[p] + ci[p] * ci[p];
    if (rho2 > rho2_max)
      rho2_max = rho2;
  }
  if (num_valid_prg == 0)
    return 1.0f;
  return rho2_max < 1.0f ? sqrtf(rho2_max) : 1.0f;
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

static void nr_mu_list_insert_or_update(nr_mu_orthogonal_list_t *list,
                                        rnti_t partner_rnti,
                                        bool orthogonal,
                                        float rho,
                                        fsn_t srs_time)
{
  const nr_mu_orthogonal_partner_t entry = {.partner_rnti = partner_rnti, .rho = rho, .srs_time = srs_time};
  int idx = nr_mu_list_find(list, partner_rnti);
  if (!orthogonal) {
    if (idx >= 0)
      nr_mu_list_remove_at(list, idx);
    return;
  }
  if (idx >= 0) {
    list->partners[idx] = entry;
    return;
  }
  if (list->num_partners < NR_MU_MIMO_MAX_TRACKED_PARTNERS) {
    list->partners[list->num_partners++] = entry;
    return;
  }
  int worst = 0;
  for (int i = 1; i < list->num_partners; i++)
    if (list->partners[i].rho > list->partners[worst].rho)
      worst = i;
  if (rho < list->partners[worst].rho)
    list->partners[worst] = entry;
}

void nr_mu_update_pair_cache(gNB_MAC_INST *mac, nr_cell_sched_t *cell, NR_UE_info_t *UE)
{
  const int max_age_slots = cell->ul_mu_mimo_srs_max_age_slots;
  const float threshold = cell->radio_config.ul_mu_mimo_corr_threshold;
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  const nr_srs_eff_channel_info_t *sig = &sched_ctrl->srs_eff_channel_info;

  FOR_EACH_CONNECTED_UE (&mac->UE_info, other) {
    if (other == UE || other->pcell != UE->pcell)
      continue;

    NR_UE_sched_ctrl_t *other_ctrl = &other->UE_sched_ctrl;
    const nr_srs_eff_channel_info_t *other_sig = &other_ctrl->srs_eff_channel_info;

    bool orthogonal = false;
    float rho = 1.0f;
    fsn_t older = {0};
    // pairing of multi-layer UEs is not supported yet
    if (sig->valid && other_sig->valid && sig->sig && other_sig->sig && sig->sig->num_layers == 1
        && other_sig->sig->num_layers == 1) {
      const int gap = fsn_get_diff(other_sig->time, sig->time);
      // the pair is as old as the older of the two SRS reports
      older = gap > 0 ? sig->time : other_sig->time;
      if (abs(gap) <= max_age_slots) {
        rho = nr_srs_pair_correlation(sig->sig, other_sig->sig);
        orthogonal = rho < threshold;
      }
    }
    nr_mu_list_insert_or_update(&sched_ctrl->mu_orthogonal_partners, other->rnti, orthogonal, rho, older);
    nr_mu_list_insert_or_update(&other_ctrl->mu_orthogonal_partners, UE->rnti, orthogonal, rho, older);
  }
}

void nr_mu_invalidate_pair_cache(gNB_MAC_INST *mac, rnti_t disconnected_rnti)
{
  FOR_EACH_CONNECTED_UE (&mac->UE_info, other) {
    nr_mu_orthogonal_list_t *list = &other->UE_sched_ctrl.mu_orthogonal_partners;
    int idx = nr_mu_list_find(list, disconnected_rnti);
    if (idx >= 0)
      nr_mu_list_remove_at(list, idx);
  }
}

// Age in slots of an SRS report at the DCI slot being scheduled
static int srs_age_slots(const nr_ul_sched_params_t *params, fsn_t srs_time)
{
  const fsn_t dci_time = {.f = params->dci_frame, .s = params->dci_slot, .mu = srs_time.mu};
  return abs(fsn_get_diff(dci_time, srs_time));
}

static bool mu_orthogonality_check(const nr_ul_sched_params_t *params,
                                   const nr_ul_candidate_t *a,
                                   const nr_ul_candidate_t *b,
                                   float *rho_out)
{
  if (a->sched_pusch.nrOfLayers + b->sched_pusch.nrOfLayers > a->UE->UE_sched_ctrl.srs_eff_channel_info.num_rx)
    return false;

  // Lookup if the pair is orthogonal
  nr_mu_orthogonal_list_t *list = &a->UE->UE_sched_ctrl.mu_orthogonal_partners;
  int idx = nr_mu_list_find(list, b->UE->rnti);
  if (idx < 0)
    return false;
  // expire the pair once its SRS reports are too old, e.g. a UE stopped sending SRS
  const nr_mu_orthogonal_partner_t *p = &list->partners[idx];
  if (srs_age_slots(params, p->srs_time) > params->cell->ul_mu_mimo_srs_max_age_slots) {
    nr_mu_list_remove_at(list, idx);
    nr_mu_orthogonal_list_t *other = &b->UE->UE_sched_ctrl.mu_orthogonal_partners;
    int other_idx = nr_mu_list_find(other, a->UE->rnti);
    if (other_idx >= 0)
      nr_mu_list_remove_at(other, other_idx);
    return false;
  }
  *rho_out = p->rho;
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
  const float wa =
      ul_pf_weight(ca->sched_pusch.mcs, ca->mcs_table, ca->sched_pusch.nrOfLayers, ca->avg_throughput) * ca->mu_pf_bias;
  const float wb =
      ul_pf_weight(cb->sched_pusch.mcs, cb->mcs_table, cb->sched_pusch.nrOfLayers, cb->avg_throughput) * cb->mu_pf_bias;
  return (wa < wb) - (wa > wb);
}

static int compare_ul_mu_pf_rb_ptrs(const void *a, const void *b)
{
  const nr_ul_candidate_t *ca = *(const nr_ul_candidate_t *const *)a;
  const nr_ul_candidate_t *cb = *(const nr_ul_candidate_t *const *)b;
  return compare_ul_mu_pf(ca, cb);
}

/* MU eligibility: rank-1, valid SRS not older than the configured maximum age
 * at the scheduled DCI slot. Pair freshness is checked in mu_orthogonality_check().
 * retx/inactive UEs never pair. */
static bool is_mu_eligible(const nr_ul_sched_params_t *params, const nr_ul_candidate_t *c)
{
  if (c->is_retx || needs_default_grant(c))
    return false;
  if (c->sched_pusch.nrOfLayers != 1) // rank-1 only for now
    return false;
  if (c->UE->current_UL_BWP.dci_format != NR_UL_DCI_FORMAT_0_1
      || c->UE->current_UL_BWP.transform_precoding == NR_PUSCH_Config__transformPrecoder_enabled)
    return false;
  const nr_srs_eff_channel_info_t *srs = &c->UE->UE_sched_ctrl.srs_eff_channel_info;
  return srs->valid && srs_age_slots(params, srs->time) <= params->cell->ul_mu_mimo_srs_max_age_slots;
}

static void find_mu_mimo_pairs(const nr_ul_sched_params_t *params, nr_ul_candidate_t *candidates, int n_candidates)
{
  const nr_mac_config_t *config = &params->cell->radio_config;
  for (int i = 0; i < n_candidates; i++)
    candidates[i].mu_partner = -1;

  for (int i = 0; i < n_candidates; i++) {
    if (candidates[i].mu_partner >= 0 || !is_mu_eligible(params, &candidates[i]))
      continue;
    int best_j = -1;
    float best_rho = config->ul_mu_mimo_corr_threshold;
    for (int j = i + 1; j < n_candidates; j++) {
      if (candidates[j].mu_partner >= 0 || !is_mu_eligible(params, &candidates[j]))
        continue;
      float rho;
      if (mu_orthogonality_check(params, &candidates[i], &candidates[j], &rho) && rho < best_rho) {
        best_rho = rho;
        best_j = j;
      }
    }
    if (best_j >= 0) {
      candidates[i].mu_partner = best_j;
      candidates[best_j].mu_partner = i;
    }
  }

  for (int i = 0; i < n_candidates; i++)
    candidates[i].mu_pf_bias = candidates[i].mu_partner >= 0 ? config->ul_mu_mimo_pf_bias : 1.0f;
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

  find_mu_mimo_pairs(params, candidates, n_candidates);

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
  bool counted[MAX_MOBILES_PER_GNB] = {false};
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
      // so excess RBs not used by some UEs could be given to others.
      // A MU-MIMO partner is co-scheduled on its anchor's RBs
      const bool partner_counted = cand->mu_partner >= 0 && counted[cand->mu_partner];
      counted[cand - candidates] = true;
      if (!partner_counted) {
        excess_total_rbs -= min(*want, n_rb_per_ue);
        excess_total_rbs = max(excess_total_rbs, 0);
      }
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
      if (alt == cand || alt == partner || alt->scheduled || !is_mu_eligible(params, alt))
        continue;
      float rho;
      if (!mu_orthogonality_check(params, cand, alt, &rho))
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
