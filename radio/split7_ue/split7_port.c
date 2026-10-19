/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#define _GNU_SOURCE
#include "split7_port.h"
#include <stdlib.h>
#include <string.h>
#include "common/utils/LOG/log.h"
#include "PHY/TOOLS/tools_defs.h"

#define SPLIT7_ALIGN 64

static uint64_t align_up(uint64_t v, uint64_t a)
{
  return (v + a - 1) / a * a;
}

static void layout(split7_port_mem_t *hdr, const split7_geometry_t *geo)
{
  hdr->geo = *geo;
  hdr->tx_stride = align_up(geo->n_rb_ul * 12, 16);
  hdr->tx_entry_size = sizeof(split7_entry_hdr_t) + (uint64_t)geo->nb_tx * hdr->tx_stride * sizeof(c16_t);
  hdr->prach_entry_size = sizeof(split7_entry_hdr_t) + align_up(geo->prach_max_samples, 16) * sizeof(c16_t);
  uint64_t off = align_up(sizeof(split7_port_mem_t), SPLIT7_ALIGN);
  hdr->rx_off = off;
  off += align_up((uint64_t)geo->nb_rx * geo->ring_symbols * geo->fft_size * sizeof(c16_t), SPLIT7_ALIGN);
  hdr->tx_off = off;
  off += (uint64_t)geo->max_clients * geo->ring_symbols * hdr->tx_entry_size;
  hdr->prach_off = off;
  off += (uint64_t)geo->max_clients * SPLIT7_MAX_PRACH * hdr->prach_entry_size;
  hdr->ring_slots = geo->ring_symbols / geo->symbols_per_slot;
  hdr->record_off = off;
  off += (uint64_t)geo->max_clients * hdr->ring_slots * sizeof(split7_slot_record_t);
  hdr->size = align_up(off, SPLIT7_ALIGN);
}

split7_port_t *split7_port_create(const split7_geometry_t *geo)
{
  AssertFatal(geo->max_clients >= 1 && geo->max_clients <= SPLIT7_MAX_CLIENTS,
              "%u client slots, 1 to %d supported\n",
              geo->max_clients,
              SPLIT7_MAX_CLIENTS);
  AssertFatal(geo->nb_rx <= SPLIT7_MAX_ANT && geo->nb_tx <= SPLIT7_MAX_ANT, "too many antennas (%u/%u)\n", geo->nb_rx, geo->nb_tx);
  split7_port_mem_t hdr = {0};
  layout(&hdr, geo);

  split7_port_t *port = calloc(1, sizeof(*port));
  AssertFatal(port, "OOM\n");
  // Zeroed: every tag empty, every client free.
  port->mem = malloc16_clear(hdr.size);
  AssertFatal(port->mem, "OOM (%lu bytes)\n", hdr.size);
  split7_port_mem_t *s = port->mem;
  memcpy(s, &hdr, sizeof(hdr));
  split7_progress_init(&s->rx_published, 0);
  split7_progress_init(&s->host_work, 0);
  split7_monitor_init(&s->monitor);
  s->timing_master = -1;
  for (int i = 0; i < s->geo.max_clients; i++)
    split7_monitor_init(&s->clients[i].monitor);
  LOG_I(PHY, "[split7] port: %lu MB\n", hdr.size >> 20);
  return port;
}

void split7_port_destroy(split7_port_t *port)
{
  if (!port)
    return;
  split7_port_mem_t *s = port->mem;
  for (int i = 0; i < s->geo.max_clients; i++)
    split7_monitor_destroy(&s->clients[i].monitor);
  split7_monitor_destroy(&s->monitor);
  split7_progress_destroy(&s->host_work);
  split7_progress_destroy(&s->rx_published);
  free(s);
  free(port);
}

/* ------------------------------------------------------------------------- */
/* Host API                                                                  */
/* ------------------------------------------------------------------------- */

void split7_port_anchor(split7_port_t *port, const split7_cell_t *cell, uint64_t first_symbol)
{
  split7_port_mem_t *s = port->mem;
  split7_progress_raise(&s->rx_published, first_symbol);
  split7_monitor_lock(&s->monitor);
  s->cell = *cell;
  s->anchored = true;
  split7_monitor_notify(&s->monitor);
  split7_monitor_unlock(&s->monitor);
  split7_progress_add(&s->host_work, 1);
}

