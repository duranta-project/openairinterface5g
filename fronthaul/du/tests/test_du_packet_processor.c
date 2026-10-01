/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_byteorder.h>
#include "xran_pkt_api.h"
#include "du_packet_processor.h"

#include "log.h"
#include "common/config/config_userapi.h"

// OAI Linkage Satisfiers
void exit_function(const char *file, const char *function, const int line, const char *s, const int assertflag)
{
  fprintf(stderr, "Error at %s:%s:%d - %s\n", file, function, line, s ? s : "None");
  exit(1);
}
configmodule_interface_t *uniqCfg = NULL;

struct rte_mempool *mp = NULL;

struct xran_eaxcid_config g_eaxcid_config = {.mask_cuPortId = 0xF000,
                                             .mask_bandSectorId = 0x0F00,
                                             .mask_ccId = 0x00F0,
                                             .mask_ruPortId = 0x000F,
                                             .bit_cuPortId = 12,
                                             .bit_bandSectorId = 8,
                                             .bit_ccId = 4,
                                             .bit_ruPortId = 0};

void setup_dpdk(int argc, char **argv)
{
  int ret = rte_eal_init(argc, argv);
  assert(ret >= 0);
  logInit();
  mp = rte_pktmbuf_pool_create("test_pool_du", 1024, 0, 0, 10000, rte_socket_id());
  assert(mp != NULL);
}

void test_basic_round_trip()
{
  printf("Testing basic round trip...\n");
  int mu = 1;
  void *ctx = init_du_packet_processor(mu, 273, 100, 300, 2, 0, 5, false, 1500, 0);
  assert(ctx != NULL);

  uint64_t current_sym = 1035;
  du_pp_handle_absolute_symbol_tick(ctx, current_sym);

  uint64_t target_sym = current_sym - 5; // Arrived delayed by 5 symbols (within 3..10 range since 100-300uS is 3-8 symbols roughly. Wait! 100uS / 35.7uS = 2.8 -> 2 symbols? 300uS / 35.7uS = 8.4 -> 8 symbols. Let's trace it carefully.)
  // 30kHz, slot=500us, symbol = 500/14 = 35.7uS
  // Ta3_min = 100uS -> 100/35.7 = 2.8 -> 2
  // Ta3_max = 300uS -> 300/35.7 = 8.4 -> 8
  // So diff must be between 2 and 8 symbols.
  target_sym = current_sym - 5;

  du_pp_expect_ul_symbol(ctx, target_sym, 0, 0, 0, 1, FH_COMP_NONE, 16);

  int num_symbols_per_frame = 10 * 2 * 14;
  int frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  int subframeId = slot_in_frame / 2;
  int slotId = slot_in_frame % 2;
  int startSymbolId = target_sym % 14;

  struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
  fill_ecpri_header(u_ecpri, &g_eaxcid_config, ECPRI_IQ_DATA, sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr) + 1 * 12 * 4, 0, 0, 0, 0);

  struct radio_app_common_hdr *u_app = (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
  fill_radio_app_header(u_app, 0, XRAN_DIR_UL, frameId, slot_in_frame, startSymbolId, mu);

  struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
  fill_data_section_header(u_data, 1, 0, 0);

  uint16_t *iq = (uint16_t *)rte_pktmbuf_append(u_mbuf, 1 * 12 * 4);
  assert(iq != NULL);
  iq[0] = rte_cpu_to_be_16(0xAA55);
  iq[1] = rte_cpu_to_be_16(0xBB66);

  du_pp_handle_uplane_packet(ctx, u_mbuf);

  du_packet_processor_stats_t stats;
  du_pp_get_stats(ctx, &stats);
  assert(stats.uplane_err_early == 0);
  assert(stats.uplane_err_late == 0);

  assert(du_pp_get_ready_ul_job_count(ctx) > 0);

  uint32_t output_iq[273 * 12] = {0};
  uint32_t *rxdataF[4] = {output_iq};
  int frame = -1, slot = -1, symbol = -1;
  uint64_t hyper_frame;
  
  while (du_pp_get_ready_ul_job_count(ctx) > 0) {
    du_pp_read_ul_iq(ctx, rxdataF, 1, &hyper_frame, &frame, &slot, &symbol);
    if (symbol == startSymbolId) {
      break;
    }
  }

  assert(symbol == startSymbolId);
  uint16_t *out_iq = (uint16_t *)output_iq;
  assert(out_iq[0] == 0xAA55);
  assert(out_iq[1] == 0xBB66);

  cleanup_du_packet_processor(ctx);
  printf("Basic round trip passed!\n");
}

