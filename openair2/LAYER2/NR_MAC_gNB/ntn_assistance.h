/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
#ifndef NTN_ASSISTANCE_H
#define NTN_ASSISTANCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NTN_ASSISTANCE_VERSION 1
#define NTN_ASSISTANCE_MAX_DATAGRAM 1200

typedef enum { NTN_ASSISTANCE_HELLO, NTN_ASSISTANCE_TIME, NTN_ASSISTANCE_UPDATE, NTN_ASSISTANCE_STATUS } ntn_assistance_kind_t;

typedef struct {
  char plmn[7]; /* MCC followed by the two or three MNC digits, including zeros. */
  uint64_t nci; /* 36-bit NR cell identity, not physical cell ID. */
} ntn_assistance_cell_t;

typedef struct {
  uint64_t generation;
  uint64_t subframe; /* Extended 1 ms downlink subframe; independent of numerology. */
} ntn_assistance_epoch_t;

typedef struct {
  ntn_assistance_epoch_t epoch;
  /* TS 38.331 v17.3.0 EphemerisInfo/NTN-Config quantized integers. */
  int32_t position[3]; /* 1.3 m, Earth-centred Earth-fixed. */
  int32_t velocity[3]; /* 0.06 m/s, derivative in that rotating ECEF frame. */
  int32_t ta_common; /* 0.004072 us */
  int32_t ta_drift; /* 0.0002 us/s */
  int32_t ta_drift_variant; /* 0.00002 us/s^2; unsigned field, not arbitrary acceleration. */
  unsigned int validity_index; /* ASN.1 ntn-UlSyncValidityDuration-r17 enumeration. */
} ntn_assistance_state_t;

typedef struct {
  ntn_assistance_kind_t kind;
  uint64_t request;
  ntn_assistance_cell_t cell;
  uint64_t session;
  uint64_t sequence;
  ntn_assistance_state_t state;
} ntn_assistance_request_t;

/* Control-worker only: strict JSON decode and full validation before replacing
 * out. Returns NULL on success, otherwise a static diagnostic code. No state or
 * output is changed on failure. UINT64 fields use canonical decimal strings. */
const char *ntn_assistance_parse(const void *data, size_t length, ntn_assistance_request_t *out);

unsigned int ntn_assistance_validity_seconds(unsigned int index);

/* Optional observation, not a UTC/PPS or calibrated RF-port clock guarantee.
 * The timestamp unit is the backend's configured sample rate. A complete RU
 * RX frame boundary supplies rx_frame_ticks and its modulo-1024 SFN. The
 * backend's current-time read occurs within the two CLOCK_MONOTONIC timestamps.
 * For the same frame label, TX sample zero is rx_frame_ticks - tx_advance_ticks.
 * Physical RF frontend group delay remains a separate calibration quantity. */
typedef struct {
  uint64_t generation;
  uint64_t observed_monotonic_ns;
  uint64_t query_before_ns;
  uint64_t query_after_ns;
  int64_t rx_frame_ticks;
  int64_t now_ticks;
  int64_t tx_advance_ticks;
  double timestamp_rate_hz;
  unsigned int sfn;
} nr_ntn_radio_time_t;

/* Control worker only, after radio readiness and before radio teardown. Output
 * is changed only on success. Unsupported/stale/failing observations return
 * false. No call may reset time, retune or open a second radio instance. */
typedef bool (*nr_ntn_radio_query_t)(void *opaque, nr_ntn_radio_time_t *observation);

typedef struct nr_cell_sched_s nr_cell_sched_t;
typedef struct nr_ntn_assistance_publisher nr_ntn_assistance_publisher_t;

#ifdef ENABLE_NTN_ASSISTANCE
typedef enum {
  NR_NTN_ASSISTANCE_NONE,
  NR_NTN_ASSISTANCE_STAGED,
  NR_NTN_ASSISTANCE_SCHEDULED,
  NR_NTN_ASSISTANCE_EXPIRED
} nr_ntn_assistance_status_t;

typedef struct {
  /* MAC scheduler position only: this is not a physical RF clock mapping. */
  bool scheduler_time_valid;
  ntn_assistance_epoch_t scheduler_time;
  unsigned int scheduler_frame;
  unsigned int scheduler_slot;
  unsigned int slots_per_frame;
  uint64_t discontinuities;

  /* Most recently accepted update; coalescing replaces an older pending update. */
  nr_ntn_assistance_status_t status;
  uint64_t session;
  uint64_t sequence;
  ntn_assistance_epoch_t epoch;

  /* Active update is expired/suppressed once its explicit epoch is past, even
   * if a UE that previously received it still has a valid UL synchronization timer. */
  nr_ntn_assistance_status_t active_status;
  uint64_t active_session;
  uint64_t active_sequence;
  ntn_assistance_epoch_t active_epoch;
  ntn_assistance_epoch_t scheduled_at;
} nr_ntn_assistance_snapshot_t;

