/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * UE Low-PHY port: the boundary between one Low-PHY host and up to
 * UE_LOW_PHY_MAX_CLIENTS High-PHY clients (UE instances). Everything crossing it lives
 * in one block of memory, laid out by offsets: process memory when the host and its
 * clients are threads of one process, a named shared memory object otherwise
 * (ue_low_phy_port_create_shared(), ue_low_phy_port_attach()).
 *
 * Time: absolute symbols, counted by the host. The time anchor maps them to 3GPP time.
 *
 * DL: a host owning a radio demodulates every symbol once into a ring shared by all
 * clients. A host simulating the clients' channels (dl_per_client) writes one ring per
 * client, holding the n_rb_dl * 12 REs of each symbol. Clients only read.
 * UL: a client joins at a TX slot, then completes every slot from there on with
 * a slot record (ue_low_phy_client_slot_done(): symbols used, PRACH, Timing Advance)
 * until the slot it leaves at. The host transmits a stretch of samples only when
 * every member has completed the slots covering it -- never on a deadline, so the
 * signal depends only on what the clients posted, and a late client makes every
 * client late. Each member is placed at its own Timing Advance; the symbols of the
 * members sharing a TA are summed in frequency domain and modulated once. A slot
 * starting before what is already final (a TA increase) is shortened by dropping its
 * first samples (TS 38.213 4.2); a TA decrease leaves a gap of zeros. Entries are tagged with their absolute symbol (or
 * slot); a client posting more than a ring ahead of the host waits.
 */

#ifndef UE_LOW_PHY_PORT_H
#define UE_LOW_PHY_PORT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "common/platform_types.h"
#include "wait.h"

#define UE_LOW_PHY_MAX_CLIENTS 256 ///< most client slots a port can have (ue_low_phy_geometry_t.max_clients)
#define UE_LOW_PHY_MAX_ANT 8
#define UE_LOW_PHY_MAX_PRACH 4
#define UE_LOW_PHY_PRACH_MAX_REPS 12 ///< most PRACH sequence repetitions (format B4)

typedef struct {
  uint32_t fft_size;
  uint32_t n_rb_dl;
  uint32_t n_rb_ul;
  uint32_t nb_rx;
  uint32_t nb_tx;
  uint32_t symbols_per_slot;
  uint32_t slots_per_frame;
  uint32_t ring_symbols; ///< depth of the RX and TX rings, a multiple of 2 frames
  uint32_t prach_max_samples; ///< largest PRACH IDFT size
  uint32_t max_clients; ///< client slots, 1..UE_LOW_PHY_MAX_CLIENTS: the port's size grows with them
  uint32_t dl_per_client; ///< nonzero: one DL ring per client, n_rb_dl * 12 REs per symbol
  uint32_t rx_ring_symbols; ///< depth of the per-client DL rings, a multiple of symbols_per_slot
} ue_low_phy_geometry_t;

typedef struct {
  uint16_t pci;
  uint16_t ssb_start_subcarrier;
  uint8_t ssb_index;
  uint8_t mib[3];
  uint8_t mib_xtra;
  int32_t half_frame_bit;
  int32_t symbol_offset;
  uint64_t mib_abs_frame; ///< absolute frame whose SSB carried mib
  uint64_t first_slot; ///< first absolute slot published after the anchor
} ue_low_phy_cell_t;

typedef enum {
  UE_LOW_PHY_CLIENT_FREE = 0,
  UE_LOW_PHY_CLIENT_OPEN, ///< open, not a member
  UE_LOW_PHY_CLIENT_JOINING, ///< asked to join at join_slot
  UE_LOW_PHY_CLIENT_MEMBER, ///< every stretch waits for its slot records
  UE_LOW_PHY_CLIENT_LEAVING, ///< its last slot is leave_slot; OPEN again once that is transmitted
  UE_LOW_PHY_CLIENT_CLOSING, ///< closed; FREE once the host has let go of it (only the host frees)
} ue_low_phy_client_state_t;

typedef struct {
  uint64_t tx_symbols;
  uint64_t prach;
  uint64_t late_stretches; ///< stretches written late while waiting for this client
  uint64_t ta_clipped; ///< samples dropped where a TA increase overlapped the previous slot
} ue_low_phy_client_stats_t;

