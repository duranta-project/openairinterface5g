/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#define _GNU_SOURCE
#include "split7_engine.h"
#include "split7_engine_internal.h"
#include "split7_ofdm.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "PHY/TOOLS/tools_defs.h"
#include "common/utils/LOG/log.h"
#include "common/utils/system.h"

static inline int16_t sat16(int32_t v)
{
  return v > INT16_MAX ? INT16_MAX : (v < INT16_MIN ? INT16_MIN : v);
}

static void *engine_malloc(size_t bytes)
{
  void *p = malloc16_clear(bytes);
  AssertFatal(p, "OOM (%zu bytes)\n", bytes);
  return p;
}

static split7_timebase_t timebase_snapshot(split7_engine_t *e)
{
  pthread_mutex_lock(&e->tb_mutex);
  split7_timebase_t tb = e->tb;
  pthread_mutex_unlock(&e->tb_mutex);
  return tb;
}

// TX thread statistics are counted privately and published once per pass.
static void publish_tx_stats(split7_engine_t *e)
{
  const split7_engine_stats_t *t = &e->tx_stats;
  pthread_mutex_lock(&e->state_mutex);
  e->stats.tx_samples += t->tx_samples;
  e->stats.tx_idft += t->tx_idft;
  e->stats.tx_late_writes += t->tx_late_writes;
  e->stats.tx_write_errors += t->tx_write_errors;
  e->stats.burst_end_markers += t->burst_end_markers;
  e->stats.burst_end_late += t->burst_end_late;
  pthread_mutex_unlock(&e->state_mutex);
  e->tx_stats = (split7_engine_stats_t){0};
}

static bool engine_running(split7_engine_t *e)
{
  pthread_mutex_lock(&e->state_mutex);
  const bool running = e->running;
  pthread_mutex_unlock(&e->state_mutex);
  return running;
}

split7_engine_stats_t split7_engine_stats(split7_engine_t *e)
{
  pthread_mutex_lock(&e->state_mutex);
  const split7_engine_stats_t stats = e->stats;
  pthread_mutex_unlock(&e->state_mutex);
  return stats;
}

bool split7_engine_service_cancelled(split7_engine_t *e)
{
  pthread_mutex_lock(&e->state_mutex);
  const bool cancelled = e->service_cancel;
  pthread_mutex_unlock(&e->state_mutex);
  return cancelled;
}

int64_t split7_engine_rx_time(split7_engine_t *e)
{
  return split7_progress_get(&e->rx_time);
}

bool split7_engine_rx_copy(split7_engine_t *e, int ant, int64_t ts, int num_samples, c16_t *dst)
{
  const int64_t now = split7_engine_rx_time(e);
  if (ts < 0 || ts + num_samples > now || now - ts > (int64_t)e->rx_size)
    return false;
  const uint64_t idx = ts % e->rx_size;
  const uint64_t first = e->rx_size - idx < (uint64_t)num_samples ? e->rx_size - idx : (uint64_t)num_samples;
  memcpy(dst, e->rx_td[ant] + idx, first * sizeof(c16_t));
  memcpy(dst + first, e->rx_td[ant], (num_samples - first) * sizeof(c16_t));
  // The radio thread may have overwritten the start while copying, also with the read
  // it has not published yet.
  return (int64_t)split7_progress_get_after_reads(&e->rx_time) + e->rx_chunk - ts <= (int64_t)e->rx_size;
}

void split7_engine_set_anchor(split7_engine_t *e, int64_t anchor_ts, uint64_t abs_frame, const split7_cell_t *cell)
{
  const NR_DL_FRAME_PARMS *fp = &e->fp;
  split7_cell_t c = cell ? *cell : (split7_cell_t){0};
  const int64_t now = split7_engine_rx_time(e);
  pthread_mutex_lock(&e->tb_mutex);
  e->tb = (split7_timebase_t){.anchor_ts = anchor_ts, .anchor_frame = abs_frame};
  // Start with the slot being received now: nothing older is worth demodulating.
  const uint64_t sym = split7_symbol_at(&e->tb, fp, now);
  e->next_demod = sym > 0 ? (sym - 1) / fp->symbols_per_slot * fp->symbols_per_slot : 0;
  c.first_slot = e->next_demod / fp->symbols_per_slot;
  const uint64_t first_symbol = e->next_demod;
  pthread_mutex_unlock(&e->tb_mutex);
  split7_port_anchor(e->port, &c, first_symbol);
  LOG_I(PHY,
        "[split7] anchored: frame %lu at sample %ld, first slot %lu (frame %lu.%lu)\n",
        abs_frame,
        anchor_ts,
        c.first_slot,
        c.first_slot / fp->slots_per_frame,
        c.first_slot % fp->slots_per_frame);
}

/* ------------------------------------------------------------------------- */
/* RX                                                                        */
/* ------------------------------------------------------------------------- */

