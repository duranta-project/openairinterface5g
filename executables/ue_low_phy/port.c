/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#define _GNU_SOURCE
#include "port.h"
#include <stdlib.h>
#include <string.h>
#include "common/utils/LOG/log.h"
#include "PHY/TOOLS/tools_defs.h"

#define UE_LOW_PHY_ALIGN 64

static uint64_t align_up(uint64_t v, uint64_t a)
{
  return (v + a - 1) / a * a;
}

static void layout(ue_low_phy_port_mem_t *hdr, const ue_low_phy_geometry_t *geo)
{
  hdr->geo = *geo;
  hdr->tx_stride = align_up(geo->n_rb_ul * 12, 16);
  hdr->tx_entry_size = sizeof(ue_low_phy_entry_hdr_t) + (uint64_t)geo->nb_tx * hdr->tx_stride * sizeof(c16_t);
  hdr->prach_entry_size = sizeof(ue_low_phy_entry_hdr_t) + align_up(geo->prach_max_samples, 16) * sizeof(c16_t);
  uint64_t off = align_up(sizeof(ue_low_phy_port_mem_t), UE_LOW_PHY_ALIGN);
  hdr->rx_off = off;
  off += align_up((uint64_t)geo->nb_rx * geo->ring_symbols * geo->fft_size * sizeof(c16_t), UE_LOW_PHY_ALIGN);
  hdr->tx_off = off;
  off += (uint64_t)geo->max_clients * geo->ring_symbols * hdr->tx_entry_size;
  hdr->prach_off = off;
  off += (uint64_t)geo->max_clients * UE_LOW_PHY_MAX_PRACH * hdr->prach_entry_size;
  hdr->ring_slots = geo->ring_symbols / geo->symbols_per_slot;
  hdr->record_off = off;
  off += (uint64_t)geo->max_clients * hdr->ring_slots * sizeof(ue_low_phy_slot_record_t);
  hdr->size = align_up(off, UE_LOW_PHY_ALIGN);
}

ue_low_phy_port_t *ue_low_phy_port_create(const ue_low_phy_geometry_t *geo)
{
  AssertFatal(geo->max_clients >= 1 && geo->max_clients <= UE_LOW_PHY_MAX_CLIENTS,
              "%u client slots, 1 to %d supported\n",
              geo->max_clients,
              UE_LOW_PHY_MAX_CLIENTS);
  AssertFatal(geo->nb_rx <= UE_LOW_PHY_MAX_ANT && geo->nb_tx <= UE_LOW_PHY_MAX_ANT, "too many antennas (%u/%u)\n", geo->nb_rx, geo->nb_tx);
  ue_low_phy_port_mem_t hdr = {0};
  layout(&hdr, geo);

  ue_low_phy_port_t *port = calloc(1, sizeof(*port));
  AssertFatal(port, "OOM\n");
  // Zeroed: every tag empty, every client free.
  port->mem = malloc16_clear_or_fail(hdr.size);
  AssertFatal(port->mem, "OOM (%lu bytes)\n", hdr.size);
  ue_low_phy_port_mem_t *s = port->mem;
  memcpy(s, &hdr, sizeof(hdr));
  ue_low_phy_progress_init(&s->rx_published, 0);
  ue_low_phy_progress_init(&s->host_work, 0);
  ue_low_phy_monitor_init(&s->monitor);
  s->timing_master = -1;
  for (int i = 0; i < s->geo.max_clients; i++)
    ue_low_phy_monitor_init(&s->clients[i].monitor);
  LOG_I(PHY, "[ue-low-phy] port: %lu MB\n", hdr.size >> 20);
  return port;
}

void ue_low_phy_port_destroy(ue_low_phy_port_t *port)
{
  if (!port)
    return;
  ue_low_phy_port_mem_t *s = port->mem;
  for (int i = 0; i < s->geo.max_clients; i++)
    ue_low_phy_monitor_destroy(&s->clients[i].monitor);
  ue_low_phy_monitor_destroy(&s->monitor);
  ue_low_phy_progress_destroy(&s->host_work);
  ue_low_phy_progress_destroy(&s->rx_published);
  free(s);
  free(port);
}

