/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
/* Exercise the real publisher and scheduling arithmetic with
 * generated NR ASN.1 and the production config/encoder objects. Including the
 * publisher exposes its mutex for a deterministic contention case. */
#include "../../openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_bch.c"
#include "../../openair2/LAYER2/NR_MAC_gNB/ntn_assistance.c"
#include "uper_decoder.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

RAN_CONTEXT_t RC;
static log_t quiet_log;
log_t *g_log = &quiet_log;

void exit_function(const char *file, const char *function, const int line,
                   const char *message, const int asserted) {
  fprintf(stderr, "%s:%d %s: %s (%d)\n", file, line, function, message,
          asserted);
  abort();
}

void logRecord_mt(const char *file, const char *function, int line,
                  int component, int level, const char *format, ...) {
  (void)component;
  (void)level;
  fprintf(stderr, "unexpected logger call %s:%d %s: %s\n", file, line, function,
          format);
  abort();
}

void *__real_calloc(size_t count, size_t size);
static unsigned int calloc_failure;
static unsigned int calloc_calls;

void *__wrap_calloc(size_t count, size_t size) {
  calloc_calls++;
  if (calloc_failure && --calloc_failure == 0)
    return NULL;
  return __real_calloc(count, size);
}

static void init_cell(nr_cell_sched_t *cell, unsigned int slots_per_frame) {
  cell->frame_structure.numb_slots_frame = slots_per_frame;
  NR_COMMON_channels_t *cc = &cell->common_channels;
  NR_ServingCellConfigCommon_t *scc = calloc(1, sizeof(*scc));
  assert(scc);
  scc->ext2 = calloc(1, sizeof(*scc->ext2));
  assert(scc->ext2);
  scc->ext2->ntn_Config_r17 = calloc(1, sizeof(*scc->ext2->ntn_Config_r17));
  assert(scc->ext2->ntn_Config_r17);
  NR_NTN_Config_r17_t *ntn = scc->ext2->ntn_Config_r17;
  ntn->cellSpecificKoffset_r17 =
      calloc(1, sizeof(*ntn->cellSpecificKoffset_r17));
  ntn->ntn_UlSyncValidityDuration_r17 =
      calloc(1, sizeof(*ntn->ntn_UlSyncValidityDuration_r17));
  ntn->ephemerisInfo_r17 = calloc(1, sizeof(*ntn->ephemerisInfo_r17));
  assert(ntn->cellSpecificKoffset_r17 && ntn->ntn_UlSyncValidityDuration_r17 &&
         ntn->ephemerisInfo_r17);
  *ntn->cellSpecificKoffset_r17 = 17;
  *ntn->ntn_UlSyncValidityDuration_r17 = 3;
  ntn->ephemerisInfo_r17->present = NR_EphemerisInfo_r17_PR_orbital_r17;
  ntn->ephemerisInfo_r17->choice.orbital_r17 =
      calloc(1, sizeof(*ntn->ephemerisInfo_r17->choice.orbital_r17));
  assert(ntn->ephemerisInfo_r17->choice.orbital_r17);
  cc->ServingCellConfigCommon = scc;
  cc->other_sib_bcch_pdu[1][0] = 0xaa;
  cc->other_sib_bcch_length[1] = 1;

  cc->sib1 = calloc(1, sizeof(*cc->sib1));
  assert(cc->sib1);
  cc->sib1->message.present = NR_BCCH_DL_SCH_MessageType_PR_c1;
  cc->sib1->message.choice.c1 = calloc(1, sizeof(*cc->sib1->message.choice.c1));
  assert(cc->sib1->message.choice.c1);
  cc->sib1->message.choice.c1->present =
      NR_BCCH_DL_SCH_MessageType__c1_PR_systemInformationBlockType1;
  NR_SIB1_t *sib1 = calloc(1, sizeof(*sib1));
  assert(sib1);
  cc->sib1->message.choice.c1->choice.systemInformationBlockType1 = sib1;
  sib1->si_SchedulingInfo = calloc(1, sizeof(*sib1->si_SchedulingInfo));
  sib1->nonCriticalExtension = calloc(1, sizeof(*sib1->nonCriticalExtension));
  assert(sib1->si_SchedulingInfo && sib1->nonCriticalExtension);
  sib1->nonCriticalExtension->nonCriticalExtension =
      calloc(1, sizeof(*sib1->nonCriticalExtension->nonCriticalExtension));
  assert(sib1->nonCriticalExtension->nonCriticalExtension);
  sib1->nonCriticalExtension->nonCriticalExtension->nonCriticalExtension =
      calloc(1, sizeof(*sib1->nonCriticalExtension->nonCriticalExtension
                            ->nonCriticalExtension));
  NR_SIB1_v1700_IEs_t *v17 =
      sib1->nonCriticalExtension->nonCriticalExtension->nonCriticalExtension;
  assert(v17);
  v17->si_SchedulingInfo_v1700 =
      calloc(1, sizeof(*v17->si_SchedulingInfo_v1700));
  assert(v17->si_SchedulingInfo_v1700);
  NR_SchedulingInfo2_r17_t *si = calloc(1, sizeof(*si));
  assert(si);
  si->si_BroadcastStatus_r17 =
      NR_SchedulingInfo2_r17__si_BroadcastStatus_r17_broadcasting;
  si->si_WindowPosition_r17 = 2;
  si->si_Periodicity_r17 = NR_SchedulingInfo2_r17__si_Periodicity_r17_rf16;
  assert(ASN_SEQUENCE_ADD(
             &v17->si_SchedulingInfo_v1700->schedulingInfoList2_r17.list, si) ==
         0);
  NR_SIB_TypeInfo_v1700_t *mapping = calloc(1, sizeof(*mapping));
  assert(mapping);
  mapping->sibType_r17.present =
      NR_SIB_TypeInfo_v1700__sibType_r17_PR_type1_r17;
  mapping->sibType_r17.choice.type1_r17 =
      NR_SIB_TypeInfo_v1700__sibType_r17__type1_r17_sibType19;
  assert(ASN_SEQUENCE_ADD(&si->sib_MappingInfo_r17.list, mapping) == 0);
}

