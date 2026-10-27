/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "ran_func_spectrum_prb_block.h"
#include "ran_func_spectrum_collision.h"
#include "ran_func_spectrum_registry.h"
#include "ran_func_spectrum_extern.h"
#include "LAYER2/NR_MAC_gNB/mac_proto.h"
#include "common/utils/nr/nr_common.h"

/* Per-cell state; a cell without an entry counts as "nothing blocked". */
static struct {
  const nr_cell_sched_t *cell;
  prb_block_state_t *state;
} g_block[NR_MAX_CELLS];

static prb_block_state_t *block_state(const nr_cell_sched_t *cell)
{
  for (int i = 0; i < NR_MAX_CELLS; i++)
    if (g_block[i].state != NULL && g_block[i].cell == cell)
      return g_block[i].state;
  return NULL;
}

void prb_block_attach(const nr_cell_sched_t *cell)
{
  if (block_state(cell) != NULL)
    return;
  int free_idx = 0;
  while (free_idx < NR_MAX_CELLS && g_block[free_idx].state != NULL)
    free_idx++;
  AssertFatal(free_idx < NR_MAX_CELLS, "no room to attach PRB blocking to another cell\n");
  prb_block_state_t *pb = calloc(1, sizeof(*pb));
  AssertFatal(pb != NULL, "out of memory allocating prb_block_state_t\n");
  pthread_mutex_init(&pb->lock, NULL);
  pb->ul_retire_at = INT64_MAX;
  pb->dl_retire_at = INT64_MAX;
  g_block[free_idx].cell = cell;
  g_block[free_idx].state = pb;
}

void prb_block_detach(const nr_cell_sched_t *cell)
{
  for (int i = 0; i < NR_MAX_CELLS; i++) {
    if (g_block[i].state != NULL && g_block[i].cell == cell) {
      pthread_mutex_destroy(&g_block[i].state->lock);
      free(g_block[i].state);
      g_block[i].state = NULL;
      g_block[i].cell = NULL;
    }
  }
}

/* Set by the control handler thread and consumed by the next install on that thread. */
static _Thread_local uint32_t g_pending_procedure;

void prb_block_set_pending_procedure(uint32_t sequence_id)
{
  g_pending_procedure = sequence_id;
}

/* Weak no-op defaults for a gNB built without the E3 agent. */
__attribute__((weak)) void prb_block_on_air_hook(uint32_t sequence_id, uint16_t sfn, uint16_t slot)
{
  (void)sequence_id;
  (void)sfn;
  (void)slot;
}

__attribute__((weak)) void prb_block_superseded_hook(uint32_t sequence_id)
{
  (void)sequence_id;
}

bool set_prb_block_mask(gNB_MAC_INST *mac, nr_cell_sched_t *cell, prb_block_dir_t dir, const uint16_t *mask, int len)
{
  prb_block_state_t *st = block_state(cell);
  if (st == NULL) {
    LOG_W(NR_MAC, "set_prb_block_mask: prb_block state not allocated\n");
    return false;
  }
  if (len > MAX_BWP_SIZE) {
    LOG_W(NR_MAC, "set_prb_block_mask: len=%d clamped to MAX_BWP_SIZE=%d\n", len, MAX_BWP_SIZE);
    len = MAX_BWP_SIZE;
  }

  uint16_t *target;
  uint16_t *prev;
  bool *active;
  int64_t *retire_at;
  if (dir == PRB_BLOCK_DIR_DL) {
    target = st->mask_dl;
    prev = st->prev_mask_dl;
    active = &st->active_dl;
    retire_at = &st->dl_retire_at;
  } else {
    target = st->mask_ul;
    prev = st->prev_mask_ul;
    active = &st->active_ul;
    retire_at = &st->ul_retire_at;
  }

  pthread_mutex_lock(&st->lock);
  /* prev must drop bits the new mask re-blocks, else shrink-then-expand leaves blocked PRBs looking free */
  for (int rb = 0; rb < MAX_BWP_SIZE; rb++) {
    uint16_t want = (mask != NULL && rb < len) ? mask[rb] : 0;
    prev[rb] = (prev[rb] | target[rb]) & ~want;
    target[rb] = want;
  }
  *active = (mask != NULL && len > 0);
  /* prev_mask is zeroed once the UL ring has fully reseeded */
  *retire_at = st->apply_counter + cell->vrb_map_UL_size;
  /* One-shot full-ring UL stamp on the next tick, closes the expand race (DL has no ring) */
  uint32_t superseded_procedure = 0;
  if (dir == PRB_BLOCK_DIR_UL) {
    st->needs_ul_full_stamp = true;
    /* Masks replace, so of several installs between two ticks the last one's id is acknowledged */
    if (st->pending_sequence_id != g_pending_procedure)
      superseded_procedure = st->pending_sequence_id;
    st->pending_sequence_id = g_pending_procedure;
  }
  /* Consumed either way, or the next dApp-decided block would be misattributed */
  g_pending_procedure = 0;
  pthread_mutex_unlock(&st->lock);

  /* Outside the lock; the same id re-installing is not a supersede */
  if (superseded_procedure != 0)
    prb_block_superseded_hook(superseded_procedure);

  /* Runs on the SM control thread, so the registry scan needs sched_lock. Take it
   * after releasing st->lock: the scheduler takes sched_lock first. */
  uint16_t effective_dl[MAX_BWP_SIZE];
  uint16_t effective_ul[MAX_BWP_SIZE];
  const bool have_dl = get_effective_prb_block_mask_dl(cell, effective_dl);
  const bool have_ul = get_effective_prb_block_mask_ul(cell, effective_ul);
  if (have_dl || have_ul) {
    NR_SCHED_LOCK(&mac->sched_lock);
    blocked_prbs_check_on_policy_update(have_dl ? effective_dl : NULL, have_ul ? effective_ul : NULL);
    NR_SCHED_UNLOCK(&mac->sched_lock);
  }

  /* LOG_D: can fire every few UL slots */
  LOG_D(NR_MAC,
        "PRB block mask updated: dir=%s active=%d len=%d\n",
        dir == PRB_BLOCK_DIR_DL ? "DL" : "UL",
        (mask != NULL && len > 0),
        len);
  return true;
}

