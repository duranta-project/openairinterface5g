/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Split 7.1 port: the boundary between one Low-PHY device (the host, owning the
 * radio) and up to SPLIT7_MAX_CLIENTS High-PHY clients (UE instances, threads of
 * the same process). Everything crossing it lives in one block of memory, laid
 * out by offsets.
 *
 * DL: the host demodulates every symbol once into a shared ring; clients only read.
 * UL: a client joins at a TX slot, then completes every slot from there on with
 * a slot record (split7_client_slot_done(): symbols used, PRACH, Timing Advance)
 * until the slot it leaves at. The host transmits a stretch of samples only when
 * every member has completed the slots covering it -- never on a deadline, so the
 * signal depends only on what the clients posted, and a late client makes every
 * client late. Each member is placed at its own Timing Advance; the symbols of the
 * members sharing a TA are summed in frequency domain and modulated once. A slot
 * starting before what is already final (a TA increase) is shortened by dropping its
 * first samples (TS 38.213 4.2); a TA decrease leaves a gap of zeros. Entries are tagged with their absolute symbol (or
 * slot); a client posting more than a ring ahead of the host waits.
 */

#ifndef SPLIT7_PORT_H
#define SPLIT7_PORT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "common/platform_types.h"
#include "split7_wait.h"

#define SPLIT7_MAX_CLIENTS 256 ///< most client slots a port can have (split7_geometry_t.max_clients)
#define SPLIT7_MAX_ANT 8
#define SPLIT7_MAX_PRACH 4
#define SPLIT7_PRACH_MAX_REPS 12 ///< most PRACH sequence repetitions (format B4)

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
  uint32_t max_clients; ///< client slots, 1..SPLIT7_MAX_CLIENTS: the port's size grows with them
} split7_geometry_t;

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
} split7_cell_t;

typedef enum {
  SPLIT7_CLIENT_FREE = 0,
  SPLIT7_CLIENT_OPEN, ///< open, not a member
  SPLIT7_CLIENT_JOINING, ///< asked to join at join_slot
  SPLIT7_CLIENT_MEMBER, ///< every stretch waits for its slot records
  SPLIT7_CLIENT_LEAVING, ///< its last slot is leave_slot; OPEN again once that is transmitted
  SPLIT7_CLIENT_CLOSING, ///< closed; FREE once the host has let go of it (only the host frees)
} split7_client_state_t;

typedef struct {
  uint64_t tx_symbols;
  uint64_t prach;
  uint64_t late_stretches; ///< stretches written late while waiting for this client
  uint64_t ta_clipped; ///< samples dropped where a TA increase overlapped the previous slot
} split7_client_stats_t;

/// One client's side of the port, guarded by monitor: the client changes its state and
/// waits for room, for leaving and for closing; the host accepts joins, confirms leaves
/// and closes, moves host_slot on and notifies. The monitor also guards the client's slot
/// records and the tags of its PRACH entries.
///
/// Entry contents are handed over without a lock, by ownership:
/// - Symbol entry: the client writes it only while it has room (the host is done with
///   that ring position), the host reads it only after taking the slot's record. So a
///   slot's record must be posted after split7_client_tx_symbol() returned for all of
///   its symbols (in the same thread, or one that waited for it).
/// - PRACH entry: the client fills one only while its tag is 0, the host reads one with
///   a posted tag and then sets the tag to 0.
/// The lock taken around the record or the tag orders the contents.
typedef struct {
  split7_monitor_t monitor;
  split7_client_state_t state;
  uint64_t join_slot; ///< requested first TX slot; the host replaces it with the accepted one
  uint32_t join_ta; ///< TA of the group's first slots when this client starts it
  uint64_t leave_slot;
  uint64_t skip_through; ///< slot + 1: slots up to here are complete even without a record
  uint64_t drop_through; ///< slot + 1: posts for slots below are discarded (split7_client_drop_through())
  uint64_t host_slot; ///< first slot the host has not finished with (backpressure)
  bool host_stopped; ///< nothing waits for the host any more
  split7_client_stats_t stats;
} split7_client_info_t;

/// Completion of one TX slot of one client.
typedef struct {
  uint64_t tag; ///< absolute slot + 1
  uint32_t used_symbols; ///< bit s: symbol s was posted
  int32_t prach; ///< PRACH entry posted for this slot, -1 for none
  uint32_t ta_samples; ///< Timing Advance of this slot
  uint8_t pad[44];
} split7_slot_record_t;