static void demodulate_ready_symbols(split7_engine_t *e)
{
  split7_port_mem_t *s = e->mem;
  const NR_DL_FRAME_PARMS *fp = &e->fp;
  const int sps = fp->symbols_per_slot;
  const int n_rb = fp->N_RB_DL;
  const int64_t now = split7_engine_rx_time(e);

  pthread_mutex_lock(&e->tb_mutex);
  split7_timebase_t tb = e->tb;
  uint64_t k = e->next_demod;
  pthread_mutex_unlock(&e->tb_mutex);

  for (;;) {
    if (k % sps == 0) {
      const int32_t shift = split7_port_take_timing_shift(e->port);
      if (shift) {
        // Only slots no member has completed are moved, and not the first of them, whose
        // start already bounds what the host may transmit: RX and TX then place every
        // slot with the same shift.
        pthread_mutex_lock(&e->tb_mutex);
        const uint64_t pending = e->max_done + 1;
        const uint64_t from = k / sps > pending ? k / sps : pending;
        split7_timebase_shift(&e->tb, from, shift);
        tb = e->tb;
        pthread_mutex_unlock(&e->tb_mutex);
      }
    }
    const int slot = (k / sps) % fp->slots_per_frame;
    const int symbol = k % sps;
    const int64_t t = split7_symbol_ts(&tb, fp, k);
    const int len = split7_symbol_samples(fp, slot, symbol);
    if (t + len > now)
      break;
    c16_t *dst_base = (c16_t *)((uint8_t *)s + s->rx_off);
    const uint64_t ring_idx = k % s->geo.ring_symbols;
    for (int a = 0; a < e->nb_rx; a++) {
      c16_t *dst = dst_base + ((uint64_t)a * s->geo.ring_symbols + ring_idx) * fp->ofdm_symbol_size;
      if (!split7_engine_rx_copy(e, a, t, len, e->rx_sym)) {
        pthread_mutex_lock(&e->state_mutex);
        e->stats.rx_overruns++;
        pthread_mutex_unlock(&e->state_mutex);
        memset(dst, 0, fp->ofdm_symbol_size * sizeof(c16_t));
        continue;
      }
      split7_ofdm_demod(fp, e->rx_link, n_rb, slot, symbol, e->rx_sym, dst);
    }
    k++;
    if (k % sps == 0) {
      split7_progress_raise(&s->rx_published, k);
      split7_progress_add(&s->host_work, 1); // the TX thread follows the radio
    }
  }

  pthread_mutex_lock(&e->tb_mutex);
  e->next_demod = k;
  pthread_mutex_unlock(&e->tb_mutex);
}

static void *anchor_service_thread(void *arg)
{
  split7_engine_t *e = arg;
  const int rc = e->service(e, e->service_ctx, e->service_pci, e->service_timeout_ms);
  split7_port_answer_sync(e->port, e->service_req, rc);
  pthread_mutex_lock(&e->state_mutex);
  e->service_busy = false;
  pthread_mutex_unlock(&e->state_mutex);
  return NULL;
}

static void serve_sync_requests(split7_engine_t *e)
{
  pthread_mutex_lock(&e->state_mutex);
  const bool busy = e->service_busy;
  pthread_mutex_unlock(&e->state_mutex);
  if (busy)
    return;
  const bool anchored = split7_port_anchored(e->port);
  if (!anchored && !e->service)
    return; // the host anchors by itself
  uint32_t req = e->served_req;
  int pci;
  uint32_t timeout_ms;
  if (!split7_port_take_sync_request(e->port, &req, &pci, &timeout_ms))
    return;
  e->served_req = req;
  if (anchored) {
    split7_port_answer_sync(e->port, req, 0);
    return;
  }
  if (e->service_thread_valid)
    pthread_join(e->service_thread, NULL);
  e->service_req = req;
  e->service_pci = pci;
  e->service_timeout_ms = timeout_ms;
  pthread_mutex_lock(&e->state_mutex);
  e->service_busy = true;
  pthread_mutex_unlock(&e->state_mutex);
  // Own thread, not a pool worker: a search runs for hundreds of ms.
  threadCreate(&e->service_thread, anchor_service_thread, e, "split7_sync", -1, OAI_PRIORITY_RT_LOW);
  e->service_thread_valid = true;
}

static void *rx_thread(void *arg)
{
  split7_engine_t *e = arg;
  const int chunk = e->rx_chunk;
  c16_t *bufs[SPLIT7_MAX_ANT];
  void *ptrs[SPLIT7_MAX_ANT];
  for (int a = 0; a < e->nb_rx; a++)
    bufs[a] = engine_malloc(chunk * sizeof(c16_t));

  while (engine_running(e)) {
    for (int a = 0; a < e->nb_rx; a++)
      ptrs[a] = bufs[a];
    openair0_timestamp_t ts = 0;
    const int n = e->radio->trx_read_func(e->radio, &ts, ptrs, chunk, e->nb_rx);
    if (n < 0) {
      usleep(1000);
      continue;
    }
    AssertFatal(n == chunk, "[split7] radio returned %d samples, expected %d\n", n, chunk);
    const uint64_t idx = ts % e->rx_size;
    const uint64_t first = e->rx_size - idx < (uint64_t)n ? e->rx_size - idx : (uint64_t)n;
    for (int a = 0; a < e->nb_rx; a++) {
      memcpy(e->rx_td[a] + idx, bufs[a], first * sizeof(c16_t));
      memcpy(e->rx_td[a], bufs[a] + first, (n - first) * sizeof(c16_t));
    }
    struct timespec wall;
    clock_gettime(CLOCK_MONOTONIC, &wall);
    const int64_t ns = wall.tv_sec * 1000000000LL + wall.tv_nsec;
    pthread_mutex_lock(&e->clk_mutex);
    e->clk_rx = ts + n;
    e->clk_ns = ns;
    if (!e->clk_ref_ns) {
      e->clk_ref_rx = e->clk_rx;
      e->clk_ref_ns = ns;
    } else if (ns - e->clk_ref_ns > 1000000000LL) {
      // Measured, not nominal: a simulated radio may run slower than real time.
      e->clk_rate = (double)(e->clk_rx - e->clk_ref_rx) / (ns - e->clk_ref_ns);
      e->clk_ref_rx = e->clk_rx;
      e->clk_ref_ns = ns;
    }
    pthread_mutex_unlock(&e->clk_mutex);
    split7_progress_raise(&e->rx_time, ts + n);

    serve_sync_requests(e);
    if (split7_port_anchored(e->port))
      demodulate_ready_symbols(e);
  }
  for (int a = 0; a < e->nb_rx; a++)
    free(bufs[a]);
  return NULL;
}