/* Not gated on active_ul: recently-released bits stay set across the retire window. */
bool get_effective_prb_block_mask_ul(const nr_cell_sched_t *cell, uint16_t out[MAX_BWP_SIZE])
{
  if (!cell || !block_state(cell) || !out)
    return false;
  prb_block_state_t *st = block_state(cell);
  bool any = false;
  pthread_mutex_lock(&st->lock);
  for (int rb = 0; rb < MAX_BWP_SIZE; rb++) {
    uint16_t eff = st->mask_ul[rb] | st->prev_mask_ul[rb];
    out[rb] = eff;
    any |= (eff != 0);
  }
  pthread_mutex_unlock(&st->lock);
  return any;
}

bool get_effective_prb_block_mask_dl(const nr_cell_sched_t *cell, uint16_t out[MAX_BWP_SIZE])
{
  if (!cell || !block_state(cell) || !out)
    return false;
  prb_block_state_t *st = block_state(cell);
  bool any = false;
  pthread_mutex_lock(&st->lock);
  for (int rb = 0; rb < MAX_BWP_SIZE; rb++) {
    uint16_t eff = st->mask_dl[rb] | st->prev_mask_dl[rb];
    out[rb] = eff;
    any |= (eff != 0);
  }
  pthread_mutex_unlock(&st->lock);
  return any;
}

void prb_block_reapply_ul_row(const nr_cell_sched_t *cell, uint16_t *row)
{
  if (!cell || !block_state(cell) || !row)
    return;
  prb_block_state_t *st = block_state(cell);
  pthread_mutex_lock(&st->lock);
  if (st->active_ul)
    for (int rb = 0; rb < MAX_BWP_SIZE; rb++)
      row[rb] |= st->mask_ul[rb];
  pthread_mutex_unlock(&st->lock);
}

bool vrb_map_UL_conflict_is_dapp_block_only(const nr_cell_sched_t *cell, int rb, uint16_t alloc)
{
  if (!cell || !block_state(cell) || rb < 0 || rb >= MAX_BWP_SIZE)
    return false;
  /* alloc == 0 is a caller bug: return false so it reaches its AssertFatal */
  if (alloc == 0)
    return false;

  /* prev_mask included so stale ring bits are attributed too */
  prb_block_state_t *st = block_state(cell);
  bool result;
  pthread_mutex_lock(&st->lock);
  const uint16_t effective = st->mask_ul[rb] | st->prev_mask_ul[rb];
  result = ((alloc & ~effective) == 0);
  pthread_mutex_unlock(&st->lock);
  return result;
}

/* Reserve a cell UL channel (PRACH/Msg3). RBs held only by the dApp block are skipped
 * with a rate-limited log; other occupancy asserts unless the slot is sensing-reserved. */
