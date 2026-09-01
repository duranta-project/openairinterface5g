/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief PRB blocking for dApp control plane.
 *
 * Maintains DL/UL per-PRB symbol-bitmap masks populated by the control
 * plane (e.g. set_prb_block_mask()) and OR's them into the current-slot
 * VRB maps at the start of each slot, before any scheduling step runs.
 *
 * The per-PRB symbol-bitmap representation matches the vrb_map / vrb_map_UL
 * encoding, so every consumer of those maps naturally treats blocked PRBs
 * as occupied with no special-casing.
 */

#include "gNB_scheduler_prb_block.h"
#include "blocked_prbs_collision_handler.h"
#include "gNB_scheduler_ul_sensing.h"
#include "LAYER2/NR_MAC_gNB/mac_proto.h"
#include "common/utils/nr/nr_common.h"

/* Allocate this cell's PRB-block state, inactive until set_prb_block_mask().
 * calloc zeros the masks; retire_at starts at INT64_MAX ("none pending") so the
 * first apply_prb_block_masks() does not retire bits that were never released. */
void prb_block_init(nr_cell_sched_t *cell)
{
  prb_block_state_t *pb = calloc(1, sizeof(*pb));
  AssertFatal(pb != NULL, "out of memory allocating prb_block_state_t\n");
  pthread_mutex_init(&pb->lock, NULL);
  pb->ul_retire_at = INT64_MAX;
  pb->dl_retire_at = INT64_MAX;
  cell->prb_block = pb;
}

/* Release the state allocated by prb_block_init(). Idempotent. */
void prb_block_free(nr_cell_sched_t *cell)
{
  if (cell->prb_block == NULL)
    return;
  pthread_mutex_destroy(&cell->prb_block->lock);
  free(cell->prb_block);
  cell->prb_block = NULL;
}

/* Replace the DL or UL block mask (NULL/len==0 clears) under st->lock. Installs
 * the requested bits verbatim, folds released bits into prev_mask + arms the
 * retire timer, and for UL requests a one-shot full-ring stamp. After a UL
 * install, Event A scans the periodic-allocation registry for PUCCH/SRS the new
 * block now lands on (collision_handler). */
bool set_prb_block_mask(gNB_MAC_INST *mac, nr_cell_sched_t *cell, prb_block_dir_t dir, const uint16_t *mask, int len)
{
  prb_block_state_t *st = cell->prb_block;
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
  /* Install/replace/clear in one loop (clear = want==0 everywhere). Per rb: fold
   * bits dropped vs the old mask into prev_mask and clear prev bits the new mask
   * re-blocks (`prev &= ~new`), so prev_mask stays a pure "released-not-yet-current"
   * tracker (see prb_block_state docs) — else a shrink-then-re-expand would leave
   * blocked PRBs looking free. */
  for (int rb = 0; rb < MAX_BWP_SIZE; rb++) {
    uint16_t want = (mask != NULL && rb < len) ? mask[rb] : 0;
    prev[rb] = (prev[rb] | target[rb]) & ~want;
    target[rb] = want;
  }
  *active = (mask != NULL && len > 0);
  /* Arm the retire timer: prev_mask is zeroed once the ring has fully reseeded
   * (see prb_block_state docs). */
  *retire_at = st->apply_counter + cell->vrb_map_UL_size;
  /* Request a one-shot full-ring UL stamp on the next tick (closes the expand
   * race; no-op for DL, which has no ring). */
  if (dir == PRB_BLOCK_DIR_UL)
    st->needs_ul_full_stamp = true;
  pthread_mutex_unlock(&st->lock);

  /* Event A: a (new) block is now live — scan the periodic-allocation registry
   * and report which configured PUCCH/SRS (UL) or NZP-CSI-RS (DL) it lands on,
   * so they can be relocated via an RRC reconfiguration (handled in the
   * collision handler, not the per-slot scheduler — these resources are
   * RRC-fixed). An install in either direction re-checks against the current
   * effective DL and UL masks; the effective masks fold in recently-released
   * bits too. The registry has no internal locking (its contract is "callers
   * hold sched_lock"), and this path runs on the SM control thread while the
   * MAC thread rebuilds entries on UE attach/reconfig/detach — so take
   * sched_lock around the scan, after st->lock is released (the scheduler
   * takes sched_lock before st->lock, never the reverse). */
  uint16_t effective_dl[MAX_BWP_SIZE];
  uint16_t effective_ul[MAX_BWP_SIZE];
  const bool have_dl = get_effective_prb_block_mask_dl(cell, effective_dl);
  const bool have_ul = get_effective_prb_block_mask_ul(cell, effective_ul);
  if (have_dl || have_ul) {
    NR_SCHED_LOCK(&mac->sched_lock);
    blocked_prbs_check_on_policy_update(have_dl ? effective_dl : NULL, have_ul ? effective_ul : NULL);
    NR_SCHED_UNLOCK(&mac->sched_lock);
  }

  /* LOG_D: the dApp can fire one per detection callback (every few UL slots),
   * which would flood the log at default verbosity. */
  LOG_D(NR_MAC,
        "PRB block mask updated: dir=%s active=%d len=%d\n",
        dir == PRB_BLOCK_DIR_DL ? "DL" : "UL",
        (mask != NULL && len > 0),
        len);
  return true;
}