/* ------------------------------------------------------------------------- */
/* TX                                                                        */
/* ------------------------------------------------------------------------- */

// Records [start, end) as transmitted content: sorted, touching intervals merged.
static void content_add(split7_engine_t *e, int64_t start, int64_t end)
{
  split7_burst_t *b = e->bursts;
  int i = 0;
  while (i < e->num_bursts && b[i].end < start)
    i++;
  if (i < e->num_bursts && b[i].start <= end) {
    // Overlaps or touches b[i] (and maybe the ones after it): merge.
    if (start < b[i].start)
      b[i].start = start;
    if (end > b[i].end)
      b[i].end = end;
    while (i + 1 < e->num_bursts && b[i + 1].start <= b[i].end) {
      if (b[i + 1].end > b[i].end)
        b[i].end = b[i + 1].end;
      b[i].opened |= b[i + 1].opened;
      memmove(&b[i + 1], &b[i + 2], (e->num_bursts - i - 2) * sizeof(*b));
      e->num_bursts--;
    }
    return;
  }
  AssertFatal(e->num_bursts < SPLIT7_MAX_BURSTS, "[split7] more than %d pending bursts\n", SPLIT7_MAX_BURSTS);
  memmove(&b[i + 1], &b[i], (e->num_bursts - i) * sizeof(*b));
  b[i] = (split7_burst_t){.start = start, .end = end};
  e->num_bursts++;
}

static void radio_write(split7_engine_t *e, int64_t ts, int n, int flags)
{
  void *ptrs[SPLIT7_MAX_ANT];
  const uint64_t idx = (ts - e->tx_base) % e->tx_size;
  for (int a = 0; a < e->nb_tx; a++)
    ptrs[a] = e->tx_td[a] + idx;
  if (e->radio->trx_write_func(e->radio, ts, ptrs, n, e->nb_tx, flags) != n)
    e->tx_stats.tx_write_errors++;
  e->tx_stats.tx_samples += n;
}

// Writes [start, end) of the playout ring as one piece of a burst, split at the
// ring's end if needed; is_start / is_end mark the burst's first / last samples.
static void write_piece(split7_engine_t *e, int64_t start, int64_t end, bool is_start, bool is_end)
{
  while (start < end) {
    const uint64_t idx = (start - e->tx_base) % e->tx_size;
    const int64_t run = e->tx_size - idx < (uint64_t)(end - start) ? (int64_t)(e->tx_size - idx) : end - start;
    const bool first = is_start, last = is_end && start + run == end;
    const int flags = first && last ? TX_BURST_START_AND_END : first ? TX_BURST_START : last ? TX_BURST_END : TX_BURST_MIDDLE;
    radio_write(e, start, run, flags);
    start += run;
    is_start = false;
  }
}

// Transmits the content of [t0, t1) and nothing else: silence is not written. A
// burst reaching t1 stays open, ended in the next stretch if nothing continues it.
static void playout_write(split7_engine_t *e, int64_t t0, int64_t t1, int64_t now)
{
  split7_burst_t *b = e->bursts;
  if (e->open_burst && !(e->num_bursts && b[0].start <= t0)) {
    radio_write(e, t0, 1, TX_BURST_END); // a zero: the burst ended at t0
    e->tx_stats.burst_end_markers++;
    if (t0 < now)
      e->tx_stats.burst_end_late++;
    e->open_burst = false;
  }
  bool open = false;
  int done = 0;
  for (int i = 0; i < e->num_bursts && b[i].start < t1; i++) {
    const int64_t start = b[i].start > t0 ? b[i].start : t0;
    const int64_t end = b[i].end < t1 ? b[i].end : t1;
    const bool is_start = !b[i].opened && !(e->open_burst && b[i].start <= t0);
    // Content added later starts at t1 at the earliest: a burst ending before t1 is over.
    const bool is_end = b[i].end < t1;
    write_piece(e, start, end, is_start, is_end);
    if (b[i].end <= t1) {
      done++;
      open = b[i].end == t1;
    } else {
      b[i].start = t1;
      b[i].opened = true;
    }
  }
  memmove(b, b + done, (e->num_bursts - done) * sizeof(*b));
  e->num_bursts -= done;
  e->open_burst = open;
  for (int64_t pos = t0; pos < t1;) {
    const uint64_t idx = (pos - e->tx_base) % e->tx_size;
    const int64_t run = e->tx_size - idx < (uint64_t)(t1 - pos) ? (int64_t)(e->tx_size - idx) : t1 - pos;
    for (int a = 0; a < e->nb_tx; a++)
      memset(e->tx_td[a] + idx, 0, run * sizeof(c16_t));
    pos += run;
  }
}

// Adds the part of src at or after from_ts into the playout ring.
static void playout_add(split7_engine_t *e, int ant, int64_t ts, const c16_t *src, int len, int64_t from_ts)
{
  int skip = 0;
  if (ts < from_ts) {
    skip = from_ts - ts;
    if (skip >= len)
      return;
  }
  int64_t pos = ts + skip;
  int n = len - skip;
  AssertFatal(pos + n - e->tx_next <= (int64_t)e->tx_size, "[split7] playout ring too small\n");
  content_add(e, pos, pos + n);
  src += skip;
  while (n > 0) {
    const uint64_t idx = (pos - e->tx_base) % e->tx_size;
    const int run = e->tx_size - idx < (uint64_t)n ? e->tx_size - idx : n;
    c16_t *dst = e->tx_td[ant] + idx;
    for (int i = 0; i < run; i++) {
      dst[i].r = sat16(dst[i].r + src[i].r);
      dst[i].i = sat16(dst[i].i + src[i].i);
    }
    pos += run;
    src += run;
    n -= run;
  }
}

