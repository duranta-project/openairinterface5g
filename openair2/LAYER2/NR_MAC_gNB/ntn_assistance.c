/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
#include "ntn_assistance.h"
#include "nr_mac_gNB.h"
#include "common/ran_context.h"
#include "common/config/config_userapi.h"
#include "common/utils/LOG/log.h"
#include "asn_internal.h"

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <jansson.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

/* Assistance message decoding and validation. */

static const unsigned int validity_seconds[] = {5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 120, 180, 240, 900};

unsigned int ntn_assistance_validity_seconds(unsigned int index)
{
  return index < sizeof(validity_seconds) / sizeof(*validity_seconds) ? validity_seconds[index] : 0;
}

static bool fields(const json_t *object, const char *const *names, size_t count)
{
  if (!json_is_object(object) || json_object_size(object) != count)
    return false;
  for (size_t i = 0; i < count; ++i)
    if (!json_object_get(object, names[i]))
      return false;
  return true;
}

static bool string_is(const json_t *value, const char *text)
{
  return json_is_string(value) && json_string_length(value) == strlen(text) && !strcmp(json_string_value(value), text);
}

static bool decimal_u64(const json_t *value, uint64_t *out)
{
  if (!json_is_string(value))
    return false;
  const char *text = json_string_value(value);
  size_t length = json_string_length(value);
  if (!length || length > 20 || (length > 1 && text[0] == '0'))
    return false;
  uint64_t n = 0;
  for (size_t i = 0; i < length; ++i) {
    if (text[i] < '0' || text[i] > '9')
      return false;
    unsigned int digit = text[i] - '0';
    if (n > (UINT64_MAX - digit) / 10)
      return false;
    n = n * 10 + digit;
  }
  *out = n;
  return true;
}

static bool parse_cell(const json_t *object, ntn_assistance_cell_t *cell)
{
  const char *const names[] = {"plmn", "nci"};
  if (!fields(object, names, 2))
    return false;
  const json_t *plmn = json_object_get(object, "plmn");
  const char *text = json_string_value(plmn);
  size_t length = json_string_length(plmn);
  if (!text || (length != 5 && length != 6))
    return false;
  for (size_t i = 0; i < length; ++i)
    if (text[i] < '0' || text[i] > '9')
      return false;
  if (!decimal_u64(json_object_get(object, "nci"), &cell->nci) || cell->nci >= (UINT64_C(1) << 36))
    return false;
  memcpy(cell->plmn, text, length);
  cell->plmn[length] = '\0';
  return true;
}

static bool quantize(const json_t *value, double step, int32_t minimum, int32_t maximum, int32_t *out)
{
  if (!json_is_number(value))
    return false;
  double physical = json_number_value(value);
  if (!isfinite(physical) || physical < minimum * step || physical > maximum * step)
    return false;
  /* Nearest grid point, half-way values away from zero. Bounds are checked
   * before conversion: an out-of-range physical value is never clipped. */
  double rounded = round(physical / step);
  if (rounded < minimum || rounded > maximum)
    return false;
  *out = (int32_t)rounded;
  return true;
}

static bool vector(const json_t *array, double step, int32_t minimum, int32_t maximum, int32_t out[3])
{
  if (!json_is_array(array) || json_array_size(array) != 3)
    return false;
  for (size_t i = 0; i < 3; ++i)
    if (!quantize(json_array_get(array, i), step, minimum, maximum, out + i))
      return false;
  return true;
}

static const char *parse_update(const json_t *root, ntn_assistance_request_t *request)
{
  const char *const names[] =
      {"version", "type", "request", "cell", "session", "sequence", "subject", "epoch", "ephemeris", "ta", "ul_sync_validity_s"};
  if (!fields(root, names, sizeof(names) / sizeof(*names)))
    return "update_fields";
  if (!decimal_u64(json_object_get(root, "sequence"), &request->sequence) || !request->sequence)
    return "sequence";
  const char *const subject_names[] = {"kind"};
  const json_t *subject = json_object_get(root, "subject");
  if (!fields(subject, subject_names, 1) || !string_is(json_object_get(subject, "kind"), "serving"))
    return "unsupported_subject";

  const char *const epoch_names[] = {"kind", "generation", "subframe"};
  const json_t *epoch = json_object_get(root, "epoch");
  if (!fields(epoch, epoch_names, 3) || !string_is(json_object_get(epoch, "kind"), "cell"))
    return "unsupported_epoch";
  if (!decimal_u64(json_object_get(epoch, "generation"), &request->state.epoch.generation) || !request->state.epoch.generation
      || !decimal_u64(json_object_get(epoch, "subframe"), &request->state.epoch.subframe))
    return "epoch";

  const char *const ephemeris_names[] = {"kind", "position_m", "velocity_mps"};
  const json_t *ephemeris = json_object_get(root, "ephemeris");
  if (!fields(ephemeris, ephemeris_names, 3) || !string_is(json_object_get(ephemeris, "kind"), "ecef"))
    return "unsupported_ephemeris";
  if (!vector(json_object_get(ephemeris, "position_m"), 1.3, -33554432, 33554431, request->state.position)
      || !vector(json_object_get(ephemeris, "velocity_mps"), 0.06, -131072, 131071, request->state.velocity))
    return "ephemeris_range";

  const char *const ta_names[] = {"common_us", "drift_us_per_s", "drift_variant_us_per_s2"};
  const json_t *ta = json_object_get(root, "ta");
  if (!fields(ta, ta_names, 3) || !quantize(json_object_get(ta, "common_us"), 0.004072, 0, 66485757, &request->state.ta_common)
      || !quantize(json_object_get(ta, "drift_us_per_s"), 0.0002, -257303, 257303, &request->state.ta_drift)
      || !quantize(json_object_get(ta, "drift_variant_us_per_s2"), 0.00002, 0, 28949, &request->state.ta_drift_variant))
    return "ta_range";

  const json_t *validity = json_object_get(root, "ul_sync_validity_s");
  if (json_is_integer(validity)) {
    for (size_t i = 0; i < sizeof(validity_seconds) / sizeof(*validity_seconds); ++i) {
      if (json_integer_value(validity) == validity_seconds[i]) {
        request->state.validity_index = i;
        return NULL;
      }
    }
  }
  return "validity";
}

