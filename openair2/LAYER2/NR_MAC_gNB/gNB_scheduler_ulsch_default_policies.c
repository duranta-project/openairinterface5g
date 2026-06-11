/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Default pluggable UL scheduler policy functions
 *
 * These are the built-in defaults behind the UL scheduler function pointers.
 * Extracted from gNB_scheduler_ulsch.c to allow clean replacement by custom
 * scheduler plugins.
 */

#include "gNB_scheduler_ulsch_default_policies.h"
#include "LAYER2/NR_MAC_gNB/mac_proto.h"
#include "executables/softmodem-common.h"
#include "common/utils/nr/nr_common.h"
#include "utils.h"
#include <openair2/UTIL/OPT/opt.h>
#include "LAYER2/nr_rlc/nr_rlc_oai_api.h"

/* Default UL RI/TPMI selector: reads rank and TPMI from SRS feedback.
 * A custom impl can compute joint (rank, TPMI) from the H matrix on the candidates. */
void nr_ul_ri_tpmi_select_default(gNB_MAC_INST *mac, nr_ul_candidate_t *cands, int n_cand)
{
  FOR_EACH_CANDIDATE(cand, cands, n_cand)
  {
    NR_UE_sched_ctrl_t *sched_ctrl = &cand->UE->UE_sched_ctrl;
    NR_UE_UL_BWP_t *current_BWP = &cand->UE->current_UL_BWP;
    cand->sched_pusch.nrOfLayers = (current_BWP->dci_format == NR_UL_DCI_FORMAT_0_0) ? 1 : sched_ctrl->srs_feedback.ul_ri + 1;
    cand->sched_pusch.tpmi = sched_ctrl->srs_feedback.tpmi;
  }
}

static NR_tda_info_t *get_new_tda_for_srs(gNB_MAC_INST *nrmac, const NR_tda_info_t *tda_info)
{
  // by current design, the next TDA would be the one for SRS with one less symbol
  NR_tda_info_t *next = seq_arr_next(&nrmac->ul_tda, tda_info);
  if (next == seq_arr_end(&nrmac->ul_tda))
    return NULL;
  AssertFatal(next->k2 == tda_info->k2,
              "K2 in TDA information for SRS %ld doesn't match with current one %ld\n",
              next->k2,
              tda_info->k2);
  AssertFatal(next->startSymbolIndex == tda_info->startSymbolIndex,
              "startSymbolIndex in TDA information for SRS %d doesn't match with current one %d\n",
              next->startSymbolIndex,
              tda_info->startSymbolIndex);
  AssertFatal(next->nrOfSymbols == tda_info->nrOfSymbols - 1,
              "nrOfSymbols in TDA information for SRS %d should be 1 more than current one %d\n",
              next->nrOfSymbols,
              tda_info->nrOfSymbols);
  return next;
}


/* Default UL TDA selector: picks the best TDA per candidate using each candidate's
 * allocated beam to check the correct VRB map. For retransmissions, reuses the original
 * TDA when it is among the valid candidates for this slot; otherwise falls back to the
 * per-beam best TDA with TBS refit. */
