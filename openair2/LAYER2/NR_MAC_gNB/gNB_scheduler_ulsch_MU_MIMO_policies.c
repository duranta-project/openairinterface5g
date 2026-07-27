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

#define MAX_AGE_SLOTS 40
#define CROSS_CORRELATION_THRESHOLD 0.4

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
                                int slots_per_frame)
{
  if (!a->valid || !b->valid)
    return false;

  if (get_srs_age_slots(a, sched_frame, sched_slot, slots_per_frame) > MAX_AGE_SLOTS)
    return false;
  if (get_srs_age_slots(b, sched_frame, sched_slot, slots_per_frame) > MAX_AGE_SLOTS)
    return false;

  const float rho = nr_srs_pair_correlation(a, b);
  return rho < CROSS_CORRELATION_THRESHOLD;
}
