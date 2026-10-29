/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#define _GNU_SOURCE
#include "split7_device.h"
#include "split7_device_internal.h"
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

static void *device_malloc(size_t bytes)
{
  void *p = malloc16_clear(bytes);
  AssertFatal(p, "OOM (%zu bytes)\n", bytes);
  return p;
}

static split7_timebase_t timebase_snapshot(split7_device_t *dev)
{
  pthread_mutex_lock(&dev->tb_mutex);
  split7_timebase_t tb = dev->tb;
  pthread_mutex_unlock(&dev->tb_mutex);
  return tb;
}

// TX thread statistics are counted privately and published once per pass.
static void publish_tx_stats(split7_device_t *dev)
{
  const split7_device_stats_t *t = &dev->tx_stats;
  pthread_mutex_lock(&dev->state_mutex);
  dev->stats.tx_samples += t->tx_samples;
  dev->stats.tx_idft += t->tx_idft;
  dev->stats.tx_late_writes += t->tx_late_writes;
  dev->stats.tx_write_errors += t->tx_write_errors;
  dev->stats.burst_end_markers += t->burst_end_markers;
  dev->stats.burst_end_late += t->burst_end_late;
  pthread_mutex_unlock(&dev->state_mutex);
  dev->tx_stats = (split7_device_stats_t){0};
}

static bool device_running(split7_device_t *dev)
{
  pthread_mutex_lock(&dev->state_mutex);
  const bool running = dev->running;
  pthread_mutex_unlock(&dev->state_mutex);
  return running;
}

split7_device_stats_t split7_device_stats(split7_device_t *dev)
{
  pthread_mutex_lock(&dev->state_mutex);
  const split7_device_stats_t stats = dev->stats;
  pthread_mutex_unlock(&dev->state_mutex);
  return stats;
}

bool split7_device_service_cancelled(split7_device_t *dev)
{
  pthread_mutex_lock(&dev->state_mutex);
  const bool cancelled = dev->service_cancel;
  pthread_mutex_unlock(&dev->state_mutex);
  return cancelled;
}

int64_t split7_device_rx_time(split7_device_t *dev)
{
  return split7_progress_get(&dev->rx_time);
}

bool split7_device_rx_copy(split7_device_t *dev, int ant, int64_t ts, int num_samples, c16_t *dst)
{
  const int64_t now = split7_device_rx_time(dev);
  if (ts < 0 || ts + num_samples > now || now - ts > (int64_t)dev->rx_size)
    return false;
  const uint64_t idx = ts % dev->rx_size;
  const uint64_t first = dev->rx_size - idx < (uint64_t)num_samples ? dev->rx_size - idx : (uint64_t)num_samples;
  memcpy(dst, dev->rx_td[ant] + idx, first * sizeof(c16_t));
  memcpy(dst + first, dev->rx_td[ant], (num_samples - first) * sizeof(c16_t));
  // The radio thread may have overwritten the start while copying, also with the read
  // it has not published yet.
  return (int64_t)split7_progress_get_after_reads(&dev->rx_time) + dev->rx_chunk - ts <= (int64_t)dev->rx_size;
}