const char *ntn_assistance_parse(const void *data, size_t length, ntn_assistance_request_t *out)
{
  if (!data || !out || !length || length > NTN_ASSISTANCE_MAX_DATAGRAM)
    return "datagram_size";
  json_error_t error;
  json_t *root = json_loadb(data, length, JSON_REJECT_DUPLICATES, &error);
  if (!root)
    return "json";
  ntn_assistance_request_t request = {0};
  const char *failure = "header";
  const json_t *version = json_object_get(root, "version");
  if (!json_is_integer(version) || json_integer_value(version) != NTN_ASSISTANCE_VERSION) {
    failure = "version";
    goto done;
  }
  if (!decimal_u64(json_object_get(root, "request"), &request.request) || !parse_cell(json_object_get(root, "cell"), &request.cell))
    goto done;

  const json_t *type = json_object_get(root, "type");
  if (string_is(type, "hello")) {
    const char *const names[] = {"version", "type", "request", "cell"};
    request.kind = NTN_ASSISTANCE_HELLO;
    failure = fields(root, names, 4) ? NULL : "hello_fields";
    goto done;
  }
  if (!decimal_u64(json_object_get(root, "session"), &request.session) || !request.session) {
    failure = "session";
    goto done;
  }
  if (string_is(type, "update")) {
    request.kind = NTN_ASSISTANCE_UPDATE;
    failure = parse_update(root, &request);
  } else if (string_is(type, "time") || string_is(type, "status")) {
    const char *const names[] = {"version", "type", "request", "cell", "session"};
    request.kind = string_is(type, "time") ? NTN_ASSISTANCE_TIME : NTN_ASSISTANCE_STATUS;
    failure = fields(root, names, 5) ? NULL : "request_fields";
  } else {
    failure = "type";
  }
done:
  json_decref(root);
  if (!failure)
    *out = request;
  return failure;
}

/* SIB19 publication and scheduling. */

#define NTN_SFN_CYCLE_SUBFRAMES 10240U
#define NTN_MAX_EPOCH_AHEAD_SUBFRAMES (NTN_SFN_CYCLE_SUBFRAMES - 1)

typedef struct {
  /* NULL while active: the SCC owns the active ASN. After retirement, this
   * field owns the ASN that was removed from the SCC at that boundary. */
  NR_NTN_Config_r17_t *config;
  uint8_t payload[NR_MAX_SIB_LENGTH / 8];
  int length;
  uint64_t session;
  uint64_t sequence;
  ntn_assistance_epoch_t epoch;
  ntn_assistance_epoch_t scheduled_at;
  atomic_bool scheduled;
} nr_ntn_assistance_candidate_t;

struct nr_ntn_assistance_publisher {
  /* The worker holds this only for pointer/metadata transfer. The scheduler
   * tries it once per SI window and never waits for the worker. */
  pthread_mutex_t mutex;
  NR_NTN_Config_r17_t *ntn_template;
  nr_ntn_assistance_candidate_t *pending;
  nr_ntn_assistance_candidate_t *active;
  nr_ntn_assistance_candidate_t *retired;
  uint64_t latest_session;
  uint64_t latest_sequence;
  ntn_assistance_epoch_t latest_epoch;

  /* Scheduler-owned timeline. Every slot is observed, including slots before
   * the first SI transmission. Contiguous SFN wrap preserves the generation. */
  unsigned int slots_per_frame;
  unsigned int slots_per_subframe;
  unsigned int previous_wrapped_slot;
  bool have_slot;
  bool timeline_failed;
  uint64_t generation;
  uint64_t extended_slot;
  uint64_t discontinuities;
  bool window_open;
  uint64_t window_generation;
  uint64_t window_last_slot;

  /* Atomic scalars make the versioned snapshot data-race-free in C. All use
   * sequential consistency so the worker can verify one complete slot sample.
   * Init requires lock-free atomics on the executing architecture. */
  atomic_uint_fast64_t time_version;
  atomic_uint_fast64_t time_valid;
  atomic_uint_fast64_t time_generation;
  atomic_uint_fast64_t time_subframe;
  atomic_uint_fast64_t time_frame;
  atomic_uint_fast64_t time_slot;
  atomic_uint_fast64_t time_discontinuities;
};

static void free_candidate(nr_ntn_assistance_candidate_t *candidate)
{
  if (!candidate)
    return;
  ASN_STRUCT_FREE(asn_DEF_NR_NTN_Config_r17, candidate->config);
  free(candidate);
}

