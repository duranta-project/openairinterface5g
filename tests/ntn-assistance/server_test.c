/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
/* Actual control handler, publisher and JSON replies. Only ASN preparation is
 * stubbed here; publisher_test exercises the real configuration and encoder. */
#include "../../openair2/LAYER2/NR_MAC_gNB/ntn_assistance.c"
#include <assert.h>

static nr_ntn_radio_time_t radio;
static bool generation_jump;
static unsigned int stage_calls;

const char *nr_prepare_sib19(const NR_NTN_Config_r17_t *ntn_template,
                             const ntn_assistance_state_t *state,
                             uint8_t *buffer,
                             size_t capacity,
                             NR_NTN_Config_r17_t **config,
                             int *length)
{
  (void)ntn_template;
  assert(state && buffer && capacity > 0 && config && length);
  stage_calls++;
  buffer[0] = 0;
  *config = NULL;
  *length = 1;
  return NULL;
}

static bool query(void *opaque, nr_ntn_radio_time_t *out)
{
  nr_ntn_assistance_publisher_t *publisher = opaque;
  *out = radio;
  if (generation_jump)
    atomic_fetch_add(&publisher->time_generation, 1);
  return true;
}

static void result_is(json_t *reply, const char *expected)
{
  assert(reply);
  assert(!strcmp(json_string_value(json_object_get(reply, "result")), expected));
  assert(json_dumpb(reply, NULL, 0, JSON_COMPACT) <= NTN_ASSISTANCE_MAX_DATAGRAM);
  json_decref(reply);
}

int main(void)
{
  nr_cell_sched_t cell = {0};
  nr_ntn_assistance_publisher_t *publisher = calloc(1, sizeof(*publisher));
  assert(publisher);
  assert(!pthread_mutex_init(&publisher->mutex, NULL));
  publisher->slots_per_frame = 10;
  publisher->slots_per_subframe = 1;
  publisher->active = calloc(1, sizeof(*publisher->active));
  assert(publisher->active);
  atomic_init(&publisher->active->scheduled, false);
  atomic_init(&publisher->time_version, 0);
  atomic_init(&publisher->time_valid, 1);
  atomic_init(&publisher->time_generation, 1);
  atomic_init(&publisher->time_subframe, 10242);
  atomic_init(&publisher->time_frame, 0);
  atomic_init(&publisher->time_slot, 2);
  atomic_init(&publisher->time_discontinuities, 0);
  cell.ntn_assistance_publisher = publisher;
  ntn_assistance_server_t service = {.count = 1};
  assert(!pthread_mutex_init(&service.radio_lock, NULL));
  ntn_assistance_target_t *target = &service.targets[0];
  target->cell = &cell;
  target->identity = (ntn_assistance_cell_t){.plmn = "20899", .nci = 12345678};
  ntn_assistance_request_t request = {.kind = NTN_ASSISTANCE_HELLO, .request = UINT64_MAX, .cell = target->identity};
  result_is(handle_request(&service, &request, htons(45000), 1), "ok");
  request.session = target->session;
  result_is(handle_request(&service, &request, htons(45001), 2), "producer_busy");
  request.kind = NTN_ASSISTANCE_UPDATE;
  request.sequence = 1;
  request.state.epoch = (ntn_assistance_epoch_t){1, 11000};
  result_is(handle_request(&service, &request, htons(45000), 3), "staged");
  result_is(handle_request(&service, &request, htons(45000), 4), "sequence_not_newer");
  request.sequence = 2;
  request.state.epoch.generation = 2;
  result_is(handle_request(&service, &request, htons(45000), 5), "timeline_generation");
  assert(stage_calls == 1 && target->sequence == 1 && target->last_request_ns == 3);
  request.kind = NTN_ASSISTANCE_HELLO;
  result_is(handle_request(&service, &request, htons(45001), UINT64_C(6000000000)), "producer_busy");

  service.radio_query = query;
  service.radio_opaque = publisher;
  radio = (nr_ntn_radio_time_t){.generation = UINT64_MAX,
                                .sfn = 1023,
                                .observed_monotonic_ns = UINT64_MAX,
                                .query_before_ns = UINT64_MAX,
                                .query_after_ns = UINT64_MAX,
                                .rx_frame_ticks = INT64_MAX,
                                .now_ticks = INT64_MAX,
                                .tx_advance_ticks = INT64_MIN,
                                .timestamp_rate_hz = 122880000};
  request.kind = NTN_ASSISTANCE_TIME;
  json_t *reply = handle_request(&service, &request, htons(45000), UINT64_MAX);
  json_t *epoch = json_object_get(json_object_get(reply, "radio_time"), "epoch");
  assert(!strcmp(json_string_value(json_object_get(epoch, "subframe")), "10230"));
  json_object_set_new(reply, "send_monotonic_ns", decimal(UINT64_MAX));
  result_is(reply, "ok");
  generation_jump = true;
  assert(json_is_null(radio_time_json(&service, target)));
  generation_jump = false;
  radio.sfn = 100;
  assert(json_is_null(radio_time_json(&service, target)));

  /* Both accepted and active states are expired before ownership can transfer. */
  publisher->active->session = publisher->latest_session;
  publisher->active->sequence = publisher->latest_sequence;
  publisher->active->epoch = publisher->latest_epoch;
  atomic_store(&publisher->active->scheduled, true);
  atomic_store(&publisher->time_generation, publisher->latest_epoch.generation);
  atomic_store(&publisher->time_subframe, publisher->latest_epoch.subframe + 1);
  nr_ntn_assistance_snapshot_t expired;
  nr_ntn_assistance_snapshot(publisher, &expired);
  assert(expired.status == NR_NTN_ASSISTANCE_EXPIRED);
  assert(expired.active_status == NR_NTN_ASSISTANCE_EXPIRED);
  target->last_request_ns = 1;
  request.kind = NTN_ASSISTANCE_HELLO;
  result_is(handle_request(&service, &request, htons(45001), UINT64_C(6000000000)), "ok");
  assert(target->sequence == 0 && target->owner_port == htons(45001));
  pthread_mutex_destroy(&service.radio_lock);
  nr_ntn_assistance_publisher_destroy(&cell);
  puts("server handler/ownership/clock reply gate passed");
}