/* ------------------------------------------------------------------------- */
/* Host API                                                                  */
/* ------------------------------------------------------------------------- */

void ue_low_phy_port_anchor(ue_low_phy_port_t *port, const ue_low_phy_cell_t *cell, uint64_t first_symbol)
{
  ue_low_phy_port_mem_t *s = port->mem;
  ue_low_phy_progress_raise(&s->rx_published, first_symbol);
  ue_low_phy_monitor_lock(&s->monitor);
  s->cell = *cell;
  s->anchored = true;
  ue_low_phy_monitor_notify(&s->monitor);
  ue_low_phy_monitor_unlock(&s->monitor);
  ue_low_phy_progress_add(&s->host_work, 1);
}

bool ue_low_phy_port_anchored(ue_low_phy_port_t *port)
{
  ue_low_phy_port_mem_t *s = port->mem;
  ue_low_phy_monitor_lock(&s->monitor);
  const bool anchored = s->anchored;
  ue_low_phy_monitor_unlock(&s->monitor);
  return anchored;
}

bool ue_low_phy_port_take_sync_request(ue_low_phy_port_t *port, uint32_t *served, int *expected_pci, uint32_t *timeout_ms)
{
  ue_low_phy_port_mem_t *s = port->mem;
  ue_low_phy_monitor_lock(&s->monitor);
  const bool fresh = s->sync_req != *served;
  if (fresh) {
    *served = s->sync_req;
    *expected_pci = s->sync_expected_pci;
    *timeout_ms = s->sync_timeout_ms;
  }
  ue_low_phy_monitor_unlock(&s->monitor);
  return fresh;
}

void ue_low_phy_port_answer_sync(ue_low_phy_port_t *port, uint32_t req, int status)
{
  ue_low_phy_port_mem_t *s = port->mem;
  ue_low_phy_monitor_lock(&s->monitor);
  s->sync_status = status;
  s->sync_done = req;
  ue_low_phy_monitor_notify(&s->monitor);
  ue_low_phy_monitor_unlock(&s->monitor);
}

void ue_low_phy_port_set_timing_master(ue_low_phy_port_t *port, int client)
{
  ue_low_phy_port_mem_t *s = port->mem;
  ue_low_phy_monitor_lock(&s->monitor);
  s->timing_master = client;
  ue_low_phy_monitor_unlock(&s->monitor);
}

int32_t ue_low_phy_port_take_timing_shift(ue_low_phy_port_t *port)
{
  ue_low_phy_port_mem_t *s = port->mem;
  ue_low_phy_monitor_lock(&s->monitor);
  const int32_t shift = s->timing_shift;
  s->timing_shift = 0;
  ue_low_phy_monitor_unlock(&s->monitor);
  return shift;
}

void ue_low_phy_port_stop(ue_low_phy_port_t *port)
{
  ue_low_phy_port_mem_t *s = port->mem;
  for (int i = 0; i < s->geo.max_clients; i++) {
    ue_low_phy_client_info_t *cs = &s->clients[i];
    ue_low_phy_monitor_lock(&cs->monitor);
    cs->host_stopped = true;
    ue_low_phy_monitor_notify(&cs->monitor);
    ue_low_phy_monitor_unlock(&cs->monitor);
  }
  ue_low_phy_monitor_lock(&s->monitor);
  s->stopped = true;
  ue_low_phy_monitor_notify(&s->monitor);
  ue_low_phy_monitor_unlock(&s->monitor);
  ue_low_phy_progress_close(&s->rx_published);
  ue_low_phy_progress_close(&s->host_work);
}

/* ------------------------------------------------------------------------- */
/* Client API                                                                */
/* ------------------------------------------------------------------------- */