static bool has_sib19_only_schedule(const nr_cell_sched_t *cell)
{
  const NR_BCCH_DL_SCH_Message_t *message = cell->common_channels.sib1;
  if (!message || !message->message.choice.c1)
    return false;
  const NR_SIB1_t *sib1 = message->message.choice.c1->choice.systemInformationBlockType1;
  if (!sib1 || !sib1->si_SchedulingInfo || !sib1->nonCriticalExtension || !sib1->nonCriticalExtension->nonCriticalExtension
      || !sib1->nonCriticalExtension->nonCriticalExtension->nonCriticalExtension)
    return false;
  const NR_SIB1_v1700_IEs_t *v17 = sib1->nonCriticalExtension->nonCriticalExtension->nonCriticalExtension;
  const NR_SI_SchedulingInfo_v1700_t *info = v17->si_SchedulingInfo_v1700;
  if (!info || info->schedulingInfoList2_r17.list.count != 1)
    return false;
  const NR_SchedulingInfo2_r17_t *si = info->schedulingInfoList2_r17.list.array[0];
  if (!si || si->si_BroadcastStatus_r17 != NR_SchedulingInfo2_r17__si_BroadcastStatus_r17_broadcasting
      || si->sib_MappingInfo_r17.list.count != 1)
    return false;
  const NR_SIB_TypeInfo_v1700_t *mapping = si->sib_MappingInfo_r17.list.array[0];
  return mapping && mapping->sibType_r17.present == NR_SIB_TypeInfo_v1700__sibType_r17_PR_type1_r17
         && mapping->sibType_r17.choice.type1_r17 == NR_SIB_TypeInfo_v1700__sibType_r17__type1_r17_sibType19;
}

const char *nr_ntn_assistance_publisher_init(nr_cell_sched_t *cell)
{
  if (!cell || cell->ntn_assistance_publisher)
    return "publisher_state";
  const NR_ServingCellConfigCommon_t *scc = cell->common_channels.ServingCellConfigCommon;
  if (!scc || !scc->ext2 || !scc->ext2->ntn_Config_r17)
    return "ntn_not_configured";
  if (!has_sib19_only_schedule(cell))
    return "unsupported_si_schedule";
  const unsigned int slots = cell->frame_structure.numb_slots_frame;
  if (slots != 10 && slots != 20 && slots != 40 && slots != 80 && slots != 160)
    return "numerology";

  nr_ntn_assistance_publisher_t *publisher = calloc(1, sizeof(*publisher));
  if (!publisher)
    return "allocation";
  publisher->active = calloc(1, sizeof(*publisher->active));
  if (!publisher->active) {
    free(publisher);
    return "allocation";
  }
  atomic_init(&publisher->active->scheduled, false);
  atomic_init(&publisher->time_version, 0);
  atomic_init(&publisher->time_valid, 0);
  atomic_init(&publisher->time_generation, 0);
  atomic_init(&publisher->time_subframe, 0);
  atomic_init(&publisher->time_frame, 0);
  atomic_init(&publisher->time_slot, 0);
  atomic_init(&publisher->time_discontinuities, 0);
  if (!atomic_is_lock_free(&publisher->time_version) || !atomic_is_lock_free(&publisher->time_valid)
      || !atomic_is_lock_free(&publisher->time_generation) || !atomic_is_lock_free(&publisher->time_subframe)
      || !atomic_is_lock_free(&publisher->time_frame) || !atomic_is_lock_free(&publisher->time_slot)
      || !atomic_is_lock_free(&publisher->time_discontinuities) || !atomic_is_lock_free(&publisher->active->scheduled)) {
    free_candidate(publisher->active);
    free(publisher);
    return "atomics_not_lock_free";
  }
  if (asn_copy(&asn_DEF_NR_NTN_Config_r17, (void **)&publisher->ntn_template, scc->ext2->ntn_Config_r17) != 0
      || !publisher->ntn_template) {
    ASN_STRUCT_FREE(asn_DEF_NR_NTN_Config_r17, publisher->ntn_template);
    free_candidate(publisher->active);
    free(publisher);
    return "allocation";
  }
  if (pthread_mutex_init(&publisher->mutex, NULL) != 0) {
    ASN_STRUCT_FREE(asn_DEF_NR_NTN_Config_r17, publisher->ntn_template);
    free_candidate(publisher->active);
    free(publisher);
    return "mutex";
  }
  publisher->slots_per_frame = slots;
  publisher->slots_per_subframe = slots / 10;
  publisher->generation = 1;
  cell->ntn_assistance_publisher = publisher;
  return NULL;
}

static void read_time(const nr_ntn_assistance_publisher_t *publisher, nr_ntn_assistance_snapshot_t *snapshot)
{
  uint64_t before, after;
  do {
    before = atomic_load(&publisher->time_version);
    if (before & 1)
      continue;
    snapshot->scheduler_time_valid = atomic_load(&publisher->time_valid);
    snapshot->scheduler_time.generation = atomic_load(&publisher->time_generation);
    snapshot->scheduler_time.subframe = atomic_load(&publisher->time_subframe);
    snapshot->scheduler_frame = atomic_load(&publisher->time_frame);
    snapshot->scheduler_slot = atomic_load(&publisher->time_slot);
    snapshot->discontinuities = atomic_load(&publisher->time_discontinuities);
    after = atomic_load(&publisher->time_version);
  } while ((before & 1) || before != after);
  snapshot->slots_per_frame = publisher->slots_per_frame;
}

static const char *check_epoch(const nr_ntn_assistance_snapshot_t *time, ntn_assistance_epoch_t epoch)
{
  if (!time->scheduler_time_valid)
    return "scheduler_time_unavailable";
  if (epoch.generation != time->scheduler_time.generation)
    return "timeline_generation";
  if (epoch.subframe < time->scheduler_time.subframe)
    return "epoch_past";
  if (epoch.subframe - time->scheduler_time.subframe > NTN_MAX_EPOCH_AHEAD_SUBFRAMES)
    return "epoch_horizon";
  return NULL;
}