void split7_device_set_anchor(split7_device_t *dev, int64_t anchor_ts, uint64_t abs_frame, const split7_cell_t *cell)
{
  const NR_DL_FRAME_PARMS *fp = &dev->fp;
  split7_cell_t c = cell ? *cell : (split7_cell_t){0};
  const int64_t now = split7_device_rx_time(dev);
  pthread_mutex_lock(&dev->tb_mutex);
  dev->tb = (split7_timebase_t){.anchor_ts = anchor_ts, .anchor_frame = abs_frame};
  // Start with the slot being received now: nothing older is worth demodulating.
  const uint64_t sym = split7_symbol_at(&dev->tb, fp, now);
  dev->next_demod = sym > 0 ? (sym - 1) / fp->symbols_per_slot * fp->symbols_per_slot : 0;
  c.first_slot = dev->next_demod / fp->symbols_per_slot;
  const uint64_t first_symbol = dev->next_demod;
  pthread_mutex_unlock(&dev->tb_mutex);
  split7_port_anchor(dev->port, &c, first_symbol);
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

static void demodulate_ready_symbols(split7_device_t *dev)
{
  split7_port_mem_t *s = dev->mem;
  const NR_DL_FRAME_PARMS *fp = &dev->fp;
  const int sps = fp->symbols_per_slot;
  const int n_rb = fp->N_RB_DL;
  const int64_t now = split7_device_rx_time(dev);

  pthread_mutex_lock(&dev->tb_mutex);
  split7_timebase_t tb = dev->tb;
  uint64_t k = dev->next_demod;
  pthread_mutex_unlock(&dev->tb_mutex);

  for (;;) {
    if (k % sps == 0) {
      const int32_t shift = split7_port_take_timing_shift(dev->port);
      if (shift) {
        // Only slots no member has completed are moved, and not the first of them, whose
        // start already bounds what the host may transmit: RX and TX then place every
        // slot with the same shift.
        pthread_mutex_lock(&dev->tb_mutex);
        const uint64_t pending = dev->max_done + 1;
        const uint64_t from = k / sps > pending ? k / sps : pending;
        split7_timebase_shift(&dev->tb, from, shift);
        tb = dev->tb;
        pthread_mutex_unlock(&dev->tb_mutex);
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
    for (int a = 0; a < dev->fp.nb_antennas_rx; a++) {
      c16_t *dst = dst_base + ((uint64_t)a * s->geo.ring_symbols + ring_idx) * fp->ofdm_symbol_size;
      if (!split7_device_rx_copy(dev, a, t, len, dev->rx_sym)) {
        pthread_mutex_lock(&dev->state_mutex);
        dev->stats.rx_overruns++;
        pthread_mutex_unlock(&dev->state_mutex);
        memset(dst, 0, fp->ofdm_symbol_size * sizeof(c16_t));
        continue;
      }
      split7_ofdm_demod(fp, dev->rx_link, n_rb, slot, symbol, dev->rx_sym, dst);
    }
    k++;
    if (k % sps == 0) {
      split7_progress_raise(&s->rx_published, k);
      split7_progress_add(&s->host_work, 1); // the TX thread follows the radio
    }
  }

  pthread_mutex_lock(&dev->tb_mutex);
  dev->next_demod = k;
  pthread_mutex_unlock(&dev->tb_mutex);
}

static void *anchor_service_thread(void *arg)
{
  split7_device_t *dev = arg;
  const int rc = dev->service(dev, dev->service_ctx, dev->service_pci, dev->service_timeout_ms);
  split7_port_answer_sync(dev->port, dev->service_req, rc);
  pthread_mutex_lock(&dev->state_mutex);
  dev->service_busy = false;
  pthread_mutex_unlock(&dev->state_mutex);
  return NULL;
}

static void serve_sync_requests(split7_device_t *dev)
{
  pthread_mutex_lock(&dev->state_mutex);
  const bool busy = dev->service_busy;
  pthread_mutex_unlock(&dev->state_mutex);
  if (busy)
    return;
  const bool anchored = split7_port_anchored(dev->port);
  if (!anchored && !dev->service)
    return; // the host anchors by itself
  uint32_t req = dev->served_req;
  int pci;
  uint32_t timeout_ms;
  if (!split7_port_take_sync_request(dev->port, &req, &pci, &timeout_ms))
    return;
  dev->served_req = req;
  if (anchored) {
    split7_port_answer_sync(dev->port, req, 0);
    return;
  }
  if (dev->service_thread_valid)
    pthread_join(dev->service_thread, NULL);
  dev->service_req = req;
  dev->service_pci = pci;
  dev->service_timeout_ms = timeout_ms;
  pthread_mutex_lock(&dev->state_mutex);
  dev->service_busy = true;
  pthread_mutex_unlock(&dev->state_mutex);
  // Own thread, not a pool worker: a search runs for hundreds of ms.
  threadCreate(&dev->service_thread, anchor_service_thread, dev, "split7_sync", -1, OAI_PRIORITY_RT_LOW);
  dev->service_thread_valid = true;
}

static void *rx_thread(void *arg)
{
  split7_device_t *dev = arg;
  const int chunk = dev->rx_chunk;
  c16_t *bufs[SPLIT7_MAX_ANT];
  void *ptrs[SPLIT7_MAX_ANT];
  for (int a = 0; a < dev->fp.nb_antennas_rx; a++)
    bufs[a] = device_malloc(chunk * sizeof(c16_t));

  while (device_running(dev)) {
    for (int a = 0; a < dev->fp.nb_antennas_rx; a++)
      ptrs[a] = bufs[a];
    openair0_timestamp_t ts = 0;
    const int n = dev->radio->trx_read_func(dev->radio, &ts, ptrs, chunk, dev->fp.nb_antennas_rx);
    if (n < 0) {
      usleep(1000);
      continue;
    }
    AssertFatal(n == chunk, "[split7] radio returned %d samples, expected %d\n", n, chunk);
    const uint64_t idx = ts % dev->rx_size;
    const uint64_t first = dev->rx_size - idx < (uint64_t)n ? dev->rx_size - idx : (uint64_t)n;
    for (int a = 0; a < dev->fp.nb_antennas_rx; a++) {
      memcpy(dev->rx_td[a] + idx, bufs[a], first * sizeof(c16_t));
      memcpy(dev->rx_td[a], bufs[a] + first, (n - first) * sizeof(c16_t));
    }
    struct timespec wall;
    clock_gettime(CLOCK_MONOTONIC, &wall);
    const int64_t ns = wall.tv_sec * 1000000000LL + wall.tv_nsec;
    pthread_mutex_lock(&dev->clk_mutex);
    dev->clk_rx = ts + n;
    dev->clk_ns = ns;
    if (!dev->clk_ref_ns) {
      dev->clk_ref_rx = dev->clk_rx;
      dev->clk_ref_ns = ns;
    } else if (ns - dev->clk_ref_ns > 1000000000LL) {
      // Measured, not nominal: a simulated radio may run slower than real time.
      dev->clk_rate = (double)(dev->clk_rx - dev->clk_ref_rx) / (ns - dev->clk_ref_ns);
      dev->clk_ref_rx = dev->clk_rx;
      dev->clk_ref_ns = ns;
    }
    pthread_mutex_unlock(&dev->clk_mutex);
    split7_progress_raise(&dev->rx_time, ts + n);
    if (dev->lockstep)
      split7_progress_add(&dev->mem->host_work, 1); // TX keeps up with every read

    serve_sync_requests(dev);
    if (split7_port_anchored(dev->port))
      demodulate_ready_symbols(dev);
  }
  for (int a = 0; a < dev->fp.nb_antennas_rx; a++)
    free(bufs[a]);
  return NULL;
}

/* ------------------------------------------------------------------------- */
/* TX                                                                        */
/* ------------------------------------------------------------------------- */

// Records [start, end) as transmitted content: sorted, touching intervals merged.
static void content_add(split7_device_t *dev, int64_t start, int64_t end)
{
  split7_burst_t *b = dev->bursts;
  int i = 0;
  while (i < dev->num_bursts && b[i].end < start)
    i++;
  if (i < dev->num_bursts && b[i].start <= end) {
    // Overlaps or touches b[i] (and maybe the ones after it): merge.
    if (start < b[i].start)
      b[i].start = start;
    if (end > b[i].end)
      b[i].end = end;
    while (i + 1 < dev->num_bursts && b[i + 1].start <= b[i].end) {
      if (b[i + 1].end > b[i].end)
        b[i].end = b[i + 1].end;
      b[i].opened |= b[i + 1].opened;
      memmove(&b[i + 1], &b[i + 2], (dev->num_bursts - i - 2) * sizeof(*b));
      dev->num_bursts--;
    }
    return;
  }
  AssertFatal(dev->num_bursts < SPLIT7_MAX_BURSTS, "[split7] more than %d pending bursts\n", SPLIT7_MAX_BURSTS);
  memmove(&b[i + 1], &b[i], (dev->num_bursts - i) * sizeof(*b));
  b[i] = (split7_burst_t){.start = start, .end = end};
  dev->num_bursts++;
}

static void radio_write(split7_device_t *dev, int64_t ts, int n, int flags)
{
  void *ptrs[SPLIT7_MAX_ANT];
  const uint64_t idx = (ts - dev->tx_base) % dev->tx_size;
  for (int a = 0; a < dev->fp.nb_antennas_tx; a++)
    ptrs[a] = dev->tx_td[a] + idx;
  if (dev->radio->trx_write_func(dev->radio, ts, ptrs, n, dev->fp.nb_antennas_tx, flags) != n)
    dev->tx_stats.tx_write_errors++;
  dev->tx_stats.tx_samples += n;
}

// Writes [start, end) of the playout ring as one piece of a burst, split at the
// ring's end if needed; is_start / is_end mark the burst's first / last samples.
static void write_piece(split7_device_t *dev, int64_t start, int64_t end, bool is_start, bool is_end)
{
  while (start < end) {
    const uint64_t idx = (start - dev->tx_base) % dev->tx_size;
    const int64_t run = dev->tx_size - idx < (uint64_t)(end - start) ? (int64_t)(dev->tx_size - idx) : end - start;
    const bool first = is_start, last = is_end && start + run == end;
    const int flags = first && last ? TX_BURST_START_AND_END : first ? TX_BURST_START : last ? TX_BURST_END : TX_BURST_MIDDLE;
    radio_write(dev, start, run, flags);
    start += run;
    is_start = false;
  }
}

// Lockstep: every sample of [t0, t1) is written, silence included, as part of one
// continuous burst; the content intervals are only dropped.
static void stream_write(split7_device_t *dev, int64_t t0, int64_t t1)
{
  split7_burst_t *b = dev->bursts;
  write_piece(dev, t0, t1, !dev->streaming, false);
  dev->streaming = true;
  int done = 0;
  while (done < dev->num_bursts && b[done].end <= t1)
    done++;
  memmove(b, b + done, (dev->num_bursts - done) * sizeof(*b));
  dev->num_bursts -= done;
  if (dev->num_bursts && b[0].start < t1)
    b[0].start = t1;
}

// Transmits the content of [t0, t1) and nothing else: silence is not written. A
// burst reaching t1 stays open, ended in the next stretch if nothing continues it.
static void burst_write(split7_device_t *dev, int64_t t0, int64_t t1, int64_t now)
{
  split7_burst_t *b = dev->bursts;
  if (dev->open_burst && !(dev->num_bursts && b[0].start <= t0)) {
    radio_write(dev, t0, 1, TX_BURST_END); // a zero: the burst ended at t0
    dev->tx_stats.burst_end_markers++;
    if (t0 < now)
      dev->tx_stats.burst_end_late++;
    dev->open_burst = false;
  }
  bool open = false;
  int done = 0;
  for (int i = 0; i < dev->num_bursts && b[i].start < t1; i++) {
    const int64_t start = b[i].start > t0 ? b[i].start : t0;
    const int64_t end = b[i].end < t1 ? b[i].end : t1;
    const bool is_start = !b[i].opened && !(dev->open_burst && b[i].start <= t0);
    // Content added later starts at t1 at the earliest: a burst ending before t1 is over.
    const bool is_end = b[i].end < t1;
    write_piece(dev, start, end, is_start, is_end);
    if (b[i].end <= t1) {
      done++;
      open = b[i].end == t1;
    } else {
      b[i].start = t1;
      b[i].opened = true;
    }
  }
  memmove(b, b + done, (dev->num_bursts - done) * sizeof(*b));
  dev->num_bursts -= done;
  dev->open_burst = open;
}

// Sends [t0, t1) of the playout ring, then clears it for reuse.
static void playout_write(split7_device_t *dev, int64_t t0, int64_t t1, int64_t now)
{
  if (dev->lockstep)
    stream_write(dev, t0, t1);
  else
    burst_write(dev, t0, t1, now);
  for (int64_t pos = t0; pos < t1;) {
    const uint64_t idx = (pos - dev->tx_base) % dev->tx_size;
    const int64_t run = dev->tx_size - idx < (uint64_t)(t1 - pos) ? (int64_t)(dev->tx_size - idx) : t1 - pos;
    for (int a = 0; a < dev->fp.nb_antennas_tx; a++)
      memset(dev->tx_td[a] + idx, 0, run * sizeof(c16_t));
    pos += run;
  }
}

// Adds the part of src at or after from_ts into the playout ring.
static void playout_add(split7_device_t *dev, int ant, int64_t ts, const c16_t *src, int len, int64_t from_ts)
{
  int skip = 0;
  if (ts < from_ts) {
    skip = from_ts - ts;
    if (skip >= len)
      return;
  }
  int64_t pos = ts + skip;
  int n = len - skip;
  AssertFatal(pos + n - dev->tx_next <= (int64_t)dev->tx_size, "[split7] playout ring too small\n");
  content_add(dev, pos, pos + n);
  src += skip;
  while (n > 0) {
    const uint64_t idx = (pos - dev->tx_base) % dev->tx_size;
    const int run = dev->tx_size - idx < (uint64_t)n ? dev->tx_size - idx : n;
    c16_t *dst = dev->tx_td[ant] + idx;
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
// A lockstep radio has no time but its reads: it waits for this TX.
static int64_t radio_now(split7_device_t *dev)
{
  if (dev->lockstep)
    return split7_device_rx_time(dev);
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  const int64_t ns = ts.tv_sec * 1000000000LL + ts.tv_nsec;
  pthread_mutex_lock(&dev->clk_mutex);
  const int64_t now = dev->clk_ns ? dev->clk_rx + (int64_t)((ns - dev->clk_ns) * dev->clk_rate) : dev->clk_rx;
  pthread_mutex_unlock(&dev->clk_mutex);
  return now;
}

// The member with the lowest id steers the downlink timing.
static void update_leader(split7_device_t *dev)
{
  int32_t leader = -1;
  for (int c = 0; c < dev->mem->geo.max_clients && leader < 0; c++)
    if (dev->members[c].member)
      leader = c;
  split7_port_set_timing_master(dev->port, leader);
}

// Frees PRACH entries of client c for slots up to through that no remaining slot
// record of it refers to: a PRACH posted for a slot given up, skipped or never
// joined would otherwise hold its entry forever.
// Holding the client's monitor.
static void release_prach(split7_device_t *dev, int c, uint64_t through)
{
  const split7_member_t *m = &dev->members[c];
  const uint32_t R = dev->mem->ring_slots;
  for (int i = 0; i < SPLIT7_MAX_PRACH; i++) {
    split7_entry_hdr_t *entry = split7_port_prach_entry(dev->port, c, i);
    if (!entry->tag || entry->tag - 1 > through)
      continue;
    const uint64_t slot = entry->tag - 1;
    const bool pending =
        m->member && slot >= m->first_slot && slot >= dev->group.prach_slot && slot <= m->done && m->prach[slot % R] == i;
    if (!pending)
      entry->tag = 0;
  }
}

// Ends membership: a leaving client is open again, a closing one free. Its PRACH
// entries are released.
static void member_drop(split7_device_t *dev, int c)
{
  split7_client_info_t *cs = &dev->mem->clients[c];
  dev->members[c].member = false;
  split7_monitor_lock(&cs->monitor);
  release_prach(dev, c, UINT64_MAX);
  if (cs->state == SPLIT7_CLIENT_LEAVING)
    cs->state = SPLIT7_CLIENT_OPEN;
  else if (cs->state == SPLIT7_CLIENT_CLOSING)
    cs->state = SPLIT7_CLIENT_FREE;
  split7_monitor_notify(&cs->monitor);
  split7_monitor_unlock(&cs->monitor);
  update_leader(dev);
  bool any = false;
  for (int m = 0; m < dev->mem->geo.max_clients; m++)
    any |= dev->members[m].member;
  dev->group.active = any;
}

static bool participates(const split7_member_t *m, uint64_t slot)
{
  return m->member && slot >= m->first_slot && slot <= m->leave_slot;
}

// First slot a join asking for slot can have: the first whose start at the joining
// member's TA has not been transmitted, and not one the group has completed or
// started assembling.
static uint64_t join_slot_for(split7_device_t *dev, const split7_timebase_t *tb, uint64_t slot, int64_t join_ta)
{
  const split7_group_t *g = &dev->group;
  const NR_DL_FRAME_PARMS *fp = &dev->fp;
  const int sps = fp->symbols_per_slot;
  if (g->active && slot < g->done + 1)
    slot = g->done + 1;
  while ((g->active && slot * sps < g->cursor) || split7_symbol_ts(tb, fp, slot * sps) - join_ta < dev->tx_next)
    slot++;
  return slot;
}

// The group starts at slot with the first member's TA.
static void group_start(split7_device_t *dev, const split7_timebase_t *tb, uint64_t slot, int64_t ta)
{
  split7_group_t *g = &dev->group;
  const int sps = dev->fp.symbols_per_slot;
  g->active = true;
  g->waiting_for = -1;
  g->done = slot - 1;
  g->cursor = slot * sps;
  g->prach_slot = slot;
  g->reach = split7_symbol_ts(tb, &dev->fp, slot * sps) - ta;
  g->ta_max[(slot - 1) % dev->mem->ring_slots] = ta;
}

// Membership events: joins are accepted at the first requested slot the group can
// still take; leaving and closing clients are let go.
static void update_membership(split7_device_t *dev, const split7_timebase_t *tb)
{
  split7_port_mem_t *s = dev->mem;
  const NR_DL_FRAME_PARMS *fp = &dev->fp;
  const int sps = fp->symbols_per_slot;
  for (int c = 0; c < dev->mem->geo.max_clients; c++) {
    split7_client_info_t *cs = &s->clients[c];
    split7_member_t *m = &dev->members[c];
    split7_monitor_lock(&cs->monitor);
    const split7_client_state_t state = cs->state;
    const uint64_t leave_slot = cs->leave_slot;
    const uint64_t asked = cs->join_slot;
    const int64_t join_ta = cs->join_ta;
    split7_monitor_unlock(&cs->monitor);
    if (state == SPLIT7_CLIENT_CLOSING) {
      if (m->member)
        LOG_W(PHY, "[split7] client %d closed without leaving: dropped at slot %lu\n", c, m->done);
      member_drop(dev, c);
      continue;
    }
    if (m->member && state == SPLIT7_CLIENT_LEAVING && m->leave_slot == UINT64_MAX)
      m->leave_slot = leave_slot;
    if (!m->member && state == SPLIT7_CLIENT_LEAVING) {
      member_drop(dev, c); // left before being accepted
      continue;
    }
    if (state != SPLIT7_CLIENT_JOINING)
      continue;
    const uint64_t slot = join_slot_for(dev, tb, asked, join_ta);
    const uint64_t host_slot = dev->group.active ? dev->group.cursor / sps : slot;
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
    if (!dev->group.active)
      group_start(dev, tb, slot, join_ta);
    *m = (split7_member_t){.member = true,
                           .first_slot = slot,
                           .leave_slot = UINT64_MAX,
                           .done = slot - 1,
                           .used = m->used,
                           .prach = m->prach,
                           .ta = m->ta,
                           .join_ta = join_ta};
    update_leader(dev);
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
static void advance(split7_device_t *dev, const split7_timebase_t *tb)
{
  split7_port_mem_t *s = dev->mem;
  split7_group_t *g = &dev->group;
  const NR_DL_FRAME_PARMS *fp = &dev->fp;
  const uint32_t R = s->ring_slots;
  // The per-slot arrays hold one ring ahead of what is assembled (a skip can
  // complete hundreds of slots at once: the rest is taken as the cursor moves).
  const uint64_t limit = g->cursor / fp->symbols_per_slot + R;
  uint64_t max_done = 0;
  for (int c = 0; c < dev->mem->geo.max_clients; c++) {
    split7_member_t *m = &dev->members[c];
    if (!m->member)
      continue;
    split7_client_info_t *cs = &s->clients[c];
    split7_monitor_lock(&cs->monitor);
    const uint64_t skip = cs->skip_through;
    for (uint64_t slot = m->done + 1; slot < limit; slot++) {
      const uint64_t idx = slot % R;
      const split7_slot_record_t *r = split7_port_record(dev->port, c, slot);
      if (r->tag == slot + 1) {
        m->used[idx] = r->used_symbols;
        m->prach[idx] = r->prach;
        m->ta[idx] = r->ta_samples;
      } else if (slot < skip) {
        m->used[idx] = 0;
        m->prach[idx] = -1;
        m->ta[idx] = slot > m->first_slot ? m->ta[(slot - 1) % R] : m->join_ta;
      } else {
        break;
      }
      m->done = slot;
    }
    release_prach(dev, c, m->done);
    split7_monitor_unlock(&cs->monitor);
    if (m->done + 1 > max_done)
      max_done = m->done + 1;
  }
  // Never backwards (a member far ahead may leave): timing shifts start after it.
  pthread_mutex_lock(&dev->tb_mutex);
  if (max_done > dev->max_done)
    dev->max_done = max_done;
  pthread_mutex_unlock(&dev->tb_mutex);

  int bottleneck = -1;
  while (g->active && g->done + 1 < limit) {
    const uint64_t slot = g->done + 1;
    const uint64_t idx = slot % R;
    int32_t ta_max = -1;
    bool any_later = false;
    for (int c = 0; c < dev->mem->geo.max_clients; c++) {
      const split7_member_t *m = &dev->members[c];
      if (m->member && slot < m->first_slot)
        any_later = true;
      if (!participates(m, slot))
        continue;
      if (m->done < slot) {
        if (bottleneck < 0 || m->done < dev->members[bottleneck].done)
          bottleneck = c;
      } else if (m->ta[idx] > ta_max) {
        ta_max = m->ta[idx];
      }
    }
    if (bottleneck >= 0) {
      g->waiting_for = bottleneck;
      break;
    }
    if (ta_max < 0 && !any_later)
      break; // every member has left: nothing more to assemble
    // Nobody in the slot (a member joins later): the previous slot's placement goes on.
    g->ta_max[idx] = ta_max >= 0 ? ta_max : g->ta_max[(slot - 1) % R];
    g->holder[idx] = g->waiting_for;
    g->waiting_for = -1;
    // The member with the largest TA ends the slot first: TX is final up to there.
    // A slot placed before what is final (a TA increase) is shortened by dropping its
    // first samples (CP first, TS 38.213 4.2). A decrease leaves a gap of zeros.
    g->clip[idx] = g->reach;
    const int64_t end = split7_symbol_ts(tb, fp, (slot + 1) * fp->symbols_per_slot) - g->ta_max[idx];
    if (end > g->reach)
      g->reach = end;
    g->done = slot;
  }
}

// Assembles the group's completed slots up to the first symbol starting at or after
// t1: PRACH windows as they are, each member at its own TA; the symbols of the
// members sharing a TA summed in frequency domain and modulated once.
static void assemble(split7_device_t *dev, const split7_timebase_t *tb, int64_t t0, int64_t t1)
{
  split7_group_t *g = &dev->group;
  const NR_DL_FRAME_PARMS *fp = &dev->fp;
  const int sps = fp->symbols_per_slot;
  const uint32_t R = dev->mem->ring_slots;
  const int n_re = fp->N_RB_UL * NR_NB_SC_PER_RB;
  for (; g->active; g->cursor++) {
    const uint64_t k = g->cursor;
    const uint64_t slot = k / sps;
    if (slot > g->done)
      break;
    const uint64_t idx = slot % R;
    const int64_t clip = g->clip[idx];
    const int64_t from = clip > t0 ? clip : t0;
    // The symbol's earliest placement, at the largest TA of its slot.
    const int64_t first = split7_symbol_ts(tb, fp, k) - g->ta_max[idx];
    if ((first > clip ? first : clip) >= t1)
      break;
    if (k % sps == 0 && g->prach_slot == slot) {
      for (int c = 0; c < dev->mem->geo.max_clients; c++) {
        const split7_member_t *m = &dev->members[c];
        if (!participates(m, slot) || m->prach[idx] < 0)
          continue;
        // The client fills an entry only while its tag is 0, the host frees it.
        split7_monitor_t *mon = &dev->mem->clients[c].monitor;
        split7_entry_hdr_t *entry = split7_port_prach_entry(dev->port, c, m->prach[idx]);
        split7_monitor_lock(mon);
        const bool posted = entry->tag == slot + 1;
        split7_monitor_unlock(mon);
        if (!posted)
          continue;
        const int total = entry->ncp + entry->reps * entry->dftlen;
        split7_ofdm_window_mod((const c16_t *)(entry + 1),
                               entry->dftlen,
                               entry->ncp,
                               entry->reps,
                               dev->prach_scratch,
                               dev->prach_buf);
        const int64_t slot_start = split7_symbol_ts(tb, fp, k) - m->ta[idx];
        playout_add(dev, 0, slot_start + entry->time_offset, dev->prach_buf, total, from);
        split7_monitor_lock(mon);
        entry->tag = 0;
        split7_monitor_unlock(mon);
      }
      g->prach_slot = slot + 1;
    }

    // The members sending the symbol, then one sum and modulation per distinct TA.
    int sending[SPLIT7_MAX_CLIENTS];
    int n = 0;
    for (int c = 0; c < dev->mem->geo.max_clients; c++) {
      const split7_member_t *m = &dev->members[c];
      if (!participates(m, slot) || !(m->used[idx] & (1u << (k % sps))))
        continue;
      // Written before the slot record, which advance() took under the client's monitor.
      const split7_entry_hdr_t *entry = split7_port_tx_entry(dev->port, c, k);
      if (entry->tag != k + 1) {
        LOG_E(PHY, "[split7] client %d: slot record lists symbol %lu, but it was not posted\n", c, k);
        continue;
      }
      sending[n++] = c;
    }
    const int len = split7_symbol_samples(fp, slot % fp->slots_per_frame, k % sps);
    while (n > 0) {
      const int32_t ta = dev->members[sending[0]].ta[idx];
      const int64_t start = split7_symbol_ts(tb, fp, k) - ta;
      int contributors = 0;
      int rest = 0;
      for (int i = 0; i < n; i++) {
        const int c = sending[i];
        if (dev->members[c].ta[idx] != ta) {
          sending[rest++] = c;
          continue;
        }
        split7_entry_hdr_t *entry = split7_port_tx_entry(dev->port, c, k);
        for (int a = 0; a < dev->fp.nb_antennas_tx; a++) {
          const c16_t *in = split7_entry_tx_data(dev->port, entry, a);
          if (!contributors) {
            memcpy(dev->fd_sum[a], in, n_re * sizeof(c16_t));
          } else {
            for (int r = 0; r < n_re; r++)
              dev->fd_sum[a][r] = (c16_t){.r = sat16(dev->fd_sum[a][r].r + in[r].r), .i = sat16(dev->fd_sum[a][r].i + in[r].i)};
          }
        }
        if (from > start) {
          split7_client_info_t *cs = &dev->mem->clients[c];
          split7_monitor_lock(&cs->monitor);
          cs->stats.ta_clipped += from - start;
          split7_monitor_unlock(&cs->monitor);
        }
        contributors++;
      }
      n = rest;
      for (int a = 0; a < dev->fp.nb_antennas_tx; a++) {
        split7_ofdm_mod(fp,
                        dev->tx_link,
                        fp->N_RB_UL,
                        slot % fp->slots_per_frame,
                        k % sps,
                        dev->fd_sum[a],
                        dev->mod_scratch,
                        dev->mod_buf);
        playout_add(dev, a, start, dev->mod_buf, len, from);
      }
      dev->tx_stats.tx_idft += dev->fp.nb_antennas_tx;
    }
  }
}

// Tells members how far the host got (backpressure) and releases leaving members
// whose last slot it passed.
static void publish_progress(split7_device_t *dev)
{
  const uint64_t slot = dev->group.cursor / dev->fp.symbols_per_slot;
  for (int c = 0; c < dev->mem->geo.max_clients; c++) {
    split7_member_t *m = &dev->members[c];
    if (!m->member)
      continue;
    split7_client_info_t *cs = &dev->mem->clients[c];
    split7_monitor_lock(&cs->monitor);
    if (cs->host_slot != slot) {
      cs->host_slot = slot;
      split7_monitor_notify(&cs->monitor);
    }
    split7_monitor_unlock(&cs->monitor);
    if (m->leave_slot != UINT64_MAX && slot > m->leave_slot) {
      LOG_I(PHY, "[split7] client %d left after slot %lu\n", c, m->leave_slot);
      member_drop(dev, c);
    }
  }
}

// Transmits a stretch only once every member has completed the slots covering it:
// what is sent depends on the members' slot records alone, never on timing. Woken
// by the members (records, joins, leaves) and by radio progress.
static void *tx_thread(void *arg)
{
  split7_device_t *dev = arg;
  split7_port_mem_t *s = dev->mem;
  split7_group_t *g = &dev->group;
  const NR_DL_FRAME_PARMS *fp = &dev->fp;
  const int sps = fp->symbols_per_slot;
  const int slot_samples = fp->samples_per_subframe / fp->slots_per_subframe;
  bool started = false;

  while (device_running(dev)) {
    // Anything posted from here on wakes the wait at the end of the pass.
    const uint64_t seen = split7_progress_get(&s->host_work);
    const bool anchored = split7_port_anchored(dev->port);
    // A lockstep radio needs TX from its first read on: its peers wait for it.
    const int64_t now = radio_now(dev);
    if (!(anchored || dev->lockstep) || (!started && dev->lockstep && !now)) {
      split7_progress_wait(&s->host_work, seen + 1, 1000);
      continue;
    }
    if (!started) {
      dev->tx_base = dev->tx_next = now;
      split7_progress_raise(&dev->tx_sent, dev->tx_next);
      started = true;
    }
    // Idle (no member, nothing left to send): TX follows the radio, sending nothing,
    // or silence on a lockstep radio, one read ahead: a radio waiting for TX to pass its
    // next read would otherwise wait forever for a TX waiting for that read.
    bool idle = !dev->num_bursts && !dev->open_burst;
    for (int c = 0; c < dev->mem->geo.max_clients; c++)
      idle &= !dev->members[c].member;
    const int64_t follow = dev->lockstep ? now + dev->rx_chunk : now;
    if (idle && dev->tx_next < follow) {
      if (dev->lockstep)
        playout_write(dev, dev->tx_next, follow, now);
      dev->tx_next = follow;
      split7_progress_raise(&dev->tx_sent, dev->tx_next);
      publish_tx_stats(dev);
    }
    if (!anchored) {
      split7_progress_wait(&s->host_work, seen + 1, 1000);
      continue;
    }

    // Completed slots first, then the timeline: a timing shift is always placed after
    // the slots seen completed here (see demodulate_ready_symbols()).
    split7_timebase_t tb = timebase_snapshot(dev);
    update_membership(dev, &tb);
    advance(dev, &tb);
    tb = timebase_snapshot(dev);

    // The group bounds the stretch while any member still has slots to send.
    bool bound = false;
    for (int c = 0; c < dev->mem->geo.max_clients; c++)
      bound |= dev->members[c].member && dev->members[c].leave_slot > g->done;
    // Members bound the stretch (they run at most rx2tx slots ahead of the radio).
    // Without any, the group's completed slots are still walked (leaving members
    // are released once passed), then what is left of the content sent and an open
    // burst ended.
    int64_t end = dev->tx_next;
    if (bound || (g->active && g->cursor <= g->done * sps + sps - 1))
      end = g->reach;
    if (!bound && dev->num_bursts && dev->bursts[dev->num_bursts - 1].end > end)
      end = dev->bursts[dev->num_bursts - 1].end;
    // An open burst nobody can continue any more is ended right away; while members
    // run, the next slot may still continue it at tx_next.
    if (!bound && end <= dev->tx_next && dev->open_burst)
      end = dev->tx_next + 1;
    if (end > dev->tx_next + slot_samples)
      end = dev->tx_next + slot_samples;
    if (end <= dev->tx_next) {
      publish_progress(dev); // a member may have left with everything already sent
      split7_progress_wait(&s->host_work, seen + 1, 1000);
      continue;
    }

    const uint64_t first_slot = g->cursor / sps;
    assemble(dev, &tb, dev->tx_next, end);
    // A lockstep radio waits for this TX: nothing is late.
    if (!dev->lockstep && dev->num_bursts && dev->bursts[0].start < end && dev->bursts[0].start < now) {
      dev->tx_stats.tx_late_writes++;
      const int holder = g->active && first_slot <= g->done ? g->holder[first_slot % s->ring_slots] : -1;
      if (holder >= 0) {
        split7_client_info_t *cs = &s->clients[holder];
        split7_monitor_lock(&cs->monitor);
        cs->stats.late_stretches++;
        split7_monitor_unlock(&cs->monitor);
      }
    }
    playout_write(dev, dev->tx_next, end, now);
    dev->tx_next = end;
    split7_progress_raise(&dev->tx_sent, dev->tx_next);

    publish_progress(dev);
    publish_tx_stats(dev);
  }
  return NULL;
}

/* ------------------------------------------------------------------------- */
/* Lifecycle                                                                 */
/* ------------------------------------------------------------------------- */

split7_device_t *split7_device_create(const split7_device_cfg_t *cfg)
{
  split7_device_t *dev = device_malloc(sizeof(*dev));
  dev->fp = *cfg->fp;
  dev->rx_link = cfg->rx_link;
  dev->tx_link = cfg->tx_link;
  const NR_DL_FRAME_PARMS *fp = &dev->fp;
  dev->lockstep = cfg->lockstep;
  AssertFatal(dev->fp.nb_antennas_rx <= SPLIT7_MAX_ANT && dev->fp.nb_antennas_tx <= SPLIT7_MAX_ANT, "too many antennas\n");
  pthread_mutex_init(&dev->tb_mutex, NULL);
  pthread_mutex_init(&dev->state_mutex, NULL);
  split7_progress_init(&dev->tx_sent, 0);
  split7_progress_init(&dev->rx_time, 0);
  dev->rx_chunk = dev->fp.ofdm_symbol_size;

  if (cfg->radio) {
    dev->radio = cfg->radio;
  } else {
    dev->radio_cfg = cfg->radio_cfg;
    dev->radio = &dev->own_radio;
    AssertFatal(openair0_device_load(dev->radio, &dev->radio_cfg) == 0, "[split7] failed to load the radio\n");
  }

  const double samples_per_us = fp->samples_per_subframe / 1000.0;

  pthread_mutex_init(&dev->clk_mutex, NULL);
  dev->clk_rate = samples_per_us / 1000.0;

  // 4 frames of RX: room for a cell search capture (20 ms + a slot) behind the live edge.
  dev->rx_size = 4 * fp->samples_per_frame;
  for (int a = 0; a < dev->fp.nb_antennas_rx; a++)
    dev->rx_td[a] = device_malloc(dev->rx_size * sizeof(c16_t));
  dev->rx_sym = device_malloc((fp->ofdm_symbol_size + fp->nb_prefix_samples0) * sizeof(c16_t));
  dev->tx_size = 2 * fp->samples_per_frame;
  for (int a = 0; a < dev->fp.nb_antennas_tx; a++)
    dev->tx_td[a] = device_malloc(dev->tx_size * sizeof(c16_t));
  dev->mod_buf = device_malloc((fp->ofdm_symbol_size + fp->nb_prefix_samples0) * sizeof(c16_t));
  dev->mod_scratch = device_malloc(fp->ofdm_symbol_size * sizeof(c16_t));
  for (int a = 0; a < dev->fp.nb_antennas_tx; a++)
    dev->fd_sum[a] = device_malloc(fp->N_RB_UL * NR_NB_SC_PER_RB * sizeof(c16_t));

  // Largest PRACH IDFT: a long preamble's 1.25 kHz spacing at this sampling rate.
  const int prach_max = fp->samples_per_subframe * 1000 / 1250;
  dev->prach_scratch = device_malloc(prach_max * sizeof(c16_t));
  dev->prach_buf = device_malloc((size_t)(prach_max * (1 + SPLIT7_PRACH_MAX_REPS)) * sizeof(c16_t)); // CP + repetitions

  split7_geometry_t geo = {
      .fft_size = fp->ofdm_symbol_size,
      .n_rb_dl = fp->N_RB_DL,
      .n_rb_ul = fp->N_RB_UL,
      .nb_rx = dev->fp.nb_antennas_rx,
      .nb_tx = dev->fp.nb_antennas_tx,
      .symbols_per_slot = fp->symbols_per_slot,
      .slots_per_frame = fp->slots_per_frame,
      .ring_symbols = 2 * fp->slots_per_frame * fp->symbols_per_slot,
      .prach_max_samples = prach_max,
      .max_clients = cfg->max_clients,
  };
  dev->port = split7_port_create(&geo);
  dev->mem = dev->port->mem;
  const uint32_t R = dev->mem->ring_slots;
  for (int c = 0; c < dev->mem->geo.max_clients; c++) {
    dev->members[c].used = device_malloc(R * sizeof(uint32_t));
    dev->members[c].prach = device_malloc(R * sizeof(int32_t));
    dev->members[c].ta = device_malloc(R * sizeof(int32_t));
  }
  dev->group.ta_max = device_malloc(R * sizeof(int32_t));
  dev->group.clip = device_malloc(R * sizeof(int64_t));
  dev->group.holder = device_malloc(R * sizeof(*dev->group.holder));
  dev->served_req = 0;
  return dev;
}

int split7_device_start(split7_device_t *dev)
{
  if (dev->radio->trx_start_func && dev->radio->trx_start_func(dev->radio) < 0) {
    LOG_E(PHY, "[split7] failed to start the radio\n");
    return -1;
  }
  pthread_mutex_lock(&dev->state_mutex);
  dev->running = true;
  pthread_mutex_unlock(&dev->state_mutex);
  threadCreate(&dev->rx_thread, rx_thread, dev, "split7_rx", -1, OAI_PRIORITY_RT_MAX);
  threadCreate(&dev->tx_thread, tx_thread, dev, "split7_tx", -1, OAI_PRIORITY_RT_MAX);
  dev->started = true;
  return 0;
}

void split7_device_destroy(split7_device_t *dev)
{
  if (!dev)
    return;
  pthread_mutex_lock(&dev->state_mutex);
  dev->service_cancel = true;
  dev->running = false;
  pthread_mutex_unlock(&dev->state_mutex);
  // Clients waiting on the port (backpressure, leave) give up; the TX thread wakes.
  split7_port_stop(dev->port);
  split7_progress_close(&dev->tx_sent);
  split7_progress_close(&dev->rx_time);
  if (dev->started) {
    pthread_join(dev->tx_thread, NULL);
    pthread_join(dev->rx_thread, NULL);
    if (dev->service_thread_valid)
      pthread_join(dev->service_thread, NULL);
    if (dev->radio->trx_stop_func)
      dev->radio->trx_stop_func(dev->radio);
  }
  if (dev->radio == &dev->own_radio && dev->radio->trx_end_func)
    dev->radio->trx_end_func(dev->radio);
  LOG_I(PHY,
        "[split7] device stats: tx samples %lu, tx IDFTs %lu, late tx stretches %lu, failed radio writes %lu, burst ends by a "
        "separate write %lu (after air time %lu), rx overruns %lu\n",
        dev->stats.tx_samples,
        dev->stats.tx_idft,
        dev->stats.tx_late_writes,
        dev->stats.tx_write_errors,
        dev->stats.burst_end_markers,
        dev->stats.burst_end_late,
        dev->stats.rx_overruns);
  for (int c = 0; c < dev->mem->geo.max_clients; c++) {
    const split7_client_stats_t *st = &dev->mem->clients[c].stats;
    if (st->tx_symbols || st->prach)
      LOG_I(PHY,
            "[split7] client %d: tx symbols %lu, prach %lu, late stretches waiting for it %lu, TA-clipped samples %lu\n",
            c,
            st->tx_symbols,
            st->prach,
            st->late_stretches,
            st->ta_clipped);
    free(dev->members[c].used);
    free(dev->members[c].prach);
    free(dev->members[c].ta);
  }
  free(dev->group.ta_max);
  free(dev->group.clip);
  free(dev->group.holder);
  split7_port_destroy(dev->port);
  for (int a = 0; a < dev->fp.nb_antennas_rx; a++)
    free(dev->rx_td[a]);
  for (int a = 0; a < dev->fp.nb_antennas_tx; a++)
    free(dev->tx_td[a]);
  free(dev->rx_sym);
  free(dev->mod_buf);
  free(dev->mod_scratch);
  for (int a = 0; a < dev->fp.nb_antennas_tx; a++)
    free(dev->fd_sum[a]);
  free(dev->prach_scratch);
  free(dev->prach_buf);
  pthread_mutex_destroy(&dev->tb_mutex);
  pthread_mutex_destroy(&dev->clk_mutex);
  pthread_mutex_destroy(&dev->state_mutex);
  split7_progress_destroy(&dev->tx_sent);
  split7_progress_destroy(&dev->rx_time);
  free(dev);
}

split7_port_t *split7_device_port(split7_device_t *dev)
{
  return dev->port;
}

const NR_DL_FRAME_PARMS *split7_device_fp(const split7_device_t *dev)
{
  return &dev->fp;
}

void split7_device_set_anchor_service(split7_device_t *dev, split7_anchor_service_t service, void *ctx)
{
  dev->service = service;
  dev->service_ctx = ctx;
}