// Tells the host something changed (records, membership).
static void notify_host(ue_low_phy_client_t *c)
{
  ue_low_phy_progress_add(&c->port->mem->host_work, 1);
}

int ue_low_phy_client_open(ue_low_phy_port_t *port, ue_low_phy_client_t *c)
{
  ue_low_phy_port_mem_t *s = port->mem;
  for (int i = 0; i < s->geo.max_clients; i++) {
    ue_low_phy_client_info_t *cs = &s->clients[i];
    ue_low_phy_monitor_lock(&cs->monitor);
    const bool free_slot = cs->state == UE_LOW_PHY_CLIENT_FREE;
    if (free_slot) {
      cs->state = UE_LOW_PHY_CLIENT_OPEN;
      cs->drop_through = 0;
      cs->stats = (ue_low_phy_client_stats_t){0};
    }
    ue_low_phy_monitor_unlock(&cs->monitor);
    if (!free_slot)
      continue;
    memset(c, 0, sizeof(*c));
    c->port = port;
    c->id = i;
    c->mem = cs;
    for (uint32_t a = 0; a < s->geo.nb_rx; a++)
      c->rx_ring[a] = ue_low_phy_port_rx_ring(port, a);
    return 0;
  }
  return -1;
}

void ue_low_phy_client_close(ue_low_phy_client_t *c)
{
  ue_low_phy_client_info_t *cs = c->mem;
  // The host drops a member at once, releases its PRACH entries, then frees the slot:
  // a client opening it next starts clean.
  ue_low_phy_monitor_lock(&cs->monitor);
  cs->state = UE_LOW_PHY_CLIENT_CLOSING;
  ue_low_phy_monitor_unlock(&cs->monitor);
  notify_host(c);
  const ue_low_phy_deadline_t forever = ue_low_phy_deadline_in(-1);
  ue_low_phy_monitor_lock(&cs->monitor);
  while (cs->state == UE_LOW_PHY_CLIENT_CLOSING && !cs->host_stopped)
    ue_low_phy_monitor_wait(&cs->monitor, &forever);
  if (cs->host_stopped) {
    for (int e = 0; e < UE_LOW_PHY_MAX_PRACH; e++)
      ue_low_phy_port_prach_entry(c->port, c->id, e)->tag = 0;
    cs->state = UE_LOW_PHY_CLIENT_FREE;
  }
  ue_low_phy_monitor_unlock(&cs->monitor);
}

uint32_t ue_low_phy_client_request_sync(ue_low_phy_client_t *c, int expected_pci, uint32_t timeout_ms)
{
  ue_low_phy_port_mem_t *s = c->port->mem;
  ue_low_phy_monitor_lock(&s->monitor);
  s->sync_expected_pci = expected_pci;
  s->sync_timeout_ms = timeout_ms;
  const uint32_t req = ++s->sync_req;
  ue_low_phy_monitor_unlock(&s->monitor);
  return req;
}

int ue_low_phy_client_sync_result(ue_low_phy_client_t *c, uint32_t req, ue_low_phy_cell_t *cell, int timeout_us)
{
  ue_low_phy_port_mem_t *s = c->port->mem;
  const ue_low_phy_deadline_t deadline = ue_low_phy_deadline_in(timeout_us);
  ue_low_phy_monitor_lock(&s->monitor);
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
    if (!ue_low_phy_monitor_wait(&s->monitor, &deadline)) {
      result = s->anchored ? 1 : 0;
      if (result)
        *cell = s->cell;
      break;
    }
  }
  ue_low_phy_monitor_unlock(&s->monitor);
  return result;
}

// True when the slot starting at symbol first is still intact, published being the
// RX progress. Keeps one slot of margin: the host may be overwriting the oldest one.
static bool rx_slot_in_ring(const ue_low_phy_port_mem_t *s, uint64_t published, uint64_t first)
{
  return published - first + s->geo.symbols_per_slot <= s->geo.ring_symbols;
}