// Estimated radio time now: the last read extrapolated at the measured rate. Used
// only to count late writes and to cap how far ahead of the radio the host writes.
static int64_t radio_now(split7_engine_t *e)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  const int64_t ns = ts.tv_sec * 1000000000LL + ts.tv_nsec;
  pthread_mutex_lock(&e->clk_mutex);
  const int64_t now = e->clk_ns ? e->clk_rx + (int64_t)((ns - e->clk_ns) * e->clk_rate) : e->clk_rx;
  pthread_mutex_unlock(&e->clk_mutex);
  return now;
}

// The member with the lowest id leads: its TA is the group's, and it steers timing.
static void update_leader(split7_engine_t *e)
{
  int32_t leader = -1;
  for (int c = 0; c < SPLIT7_MAX_CLIENTS && leader < 0; c++)
    if (e->members[c].member)
      leader = c;
  split7_port_set_timing_master(e->port, leader);
}

// Frees PRACH entries of client c for slots up to through that no remaining slot
// record of it refers to: a PRACH posted for a slot given up, skipped or never
// joined would otherwise hold its entry forever.
// Holding the client's monitor.
static void release_prach(split7_engine_t *e, int c, uint64_t through)
{
  const split7_member_t *m = &e->members[c];
  const uint32_t R = e->mem->ring_slots;
  for (int i = 0; i < SPLIT7_MAX_PRACH; i++) {
    split7_entry_hdr_t *entry = split7_port_prach_entry(e->port, c, i);
    if (!entry->tag || entry->tag - 1 > through)
      continue;
    const uint64_t slot = entry->tag - 1;
    const bool pending =
        m->member && slot >= m->first_slot && slot >= e->group.prach_slot && slot <= m->done && m->prach[slot % R] == i;
    if (!pending)
      entry->tag = 0;
  }
}

// Ends membership: a leaving client is open again, a closing one free. Its PRACH
// entries are released.
static void member_drop(split7_engine_t *e, int c)
{
  split7_client_info_t *cs = &e->mem->clients[c];
  e->members[c].member = false;
  split7_monitor_lock(&cs->monitor);
  release_prach(e, c, UINT64_MAX);
  if (cs->state == SPLIT7_CLIENT_LEAVING)
    cs->state = SPLIT7_CLIENT_OPEN;
  else if (cs->state == SPLIT7_CLIENT_CLOSING)
    cs->state = SPLIT7_CLIENT_FREE;
  split7_monitor_notify(&cs->monitor);
  split7_monitor_unlock(&cs->monitor);
  update_leader(e);
  bool any = false;
  for (int m = 0; m < SPLIT7_MAX_CLIENTS; m++)
    any |= e->members[m].member;
  e->group.active = any;
}

static bool participates(const split7_member_t *m, uint64_t slot)
{
  return m->member && slot >= m->first_slot && slot <= m->leave_slot;
}

// First slot a join asking for slot can have: for the first member, the first whose
// start (at its TA) the radio has not passed; otherwise also not one the group has
// started assembling or already sent.
static uint64_t join_slot_for(split7_engine_t *e, const split7_timebase_t *tb, uint64_t slot, int64_t join_ta)
{
  const split7_group_t *g = &e->group;
  const NR_DL_FRAME_PARMS *fp = &e->fp;
  const int sps = fp->symbols_per_slot;
  const int64_t ta = g->active ? g->ta[g->done % e->mem->ring_slots] : join_ta;
  if (g->active && slot < g->done + 1)
    slot = g->done + 1;
  while ((g->active && slot * sps < g->cursor) || split7_symbol_ts(tb, fp, slot * sps) - ta < e->tx_next)
    slot++;
  return slot;
}

// The group starts at slot with the first member's TA.
static void group_start(split7_engine_t *e, const split7_timebase_t *tb, uint64_t slot, int64_t ta)
{
  split7_group_t *g = &e->group;
  const int sps = e->fp.symbols_per_slot;
  g->active = true;
  g->waiting_for = -1;
  g->done = slot - 1;
  g->cursor = slot * sps;
  g->prach_slot = slot;
  g->reach = split7_symbol_ts(tb, &e->fp, slot * sps) - ta;
  g->ta[(slot - 1) % e->mem->ring_slots] = ta;
}