/// One client's side of the port, guarded by monitor: the client changes its state and
/// waits for room, for leaving and for closing; the host accepts joins, confirms leaves
/// and closes, moves host_slot on and notifies. The monitor also guards the client's slot
/// records and the tags of its PRACH entries.
///
/// Entry contents are handed over without a lock, by ownership:
/// - Symbol entry: the client writes it only while it has room (the host is done with
///   that ring position), the host reads it only after taking the slot's record. So a
///   slot's record must be posted after ue_low_phy_client_tx_symbol() returned for all of
///   its symbols (in the same thread, or one that waited for it).
/// - PRACH entry: the client fills one only while its tag is 0, the host reads one with
///   a posted tag and then sets the tag to 0.
/// The lock taken around the record or the tag orders the contents.
typedef struct {
  ue_low_phy_monitor_t monitor;
  ue_low_phy_client_state_t state;
  uint64_t join_slot; ///< requested first TX slot; the host replaces it with the accepted one
  uint32_t join_ta; ///< TA of the group's first slots when this client starts it
  uint64_t leave_slot;
  uint64_t skip_through; ///< slot + 1: slots up to here are complete even without a record
  uint64_t drop_through; ///< slot + 1: posts for slots below are discarded (ue_low_phy_client_drop_through())
  uint64_t host_slot; ///< first slot the host has not finished with (backpressure)
  bool host_stopped; ///< nothing waits for the host any more
  ue_low_phy_client_stats_t stats;
} ue_low_phy_client_info_t;

/// Completion of one TX slot of one client.
typedef struct {
  uint64_t tag; ///< absolute slot + 1
  uint32_t used_symbols; ///< bit s: symbol s was posted
  int32_t prach; ///< PRACH entry posted for this slot, -1 for none
  uint32_t ta_samples; ///< Timing Advance of this slot
  uint8_t pad[44];
} ue_low_phy_slot_record_t;

typedef struct {
  uint64_t tag; ///< absolute symbol (or slot for PRACH) + 1; 0 means empty
  int32_t time_offset; ///< PRACH: samples from the start of the slot
  int32_t ncp;
  int32_t dftlen;
  int32_t reps;
  uint8_t pad[40];
} ue_low_phy_entry_hdr_t;

typedef struct {
  uint64_t size;
  ue_low_phy_geometry_t geo;
  uint32_t ready; ///< set last by the creator: the rest is initialized
  uint64_t rx_off; ///< [nb_rx][ring_symbols][fft_size], or with dl_per_client [client][nb_rx][rx_ring_symbols][rx_stride]
  uint32_t rx_ring_symbols; ///< depth of the DL ring(s)
  uint32_t rx_stride; ///< REs per symbol in the DL ring(s)
  uint64_t tx_off; ///< [client][ring_symbols] (hdr + [nb_tx][tx_stride])
  uint64_t prach_off; ///< [client][UE_LOW_PHY_MAX_PRACH] (hdr + [prach_max_samples])
  uint64_t record_off; ///< [client][ring_slots]
  uint32_t ring_slots; ///< ring_symbols / symbols_per_slot
  uint64_t tx_entry_size;
  uint64_t prach_entry_size;
  uint32_t tx_stride; ///< n_rb_ul * 12 rounded up to 16

  ue_low_phy_progress_t rx_published; ///< absolute symbols below this one are in the RX ring
  ue_low_phy_progress_t host_work; ///< posted by clients (records, membership) and host RX progress: the host's TX waits on it

  // Guarded by monitor: cell, sync and timing.
  ue_low_phy_monitor_t monitor;
  bool anchored; ///< cell is set, once
  ue_low_phy_cell_t cell;
  // Sync: clients ask for an anchor; the host answers once it has one.
  uint32_t sync_req; ///< request counter
  int32_t sync_expected_pci;
  uint32_t sync_timeout_ms;
  uint32_t sync_done; ///< last request answered
  int32_t sync_status; ///< 0 success, < 0 failure of the last search
  // Downlink timing tracking: one member (the lowest id) steers the shared RX/TX boundary.
  int32_t timing_master; ///< its client id (set by the host), -1 for none
  int32_t timing_shift; ///< accumulated request, monolithic shiftForNextFrame convention
  bool stopped; ///< the host stopped: sync requests are never answered
  // 3GPP time: absolute symbol a is symbol time_symbol0 + a counted from hyper frame 0,
  // frame 0, slot 0, symbol 0.
  bool time_anchored;
  uint64_t time_symbol0;

  ue_low_phy_client_info_t clients[UE_LOW_PHY_MAX_CLIENTS];
} ue_low_phy_port_mem_t;