/* Copy the effective UL mask (mask_ul | prev_mask_ul) into out under st->lock;
 * returns true iff any bit is set. Not gated on active_ul, so recently-released
 * bits stay sensing-eligible across the retire window. */
bool get_effective_prb_block_mask_ul(const nr_cell_sched_t *cell, uint16_t out[MAX_BWP_SIZE])
{
  if (!cell || !cell->prb_block || !out)
    return false;
  prb_block_state_t *st = cell->prb_block;
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

/* Copy the effective DL mask (mask_dl | prev_mask_dl) into out under st->lock;
 * returns true iff any bit is set. DL counterpart of the UL getter, used to
 * check NZP-CSI-RS collisions against a DL block. */
bool get_effective_prb_block_mask_dl(const nr_cell_sched_t *cell, uint16_t out[MAX_BWP_SIZE])
{
  if (!cell || !cell->prb_block || !out)
    return false;
  prb_block_state_t *st = cell->prb_block;
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

/* OR the active UL block mask into one vrb_map_UL row (MAX_BWP_SIZE entries) under
 * the lock; no-op when no UL block is active. Re-applies the block after a
 * sensing-reserved row was reset to ulprbbl. */
void prb_block_reapply_ul_row(const nr_cell_sched_t *cell, uint16_t *row)
{
  if (!cell || !cell->prb_block || !row)
    return;
  prb_block_state_t *st = cell->prb_block;
  pthread_mutex_lock(&st->lock);
  if (st->active_ul)
    for (int rb = 0; rb < MAX_BWP_SIZE; rb++)
      row[rb] |= st->mask_ul[rb];
  pthread_mutex_unlock(&st->lock);
}

/* Per-PRB PUCCH occupancy for the dedicated-PUCCH occasion search. Passthrough:
 * PUCCH is not steered around the dApp block per slot. Collisions of periodic
 * UE-specific signals (PUCCH/SRS) with a block are detected at install +
 * RRC-reconfig events (blocked_prbs_collision_handler), not in this per-slot
 * scheduler. */
uint16_t prb_block_pucch_effective_ul(const nr_cell_sched_t *cell, uint16_t slice_bits, int rb)
{
  (void)cell;
  (void)rb;
  return slice_bits;
}

/* True iff every bit of a UL vrb_map collision (alloc) is attributable to the
 * dApp block, i.e. set in mask_ul[rb] | prev_mask_ul[rb] (read under st->lock).
 * Cold path; returns false on the alloc==0 contract violation. */
bool vrb_map_UL_conflict_is_dapp_block_only(const nr_cell_sched_t *cell, int rb, uint16_t alloc)
{
  if (!cell || !cell->prb_block || rb < 0 || rb >= MAX_BWP_SIZE)
    return false;
  /* Caller should have filtered alloc==0; return false defensively (sends it
   * down its AssertFatal path). */
  if (alloc == 0)
    return false;

  /* Cold path (conflict branch only). Union mask_ul | prev_mask_ul under the lock
   * so stale ring bits are attributed too (see prb_block_state docs). */
  prb_block_state_t *st = cell->prb_block;
  bool result;
  pthread_mutex_lock(&st->lock);
  const uint16_t effective = st->mask_ul[rb] | st->prev_mask_ul[rb];
  result = ((alloc & ~effective) == 0);
  pthread_mutex_unlock(&st->lock);
  return result;
}

/* Reserve `mask` over [rb_start, rb_start+nb_rb) of vrb_map_UL for a cell UL
 * channel (PRACH/Msg3 reception), handling dApp-block collisions uniformly: a
 * free RB is OR'd; an RB occupied only by the dApp's UL block is left as-is
 * (apply_prb_block_masks already stamped it) and counted for one rate-limited
 * summary; any other occupancy asserts unless the slot is sensing-reserved.
 * Folds the graceful-skip loop shared by fill_vrb / nr_add_msg3 so those
 * scheduler sites stay one call. */
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
    /* Not free and not a dApp block: only a sensing-reserved slot may
     * legitimately show these RBs occupied. */
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

/* Emit one rate-limited LOG_W summarizing dApp-block collisions at a call site
 * (first/last/count). No-op when count==0; a thread-local, pointer-keyed
 * per-site table prints the first 5 then every 1000th. */
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
  /* Per-site rate-limit (first 5, then every 1000th): a per-site table, not one
   * shared counter, so interleaved PRACH/SRS/Msg3 don't reset each other. Sites
   * are string literals, so pointer-equality identifies them. */
  enum { N_SITE_SLOTS = 8 };
  static _Thread_local struct {
    const char *site;
    uint64_t count;
  } site_counters[N_SITE_SLOTS] = {{0}};
  /* Find this site's slot (or allocate a fresh one if first sighting). */
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
  /* first/last bound the colliding range; collisions may be sparse within it
   * (count <= last-first+1). UE clause appended only when rnti != 0. */
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

/* Collapse the set of PRBs with any blocked symbol in m[] into a compact
 * range string ("0-15,62,80-105") in buf, appending "..." if it overflows.
 * Returns the total blocked-PRB count (accurate even if the string was
 * truncated). */
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

/* Periodically log the active blocked-PRB set (UL + DL) for monitoring: once per
 * 128 frames at slot 0 (~1.28 s at mu=1), alongside the MAC stats. No-op when
 * nothing is blocked; the cadence gate is lock-free, the lock taken only to dump. */
static void prb_block_log_active(const nr_cell_sched_t *cell, frame_t frame, slot_t slot)
{
  if (slot != 0 || (frame & 127) != 0)
    return;
  prb_block_state_t *st = cell->prb_block;
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

/* Per-slot hook: under st->lock, advance apply_counter (retiring prev_mask once
 * its ring-cycle window elapses) and OR mask_dl/mask_ul into the current-/
 * prev-slot vrb_map slices so the scheduler sees blocked PRBs as occupied; a
 * pending one-shot stamp writes mask_ul into all UL ring slices. Locks
 * unconditionally (counter must tick) but skips the OR when nothing is active. */
void apply_prb_block_masks(nr_cell_sched_t *cell, frame_t frame, slot_t slot)
{
  prb_block_state_t *st = cell->prb_block;
  if (st == NULL)
    return;

  prb_block_log_active(cell, frame, slot); /* rate-limited monitor dump */

  const int slots_per_frame = cell->frame_structure.numb_slots_frame;
  int num_beams = (cell->beam_info.beam_mode != NO_BEAM_MODE) ? cell->beam_info.beams_per_period : 1;

  /* Lock unconditionally: apply_counter must tick even when idle, to retire
   * prev_mask after one ring cycle. One uncontended acquire/tick (~50 ns). */
  pthread_mutex_lock(&st->lock);

  /* Retire released bits whose ring slices have all been reseeded. */
  st->apply_counter++;
  if (st->apply_counter >= st->ul_retire_at) {
    memset(st->prev_mask_ul, 0, MAX_BWP_SIZE * sizeof(uint16_t));
    st->ul_retire_at = INT64_MAX;
  }
  if (st->apply_counter >= st->dl_retire_at) {
    memset(st->prev_mask_dl, 0, MAX_BWP_SIZE * sizeof(uint16_t));
    st->dl_retire_at = INT64_MAX;
  }

  /* Detect-only: always stamp the active mask (the SCHED_UL/DL exempt gate is
   * in the deferred policy subsystem). */
  const bool dl_on = st->active_dl;
  const bool ul_on = st->active_ul;
  /* Snapshot+clear the one-shot full-ring UL stamp flag; honored even on a clear
   * (harmless no-op then) so the flag is always consumed. */
  const bool do_ul_full_stamp = st->needs_ul_full_stamp;
  st->needs_ul_full_stamp = false;

  if (!dl_on && !ul_on && !do_ul_full_stamp) {
    pthread_mutex_unlock(&st->lock);
    return;
  }

  {
    NR_COMMON_channels_t *cc = &cell->common_channels;

    /* DL: current slot's vrb_map. Same slice cleared in gNB_dlsch_ulsch_scheduler. */
    if (dl_on) {
      for (int b = 0; b < num_beams; b++) {
        uint16_t *vrb = cc->vrb_map[b];
        for (int rb = 0; rb < MAX_BWP_SIZE; rb++)
          vrb[rb] |= st->mask_dl[rb];
      }
    }

    /* UL: prev_slot's vrb_map slice (matches the seed location in
     * gNB_dlsch_ulsch_scheduler). On a one-shot full-ring stamp, write mask_ul
     * into EVERY slice instead, closing the expand race for lookahead slots. */
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
}