void test_multi_fragment_reassembly()
{
  printf("Testing multi fragment reassembly...\n");
  int mu = 1;
  void *ctx = init_du_packet_processor(mu, 273, 100, 300, 2, 0, 5, false, 1500, 0);
  assert(ctx != NULL);

  uint64_t current_sym = 1035;
  du_pp_handle_absolute_symbol_tick(ctx, current_sym);
  uint64_t target_sym = current_sym - 5;

  du_pp_expect_ul_symbol(ctx, target_sym, 0, 0, 0, 10, FH_COMP_NONE, 16);
  du_pp_expect_ul_symbol(ctx, target_sym, 1, 0, 0, 10, FH_COMP_NONE, 16);

  int num_symbols_per_frame = 10 * 2 * 14;
  int frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  int startSymbolId = target_sym % 14;

  int initial_ready = du_pp_get_ready_ul_job_count(ctx);

  struct rte_mbuf *u_mbuf1 = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *u_ecpri1 = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf1, sizeof(struct xran_ecpri_hdr));
  fill_ecpri_header(u_ecpri1, &g_eaxcid_config, ECPRI_IQ_DATA, sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr) + 10 * 12 * 4, 0, 0, 0, 0);
  struct radio_app_common_hdr *u_app1 = (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf1, sizeof(struct radio_app_common_hdr));
  fill_radio_app_header(u_app1, 0, XRAN_DIR_UL, frameId, slot_in_frame, startSymbolId, mu);
  struct data_section_hdr *u_data1 = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf1, sizeof(struct data_section_hdr));
  fill_data_section_header(u_data1, 10, 0, 0);
  rte_pktmbuf_append(u_mbuf1, 10 * 12 * 4);
  du_pp_handle_uplane_packet(ctx, u_mbuf1);

  assert(du_pp_get_ready_ul_job_count(ctx) == initial_ready); // Not ready yet

  struct rte_mbuf *u_mbuf2 = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *u_ecpri2 = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf2, sizeof(struct xran_ecpri_hdr));
  fill_ecpri_header(u_ecpri2, &g_eaxcid_config, ECPRI_IQ_DATA, sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr) + 10 * 12 * 4, 0, 1, 0, 0);
  struct radio_app_common_hdr *u_app2 = (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf2, sizeof(struct radio_app_common_hdr));
  fill_radio_app_header(u_app2, 0, XRAN_DIR_UL, frameId, slot_in_frame, startSymbolId, mu);
  struct data_section_hdr *u_data2 = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf2, sizeof(struct data_section_hdr));
  fill_data_section_header(u_data2, 10, 0, 0);
  rte_pktmbuf_append(u_mbuf2, 10 * 12 * 4);
  du_pp_handle_uplane_packet(ctx, u_mbuf2);

  assert(du_pp_get_ready_ul_job_count(ctx) > initial_ready); // Now ready!

  uint32_t output_iq1[273 * 12] = {0};
  uint32_t output_iq2[273 * 12] = {0};
  uint32_t *rxdataF[4] = {output_iq1, output_iq2, NULL, NULL};
  int frame = -1, slot = -1, symbol = -1;
  uint64_t hyper_frame;
  while (du_pp_get_ready_ul_job_count(ctx) > 0) {
    du_pp_read_ul_iq(ctx, rxdataF, 2, &hyper_frame, &frame, &slot, &symbol);
    if (symbol == startSymbolId) break;
  }
  assert(symbol == startSymbolId);

  cleanup_du_packet_processor(ctx);
  printf("Multi fragment reassembly passed!\n");
}