// Membership events: joins are accepted at the first requested slot the group can
// still take; leaving and closing clients are let go.
static void update_membership(split7_engine_t *e, const split7_timebase_t *tb)
{
  split7_port_mem_t *s = e->mem;
  const NR_DL_FRAME_PARMS *fp = &e->fp;
  const int sps = fp->symbols_per_slot;
  for (int c = 0; c < SPLIT7_MAX_CLIENTS; c++) {
    split7_client_info_t *cs = &s->clients[c];
    split7_member_t *m = &e->members[c];
    split7_monitor_lock(&cs->monitor);
    const split7_client_state_t state = cs->state;
    const uint64_t leave_slot = cs->leave_slot;
    const uint64_t asked = cs->join_slot;
    const int64_t join_ta = cs->join_ta;
    split7_monitor_unlock(&cs->monitor);
    if (state == SPLIT7_CLIENT_CLOSING) {
      if (m->member)
        LOG_W(PHY, "[split7] client %d closed without leaving: dropped at slot %lu\n", c, m->done);
      member_drop(e, c);
      continue;
    }
    if (m->member && state == SPLIT7_CLIENT_LEAVING && m->leave_slot == UINT64_MAX)
      m->leave_slot = leave_slot;
    if (!m->member && state == SPLIT7_CLIENT_LEAVING) {
      member_drop(e, c); // left before being accepted
      continue;
    }
    if (state != SPLIT7_CLIENT_JOINING)
      continue;
    const uint64_t slot = join_slot_for(e, tb, asked, join_ta);
    const uint64_t host_slot = e->group.active ? e->group.cursor / sps : slot;
    // Unless the client left or closed meanwhile: it cannot ask again before this
    // thread lets it go, so a client still joining asks for what was read above.
    split7_monitor_lock(&cs->monitor);
    const bool accepted = cs->state == SPLIT7_CLIENT_JOINING;
    if (accepted) {
      cs->host_slot = host_slot;
      cs->join_slot = slot;
      cs->state = SPLIT7_CLIENT_MEMBER;
      split7_monitor_notify(&cs->monitor);
    }
    split7_monitor_unlock(&cs->monitor);
    if (!accepted)
      continue; // the next pass reads the new state
    if (!e->group.active)
      group_start(e, tb, slot, join_ta);
    *m = (split7_member_t){.member = true,
                           .first_slot = slot,
                           .leave_slot = UINT64_MAX,
                           .done = slot - 1,
                           .used = m->used,
                           .prach = m->prach,
                           .ta = m->ta};
    update_leader(e);
    LOG_I(PHY,
          "[split7] client %d joined at slot %lu (frame %lu.%lu)\n",
          c,
          slot,
          slot / fp->slots_per_frame,
          slot % fp->slots_per_frame);
  }
}

// Takes every member's newly completed slots, then the slots every participating
// member has completed.
static void advance(split7_engine_t *e, const split7_timebase_t *tb)
{
  split7_port_mem_t *s = e->mem;
  split7_group_t *g = &e->group;
  const NR_DL_FRAME_PARMS *fp = &e->fp;
  const uint32_t R = s->ring_slots;
  // The per-slot arrays hold one ring ahead of what is assembled (a skip can
  // complete hundreds of slots at once: the rest is taken as the cursor moves).
  const uint64_t limit = g->cursor / fp->symbols_per_slot + R;
  uint64_t max_done = 0;
  for (int c = 0; c < SPLIT7_MAX_CLIENTS; c++) {
    split7_member_t *m = &e->members[c];
    if (!m->member)
      continue;
    split7_client_info_t *cs = &s->clients[c];
    split7_monitor_lock(&cs->monitor);
    const uint64_t skip = cs->skip_through;
    for (uint64_t slot = m->done + 1; slot < limit; slot++) {
      const uint64_t idx = slot % R;
      const split7_slot_record_t *r = split7_port_record(e->port, c, slot);
      if (r->tag == slot + 1) {
        m->used[idx] = r->used_symbols;
        m->prach[idx] = r->prach;
        m->ta[idx] = r->ta_samples;
      } else if (slot < skip) {
        m->used[idx] = 0;
        m->prach[idx] = -1;
        m->ta[idx] = -1;
      } else {
        break;
      }
      m->done = slot;
    }
    release_prach(e, c, m->done);
    split7_monitor_unlock(&cs->monitor);
    if (m->done + 1 > max_done)
      max_done = m->done + 1;
  }
  // Never backwards (a member far ahead may leave): timing shifts start after it.
  pthread_mutex_lock(&e->tb_mutex);
  if (max_done > e->max_done)
    e->max_done = max_done;
  pthread_mutex_unlock(&e->tb_mutex);

  int bottleneck = -1;
  while (g->active && g->done + 1 < limit) {
    const uint64_t slot = g->done + 1;
    int leader = -1;
    bool any_later = false;
    for (int c = 0; c < SPLIT7_MAX_CLIENTS; c++) {
      const split7_member_t *m = &e->members[c];
      if (m->member && slot < m->first_slot)
        any_later = true;
      if (!participates(m, slot))
        continue;
      if (m->done < slot) {
        if (bottleneck < 0 || m->done < e->members[bottleneck].done)
          bottleneck = c;
      } else if (leader < 0 && m->ta[slot % R] >= 0) {
        leader = c; // the lowest id that actually sends the slot
      }
    }
    if (bottleneck >= 0) {
      g->waiting_for = bottleneck;
      break;
    }
    bool any = false;
    for (int c = 0; c < SPLIT7_MAX_CLIENTS; c++)
      any |= participates(&e->members[c], slot);
    if (!any && !any_later)
      break; // every member has left: nothing more to assemble
    const uint64_t idx = slot % R;
    g->ta[idx] = leader >= 0 ? e->members[leader].ta[idx] : g->ta[(slot - 1) % R];
    g->holder[idx] = g->waiting_for;
    g->waiting_for = -1;
    // A TA increase makes the slot start inside the previous one: it is shortened by
    // dropping its first samples (CP first, TS 38.213 4.2). A decrease leaves a gap of
    // zeros.
    g->clip[idx] = g->reach;
    const int64_t end = split7_symbol_ts(tb, fp, (slot + 1) * fp->symbols_per_slot) - g->ta[idx];
    if (end > g->reach)
      g->reach = end;
    g->done = slot;
  }
}

