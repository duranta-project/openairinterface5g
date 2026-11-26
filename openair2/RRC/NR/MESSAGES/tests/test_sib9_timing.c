/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
#include "test_sib9_fixture.h"
#include "openair2/LAYER2/NR_MAC_gNB/nr_sib9_mac.h"
#include <limits.h>

RAN_CONTEXT_t RC;
static struct timespec sample = {.tv_sec = 1700000000, .tv_nsec = 19000000};
static bool fail_clock, fail_encode, use_real_clock;
static unsigned reads;
int __real_clock_gettime(clockid_t clock, struct timespec *now);
int __wrap_clock_gettime(clockid_t clock, struct timespec *now)
{
  if (clock != CLOCK_REALTIME || use_real_clock)
    return __real_clock_gettime(clock, now);
  reads++;
  if (fail_clock)
    return -1;
  *now = sample;
  return 0;
}
int __real_encode_sysinfo_ie(NR_SystemInformation_IEs_t *si, uint8_t *pdu, int size);
int __wrap_encode_sysinfo_ie(NR_SystemInformation_IEs_t *si, uint8_t *pdu, int size)
{
  return fail_encode ? -1 : __real_encode_sysinfo_ie(si, pdu, size);
}

static nr_cell_sched_t *fixture(int spf)
{
  nr_cell_sched_t *cell = calloc_or_fail(1, sizeof(*cell));
  cell->nr_cellid = 917505;
  cell->frame_structure.numb_slots_frame = spf;
  cell->common_channels.sib1 = make_sib1(cell->nr_cellid);
  NR_SIB2_t sibling = {0};
  sibling.intraFreqCellReselectionInfo.q_RxLevMin = -56;
  sib9_config_t profile = {.time_info_utc = 7, .leap_seconds_present = true, .leap_seconds = 18};
  NR_SIB9_t *sib9 = get_SIB9_NR(&profile);
  f1ap_sib_msg_t list[] = {{.SI_type = 2, .SI_container = do_SIB2_NR(&sibling)}, {.SI_type = 9, .SI_container = do_SIB9_NR(sib9)}};
  AssertFatal(nr_mac_configure_other_sib(cell, 2, list) == NR_SI_PUBLISHED, "fixture publish\n");
  ASN_STRUCT_FREE(asn_DEF_NR_SIB9, sib9);
  free_byte_array(list[0].SI_container);
  free_byte_array(list[1].SI_container);
  return cell;
}

static void release(nr_cell_sched_t *cell)
{
  ASN_STRUCT_FREE(asn_DEF_NR_BCCH_DL_SCH_Message, cell->common_channels.sib1);
  ASN_STRUCT_FREE(asn_DEF_NR_SystemInformation_IEs, cell->common_channels.sib9_si);
  free(cell);
}

static void tick(nr_cell_sched_t *cell, int tx)
{
  int spf = cell->frame_structure.numb_slots_frame, cycle = 1024 * spf;
  tx = (tx % cycle + cycle) % cycle;
  int rx = (tx - 6 + cycle) % cycle;
  nr_slot_time_t context = {.tx_frame = tx / spf, .tx_slot = tx % spf, .rx_frame = rx / spf, .rx_slot = rx % spf};
  nr_sib9_slot(cell, context.tx_frame, context.tx_slot, &context);
}

static uint64_t payload(nr_cell_sched_t *cell, int rel_frame, int rel_slot, int window)
{
  int length;
  const uint8_t *pdu = nr_sib9_payload(cell, rel_frame, rel_slot, window, &length);
  AssertFatal(pdu && length > 0, "fresh payload\n");
  return read_utc(pdu, length);
}

static void benchmark(void)
{
  nr_cell_sched_t *cell = fixture(20);
  cell->common_channels.sib9_mode = NR_SIB9_REALTIME;
  use_real_clock = true;
  const int iterations = 10000;
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  for (int i = 0; i < iterations; ++i) {
    tick(cell, i * 320);
    payload(cell, 0, 0, 10);
  }
  clock_gettime(CLOCK_MONOTONIC, &end);
  double refresh_ns = ((end.tv_sec - start.tv_sec) * 1e9 + end.tv_nsec - start.tv_nsec) / iterations;
  int length;
  clock_gettime(CLOCK_MONOTONIC, &start);
  for (int i = 0; i < iterations * 100; ++i)
    AssertFatal(nr_sib9_payload(cell, 0, 0, 10, &length), "cached payload\n");
  clock_gettime(CLOCK_MONOTONIC, &end);
  double cache_ns = ((end.tv_sec - start.tv_sec) * 1e9 + end.tv_nsec - start.tv_nsec) / (iterations * 100);
  printf("{\"refresh_mean_ns\": %.1f, \"cache_mean_ns\": %.1f, \"refreshes\": %d}\n", refresh_ns, cache_ns, iterations);
  release(cell);
}