uint64_t ue_low_phy_client_next_published_slot(const ue_low_phy_client_t *c)
{
  ue_low_phy_port_mem_t *s = c->port->mem;
  return ue_low_phy_progress_get(&s->rx_published) / s->geo.symbols_per_slot;
}

int ue_low_phy_client_wait_slot(ue_low_phy_client_t *c, uint64_t abs_slot, int timeout_us)
{
  ue_low_phy_port_mem_t *s = c->port->mem;
  const uint64_t first = abs_slot * s->geo.symbols_per_slot;
  const uint64_t end = first + s->geo.symbols_per_slot;
  switch (ue_low_phy_progress_wait(&s->rx_published, end, timeout_us)) {
    case UE_LOW_PHY_TIMEOUT:
      return 1;
    case UE_LOW_PHY_CLOSED:
      return -2;
    case UE_LOW_PHY_REACHED:
      break;
  }
  return rx_slot_in_ring(s, ue_low_phy_progress_get(&s->rx_published), first) ? 0 : -1;
}

bool ue_low_phy_client_rx_symbol(ue_low_phy_client_t *c, uint64_t abs_sym, c16_t *const *dst, int nb_ant)
{
  ue_low_phy_port_mem_t *s = c->port->mem;
  const uint64_t first = abs_sym - abs_sym % s->geo.symbols_per_slot;
  if (abs_sym >= ue_low_phy_progress_get(&s->rx_published) || !rx_slot_in_ring(s, ue_low_phy_progress_get(&s->rx_published), first))
    return false;
  for (int a = 0; a < nb_ant; a++)
    memcpy(dst[a], c->rx_ring[a] + (abs_sym % s->geo.ring_symbols) * s->geo.fft_size, s->geo.fft_size * sizeof(c16_t));
  // Checked again after the copy: the host may have started overwriting it meanwhile.
  return rx_slot_in_ring(s, ue_low_phy_progress_get_after_reads(&s->rx_published), first);
}

// Holding the client's monitor: waits while the client leaves. False when the host stopped.
static bool wait_left(ue_low_phy_client_info_t *cs, const ue_low_phy_deadline_t *deadline)
{
  while (cs->state == UE_LOW_PHY_CLIENT_LEAVING) {
    if (cs->host_stopped || !ue_low_phy_monitor_wait(&cs->monitor, deadline))
      return cs->state != UE_LOW_PHY_CLIENT_LEAVING;
  }
  return true;
}

bool ue_low_phy_client_join(ue_low_phy_client_t *c, uint64_t first_tx_slot, uint32_t ta_samples)
{
  ue_low_phy_client_info_t *cs = c->mem;
  const ue_low_phy_deadline_t forever = ue_low_phy_deadline_in(-1);
  ue_low_phy_monitor_lock(&cs->monitor);
  const bool ok = wait_left(cs, &forever) && cs->state == UE_LOW_PHY_CLIENT_OPEN && !cs->host_stopped;
  if (ok) {
    cs->join_slot = first_tx_slot;
    cs->join_ta = ta_samples;
    cs->skip_through = 0;
    // Slots given up belong to the previous membership; nothing before the join slot counts.
    cs->drop_through = first_tx_slot;
    cs->state = UE_LOW_PHY_CLIENT_JOINING;
  }
  ue_low_phy_monitor_unlock(&cs->monitor);
  if (ok)
    notify_host(c);
  return ok;
}

// Holding the client's monitor.
static void raise_skip_through(ue_low_phy_client_info_t *cs, uint64_t last_slot)
{
  if (last_slot + 1 > cs->skip_through)
    cs->skip_through = last_slot + 1;
}

void ue_low_phy_client_skip_through(ue_low_phy_client_t *c, uint64_t last_slot)
{
  ue_low_phy_monitor_lock(&c->mem->monitor);
  raise_skip_through(c->mem, last_slot);
  ue_low_phy_monitor_unlock(&c->mem->monitor);
  notify_host(c);
}