bool split7_port_anchored(split7_port_t *port)
{
  split7_port_mem_t *s = port->mem;
  split7_monitor_lock(&s->monitor);
  const bool anchored = s->anchored;
  split7_monitor_unlock(&s->monitor);
  return anchored;
}

bool split7_port_take_sync_request(split7_port_t *port, uint32_t *served, int *expected_pci, uint32_t *timeout_ms)
{
  split7_port_mem_t *s = port->mem;
  split7_monitor_lock(&s->monitor);
  const bool fresh = s->sync_req != *served;
  if (fresh) {
    *served = s->sync_req;
    *expected_pci = s->sync_expected_pci;
    *timeout_ms = s->sync_timeout_ms;
  }
  split7_monitor_unlock(&s->monitor);
  return fresh;
}

void split7_port_answer_sync(split7_port_t *port, uint32_t req, int status)
{
  split7_port_mem_t *s = port->mem;
  split7_monitor_lock(&s->monitor);
  s->sync_status = status;
  s->sync_done = req;
  split7_monitor_notify(&s->monitor);
  split7_monitor_unlock(&s->monitor);
}

void split7_port_set_timing_master(split7_port_t *port, int client)
{
  split7_port_mem_t *s = port->mem;
  split7_monitor_lock(&s->monitor);
  s->timing_master = client;
  split7_monitor_unlock(&s->monitor);
}

int32_t split7_port_take_timing_shift(split7_port_t *port)
{
  split7_port_mem_t *s = port->mem;
  split7_monitor_lock(&s->monitor);
  const int32_t shift = s->timing_shift;
  s->timing_shift = 0;
  split7_monitor_unlock(&s->monitor);
  return shift;
}

void split7_port_stop(split7_port_t *port)
{
  split7_port_mem_t *s = port->mem;
  for (int i = 0; i < s->geo.max_clients; i++) {
    split7_client_info_t *cs = &s->clients[i];
    split7_monitor_lock(&cs->monitor);
    cs->host_stopped = true;
    split7_monitor_notify(&cs->monitor);
    split7_monitor_unlock(&cs->monitor);
  }
  split7_monitor_lock(&s->monitor);
  s->stopped = true;
  split7_monitor_notify(&s->monitor);
  split7_monitor_unlock(&s->monitor);
  split7_progress_close(&s->rx_published);
  split7_progress_close(&s->host_work);
}

/* ------------------------------------------------------------------------- */
/* Client API                                                                */
/* ------------------------------------------------------------------------- */

// Tells the host something changed (records, membership).
static void notify_host(split7_client_t *c)
{
  split7_progress_add(&c->port->mem->host_work, 1);
}

int split7_client_open(split7_port_t *port, split7_client_t *c)
{
  split7_port_mem_t *s = port->mem;
  for (int i = 0; i < s->geo.max_clients; i++) {
    split7_client_info_t *cs = &s->clients[i];
    split7_monitor_lock(&cs->monitor);
    const bool free_slot = cs->state == SPLIT7_CLIENT_FREE;
    if (free_slot) {
      cs->state = SPLIT7_CLIENT_OPEN;
      cs->drop_through = 0;
      cs->stats = (split7_client_stats_t){0};
    }
    split7_monitor_unlock(&cs->monitor);
    if (!free_slot)
      continue;
    memset(c, 0, sizeof(*c));
    c->port = port;
    c->id = i;
    c->mem = cs;
    for (uint32_t a = 0; a < s->geo.nb_rx; a++)
      c->rx_ring[a] = split7_port_rx_ring(port, a);
    return 0;
  }
  return -1;
}

void split7_client_close(split7_client_t *c)
{
  split7_client_info_t *cs = c->mem;
  // The host drops a member at once, releases its PRACH entries, then frees the slot:
  // a client opening it next starts clean.
  split7_monitor_lock(&cs->monitor);
  cs->state = SPLIT7_CLIENT_CLOSING;
  split7_monitor_unlock(&cs->monitor);
  notify_host(c);
  const split7_deadline_t forever = split7_deadline_in(-1);
  split7_monitor_lock(&cs->monitor);
  while (cs->state == SPLIT7_CLIENT_CLOSING && !cs->host_stopped)
    split7_monitor_wait(&cs->monitor, &forever);
  if (cs->host_stopped) {
    for (int e = 0; e < SPLIT7_MAX_PRACH; e++)
      split7_port_prach_entry(c->port, c->id, e)->tag = 0;
    cs->state = SPLIT7_CLIENT_FREE;
  }
  split7_monitor_unlock(&cs->monitor);
}