static nr_cell_sched_t *new_cell(unsigned int slots_per_frame) {
  nr_cell_sched_t *cell = calloc(1, sizeof(*cell));
  assert(cell);
  init_cell(cell, slots_per_frame);
  return cell;
}

static void free_cell(nr_cell_sched_t *cell) {
  nr_ntn_assistance_publisher_destroy(cell);
  ASN_STRUCT_FREE(asn_DEF_NR_BCCH_DL_SCH_Message, cell->common_channels.sib1);
  ASN_STRUCT_FREE(asn_DEF_NR_ServingCellConfigCommon,
                  cell->common_channels.ServingCellConfigCommon);
  free(cell);
}

static ntn_assistance_state_t state_at(uint64_t generation, uint64_t subframe,
                                       int32_t position) {
  return (ntn_assistance_state_t){.epoch = {generation, subframe},
                                  .position = {position, -33554432, 33554431},
                                  .velocity = {-131072, 0, 131071},
                                  .ta_common = 66485757,
                                  .ta_drift = -257303,
                                  .ta_drift_variant = 28949,
                                  .validity_index = 15};
}

static void advance_slots(nr_ntn_assistance_publisher_t *publisher,
                          unsigned int count) {
  unsigned int wrapped = publisher->previous_wrapped_slot;
  for (unsigned int i = 0; i < count; i++) {
    wrapped = (wrapped + 1) % (1024 * publisher->slots_per_frame);
    nr_ntn_assistance_tick(publisher, wrapped / publisher->slots_per_frame,
                           wrapped % publisher->slots_per_frame);
  }
}