const char *nr_ntn_assistance_stage(nr_ntn_assistance_publisher_t *publisher,
                                    uint64_t session,
                                    uint64_t sequence,
                                    const ntn_assistance_state_t *state)
{
  if (!publisher || !state || !session || !sequence)
    return "invalid_argument";
  nr_ntn_assistance_snapshot_t time = {0};
  read_time(publisher, &time);
  const char *error = check_epoch(&time, state->epoch);
  if (error)
    return error;

  nr_ntn_assistance_candidate_t *candidate = calloc(1, sizeof(*candidate));
  if (!candidate)
    return "allocation";
  atomic_init(&candidate->scheduled, false);
  error = nr_prepare_sib19(publisher->ntn_template,
                           state,
                           candidate->payload,
                           sizeof(candidate->payload),
                           &candidate->config,
                           &candidate->length);
  if (error) {
    free_candidate(candidate);
    return error;
  }
  candidate->session = session;
  candidate->sequence = sequence;
  candidate->epoch = state->epoch;

  pthread_mutex_lock(&publisher->mutex);
  read_time(publisher, &time);
  error = check_epoch(&time, state->epoch);
  if (!error && publisher->latest_session == session && sequence <= publisher->latest_sequence)
    error = "stale_sequence";
  nr_ntn_assistance_candidate_t *superseded = NULL;
  nr_ntn_assistance_candidate_t *retired = publisher->retired;
  publisher->retired = NULL;
  if (!error) {
    superseded = publisher->pending;
    publisher->pending = candidate;
    publisher->latest_session = session;
    publisher->latest_sequence = sequence;
    publisher->latest_epoch = state->epoch;
  }
  pthread_mutex_unlock(&publisher->mutex);
  free_candidate(retired);
  free_candidate(superseded);
  if (error)
    free_candidate(candidate);
  return error;
}

static bool epoch_expired(const nr_ntn_assistance_snapshot_t *time, ntn_assistance_epoch_t epoch)
{
  return !time->scheduler_time_valid || epoch.generation != time->scheduler_time.generation
         || epoch.subframe < time->scheduler_time.subframe;
}

void nr_ntn_assistance_snapshot(nr_ntn_assistance_publisher_t *publisher, nr_ntn_assistance_snapshot_t *snapshot)
{
  if (!snapshot)
    return;
  memset(snapshot, 0, sizeof(*snapshot));
  if (!publisher)
    return;
  pthread_mutex_lock(&publisher->mutex);
  read_time(publisher, snapshot);
  snapshot->session = publisher->latest_session;
  snapshot->sequence = publisher->latest_sequence;
  snapshot->epoch = publisher->latest_epoch;
  const nr_ntn_assistance_candidate_t *active = publisher->active;
  if (active->session) {
    snapshot->active_session = active->session;
    snapshot->active_sequence = active->sequence;
    snapshot->active_epoch = active->epoch;
    snapshot->scheduled_at = active->scheduled_at;
    snapshot->active_status = epoch_expired(snapshot, active->epoch) ? NR_NTN_ASSISTANCE_EXPIRED
                              : atomic_load(&active->scheduled)      ? NR_NTN_ASSISTANCE_SCHEDULED
                                                                     : NR_NTN_ASSISTANCE_STAGED;
  }
  if (publisher->latest_session) {
    snapshot->status = epoch_expired(snapshot, publisher->latest_epoch) ? NR_NTN_ASSISTANCE_EXPIRED
                       : active->session == publisher->latest_session && active->sequence == publisher->latest_sequence
                           ? snapshot->active_status
                           : NR_NTN_ASSISTANCE_STAGED;
  }
  nr_ntn_assistance_candidate_t *retired = publisher->retired;
  publisher->retired = NULL;
  pthread_mutex_unlock(&publisher->mutex);
  free_candidate(retired);
}

void nr_ntn_assistance_publisher_destroy(nr_cell_sched_t *cell)
{
  if (!cell || !cell->ntn_assistance_publisher)
    return;
  nr_ntn_assistance_publisher_t *publisher = cell->ntn_assistance_publisher;
  cell->ntn_assistance_publisher = NULL;
  free_candidate(publisher->pending);
  free_candidate(publisher->active);
  free_candidate(publisher->retired);
  ASN_STRUCT_FREE(asn_DEF_NR_NTN_Config_r17, publisher->ntn_template);
  pthread_mutex_destroy(&publisher->mutex);
  free(publisher);
}

void nr_ntn_assistance_tick(nr_ntn_assistance_publisher_t *publisher, unsigned int frame, unsigned int slot)
{
  if (!publisher)
    return;
  const unsigned int cycle_slots = 1024 * publisher->slots_per_frame;
  const unsigned int wrapped_slot = frame * publisher->slots_per_frame + slot;
  if (frame >= 1024 || slot >= publisher->slots_per_frame) {
    publisher->have_slot = false;
    publisher->window_open = false;
    publisher->timeline_failed = true;
  } else if (!publisher->timeline_failed) {
    if (!publisher->have_slot) {
      publisher->extended_slot = wrapped_slot;
      publisher->have_slot = true;
    } else if (wrapped_slot != (publisher->previous_wrapped_slot + 1) % cycle_slots) {
      if (publisher->generation == UINT64_MAX || publisher->discontinuities == UINT64_MAX) {
        publisher->timeline_failed = true;
      } else {
        publisher->generation++;
        publisher->discontinuities++;
        publisher->extended_slot = wrapped_slot;
      }
      publisher->window_open = false;
    } else if (publisher->extended_slot == UINT64_MAX) {
      publisher->timeline_failed = true;
      publisher->window_open = false;
    } else {
      publisher->extended_slot++;
    }
    publisher->previous_wrapped_slot = wrapped_slot;
  }

  atomic_fetch_add(&publisher->time_version, 1);
  atomic_store(&publisher->time_valid, publisher->have_slot && !publisher->timeline_failed);
  atomic_store(&publisher->time_generation, publisher->generation);
  atomic_store(&publisher->time_subframe, publisher->extended_slot / publisher->slots_per_subframe);
  atomic_store(&publisher->time_frame, frame);
  atomic_store(&publisher->time_slot, slot);
  atomic_store(&publisher->time_discontinuities, publisher->discontinuities);
  atomic_fetch_add(&publisher->time_version, 1);
}