uint32_t split7_client_request_sync(split7_client_t *c, int expected_pci, uint32_t timeout_ms)
{
  split7_port_mem_t *s = c->port->mem;
  split7_monitor_lock(&s->monitor);
  s->sync_expected_pci = expected_pci;
  s->sync_timeout_ms = timeout_ms;
  const uint32_t req = ++s->sync_req;
  split7_monitor_unlock(&s->monitor);
  return req;
}

int split7_client_sync_result(split7_client_t *c, uint32_t req, split7_cell_t *cell, int timeout_us)
{
  split7_port_mem_t *s = c->port->mem;
  const split7_deadline_t deadline = split7_deadline_in(timeout_us);
  split7_monitor_lock(&s->monitor);
  int result;
  for (;;) {
    if (s->anchored) {
      *cell = s->cell;
      result = 1;
      break;
    }
    if (s->stopped || ((int32_t)(s->sync_done - req) >= 0 && s->sync_status < 0)) {
      result = -1;
      break;
    }
    if (!split7_monitor_wait(&s->monitor, &deadline)) {
      result = s->anchored ? 1 : 0;
      if (result)
        *cell = s->cell;
      break;
    }
  }
  split7_monitor_unlock(&s->monitor);
  return result;
}

// True when the slot starting at symbol first is still intact, published being the
// RX progress. Keeps one slot of margin: the host may be overwriting the oldest one.
static bool rx_slot_in_ring(const split7_port_mem_t *s, uint64_t published, uint64_t first)
{
  return published - first + s->geo.symbols_per_slot <= s->geo.ring_symbols;
}

uint64_t split7_client_next_published_slot(const split7_client_t *c)
{
  split7_port_mem_t *s = c->port->mem;
  return split7_progress_get(&s->rx_published) / s->geo.symbols_per_slot;
}

int split7_client_wait_slot(split7_client_t *c, uint64_t abs_slot, int timeout_us)
{
  split7_port_mem_t *s = c->port->mem;
  const uint64_t first = abs_slot * s->geo.symbols_per_slot;
  const uint64_t end = first + s->geo.symbols_per_slot;
  switch (split7_progress_wait(&s->rx_published, end, timeout_us)) {
    case SPLIT7_TIMEOUT:
      return 1;
    case SPLIT7_CLOSED:
      return -2;
    case SPLIT7_REACHED:
      break;
  }
  return rx_slot_in_ring(s, split7_progress_get(&s->rx_published), first) ? 0 : -1;
}

bool split7_client_rx_symbol(split7_client_t *c, uint64_t abs_sym, c16_t *const *dst, int nb_ant)
{
  split7_port_mem_t *s = c->port->mem;
  const uint64_t first = abs_sym - abs_sym % s->geo.symbols_per_slot;
  if (abs_sym >= split7_progress_get(&s->rx_published) || !rx_slot_in_ring(s, split7_progress_get(&s->rx_published), first))
    return false;
  for (int a = 0; a < nb_ant; a++)
    memcpy(dst[a], c->rx_ring[a] + (abs_sym % s->geo.ring_symbols) * s->geo.fft_size, s->geo.fft_size * sizeof(c16_t));
  // Checked again after the copy: the host may have started overwriting it meanwhile.
  return rx_slot_in_ring(s, split7_progress_get_after_reads(&s->rx_published), first);
}

// Holding the client's monitor: waits while the client leaves. False when the host stopped.
static bool wait_left(split7_client_info_t *cs, const split7_deadline_t *deadline)
{
  while (cs->state == SPLIT7_CLIENT_LEAVING) {
    if (cs->host_stopped || !split7_monitor_wait(&cs->monitor, deadline))
      return cs->state != SPLIT7_CLIENT_LEAVING;
  }
  return true;
}

