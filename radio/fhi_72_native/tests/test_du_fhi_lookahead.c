/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include "du_fhi_core.h"
#include "common/config/config_userapi.h"
#include <rte_eal.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

// OAI Linkage Satisfiers
void exit_function(const char *file, const char *function, const int line, const char *s, const int assertflag)
{
  fprintf(stderr, "Error at %s:%s:%d - %s\n", file, function, line, s ? s : "None");
  exit(1);
}
configmodule_interface_t *uniqCfg = NULL;

#include "log.h"

static int get_available_core(void)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  if (sched_getaffinity(0, sizeof(cpu_set_t), &cpuset) == -1)
    return -1;
  long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
  for (int i = 0; i < num_cpus; i++)
    if (CPU_ISSET(i, &cpuset))
      return i;
  return -1;
}

static void test_no_gap_no_duplicate(void)
{
  printf("Testing UL-grant look-ahead: no gaps, no duplicate scheduling...\n");

  char *extra_eal_args[] = {"--no-shconf", "--no-huge", "--no-pci", "--iova-mode=va", "-m", "1024", "--file-prefix=test_du_fhi_lookahead"};

  du_fh_config_t cfg = {.comp_type = FH_COMP_NONE,
                        .numerology = 1,
                        .ru_mac_addrs = {"00:11:22:33:44:66"},
                        .num_ru_mac_addrs = 1,
                        .num_prbs = 106,
                        .mtu = 1500,
                        .dpdk_conf = {.num_dpdk_devices = 1, .dpdk_devices = {"net_null6"}, .extra_eal_args = extra_eal_args, .num_extra_eal_args = 7},
                        .worker_core = get_available_core(),
                        .fdd_mode = false,
                        .tdd_pattern = {.num_dl_slots = 3, .num_ul_slots = 1, .num_dl_symbols = 6, .num_ul_symbols = 4, .tdd_pattern_length_slots = 5},
                        .T1a_cp_dl_min_uS = 100,
                        .T1a_cp_dl_max_uS = 400,
                        .T1a_cp_ul_min_uS = 100,
                        .T1a_cp_ul_max_uS = 400,
                        .T1a_up_min_uS = 50,
                        .T1a_up_max_uS = 200,
                        .Ta3_min_uS = 50,
                        .Ta3_max_uS = 200,
                        .prach_eaxc_offset = 4,
                        .prach_kbar = 2};

  void *handle = du_fh_init(&cfg);
  assert(handle != NULL);

  du_fhi_state_t st;
  memset(&st, 0, sizeof(st));
  st.cfg = cfg;
  st.du_fh_handle = handle;
  int slot_duration_uS = 1000 >> cfg.numerology;
  st.ul_lookahead_slots = (cfg.T1a_cp_ul_max_uS / slot_duration_uS) + 2;

  int nb_rx = 1;

  // hyper_frame is queried live from du_fh's own real-time clock (see
  // du_fhi_query_hyper_frame()), so the expected baseline must be computed the same way rather
  // than assumed to be 0.
  int slots_per_frame = 10 << cfg.numerology;
  uint64_t hyper_frame = du_fhi_query_hyper_frame(handle, 0);
  uint64_t expected_target1 = du_fhi_absolute_slot_number(hyper_frame, 0, 0, slots_per_frame) + st.ul_lookahead_slots;

  uint64_t target1 = du_fhi_advance_ul_schedule_lookahead(&st, 0, 0, nb_rx);
  assert(target1 == expected_target1);
  assert(st.last_scheduled_absolute_slot == target1);

  // Same (frame, slot) again: must not move the frontier backward or skip ahead unexpectedly.
  uint64_t target2 = du_fhi_advance_ul_schedule_lookahead(&st, 0, 0, nb_rx);
  assert(target2 == target1);
  assert(st.last_scheduled_absolute_slot == target1);

  // Advancing by exactly one slot must advance the frontier by exactly one slot (no gap, no jump).
  uint64_t target3 = du_fhi_advance_ul_schedule_lookahead(&st, 0, 1, nb_rx);
  assert(target3 == target1 + 1);
  assert(st.last_scheduled_absolute_slot == target1 + 1);

  du_fh_cleanup(handle);
  printf("PASS\n");
}

static void test_hyper_frame_wrap_adjustment(void)
{
  printf("Testing hyper-frame wrap adjustment near the 1023->0 boundary...\n");

  // Ordinary small skew between the query-time anchor and the caller's own frame (matching
  // south_in's Ta3-window lag, or plain scheduling jitter) must never trigger a correction.
  assert(du_fhi_hyper_frame_wrap_adjustment(560, 560) == 0);
  assert(du_fhi_hyper_frame_wrap_adjustment(560, 559) == 0);
  assert(du_fhi_hyper_frame_wrap_adjustment(559, 560) == 0);

  // Anchor has already wrapped to a low frame while the caller still reports a pre-wrap high
  // frame belonging to the previous hyperframe -- subtract 1 to pair it correctly.
  assert(du_fhi_hyper_frame_wrap_adjustment(0, 1023) == -1);
  // Anchor hasn't wrapped yet but the caller already reports the new hyperframe's low frame --
  // add 1 to pair it correctly.
  assert(du_fhi_hyper_frame_wrap_adjustment(1023, 0) == 1);

  int slots_per_frame = 20;
  uint64_t abs_before_wrap = du_fhi_absolute_slot_number(0, 1023, 5, slots_per_frame);
  uint64_t abs_after_wrap = du_fhi_absolute_slot_number(1, 0, 0, slots_per_frame);
  assert(abs_after_wrap > abs_before_wrap);
  printf("PASS\n");
}

int main(int argc, char **argv)
{
  logInit();
  test_no_gap_no_duplicate();
  test_hyper_frame_wrap_adjustment();
  printf("--- All du_fhi_core look-ahead tests passed ---\n");
  return 0;
}
