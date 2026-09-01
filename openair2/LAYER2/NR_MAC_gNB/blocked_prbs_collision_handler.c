/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Collision detection between blocked PRBs and UE-specific, RRC-configured
 *        periodic signals (PUCCH/SRS/CSI-RS), read from the periodic allocation
 *        registry. See header for the two events handled.
 *
 * There is one handler function per (signal, event), called directly when that
 * collision is detected. To act on a collision (instead of only logging), edit
 * the relevant handler.
 *
 * Cell-wide signals (SSB, CORESET0, PRACH) are intentionally out of scope here
 * and will be handled separately.
 */

#include "blocked_prbs_collision_handler.h"
#include "periodic_alloc_registry.h"

#include <stdbool.h>
#include <string.h>

#include "common/utils/nr/nr_common.h"
#include "NR_MAC_COMMON/nr_mac.h"
#include "common/utils/LOG/log.h"
#include "common/config/config_userapi.h"
#include "common/config/config_paramdesc.h"

#define E3CONFIG_SECTION "E3Configuration"

/* ----------------------------------------------------------------------- */
/* Event A handlers: a newly applied block lands on an existing allocation. */
/* ----------------------------------------------------------------------- */

static void on_pucch_hit_by_block(rnti_t rnti, int rb_start, int rb_count, int first_overlap, int num_overlap)
{
  LOG_W(NR_MAC,
        "[PRB-BLOCK] new block hits configured PUCCH (RNTI %04x): PRB[%d..%d], %d PRB(s) blocked from PRB %d\n",
        rnti,
        rb_start,
        rb_start + rb_count - 1,
        num_overlap,
        first_overlap);
}

static void on_srs_hit_by_block(rnti_t rnti, int rb_start, int rb_count, int first_overlap, int num_overlap)
{
  LOG_W(NR_MAC,
        "[PRB-BLOCK] new block hits configured SRS (RNTI %04x): PRB[%d..%d], %d PRB(s) blocked from PRB %d\n",
        rnti,
        rb_start,
        rb_start + rb_count - 1,
        num_overlap,
        first_overlap);
}

static void on_csirs_hit_by_block(rnti_t rnti, int rb_start, int rb_count, int first_overlap, int num_overlap)
{
  LOG_W(NR_MAC,
        "[PRB-BLOCK] new block hits configured NZP-CSI-RS (RNTI %04x): PRB[%d..%d], %d PRB(s) blocked from PRB %d\n",
        rnti,
        rb_start,
        rb_start + rb_count - 1,
        num_overlap,
        first_overlap);
}

/* ----------------------------------------------------------------------- */
/* Event B handlers: a (re)configured allocation lands on an active block.  */
/* ----------------------------------------------------------------------- */

static void on_pucch_placed_on_block(rnti_t rnti, int rb_start, int rb_count, int first_overlap, int num_overlap)
{
  LOG_W(NR_MAC,
        "[PRB-BLOCK] PUCCH (RNTI %04x) configured on already-blocked PRB(s): PRB[%d..%d], %d blocked from PRB %d\n",
        rnti,
        rb_start,
        rb_start + rb_count - 1,
        num_overlap,
        first_overlap);
}

static void on_srs_placed_on_block(rnti_t rnti, int rb_start, int rb_count, int first_overlap, int num_overlap)
{
  LOG_W(NR_MAC,
        "[PRB-BLOCK] SRS (RNTI %04x) configured on already-blocked PRB(s): PRB[%d..%d], %d blocked from PRB %d\n",
        rnti,
        rb_start,
        rb_start + rb_count - 1,
        num_overlap,
        first_overlap);
}

static void on_csirs_placed_on_block(rnti_t rnti, int rb_start, int rb_count, int first_overlap, int num_overlap)
{
  LOG_W(NR_MAC,
        "[PRB-BLOCK] NZP-CSI-RS (RNTI %04x) configured on already-blocked PRB(s): PRB[%d..%d], %d blocked from PRB %d\n",
        rnti,
        rb_start,
        rb_start + rb_count - 1,
        num_overlap,
        first_overlap);
}

/* ----------------------------------------------------------------------- */
/* Helpers                                                                 */
/* ----------------------------------------------------------------------- */

/* Guard margin (PRBs) added on each side of every blocked PRB to account for
 * adjacent-PRB RF leakage. Read once from E3Configuration.collision_margin. */
static int get_collision_margin(void)
{
  static int margin = -1;
  if (margin < 0) {
    int m = 0;
    paramdef_t p[] = {
        {"collision_margin",
         "guard PRBs added on each side of blocked PRBs for collision detection",
         0,
         .iptr = &m,
         .defintval = 0,
         TYPE_INT,
         0},
    };
    config_get(config_get_if(), p, sizeof(p) / sizeof(*p), E3CONFIG_SECTION);
    margin = (m < 0) ? 0 : m;
  }
  return margin;
}