void test_early_rejection()
{
  printf("Testing early rejection...\n");
  int mu = 1;
  void *ctx = init_du_packet_processor(mu, 273, 100, 300, 2, 0, 5, false, 1500, 0);
  assert(ctx != NULL);
  
  uint64_t current_sym = 1035;
  du_pp_handle_absolute_symbol_tick(ctx, current_sym);
  
  uint64_t target_sym = current_sym - 1; // diff = 1 (too early, min is 2)
  du_pp_expect_ul_symbol(ctx, target_sym, 0, 0, 0, 1, FH_COMP_NONE, 16);
  
  int num_symbols_per_frame = 10 * 2 * 14;
  int frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  int startSymbolId = target_sym % 14;

  struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
  fill_ecpri_header(u_ecpri, &g_eaxcid_config, ECPRI_IQ_DATA, sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr) + 1 * 12 * 4, 0, 0, 0, 0);
  struct radio_app_common_hdr *u_app = (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
  fill_radio_app_header(u_app, 0, XRAN_DIR_UL, frameId, slot_in_frame, startSymbolId, mu);
  struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
  fill_data_section_header(u_data, 1, 0, 0);
  rte_pktmbuf_append(u_mbuf, 1 * 12 * 4);

  du_pp_handle_uplane_packet(ctx, u_mbuf);

  du_packet_processor_stats_t stats;
  du_pp_get_stats(ctx, &stats);
  assert(stats.uplane_err_early == 1);
  assert(stats.uplane_err_late == 0);

  cleanup_du_packet_processor(ctx);
  printf("Early rejection passed!\n");
}

void test_late_rejection()
{
  printf("Testing late rejection...\n");
  int mu = 1;
  void *ctx = init_du_packet_processor(mu, 273, 100, 300, 2, 0, 5, false, 1500, 0);
  assert(ctx != NULL);
  
  uint64_t current_sym = 1035;
  du_pp_handle_absolute_symbol_tick(ctx, current_sym);
  
  uint64_t target_sym = current_sym - 10; // diff = 10 (too late, max is 8)
  du_pp_expect_ul_symbol(ctx, target_sym, 0, 0, 0, 1, FH_COMP_NONE, 16);
  
  int num_symbols_per_frame = 10 * 2 * 14;
  int frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  int startSymbolId = target_sym % 14;

  struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
  fill_ecpri_header(u_ecpri, &g_eaxcid_config, ECPRI_IQ_DATA, sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr) + 1 * 12 * 4, 0, 0, 0, 0);
  struct radio_app_common_hdr *u_app = (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
  fill_radio_app_header(u_app, 0, XRAN_DIR_UL, frameId, slot_in_frame, startSymbolId, mu);
  struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
  fill_data_section_header(u_data, 1, 0, 0);
  rte_pktmbuf_append(u_mbuf, 1 * 12 * 4);

  du_pp_handle_uplane_packet(ctx, u_mbuf);

  du_packet_processor_stats_t stats;
  du_pp_get_stats(ctx, &stats);
  assert(stats.uplane_err_early == 0);
  assert(stats.uplane_err_late == 1);

  cleanup_du_packet_processor(ctx);
  printf("Late rejection passed!\n");
}

void test_window_eviction()
{
  printf("Testing window eviction...\n");
  int mu = 1;
  void *ctx = init_du_packet_processor(mu, 273, 100, 300, 2, 0, 5, false, 1500, 0);
  assert(ctx != NULL);
  
  uint64_t current_sym = 1035;
  du_pp_handle_absolute_symbol_tick(ctx, current_sym);
  
  uint64_t target_sym = current_sym - 5;
  du_pp_expect_ul_symbol(ctx, target_sym, 0, 0, 0, 10, FH_COMP_NONE, 16);
  int initial_ready = du_pp_get_ready_ul_job_count(ctx);

  // Tick the timer well past Ta3_max_sym_diff (which is 8)
  du_pp_handle_absolute_symbol_tick(ctx, current_sym + 10);
  
  assert(du_pp_get_ready_ul_job_count(ctx) > initial_ready);

  uint32_t output_iq[273 * 12] = {0};
  uint32_t *rxdataF[4] = {output_iq};
  int frame = -1, slot = -1, symbol = -1;
  uint64_t hyper_frame;
  while (du_pp_get_ready_ul_job_count(ctx) > 0) {
    du_pp_read_ul_iq(ctx, rxdataF, 1, &hyper_frame, &frame, &slot, &symbol);
    if (symbol == target_sym % 14) break;
  }
  
  assert(symbol == target_sym % 14);

  cleanup_du_packet_processor(ctx);
  printf("Window eviction passed!\n");
}