bool split7_client_join(split7_client_t *c, uint64_t first_tx_slot, uint32_t ta_samples)
{
  split7_client_info_t *cs = c->mem;
  const split7_deadline_t forever = split7_deadline_in(-1);
  split7_monitor_lock(&cs->monitor);
  const bool ok = wait_left(cs, &forever) && cs->state == SPLIT7_CLIENT_OPEN && !cs->host_stopped;
  if (ok) {
    cs->join_slot = first_tx_slot;
    cs->join_ta = ta_samples;
    cs->skip_through = 0;
    // Slots given up belong to the previous membership; nothing before the join slot counts.
    cs->drop_through = first_tx_slot;
    cs->state = SPLIT7_CLIENT_JOINING;
  }
  split7_monitor_unlock(&cs->monitor);
  if (ok)
    notify_host(c);
  return ok;
}

// Holding the client's monitor.
static void raise_skip_through(split7_client_info_t *cs, uint64_t last_slot)
{
  if (last_slot + 1 > cs->skip_through)
    cs->skip_through = last_slot + 1;
}

void split7_client_skip_through(split7_client_t *c, uint64_t last_slot)
{
  split7_monitor_lock(&c->mem->monitor);
  raise_skip_through(c->mem, last_slot);
  split7_monitor_unlock(&c->mem->monitor);
  notify_host(c);
}

bool split7_client_leave(split7_client_t *c, uint64_t last_slot, int timeout_us)
{
  split7_client_info_t *cs = c->mem;
  const split7_deadline_t deadline = split7_deadline_in(timeout_us);
  split7_monitor_lock(&cs->monitor);
  const bool in_group =
      cs->state == SPLIT7_CLIENT_JOINING || cs->state == SPLIT7_CLIENT_MEMBER || cs->state == SPLIT7_CLIENT_LEAVING;
  if (in_group) {
    raise_skip_through(cs, last_slot);
    cs->leave_slot = last_slot;
    cs->state = SPLIT7_CLIENT_LEAVING;
  }
  split7_monitor_unlock(&cs->monitor);
  if (!in_group)
    return true;
  notify_host(c);
  split7_monitor_lock(&cs->monitor);
  const bool left = wait_left(cs, &deadline);
  split7_monitor_unlock(&cs->monitor);
  return left;
}

bool split7_client_is_timing_master(const split7_client_t *c)
{
  split7_port_mem_t *s = c->port->mem;
  split7_monitor_lock(&s->monitor);
  const bool master = s->timing_master == c->id;
  split7_monitor_unlock(&s->monitor);
  return master;
}

void split7_client_adjust_timing(split7_client_t *c, int32_t shift_samples)
{
  split7_port_mem_t *s = c->port->mem;
  if (shift_samples == 0)
    return;
  split7_monitor_lock(&s->monitor);
  if (s->timing_master == c->id)
    s->timing_shift += shift_samples;
  split7_monitor_unlock(&s->monitor);
}

split7_client_stats_t split7_client_stats(const split7_client_t *c)
{
  split7_monitor_lock(&c->mem->monitor);
  const split7_client_stats_t stats = c->mem->stats;
  split7_monitor_unlock(&c->mem->monitor);
  return stats;
}

static bool in_group(const split7_client_info_t *cs)
{
  return cs->state == SPLIT7_CLIENT_MEMBER || cs->state == SPLIT7_CLIENT_LEAVING;
}

// Backpressure: a member may run at most a ring ahead of what the host has transmitted.
// Holding the client's monitor. 1: abs_slot may be posted now, 0: not yet (no room),
// -1: never (given up, split7_client_drop_through()).
static int room_for(const split7_client_t *c, uint64_t abs_slot)
{
  const split7_client_info_t *cs = c->mem;
  if (abs_slot < cs->drop_through)
    return -1;
  if (!in_group(cs) || cs->host_stopped)
    return 1;
  return abs_slot + 1 < cs->host_slot + c->port->mem->ring_slots;
}

// Holding the client's monitor: waits until abs_slot may be posted (true) or never may (false).
static bool wait_for_room(split7_client_t *c, uint64_t abs_slot)
{
  const split7_deadline_t forever = split7_deadline_in(-1);
  int room;
  while (!(room = room_for(c, abs_slot)))
    split7_monitor_wait(&c->mem->monitor, &forever);
  return room > 0;
}