static bool covers_window(const nr_ntn_assistance_publisher_t *publisher,
                          const nr_ntn_assistance_candidate_t *candidate,
                          uint64_t last_slot)
{
  const uint64_t now = publisher->extended_slot / publisher->slots_per_subframe;
  return candidate->session && candidate->epoch.generation == publisher->generation && candidate->epoch.subframe >= now
         && candidate->epoch.subframe - now <= NTN_MAX_EPOCH_AHEAD_SUBFRAMES
         && candidate->epoch.subframe >= last_slot / publisher->slots_per_subframe;
}

bool nr_ntn_assistance_si_occasion(nr_ntn_assistance_publisher_t *publisher,
                                   nr_cell_sched_t *cell,
                                   bool first_occasion,
                                   unsigned int last_occasion_delta)
{
  if (!publisher)
    return true;
  if (!publisher->have_slot || publisher->timeline_failed)
    return false;
  if (first_occasion) {
    publisher->window_open = false;
    if (UINT64_MAX - publisher->extended_slot < last_occasion_delta)
      return false;
    publisher->window_last_slot = publisher->extended_slot + last_occasion_delta;
    publisher->window_generation = publisher->generation;

    if (pthread_mutex_trylock(&publisher->mutex) == 0) {
      nr_ntn_assistance_candidate_t *candidate = publisher->pending;
      if (candidate && !publisher->retired && covers_window(publisher, candidate, publisher->window_last_slot)) {
        NR_COMMON_channels_t *cc = &cell->common_channels;
        publisher->retired = publisher->active;
        publisher->retired->config = cc->ServingCellConfigCommon->ext2->ntn_Config_r17;
        cc->ServingCellConfigCommon->ext2->ntn_Config_r17 = candidate->config;
        candidate->config = NULL;
        publisher->active = candidate;
        publisher->pending = NULL;
        memcpy(cc->other_sib_bcch_pdu[1], candidate->payload, sizeof(candidate->payload));
        cc->other_sib_bcch_length[1] = candidate->length;
        candidate->scheduled_at =
            (ntn_assistance_epoch_t){publisher->generation, publisher->extended_slot / publisher->slots_per_subframe};
      }
      pthread_mutex_unlock(&publisher->mutex);
    }
    publisher->window_open = covers_window(publisher, publisher->active, publisher->window_last_slot);
  }
  return publisher->window_open && publisher->window_generation == publisher->generation
         && publisher->extended_slot <= publisher->window_last_slot
         && covers_window(publisher, publisher->active, publisher->extended_slot);
}

void nr_ntn_assistance_mark_scheduled(nr_ntn_assistance_publisher_t *publisher)
{
  if (publisher)
    atomic_store(&publisher->active->scheduled, true);
}

/* UDP assistance server and radio-time adapter. */

#define NTN_ASSISTANCE_DEFAULT_PORT 9760
#define NTN_ASSISTANCE_OWNER_IDLE_NS UINT64_C(5000000000)
#define NTN_ASSISTANCE_MIN_PACKET_NS UINT64_C(1000000)

typedef struct {
  nr_cell_sched_t *cell;
  ntn_assistance_cell_t identity;
  uint64_t session;
  uint64_t sequence;
  uint64_t last_request_ns;
  in_port_t owner_port;
  bool claimed;
} ntn_assistance_target_t;

typedef struct {
  pthread_t thread;
  _Atomic bool stop;
  pthread_mutex_t radio_lock;
  nr_ntn_radio_query_t radio_query;
  void *radio_opaque;
  int socket;
  struct in_addr peer;
  size_t count;
  ntn_assistance_target_t targets[NR_MAX_CELLS];
} ntn_assistance_server_t;

static ntn_assistance_server_t *server;
static pthread_mutex_t lifecycle_lock = PTHREAD_MUTEX_INITIALIZER;

static uint64_t monotonic_ns(void)
{
  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now))
    return 0;
  return (uint64_t)now.tv_sec * UINT64_C(1000000000) + now.tv_nsec;
}

static bool new_session(uint64_t *session)
{
  ssize_t result;
  do {
    result = getrandom(session, sizeof(*session), 0);
  } while (result < 0 && errno == EINTR);
  return result == sizeof(*session) && *session != 0;
}

static json_t *decimal(uint64_t value)
{
  char text[21];
  snprintf(text, sizeof(text), "%" PRIu64, value);
  return json_string(text);
}

static json_t *signed_decimal(int64_t value)
{
  char text[21];
  snprintf(text, sizeof(text), "%" PRId64, value);
  return json_string(text);
}

static json_t *epoch_json(ntn_assistance_epoch_t epoch)
{
  return json_pack("{s:s,s:o,s:o}", "kind", "cell", "generation", decimal(epoch.generation), "subframe", decimal(epoch.subframe));
}

static const char *status_name(nr_ntn_assistance_status_t status)
{
  switch (status) {
    case NR_NTN_ASSISTANCE_NONE:
      return "none";
    case NR_NTN_ASSISTANCE_STAGED:
      return "staged";
    case NR_NTN_ASSISTANCE_SCHEDULED:
      return "scheduled";
    case NR_NTN_ASSISTANCE_EXPIRED:
      return "expired";
  }
  return "unavailable";
}