void test_fdd_mode()
{
  printf("Testing FDD mode...\n");
  int mu = 1;
  void *ctx = init_du_packet_processor(mu, 273, 100, 300, 0, 0, 5, true, 1500, 0); // fdd_mode = true, even with 0 UL slots
  assert(ctx != NULL);
  
  uint64_t current_sym = 1000;
  du_pp_handle_absolute_symbol_tick(ctx, current_sym);
  
  uint64_t target_sym = current_sym - 5;
  du_pp_expect_ul_symbol(ctx, target_sym, 0, 0, 0, 1, FH_COMP_NONE, 16);
  
  int num_symbols_per_frame = 10 * 2 * 14;
  int frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  int startSymbolId = target_sym % 14;

  struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
  fill_ecpri_header(u_ecpri, &g_eaxcid_config, ECPRI_IQ_DATA, sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr) + 1 * 12 * 4, 0, 0, 0, 0);
  struct radio_app_common_hdr *u_app = (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
  fill_radio_app_header(u_app, 0, XRAN_DIR_UL, frameId, slot_in_frame, startSymbolId, mu);
  struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
  fill_data_section_header(u_data, 1, 0, 0);
  rte_pktmbuf_append(u_mbuf, 1 * 12 * 4);

  du_pp_handle_uplane_packet(ctx, u_mbuf);

  du_packet_processor_stats_t stats;
  du_pp_get_stats(ctx, &stats);
  assert(stats.ul_tdd_mismatch == 0); // Accepted!

  cleanup_du_packet_processor(ctx);
  printf("FDD mode passed!\n");
}

void test_tdd_mismatch()
{
  printf("Testing TDD mismatch...\n");
  int mu = 1;
  void *ctx = init_du_packet_processor(mu, 273, 100, 300, 2, 0, 5, false, 1500, 0);
  assert(ctx != NULL);
  
  uint64_t current_sym = 1000;
  du_pp_handle_absolute_symbol_tick(ctx, current_sym);
  
  uint64_t target_sym = current_sym - 5; 
  // 5 slots pattern. DL: ? UL: 2 slots. Let's make target_sym point to a non-UL symbol.
  // 5 slots * 14 = 70 symbols per pattern. The last 2 slots are UL, meaning last 28 symbols.
  // We want a symbol in the first 42 symbols of the pattern.
  target_sym = (target_sym / 70) * 70 + 10; // definitely DL symbol
  
  // Re-adjust current_sym so diff is 5
  current_sym = target_sym + 5;
  du_pp_handle_absolute_symbol_tick(ctx, current_sym);
  
  du_pp_expect_ul_symbol(ctx, target_sym, 0, 0, 0, 1, FH_COMP_NONE, 16);
  
  int num_symbols_per_frame = 10 * 2 * 14;
  int frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  int startSymbolId = target_sym % 14;

  struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
  fill_ecpri_header(u_ecpri, &g_eaxcid_config, ECPRI_IQ_DATA, sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr) + 1 * 12 * 4, 0, 0, 0, 0);
  struct radio_app_common_hdr *u_app = (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
  fill_radio_app_header(u_app, 0, XRAN_DIR_UL, frameId, slot_in_frame, startSymbolId, mu);
  struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
  fill_data_section_header(u_data, 1, 0, 0);
  rte_pktmbuf_append(u_mbuf, 1 * 12 * 4);

  du_pp_handle_uplane_packet(ctx, u_mbuf);

  du_packet_processor_stats_t stats;
  du_pp_get_stats(ctx, &stats);
  assert(stats.ul_tdd_mismatch == 1); // Rejected!

  cleanup_du_packet_processor(ctx);
  printf("TDD mismatch passed!\n");
}