int nr_ul_tda_select_default(gNB_MAC_INST *mac,
                             nr_ul_candidate_t *cands,
                             int n_cand,
                             frame_t sched_frame,
                             slot_t sched_slot,
                             int k2)
{
  const NR_tda_info_t *tda_list = NULL;
  int n_tda = get_num_ul_tda(mac, sched_slot, k2, &tda_list);
  if (n_tda == 0)
    return 0;

  NR_ServingCellConfigCommon_t *scc = mac->common_channels[0].ServingCellConfigCommon;

  int n_valid = 0;
  FOR_EACH_CANDIDATE(cand, cands, n_cand)
  {
    if (cand->skipped)
      continue;

    int beam = cand->alloc_beam_idx;
    int rb_start = 0, rb_len = cand->bwp_size;
    const NR_tda_info_t *best = get_best_ul_tda(mac, beam, tda_list, n_tda, sched_frame, sched_slot, &rb_start, &rb_len);
    DevAssert(best->valid_tda);

    if (cand->is_retx) {
      /* Try to reuse the original TDA if it is among the valid candidates for this slot */
      const NR_sched_pusch_t *retInfo = &cand->UE->UE_sched_ctrl.ul_harq_processes[cand->retx_harq_pid].sched_pusch;
      /* Check exact TDA index, not just K2 — same K2 doesn't guarantee valid symbols in mixed slots */
      const NR_tda_info_t *orig = seq_arr_at(&mac->ul_tda, retInfo->time_domain_allocation);
      ptrdiff_t offset = orig - tda_list;
      if (offset >= 0 && offset < n_tda) {
        /* Original TDA is valid — reuse it directly */
        cand->sched_pusch.time_domain_allocation = retInfo->time_domain_allocation;
        cand->sched_pusch.tda_info = *orig;
        cand->alloc_slbitmap = SL_to_bitmap(orig->startSymbolIndex, orig->nrOfSymbols);
        cand->retx_rbSize = retInfo->rbSize;
        n_valid++;
        continue;
      }
      /* Original TDA not available — try the per-beam best TDA with TBS refit */
      int tda = seq_arr_dist(&mac->ul_tda, seq_arr_front(&mac->ul_tda), best);
      AssertFatal(tda >= 0 && tda < 16, "illegal TDA index %d\n", tda);
      uint16_t needed = check_ul_retx_feasibility(cand, tda, best, scc, cand->bwp_size);
      if (needed == 0) {
        LOG_D(NR_MAC, "[UE %04x] retx infeasible with TDA %d, deferring\n", cand->UE->rnti, tda);
        cand->skipped = true;
        continue;
      }
      cand->sched_pusch.time_domain_allocation = tda;
      cand->sched_pusch.tda_info = *best;
      cand->alloc_slbitmap = SL_to_bitmap(best->startSymbolIndex, best->nrOfSymbols);
      cand->retx_rbSize = needed;
    } else {
      NR_tda_info_t *srs_best = NULL;
      if (cand->sched_srs > 0) {
        srs_best = get_new_tda_for_srs(mac, best);
        if (!srs_best)
          cand->sched_srs = 0;
      }
      const NR_tda_info_t *new_best = srs_best ? srs_best : best;
      int tda = seq_arr_dist(&mac->ul_tda, seq_arr_front(&mac->ul_tda), new_best);
      AssertFatal(tda >= 0 && tda < 16, "illegal TDA index %d\n", tda);
      cand->sched_pusch.time_domain_allocation = tda;
      cand->sched_pusch.tda_info = *new_best;
      cand->alloc_slbitmap = SL_to_bitmap(new_best->startSymbolIndex, new_best->nrOfSymbols);
    }
    n_valid++;
  }
  return n_valid;
}

static int compare_ul_pf_ptrs(const void *a, const void *b)
{
  const nr_ul_candidate_t *ca = *(const nr_ul_candidate_t *const *)a;
  const nr_ul_candidate_t *cb = *(const nr_ul_candidate_t *const *)b;
  /* retx first (INFINITY weight), then highest PF weight */
  float wa = ca->is_retx ? INFINITY : ul_pf_weight(ca->current_mcs, ca->mcs_table, ca->sched_pusch.nrOfLayers, ca->avg_throughput);
  float wb = cb->is_retx ? INFINITY : ul_pf_weight(cb->current_mcs, cb->mcs_table, cb->sched_pusch.nrOfLayers, cb->avg_throughput);
  return (wa < wb) - (wa > wb);
}

int nr_ul_beam_select_default(NR_beam_info_t *beam_info,
                              const int16_t *beam_index_list,
                              nr_ul_candidate_t *candidates,
                              int n_candidates,
                              frame_t frame,
                              slot_t slot,
                              frame_t sched_frame,
                              slot_t sched_slot,
                              int slots_per_frame)
{
  /* Build pointer array sorted by PF priority so retx and high-priority UEs claim beams first. */
  nr_ul_candidate_t *order[MAX_MOBILES_PER_GNB];
  int n_active = 0;
  FOR_EACH_CANDIDATE(cand, candidates, n_candidates)
  if (!cand->skipped)
    order[n_active++] = cand;
  qsort(order, n_active, sizeof(*order), compare_ul_pf_ptrs);

  int n_valid = 0;
  for (int i = 0; i < n_active; i++) {
    nr_ul_candidate_t *cand = order[i];

    /* Allocate beam for DCI slot */
    NR_beam_alloc_t dci_beam = beam_allocation_procedure(beam_info, frame, slot, cand->beam_index, slots_per_frame);
    if (dci_beam.idx < 0) {
      LOG_D(NR_MAC, "[UE %04x][%4d.%2d] DCI beam could not be allocated\n", cand->UE->rnti, frame, slot);
      cand->skipped = true;
      continue;
    }

    /* Allocate beam for scheduled PUSCH slot */
    NR_beam_alloc_t sched_beam = beam_allocation_procedure(beam_info, sched_frame, sched_slot, cand->beam_index, slots_per_frame);
    if (sched_beam.idx < 0) {
      LOG_D(NR_MAC, "[UE %04x][%4d.%2d] Sched beam could not be allocated\n", cand->UE->rnti, frame, slot);
      reset_beam_status(beam_info, frame, slot, cand->beam_index, slots_per_frame, dci_beam.new_beam);
      cand->skipped = true;
      continue;
    }

    cand->alloc_dci_beam_idx = dci_beam.idx;
    cand->alloc_dci_beam_new = dci_beam.new_beam;
    cand->alloc_beam_idx = sched_beam.idx;
    cand->alloc_sched_beam_new = sched_beam.new_beam;
    n_valid++;
  }
  return n_valid;
}