void split7_client_drop_through(split7_client_t *c, uint64_t last_slot)
{
  split7_monitor_lock(&c->mem->monitor);
  if (last_slot + 1 > c->mem->drop_through)
    c->mem->drop_through = last_slot + 1;
  raise_skip_through(c->mem, last_slot);
  split7_monitor_notify(&c->mem->monitor); // posts waiting for room give up
  split7_monitor_unlock(&c->mem->monitor);
  notify_host(c);
}

void split7_client_tx_symbol(split7_client_t *c, uint64_t abs_sym, c16_t *const *data, int nb_ant)
{
  const split7_port_t *port = c->port;
  split7_monitor_lock(&c->mem->monitor);
  const bool post = wait_for_room(c, abs_sym / port->mem->geo.symbols_per_slot);
  if (post && in_group(c->mem))
    c->mem->stats.tx_symbols++;
  split7_monitor_unlock(&c->mem->monitor);
  if (!post)
    return;
  split7_entry_hdr_t *e = split7_port_tx_entry(port, c->id, abs_sym);
  const int n_re = port->mem->geo.n_rb_ul * 12;
  for (int a = 0; a < nb_ant; a++)
    memcpy(split7_entry_tx_data(port, e, a), data[a], n_re * sizeof(c16_t));
  e->tag = abs_sym + 1;
}

int split7_client_tx_prach(split7_client_t *c, uint64_t abs_slot, int time_offset, int ncp, int dftlen, int reps, const c16_t *bins)
{
  const split7_port_t *port = c->port;
  const int max = port->mem->geo.prach_max_samples;
  // The host modulates ncp + reps * dftlen samples into a buffer sized for these bounds.
  AssertFatal(
      dftlen > 0 && dftlen <= max && ncp >= 0 && ncp <= max && reps >= 1 && reps <= SPLIT7_PRACH_MAX_REPS && time_offset >= 0,
      "invalid PRACH: dftlen %d, ncp %d, reps %d, time_offset %d (max size %d, max reps %d)\n",
      dftlen,
      ncp,
      reps,
      time_offset,
      max,
      SPLIT7_PRACH_MAX_REPS);
  // Filled holding the monitor: UL jobs of one client may post concurrently. The host
  // reads an entry only once its tag is posted.
  int entry = -1;
  split7_monitor_lock(&c->mem->monitor);
  for (int i = 0; i < SPLIT7_MAX_PRACH && entry < 0; i++) {
    split7_entry_hdr_t *e = split7_port_prach_entry(port, c->id, i);
    if (e->tag != 0)
      continue;
    e->time_offset = time_offset;
    e->ncp = ncp;
    e->dftlen = dftlen;
    e->reps = reps;
    memcpy(e + 1, bins, dftlen * sizeof(c16_t));
    e->tag = abs_slot + 1;
    c->mem->stats.prach++;
    entry = i;
  }
  split7_monitor_unlock(&c->mem->monitor);
  return entry;
}

// Holding the client's monitor.
static void write_record(split7_client_t *c, uint64_t abs_slot, uint32_t used_symbols, int prach, uint32_t ta_samples)
{
  split7_slot_record_t *r = split7_port_record(c->port, c->id, abs_slot);
  r->used_symbols = used_symbols;
  r->prach = prach;
  r->ta_samples = ta_samples;
  r->tag = abs_slot + 1;
}

void split7_client_slot_done(split7_client_t *c, uint64_t abs_slot, uint32_t used_symbols, int prach, uint32_t ta_samples)
{
  split7_monitor_lock(&c->mem->monitor);
  const bool post = wait_for_room(c, abs_slot);
  if (post)
    write_record(c, abs_slot, used_symbols, prach, ta_samples);
  split7_monitor_unlock(&c->mem->monitor);
  if (post)
    notify_host(c);
}

bool split7_client_try_slot_done(split7_client_t *c, uint64_t abs_slot, uint32_t used_symbols, int prach, uint32_t ta_samples)
{
  split7_monitor_lock(&c->mem->monitor);
  const bool post = room_for(c, abs_slot) > 0;
  if (post)
    write_record(c, abs_slot, used_symbols, prach, ta_samples);
  split7_monitor_unlock(&c->mem->monitor);
  if (post)
    notify_host(c);
  return post;
}