void test_prach_basic()
{
  printf("Testing PRACH basic...\n");
  int mu = 1;
  void *ctx = init_du_packet_processor(mu, 273, 100, 300, 2, 0, 5, false, 1500, 0);
  assert(ctx != NULL);
  
  uint64_t current_sym = 1035;
  du_pp_handle_absolute_symbol_tick(ctx, current_sym);
  
  uint64_t target_sym = current_sym - 5;
  int num_symbols_per_frame = 10 * 2 * 14;
  int frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  int startSymbolId = target_sym % 14;
  int kbar = 2;
  
  du_pp_expect_prach_occasion(ctx, target_sym, 1, slot_in_frame, 0, 1, 0, 12, FH_COMP_NONE, 16, kbar);
  
  struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
  fill_ecpri_header(u_ecpri, &g_eaxcid_config, ECPRI_IQ_DATA, sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr) + (139 + kbar) * 2 * 2, 0, 0, 0, 0);
  struct radio_app_common_hdr *u_app = (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
  fill_radio_app_header(u_app, 0, XRAN_DIR_UL, frameId, slot_in_frame, startSymbolId, mu);
  struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
  fill_data_section_header(u_data, 12, 0, 1);
  
  uint16_t *iq = (uint16_t *)rte_pktmbuf_append(u_mbuf, (139 + kbar) * 2 * 2);
  memset(iq, 0, (139 + kbar) * 2 * 2);
  iq[kbar + 0] = rte_cpu_to_be_16(0x1111);
  iq[kbar + 1] = rte_cpu_to_be_16(0x2222);
  
  du_pp_handle_prach_uplane_packet(ctx, u_mbuf);
  
  du_pp_handle_absolute_symbol_tick(ctx, current_sym + 10);
  
  assert(du_pp_get_ready_prach_job_count(ctx) == 1);
  
  int16_t out_iq[139 * 2] = {0};
  uint64_t hf;
  int f, s, ant, sec;
  du_pp_read_prach_iq(ctx, out_iq, &hf, &f, &s, &ant, &sec);
  
  assert(f == frameId);
  assert(s == slot_in_frame);
  assert(ant == 0);
  assert(sec == 1);
  assert((uint16_t)out_iq[0] == 0x1111);
  assert((uint16_t)out_iq[1] == 0x2222);
  
  cleanup_du_packet_processor(ctx);
  printf("PRACH basic passed!\n");
}

void test_prach_multi_symbol()
{
  printf("Testing PRACH multi symbol...\n");
  int mu = 1;
  void *ctx = init_du_packet_processor(mu, 273, 100, 300, 2, 0, 5, false, 1500, 0);
  assert(ctx != NULL);
  
  uint64_t current_sym = 1035;
  du_pp_handle_absolute_symbol_tick(ctx, current_sym);
  uint64_t target_sym = current_sym - 5;
  int num_symbols_per_frame = 10 * 2 * 14;
  int frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  int startSymbolId = target_sym % 14;
  int kbar = 0;
  
  du_pp_expect_prach_occasion(ctx, target_sym, 2, slot_in_frame, 0, 1, 0, 12, FH_COMP_NONE, 16, kbar);
  
  for (int i = 0; i < 2; i++) {
    struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
    struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
    fill_ecpri_header(u_ecpri, &g_eaxcid_config, ECPRI_IQ_DATA, sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr) + 139 * 2 * 2, 0, 0, 0, 0);
    struct radio_app_common_hdr *u_app = (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
    fill_radio_app_header(u_app, 0, XRAN_DIR_UL, frameId, slot_in_frame, startSymbolId + i, mu);
    struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
    fill_data_section_header(u_data, 12, 0, 1);
    
    uint16_t *iq = (uint16_t *)rte_pktmbuf_append(u_mbuf, 139 * 2 * 2);
    memset(iq, 0, 139 * 2 * 2);
    iq[0] = rte_cpu_to_be_16(100);
    iq[1] = rte_cpu_to_be_16(200);
    du_pp_handle_prach_uplane_packet(ctx, u_mbuf);
  }
  
  du_pp_handle_absolute_symbol_tick(ctx, current_sym + 10);
  assert(du_pp_get_ready_prach_job_count(ctx) == 1);
  
  int16_t out_iq[139 * 2] = {0};
  du_pp_read_prach_iq(ctx, out_iq, NULL, NULL, NULL, NULL, NULL);
  
  assert(out_iq[0] == 200);
  assert(out_iq[1] == 400);
  
  cleanup_du_packet_processor(ctx);
  printf("PRACH multi symbol passed!\n");
}

