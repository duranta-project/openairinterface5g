/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include "oru_fh.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
  printf("--- Running ORU_FH Initialization Test ---\n");
  logInit();
  g_log->log_component[HW].level = OAILOG_DEBUG;

  char *extra_eal_args[] = {"--no-shconf",
                            "--no-huge",
                            "--no-pci",
                            "--iova-mode=va",
                            "--log-level=lib.eal:7",
                            "-m",
                            "1024",
                            "--file-prefix=test_oru_fh"};

  oru_fh_config_t cfg = {.numerology = 1, // 30kHz
                         .du_mac_addrs = {"00:11:22:33:44:55"},
                         .num_du_mac_addrs = 1,
                         .num_prbs = 106,
                         .mtu = 1500,
                         .dpdk_conf = {.num_dpdk_devices = 1,
                                       .dpdk_devices = {"net_null0"},
                                       .extra_eal_args = extra_eal_args,
                                       .num_extra_eal_args = 8},
                         .rx_core = get_available_core(),
                         .tdd_pattern = {.num_dl_slots = 3,
                                         .num_ul_slots = 1,
                                         .num_dl_symbols = 6,
                                         .num_ul_symbols = 4,
                                         .tdd_pattern_length_slots = 5}};

  void *handle = oru_fh_init(&cfg);
  if (!handle) {
    fprintf(stderr, "FAIL: oru_fh_init failed\n");
    return 1;
  }
  printf("SUCCESS: oru_fh initialized handle: %p\n", handle);

  // Testing Lifecycle (Start/Stop)
  printf("Starting ORU_FH...\n");
  if (oru_fh_start(handle) < 0) {
    fprintf(stderr, "FAIL: oru_fh_start failed\n");
    oru_fh_cleanup(handle);
    return 1;
  }

  // Testing UTC Anchor Point and Hyper-frame
  printf("Testing UTC Anchor Point and Hyper-frame...\n");
  uint64_t hf;
  uint32_t f, s;
  struct timespec ts;
  if (oru_fh_get_utc_anchor_point(handle, &hf, &f, &s, &ts) < 0) {
    fprintf(stderr, "FAIL: oru_fh_get_utc_anchor_point failed\n");
    oru_fh_cleanup(handle);
    return 1;
  }
  printf("UTC Anchor Point: hf=%lu, f=%u, s=%u, ts=%ld.%09ld\n", hf, f, s, ts.tv_sec, ts.tv_nsec);
  assert(f < 1024);
  assert(s < (10 << cfg.numerology));

  printf("Running live loop for 2 seconds...\n");
  uint32_t *txData[1];
  txData[0] = malloc(273 * 12 * sizeof(uint32_t));
  for (int i = 0; i < 2000; i++) {

    uint64_t start_cycles = rte_get_timer_cycles();
    uint64_t target_cycles = start_cycles + (rte_get_timer_hz() / 1000);
    while (rte_get_timer_cycles() < target_cycles) {
      int f, s, sym;
      uint64_t hf;
      while (oru_fh_get_ready_jobs(handle) > 0) {
        oru_fh_tx_read_symbol(handle, txData, 1, &hf, &f, &s, &sym, NULL);
      }
    }
  }
  free(txData[0]);

  printf("Stopping ORU_FH...\n");
  oru_fh_stop(handle);

  printf("Cleaning up handle...\n");
  oru_fh_cleanup(handle);
  printf("--- ORU_FH Test Complete ---\n");
  return 0;
}