// Assembles the group's completed slots up to the first symbol starting at or after
// t1: PRACH windows as they are, symbols summed over the members in frequency
// domain and modulated once.
static void assemble(split7_engine_t *e, const split7_timebase_t *tb, int64_t t0, int64_t t1)
{
  split7_group_t *g = &e->group;
  const NR_DL_FRAME_PARMS *fp = &e->fp;
  const int sps = fp->symbols_per_slot;
  const uint32_t R = e->mem->ring_slots;
  const int n_re = fp->N_RB_UL * NR_NB_SC_PER_RB;
  for (; g->active; g->cursor++) {
    const uint64_t k = g->cursor;
    const uint64_t slot = k / sps;
    if (slot > g->done)
      break;
    const uint64_t idx = slot % R;
    const int64_t ta = g->ta[idx];
    const int64_t clip = g->clip[idx];
    const int64_t from = clip > t0 ? clip : t0;
    if (k % sps == 0 && g->prach_slot == slot) {
      const int64_t slot_start = split7_symbol_ts(tb, fp, k) - ta;
      if ((slot_start > clip ? slot_start : clip) >= t1)
        break;
      for (int c = 0; c < SPLIT7_MAX_CLIENTS; c++) {
        const split7_member_t *m = &e->members[c];
        if (!participates(m, slot) || m->prach[idx] < 0)
          continue;
        // The client fills an entry only while its tag is 0, the host frees it.
        split7_monitor_t *mon = &e->mem->clients[c].monitor;
        split7_entry_hdr_t *entry = split7_port_prach_entry(e->port, c, m->prach[idx]);
        split7_monitor_lock(mon);
        const bool posted = entry->tag == slot + 1;
        split7_monitor_unlock(mon);
        if (!posted)
          continue;
        const int total = entry->ncp + entry->reps * entry->dftlen;
        split7_ofdm_window_mod((const c16_t *)(entry + 1), entry->dftlen, entry->ncp, entry->reps, e->prach_scratch, e->prach_buf);
        playout_add(e, 0, slot_start + entry->time_offset, e->prach_buf, total, from);
        split7_monitor_lock(mon);
        entry->tag = 0;
        split7_monitor_unlock(mon);
      }
      g->prach_slot = slot + 1;
    }
    const int64_t start = split7_symbol_ts(tb, fp, k) - ta;
    if ((start > clip ? start : clip) >= t1)
      break;

    int contributors = 0;
    for (int c = 0; c < SPLIT7_MAX_CLIENTS; c++) {
      const split7_member_t *m = &e->members[c];
      if (!participates(m, slot) || !(m->used[idx] & (1u << (k % sps))))
        continue;
      // Written before the slot record, which advance() took under the client's monitor.
      split7_entry_hdr_t *entry = split7_port_tx_entry(e->port, c, k);
      if (entry->tag != k + 1) {
        LOG_E(PHY, "[split7] client %d: slot record lists symbol %lu, but it was not posted\n", c, k);
        continue;
      }
      for (int a = 0; a < e->nb_tx; a++) {
        const c16_t *in = split7_entry_tx_data(e->port, entry, a);
        if (!contributors) {
          memcpy(e->fd_sum[a], in, n_re * sizeof(c16_t));
        } else {
          for (int r = 0; r < n_re; r++)
            e->fd_sum[a][r] = (c16_t){.r = sat16(e->fd_sum[a][r].r + in[r].r), .i = sat16(e->fd_sum[a][r].i + in[r].i)};
        }
      }
      if (from > start) {
        split7_client_info_t *cs = &e->mem->clients[c];
        split7_monitor_lock(&cs->monitor);
        cs->stats.ta_clipped += from - start;
        split7_monitor_unlock(&cs->monitor);
      }
      contributors++;
    }
    if (!contributors)
      continue;
    const int len = split7_symbol_samples(fp, slot % fp->slots_per_frame, k % sps);
    for (int a = 0; a < e->nb_tx; a++) {
      split7_ofdm_mod(fp, e->tx_link, fp->N_RB_UL, slot % fp->slots_per_frame, k % sps, e->fd_sum[a], e->mod_scratch, e->mod_buf);
      playout_add(e, a, start, e->mod_buf, len, from);
    }
    e->tx_stats.tx_idft += e->nb_tx;
  }
}

// Tells members how far the host got (backpressure) and releases leaving members
// whose last slot it passed.
static void publish_progress(split7_engine_t *e)
{
  const uint64_t slot = e->group.cursor / e->fp.symbols_per_slot;
  for (int c = 0; c < SPLIT7_MAX_CLIENTS; c++) {
    split7_member_t *m = &e->members[c];
    if (!m->member)
      continue;
    split7_client_info_t *cs = &e->mem->clients[c];
    split7_monitor_lock(&cs->monitor);
    if (cs->host_slot != slot) {
      cs->host_slot = slot;
      split7_monitor_notify(&cs->monitor);
    }
    split7_monitor_unlock(&cs->monitor);
    if (m->leave_slot != UINT64_MAX && slot > m->leave_slot) {
      LOG_I(PHY, "[split7] client %d left after slot %lu\n", c, m->leave_slot);
      member_drop(e, c);
    }
  }
}