bool ue_low_phy_client_leave(ue_low_phy_client_t *c, uint64_t last_slot, int timeout_us)
{
  ue_low_phy_client_info_t *cs = c->mem;
  const ue_low_phy_deadline_t deadline = ue_low_phy_deadline_in(timeout_us);
  ue_low_phy_monitor_lock(&cs->monitor);
  const bool in_group =
      cs->state == UE_LOW_PHY_CLIENT_JOINING || cs->state == UE_LOW_PHY_CLIENT_MEMBER || cs->state == UE_LOW_PHY_CLIENT_LEAVING;
  if (in_group) {
    raise_skip_through(cs, last_slot);
    cs->leave_slot = last_slot;
    cs->state = UE_LOW_PHY_CLIENT_LEAVING;
  }
  ue_low_phy_monitor_unlock(&cs->monitor);
  if (!in_group)
    return true;
  notify_host(c);
  ue_low_phy_monitor_lock(&cs->monitor);
  const bool left = wait_left(cs, &deadline);
  ue_low_phy_monitor_unlock(&cs->monitor);
  return left;
}

bool ue_low_phy_client_is_timing_master(const ue_low_phy_client_t *c)
{
  ue_low_phy_port_mem_t *s = c->port->mem;
  ue_low_phy_monitor_lock(&s->monitor);
  const bool master = s->timing_master == c->id;
  ue_low_phy_monitor_unlock(&s->monitor);
  return master;
}

void ue_low_phy_client_adjust_timing(ue_low_phy_client_t *c, int32_t shift_samples)
{
  ue_low_phy_port_mem_t *s = c->port->mem;
  if (shift_samples == 0)
    return;
  ue_low_phy_monitor_lock(&s->monitor);
  if (s->timing_master == c->id)
    s->timing_shift += shift_samples;
  ue_low_phy_monitor_unlock(&s->monitor);
}

ue_low_phy_client_stats_t ue_low_phy_client_stats(const ue_low_phy_client_t *c)
{
  ue_low_phy_monitor_lock(&c->mem->monitor);
  const ue_low_phy_client_stats_t stats = c->mem->stats;
  ue_low_phy_monitor_unlock(&c->mem->monitor);
  return stats;
}

static bool in_group(const ue_low_phy_client_info_t *cs)
{
  return cs->state == UE_LOW_PHY_CLIENT_MEMBER || cs->state == UE_LOW_PHY_CLIENT_LEAVING;
}

// Backpressure: a member may run at most a ring ahead of what the host has transmitted.
// Holding the client's monitor. 1: abs_slot may be posted now, 0: not yet (no room),
// -1: never (given up, ue_low_phy_client_drop_through()).
static int room_for(const ue_low_phy_client_t *c, uint64_t abs_slot)
{
  const ue_low_phy_client_info_t *cs = c->mem;
  if (abs_slot < cs->drop_through)
    return -1;
  if (!in_group(cs) || cs->host_stopped)
    return 1;
  return abs_slot + 1 < cs->host_slot + c->port->mem->ring_slots;
}

// Holding the client's monitor: waits until abs_slot may be posted (true) or never may (false).
static bool wait_for_room(ue_low_phy_client_t *c, uint64_t abs_slot)
{
  const ue_low_phy_deadline_t forever = ue_low_phy_deadline_in(-1);
  int room;
  while (!(room = room_for(c, abs_slot)))
    ue_low_phy_monitor_wait(&c->mem->monitor, &forever);
  return room > 0;
}

void ue_low_phy_client_drop_through(ue_low_phy_client_t *c, uint64_t last_slot)
{
  ue_low_phy_monitor_lock(&c->mem->monitor);
  if (last_slot + 1 > c->mem->drop_through)
    c->mem->drop_through = last_slot + 1;
  raise_skip_through(c->mem, last_slot);
  ue_low_phy_monitor_notify(&c->mem->monitor); // posts waiting for room give up
  ue_low_phy_monitor_unlock(&c->mem->monitor);
  notify_host(c);
}