void test_prach_missing_symbol()
{
  printf("Testing PRACH missing symbol...\n");
  int mu = 1;
  void *ctx = init_du_packet_processor(mu, 273, 100, 300, 2, 0, 5, false, 1500, 0);
  assert(ctx != NULL);
  
  uint64_t current_sym = 1035;
  du_pp_handle_absolute_symbol_tick(ctx, current_sym);
  uint64_t target_sym = current_sym - 5;
  int num_symbols_per_frame = 10 * 2 * 14;
  int frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  int startSymbolId = target_sym % 14;
  int kbar = 0;
  
  du_pp_expect_prach_occasion(ctx, target_sym, 2, slot_in_frame, 0, 1, 0, 12, FH_COMP_NONE, 16, kbar);
  
  struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
  fill_ecpri_header(u_ecpri, &g_eaxcid_config, ECPRI_IQ_DATA, sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr) + 139 * 2 * 2, 0, 0, 0, 0);
  struct radio_app_common_hdr *u_app = (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
  fill_radio_app_header(u_app, 0, XRAN_DIR_UL, frameId, slot_in_frame, startSymbolId + 1, mu);
  struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
  fill_data_section_header(u_data, 12, 0, 1);
  uint16_t *iq = (uint16_t *)rte_pktmbuf_append(u_mbuf, 139 * 2 * 2);
  memset(iq, 0, 139 * 2 * 2);
  iq[0] = rte_cpu_to_be_16(100);
  du_pp_handle_prach_uplane_packet(ctx, u_mbuf);
  
  du_pp_handle_absolute_symbol_tick(ctx, current_sym + 10);
  assert(du_pp_get_ready_prach_job_count(ctx) == 1);
  
  int16_t out_iq[139 * 2] = {0};
  du_pp_read_prach_iq(ctx, out_iq, NULL, NULL, NULL, NULL, NULL);
  
  assert(out_iq[0] == 100);
  
  cleanup_du_packet_processor(ctx);
  printf("PRACH missing symbol passed!\n");
}

void test_prach_compressed()
{
  printf("Testing PRACH compressed...\n");
  int mu = 1;
  void *ctx = init_du_packet_processor(mu, 273, 100, 300, 2, 0, 5, false, 1500, 0);
  assert(ctx != NULL);
  
  uint64_t current_sym = 1035;
  du_pp_handle_absolute_symbol_tick(ctx, current_sym);
  uint64_t target_sym = current_sym - 5;
  int num_symbols_per_frame = 10 * 2 * 14;
  int frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  int startSymbolId = target_sym % 14;
  int kbar = 4;
  int iq_width = 9;
  
  du_pp_expect_prach_occasion(ctx, target_sym, 1, slot_in_frame, 0, 1, 0, 12, FH_COMP_BFP, iq_width, kbar);
  
  struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
  size_t payload_len = FH_COMP_PRB_BYTES(iq_width) * 12;
  fill_ecpri_header(u_ecpri, &g_eaxcid_config, ECPRI_IQ_DATA, sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr) + sizeof(struct data_section_compression_hdr) + payload_len, 0, 0, 0, 0);
  
  struct radio_app_common_hdr *u_app = (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
  fill_radio_app_header(u_app, 0, XRAN_DIR_UL, frameId, slot_in_frame, startSymbolId, mu);
  
  struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
  fill_data_section_header(u_data, 12, 0, 1);
  
  struct data_section_compression_hdr *comp_hdr = (struct data_section_compression_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_compression_hdr));
  comp_hdr->ud_comp_hdr.ud_comp_meth = FH_COMP_BFP;
  comp_hdr->ud_comp_hdr.ud_iq_width = XRAN_CONVERT_IQWIDTH(iq_width);
  comp_hdr->rsrvd = 0;
  
  int8_t *iq = (int8_t *)rte_pktmbuf_append(u_mbuf, payload_len);
  
  int16_t input_iq[139 * 2] = {0};
  input_iq[0] = 500;
  input_iq[1] = -500;
  fh_compress_prach(FH_COMP_BFP, iq_width, kbar, input_iq, iq);
  
  du_pp_handle_prach_uplane_packet(ctx, u_mbuf);
  
  du_pp_handle_absolute_symbol_tick(ctx, current_sym + 10);
  assert(du_pp_get_ready_prach_job_count(ctx) == 1);
  
  int16_t out_iq[139 * 2] = {0};
  du_pp_read_prach_iq(ctx, out_iq, NULL, NULL, NULL, NULL, NULL);
  
  assert(out_iq[0] > 400 && out_iq[0] < 600);
  assert(out_iq[1] > -600 && out_iq[1] < -400);
  
  cleanup_du_packet_processor(ctx);
  printf("PRACH compressed passed!\n");
}

int main(int argc, char **argv)
{
  setup_dpdk(argc, argv);
  test_basic_round_trip();
  test_multi_fragment_reassembly();
  test_early_rejection();
  test_late_rejection();
  test_window_eviction();
  test_fdd_mode();
  test_tdd_mismatch();
  
  test_prach_basic();
  test_prach_multi_symbol();
  test_prach_missing_symbol();
  test_prach_compressed();
  
  printf("All DU Packet Processor tests passed!\n");
  return 0;
}
