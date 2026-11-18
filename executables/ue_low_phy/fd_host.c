/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "fd_host.h"
#include <stdlib.h>
#include "common/utils/LOG/log.h"
#include "common/utils/assertions.h"

typedef struct {
  bool member;
  uint64_t first_slot;
  uint64_t leave_slot; ///< UINT64_MAX until the client leaves
} member_t;

struct ue_low_phy_fd_host {
  ue_low_phy_port_t *port;
  uint64_t next_slot; ///< first slot not finished
  member_t members[UE_LOW_PHY_MAX_CLIENTS];
};

ue_low_phy_fd_host_t *ue_low_phy_fd_host_create(const char *name, const ue_low_phy_geometry_t *geo)
{
  AssertFatal(geo->dl_per_client, "an FD host writes one DL ring per client\n");
  ue_low_phy_fd_host_t *h = calloc(1, sizeof(*h));
  AssertFatal(h, "OOM\n");
  h->port = name ? ue_low_phy_port_create_shared(name, geo) : ue_low_phy_port_create(geo);
  return h;
}

void ue_low_phy_fd_host_destroy(ue_low_phy_fd_host_t *h)
{
  if (!h)
    return;
  ue_low_phy_port_destroy(h->port);
  free(h);
}

ue_low_phy_port_t *ue_low_phy_fd_host_port(ue_low_phy_fd_host_t *h)
{
  return h->port;
}

// Holding the client's monitor: entries of slots up to through are done with.
static void release_prach(ue_low_phy_fd_host_t *h, int c, uint64_t through)
{
  for (int i = 0; i < UE_LOW_PHY_MAX_PRACH; i++) {
    ue_low_phy_entry_hdr_t *e = ue_low_phy_port_prach_entry(h->port, c, i);
    if (e->tag && e->tag - 1 <= through)
      e->tag = 0;
  }
}

// Holding the client's monitor: a leaving client is open again, a closing one free.
static void member_drop(ue_low_phy_fd_host_t *h, int c, ue_low_phy_client_info_t *cs)
{
  h->members[c].member = false;
  release_prach(h, c, UINT64_MAX);
  if (cs->state == UE_LOW_PHY_CLIENT_LEAVING)
    cs->state = UE_LOW_PHY_CLIENT_OPEN;
  else if (cs->state == UE_LOW_PHY_CLIENT_CLOSING)
    cs->state = UE_LOW_PHY_CLIENT_FREE;
  ue_low_phy_monitor_notify(&cs->monitor);
}

void ue_low_phy_fd_host_update(ue_low_phy_fd_host_t *h)
{
  ue_low_phy_port_mem_t *s = h->port->mem;
  for (int c = 0; c < s->geo.max_clients; c++) {
    ue_low_phy_client_info_t *cs = &s->clients[c];
    member_t *m = &h->members[c];
    ue_low_phy_monitor_lock(&cs->monitor);
    switch (cs->state) {
      case UE_LOW_PHY_CLIENT_CLOSING:
        member_drop(h, c, cs);
        break;
      case UE_LOW_PHY_CLIENT_LEAVING:
        if (!m->member || cs->leave_slot < h->next_slot)
          member_drop(h, c, cs);
        else
          m->leave_slot = cs->leave_slot;
        break;
      case UE_LOW_PHY_CLIENT_JOINING: {
        const uint64_t slot = cs->join_slot > h->next_slot ? cs->join_slot : h->next_slot;
        cs->host_slot = h->next_slot;
        cs->join_slot = slot;
        cs->state = UE_LOW_PHY_CLIENT_MEMBER;
        *m = (member_t){.member = true, .first_slot = slot, .leave_slot = UINT64_MAX};
        ue_low_phy_monitor_notify(&cs->monitor);
        LOG_I(PHY, "[ue-low-phy] client %d joined at slot %lu\n", c, slot);
        break;
      }
      default:
        break;
    }
    ue_low_phy_monitor_unlock(&cs->monitor);
  }
}

bool ue_low_phy_fd_host_client_open(const ue_low_phy_fd_host_t *h, int c)
{
  ue_low_phy_client_info_t *cs = &h->port->mem->clients[c];
  ue_low_phy_monitor_lock(&cs->monitor);
  const bool open = cs->state != UE_LOW_PHY_CLIENT_FREE;
  ue_low_phy_monitor_unlock(&cs->monitor);
  return open;
}

c16_t *ue_low_phy_fd_host_dl_symbol(ue_low_phy_fd_host_t *h, int c, int ant, uint64_t abs_sym)
{
  const ue_low_phy_port_mem_t *s = h->port->mem;
  return ue_low_phy_port_client_rx_ring(h->port, c, ant) + (abs_sym % s->rx_ring_symbols) * s->rx_stride;
}

void ue_low_phy_fd_host_publish(ue_low_phy_fd_host_t *h, uint64_t end)
{
  ue_low_phy_progress_raise(&h->port->mem->rx_published, end);
}

bool ue_low_phy_fd_host_take_slot(ue_low_phy_fd_host_t *h, int c, uint64_t abs_slot, bool deadline, ue_low_phy_fd_ul_slot_t *out)
{
  const member_t *m = &h->members[c];
  *out = (ue_low_phy_fd_ul_slot_t){0};
  if (!m->member || abs_slot < m->first_slot || abs_slot > m->leave_slot)
    return true;
  out->member = true;
  ue_low_phy_client_info_t *cs = &h->port->mem->clients[c];
  ue_low_phy_monitor_lock(&cs->monitor);
  const ue_low_phy_slot_record_t *r = ue_low_phy_port_record(h->port, c, abs_slot);
  bool taken = true;
  if (r->tag == abs_slot + 1) {
    out->used_symbols = r->used_symbols;
    if (r->prach >= 0) {
      const ue_low_phy_entry_hdr_t *e = ue_low_phy_port_prach_entry(h->port, c, r->prach);
      out->prach = e->tag == abs_slot + 1 ? e : NULL;
    }
  } else if (abs_slot >= cs->skip_through) {
    if (deadline)
      cs->stats.late_stretches++;
    else
      taken = false;
  }
  ue_low_phy_monitor_unlock(&cs->monitor);
  return taken;
}

const c16_t *ue_low_phy_fd_host_ul_symbol(ue_low_phy_fd_host_t *h, int c, int ant, uint64_t abs_sym)
{
  ue_low_phy_entry_hdr_t *e = ue_low_phy_port_tx_entry(h->port, c, abs_sym);
  return ue_low_phy_entry_tx_data(h->port, e, ant);
}

void ue_low_phy_fd_host_finish_slot(ue_low_phy_fd_host_t *h, uint64_t abs_slot)
{
  ue_low_phy_port_mem_t *s = h->port->mem;
  for (int c = 0; c < s->geo.max_clients; c++) {
    ue_low_phy_client_info_t *cs = &s->clients[c];
    member_t *m = &h->members[c];
    ue_low_phy_monitor_lock(&cs->monitor);
    if (cs->state != UE_LOW_PHY_CLIENT_FREE) {
      release_prach(h, c, abs_slot);
      if (m->member && m->leave_slot <= abs_slot)
        member_drop(h, c, cs);
      if (cs->host_slot < abs_slot + 1)
        cs->host_slot = abs_slot + 1;
      ue_low_phy_monitor_notify(&cs->monitor);
    }
    ue_low_phy_monitor_unlock(&cs->monitor);
  }
  h->next_slot = abs_slot + 1;
}

void ue_low_phy_fd_host_stop(ue_low_phy_fd_host_t *h)
{
  ue_low_phy_port_stop(h->port);
}