static void check_decoded(const nr_cell_sched_t *cell,
                          const ntn_assistance_state_t *state) {
  const NR_COMMON_channels_t *cc = &cell->common_channels;
  NR_BCCH_DL_SCH_Message_t *message = NULL;
  const asn_dec_rval_t result = uper_decode_complete(
      NULL, &asn_DEF_NR_BCCH_DL_SCH_Message, (void **)&message,
      cc->other_sib_bcch_pdu[1], cc->other_sib_bcch_length[1]);
  assert(result.code == RC_OK && message && message->message.choice.c1);
  const NR_SystemInformation_IEs_t *info =
      message->message.choice.c1->choice.systemInformation->criticalExtensions
          .choice.systemInformation;
  assert(info->sib_TypeAndInfo.list.count == 1);
  const NR_NTN_Config_r17_t *ntn =
      info->sib_TypeAndInfo.list.array[0]->choice.sib19_v1700->ntn_Config_r17;
  assert(ntn && ntn->epochTime_r17);
  assert(ntn->epochTime_r17->sfn_r17 ==
         (long)((state->epoch.subframe / 10) % 1024));
  assert(ntn->epochTime_r17->subFrameNR_r17 ==
         (long)(state->epoch.subframe % 10));
  assert(*ntn->cellSpecificKoffset_r17 == 17 &&
         *ntn->ntn_UlSyncValidityDuration_r17 == (long)state->validity_index);
  assert(ntn->ta_Info_r17->ta_Common_r17 == state->ta_common);
  assert(state->ta_drift
             ? *ntn->ta_Info_r17->ta_CommonDrift_r17 == state->ta_drift
             : !ntn->ta_Info_r17->ta_CommonDrift_r17);
  assert(state->ta_drift_variant
             ? *ntn->ta_Info_r17->ta_CommonDriftVariant_r17 ==
                   state->ta_drift_variant
             : !ntn->ta_Info_r17->ta_CommonDriftVariant_r17);
  const NR_PositionVelocity_r17_t *pv =
      ntn->ephemerisInfo_r17->choice.positionVelocity_r17;
  assert(ntn->ephemerisInfo_r17->present ==
         NR_EphemerisInfo_r17_PR_positionVelocity_r17);
  assert(pv->positionX_r17 == state->position[0] &&
         pv->positionY_r17 == state->position[1] &&
         pv->positionZ_r17 == state->position[2]);
  assert(pv->velocityVX_r17 == state->velocity[0] &&
         pv->velocityVY_r17 == state->velocity[1] &&
         pv->velocityVZ_r17 == state->velocity[2]);
  ASN_STRUCT_FREE(asn_DEF_NR_BCCH_DL_SCH_Message, message);
}

static void test_timeline_and_wrap(void) {
  const unsigned int slots[] = {10, 20, 40, 80};
  for (size_t i = 0; i < sizeof(slots) / sizeof(*slots); i++) {
    nr_cell_sched_t *cell = new_cell(slots[i]);
    const char *error = nr_ntn_assistance_publisher_init(cell);
    if (error)
      fprintf(stderr, "publisher init error: %s\n", error);
    assert(!error);
    nr_ntn_assistance_publisher_t *publisher = cell->ntn_assistance_publisher;
    ntn_assistance_state_t state = state_at(1, 10244, 10);
    assert(!strcmp(nr_ntn_assistance_stage(publisher, 1, 1, &state),
                   "scheduler_time_unavailable"));
    nr_ntn_assistance_tick(publisher, 1023, slots[i] - 1);
    advance_slots(publisher, 1);
    nr_ntn_assistance_snapshot_t snapshot;
    nr_ntn_assistance_snapshot(publisher, &snapshot);
    assert(snapshot.scheduler_time_valid &&
           snapshot.scheduler_time.generation == 1 &&
           snapshot.scheduler_time.subframe == 10240);
    assert(snapshot.scheduler_frame == 0 && snapshot.scheduler_slot == 0 &&
           snapshot.slots_per_frame == slots[i]);
    assert(snapshot.discontinuities == 0);
    assert(!nr_ntn_assistance_stage(publisher, 1, 1, &state));
    assert(nr_ntn_assistance_si_occasion(publisher, cell, true,
                                         slots[i] / 10 * 3));
    nr_ntn_assistance_mark_scheduled(publisher);
    check_decoded(cell, &state);
    advance_slots(publisher, slots[i] / 10 * 5);
    assert(!nr_ntn_assistance_si_occasion(publisher, cell, true, 0));
    nr_ntn_assistance_snapshot(publisher, &snapshot);
    assert(snapshot.status == NR_NTN_ASSISTANCE_EXPIRED &&
           snapshot.active_status == NR_NTN_ASSISTANCE_EXPIRED);
    nr_ntn_assistance_tick(publisher, 20, 0);
    nr_ntn_assistance_snapshot(publisher, &snapshot);
    assert(snapshot.scheduler_time.generation == 2 &&
           snapshot.discontinuities == 1 &&
           snapshot.scheduler_time.subframe == 200);
    assert(!strcmp(nr_ntn_assistance_stage(publisher, 1, 2, &state),
                   "timeline_generation"));
    free_cell(cell);
  }
  /* OAI's current frame_structure stores this field in int8_t and supports
   * at most mu=3. A 160-slot request is rejected without installing ownership.
   */
  nr_cell_sched_t *unsupported = new_cell(160);
  assert(!strcmp(nr_ntn_assistance_publisher_init(unsupported), "numerology"));
  assert(!unsupported->ntn_assistance_publisher);
  free_cell(unsupported);
}