typedef struct {
  uint64_t tag; ///< absolute symbol (or slot for PRACH) + 1; 0 means empty
  int32_t time_offset; ///< PRACH: samples from the start of the slot
  int32_t ncp;
  int32_t dftlen;
  int32_t reps;
  uint8_t pad[40];
} split7_entry_hdr_t;

typedef struct {
  uint64_t size;
  split7_geometry_t geo;
  uint64_t rx_off; ///< [nb_rx][ring_symbols][fft_size]
  uint64_t tx_off; ///< [client][ring_symbols] (hdr + [nb_tx][tx_stride])
  uint64_t prach_off; ///< [client][SPLIT7_MAX_PRACH] (hdr + [prach_max_samples])
  uint64_t record_off; ///< [client][ring_slots]
  uint32_t ring_slots; ///< ring_symbols / symbols_per_slot
  uint64_t tx_entry_size;
  uint64_t prach_entry_size;
  uint32_t tx_stride; ///< n_rb_ul * 12 rounded up to 16

  split7_progress_t rx_published; ///< absolute symbols below this one are in the RX ring
  split7_progress_t host_work; ///< posted by clients (records, membership) and host RX progress: the host's TX waits on it

  // Guarded by monitor: cell, sync and timing.
  split7_monitor_t monitor;
  bool anchored; ///< cell is set, once
  split7_cell_t cell;
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

  split7_client_info_t clients[SPLIT7_MAX_CLIENTS];
} split7_port_mem_t;

typedef struct split7_port {
  split7_port_mem_t *mem;
} split7_port_t;

split7_port_t *split7_port_create(const split7_geometry_t *geo);
void split7_port_destroy(split7_port_t *port);

static inline c16_t *split7_port_rx_ring(const split7_port_t *port, int ant)
{
  const split7_geometry_t *g = &port->mem->geo;
  return (c16_t *)((uint8_t *)port->mem + port->mem->rx_off) + (size_t)ant * g->ring_symbols * g->fft_size;
}

static inline split7_entry_hdr_t *split7_port_tx_entry(const split7_port_t *port, int client, uint64_t abs_sym)
{
  const split7_port_mem_t *s = port->mem;
  const uint64_t idx = (uint64_t)client * s->geo.ring_symbols + abs_sym % s->geo.ring_symbols;
  return (split7_entry_hdr_t *)((uint8_t *)s + s->tx_off + idx * s->tx_entry_size);
}

static inline c16_t *split7_entry_tx_data(const split7_port_t *port, split7_entry_hdr_t *e, int ant)
{
  return (c16_t *)(e + 1) + (size_t)ant * port->mem->tx_stride;
}

static inline split7_entry_hdr_t *split7_port_prach_entry(const split7_port_t *port, int client, int idx)
{
  const split7_port_mem_t *s = port->mem;
  return (split7_entry_hdr_t *)((uint8_t *)s + s->prach_off + ((uint64_t)client * SPLIT7_MAX_PRACH + idx) * s->prach_entry_size);
}

static inline split7_slot_record_t *split7_port_record(const split7_port_t *port, int client, uint64_t abs_slot)
{
  const split7_port_mem_t *s = port->mem;
  const uint64_t idx = (uint64_t)client * s->ring_slots + abs_slot % s->ring_slots;
  return (split7_slot_record_t *)((uint8_t *)s + s->record_off) + idx;
}

/* ------------------------------------------------------------------------- */
/* Host API                                                                  */
/* ------------------------------------------------------------------------- */

/// Publishes the cell (once) and the first RX symbol, and answers pending sync requests.
void split7_port_anchor(split7_port_t *port, const split7_cell_t *cell, uint64_t first_symbol);
bool split7_port_anchored(split7_port_t *port);
/// The newest sync request if it differs from *served (then updated), else false.
bool split7_port_take_sync_request(split7_port_t *port, uint32_t *served, int *expected_pci, uint32_t *timeout_ms);
/// Answers sync request req: status 0 anchored, < 0 failed.
void split7_port_answer_sync(split7_port_t *port, uint32_t req, int status);
void split7_port_set_timing_master(split7_port_t *port, int client);
/// The timing shift accumulated since the last call.
int32_t split7_port_take_timing_shift(split7_port_t *port);
/// Releases clients waiting for the host, for good.
void split7_port_stop(split7_port_t *port);