void prb_block_reserve_ul_channel(const nr_cell_sched_t *cell,
                                  uint16_t *vrb_map_UL,
                                  int rb_start,
                                  int nb_rb,
                                  uint16_t mask,
                                  frame_t frame,
                                  slot_t slot,
                                  rnti_t rnti,
                                  const char *channel_label)
{
  int collision_first = -1, collision_last = -1, collision_count = 0;
  for (int i = 0; i < nb_rb; ++i) {
    const int rb = rb_start + i;
    const uint16_t occupied = vrb_map_UL[rb] & mask;
    if (occupied == 0) {
      vrb_map_UL[rb] |= mask;
      continue;
    }
    if (vrb_map_UL_conflict_is_dapp_block_only(cell, rb, occupied)) {
      if (collision_first < 0)
        collision_first = rb;
      collision_last = rb;
      ++collision_count;
      continue;
    }
    AssertFatal(nr_mac_ul_slot_is_sensing_reserved(cell, slot),
                "%s: RB %d not free (occupied 0x%x mask 0x%x) at %d.%d\n",
                channel_label,
                rb,
                (unsigned)occupied,
                (unsigned)mask,
                frame,
                slot);
  }
  log_prb_block_collision_summary(channel_label, collision_first, collision_last, collision_count, mask, frame, slot, rnti);
}

void log_prb_block_collision_summary(const char *site,
                                     int first_rb,
                                     int last_rb,
                                     int count,
                                     uint16_t mask,
                                     frame_t frame,
                                     slot_t slot,
                                     rnti_t rnti)
{
  if (count == 0)
    return;
  /* First 5, then every 1000th, per site; sites are string literals so pointer equality identifies them */
  enum { N_SITE_SLOTS = 8 };
  static _Thread_local struct {
    const char *site;
    uint64_t count;
  } site_counters[N_SITE_SLOTS] = {{0}};
  int slot_i = -1;
  int free_i = -1;
  for (int i = 0; i < N_SITE_SLOTS; i++) {
    if (site_counters[i].site == site) {
      slot_i = i;
      break;
    }
    if (free_i < 0 && site_counters[i].site == NULL)
      free_i = i;
  }
  if (slot_i < 0) {
    if (free_i < 0)
      free_i = 0; /* table full -- overwrite slot 0; cosmetic only */
    site_counters[free_i].site = site;
    site_counters[free_i].count = 0;
    slot_i = free_i;
  }
  const uint64_t same_site_count = ++site_counters[slot_i].count;
  if (same_site_count > 5 && (same_site_count % 1000) != 0)
    return;
  /* first/last bound the range; collisions may be sparse within it */
  char ue[32] = "";
  if (rnti != 0)
    snprintf(ue, sizeof(ue), " -- UE %04x may fail", rnti);
  LOG_W(NR_MAC,
        "%s %4d.%2d: %d PRB(s) collide with dApp UL prb_block "
        "(first=%d last=%d mask=0x%04x)%s [#%llu]\n",
        site,
        frame,
        slot,
        count,
        first_rb,
        last_rb,
        mask,
        ue,
        (unsigned long long)same_site_count);
}

/* Formats blocked PRBs as "0-15,62,80-105", with "..." on overflow; the returned
 * count stays exact. */
static int prb_block_format_ranges(const uint16_t *m, char *buf, size_t buflen)
{
  int count = 0;
  size_t pos = 0;
  int run_start = -1;
  bool truncated = false;
  if (buflen)
    buf[0] = '\0';
  for (int rb = 0; rb <= MAX_BWP_SIZE; rb++) {
    const bool blocked = (rb < MAX_BWP_SIZE) && (m[rb] != 0);
    if (blocked) {
      count++;
      if (run_start < 0)
        run_start = rb;
      continue;
    }
    if (run_start >= 0) {
      const int run_end = rb - 1;
      if (!truncated) {
        char tok[24];
        int n = (run_start == run_end) ? snprintf(tok, sizeof(tok), "%s%d", pos ? "," : "", run_start)
                                       : snprintf(tok, sizeof(tok), "%s%d-%d", pos ? "," : "", run_start, run_end);
        if (n > 0 && pos + (size_t)n + 4 < buflen) {
          memcpy(buf + pos, tok, (size_t)n + 1);
          pos += (size_t)n;
        } else {
          snprintf(buf + pos, buflen - pos, "%s...", pos ? "," : "");
          truncated = true;
        }
      }
      run_start = -1;
    }
  }
  return count;
}

/* Once per 128 frames (1.28 s), alongside the MAC stats. */
static void prb_block_log_active(const nr_cell_sched_t *cell, frame_t frame, slot_t slot)
{
  if (slot != 0 || (frame & 127) != 0)
    return;
  prb_block_state_t *st = block_state(cell);
  char ul_buf[1024], dl_buf[1024];
  pthread_mutex_lock(&st->lock);
  const bool any = st->active_ul || st->active_dl;
  const int ul_n = prb_block_format_ranges(st->mask_ul, ul_buf, sizeof(ul_buf));
  const int dl_n = prb_block_format_ranges(st->mask_dl, dl_buf, sizeof(dl_buf));
  pthread_mutex_unlock(&st->lock);
  if (!any && ul_n == 0 && dl_n == 0)
    return;
  LOG_I(NR_MAC, "[PRB-BLOCK] active blocked PRBs: UL(%d)=[%s] DL(%d)=[%s]\n", ul_n, ul_buf, dl_n, dl_buf);
}