typedef enum {
  UE_LOW_PHY_PORT_PRIVATE, ///< process memory
  UE_LOW_PHY_PORT_SHARED_OWNER, ///< shared memory object created by this process
  UE_LOW_PHY_PORT_SHARED_ATTACHED, ///< shared memory object of another process
} ue_low_phy_port_kind_t;

typedef struct ue_low_phy_port {
  ue_low_phy_port_mem_t *mem;
  ue_low_phy_port_kind_t kind;
  char name[64];
} ue_low_phy_port_t;

ue_low_phy_port_t *ue_low_phy_port_create(const ue_low_phy_geometry_t *geo);
/// Creates the port as shared memory object name (shm_open() naming), replacing a stale one.
ue_low_phy_port_t *ue_low_phy_port_create_shared(const char *name, const ue_low_phy_geometry_t *geo);
/// Maps the port another process created as name, waiting up to timeout_ms for it to be
/// ready. NULL on timeout.
ue_low_phy_port_t *ue_low_phy_port_attach(const char *name, int timeout_ms);
/// Unmaps an attached port; destroys a created one (and unlinks its name).
void ue_low_phy_port_destroy(ue_low_phy_port_t *port);

/// Shared DL ring of antenna ant (without dl_per_client): [ring_symbols][fft_size].
static inline c16_t *ue_low_phy_port_rx_ring(const ue_low_phy_port_t *port, int ant)
{
  const ue_low_phy_geometry_t *g = &port->mem->geo;
  return (c16_t *)((uint8_t *)port->mem + port->mem->rx_off) + (size_t)ant * g->ring_symbols * g->fft_size;
}

/// DL ring of client and antenna ant (dl_per_client): [rx_ring_symbols][rx_stride], the
/// n_rb_dl * 12 REs of a symbol from the lowest subcarrier up.
static inline c16_t *ue_low_phy_port_client_rx_ring(const ue_low_phy_port_t *port, int client, int ant)
{
  const ue_low_phy_port_mem_t *s = port->mem;
  const size_t ring = (size_t)s->rx_ring_symbols * s->rx_stride;
  return (c16_t *)((uint8_t *)s + s->rx_off) + ((size_t)client * s->geo.nb_rx + ant) * ring;
}

/// 3GPP time of a 3GPP symbol count (time_symbol0 + absolute symbol).
typedef struct {
  uint64_t hfn;
  int frame;
  int slot;
  int symbol;
} ue_low_phy_3gpp_time_t;

static inline ue_low_phy_3gpp_time_t ue_low_phy_3gpp_time(const ue_low_phy_geometry_t *g, uint64_t symbol_3gpp)
{
  const uint64_t symbols_per_frame = (uint64_t)g->slots_per_frame * g->symbols_per_slot;
  const uint64_t frames = symbol_3gpp / symbols_per_frame;
  const uint64_t in_frame = symbol_3gpp % symbols_per_frame;
  return (ue_low_phy_3gpp_time_t){.hfn = frames / 1024,
                                  .frame = (int)(frames % 1024),
                                  .slot = (int)(in_frame / g->symbols_per_slot),
                                  .symbol = (int)(in_frame % g->symbols_per_slot)};
}

static inline uint64_t ue_low_phy_3gpp_symbol(const ue_low_phy_geometry_t *g, uint64_t hfn, int frame, int slot, int symbol)
{
  return ((hfn * 1024 + frame) * g->slots_per_frame + slot) * g->symbols_per_slot + symbol;
}

static inline ue_low_phy_entry_hdr_t *ue_low_phy_port_tx_entry(const ue_low_phy_port_t *port, int client, uint64_t abs_sym)
{
  const ue_low_phy_port_mem_t *s = port->mem;
  const uint64_t idx = (uint64_t)client * s->geo.ring_symbols + abs_sym % s->geo.ring_symbols;
  return (ue_low_phy_entry_hdr_t *)((uint8_t *)s + s->tx_off + idx * s->tx_entry_size);
}