void ue_low_phy_client_tx_symbol(ue_low_phy_client_t *c, uint64_t abs_sym, c16_t *const *data, int nb_ant)
{
  const ue_low_phy_port_t *port = c->port;
  ue_low_phy_monitor_lock(&c->mem->monitor);
  const bool post = wait_for_room(c, abs_sym / port->mem->geo.symbols_per_slot);
  if (post && in_group(c->mem))
    c->mem->stats.tx_symbols++;
  ue_low_phy_monitor_unlock(&c->mem->monitor);
  if (!post)
    return;
  ue_low_phy_entry_hdr_t *e = ue_low_phy_port_tx_entry(port, c->id, abs_sym);
  const int n_re = port->mem->geo.n_rb_ul * 12;
  for (int a = 0; a < nb_ant; a++)
    memcpy(ue_low_phy_entry_tx_data(port, e, a), data[a], n_re * sizeof(c16_t));
  e->tag = abs_sym + 1;
}

int ue_low_phy_client_tx_prach(ue_low_phy_client_t *c, uint64_t abs_slot, int time_offset, int ncp, int dftlen, int reps, const c16_t *bins)
{
  const ue_low_phy_port_t *port = c->port;
  const int max = port->mem->geo.prach_max_samples;
  // The host modulates ncp + reps * dftlen samples into a buffer sized for these bounds.
  AssertFatal(
      dftlen > 0 && dftlen <= max && ncp >= 0 && ncp <= max && reps >= 1 && reps <= UE_LOW_PHY_PRACH_MAX_REPS && time_offset >= 0,
      "invalid PRACH: dftlen %d, ncp %d, reps %d, time_offset %d (max size %d, max reps %d)\n",
      dftlen,
      ncp,
      reps,
      time_offset,
      max,
      UE_LOW_PHY_PRACH_MAX_REPS);
  // Filled holding the monitor: UL jobs of one client may post concurrently. The host
  // reads an entry only once its tag is posted.
  int entry = -1;
  ue_low_phy_monitor_lock(&c->mem->monitor);
  for (int i = 0; i < UE_LOW_PHY_MAX_PRACH && entry < 0; i++) {
    ue_low_phy_entry_hdr_t *e = ue_low_phy_port_prach_entry(port, c->id, i);
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
  ue_low_phy_monitor_unlock(&c->mem->monitor);
  return entry;
}

// Holding the client's monitor.
static void write_record(ue_low_phy_client_t *c, uint64_t abs_slot, uint32_t used_symbols, int prach, uint32_t ta_samples)
{
  ue_low_phy_slot_record_t *r = ue_low_phy_port_record(c->port, c->id, abs_slot);
  r->used_symbols = used_symbols;
  r->prach = prach;
  r->ta_samples = ta_samples;
  r->tag = abs_slot + 1;
}

void ue_low_phy_client_slot_done(ue_low_phy_client_t *c, uint64_t abs_slot, uint32_t used_symbols, int prach, uint32_t ta_samples)
{
  ue_low_phy_monitor_lock(&c->mem->monitor);
  const bool post = wait_for_room(c, abs_slot);
  if (post)
    write_record(c, abs_slot, used_symbols, prach, ta_samples);
  ue_low_phy_monitor_unlock(&c->mem->monitor);
  if (post)
    notify_host(c);
}

bool ue_low_phy_client_try_slot_done(ue_low_phy_client_t *c, uint64_t abs_slot, uint32_t used_symbols, int prach, uint32_t ta_samples)
{
  ue_low_phy_monitor_lock(&c->mem->monitor);
  const bool post = room_for(c, abs_slot) > 0;
  if (post)
    write_record(c, abs_slot, used_symbols, prach, ta_samples);
  ue_low_phy_monitor_unlock(&c->mem->monitor);
  if (post)
    notify_host(c);
  return post;
}