int main(int argc, char **argv)
{
  logInit();
  if (argc == 2 && !strcmp(argv[1], "--benchmark")) {
    benchmark();
    logTerm();
    return 0;
  }
  nr_cell_sched_t *cell = fixture(20);
  NR_COMMON_channels_t *cc = &cell->common_channels;
  AssertFatal(payload(cell, 0, 0, 10) == 7 && reads == 0, "static does not read system clock\n");
  uint8_t sib1[NR_SIB9_MAX_BYTES], sibling[NR_SIB9_MAX_BYTES];
  memcpy(sib1, cc->sib1_bcch_pdu, sizeof(sib1));
  memcpy(sibling, cc->other_sib_bcch_pdu[0], sizeof(sibling));
  cc->sib9_mode = NR_SIB9_REALTIME;
  tick(cell, 0);
  AssertFatal(payload(cell, 0, 0, 10) == UINT64_C(390898880003) && reads == 1, "RX end, lead, window target\n");
  tick(cell, 1);
  tick(cell, 2);
  sample.tv_sec += 100;
  AssertFatal(payload(cell, 0, 2, 10) == UINT64_C(390898880003) && reads == 1, "same window across SSBs\n");
  tick(cell, 320);
  AssertFatal(payload(cell, 0, 0, 10) == UINT64_C(390898890003), "forward clock step\n");
  sample.tv_sec -= 200;
  tick(cell, 640);
  AssertFatal(payload(cell, 0, 0, 10) == UINT64_C(390898870003), "backward clock step follows host\n");
  int length;
  tick(cell, 960);
  fail_clock = true;
  AssertFatal(!nr_sib9_payload(cell, 0, 0, 10, &length) && length == 0, "no stale fallback\n");
  fail_clock = false;
  AssertFatal(!nr_sib9_payload(cell, 0, 0, 10, &length), "failed window not retried\n");
  tick(cell, 1280);
  AssertFatal(payload(cell, 0, 0, 10) == UINT64_C(390898870003), "next-window recovery\n");
  tick(cell, 1600);
  fail_encode = true;
  AssertFatal(!nr_sib9_payload(cell, 0, 0, 10, &length), "encode failure\n");
  fail_encode = false;
  tick(cell, 1920);
  AssertFatal(payload(cell, 0, 0, 10) == UINT64_C(390898870003), "encode recovery\n");
  AssertFatal(!memcmp(sib1, cc->sib1_bcch_pdu, sizeof(sib1)) && !memcmp(sibling, cc->other_sib_bcch_pdu[0], sizeof(sibling)),
              "SIB1 tags and SIB2 preserved\n");
  nr_sib9_slot(cell, 100, 0, NULL);
  AssertFatal(!nr_sib9_payload(cell, 0, 0, 10, &length), "missing internal L1 timing context\n");
  tick(cell, 1024 * 20 - 1);
  tick(cell, 0);
  sample = (struct timespec){.tv_sec = 1700006399, .tv_nsec = 999000000};
  AssertFatal(payload(cell, 0, 0, 10) == UINT64_C(390899520001), "SFN and UTC-day rollover\n");
  tick(cell, 320);
  sample.tv_sec = LLONG_MAX;
  AssertFatal(!nr_sib9_payload(cell, 0, 0, 10, &length), "range failure\n");
  release(cell);
  for (int spf = 10; spf <= 80; spf *= 2) {
    cell = fixture(spf);
    cell->common_channels.sib9_mode = NR_SIB9_REALTIME;
    tick(cell, 0);
    sample = (struct timespec){.tv_sec = 1700000000, .tv_nsec = 999000000};
    AssertFatal(payload(cell, 0, 0, spf / 2) == (spf < 80 ? UINT64_C(390898880101) : UINT64_C(390898880100)),
                "numerology and window size\n");
    release(cell);
  }
  logTerm();
  return 0;
}