// Transmits a stretch only once every member has completed the slots covering it:
// what is sent depends on the members' slot records alone, never on timing. Woken
// by the members (records, joins, leaves) and by radio progress.
static void *tx_thread(void *arg)
{
  split7_engine_t *e = arg;
  split7_port_mem_t *s = e->mem;
  split7_group_t *g = &e->group;
  const NR_DL_FRAME_PARMS *fp = &e->fp;
  const int sps = fp->symbols_per_slot;
  const int slot_samples = fp->samples_per_subframe / fp->slots_per_subframe;
  bool started = false;

  while (engine_running(e)) {
    // Anything posted from here on wakes the wait at the end of the pass.
    const uint64_t seen = split7_progress_get(&s->host_work);
    if (!split7_port_anchored(e->port)) {
      split7_progress_wait(&s->host_work, seen + 1, 1000);
      continue;
    }
    const int64_t now = radio_now(e);
    if (!started) {
      e->tx_base = e->tx_next = now;
      split7_progress_raise(&e->tx_sent, e->tx_next);
      started = true;
    }
    // Idle (no member, nothing left to send): TX follows the radio, sending nothing.
    bool idle = !e->num_bursts && !e->open_burst;
    for (int c = 0; c < SPLIT7_MAX_CLIENTS; c++)
      idle &= !e->members[c].member;
    if (idle && e->tx_next < now) {
      e->tx_next = now;
      split7_progress_raise(&e->tx_sent, e->tx_next);
    }

    // Completed slots first, then the timeline: a timing shift is always placed after
    // the slots seen completed here (see demodulate_ready_symbols()).
    split7_timebase_t tb = timebase_snapshot(e);
    update_membership(e, &tb);
    advance(e, &tb);
    tb = timebase_snapshot(e);

    // The group bounds the stretch while any member still has slots to send.
    bool bound = false;
    for (int c = 0; c < SPLIT7_MAX_CLIENTS; c++)
      bound |= e->members[c].member && e->members[c].leave_slot > g->done;
    // Members bound the stretch (they run at most rx2tx slots ahead of the radio).
    // Without any, the group's completed slots are still walked (leaving members
    // are released once passed), then what is left of the content sent and an open
    // burst ended.
    int64_t end = e->tx_next;
    if (bound || (g->active && g->cursor <= g->done * sps + sps - 1))
      end = g->reach;
    if (!bound && e->num_bursts && e->bursts[e->num_bursts - 1].end > end)
      end = e->bursts[e->num_bursts - 1].end;
    // An open burst nobody can continue any more is ended right away; while members
    // run, the next slot may still continue it at tx_next.
    if (!bound && end <= e->tx_next && e->open_burst)
      end = e->tx_next + 1;
    if (end > e->tx_next + slot_samples)
      end = e->tx_next + slot_samples;
    if (end <= e->tx_next) {
      publish_progress(e); // a member may have left with everything already sent
      split7_progress_wait(&s->host_work, seen + 1, 1000);
      continue;
    }

    const uint64_t first_slot = g->cursor / sps;
    assemble(e, &tb, e->tx_next, end);
    if (e->num_bursts && e->bursts[0].start < end && e->bursts[0].start < now) {
      e->tx_stats.tx_late_writes++;
      const int holder = g->active && first_slot <= g->done ? g->holder[first_slot % s->ring_slots] : -1;
      if (holder >= 0) {
        split7_client_info_t *cs = &s->clients[holder];
        split7_monitor_lock(&cs->monitor);
        cs->stats.late_stretches++;
        split7_monitor_unlock(&cs->monitor);
      }
    }
    playout_write(e, e->tx_next, end, now);
    e->tx_next = end;
    split7_progress_raise(&e->tx_sent, e->tx_next);

    publish_progress(e);
    publish_tx_stats(e);
  }
  return NULL;
}

/* ------------------------------------------------------------------------- */
/* Lifecycle                                                                 */
/* ------------------------------------------------------------------------- */

split7_engine_t *split7_engine_create(const split7_engine_cfg_t *cfg)
{
  split7_engine_t *e = engine_malloc(sizeof(*e));
  e->fp = *cfg->fp;
  e->rx_link = cfg->rx_link;
  e->tx_link = cfg->tx_link;
  const NR_DL_FRAME_PARMS *fp = &e->fp;
  e->nb_rx = fp->nb_antennas_rx;
  e->nb_tx = fp->nb_antennas_tx;
  AssertFatal(e->nb_rx <= SPLIT7_MAX_ANT && e->nb_tx <= SPLIT7_MAX_ANT, "too many antennas\n");
  pthread_mutex_init(&e->tb_mutex, NULL);
  pthread_mutex_init(&e->state_mutex, NULL);
  split7_progress_init(&e->tx_sent, 0);
  split7_progress_init(&e->rx_time, 0);
  e->rx_chunk = e->fp.ofdm_symbol_size;

  if (cfg->radio) {
    e->radio = cfg->radio;
  } else {
    e->radio_cfg = cfg->radio_cfg;
    e->radio = &e->own_radio;
    AssertFatal(openair0_device_load(e->radio, &e->radio_cfg) == 0, "[split7] failed to load the radio\n");
  }

  const double samples_per_us = fp->samples_per_subframe / 1000.0;

  pthread_mutex_init(&e->clk_mutex, NULL);
  e->clk_rate = samples_per_us / 1000.0;

  // 4 frames of RX: room for a cell search capture (20 ms + a slot) behind the live edge.
  e->rx_size = 4 * fp->samples_per_frame;
  for (int a = 0; a < e->nb_rx; a++)
    e->rx_td[a] = engine_malloc(e->rx_size * sizeof(c16_t));
  e->rx_sym = engine_malloc((fp->ofdm_symbol_size + fp->nb_prefix_samples0) * sizeof(c16_t));
  e->tx_size = 2 * fp->samples_per_frame;
  for (int a = 0; a < e->nb_tx; a++)
    e->tx_td[a] = engine_malloc(e->tx_size * sizeof(c16_t));
  e->mod_buf = engine_malloc((fp->ofdm_symbol_size + fp->nb_prefix_samples0) * sizeof(c16_t));
  e->mod_scratch = engine_malloc(fp->ofdm_symbol_size * sizeof(c16_t));
  for (int a = 0; a < e->nb_tx; a++)
    e->fd_sum[a] = engine_malloc(fp->N_RB_UL * NR_NB_SC_PER_RB * sizeof(c16_t));

  // Largest PRACH IDFT: a long preamble's 1.25 kHz spacing at this sampling rate.
  const int prach_max = fp->samples_per_subframe * 1000 / 1250;
  e->prach_scratch = engine_malloc(prach_max * sizeof(c16_t));
  e->prach_buf = engine_malloc((size_t)(prach_max * (1 + SPLIT7_PRACH_MAX_REPS)) * sizeof(c16_t)); // CP + repetitions

  split7_geometry_t geo = {
      .fft_size = fp->ofdm_symbol_size,
      .n_rb_dl = fp->N_RB_DL,
      .n_rb_ul = fp->N_RB_UL,
      .nb_rx = e->nb_rx,
      .nb_tx = e->nb_tx,
      .symbols_per_slot = fp->symbols_per_slot,
      .slots_per_frame = fp->slots_per_frame,
      .ring_symbols = 2 * fp->slots_per_frame * fp->symbols_per_slot,
      .prach_max_samples = prach_max,
  };
  e->port = split7_port_create(&geo);
  e->mem = e->port->mem;
  const uint32_t R = e->mem->ring_slots;
  for (int c = 0; c < SPLIT7_MAX_CLIENTS; c++) {
    e->members[c].used = engine_malloc(R * sizeof(uint32_t));
    e->members[c].prach = engine_malloc(R * sizeof(int32_t));
    e->members[c].ta = engine_malloc(R * sizeof(int32_t));
  }
  e->group.ta = engine_malloc(R * sizeof(int32_t));
  e->group.clip = engine_malloc(R * sizeof(int64_t));
  e->group.holder = engine_malloc(R * sizeof(int8_t));
  e->served_req = 0;
  return e;
}