static void test_window_and_coalescing(void) {
  nr_cell_sched_t *cell = new_cell(20);
  assert(!nr_ntn_assistance_publisher_init(cell));
  nr_ntn_assistance_publisher_t *publisher = cell->ntn_assistance_publisher;
  nr_ntn_assistance_tick(publisher, 100, 0);
  assert(!nr_ntn_assistance_si_occasion(publisher, cell, true, 6));
  assert(cell->common_channels.other_sib_bcch_pdu[1][0] == 0xaa);
  ntn_assistance_state_t state = state_at(1, 1001, 1);
  assert(!nr_ntn_assistance_stage(publisher, 7, 1, &state));
  assert(!nr_ntn_assistance_si_occasion(publisher, cell, true, 6));
  state.epoch.subframe = 1010;
  state.position[0] = 2;
  assert(!nr_ntn_assistance_stage(publisher, 7, 2, &state));
  assert(nr_ntn_assistance_si_occasion(publisher, cell, true, 6));
  nr_ntn_assistance_snapshot_t snapshot;
  nr_ntn_assistance_snapshot(publisher, &snapshot);
  assert(snapshot.status == NR_NTN_ASSISTANCE_STAGED);
  nr_ntn_assistance_mark_scheduled(publisher);
  nr_ntn_assistance_snapshot(publisher, &snapshot);
  assert(snapshot.status == NR_NTN_ASSISTANCE_SCHEDULED &&
         snapshot.active_sequence == 2);
  assert(snapshot.scheduled_at.subframe == 1000);
  check_decoded(cell, &state);
  uint8_t frozen[NR_MAX_SIB_LENGTH / 8];
  memcpy(frozen, cell->common_channels.other_sib_bcch_pdu[1], sizeof(frozen));
  NR_NTN_Config_r17_t *active_ntn =
      cell->common_channels.ServingCellConfigCommon->ext2->ntn_Config_r17;

  ntn_assistance_state_t next = state_at(1, 1020, 3);
  assert(!nr_ntn_assistance_stage(publisher, 7, 3, &next));
  next.position[0] = 4;
  assert(!nr_ntn_assistance_stage(publisher, 7, 4, &next));
  assert(!strcmp(nr_ntn_assistance_stage(publisher, 7, 3, &next),
                 "stale_sequence"));
  for (int i = 0; i < 3; i++) {
    advance_slots(publisher, 2);
    assert(nr_ntn_assistance_si_occasion(publisher, cell, false, 6));
    nr_ntn_assistance_mark_scheduled(publisher);
    assert(!memcmp(frozen, cell->common_channels.other_sib_bcch_pdu[1],
                   sizeof(frozen)));
    assert(
        cell->common_channels.ServingCellConfigCommon->ext2->ntn_Config_r17 ==
        active_ntn);
  }
  advance_slots(publisher, 2);
  assert(!nr_ntn_assistance_si_occasion(publisher, cell, false, 6));
  assert(nr_ntn_assistance_si_occasion(publisher, cell, true, 6));
  nr_ntn_assistance_mark_scheduled(publisher);
  check_decoded(cell, &next);
  nr_ntn_assistance_snapshot(publisher, &snapshot);
  assert(snapshot.status == NR_NTN_ASSISTANCE_SCHEDULED &&
         snapshot.active_sequence == 4);
  next.epoch.generation = 2;
  nr_ntn_assistance_tick(publisher, 200, 0);
  assert(!nr_ntn_assistance_si_occasion(publisher, cell, false, 6));
  nr_ntn_assistance_snapshot(publisher, &snapshot);
  assert(snapshot.active_status == NR_NTN_ASSISTANCE_EXPIRED);
  free_cell(cell);
}