/* ------------------------------------------------------------------------- */
/* Client API                                                                */
/* ------------------------------------------------------------------------- */

typedef struct {
  split7_port_t *port;
  int id;
  split7_client_info_t *mem;
  c16_t *rx_ring[SPLIT7_MAX_ANT]; ///< FD symbol ring per antenna, indexed abs_sym % ring_symbols
} split7_client_t;

/// Claims a client slot. Returns 0, or -1 when all slots are taken.
int split7_client_open(split7_port_t *port, split7_client_t *c);
/// Releases the slot; a member is dropped at once. Returns once the host let go of it.
void split7_client_close(split7_client_t *c);

/// Asks the host for a cell anchor (expected_pci -1: any). Returns the request id.
uint32_t split7_client_request_sync(split7_client_t *c, int expected_pci, uint32_t timeout_ms);
/// 1 anchored (cell filled), 0 pending after timeout_us, -1 the search for this request
/// failed or the host stopped.
int split7_client_sync_result(split7_client_t *c, uint32_t req, split7_cell_t *cell, int timeout_us);

/// Next absolute slot not yet completely published.
uint64_t split7_client_next_published_slot(const split7_client_t *c);
/// 0 when abs_slot is in the RX ring, 1 on timeout, -1 when it already left the ring,
/// -2 when the host stopped.
int split7_client_wait_slot(split7_client_t *c, uint64_t abs_slot, int timeout_us);
/// Copies published symbol abs_sym of each antenna (fft_size REs) to dst[ant]. False when
/// the host overwrote it (before or during the copy): dst holds no usable data then.
bool split7_client_rx_symbol(split7_client_t *c, uint64_t abs_sym, c16_t *const *dst, int nb_ant);

/// Asks to become a member from first_tx_slot on (or the first slot the host can still
/// take, if later). Records for earlier slots are ignored. A leaving client first waits
/// until it has left. False (nothing asked) when the host stopped or c is already a member.
bool split7_client_join(split7_client_t *c, uint64_t first_tx_slot, uint32_t ta_samples);
/// Ends membership after last_slot. Slots up to last_slot without a record count as
/// empty, so nothing may be posted for them afterwards. Returns once the host is done
/// with the client, or after timeout_us (false).
bool split7_client_leave(split7_client_t *c, uint64_t last_slot, int timeout_us);
/// Declares slots up to last_slot complete; those without a record are empty.
void split7_client_skip_through(split7_client_t *c, uint64_t last_slot);
/// Gives up slots up to last_slot: declared complete (as skip_through), and posts for
/// them -- including ones waiting for room right now -- are discarded. For a client
/// whose TX fell further behind than its own pipeline can hold. A symbol post already
/// past its wait still copies its data: stop the posting threads before reusing the
/// ring positions of the slots given up.
void split7_client_drop_through(split7_client_t *c, uint64_t last_slot);
/// Steers the shared RX/TX boundary (monolithic shiftForNextFrame convention:
/// positive moves the boundary earlier). Ignored unless c leads the group.
void split7_client_adjust_timing(split7_client_t *c, int32_t shift_samples);
bool split7_client_is_timing_master(const split7_client_t *c);
split7_client_stats_t split7_client_stats(const split7_client_t *c);

/// Posts one symbol: data[ant] holds n_rb_ul * 12 REs, FFT shifted, unrotated.
/// Waits while abs_sym is more than a ring ahead of what the host has transmitted.
void split7_client_tx_symbol(split7_client_t *c, uint64_t abs_sym, c16_t *const *data, int nb_ant);
/// Posts a PRACH window: bins (dftlen, native layout) modulated with ncp CP and reps
/// (at most SPLIT7_PRACH_MAX_REPS) repetitions,
/// starting time_offset samples after the start of abs_slot. Returns the entry for the slot record,
/// or -1 if all PRACH entries are busy.
int split7_client_tx_prach(split7_client_t *c,
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
void split7_client_slot_done(split7_client_t *c, uint64_t abs_slot, uint32_t used_symbols, int prach, uint32_t ta_samples);
/// As split7_client_slot_done(), but never waits: false (nothing written) without room.
bool split7_client_try_slot_done(split7_client_t *c, uint64_t abs_slot, uint32_t used_symbols, int prach, uint32_t ta_samples);

#endif