/* Per-slot hook, before any allocator runs. */
void apply_prb_block_masks(nr_cell_sched_t *cell, frame_t frame, slot_t slot)
{
  prb_block_state_t *st = block_state(cell);
  if (st == NULL)
    return;

  prb_block_log_active(cell, frame, slot);

  const int slots_per_frame = cell->frame_structure.numb_slots_frame;
  int num_beams = (cell->beam_info.bf_method != BF_METHOD_STRAIGHT_WIRE) ? cell->beam_info.beams_per_period : 1;

  /* Locked even when idle: apply_counter must tick to retire prev_mask */
  pthread_mutex_lock(&st->lock);

  st->apply_counter++;
  if (st->apply_counter >= st->ul_retire_at) {
    memset(st->prev_mask_ul, 0, MAX_BWP_SIZE * sizeof(uint16_t));
    st->ul_retire_at = INT64_MAX;
  }
  if (st->apply_counter >= st->dl_retire_at) {
    memset(st->prev_mask_dl, 0, MAX_BWP_SIZE * sizeof(uint16_t));
    st->dl_retire_at = INT64_MAX;
  }

  const bool dl_on = st->active_dl;
  const bool ul_on = st->active_ul;
  /* Consumed even on a clear (harmless no-op then) */
  const bool do_ul_full_stamp = st->needs_ul_full_stamp;
  st->needs_ul_full_stamp = false;
  const uint32_t stamp_sequence_id = st->pending_sequence_id;
  st->pending_sequence_id = 0;

  if (!dl_on && !ul_on && !do_ul_full_stamp) {
    pthread_mutex_unlock(&st->lock);
    return;
  }

  {
    NR_COMMON_channels_t *cc = &cell->common_channels;

    /* DL: current slot's vrb_map */
    if (dl_on) {
      for (int b = 0; b < num_beams; b++) {
        uint16_t *vrb = cc->vrb_map[b];
        for (int rb = 0; rb < MAX_BWP_SIZE; rb++)
          vrb[rb] |= st->mask_dl[rb];
      }
    }

    /* UL: prev_slot's slice, where gNB_dlsch_ulsch_scheduler seeds; a full stamp covers every slice */
    if (ul_on) {
      const int size = cell->vrb_map_UL_size;
      const int prev_slot = frame * slots_per_frame + slot + size - 1;
      for (int b = 0; b < num_beams; b++) {
        uint16_t *vrb_ul_base = cc->vrb_map_UL[b];
        if (do_ul_full_stamp) {
          for (int s = 0; s < size; s++) {
            uint16_t *vrb_ul = &vrb_ul_base[s * MAX_BWP_SIZE];
            for (int rb = 0; rb < MAX_BWP_SIZE; rb++)
              vrb_ul[rb] |= st->mask_ul[rb];
          }
        } else {
          uint16_t *vrb_ul = &vrb_ul_base[prev_slot % size * MAX_BWP_SIZE];
          for (int rb = 0; rb < MAX_BWP_SIZE; rb++)
            vrb_ul[rb] |= st->mask_ul[rb];
        }
      }
    }
  }
  pthread_mutex_unlock(&st->lock);

  /* Runs on the scheduler thread within the slot deadline: the hook must not block */
  if (do_ul_full_stamp && stamp_sequence_id != 0)
    prb_block_on_air_hook(stamp_sequence_id, (uint16_t)frame, (uint16_t)slot);
}

/* Covers attach, reconfiguration and BWP switch. */
void e3_spectrum_on_ue_configured(const nr_cell_sched_t *cell, NR_UE_info_t *UE)
{
  periodic_alloc_refresh_ue(UE);
  uint16_t effective_dl[MAX_BWP_SIZE];
  uint16_t effective_ul[MAX_BWP_SIZE];
  const bool have_dl = get_effective_prb_block_mask_dl(cell, effective_dl);
  const bool have_ul = get_effective_prb_block_mask_ul(cell, effective_ul);
  if (have_dl || have_ul)
    blocked_prbs_check_on_ue_config(UE->uid, have_dl ? effective_dl : NULL, have_ul ? effective_ul : NULL);
}

void e3_spectrum_on_ue_deleted(const NR_UE_info_t *UE)
{
  periodic_alloc_unregister_ue(UE->uid);
}