static inline c16_t *ue_low_phy_entry_tx_data(const ue_low_phy_port_t *port, ue_low_phy_entry_hdr_t *e, int ant)
{
  return (c16_t *)(e + 1) + (size_t)ant * port->mem->tx_stride;
}

static inline ue_low_phy_entry_hdr_t *ue_low_phy_port_prach_entry(const ue_low_phy_port_t *port, int client, int idx)
{
  const ue_low_phy_port_mem_t *s = port->mem;
  return (ue_low_phy_entry_hdr_t *)((uint8_t *)s + s->prach_off + ((uint64_t)client * UE_LOW_PHY_MAX_PRACH + idx) * s->prach_entry_size);
}

static inline ue_low_phy_slot_record_t *ue_low_phy_port_record(const ue_low_phy_port_t *port, int client, uint64_t abs_slot)
{
  const ue_low_phy_port_mem_t *s = port->mem;
  const uint64_t idx = (uint64_t)client * s->ring_slots + abs_slot % s->ring_slots;
  return (ue_low_phy_slot_record_t *)((uint8_t *)s + s->record_off) + idx;
}

/* ------------------------------------------------------------------------- */
/* Host API                                                                  */
/* ------------------------------------------------------------------------- */

/// Publishes the time anchor: absolute symbol 0 is 3GPP symbol symbol0 (ue_low_phy_3gpp_symbol()).
void ue_low_phy_port_set_time(ue_low_phy_port_t *port, uint64_t symbol0);
/// Publishes the cell (once) and the first RX symbol, and answers pending sync requests.
/// The absolute frames of the cell's host are SFNs (abs_frame % 1024): the time anchor is 0.
void ue_low_phy_port_anchor(ue_low_phy_port_t *port, const ue_low_phy_cell_t *cell, uint64_t first_symbol);
bool ue_low_phy_port_anchored(ue_low_phy_port_t *port);
/// The newest sync request if it differs from *served (then updated), else false.
bool ue_low_phy_port_take_sync_request(ue_low_phy_port_t *port, uint32_t *served, int *expected_pci, uint32_t *timeout_ms);
/// Answers sync request req: status 0 anchored, < 0 failed.
void ue_low_phy_port_answer_sync(ue_low_phy_port_t *port, uint32_t req, int status);
void ue_low_phy_port_set_timing_master(ue_low_phy_port_t *port, int client);
/// The timing shift accumulated since the last call.
int32_t ue_low_phy_port_take_timing_shift(ue_low_phy_port_t *port);
/// Releases clients waiting for the host, for good.
void ue_low_phy_port_stop(ue_low_phy_port_t *port);

/* ------------------------------------------------------------------------- */
/* Client API                                                                */
/* ------------------------------------------------------------------------- */

typedef struct {
  ue_low_phy_port_t *port;
  int id;
  ue_low_phy_client_info_t *mem;
  c16_t *rx_ring[UE_LOW_PHY_MAX_ANT]; ///< FD symbol ring per antenna, indexed abs_sym % ring_symbols
} ue_low_phy_client_t;

/// Claims a client slot. Returns 0, or -1 when all slots are taken.
int ue_low_phy_client_open(ue_low_phy_port_t *port, ue_low_phy_client_t *c);
/// Releases the slot; a member is dropped at once. Returns once the host let go of it.
void ue_low_phy_client_close(ue_low_phy_client_t *c);

/// Asks the host for a cell anchor (expected_pci -1: any). Returns the request id.
uint32_t ue_low_phy_client_request_sync(ue_low_phy_client_t *c, int expected_pci, uint32_t timeout_ms);
/// 1 anchored (cell filled), 0 pending after timeout_us, -1 the search for this request
/// failed or the host stopped.
int ue_low_phy_client_sync_result(ue_low_phy_client_t *c, uint32_t req, ue_low_phy_cell_t *cell, int timeout_us);

/// Waits up to timeout_us for the time anchor: true with *symbol0 set, false on timeout or
/// when the host stopped.
bool ue_low_phy_client_time(ue_low_phy_client_t *c, uint64_t *symbol0, int timeout_us);

