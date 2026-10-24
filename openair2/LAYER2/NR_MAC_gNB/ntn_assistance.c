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
#include <stdarg.h>
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

static bool decimal_u64(const char *text, uint64_t *out)
{
  if (!text)
    return false;
  size_t length = strlen(text);
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

static bool decimal_number(const char *text, double *out)
{
  const char *p = text;
  if (*p == '+' || *p == '-')
    ++p;
  unsigned int digits = 0;
  while (*p >= '0' && *p <= '9') {
    ++digits;
    ++p;
  }
  if (*p == '.') {
    ++p;
    while (*p >= '0' && *p <= '9') {
      ++digits;
      ++p;
    }
  }
  if (!digits)
    return false;
  if (*p == 'e' || *p == 'E') {
    ++p;
    if (*p == '+' || *p == '-')
      ++p;
    const char *exponent = p;
    while (*p >= '0' && *p <= '9')
      ++p;
    if (p == exponent)
      return false;
  }
  if (*p)
    return false;
  char *end;
  double number = strtod(text, &end);
  if (*end || !isfinite(number))
    return false;
  *out = number;
  return true;
}

static bool quantize(const char *text, double step, int32_t minimum, int32_t maximum, int32_t *out)
{
  double physical;
  if (!decimal_number(text, &physical) || physical < minimum * step || physical > maximum * step)
    return false;
  /* Nearest grid point, half-way values away from zero. Bounds are checked
   * before conversion: an out-of-range physical value is never clipped. */
  double rounded = round(physical / step);
  if (rounded < minimum || rounded > maximum)
    return false;
  *out = (int32_t)rounded;
  return true;
}

static bool vector(char *text, double step, int32_t minimum, int32_t maximum, int32_t out[3])
{
  for (size_t i = 0; i < 3; ++i) {
    char *comma = strchr(text, ',');
    if ((i < 2) != (comma != NULL))
      return false;
    if (comma)
      *comma = '\0';
    if (!quantize(text, step, minimum, maximum, out + i))
      return false;
    if (comma)
      text = comma + 1;
  }
  return true;
}

enum {
  FIELD_TYPE,
  FIELD_REQUEST,
  FIELD_PLMN,
  FIELD_NCI,
  FIELD_SESSION,
  FIELD_SEQUENCE,
  FIELD_SUBJECT,
  FIELD_EPOCH_KIND,
  FIELD_GENERATION,
  FIELD_SUBFRAME,
  FIELD_EPHEMERIS,
  FIELD_POSITION,
  FIELD_VELOCITY,
  FIELD_TA_COMMON,
  FIELD_TA_DRIFT,
  FIELD_TA_VARIANT,
  FIELD_VALIDITY,
  FIELD_COUNT
};

static const char *const field_names[FIELD_COUNT] = {"type",
                                                     "request",
                                                     "cell.plmn",
                                                     "cell.nci",
                                                     "session",
                                                     "sequence",
                                                     "subject.kind",
                                                     "epoch.kind",
                                                     "epoch.generation",
                                                     "epoch.subframe",
                                                     "ephemeris.kind",
                                                     "ephemeris.position_m",
                                                     "ephemeris.velocity_mps",
                                                     "ta.common_us",
                                                     "ta.drift_us_per_s",
                                                     "ta.drift_variant_us_per_s2",
                                                     "ul_sync_validity_s"};

static const char *parse_update(char *const values[FIELD_COUNT], ntn_assistance_request_t *request)
{
  if (!decimal_u64(values[FIELD_SEQUENCE], &request->sequence) || !request->sequence)
    return "sequence";
  if (strcmp(values[FIELD_SUBJECT], "serving"))
    return "unsupported_subject";
  if (strcmp(values[FIELD_EPOCH_KIND], "cell"))
    return "unsupported_epoch";
  if (!decimal_u64(values[FIELD_GENERATION], &request->state.epoch.generation) || !request->state.epoch.generation
      || !decimal_u64(values[FIELD_SUBFRAME], &request->state.epoch.subframe))
    return "epoch";
  if (strcmp(values[FIELD_EPHEMERIS], "ecef"))
    return "unsupported_ephemeris";
  if (!vector(values[FIELD_POSITION], 1.3, -33554432, 33554431, request->state.position)
      || !vector(values[FIELD_VELOCITY], 0.06, -131072, 131071, request->state.velocity))
    return "ephemeris_range";
  if (!quantize(values[FIELD_TA_COMMON], 0.004072, 0, 66485757, &request->state.ta_common)
      || !quantize(values[FIELD_TA_DRIFT], 0.0002, -257303, 257303, &request->state.ta_drift)
      || !quantize(values[FIELD_TA_VARIANT], 0.00002, 0, 28949, &request->state.ta_drift_variant))
    return "ta_range";
  uint64_t validity;
  if (decimal_u64(values[FIELD_VALIDITY], &validity)) {
    for (size_t i = 0; i < sizeof(validity_seconds) / sizeof(*validity_seconds); ++i) {
      if (validity == validity_seconds[i]) {
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
  const unsigned char *bytes = data;
  if (bytes[length - 1] == '\n')
    --length;
  for (size_t i = 0; i < length; ++i)
    if (bytes[i] < ' ' || bytes[i] > '~')
      return "text";
  if (length < 5 || memcmp(data, "NTN2 ", 5))
    return "version";
  char text[NTN_ASSISTANCE_MAX_DATAGRAM + 1];
  memcpy(text, data, length);
  text[length] = '\0';
  char *values[FIELD_COUNT] = {0};
  unsigned int seen = 0;
  char *next = text + 5;
  while (*next) {
    if (*next == ' ') {
      ++next;
      continue;
    }
    char *token = next;
    while (*next && *next != ' ')
      ++next;
    if (*next)
      *next++ = '\0';
    char *equal = strchr(token, '=');
    if (!equal || equal == token || !equal[1] || strchr(equal + 1, '='))
      return "fields";
    *equal++ = '\0';
    unsigned int field = 0;
    while (field < FIELD_COUNT && strcmp(token, field_names[field]))
      ++field;
    if (field == FIELD_COUNT || (seen & (1U << field)))
      return "fields";
    seen |= 1U << field;
    values[field] = equal;
  }
  const unsigned int header = (1U << FIELD_SESSION) - 1;
  ntn_assistance_request_t request = {0};
  if ((seen & header) != header || !decimal_u64(values[FIELD_REQUEST], &request.request))
    return "header";
  const char *plmn = values[FIELD_PLMN];
  size_t plmn_length = strlen(plmn);
  if (plmn_length != 5 && plmn_length != 6)
    return "header";
  for (size_t i = 0; i < plmn_length; ++i)
    if (plmn[i] < '0' || plmn[i] > '9')
      return "header";
  if (!decimal_u64(values[FIELD_NCI], &request.cell.nci) || request.cell.nci >= (UINT64_C(1) << 36))
    return "header";
  memcpy(request.cell.plmn, plmn, plmn_length + 1);
  const char *failure = NULL;
  if (!strcmp(values[FIELD_TYPE], "hello")) {
    request.kind = NTN_ASSISTANCE_HELLO;
    if (seen != header)
      return "hello_fields";
  } else {
    if (!decimal_u64(values[FIELD_SESSION], &request.session) || !request.session)
      return "session";
    if (!strcmp(values[FIELD_TYPE], "update")) {
      request.kind = NTN_ASSISTANCE_UPDATE;
      if (seen != (1U << FIELD_COUNT) - 1)
        return "update_fields";
      failure = parse_update(values, &request);
    } else if (!strcmp(values[FIELD_TYPE], "time") || !strcmp(values[FIELD_TYPE], "status")) {
      request.kind = !strcmp(values[FIELD_TYPE], "time") ? NTN_ASSISTANCE_TIME : NTN_ASSISTANCE_STATUS;
      if (seen != (1U << FIELD_SEQUENCE) - 1)
        return "request_fields";
    } else {
      return "type";
    }
  }
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

typedef struct {
  char data[NTN_ASSISTANCE_MAX_DATAGRAM + 1];
  size_t length;
  bool failed;
} ntn_assistance_reply_t;

static void reply_append(ntn_assistance_reply_t *reply, const char *format, ...)
{
  if (reply->failed)
    return;
  va_list arguments;
  va_start(arguments, format);
  int written = vsnprintf(reply->data + reply->length, sizeof(reply->data) - reply->length, format, arguments);
  va_end(arguments);
  if (written < 0 || (size_t)written >= sizeof(reply->data) - reply->length) {
    reply->failed = true;
    return;
  }
  reply->length += written;
}

static void reply_epoch(ntn_assistance_reply_t *reply, const char *name, ntn_assistance_epoch_t epoch)
{
  reply_append(reply,
               " %s.kind=cell %s.generation=%" PRIu64 " %s.subframe=%" PRIu64,
               name,
               name,
               epoch.generation,
               name,
               epoch.subframe);
}

static ntn_assistance_reply_t response(const ntn_assistance_request_t *request, const char *reason)
{
  ntn_assistance_reply_t reply = {0};
  reply_append(&reply, "NTN2 type=status request=%" PRIu64 " result=%s", request->request, reason);
  return reply;
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

static bool radio_time_observation(ntn_assistance_server_t *service,
                                   ntn_assistance_target_t *target,
                                   nr_ntn_radio_time_t *observation,
                                   ntn_assistance_epoch_t *observed_epoch)
{
  nr_ntn_assistance_snapshot_t before;
  nr_ntn_assistance_snapshot(target->cell->ntn_assistance_publisher, &before);
  if (!before.scheduler_time_valid)
    return false;
  nr_ntn_radio_time_t radio;
  pthread_mutex_lock(&service->radio_lock);
  bool valid = service->radio_query && service->radio_query(service->radio_opaque, &radio);
  pthread_mutex_unlock(&service->radio_lock);
  if (!valid)
    return false;
  nr_ntn_assistance_snapshot_t snapshot;
  nr_ntn_assistance_snapshot(target->cell->ntn_assistance_publisher, &snapshot);
  if (!snapshot.scheduler_time_valid || snapshot.scheduler_time.generation != before.scheduler_time.generation || radio.sfn >= 1024)
    return false;
  /* The fresh RU anchor may be just behind or ahead of the scheduler across
   * SFN wrap. Do not silently select a different 10.24-second occurrence. */
  int64_t distance = (snapshot.scheduler_time.subframe % 10240 + 10240 - radio.sfn * 10) % 10240;
  if (distance > 5120)
    distance -= 10240;
  if (distance < -200 || distance > 200 || (distance > 0 && snapshot.scheduler_time.subframe < distance)
      || (distance < 0 && snapshot.scheduler_time.subframe > UINT64_MAX + distance))
    return false;
  ntn_assistance_epoch_t epoch = snapshot.scheduler_time;
  epoch.subframe = distance >= 0 ? epoch.subframe - distance : epoch.subframe + (-distance);
  *observation = radio;
  *observed_epoch = epoch;
  return true;
}

static ntn_assistance_reply_t handle_request(ntn_assistance_server_t *service,
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
  ntn_assistance_reply_t reply = response(request, result);
  reply_append(&reply,
               " session=%" PRIu64 " receive_monotonic_ns=%" PRIu64 " scheduler_time_valid=%u",
               target->session,
               receive_ns,
               snapshot.scheduler_time_valid);
  reply_epoch(&reply, "scheduler_epoch", snapshot.scheduler_time);
  reply_append(&reply, " time_basis=mac_scheduler_not_rf_anchor");
  if (request->kind == NTN_ASSISTANCE_TIME) {
    nr_ntn_radio_time_t radio;
    ntn_assistance_epoch_t epoch;
    if (radio_time_observation(service, target, &radio, &epoch)) {
      if (!isfinite(radio.timestamp_rate_hz))
        reply.failed = true;
      reply_epoch(&reply, "radio_time.epoch", epoch);
      reply_append(
          &reply,
          " radio_time.anchor_generation=%" PRIu64 " radio_time.rx_frame_ticks=%" PRId64 " radio_time.now_ticks=%" PRId64
          " radio_time.tx_advance_ticks=%" PRId64 " radio_time.timestamp_rate_hz=%.17g radio_time.observed_monotonic_ns=%" PRIu64
          " radio_time.query_before_ns=%" PRIu64 " radio_time.query_after_ns=%" PRIu64 " radio_time.reference=rx_sample_zero",
          radio.generation,
          radio.rx_frame_ticks,
          radio.now_ticks,
          radio.tx_advance_ticks,
          radio.timestamp_rate_hz,
          radio.observed_monotonic_ns,
          radio.query_before_ns,
          radio.query_after_ns);
    } else {
      reply_append(&reply, " radio_time=unavailable");
    }
  }
  if (request->kind == NTN_ASSISTANCE_STATUS || request->kind == NTN_ASSISTANCE_UPDATE) {
    reply_append(&reply,
                 " sequence=%" PRIu64 " state=%s active_sequence=%" PRIu64 " active_state=%s",
                 snapshot.sequence,
                 status_name(snapshot.status),
                 snapshot.active_sequence,
                 status_name(snapshot.active_status));
    reply_epoch(&reply, "active_epoch", snapshot.active_epoch);
    reply_epoch(&reply, "scheduled_at", snapshot.scheduled_at);
  }
  if (request->kind == NTN_ASSISTANCE_HELLO)
    reply_append(&reply,
                 " capabilities.max_datagram=%u capabilities.ephemeris=ecef capabilities.epoch=cell"
                 " capabilities.subject=serving capabilities.owner_idle_ms=5000 capabilities.absolute_time=0",
                 NTN_ASSISTANCE_MAX_DATAGRAM);
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
    ntn_assistance_reply_t reply = handle_request(service, &request, peer.sin_port, receive_ns);
    reply_append(&reply, " send_monotonic_ns=%" PRIu64, monotonic_ns());
    if (!reply.failed && reply.length)
      sendto(service->socket, reply.data, reply.length, MSG_DONTWAIT | MSG_NOSIGNAL, (struct sockaddr *)&peer, sizeof(peer));
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