static void test_contention_and_error_atomicity(void) {
  nr_cell_sched_t *cell = new_cell(20);
  assert(!nr_ntn_assistance_publisher_init(cell));
  nr_ntn_assistance_publisher_t *publisher = cell->ntn_assistance_publisher;
  nr_ntn_assistance_tick(publisher, 0, 0);
  ntn_assistance_state_t state = state_at(1, 100, 9);
  calloc_calls = 0;
  assert(!nr_ntn_assistance_stage(publisher, 1, 1, &state));
  const unsigned int allocations = calloc_calls;
  NR_NTN_Config_r17_t *original =
      cell->common_channels.ServingCellConfigCommon->ext2->ntn_Config_r17;
  assert(pthread_mutex_lock(&publisher->mutex) == 0);
  assert(!nr_ntn_assistance_si_occasion(publisher, cell, true, 4));
  assert(publisher->pending &&
         cell->common_channels.ServingCellConfigCommon->ext2->ntn_Config_r17 ==
             original);
  assert(pthread_mutex_unlock(&publisher->mutex) == 0);
  advance_slots(publisher, 2);
  assert(!nr_ntn_assistance_si_occasion(publisher, cell, false, 4));

  ntn_assistance_state_t invalid = state;
  invalid.position[0] = 33554432;
  assert(!strcmp(nr_ntn_assistance_stage(publisher, 1, 2, &invalid),
                 "assistance_range"));
  invalid = state;
  invalid.epoch.subframe = 0;
  assert(!strcmp(nr_ntn_assistance_stage(publisher, 1, 2, &invalid),
                 "epoch_past"));
  invalid = state;
  invalid.epoch.subframe = 10241;
  assert(!strcmp(nr_ntn_assistance_stage(publisher, 1, 2, &invalid),
                 "epoch_horizon"));
  for (unsigned int fail = 1; fail <= allocations; fail++) {
    calloc_failure = fail;
    assert(nr_ntn_assistance_stage(publisher, 1, 2, &state));
    calloc_failure = 0;
    nr_ntn_assistance_snapshot_t snapshot;
    nr_ntn_assistance_snapshot(publisher, &snapshot);
    assert(snapshot.status == NR_NTN_ASSISTANCE_STAGED &&
           snapshot.sequence == 1);
    assert(
        cell->common_channels.ServingCellConfigCommon->ext2->ntn_Config_r17 ==
        original);
    assert(cell->common_channels.other_sib_bcch_pdu[1][0] == 0xaa);
  }
  NR_NTN_Config_r17_t *prepared = NULL;
  uint8_t too_small[1];
  int length = -7;
  assert(!strcmp(nr_prepare_sib19(publisher->ntn_template, &state, too_small,
                                  sizeof(too_small), &prepared, &length),
                 "encoding"));
  assert(!prepared && length == -7);
  assert(nr_ntn_assistance_si_occasion(publisher, cell, true, 4));
  nr_ntn_assistance_mark_scheduled(publisher);
  check_decoded(cell, &state);

  state.ta_drift = 0;
  state.ta_drift_variant = 0;
  assert(!nr_ntn_assistance_stage(publisher, 1, 2, &state));
  assert(nr_ntn_assistance_si_occasion(publisher, cell, true, 4));
  nr_ntn_assistance_mark_scheduled(publisher);
  check_decoded(cell, &state);
  free_cell(cell);
}