/// Next absolute slot not yet completely published.
uint64_t ue_low_phy_client_next_published_slot(const ue_low_phy_client_t *c);
/// 0 when abs_slot is in the RX ring, 1 on timeout, -1 when it already left the ring,
/// -2 when the host stopped.
int ue_low_phy_client_wait_slot(ue_low_phy_client_t *c, uint64_t abs_slot, int timeout_us);
/// Copies symbol abs_sym of each antenna to dst[ant], FFT shifted as nr_symbol_fep() leaves
/// it (with dl_per_client, zeros after the REs). False if the host overwrote it.
bool ue_low_phy_client_rx_symbol(ue_low_phy_client_t *c, uint64_t abs_sym, c16_t *const *dst, int nb_ant);

/// Asks to become a member from first_tx_slot on (or the first slot the host can still
/// take, if later). Records for earlier slots are ignored. A leaving client first waits
/// until it has left. False (nothing asked) when the host stopped or c is already a member.
bool ue_low_phy_client_join(ue_low_phy_client_t *c, uint64_t first_tx_slot, uint32_t ta_samples);
/// Ends membership after last_slot. Slots up to last_slot without a record count as
/// empty, so nothing may be posted for them afterwards. Returns once the host is done
/// with the client, or after timeout_us (false).
bool ue_low_phy_client_leave(ue_low_phy_client_t *c, uint64_t last_slot, int timeout_us);
/// Declares slots up to last_slot complete; those without a record are empty.
void ue_low_phy_client_skip_through(ue_low_phy_client_t *c, uint64_t last_slot);
/// Gives up slots up to last_slot: declared complete (as skip_through), and posts for
/// them -- including ones waiting for room right now -- are discarded. For a client
/// whose TX fell further behind than its own pipeline can hold. A symbol post already
/// past its wait still copies its data: stop the posting threads before reusing the
/// ring positions of the slots given up.
void ue_low_phy_client_drop_through(ue_low_phy_client_t *c, uint64_t last_slot);
/// Steers the shared RX/TX boundary (monolithic shiftForNextFrame convention:
/// positive moves the boundary earlier). Ignored unless c leads the group.
void ue_low_phy_client_adjust_timing(ue_low_phy_client_t *c, int32_t shift_samples);
bool ue_low_phy_client_is_timing_master(const ue_low_phy_client_t *c);
ue_low_phy_client_stats_t ue_low_phy_client_stats(const ue_low_phy_client_t *c);

/// Posts one symbol: data[ant] holds n_rb_ul * 12 REs, FFT shifted, unrotated.
/// Waits while abs_sym is more than a ring ahead of what the host has transmitted.
void ue_low_phy_client_tx_symbol(ue_low_phy_client_t *c, uint64_t abs_sym, c16_t *const *data, int nb_ant);
/// Posts a PRACH window: bins (dftlen, native layout) modulated with ncp CP and reps
/// (at most UE_LOW_PHY_PRACH_MAX_REPS) repetitions,
/// starting time_offset samples after the start of abs_slot. Returns the entry for the slot record,
/// or -1 if all PRACH entries are busy.
int ue_low_phy_client_tx_prach(ue_low_phy_client_t *c,
                           uint64_t abs_slot,
                           int time_offset,
                           int ncp,
                           int dftlen,
                           int reps,
                           const c16_t *bins);
/// Completes abs_slot: used_symbols (bit per symbol posted), prach (entry or -1), the slot's TA
/// (used when c leads the group).
/// Slots complete in any order; the host takes them in slot order. Complete a slot as
/// early as its content is known (e.g. one that cannot carry UL when scheduling it):
/// the host ends a burst only once the slot after it is complete.
void ue_low_phy_client_slot_done(ue_low_phy_client_t *c, uint64_t abs_slot, uint32_t used_symbols, int prach, uint32_t ta_samples);
/// As ue_low_phy_client_slot_done(), but never waits: false (nothing written) without room.
bool ue_low_phy_client_try_slot_done(ue_low_phy_client_t *c, uint64_t abs_slot, uint32_t used_symbols, int prach, uint32_t ta_samples);

#endif