int split7_engine_start(split7_engine_t *e)
{
  if (e->radio->trx_start_func && e->radio->trx_start_func(e->radio) < 0) {
    LOG_E(PHY, "[split7] failed to start the radio\n");
    return -1;
  }
  pthread_mutex_lock(&e->state_mutex);
  e->running = true;
  pthread_mutex_unlock(&e->state_mutex);
  threadCreate(&e->rx_thread, rx_thread, e, "split7_rx", -1, OAI_PRIORITY_RT_MAX);
  threadCreate(&e->tx_thread, tx_thread, e, "split7_tx", -1, OAI_PRIORITY_RT_MAX);
  e->started = true;
  return 0;
}

void split7_engine_destroy(split7_engine_t *e)
{
  if (!e)
    return;
  pthread_mutex_lock(&e->state_mutex);
  e->service_cancel = true;
  e->running = false;
  pthread_mutex_unlock(&e->state_mutex);
  // Clients waiting on the port (backpressure, leave) give up; the TX thread wakes.
  split7_port_stop(e->port);
  split7_progress_close(&e->tx_sent);
  split7_progress_close(&e->rx_time);
  if (e->started) {
    pthread_join(e->tx_thread, NULL);
    pthread_join(e->rx_thread, NULL);
    if (e->service_thread_valid)
      pthread_join(e->service_thread, NULL);
    if (e->radio->trx_stop_func)
      e->radio->trx_stop_func(e->radio);
  }
  if (e->radio == &e->own_radio && e->radio->trx_end_func)
    e->radio->trx_end_func(e->radio);
  LOG_I(PHY,
        "[split7] engine stats: tx samples %lu, tx IDFTs %lu, late tx stretches %lu, failed radio writes %lu, burst ends by a "
        "separate write %lu (after air time %lu), rx overruns %lu\n",
        e->stats.tx_samples,
        e->stats.tx_idft,
        e->stats.tx_late_writes,
        e->stats.tx_write_errors,
        e->stats.burst_end_markers,
        e->stats.burst_end_late,
        e->stats.rx_overruns);
  for (int c = 0; c < SPLIT7_MAX_CLIENTS; c++) {
    const split7_client_stats_t *st = &e->mem->clients[c].stats;
    if (st->tx_symbols || st->prach)
      LOG_I(PHY,
            "[split7] client %d: tx symbols %lu, prach %lu, late stretches waiting for it %lu, TA-clipped samples %lu\n",
            c,
            st->tx_symbols,
            st->prach,
            st->late_stretches,
            st->ta_clipped);
    free(e->members[c].used);
    free(e->members[c].prach);
    free(e->members[c].ta);
  }
  free(e->group.ta);
  free(e->group.clip);
  free(e->group.holder);
  split7_port_destroy(e->port);
  for (int a = 0; a < e->nb_rx; a++)
    free(e->rx_td[a]);
  for (int a = 0; a < e->nb_tx; a++)
    free(e->tx_td[a]);
  free(e->rx_sym);
  free(e->mod_buf);
  free(e->mod_scratch);
  for (int a = 0; a < e->nb_tx; a++)
    free(e->fd_sum[a]);
  free(e->prach_scratch);
  free(e->prach_buf);
  pthread_mutex_destroy(&e->tb_mutex);
  pthread_mutex_destroy(&e->clk_mutex);
  pthread_mutex_destroy(&e->state_mutex);
  split7_progress_destroy(&e->tx_sent);
  split7_progress_destroy(&e->rx_time);
  free(e);
}

split7_port_t *split7_engine_port(split7_engine_t *e)
{
  return e->port;
}

const NR_DL_FRAME_PARMS *split7_engine_fp(const split7_engine_t *e)
{
  return &e->fp;
}

void split7_engine_set_anchor_service(split7_engine_t *e, split7_anchor_service_t service, void *ctx)
{
  e->service = service;
  e->service_ctx = ctx;
}