void nr_ul_mcs_select_default(const gNB_MAC_INST *mac, nr_ul_candidate_t *candidates, int n_candidates)
{
  const NR_bler_options_t *bo = &mac->ul_bler;
  FOR_EACH_CANDIDATE(cand, candidates, n_candidates)
  {
    int mcs;
    if (cand->is_retx) {
      mcs = cand->current_mcs;
    } else if (bo->harq_round_max == 1) {
      mcs = get_mcs_from_SINRx10(cand->mcs_table, cand->snrx10, cand->sched_pusch.nrOfLayers);
      mcs = max(bo->min_mcs, min(bo->max_mcs, min(cand->max_mcs, mcs)));
    } else if (!cand->bler_updated) {
      mcs = cand->current_mcs;
    } else {
      mcs = nr_adapt_mcs_from_bler(cand->current_mcs,
                                   bo->min_mcs,
                                   cand->max_mcs,
                                   cand->bler,
                                   bo->lower,
                                   bo->upper);
    }
    cand->sched_pusch.mcs = mcs;
    if (!cand->is_retx)
      cand->UE->UE_sched_ctrl.ul_bler_stats.mcs = mcs;
  }
}

static int compare_ul_pf_rb_ptrs(const void *a, const void *b)
{
  const nr_ul_candidate_t *ca = *(const nr_ul_candidate_t *const *)a;
  const nr_ul_candidate_t *cb = *(const nr_ul_candidate_t *const *)b;
  /* retx first, then highest PF weight (uses sched_pusch.mcs, which is set by mcs_select) */
  float wa =
      ca->is_retx ? INFINITY : ul_pf_weight(ca->sched_pusch.mcs, ca->mcs_table, ca->sched_pusch.nrOfLayers, ca->avg_throughput);
  float wb =
      cb->is_retx ? INFINITY : ul_pf_weight(cb->sched_pusch.mcs, cb->mcs_table, cb->sched_pusch.nrOfLayers, cb->avg_throughput);
  return (wa < wb) - (wa > wb);
}

static void nr_ul_port_select_default(const nr_ul_sched_params_t *params, nr_ul_candidate_t *cand)
{
  if (cand->is_retx) {
    const NR_sched_pusch_t *retInfo = &cand->UE->UE_sched_ctrl.ul_harq_processes[cand->retx_harq_pid].sched_pusch;
    cand->sched_pusch.dmrs_info = retInfo->dmrs_info;
  } else {
    int layers = cand->sched_pusch.nrOfLayers;
    NR_UE_UL_BWP_t *current_BWP = &cand->UE->current_UL_BWP;
    NR_tda_info_t *tda_info = &cand->sched_pusch.tda_info;
    uint8_t cdm_groups;
    if (current_BWP->transform_precoding == NR_PUSCH_Config__transformPrecoder_enabled) {
      cdm_groups = 2;
    } else if (current_BWP->dci_format == NR_UL_DCI_FORMAT_0_0) {
      cdm_groups = (tda_info->nrOfSymbols <= 2) ? 1 : 2;
    } else {
      cdm_groups = (layers < 3) ? 1 : 2;
    }
    cand->sched_pusch.dmrs_info.dmrs_ports = (uint16_t)((1 << layers) - 1);
    cand->sched_pusch.dmrs_info =
        get_ul_dmrs_params(params->scc, current_BWP, tda_info, layers, cand->sched_pusch.dmrs_info.dmrs_ports, cdm_groups);
  }
}

int nr_ul_proportional_fair(const nr_ul_sched_params_t *params, nr_ul_candidate_t *candidates, int n_candidates)
{
  int n_scheduled = 0;
  const int min_rb = params->min_rb;

  /* Build pointer array sorted by PF priority (retx first, then highest weight) */
  nr_ul_candidate_t *order[MAX_MOBILES_PER_GNB];
  int n_active = 0;
  FOR_EACH_CANDIDATE(cand, candidates, n_candidates)
  if (!cand->skipped)
    order[n_active++] = cand;
  qsort(order, n_active, sizeof(*order), compare_ul_pf_rb_ptrs);

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
  int n_rb_per_ue = max(min_rb, max_rbSize / n_remain_ue);

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
    if (cand->is_retx || cand->sched_inactive || rbs_ue[j] == 0)
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
  }

  return n_scheduled;
}
