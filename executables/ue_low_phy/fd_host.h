/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * UE Low-PHY port host without radio or OFDM, for a simulator writing each client's DL and
 * reading each member's UL in frequency domain. A UL slot taken by its deadline without a
 * record is silence (stats.late_stretches), so a slow client never holds back the others.
 * One thread per host; per slot: update(), take_slot() per client, finish_slot().
 */

#ifndef UE_LOW_PHY_FD_HOST_H
#define UE_LOW_PHY_FD_HOST_H

#include <stdbool.h>
#include <stdint.h>
#include "port.h"

typedef struct ue_low_phy_fd_host ue_low_phy_fd_host_t;

/// geo needs dl_per_client. name: shared memory object for clients in other processes, NULL
/// for a port in process memory.
ue_low_phy_fd_host_t *ue_low_phy_fd_host_create(const char *name, const ue_low_phy_geometry_t *geo);
void ue_low_phy_fd_host_destroy(ue_low_phy_fd_host_t *h);
ue_low_phy_port_t *ue_low_phy_fd_host_port(ue_low_phy_fd_host_t *h);

/// Accepts joins (at the slot asked for, or the first slot not finished yet if later) and
/// lets leaving and closing clients go.
void ue_low_phy_fd_host_update(ue_low_phy_fd_host_t *h);

/// True when client c has an open slot: it reads DL, member or not.
bool ue_low_phy_fd_host_client_open(const ue_low_phy_fd_host_t *h, int c);

/// Where client c's DL symbol abs_sym of antenna ant goes: n_rb_dl * 12 REs, lowest
/// subcarrier first. Written before ue_low_phy_fd_host_publish() covers abs_sym.
c16_t *ue_low_phy_fd_host_dl_symbol(ue_low_phy_fd_host_t *h, int c, int ant, uint64_t abs_sym);

/// Makes the DL symbols below end visible to the clients.
void ue_low_phy_fd_host_publish(ue_low_phy_fd_host_t *h, uint64_t end);

typedef struct {
  bool member; ///< c transmits in this slot
  uint32_t used_symbols; ///< bit s: symbol s posted (ue_low_phy_fd_host_ul_symbol())
  const ue_low_phy_entry_hdr_t *prach; ///< PRACH window of the slot, or NULL; bins follow the header
} ue_low_phy_fd_ul_slot_t;

/// Client c's UL slot abs_slot. Without deadline: false while c may still complete it.
/// With deadline: always true, a missing record being an empty slot. Slots are taken in
/// order, each slot once per client, abs_slot at most a ring ahead of finished slots.
bool ue_low_phy_fd_host_take_slot(ue_low_phy_fd_host_t *h, int c, uint64_t abs_slot, bool deadline, ue_low_phy_fd_ul_slot_t *out);

/// UL symbol abs_sym of a slot taken with that symbol used: n_rb_ul * 12 REs of antenna ant.
const c16_t *ue_low_phy_fd_host_ul_symbol(ue_low_phy_fd_host_t *h, int c, int ant, uint64_t abs_sym);

/// Every client's slot abs_slot is taken: releases its entries and lets members post a slot further.
void ue_low_phy_fd_host_finish_slot(ue_low_phy_fd_host_t *h, uint64_t abs_slot);

/// Releases every client waiting for the host, for good.
void ue_low_phy_fd_host_stop(ue_low_phy_fd_host_t *h);

#endif
