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