static void test_legacy_owner_exclusion(void) {
  gNB_MAC_INST *mac = calloc(1, sizeof(*mac));
  assert(mac && !pthread_mutex_init(&mac->sched_lock, NULL));
  gNB_MAC_INST *instances[] = {mac};
  RC.nrmac = instances;
  init_cell(&mac->cells[0], 20);
  init_cell(&mac->cells[1], 20);
  assert(!nr_ntn_assistance_publisher_init(&mac->cells[0]));
  NR_NTN_Config_r17_t *owned =
      mac->cells[0]
          .common_channels.ServingCellConfigCommon->ext2->ntn_Config_r17;
  const gnb_sat_position_update_t legacy = {.sfn = 20,
                                            .subframe = 1,
                                            .delay = 1,
                                            .position = {2, 3, 4},
                                            .velocity = {5, 6, 7}};
  assert(nr_update_sib19(&legacy));
  assert(
      mac->cells[0]
              .common_channels.ServingCellConfigCommon->ext2->ntn_Config_r17 ==
          owned &&
      !owned->epochTime_r17);
  const NR_NTN_Config_r17_t *updated =
      mac->cells[1]
          .common_channels.ServingCellConfigCommon->ext2->ntn_Config_r17;
  assert(updated->epochTime_r17->sfn_r17 == 20 &&
         updated->epochTime_r17->subFrameNR_r17 == 1);
  assert(
      updated->ephemerisInfo_r17->choice.positionVelocity_r17->positionX_r17 ==
      2);
  assert(mac->cells[0].common_channels.other_sib_bcch_pdu[1][0] == 0xaa);
  for (int i = 0; i < 2; i++) {
    nr_ntn_assistance_publisher_destroy(&mac->cells[i]);
    ASN_STRUCT_FREE(asn_DEF_NR_BCCH_DL_SCH_Message,
                    mac->cells[i].common_channels.sib1);
    ASN_STRUCT_FREE(asn_DEF_NR_ServingCellConfigCommon,
                    mac->cells[i].common_channels.ServingCellConfigCommon);
  }
  pthread_mutex_destroy(&mac->sched_lock);
  free(mac);
  RC.nrmac = NULL;
}

static void test_si_occasion_arithmetic(void) {
  /* Window starts at frame 0, slot 5 for mu=1. A relative slot of 18
   * carries to frame 1, slot 3 rather than disappearing at slot 23. */
  assert(!test_other_sib_sched_occasion(2, 5, 1, 20, 1, 3, 0, 18));
  assert(test_other_sib_sched_occasion(2, 5, 1, 20, 0, 3, 0, 18));
  /* Window starts at frame 1023, slot 0 in the 1024-frame period; the next
   * repetition wraps to frame 0, slot 2. */
  assert(!test_other_sib_sched_occasion(2, 10230, 7, 10, 1023, 0, 0, 0));
  assert(!test_other_sib_sched_occasion(2, 10230, 7, 10, 0, 2, 1, 2));
  assert(test_other_sib_sched_occasion(2, 10230, 7, 10, 1023, 2, 1, 2));
}

int main(void) {
  quiet_log.log_component[NR_MAC].level = -1;
  test_timeline_and_wrap();
  test_window_and_coalescing();
  test_contention_and_error_atomicity();
  test_legacy_owner_exclusion();
  test_si_occasion_arithmetic();
  puts("publisher: production ASN decode, ownership, timeline, SI-window, "
       "contention and error-atomicity gates passed");
  return 0;
}