static ntn_assistance_target_t *find_target(ntn_assistance_server_t *service, const ntn_assistance_cell_t *identity)
{
  for (size_t i = 0; i < service->count; ++i) {
    ntn_assistance_target_t *target = service->targets + i;
    if (target->identity.nci == identity->nci && !strcmp(target->identity.plmn, identity->plmn))
      return target;
  }
  return NULL;
}

static json_t *response(const ntn_assistance_request_t *request, const char *reason)
{
  return json_pack("{s:i,s:s,s:o,s:s}",
                   "version",
                   NTN_ASSISTANCE_VERSION,
                   "type",
                   "status",
                   "request",
                   decimal(request->request),
                   "result",
                   reason);
}

static json_t *radio_time_json(ntn_assistance_server_t *service, ntn_assistance_target_t *target)
{
  nr_ntn_assistance_snapshot_t before;
  nr_ntn_assistance_snapshot(target->cell->ntn_assistance_publisher, &before);
  if (!before.scheduler_time_valid)
    return json_null();
  nr_ntn_radio_time_t radio;
  pthread_mutex_lock(&service->radio_lock);
  bool valid = service->radio_query && service->radio_query(service->radio_opaque, &radio);
  pthread_mutex_unlock(&service->radio_lock);
  if (!valid)
    return json_null();
  nr_ntn_assistance_snapshot_t snapshot;
  nr_ntn_assistance_snapshot(target->cell->ntn_assistance_publisher, &snapshot);
  if (!snapshot.scheduler_time_valid || snapshot.scheduler_time.generation != before.scheduler_time.generation || radio.sfn >= 1024)
    return json_null();
  /* The fresh RU anchor may be just behind or ahead of the scheduler across
   * SFN wrap. Do not silently select a different 10.24-second occurrence. */
  int64_t distance = (snapshot.scheduler_time.subframe % 10240 + 10240 - radio.sfn * 10) % 10240;
  if (distance > 5120)
    distance -= 10240;
  if (distance < -200 || distance > 200 || (distance > 0 && snapshot.scheduler_time.subframe < distance)
      || (distance < 0 && snapshot.scheduler_time.subframe > UINT64_MAX + distance))
    return json_null();
  ntn_assistance_epoch_t epoch = snapshot.scheduler_time;
  epoch.subframe = distance >= 0 ? epoch.subframe - distance : epoch.subframe + (-distance);
  return json_pack("{s:o,s:o,s:o,s:o,s:o,s:f,s:o,s:o,s:o,s:s}",
                   "epoch", epoch_json(epoch),
                   "anchor_generation", decimal(radio.generation),
                   "rx_frame_ticks", signed_decimal(radio.rx_frame_ticks),
                   "now_ticks", signed_decimal(radio.now_ticks),
                   "tx_advance_ticks", signed_decimal(radio.tx_advance_ticks),
                   "timestamp_rate_hz", radio.timestamp_rate_hz,
                   "observed_monotonic_ns", decimal(radio.observed_monotonic_ns),
                   "query_before_ns", decimal(radio.query_before_ns),
                   "query_after_ns", decimal(radio.query_after_ns),
                   "reference", "rx_sample_zero");
}

static json_t *handle_request(ntn_assistance_server_t *service,
                              const ntn_assistance_request_t *request,
                              in_port_t port,
                              uint64_t receive_ns)
{
  ntn_assistance_target_t *target = find_target(service, &request->cell);
  if (!target)
    return response(request, "unknown_cell");
  nr_ntn_assistance_snapshot_t snapshot;
  nr_ntn_assistance_snapshot(target->cell->ntn_assistance_publisher, &snapshot);
  if (request->kind == NTN_ASSISTANCE_HELLO) {
    if (target->claimed && target->owner_port != port) {
      bool idle = receive_ns >= target->last_request_ns && receive_ns - target->last_request_ns >= NTN_ASSISTANCE_OWNER_IDLE_NS;
      bool pending = snapshot.status == NR_NTN_ASSISTANCE_STAGED;
      bool active = snapshot.active_status == NR_NTN_ASSISTANCE_SCHEDULED;
      if (!idle || pending || active)
        return response(request, "producer_busy");
      target->claimed = false;
    }
    if (!target->claimed) {
      if (!new_session(&target->session))
        return response(request, "session_unavailable");
      target->sequence = 0;
      target->owner_port = port;
      target->claimed = true;
    }
  } else if (!target->claimed || target->owner_port != port || target->session != request->session) {
    return response(request, "session");
  }

  const char *result = "ok";
  if (request->kind == NTN_ASSISTANCE_UPDATE) {
    if (request->sequence <= target->sequence)
      return response(request, "sequence_not_newer");
    result = nr_ntn_assistance_stage(target->cell->ntn_assistance_publisher, target->session, request->sequence, &request->state);
    if (result)
      return response(request, result);
    target->sequence = request->sequence;
    result = "staged";
    nr_ntn_assistance_snapshot(target->cell->ntn_assistance_publisher, &snapshot);
  }
  target->last_request_ns = receive_ns;
  json_t *reply = response(request, result);
  if (!reply)
    return NULL;
  json_object_set_new(reply, "session", decimal(target->session));
  json_object_set_new(reply, "receive_monotonic_ns", decimal(receive_ns));
  json_object_set_new(reply, "scheduler_time_valid", json_boolean(snapshot.scheduler_time_valid));
  json_object_set_new(reply, "scheduler_epoch", epoch_json(snapshot.scheduler_time));
  json_object_set_new(reply, "time_basis", json_string("mac_scheduler_not_rf_anchor"));
  if (request->kind == NTN_ASSISTANCE_TIME)
    json_object_set_new(reply, "radio_time", radio_time_json(service, target));
  if (request->kind == NTN_ASSISTANCE_STATUS || request->kind == NTN_ASSISTANCE_UPDATE) {
    json_object_set_new(reply, "sequence", decimal(snapshot.sequence));
    json_object_set_new(reply, "state", json_string(status_name(snapshot.status)));
    json_object_set_new(reply, "active_sequence", decimal(snapshot.active_sequence));
    json_object_set_new(reply, "active_state", json_string(status_name(snapshot.active_status)));
    json_object_set_new(reply, "active_epoch", epoch_json(snapshot.active_epoch));
    json_object_set_new(reply, "scheduled_at", epoch_json(snapshot.scheduled_at));
  }
  if (request->kind == NTN_ASSISTANCE_HELLO) {
    json_t *capabilities = json_pack("{s:i,s:[s],s:[s],s:[s],s:i,s:b}",
                                     "max_datagram",
                                     NTN_ASSISTANCE_MAX_DATAGRAM,
                                     "ephemeris",
                                     "ecef",
                                     "epoch",
                                     "cell",
                                     "subject",
                                     "serving",
                                     "owner_idle_ms",
                                     5000,
                                     "absolute_time",
                                     0);
    json_object_set_new(reply, "capabilities", capabilities);
  }
  return reply;
}