/* Build a boolean blocked map from the mask, expanded by the guard margin. */
static void build_blocked_set(const uint16_t *mask, int margin, bool blocked[MAX_BWP_SIZE])
{
  memset(blocked, 0, MAX_BWP_SIZE * sizeof(bool));
  for (int p = 0; p < MAX_BWP_SIZE; p++) {
    if (mask[p] == 0)
      continue;
    int lo = p - margin;
    int hi = p + margin;
    if (lo < 0)
      lo = 0;
    if (hi >= MAX_BWP_SIZE)
      hi = MAX_BWP_SIZE - 1;
    for (int q = lo; q <= hi; q++)
      blocked[q] = true;
  }
}

/* Number of PRBs of [rb_start, rb_start+rb_count) that fall in the blocked set;
 * *first_overlap is set to the first such PRB (or left untouched if none). */
static int compute_overlap(const bool blocked[MAX_BWP_SIZE], int rb_start, int rb_count, int *first_overlap)
{
  int first = -1;
  int count = 0;
  for (int p = rb_start; p < rb_start + rb_count; p++) {
    if (p < 0 || p >= MAX_BWP_SIZE)
      continue;
    if (blocked[p]) {
      if (first < 0)
        first = p;
      count++;
    }
  }
  if (count > 0)
    *first_overlap = first;
  return count;
}

/* ----------------------------------------------------------------------- */
/* Entry points                                                            */
/* ----------------------------------------------------------------------- */

/* Pick the blocked set matching an entry's direction (NULL if no block in that
 * direction, i.e. nothing to check). */
static const bool *blocked_set_for_dir(pa_dir_t dir, const bool *blocked_dl, bool have_dl, const bool *blocked_ul, bool have_ul)
{
  if (dir == PA_DIR_DL)
    return have_dl ? blocked_dl : NULL;
  return have_ul ? blocked_ul : NULL;
}

int blocked_prbs_check_on_policy_update(const uint16_t *blocked_dl, const uint16_t *blocked_ul)
{
  if (!blocked_dl && !blocked_ul)
    return 0;

  const int margin = get_collision_margin();
  bool blocked_dl_set[MAX_BWP_SIZE];
  bool blocked_ul_set[MAX_BWP_SIZE];
  if (blocked_dl)
    build_blocked_set(blocked_dl, margin, blocked_dl_set);
  if (blocked_ul)
    build_blocked_set(blocked_ul, margin, blocked_ul_set);

  int collisions = 0;
  size_t n_alloc = periodic_alloc_count();
  for (size_t i = 0; i < n_alloc; i++) {
    const periodic_alloc_t *e = periodic_alloc_at(i);
    if (!e)
      continue;
    const bool *blocked = blocked_set_for_dir(e->dir, blocked_dl_set, blocked_dl != NULL, blocked_ul_set, blocked_ul != NULL);
    if (!blocked)
      continue;
    int first = -1;
    int n = compute_overlap(blocked, e->rb_start, e->rb_count, &first);
    if (n == 0)
      continue;
    switch (e->type) {
      case PA_PUCCH:
        on_pucch_hit_by_block(e->rnti, e->rb_start, e->rb_count, first, n);
        break;
      case PA_SRS:
        on_srs_hit_by_block(e->rnti, e->rb_start, e->rb_count, first, n);
        break;
      case PA_CSIRS:
        on_csirs_hit_by_block(e->rnti, e->rb_start, e->rb_count, first, n);
        break;
    }
    collisions++;
  }

  if (collisions == 0)
    LOG_I(NR_MAC, "[PRB-BLOCK] applied block collides with no configured UE-specific periodic signal\n");

  return collisions;
}

int blocked_prbs_check_on_ue_config(uid_t uid, const uint16_t *blocked_dl, const uint16_t *blocked_ul)
{
  if (!blocked_dl && !blocked_ul)
    return 0;

  const int margin = get_collision_margin();
  bool blocked_dl_set[MAX_BWP_SIZE];
  bool blocked_ul_set[MAX_BWP_SIZE];
  if (blocked_dl)
    build_blocked_set(blocked_dl, margin, blocked_dl_set);
  if (blocked_ul)
    build_blocked_set(blocked_ul, margin, blocked_ul_set);

  int collisions = 0;
  size_t n_alloc = periodic_alloc_count();
  for (size_t i = 0; i < n_alloc; i++) {
    const periodic_alloc_t *e = periodic_alloc_at(i);
    if (!e || e->uid != uid)
      continue;
    const bool *blocked = blocked_set_for_dir(e->dir, blocked_dl_set, blocked_dl != NULL, blocked_ul_set, blocked_ul != NULL);
    if (!blocked)
      continue;
    int first = -1;
    int n = compute_overlap(blocked, e->rb_start, e->rb_count, &first);
    if (n == 0)
      continue;
    switch (e->type) {
      case PA_PUCCH:
        on_pucch_placed_on_block(e->rnti, e->rb_start, e->rb_count, first, n);
        break;
      case PA_SRS:
        on_srs_placed_on_block(e->rnti, e->rb_start, e->rb_count, first, n);
        break;
      case PA_CSIRS:
        on_csirs_placed_on_block(e->rnti, e->rb_start, e->rb_count, first, n);
        break;
    }
    collisions++;
  }
  return collisions; // silent when no collision (called on every UE (re)config)
}