/* Startup only, before the worker or scheduler starts. Requires configured NTN
 * and SIB19 scheduling. Installs external ownership only on success. NULL means
 * success; an error leaves the cell and its startup SIB19 unchanged. */
const char *nr_ntn_assistance_publisher_init(nr_cell_sched_t *cell);

/* One serialized control worker per publisher. Full ASN preparation/encoding
 * happens here, outside the scheduler lock. Errors leave the accepted update,
 * authoritative SCC, and broadcast payload unchanged. */
const char *nr_ntn_assistance_stage(nr_ntn_assistance_publisher_t *publisher,
                                    uint64_t session,
                                    uint64_t sequence,
                                    const ntn_assistance_state_t *state);

/* Control worker only. Does not take sched_lock; also reclaims retired ASN. */
void nr_ntn_assistance_snapshot(nr_ntn_assistance_publisher_t *publisher, nr_ntn_assistance_snapshot_t *snapshot);

/* Shutdown only, after the worker and scheduler stop. The active NTN config
 * remains owned by the SCC and is freed by the existing MAC destructor. */
void nr_ntn_assistance_publisher_destroy(nr_cell_sched_t *cell);

/* Scheduler only, with the existing sched_lock held. No allocation/free/ASN
 * encoding or blocking mutex. Called for every slot, including unscheduled SI. */
void nr_ntn_assistance_tick(nr_ntn_assistance_publisher_t *publisher, unsigned int frame, unsigned int slot);

/* Called for each SIB19 occasion. first_occasion is the first SSB of this
 * SI window; last_occasion_delta is the slot distance to its final SSB occasion.
 * Publication is attempted only on first_occasion and the same bytes/config
 * remain active throughout the window. False suppresses this SIB19 occasion. */
bool nr_ntn_assistance_si_occasion(nr_ntn_assistance_publisher_t *publisher,
                                   nr_cell_sched_t *cell,
                                   bool first_occasion,
                                   unsigned int last_occasion_delta);

/* Call after the SIB19 bytes have been copied into this slot's TX request.
 * Scheduled is distinct from RF-transmitted or UE-decoded. */
void nr_ntn_assistance_mark_scheduled(nr_ntn_assistance_publisher_t *publisher);

/* Called after F1/SIB setup and before radio workers are released. The optional
 * ntn_assistance config section defaults to disabled. NULL means success. */
const char *nr_ntn_assistance_server_start(void);

/* Optional one-RU/one-cell adapter. Registration is serialized with queries.
 * Other deployments retain the generic cell-time protocol without claiming a
 * hardware clock observation. Register after device initialization; the query
 * itself must reject requests until streaming has produced a valid anchor. */
bool nr_ntn_assistance_server_needs_radio_time(void);
bool nr_ntn_assistance_server_set_radio_query(nr_ntn_radio_query_t query, void *opaque);

/* Join before stopping/freeing any radio or MAC cell. Idempotent. */
void nr_ntn_assistance_server_stop(void);
#else
static inline void nr_ntn_assistance_publisher_destroy(nr_cell_sched_t *cell)
{
  (void)cell;
}

static inline void nr_ntn_assistance_tick(nr_ntn_assistance_publisher_t *publisher, unsigned int frame, unsigned int slot)
{
  (void)publisher;
  (void)frame;
  (void)slot;
}

static inline bool nr_ntn_assistance_si_occasion(nr_ntn_assistance_publisher_t *publisher,
                                                 nr_cell_sched_t *cell,
                                                 bool first_occasion,
                                                 unsigned int last_occasion_delta)
{
  (void)publisher;
  (void)cell;
  (void)first_occasion;
  (void)last_occasion_delta;
  return true;
}

static inline void nr_ntn_assistance_mark_scheduled(nr_ntn_assistance_publisher_t *publisher)
{
  (void)publisher;
}

static inline const char *nr_ntn_assistance_server_start(void)
{
  return NULL;
}

static inline bool nr_ntn_assistance_server_needs_radio_time(void)
{
  return false;
}

static inline bool nr_ntn_assistance_server_set_radio_query(nr_ntn_radio_query_t query, void *opaque)
{
  (void)query;
  (void)opaque;
  return false;
}

static inline void nr_ntn_assistance_server_stop(void)
{
}
#endif

#endif