static void *receive_updates(void *opaque)
{
  ntn_assistance_server_t *service = opaque;
  uint64_t credit_time = 0;
  unsigned int credits = 8;
  while (!atomic_load(&service->stop)) {
    struct pollfd fd = {.fd = service->socket, .events = POLLIN};
    int ready = poll(&fd, 1, 200);
    if (ready < 0 && errno == EINTR)
      continue;
    if (ready < 0 || (ready > 0 && (fd.revents & (POLLERR | POLLHUP | POLLNVAL)))) {
      LOG_E(NR_MAC, "NTN assistance socket failed; no further updates will be accepted\n");
      break;
    }
    if (!ready)
      continue;
    char data[NTN_ASSISTANCE_MAX_DATAGRAM];
    struct sockaddr_in peer = {0};
    struct iovec buffer = {.iov_base = data, .iov_len = sizeof(data)};
    struct msghdr message = {.msg_name = &peer, .msg_namelen = sizeof(peer), .msg_iov = &buffer, .msg_iovlen = 1};
    ssize_t length = recvmsg(service->socket, &message, MSG_TRUNC | MSG_DONTWAIT);
    uint64_t receive_ns = monotonic_ns();
    if (length <= 0 || length > sizeof(data) || (message.msg_flags & MSG_TRUNC) || peer.sin_family != AF_INET
        || peer.sin_addr.s_addr != service->peer.s_addr || !receive_ns)
      continue;
    if (!credit_time)
      credit_time = receive_ns;
    uint64_t earned = receive_ns >= credit_time ? (receive_ns - credit_time) / NTN_ASSISTANCE_MIN_PACKET_NS : 0;
    if (earned) {
      credits = earned >= 8 - credits ? 8 : credits + earned;
      credit_time = receive_ns;
    }
    if (!credits)
      continue;
    --credits;
    ntn_assistance_request_t request;
    const char *error = ntn_assistance_parse(data, length, &request);
    if (error)
      continue; /* No reply to malformed or unauthenticated uncorrelatable input. */
    json_t *reply = handle_request(service, &request, peer.sin_port, receive_ns);
    if (!reply)
      continue;
    json_object_set_new(reply, "send_monotonic_ns", decimal(monotonic_ns()));
    char encoded[NTN_ASSISTANCE_MAX_DATAGRAM];
    size_t size = json_dumpb(reply, encoded, sizeof(encoded), JSON_COMPACT);
    if (size && size <= sizeof(encoded))
      sendto(service->socket, encoded, size, MSG_DONTWAIT | MSG_NOSIGNAL, (struct sockaddr *)&peer, sizeof(peer));
    json_decref(reply);
  }
  return NULL;
}

