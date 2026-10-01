/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include "du_fh.h"
#include <stdio.h>
#include <stdlib.h>
#include "common/config/config_userapi.h"
#include <rte_eal.h>
#include <assert.h>

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
  if (sched_getaffinity(0, sizeof(cpu_set_t), &cpuset) == -1) {
    return -1;
  }
  long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
  for (int i = 0; i < num_cpus; i++) {
    if (CPU_ISSET(i, &cpuset)) {
      return i;
    }
  }
  return -1;
}

int main(int argc, char **argv)
{
  printf("--- Running DU_FH Initialization Test ---\n");
  logInit();
  g_log->log_component[HW].level = OAILOG_DEBUG;

  char *extra_eal_args[] = {"--no-shconf",
                            "--no-huge",
                            "--no-pci",
                            "--iova-mode=va",
                            "--log-level=lib.eal:7",
                            "-m",
                            "1024",
                            "--file-prefix=test_du_fh"};

  du_fh_config_t cfg = {.comp_type = FH_COMP_NONE,
                        .numerology = 1, // 30kHz
                        .ru_mac_addrs = {"00:11:22:33:44:55"},
                        .num_ru_mac_addrs = 1,
                        .num_prbs = 106,
                        .mtu = 1500,
                        .dpdk_conf = {.num_dpdk_devices = 1,
                                      .dpdk_devices = {"net_null4"},
                                      .extra_eal_args = extra_eal_args,
                                      .num_extra_eal_args = 8},
                        .worker_core = get_available_core(),
                        .fdd_mode = false,
                        .tdd_pattern = {.num_dl_slots = 3,
                                        .num_ul_slots = 1,
                                        .num_dl_symbols = 6,
                                        .num_ul_symbols = 4,
                                        .tdd_pattern_length_slots = 5},
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
  if (!handle) {
    fprintf(stderr, "FAIL: du_fh_init failed\n");
    return 1;
  }
  printf("SUCCESS: du_fh initialized handle: %p\n", handle);

  printf("Starting DU_FH...\n");
  if (du_fh_start(handle) < 0) {
    fprintf(stderr, "FAIL: du_fh_start failed\n");
    du_fh_cleanup(handle);
    return 1;
  }

  printf("Testing UTC Anchor Point and Hyper-frame...\n");
  uint64_t hf;
  uint32_t f, s;
  struct timespec ts;
  if (du_fh_get_utc_anchor_point(handle, &hf, &f, &s, &ts) < 0) {
    fprintf(stderr, "FAIL: du_fh_get_utc_anchor_point failed\n");
    du_fh_cleanup(handle);
    return 1;
  }
  printf("UTC Anchor Point: hf=%lu, f=%u, s=%u, ts=%ld.%09ld\n", hf, f, s, ts.tv_sec, ts.tv_nsec);
  assert(f < 1024);
  assert(s < (10 << cfg.numerology));

  printf("Testing forwarding correctness (TX Schedule DL IQ)...\n");
  uint32_t *txData[1];
  txData[0] = malloc(cfg.num_prbs * 12 * sizeof(uint32_t));
  
  du_tx_dl_section_t sections[1] = {{.beam_id = 0, .start_prb = 0, .num_prb = cfg.num_prbs, .section_id = 0}};
  du_fh_tx_send_dl_iq(handle, txData, 1, 0, 0, 0, 0, sections, 1);

  printf("Testing forwarding correctness (RX Expect UL Symbol)...\n");
  du_fh_expect_ul_symbol(handle, 10, 0, 0, 0, cfg.num_prbs, cfg.comp_type, 16);

  du_tx_scheduler_stats_t tx_stats = {0};
  du_packet_processor_stats_t rx_stats = {0};

  // Allow timer ticks to happen
  printf("Running live loop for 1 second...\n");
  uint64_t start_cycles = rte_get_timer_cycles();
  uint64_t target_cycles = start_cycles + (rte_get_timer_hz() / 1);
  while (rte_get_timer_cycles() < target_cycles) {
    // Spin while fh_timer ticks in the background thread
  }

  du_fh_get_stats(handle, &rx_stats, &tx_stats);
  printf("TX Scheduled: %lu\n", tx_stats.total_dl_scheduled);
  assert(tx_stats.total_dl_scheduled > 0);

  free(txData[0]);

  printf("Stopping DU_FH...\n");
  du_fh_stop(handle);

  printf("Cleaning up handle...\n");
  du_fh_cleanup(handle);
  printf("--- DU_FH Test Complete ---\n");
  return 0;
}