static const char *server_start(void)
{
  int enabled = 0, port = NTN_ASSISTANCE_DEFAULT_PORT, allow_remote = 0;
  char *bind_address = NULL, *peer_address = NULL;
  paramdef_t params[] = {
      {"enabled", "Enable external NTN assistance", PARAMFLAG_BOOL, .iptr = &enabled, .defintval = 0, TYPE_INT, 0},
      {"bind_address",
       "Numeric IPv4 management bind address",
       0,
       .strptr = &bind_address,
       .defstrval = "127.0.0.1",
       TYPE_STRING,
       0},
      {"port", "UDP management port", 0, .iptr = &port, .defintval = NTN_ASSISTANCE_DEFAULT_PORT, TYPE_INT, 0},
      {"peer_address", "Only accepted producer IPv4 address", 0, .strptr = &peer_address, .defstrval = "127.0.0.1", TYPE_STRING, 0},
      {"allow_remote",
       "Explicit trusted-management-network opt-in",
       PARAMFLAG_BOOL,
       .iptr = &allow_remote,
       .defintval = 0,
       TYPE_INT,
       0}};
  int count = config_get(config_get_if(), params, sizeof(params) / sizeof(*params), "ntn_assistance");
  if (count < 0)
    return "ntn_assistance configuration unavailable";
  if (!enabled)
    return NULL;
  if (server || port < 1 || port > 65535 || RC.nb_nr_macrlc_inst != 1 || !RC.nrmac || !RC.nrmac[0])
    return "ntn_assistance requires one configured MAC and a valid port";
  struct sockaddr_in local = {.sin_family = AF_INET, .sin_port = htons(port)};
  struct in_addr peer;
  if (!bind_address || !peer_address || inet_pton(AF_INET, bind_address, &local.sin_addr) != 1
      || inet_pton(AF_INET, peer_address, &peer) != 1)
    return "ntn_assistance requires numeric IPv4 addresses";
  if (!allow_remote && ((ntohl(local.sin_addr.s_addr) >> 24) != 127 || (ntohl(peer.s_addr) >> 24) != 127))
    return "ntn_assistance remote access requires allow_remote and a trusted network";

  ntn_assistance_server_t *service = calloc(1, sizeof(*service));
  if (!service)
    return "ntn_assistance allocation failed";
  if (pthread_mutex_init(&service->radio_lock, NULL)) {
    free(service);
    return "ntn_assistance radio lock initialization failed";
  }
  service->socket = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  atomic_init(&service->stop, false);
  service->peer = peer;
  const char *error = "ntn_assistance socket/bind failed";
  if (service->socket < 0 || bind(service->socket, (struct sockaddr *)&local, sizeof(local)))
    goto failed;
  for (int i = 0; i < NR_MAX_CELLS; ++i) {
    nr_cell_sched_t *cell = &RC.nrmac[0]->cells[i];
    NR_ServingCellConfigCommon_t *scc = cell->common_channels.ServingCellConfigCommon;
    if (!scc || !scc->ext2 || !scc->ext2->ntn_Config_r17)
      continue;
    unsigned int mcc = cell->plmn.mcc, mnc = cell->plmn.mnc;
    unsigned int digits = cell->plmn.mnc_digit_length;
    error = "ntn_assistance invalid configured cell identity";
    if (mcc > 999 || (digits != 2 && digits != 3) || mnc > (digits == 2 ? 99 : 999)
        || cell->nr_cellid >= (UINT64_C(1) << 36))
      goto failed;
    ntn_assistance_cell_t identity = {.nci = cell->nr_cellid};
    if (digits == 2)
      snprintf(identity.plmn, sizeof(identity.plmn), "%03u%02u", mcc, mnc);
    else
      snprintf(identity.plmn, sizeof(identity.plmn), "%03u%03u", mcc, mnc);
    error = "ntn_assistance duplicate configured cell identity";
    if (find_target(service, &identity))
      goto failed;
    error = nr_ntn_assistance_publisher_init(cell);
    if (error)
      goto failed;
    ntn_assistance_target_t *target = service->targets + service->count++;
    target->cell = cell;
    target->identity = identity;
  }
  error = "ntn_assistance found no configured NTN cell";
  if (!service->count)
    goto failed;
  error = "ntn_assistance control thread failed";
  /* The control plane must not inherit a real-time main-thread policy. */
  pthread_attr_t attributes;
  if (pthread_attr_init(&attributes))
    goto failed;
  struct sched_param scheduling = {.sched_priority = 0};
  int thread_error = pthread_attr_setinheritsched(&attributes, PTHREAD_EXPLICIT_SCHED);
  if (!thread_error)
    thread_error = pthread_attr_setschedpolicy(&attributes, SCHED_OTHER);
  if (!thread_error)
    thread_error = pthread_attr_setschedparam(&attributes, &scheduling);
  if (!thread_error)
    thread_error = pthread_create(&service->thread, &attributes, receive_updates, service);
  pthread_attr_destroy(&attributes);
  if (thread_error)
    goto failed;
  pthread_mutex_lock(&lifecycle_lock);
  server = service;
  pthread_mutex_unlock(&lifecycle_lock);
  LOG_I(NR_MAC,
        "NTN assistance UDP enabled on %s:%d for %zu cells; peer %s (trusted network, not authentication)\n",
        bind_address,
        port,
        service->count,
        peer_address);
  return NULL;
failed:
  for (size_t i = 0; i < service->count; ++i)
    nr_ntn_assistance_publisher_destroy(service->targets[i].cell);
  if (service->socket >= 0)
    close(service->socket);
  pthread_mutex_destroy(&service->radio_lock);
  free(service);
  return error;
}

const char *nr_ntn_assistance_server_start(void)
{
  /* Startup is serialized by main, before radio workers. Do not hold the
   * lifecycle mutex around config parsing: OAI may invoke fatal shutdown from
   * that same caller on a configuration error. */
  return server_start();
}

bool nr_ntn_assistance_server_needs_radio_time(void)
{
  pthread_mutex_lock(&lifecycle_lock);
  bool needed = server && server->count == 1;
  pthread_mutex_unlock(&lifecycle_lock);
  return needed;
}

bool nr_ntn_assistance_server_set_radio_query(nr_ntn_radio_query_t query, void *opaque)
{
  pthread_mutex_lock(&lifecycle_lock);
  bool available = server && server->count == 1;
  if (available) {
    pthread_mutex_lock(&server->radio_lock);
    server->radio_opaque = opaque;
    server->radio_query = query;
    pthread_mutex_unlock(&server->radio_lock);
  }
  pthread_mutex_unlock(&lifecycle_lock);
  return available;
}

void nr_ntn_assistance_server_stop(void)
{
  pthread_mutex_lock(&lifecycle_lock);
  if (server) {
    atomic_store(&server->stop, true);
    pthread_join(server->thread, NULL);
    close(server->socket);
    pthread_mutex_destroy(&server->radio_lock);
    free(server);
    server = NULL;
  }
  pthread_mutex_unlock(&lifecycle_lock);
}
