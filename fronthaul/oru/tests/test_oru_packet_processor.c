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
#include "xran_pkt_up.h"
#include "oru_packet_processor.h"
#include "fh_compression.h"

#include "log.h"
#include "common/config/config_userapi.h"

// OAI Linkage Satisfiers
void exit_function(const char *file, const char *function, const int line, const char *s, const int assertflag)
{
  fprintf(stderr, "Error at %s:%s:%d - %s\n", file, function, line, s ? s : "None");
  exit(1);
}
configmodule_interface_t *uniqCfg = NULL;

/* Global config for tests, matches processor init */
struct xran_eaxcid_config g_eaxcid_config = {.mask_cuPortId = 0xF000,
                                             .mask_bandSectorId = 0x0F00,
                                             .mask_ccId = 0x00F0,
                                             .mask_ruPortId = 0x000F,
                                             .bit_cuPortId = 12,
                                             .bit_bandSectorId = 8,
                                             .bit_ccId = 4,
                                             .bit_ruPortId = 0};

struct rte_mempool *mp = NULL;

#define TEST_PRACH_KBAR 4

static int16_t g_ul_known_src[20 * FH_VALS_PER_PRB];
static int16_t g_ul_recovered_iq[20 * FH_VALS_PER_PRB];
static const int g_ul_test_num_prb = 20;
static const int g_ul_test_iq_width = 9;

void setup_dpdk(int argc, char **argv)
{
  int ret = rte_eal_init(argc, argv);
  assert(ret >= 0);
  logInit();
  mp = rte_pktmbuf_pool_create("test_pool", 1024, 0, 0, 10000, rte_socket_id());
  assert(mp != NULL);
}

void *test_alloc_mbuf(void *io_controller)
{
  return rte_pktmbuf_alloc(mp);
}

void *last_sent_mbuf = NULL;
size_t last_sent_size = 0;

void test_send_mbuf(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++) {
    if (last_sent_mbuf) {
      rte_pktmbuf_free(last_sent_mbuf);
    }
    last_sent_mbuf = mbufs[i];
    last_sent_size = rte_pktmbuf_pkt_len(mbufs[i]) + 14;
  }
}

// Places/sums read_dl_iq_streams()'s raw output into txdataF, and picks a representative
// beam_id per antenna (largest PRB coverage) for tests that check it.
static void test_assemble_streams(uint32_t **txdataF,
                                  int nb_tx,
                                  int num_prb,
                                  const dl_iq_stream_t *streams,
                                  int num_streams,
                                  uint16_t *beam_ids)
{
  for (int a = 0; a < nb_tx; a++) {
    memset(txdataF[a], 0, num_prb * 12 * sizeof(uint32_t));
  }
  int best_num_prb[nb_tx];
  memset(best_num_prb, 0, sizeof(best_num_prb));
  if (beam_ids) {
    memset(beam_ids, 0, nb_tx * sizeof(*beam_ids));
  }
  for (int i = 0; i < num_streams; i++) {
    const dl_iq_stream_t *s = &streams[i];
    if (s->ant_id >= (unsigned)nb_tx) {
      continue;
    }
    int16_t *dst = (int16_t *)&txdataF[s->ant_id][s->start_prb * 12];
    const int16_t *src = (const int16_t *)s->iq;
    for (int j = 0; j < s->num_prb * 12 * 2; j++) {
      dst[j] += src[j];
    }
    if (beam_ids && s->num_prb > best_num_prb[s->ant_id]) {
      best_num_prb[s->ant_id] = s->num_prb;
      beam_ids[s->ant_id] = s->beam_id;
    }
  }
}

void test_init_cleanup()
{
  printf("Testing init and cleanup...\n");
  void *ctx = init_packet_processor(1,
                                    273,
                                    200,
                                    400,
                                    100,
                                    300,
                                    2,
                                    2,
                                    0,
                                    0,
                                    5,
                                    test_alloc_mbuf,
                                    test_send_mbuf,
                                    NULL,
                                    1500,
                                    0,
                                    FH_COMP_NONE,
                                    0);
  assert(ctx != NULL);
  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.dl_tdd_mismatch == 0);
  assert(stats.ul_tdd_mismatch == 0);
  cleanup_packet_processor(ctx);
  printf("Init/cleanup passed!\n");
}

void test_cplane_timing_errors()
{
  printf("Testing C-Plane timing errors...\n");
  void *ctx = init_packet_processor(1,
                                    273,
                                    200,
                                    400,
                                    100,
                                    300,
                                    2,
                                    2,
                                    0,
                                    0,
                                    5,
                                    test_alloc_mbuf,
                                    test_send_mbuf,
                                    NULL,
                                    1500,
                                    0,
                                    FH_COMP_NONE,
                                    0);
  assert(ctx != NULL);

  // Set current symbol
  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);

  struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mp);
  assert(mbuf != NULL);

  // Create eCPRI header
  struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(mbuf, sizeof(struct xran_ecpri_hdr));
  ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0);
  ecpri->ecpri_seq_id.bits.seq_id = 1;

  // Create App Header (early packet)
  struct xran_cp_radioapp_section1_header *apphdr =
      (struct xran_cp_radioapp_section1_header *)rte_pktmbuf_append(mbuf, sizeof(struct xran_cp_radioapp_section1_header));
  memset(apphdr, 0, sizeof(*apphdr));
  apphdr->cmnhdr.field.dataDirection = XRAN_DIR_DL;
  apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;
  // Let's target a symbol in the future that is early
  int num_symbols_per_frame = 10 * (1 << 1) * 14;
  uint64_t target_sym = current_sym + 50; // Early
  apphdr->cmnhdr.field.frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  apphdr->cmnhdr.field.subframeId = slot_in_frame / (1 << 1);
  apphdr->cmnhdr.field.slotId = slot_in_frame % (1 << 1);
  apphdr->cmnhdr.field.startSymbolId = target_sym % 14;
  apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_1;
  apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);

  struct xran_cp_radioapp_section1 *sec =
      (struct xran_cp_radioapp_section1 *)rte_pktmbuf_append(mbuf, sizeof(struct xran_cp_radioapp_section1));
  memset(sec, 0, sizeof(*sec));
  sec->hdr.u.s1.numSymbol = 1;
  sec->hdr.u1.common.numPrbc = 0;
  // Need to handle endianness for section
  *((uint64_t *)sec) = rte_be_to_cpu_64(*((uint64_t *)sec));

  // Send early packet
  handle_cplane_packet(ctx, mbuf);

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_early == 1);
  assert(stats.dl_tdd_mismatch == 0);
  assert(stats.ul_tdd_mismatch == 0);

  cleanup_packet_processor(ctx);
  printf("C-Plane timing errors passed!\n");
}

void test_cplane_uplane_match()
{
  printf("Testing C-Plane / U-Plane match...\n");
  int mu = 1; // 30kHz
  int slots_per_subframe = 1 << mu;
  // T2a_cp ranges: 200 to 400 us (approx 5 to 11 symbols)
  // T2a_up ranges: 100 to 300 us (approx 2 to 8 symbols)
  void *ctx = init_packet_processor(mu,
                                    273,
                                    200,
                                    400,
                                    100,
                                    300,
                                    2,
                                    2,
                                    0,
                                    0,
                                    5,
                                    test_alloc_mbuf,
                                    test_send_mbuf,
                                    NULL,
                                    1500,
                                    0,
                                    FH_COMP_NONE,
                                    0);
  assert(ctx != NULL);

  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint64_t target_sym = current_sym + 7; // Good for C-plane (between 5 and 11)

  // 1. C-plane packet
  struct rte_mbuf *c_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_ecpri_hdr));
  ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0); // Ant 0

  struct xran_cp_radioapp_section1_header *apphdr =
      (struct xran_cp_radioapp_section1_header *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1_header));
  memset(apphdr, 0, sizeof(*apphdr));
  apphdr->cmnhdr.field.dataDirection = XRAN_DIR_DL;
  apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;

  int num_symbols_per_frame = 10 * slots_per_subframe * 14;
  apphdr->cmnhdr.field.frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  apphdr->cmnhdr.field.subframeId = slot_in_frame / slots_per_subframe;
  apphdr->cmnhdr.field.slotId = slot_in_frame % slots_per_subframe;
  apphdr->cmnhdr.field.startSymbolId = target_sym % 14;
  apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_1;
  apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);

  struct xran_cp_radioapp_section1 *sec =
      (struct xran_cp_radioapp_section1 *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1));
  memset(sec, 0, sizeof(*sec));
  sec->hdr.u.s1.numSymbol = 1;
  sec->hdr.u.s1.beamId = 37;
  sec->hdr.u1.common.numPrbc = 1;
  *((uint64_t *)sec) = rte_be_to_cpu_64(*((uint64_t *)sec));

  handle_cplane_packet(ctx, c_mbuf);

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_early == 0);
  assert(stats.cplane_err_late == 0);
  assert(stats.total_cplane == 1);
  assert(stats.dl_tdd_mismatch == 0);
  assert(stats.ul_tdd_mismatch == 0);

  // Advance time so U-plane is in valid range (target_sym = current_sym + 4 -> 4 is between 2 and 8)
  current_sym += 3;
  handle_absolute_symbol_tick(ctx, current_sym);

  // 2. U-plane packet
  struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
  u_ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0); // Ant 0

  struct radio_app_common_hdr *u_app =
      (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
  u_app->frame_id = (target_sym / num_symbols_per_frame) % 256;
  u_app->sf_slot_sym.subframe_id = slot_in_frame / slots_per_subframe;
  u_app->sf_slot_sym.slot_id = slot_in_frame % slots_per_subframe;
  u_app->sf_slot_sym.symb_id = target_sym % 14;
  u_app->sf_slot_sym.value = rte_cpu_to_be_16(u_app->sf_slot_sym.value);

  struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
  u_data->fields.num_prbu = 1; // 1 PRB for testing
  u_data->fields.start_prbu = 0;
  u_data->fields.sect_id = 0;
  u_data->fields.all_bits = rte_cpu_to_be_32(u_data->fields.all_bits);

  // IQ Data
  uint16_t *iq = (uint16_t *)rte_pktmbuf_append(u_mbuf, 1 * 12 * 4); // 1 PRB * 12 SC * 4 bytes
  assert(iq != NULL);
  iq[0] = 0xFF00;
  iq[1] = 0xEE11;
  iq[2] = 0xDD22;
  iq[3] = 0xCC33;

  handle_uplane_packet(ctx, u_mbuf);

  get_packet_processor_stats(ctx, &stats);
  assert(stats.uplane_err_early == 0);
  assert(stats.uplane_err_late == 0);
  assert(stats.dl_tdd_mismatch == 0);
  assert(stats.ul_tdd_mismatch == 0);

  // 3. Advance to trigger window expiry and job completion
  current_sym += 10;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t *txdataF[4] = {0};
  uint32_t output_iq[273 * 12] = {0};
  txdataF[0] = output_iq;

  int frame, slot, symbol;
  uint64_t hyper_frame;
  uint16_t beam_ids[1] = {0};
  dl_iq_stream_t dl_streams[MAX_DL_IQ_STREAMS_PER_SYMBOL];
  uint32_t dl_iq_arena[MAX_DL_IQ_STREAMS_PER_SYMBOL * 273 * 12];
  do {
    int n = read_dl_iq_streams(ctx, dl_streams, dl_iq_arena, MAX_DL_IQ_STREAMS_PER_SYMBOL, &hyper_frame, &frame, &slot, &symbol);
    test_assemble_streams(txdataF, 1, 273, dl_streams, n, beam_ids);
  } while (!(frame == (target_sym / num_symbols_per_frame) % 1024 && slot == slot_in_frame && symbol == target_sym % 14));

  assert(symbol == target_sym % 14);
  assert(beam_ids[0] == 37);
  uint16_t *out_iq = (uint16_t *)output_iq;
  assert(out_iq[0] == 0x00FF);
  assert(out_iq[1] == 0x11EE);
  assert(out_iq[2] == 0x22DD);
  assert(out_iq[3] == 0x33CC);

  cleanup_packet_processor(ctx);
  printf("C-Plane / U-Plane match passed!\n");
}

// start_prbu/num_prbu are wire-controlled. combine_dl_streams() later writes num_prb PRBs at
// start_prb into a txDataF buffer sized for ctx->num_prb PRBs - a packet claiming a range beyond
// ctx->num_prb must be rejected up front, not handed through to that write.
void test_uplane_prb_range_rejected()
{
  printf("Testing U-Plane PRB range rejection...\n");
  int mu = 1;
  void *ctx = init_packet_processor(mu, 273, 200, 400, 100, 300, 2, 2, 0, 0, 5, test_alloc_mbuf, test_send_mbuf, NULL, 1500, 0, FH_COMP_NONE, 0);
  assert(ctx != NULL);

  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);

  struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
  u_ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0); // Ant 0

  struct radio_app_common_hdr *u_app =
      (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
  u_app->frame_id = 0;
  u_app->sf_slot_sym.symb_id = 0;
  u_app->sf_slot_sym.value = rte_cpu_to_be_16(u_app->sf_slot_sym.value);

  struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
  u_data->fields.num_prbu = 10; // 270 + 10 = 280 > ctx->num_prb (273)
  u_data->fields.start_prbu = 270;
  u_data->fields.sect_id = 0;
  u_data->fields.all_bits = rte_cpu_to_be_32(u_data->fields.all_bits);

  uint16_t *iq = (uint16_t *)rte_pktmbuf_append(u_mbuf, 10 * 12 * 4);
  assert(iq != NULL);
  memset(iq, 0, 10 * 12 * 4);

  handle_uplane_packet(ctx, u_mbuf);

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.uplane_err_prb_range == 1);
  assert(stats.uplane_missing_cplane == 0); // rejected before the C-Plane stream lookup even ran
  assert(stats.uplane_err_short_payload == 0);

  cleanup_packet_processor(ctx);
  printf("U-Plane PRB range rejection passed!\n");
}

// xran_extract_iq_samples() returns the packet length still remaining after stripping headers -
// a truncated packet (fewer bytes than num_prbu/iqWidth/compMeth implies) must be rejected before
// its iq_data pointer is handed off to the deferred unpack_iq() in read_dl_iq_streams().
void test_uplane_short_payload_rejected()
{
  printf("Testing U-Plane short payload rejection...\n");
  int mu = 1;
  void *ctx = init_packet_processor(mu, 273, 200, 400, 100, 300, 2, 2, 0, 0, 5, test_alloc_mbuf, test_send_mbuf, NULL, 1500, 0, FH_COMP_NONE, 0);
  assert(ctx != NULL);

  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);

  struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
  u_ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0); // Ant 0

  struct radio_app_common_hdr *u_app =
      (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
  u_app->frame_id = 0;
  u_app->sf_slot_sym.symb_id = 0;
  u_app->sf_slot_sym.value = rte_cpu_to_be_16(u_app->sf_slot_sym.value);

  struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
  u_data->fields.num_prbu = 1; // uncompressed 1 PRB needs 12 * 2 * sizeof(uint16_t) = 48 bytes
  u_data->fields.start_prbu = 0;
  u_data->fields.sect_id = 0;
  u_data->fields.all_bits = rte_cpu_to_be_32(u_data->fields.all_bits);

  // Only 8 of the required 48 bytes present - truncated packet.
  uint16_t *iq = (uint16_t *)rte_pktmbuf_append(u_mbuf, 8);
  assert(iq != NULL);
  memset(iq, 0, 8);

  handle_uplane_packet(ctx, u_mbuf);

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.uplane_err_short_payload == 1);
  assert(stats.uplane_err_prb_range == 0);
  assert(stats.uplane_missing_cplane == 0);

  cleanup_packet_processor(ctx);
  printf("U-Plane short payload rejection passed!\n");
}

// Two C-Plane sections for one beam (disjoint PRB ranges) must merge into one stream. A repeated
// section_id is rejected as a duplicate. A third section with a *different* beam on the same eaxc
// (sub-band precoding) is accepted as a genuinely separate stream, not merged or rejected.
void test_dl_multi_section_same_beam()
{
  printf("Testing DL multi-section same-beam merge...\n");
  int mu = 1; // 30kHz
  int slots_per_subframe = 1 << mu;
  void *ctx = init_packet_processor(mu,
                                    273,
                                    200,
                                    400,
                                    100,
                                    300,
                                    2,
                                    2,
                                    0,
                                    0,
                                    5,
                                    test_alloc_mbuf,
                                    test_send_mbuf,
                                    NULL,
                                    1500,
                                    0,
                                    FH_COMP_NONE,
                                    0);
  assert(ctx != NULL);

  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint64_t target_sym = current_sym + 7;
  int num_symbols_per_frame = 10 * slots_per_subframe * 14;
  int frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  int subframeId = slot_in_frame / slots_per_subframe;
  int slotId = slot_in_frame % slots_per_subframe;
  int startSymbolId = target_sym % 14;
  const uint16_t beam_id = 42;

  // Two C-Plane sections for the same eaxc/beam, disjoint PRB ranges, distinct sectionId.
  int section_ids[2] = {0, 1};
  int start_prbc[2] = {0, 10};
  int num_prbc[2] = {10, 10};
  for (int i = 0; i < 2; i++) {
    struct rte_mbuf *c_mbuf = rte_pktmbuf_alloc(mp);
    struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_ecpri_hdr));
    ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0);

    struct xran_cp_radioapp_section1_header *apphdr =
        (struct xran_cp_radioapp_section1_header *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1_header));
    memset(apphdr, 0, sizeof(*apphdr));
    apphdr->cmnhdr.field.dataDirection = XRAN_DIR_DL;
    apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;
    apphdr->cmnhdr.field.frameId = frameId;
    apphdr->cmnhdr.field.subframeId = subframeId;
    apphdr->cmnhdr.field.slotId = slotId;
    apphdr->cmnhdr.field.startSymbolId = startSymbolId;
    apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_1;
    apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);

    struct xran_cp_radioapp_section1 *sec =
        (struct xran_cp_radioapp_section1 *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1));
    memset(sec, 0, sizeof(*sec));
    sec->hdr.u.s1.numSymbol = 1;
    sec->hdr.u.s1.beamId = beam_id;
    sec->hdr.u1.common.sectionId = section_ids[i];
    sec->hdr.u1.common.startPrbc = start_prbc[i];
    sec->hdr.u1.common.numPrbc = num_prbc[i];
    *((uint64_t *)sec) = rte_be_to_cpu_64(*((uint64_t *)sec));
    handle_cplane_packet(ctx, c_mbuf);
  }

  // Re-declaring sectionId 0 must be rejected as a duplicate, not merged as a third section.
  {
    struct rte_mbuf *c_mbuf = rte_pktmbuf_alloc(mp);
    struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_ecpri_hdr));
    ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0);

    struct xran_cp_radioapp_section1_header *apphdr =
        (struct xran_cp_radioapp_section1_header *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1_header));
    memset(apphdr, 0, sizeof(*apphdr));
    apphdr->cmnhdr.field.dataDirection = XRAN_DIR_DL;
    apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;
    apphdr->cmnhdr.field.frameId = frameId;
    apphdr->cmnhdr.field.subframeId = subframeId;
    apphdr->cmnhdr.field.slotId = slotId;
    apphdr->cmnhdr.field.startSymbolId = startSymbolId;
    apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_1;
    apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);

    struct xran_cp_radioapp_section1 *sec =
        (struct xran_cp_radioapp_section1 *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1));
    memset(sec, 0, sizeof(*sec));
    sec->hdr.u.s1.numSymbol = 1;
    sec->hdr.u.s1.beamId = beam_id;
    sec->hdr.u1.common.sectionId = 0;
    sec->hdr.u1.common.startPrbc = 0;
    sec->hdr.u1.common.numPrbc = 10;
    *((uint64_t *)sec) = rte_be_to_cpu_64(*((uint64_t *)sec));
    handle_cplane_packet(ctx, c_mbuf);
  }

  const uint16_t beam_id2 = beam_id + 1;
  // A third section for the same eaxc, new sectionId, a *different* beamId, and a disjoint PRB
  // range - accepted as a genuinely separate stream (sub-band precoding), not rejected.
  {
    struct rte_mbuf *c_mbuf = rte_pktmbuf_alloc(mp);
    struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_ecpri_hdr));
    ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0);

    struct xran_cp_radioapp_section1_header *apphdr =
        (struct xran_cp_radioapp_section1_header *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1_header));
    memset(apphdr, 0, sizeof(*apphdr));
    apphdr->cmnhdr.field.dataDirection = XRAN_DIR_DL;
    apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;
    apphdr->cmnhdr.field.frameId = frameId;
    apphdr->cmnhdr.field.subframeId = subframeId;
    apphdr->cmnhdr.field.slotId = slotId;
    apphdr->cmnhdr.field.startSymbolId = startSymbolId;
    apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_1;
    apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);

    struct xran_cp_radioapp_section1 *sec =
        (struct xran_cp_radioapp_section1 *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1));
    memset(sec, 0, sizeof(*sec));
    sec->hdr.u.s1.numSymbol = 1;
    sec->hdr.u.s1.beamId = beam_id2;
    sec->hdr.u1.common.sectionId = 2; // new sectionId, not a duplicate
    sec->hdr.u1.common.startPrbc = 20;
    sec->hdr.u1.common.numPrbc = 10;
    *((uint64_t *)sec) = rte_be_to_cpu_64(*((uint64_t *)sec));
    handle_cplane_packet(ctx, c_mbuf);
  }

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_dup == 1);
  assert(stats.cplane_err_dup_dl == 1);
  assert(stats.dl_stream_pool_exhausted == 0);
  assert(stats.dl_stream_sections_exhausted == 0);

  current_sym += 3;
  handle_absolute_symbol_tick(ctx, current_sym);

  // Three U-Plane packets: two for beam_id's merged sections, one for beam_id2's separate section.
  int all_section_ids[3] = {section_ids[0], section_ids[1], 2};
  int all_start_prbc[3] = {start_prbc[0], start_prbc[1], 20};
  int all_num_prbc[3] = {num_prbc[0], num_prbc[1], 10};
  uint16_t patterns[3] = {0x1111, 0x2222, 0x3333};
  for (int i = 0; i < 3; i++) {
    struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
    struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
    u_ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0);

    struct radio_app_common_hdr *u_app =
        (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
    u_app->frame_id = frameId;
    u_app->sf_slot_sym.subframe_id = subframeId;
    u_app->sf_slot_sym.slot_id = slotId;
    u_app->sf_slot_sym.symb_id = startSymbolId;
    u_app->sf_slot_sym.value = rte_cpu_to_be_16(u_app->sf_slot_sym.value);

    struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
    u_data->fields.num_prbu = all_num_prbc[i];
    u_data->fields.start_prbu = all_start_prbc[i];
    u_data->fields.sect_id = all_section_ids[i];
    u_data->fields.all_bits = rte_cpu_to_be_32(u_data->fields.all_bits);

    uint16_t *iq = (uint16_t *)rte_pktmbuf_append(u_mbuf, all_num_prbc[i] * 12 * 4);
    assert(iq != NULL);
    for (int j = 0; j < all_num_prbc[i] * 12 * 2; j++) {
      iq[j] = rte_cpu_to_be_16(patterns[i]);
    }

    handle_uplane_packet(ctx, u_mbuf);
  }

  get_packet_processor_stats(ctx, &stats);
  assert(stats.uplane_err_early == 0);
  assert(stats.uplane_err_late == 0);
  assert(stats.uplane_missing_cplane == 0);

  current_sym += 10;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t *txdataF[4] = {0};
  uint32_t output_iq[273 * 12] = {0};
  txdataF[0] = output_iq;

  int frame, slot, symbol;
  uint64_t hyper_frame;
  uint16_t beam_ids[1] = {0};
  dl_iq_stream_t dl_streams[MAX_DL_IQ_STREAMS_PER_SYMBOL];
  uint32_t dl_iq_arena[MAX_DL_IQ_STREAMS_PER_SYMBOL * 273 * 12];
  do {
    int n = read_dl_iq_streams(ctx, dl_streams, dl_iq_arena, MAX_DL_IQ_STREAMS_PER_SYMBOL, &hyper_frame, &frame, &slot, &symbol);
    test_assemble_streams(txdataF, 1, 273, dl_streams, n, beam_ids);
  } while (!(frame == frameId && slot == slot_in_frame && symbol == startSymbolId));

  // beam_id covers 20 PRBs total (2 merged sections) vs. beam_id2's 10 - the representative beam
  // for a single-beam-per-antenna consumer must be the larger one.
  assert(beam_ids[0] == beam_id);
  uint16_t *out_iq = (uint16_t *)output_iq;
  for (int prb = 0; prb < 30; prb++) {
    uint16_t expected = prb < 10 ? patterns[0] : prb < 20 ? patterns[1] : patterns[2];
    for (int sc = 0; sc < 12; sc++) {
      int idx = prb * 12 + sc;
      assert(out_iq[2 * idx] == expected);
      assert(out_iq[2 * idx + 1] == expected);
    }
  }

  get_packet_processor_stats(ctx, &stats);
  assert(stats.dl_stream_pool_exhausted == 0);
  assert(stats.dl_stream_sections_exhausted == 0);
  assert(stats.dl_iq_streams_pool_exhausted == 0);

  cleanup_packet_processor(ctx);
  printf("DL multi-section same-beam merge passed!\n");
}

void test_frame_wrap_around()
{
  printf("Testing frame wrap around...\n");
  int mu = 1; // 30kHz
  int slots_per_subframe = 1 << mu;
  void *ctx = init_packet_processor(mu,
                                    273,
                                    200,
                                    400,
                                    100,
                                    300,
                                    2,
                                    2,
                                    0,
                                    0,
                                    5,
                                    test_alloc_mbuf,
                                    test_send_mbuf,
                                    NULL,
                                    1500,
                                    0,
                                    FH_COMP_NONE,
                                    0);
  assert(ctx != NULL);

  // Total symbols = 5 slots * 14 = 70.
  // DL symbols: 0 to 2*14-1 = 27.
  // target_sym = 71680. 71680 % 70 = 0 (DL symbol)
  uint64_t current_sym = 256 * 14 * 10 * slots_per_subframe - 7;
  handle_absolute_symbol_tick(ctx, current_sym);

  int num_symbols_per_frame = 10 * slots_per_subframe * 14;
  uint64_t target_sym = current_sym + 7;

  // 1. C-plane packet
  struct rte_mbuf *c_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_ecpri_hdr));
  ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0);

  struct xran_cp_radioapp_section1_header *apphdr =
      (struct xran_cp_radioapp_section1_header *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1_header));
  memset(apphdr, 0, sizeof(*apphdr));
  apphdr->cmnhdr.field.dataDirection = XRAN_DIR_DL;
  apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;

  apphdr->cmnhdr.field.frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  apphdr->cmnhdr.field.subframeId = slot_in_frame / slots_per_subframe;
  apphdr->cmnhdr.field.slotId = slot_in_frame % slots_per_subframe;
  apphdr->cmnhdr.field.startSymbolId = target_sym % 14;
  apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_1;
  apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);

  struct xran_cp_radioapp_section1 *sec =
      (struct xran_cp_radioapp_section1 *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1));
  memset(sec, 0, sizeof(*sec));
  sec->hdr.u.s1.numSymbol = 1;
  sec->hdr.u1.common.numPrbc = 1;
  *((uint64_t *)sec) = rte_be_to_cpu_64(*((uint64_t *)sec));

  handle_cplane_packet(ctx, c_mbuf);

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_early == 0);
  assert(stats.cplane_err_late == 0);
  assert(stats.total_cplane == 1);
  assert(stats.dl_tdd_mismatch == 0);
  assert(stats.ul_tdd_mismatch == 0);

  // Advance time so U-plane is in valid range
  current_sym += 3; // 279
  handle_absolute_symbol_tick(ctx, current_sym);

  // 2. U-plane packet
  struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
  u_ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0); // Ant 0

  struct radio_app_common_hdr *u_app =
      (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
  u_app->frame_id = (target_sym / num_symbols_per_frame) % 256;
  u_app->sf_slot_sym.subframe_id = slot_in_frame / slots_per_subframe;
  u_app->sf_slot_sym.slot_id = slot_in_frame % slots_per_subframe;
  u_app->sf_slot_sym.symb_id = target_sym % 14;
  u_app->sf_slot_sym.value = rte_cpu_to_be_16(u_app->sf_slot_sym.value);

  struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
  u_data->fields.num_prbu = 1; // 1 PRB for testing
  u_data->fields.start_prbu = 0;
  u_data->fields.sect_id = 0;
  u_data->fields.all_bits = rte_cpu_to_be_32(u_data->fields.all_bits);

  handle_uplane_packet(ctx, u_mbuf);

  get_packet_processor_stats(ctx, &stats);
  assert(stats.uplane_err_early == 0);
  assert(stats.uplane_err_late == 0);
  assert(stats.dl_tdd_mismatch == 0);
  assert(stats.ul_tdd_mismatch == 0);

  // 3. Advance to trigger window expiry and job completion
  current_sym += 10;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t *txdataF[4] = {0};
  uint32_t output_iq[273 * 12] = {0};
  txdataF[0] = output_iq;

  int frame, slot, symbol;
  uint64_t hyper_frame;
  dl_iq_stream_t dl_streams[MAX_DL_IQ_STREAMS_PER_SYMBOL];
  uint32_t dl_iq_arena[MAX_DL_IQ_STREAMS_PER_SYMBOL * 273 * 12];
  do {
    int n = read_dl_iq_streams(ctx, dl_streams, dl_iq_arena, MAX_DL_IQ_STREAMS_PER_SYMBOL, &hyper_frame, &frame, &slot, &symbol);
    test_assemble_streams(txdataF, 1, 273, dl_streams, n, NULL);
  } while (!(frame == (target_sym / num_symbols_per_frame) % 1024 && slot == slot_in_frame && symbol == target_sym % 14));

  assert(frame == (target_sym / num_symbols_per_frame) % 1024);
  assert(symbol == target_sym % 14);

  cleanup_packet_processor(ctx);
  printf("Frame wrap around passed!\n");
}

void test_cplane_14_symbols()
{
  printf("Testing 1 C-plane message for 14 symbols...\n");
  int mu = 1; // 30kHz
  // T2a_cp_max is widened to 900uS: a single section spanning 14 symbols needs its LAST symbol to
  // still be within T2a_min_cp of "now" too (each symbol has its own individual C-Plane deadline,
  // not just the section's first symbol), so the window has to be at least ~14 symbols wide.
  void *ctx = init_packet_processor(mu,
                                    273,
                                    200,
                                    900,
                                    100,
                                    300,
                                    5,
                                    0,
                                    0,
                                    0,
                                    5,
                                    test_alloc_mbuf,
                                    test_send_mbuf,
                                    NULL,
                                    1500,
                                    0,
                                    FH_COMP_NONE,
                                    0);
  assert(ctx != NULL);

  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);

  // T2a_min_cp_sym_diff=5, T2a_max_cp_sym_diff=25. Symbol i of the section sits at diff+i, so with
  // diff=10 the first symbol (i=0) is at 10 (>=5) and the last (i=13) is at 23 (<=25) - both stay
  // comfortably inside the window.
  uint64_t target_sym = current_sym + 10;

  // 1. C-plane packet with numSymbol = 14
  struct rte_mbuf *c_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_ecpri_hdr));
  ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0); // Ant 0

  struct xran_cp_radioapp_section1_header *apphdr =
      (struct xran_cp_radioapp_section1_header *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1_header));
  memset(apphdr, 0, sizeof(*apphdr));
  apphdr->cmnhdr.field.dataDirection = XRAN_DIR_DL;
  apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;

  int num_symbols_per_frame = 10 * 2 * 14;
  apphdr->cmnhdr.field.frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  apphdr->cmnhdr.field.subframeId = slot_in_frame / 2;
  apphdr->cmnhdr.field.slotId = slot_in_frame % 2;
  apphdr->cmnhdr.field.startSymbolId = target_sym % 14;
  apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_1;
  apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);

  struct xran_cp_radioapp_section1 *sec =
      (struct xran_cp_radioapp_section1 *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1));
  memset(sec, 0, sizeof(*sec));
  sec->hdr.u.s1.numSymbol = 14;
  sec->hdr.u1.common.numPrbc = 1;
  *((uint64_t *)sec) = rte_be_to_cpu_64(*((uint64_t *)sec));

  handle_cplane_packet(ctx, c_mbuf);

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_early == 0);
  assert(stats.cplane_err_late == 0);
  assert(stats.total_cplane == 1);
  assert(stats.dl_tdd_mismatch == 0);
  assert(stats.ul_tdd_mismatch == 0);

  // 2. Advance time so we can send U-plane for ALL 14 symbols. target_sym - current_sym must land
  // within U-Plane's own [T2a_min_up_dl_sym_diff, T2a_max_up_dl_sym_diff] = [2, 8] window and stay
  // there as both advance together below.
  current_sym += 6;
  handle_absolute_symbol_tick(ctx, current_sym);

  for (int i = 0; i < 14; i++) {
    struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
    struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
    u_ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0); // Ant 0

    struct radio_app_common_hdr *u_app =
        (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
    uint64_t sym_i = target_sym + i;
    u_app->frame_id = (sym_i / num_symbols_per_frame) % 256;
    int sf_slot_in_frame = (sym_i % num_symbols_per_frame) / 14;
    u_app->sf_slot_sym.subframe_id = sf_slot_in_frame / 2;
    u_app->sf_slot_sym.slot_id = sf_slot_in_frame % 2;
    u_app->sf_slot_sym.symb_id = sym_i % 14;
    u_app->sf_slot_sym.value = rte_cpu_to_be_16(u_app->sf_slot_sym.value);

    struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
    u_data->fields.num_prbu = 1; // 1 PRB for testing
    u_data->fields.start_prbu = 0;
    u_data->fields.sect_id = 0;
    u_data->fields.all_bits = rte_cpu_to_be_32(u_data->fields.all_bits);

    // IQ Data
    uint16_t *iq = (uint16_t *)rte_pktmbuf_append(u_mbuf, 1 * 12 * 4); // 1 PRB * 12 SC * 4 bytes
    assert(iq != NULL);
    iq[0] = rte_cpu_to_be_16(i);

    handle_uplane_packet(ctx, u_mbuf);

    // Advance tick so next symbol's U-plane is timely
    current_sym++;
    handle_absolute_symbol_tick(ctx, current_sym);
  }

  get_packet_processor_stats(ctx, &stats);
  assert(stats.uplane_err_early == 0);
  assert(stats.uplane_err_late == 0);
  assert(stats.dl_tdd_mismatch == 0);
  assert(stats.ul_tdd_mismatch == 0);

  // 3. Advance to trigger window expiry and job completion
  current_sym += 15;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t *txdataF[4] = {0};
  uint32_t output_iq[273 * 12] = {0};
  txdataF[0] = output_iq;

  dl_iq_stream_t dl_streams[MAX_DL_IQ_STREAMS_PER_SYMBOL];
  uint32_t dl_iq_arena[MAX_DL_IQ_STREAMS_PER_SYMBOL * 273 * 12];
  for (int i = 0; i < 14; i++) {
    int frame, slot, symbol;
    uint64_t hyper_frame;
    uint64_t sym_i = target_sym + i;
    int sym_i_slot_in_frame = (sym_i % num_symbols_per_frame) / 14;
    do {
      int n = read_dl_iq_streams(ctx, dl_streams, dl_iq_arena, MAX_DL_IQ_STREAMS_PER_SYMBOL, &hyper_frame, &frame, &slot, &symbol);
      test_assemble_streams(txdataF, 1, 273, dl_streams, n, NULL);
    } while (!(frame == (sym_i / num_symbols_per_frame) % 1024 && slot == sym_i_slot_in_frame && symbol == sym_i % 14));

    assert(frame == (sym_i / num_symbols_per_frame) % 1024);
    assert(symbol == sym_i % 14);
    uint16_t *out_iq = (uint16_t *)output_iq;
    assert(out_iq[0] == i);
  }

  cleanup_packet_processor(ctx);
  printf("1 C-plane message for 14 symbols passed!\n");
}

void test_other_bw_4ant_prb_offset()
{
  printf("Testing other bandwidth, 4 antennas, and PRB offset...\n");
  int mu = 1; // 30kHz
  int num_prb = 106; // Different bandwidth
  void *ctx = init_packet_processor(mu,
                                    num_prb,
                                    200,
                                    400,
                                    100,
                                    300,
                                    4,
                                    4,
                                    0,
                                    0,
                                    5,
                                    test_alloc_mbuf,
                                    test_send_mbuf,
                                    NULL,
                                    1500,
                                    0,
                                    FH_COMP_NONE,
                                    0);
  assert(ctx != NULL);

  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint64_t target_sym = current_sym + 7;

  int num_symbols_per_frame = 10 * 2 * 14;
  int frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  int subframeId = slot_in_frame / 2;
  int slotId = slot_in_frame % 2;
  int startSymbolId = target_sym % 14;

  // Send 1 C-plane packet for ALL 4 antennas using eAxC ID
  // Actually, O-RAN allows 1 C-plane packet per antenna, let's just send 4 C-plane packets.
  for (int ant = 0; ant < 4; ant++) {
    struct rte_mbuf *c_mbuf = rte_pktmbuf_alloc(mp);
    struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_ecpri_hdr));
    ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, ant);

    struct xran_cp_radioapp_section1_header *apphdr =
        (struct xran_cp_radioapp_section1_header *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1_header));
    memset(apphdr, 0, sizeof(*apphdr));
    apphdr->cmnhdr.field.dataDirection = XRAN_DIR_DL;
    apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;
    apphdr->cmnhdr.field.frameId = frameId;
    apphdr->cmnhdr.field.subframeId = subframeId;
    apphdr->cmnhdr.field.slotId = slotId;
    apphdr->cmnhdr.field.startSymbolId = startSymbolId;
    apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_1;
    apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);

    struct xran_cp_radioapp_section1 *sec =
        (struct xran_cp_radioapp_section1 *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1));
    memset(sec, 0, sizeof(*sec));
    sec->hdr.u.s1.numSymbol = 1;
    sec->hdr.u1.common.startPrbc = 20; // PRB offset
    sec->hdr.u1.common.numPrbc = 20; // subset of PRBs
    *((uint64_t *)sec) = rte_be_to_cpu_64(*((uint64_t *)sec));
    handle_cplane_packet(ctx, c_mbuf);
  }

  current_sym += 3;
  handle_absolute_symbol_tick(ctx, current_sym);

  // Send U-plane packets for all 4 antennas
  for (int ant = 0; ant < 4; ant++) {
    struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
    struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
    u_ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, ant);

    struct radio_app_common_hdr *u_app =
        (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
    u_app->frame_id = frameId;
    u_app->sf_slot_sym.subframe_id = subframeId;
    u_app->sf_slot_sym.slot_id = slotId;
    u_app->sf_slot_sym.symb_id = startSymbolId;
    u_app->sf_slot_sym.value = rte_cpu_to_be_16(u_app->sf_slot_sym.value);

    struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
    u_data->fields.num_prbu = 20;
    u_data->fields.start_prbu = 20; // Match C-plane
    u_data->fields.sect_id = 0;
    u_data->fields.all_bits = rte_cpu_to_be_32(u_data->fields.all_bits);

    uint16_t *iq = (uint16_t *)rte_pktmbuf_append(u_mbuf, 20 * 12 * 4); // 20 PRBs
    assert(iq != NULL);
    for (int j = 0; j < 20 * 12 * 2; j++) {
      iq[j] = rte_cpu_to_be_16(ant + 1); // Specific pattern per antenna
    }

    handle_uplane_packet(ctx, u_mbuf);
  }

  current_sym += 10;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t *txdataF[4] = {0};
  uint32_t out_iq0[106 * 12] = {0};
  uint32_t out_iq1[106 * 12] = {0};
  uint32_t out_iq2[106 * 12] = {0};
  uint32_t out_iq3[106 * 12] = {0};
  txdataF[0] = out_iq0;
  txdataF[1] = out_iq1;
  txdataF[2] = out_iq2;
  txdataF[3] = out_iq3;

  int frame, slot, symbol;
  uint64_t hyper_frame;
  dl_iq_stream_t dl_streams[MAX_DL_IQ_STREAMS_PER_SYMBOL];
  uint32_t dl_iq_arena[MAX_DL_IQ_STREAMS_PER_SYMBOL * 106 * 12];
  do {
    int n = read_dl_iq_streams(ctx, dl_streams, dl_iq_arena, MAX_DL_IQ_STREAMS_PER_SYMBOL, &hyper_frame, &frame, &slot, &symbol);
    test_assemble_streams(txdataF, 4, 106, dl_streams, n, NULL);
  } while (!(frame == frameId && symbol == startSymbolId));

  // Verify memory contents for each antenna
  for (int ant = 0; ant < 4; ant++) {
    uint16_t *out_iq = (uint16_t *)txdataF[ant];
    for (int prb = 0; prb < num_prb; prb++) {
      for (int sc = 0; sc < 12; sc++) {
        int idx = prb * 12 + sc;
        if (prb >= 20 && prb < 40) {
          assert(out_iq[2 * idx] == ant + 1);
          assert(out_iq[2 * idx + 1] == ant + 1);
        } else {
          // Unused PRBs should be zero
          assert(out_iq[2 * idx] == 0);
          assert(out_iq[2 * idx + 1] == 0);
        }
      }
    }
  }

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.dl_tdd_mismatch == 0);
  assert(stats.ul_tdd_mismatch == 0);

  cleanup_packet_processor(ctx);
  printf("Other bandwidth, 4 antennas, and PRB offset passed!\n");
}

int g_packets_sent = 0;

void test_send_mbuf_no_frag(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++) {
    struct rte_mbuf *mbuf = mbufs[i];
    size_t size = rte_pktmbuf_pkt_len(mbuf) + 14;
    g_packets_sent++;
    struct xran_ecpri_hdr *sent_ecpri = rte_pktmbuf_mtod((struct rte_mbuf *)mbuf, struct xran_ecpri_hdr *);
    struct radio_app_common_hdr *sent_app = (struct radio_app_common_hdr *)(sent_ecpri + 1);
    struct data_section_hdr *sent_sec = (struct data_section_hdr *)(sent_app + 1);
    sent_sec->fields.all_bits = rte_be_to_cpu_32(sent_sec->fields.all_bits);

    size_t expected_header_len = sizeof(struct xran_ecpri_hdr) + sizeof(struct radio_app_common_hdr)
                                 + sizeof(struct data_section_hdr);

    // In this test, we expect exactly 20 PRBs in a single packet
    assert(sent_sec->fields.num_prbu == (uint8_t)XRAN_CONVERT_NUMPRBC(20));
    assert(sent_sec->fields.start_prbu == 0);
    // size includes 14 bytes ethernet header
    assert(size == expected_header_len + 20 * 12 * 4 + 14);

    rte_pktmbuf_free(mbuf);
  }
}

void test_send_mbuf_prach(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++) {
    struct rte_mbuf *mbuf = mbufs[i];
    size_t size = rte_pktmbuf_pkt_len(mbuf) + 14;
    g_packets_sent++;
    struct xran_ecpri_hdr *sent_ecpri = rte_pktmbuf_mtod((struct rte_mbuf *)mbuf, struct xran_ecpri_hdr *);
    struct radio_app_common_hdr *sent_app = (struct radio_app_common_hdr *)(sent_ecpri + 1);
    struct data_section_hdr *sent_sec = (struct data_section_hdr *)(sent_app + 1);
    sent_sec->fields.all_bits = rte_be_to_cpu_32(sent_sec->fields.all_bits);

    size_t expected_header_len =
        sizeof(struct xran_ecpri_hdr) + sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr);

    // In this test, we expect exactly 20 PRBs in the header
    assert(sent_sec->fields.num_prbu == (uint8_t)XRAN_CONVERT_NUMPRBC(20));
    assert(sent_sec->fields.start_prbu == 0);
    // size includes 14 bytes ethernet header.
    assert(size == expected_header_len + (139 + TEST_PRACH_KBAR) * 4 + 14);
    uint16_t *payload = (uint16_t *)(sent_sec + 1);
    for (int j = 0; j < TEST_PRACH_KBAR; j++)
      assert(payload[j] == 0);
    assert(rte_be_to_cpu_16(payload[TEST_PRACH_KBAR]) == 1);

    rte_pktmbuf_free(mbuf);
  }
}

void test_send_mbuf_frag(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++) {
    struct rte_mbuf *mbuf = mbufs[i];
    size_t size = rte_pktmbuf_pkt_len(mbuf) + 14;
    g_packets_sent++;
    struct xran_ecpri_hdr *sent_ecpri = rte_pktmbuf_mtod((struct rte_mbuf *)mbuf, struct xran_ecpri_hdr *);
    struct radio_app_common_hdr *sent_app = (struct radio_app_common_hdr *)(sent_ecpri + 1);
    struct data_section_hdr *sent_sec = (struct data_section_hdr *)(sent_app + 1);
    sent_sec->fields.all_bits = rte_be_to_cpu_32(sent_sec->fields.all_bits);

    size_t expected_header_len = sizeof(struct xran_ecpri_hdr) + sizeof(struct radio_app_common_hdr)
                                 + sizeof(struct data_section_hdr);

    if (g_packets_sent == 1) {
      // First fragment: 30 PRBs
      assert(sent_sec->fields.num_prbu == (uint8_t)XRAN_CONVERT_NUMPRBC(30));
      assert(sent_sec->fields.start_prbu == 0);
      assert(size == expected_header_len + 30 * 12 * 4 + 14);
    } else if (g_packets_sent == 2) {
      // Second fragment: 10 PRBs
      assert(sent_sec->fields.num_prbu == (uint8_t)XRAN_CONVERT_NUMPRBC(10));
      assert(sent_sec->fields.start_prbu == 30);
      assert(size == expected_header_len + 10 * 12 * 4 + 14);
    }
    rte_pktmbuf_free(mbuf);
  }
}

void test_send_mbuf_large_mtu(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++) {
    struct rte_mbuf *mbuf = mbufs[i];
    size_t size = rte_pktmbuf_pkt_len(mbuf) + 14;
    g_packets_sent++;
    struct xran_ecpri_hdr *sent_ecpri = rte_pktmbuf_mtod((struct rte_mbuf *)mbuf, struct xran_ecpri_hdr *);
    struct radio_app_common_hdr *sent_app = (struct radio_app_common_hdr *)(sent_ecpri + 1);
    struct data_section_hdr *sent_sec = (struct data_section_hdr *)(sent_app + 1);
    sent_sec->fields.all_bits = rte_be_to_cpu_32(sent_sec->fields.all_bits);

    size_t expected_header_len = sizeof(struct xran_ecpri_hdr) + sizeof(struct radio_app_common_hdr)
                                 + sizeof(struct data_section_hdr);

    // With MTU 9600, 100 PRBs should fit in a single packet
    assert(g_packets_sent == 1);
    assert(sent_sec->fields.num_prbu == (uint8_t)XRAN_CONVERT_NUMPRBC(100));
    assert(sent_sec->fields.start_prbu == 0);

    // size should be trimmed to actual data, not full MTU
    size_t expected_data_len = 100 * 12 * 4;
    size_t expected_total_size = expected_header_len + expected_data_len + 14; // +14 for Ethernet
    assert(size == expected_total_size);
    assert(size < 9600); // Verify it's trimmed and not just MTU size

    rte_pktmbuf_free(mbuf);
  }
}

void test_send_mbuf_prb_offset(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++) {
    struct rte_mbuf *mbuf = mbufs[i];
    g_packets_sent++;
    struct xran_ecpri_hdr *sent_ecpri = rte_pktmbuf_mtod((struct rte_mbuf *)mbuf, struct xran_ecpri_hdr *);
    struct radio_app_common_hdr *sent_app = (struct radio_app_common_hdr *)(sent_ecpri + 1);
    struct data_section_hdr *sent_sec = (struct data_section_hdr *)(sent_app + 1);
    sent_sec->fields.all_bits = rte_be_to_cpu_32(sent_sec->fields.all_bits);

    // We expect start_prbu = 10, num_prbu = 20
    assert(sent_sec->fields.start_prbu == 10);
    assert(sent_sec->fields.num_prbu == (uint8_t)XRAN_CONVERT_NUMPRBC(20));

    // Check data: should match the offset in txdataF
    uint32_t *iq =
        (uint32_t *)((uint8_t *)sent_sec + sizeof(struct data_section_hdr));
    assert(rte_be_to_cpu_32(iq[0]) == 0x10101010);

    rte_pktmbuf_free(mbuf);
  }
}

void test_send_mbuf_ul_bfp(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++) {
    struct rte_mbuf *mbuf = mbufs[i];
    g_packets_sent++;

    struct xran_ecpri_hdr *sent_ecpri = rte_pktmbuf_mtod(mbuf, struct xran_ecpri_hdr *);
    struct radio_app_common_hdr *sent_app = (struct radio_app_common_hdr *)(sent_ecpri + 1);
    struct data_section_hdr *sent_sec = (struct data_section_hdr *)(sent_app + 1);
    sent_sec->fields.all_bits = rte_be_to_cpu_32(sent_sec->fields.all_bits);

    struct data_section_compression_hdr *sent_comp = (struct data_section_compression_hdr *)(sent_sec + 1);
    assert(sent_comp->ud_comp_hdr.ud_comp_meth == FH_COMP_BFP);
    assert(sent_comp->ud_comp_hdr.ud_iq_width == (uint8_t)XRAN_CONVERT_IQWIDTH(g_ul_test_iq_width));

    int num_prb = sent_sec->fields.num_prbu == 0 ? 273 : (int)sent_sec->fields.num_prbu;
    size_t header_len = sizeof(struct xran_ecpri_hdr) + sizeof(struct radio_app_common_hdr)
                        + sizeof(struct data_section_hdr) + sizeof(struct data_section_compression_hdr);
    size_t payload_len = rte_pktmbuf_pkt_len(mbuf) - header_len;
    assert(payload_len == (size_t)FH_COMP_PRB_BYTES(g_ul_test_iq_width) * num_prb);

    const int8_t *payload = (const int8_t *)(sent_comp + 1);
    fh_decompress_prbs(FH_COMP_BFP, g_ul_test_iq_width, num_prb, payload, g_ul_recovered_iq);

    rte_pktmbuf_free(mbuf);
  }
}

void test_uplink_basic()
{
  printf("Testing basic uplink...\n");
  int mu = 1;
  int num_prb = 100;
  g_packets_sent = 0;
  void *ctx = init_packet_processor(mu,
                                    num_prb,
                                    200,
                                    400,
                                    100,
                                    300,
                                    2,
                                    2,
                                    0,
                                    0,
                                    5,
                                    test_alloc_mbuf,
                                    test_send_mbuf_no_frag,
                                    NULL,
                                    1500,
                                    0,
                                    FH_COMP_NONE,
                                    0);
  assert(ctx != NULL);

  // Pattern: 5 slots, 2 DL, 2 UL. Symbols 0-27 DL, 42-69 UL.
  uint64_t current_sym = 47; // 47 % 70 = 47 (UL)
  handle_absolute_symbol_tick(ctx, current_sym);

  uint64_t target_sym = current_sym + 5; // 52 % 70 = 52 (UL)
  int slots_per_subframe = 1 << mu;
  int num_symbols_per_frame = 10 * slots_per_subframe * 14;
  int frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  int subframeId = slot_in_frame / slots_per_subframe;
  int slotId = slot_in_frame % slots_per_subframe;
  int startSymbolId = target_sym % 14;

  // 1. Send Uplink C-plane for 20 PRBs
  struct rte_mbuf *c_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_ecpri_hdr));
  ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0);

  struct xran_cp_radioapp_section1_header *apphdr =
      (struct xran_cp_radioapp_section1_header *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1_header));
  memset(apphdr, 0, sizeof(*apphdr));
  apphdr->cmnhdr.field.dataDirection = XRAN_DIR_UL;
  apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;
  apphdr->cmnhdr.field.frameId = frameId;
  apphdr->cmnhdr.field.subframeId = subframeId;
  apphdr->cmnhdr.field.slotId = slotId;
  apphdr->cmnhdr.field.startSymbolId = startSymbolId;
  apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_1;
  apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);

  struct xran_cp_radioapp_section1 *sec =
      (struct xran_cp_radioapp_section1 *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1));
  memset(sec, 0, sizeof(*sec));
  sec->hdr.u.s1.numSymbol = 1;
  sec->hdr.u1.common.numPrbc = 20;
  sec->hdr.u1.common.startPrbc = 0;
  sec->hdr.u1.common.sectionId = 123;
  *((uint64_t *)sec) = rte_be_to_cpu_64(*((uint64_t *)sec));

  handle_cplane_packet(ctx, c_mbuf);

  // 2. Poll the UL job created by handle_cplane_packet and call write_ul_iq
  ul_job_t job;
  int poll_ret = poll_ul_job(ctx, &job);
  assert(poll_ret == 0);
  assert(job.num_prb == 20);
  assert(job.start_prb == 0);
  assert(job.antenna_id == 0);

  uint32_t iq_input[100 * 12];
  for (int i = 0; i < 100 * 12; i++) {
    iq_input[i] = 0x11223344;
  }

  write_ul_iq(ctx, iq_input, startSymbolId, &job);

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.dl_tdd_mismatch == 0);
  assert(stats.ul_tdd_mismatch == 0);
  assert(g_packets_sent == 1);

  cleanup_packet_processor(ctx);
  printf("Basic uplink passed!\n");
}

void test_uplink_fragmentation()
{
  printf("Testing uplink fragmentation...\n");
  int mu = 1;
  int num_prb = 100;
  g_packets_sent = 0;
  void *ctx = init_packet_processor(mu,
                                    num_prb,
                                    200,
                                    400,
                                    100,
                                    300,
                                    2,
                                    2,
                                    0,
                                    0,
                                    5,
                                    test_alloc_mbuf,
                                    test_send_mbuf_frag,
                                    NULL,
                                    1500,
                                    0,
                                    FH_COMP_NONE,
                                    0);
  assert(ctx != NULL);

  // Pattern: 5 slots, 2 DL, 2 UL. Symbols 0-27 DL, 42-69 UL.
  uint64_t current_sym = 1030; // 1030 % 70 = 50 (UL)
  handle_absolute_symbol_tick(ctx, current_sym);

  uint64_t target_sym = current_sym + 5; // 1035 % 70 = 55 (UL)
  int slots_per_subframe = 1 << mu;
  int num_symbols_per_frame = 10 * slots_per_subframe * 14;
  int frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  int subframeId = slot_in_frame / slots_per_subframe;
  int slotId = slot_in_frame % slots_per_subframe;
  int startSymbolId = target_sym % 14;

  // 1. Send Uplink C-plane for 40 PRBs (should split into 30 + 10)
  struct rte_mbuf *c_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_ecpri_hdr));
  ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0);

  struct xran_cp_radioapp_section1_header *apphdr =
      (struct xran_cp_radioapp_section1_header *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1_header));
  memset(apphdr, 0, sizeof(*apphdr));
  apphdr->cmnhdr.field.dataDirection = XRAN_DIR_UL;
  apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;
  apphdr->cmnhdr.field.frameId = frameId;
  apphdr->cmnhdr.field.subframeId = subframeId;
  apphdr->cmnhdr.field.slotId = slotId;
  apphdr->cmnhdr.field.startSymbolId = startSymbolId;
  apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_1;
  apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);

  struct xran_cp_radioapp_section1 *sec =
      (struct xran_cp_radioapp_section1 *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1));
  memset(sec, 0, sizeof(*sec));
  sec->hdr.u.s1.numSymbol = 1;
  sec->hdr.u1.common.numPrbc = 40;
  sec->hdr.u1.common.startPrbc = 0;
  sec->hdr.u1.common.sectionId = 123;
  *((uint64_t *)sec) = rte_be_to_cpu_64(*((uint64_t *)sec));

  handle_cplane_packet(ctx, c_mbuf);

  // 2. Poll the UL job created by handle_cplane_packet and call write_ul_iq
  ul_job_t job;
  int poll_ret = poll_ul_job(ctx, &job);
  assert(poll_ret == 0);
  assert(job.num_prb == 40);
  assert(job.start_prb == 0);
  assert(job.antenna_id == 0);

  uint32_t iq_input[100 * 12];
  for (int i = 0; i < 100 * 12; i++) {
    iq_input[i] = 0x11223344;
  }

  write_ul_iq(ctx, iq_input, startSymbolId, &job);

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.dl_tdd_mismatch == 0);
  assert(stats.ul_tdd_mismatch == 0);
  assert(g_packets_sent == 2);

  cleanup_packet_processor(ctx);
  printf("Uplink fragmentation passed!\n");
}

void test_uplink_large_mtu()
{
  printf("Testing uplink with large MTU (9600)...\n");
  int mu = 1;
  int num_prb = 273;
  g_packets_sent = 0;
  // Initialize with jumbo frame MTU
  void *ctx = init_packet_processor(mu,
                                    num_prb,
                                    200,
                                    400,
                                    100,
                                    300,
                                    2,
                                    2,
                                    0,
                                    0,
                                    5,
                                    test_alloc_mbuf,
                                    test_send_mbuf_large_mtu,
                                    NULL,
                                    9600,
                                    0,
                                    FH_COMP_NONE,
                                    0);
  assert(ctx != NULL);

  // Pattern: 5 slots, 2 DL, 2 UL. Symbols 0-27 DL, 42-69 UL.
  uint64_t current_sym = 1030; // 1030 % 70 = 50 (UL)
  handle_absolute_symbol_tick(ctx, current_sym);

  uint64_t target_sym = current_sym + 5; // 1035 % 70 = 55 (UL)
  int slots_per_subframe = 1 << mu;
  int num_symbols_per_frame = 10 * slots_per_subframe * 14;
  int frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  int subframeId = slot_in_frame / slots_per_subframe;
  int slotId = slot_in_frame % slots_per_subframe;
  int startSymbolId = target_sym % 14;

  // 1. Send Uplink C-plane for 100 PRBs
  struct rte_mbuf *c_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_ecpri_hdr));
  ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0);

  struct xran_cp_radioapp_section1_header *apphdr =
      (struct xran_cp_radioapp_section1_header *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1_header));
  memset(apphdr, 0, sizeof(*apphdr));
  apphdr->cmnhdr.field.dataDirection = XRAN_DIR_UL;
  apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;
  apphdr->cmnhdr.field.frameId = frameId;
  apphdr->cmnhdr.field.subframeId = subframeId;
  apphdr->cmnhdr.field.slotId = slotId;
  apphdr->cmnhdr.field.startSymbolId = startSymbolId;
  apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_1;
  apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);

  struct xran_cp_radioapp_section1 *sec =
      (struct xran_cp_radioapp_section1 *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1));
  memset(sec, 0, sizeof(*sec));
  sec->hdr.u.s1.numSymbol = 1;
  sec->hdr.u1.common.numPrbc = 100;
  sec->hdr.u1.common.startPrbc = 0;
  sec->hdr.u1.common.sectionId = 123;
  *((uint64_t *)sec) = rte_be_to_cpu_64(*((uint64_t *)sec));

  handle_cplane_packet(ctx, c_mbuf);

  // 2. Poll the UL job created by handle_cplane_packet and call write_ul_iq
  ul_job_t job;
  int poll_ret = poll_ul_job(ctx, &job);
  assert(poll_ret == 0);
  assert(job.num_prb == 100);
  assert(job.start_prb == 0);
  assert(job.antenna_id == 0);

  uint32_t iq_input[num_prb * 12];
  for (int i = 0; i < num_prb * 12; i++) {
    iq_input[i] = 0x55667788;
  }

  write_ul_iq(ctx, iq_input, startSymbolId, &job);

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.dl_tdd_mismatch == 0);
  assert(stats.ul_tdd_mismatch == 0);
  assert(g_packets_sent == 1);

  cleanup_packet_processor(ctx);
  printf("Uplink large MTU passed!\n");
}

void test_uplink_prb_offset()
{
  printf("Testing uplink with PRB offset...\n");
  int mu = 1;
  int num_prb = 100;
  g_packets_sent = 0;
  void *ctx = init_packet_processor(mu,
                                    num_prb,
                                    200,
                                    400,
                                    100,
                                    300,
                                    2,
                                    2,
                                    0,
                                    0,
                                    5,
                                    test_alloc_mbuf,
                                    test_send_mbuf_prb_offset,
                                    NULL,
                                    1500,
                                    0,
                                    FH_COMP_NONE,
                                    0);
  assert(ctx != NULL);

  // Pattern: 5 slots, 2 DL, 2 UL. Symbols 0-27 DL, 42-69 UL.
  uint64_t current_sym = 1030; // 1030 % 70 = 50 (UL)
  handle_absolute_symbol_tick(ctx, current_sym);

  uint64_t target_sym = current_sym + 5; // 1035 % 70 = 55 (UL)
  int slots_per_subframe = 1 << mu;
  int num_symbols_per_frame = 10 * slots_per_subframe * 14;
  int frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  int subframeId = slot_in_frame / slots_per_subframe;
  int slotId = slot_in_frame % slots_per_subframe;
  int startSymbolId = target_sym % 14;

  // 1. Send Uplink C-plane for 20 PRBs starting at PRB 10
  struct rte_mbuf *c_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_ecpri_hdr));
  ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0);

  struct xran_cp_radioapp_section1_header *apphdr =
      (struct xran_cp_radioapp_section1_header *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1_header));
  memset(apphdr, 0, sizeof(*apphdr));
  apphdr->cmnhdr.field.dataDirection = XRAN_DIR_UL;
  apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;
  apphdr->cmnhdr.field.frameId = frameId;
  apphdr->cmnhdr.field.subframeId = subframeId;
  apphdr->cmnhdr.field.slotId = slotId;
  apphdr->cmnhdr.field.startSymbolId = startSymbolId;
  apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_1;
  apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);

  struct xran_cp_radioapp_section1 *sec =
      (struct xran_cp_radioapp_section1 *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1));
  memset(sec, 0, sizeof(*sec));
  sec->hdr.u.s1.numSymbol = 1;
  sec->hdr.u1.common.numPrbc = 20;
  sec->hdr.u1.common.startPrbc = 10; // PRB offset 10
  sec->hdr.u1.common.sectionId = 123;
  *((uint64_t *)sec) = rte_be_to_cpu_64(*((uint64_t *)sec));

  handle_cplane_packet(ctx, c_mbuf);

  // 2. Poll the UL job created by handle_cplane_packet and call write_ul_iq
  ul_job_t job;
  int poll_ret = poll_ul_job(ctx, &job);
  assert(poll_ret == 0);
  assert(job.num_prb == 20);
  assert(job.start_prb == 10);
  assert(job.antenna_id == 0);

  uint32_t iq_input[100 * 12];
  memset(iq_input, 0, sizeof(iq_input));
  iq_input[10 * 12] = 0x10101010; // Mark PRB 10

  write_ul_iq(ctx, iq_input, startSymbolId, &job);

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.dl_tdd_mismatch == 0);
  assert(stats.ul_tdd_mismatch == 0);
  assert(g_packets_sent == 1);

  cleanup_packet_processor(ctx);
  printf("Uplink PRB offset passed!\n");
}

void test_prach_generation()
{
  printf("Testing PRACH generation...\n");
  int mu = 1;
  int num_prb = 100;
  int prach_eaxc_offset = 4;
  g_packets_sent = 0;
  void *ctx = init_packet_processor(mu,
                                    num_prb,
                                    200,
                                    400,
                                    100,
                                    300,
                                    2,
                                    2,
                                    0,
                                    0,
                                    5,
                                    test_alloc_mbuf,
                                    test_send_mbuf_prach,
                                    NULL,
                                    1500,
                                    prach_eaxc_offset,
                                    FH_COMP_NONE,
                                    TEST_PRACH_KBAR);
  assert(ctx != NULL);

  // Pattern: 5 slots, 2 DL, 2 UL. Symbols 0-27 DL, 42-69 UL.
  uint64_t current_sym = 1030; // 1030 % 70 = 50 (UL)
  handle_absolute_symbol_tick(ctx, current_sym);

  uint64_t target_sym = current_sym + 5; // 1035 % 70 = 55 (UL)
  int slots_per_subframe = 1 << mu;
  int num_symbols_per_frame = 10 * slots_per_subframe * 14;
  int frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  int subframeId = slot_in_frame / slots_per_subframe;
  int slotId = slot_in_frame % slots_per_subframe;
  int startSymbolId = target_sym % 14;

  // 1. Send PRACH C-plane for 20 PRBs using section type 3
  struct rte_mbuf *c_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_ecpri_hdr));
  ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, prach_eaxc_offset);

  struct xran_cp_radioapp_section3_header *apphdr =
      (struct xran_cp_radioapp_section3_header *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section3_header));
  memset(apphdr, 0, sizeof(*apphdr));
  apphdr->cmnhdr.field.dataDirection = XRAN_DIR_UL;
  apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;
  apphdr->cmnhdr.field.frameId = frameId;
  apphdr->cmnhdr.field.subframeId = subframeId;
  apphdr->cmnhdr.field.slotId = slotId;
  apphdr->cmnhdr.field.startSymbolId = startSymbolId;
  apphdr->cmnhdr.field.filterIndex = XRAN_FILTERINDEX_PRACH_012;
  apphdr->cmnhdr.numOfSections = 1;
  apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_3;
  apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);

  struct xran_cp_radioapp_section3 *sec =
      (struct xran_cp_radioapp_section3 *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section3));
  memset(sec, 0, sizeof(*sec));
  sec->hdr.u.s3.numSymbol = 4;
  sec->hdr.u1.common.numPrbc = 20;
  sec->hdr.u1.common.startPrbc = 0;
  sec->hdr.u1.common.sectionId = 123;
  *((uint64_t *)sec) = rte_be_to_cpu_64(*((uint64_t *)sec));

  handle_cplane_packet(ctx, c_mbuf);

  // 2. Call write_prach_iq for each of the 4 symbols
  uint32_t *txdataF[1];
  uint32_t iq_input[100 * 12];
  memset(iq_input, 0, sizeof(iq_input));
  uint16_t *iq_raw = (uint16_t *)iq_input;
  for (int i = 0; i < 139 * 2; i++)
    iq_raw[i] = i + 1;
  txdataF[0] = iq_input;

  for (int i = 0; i < 4; i++) {
    write_prach_iq(ctx, txdataF, 1, frameId, slot_in_frame, startSymbolId + i);
  }

  // Expect 4 packets to be sent
  assert(g_packets_sent == 4);

  // Attempting to write a 5th symbol should not send a packet, as the job should be deactivated
  write_prach_iq(ctx, txdataF, 1, frameId, slot_in_frame, startSymbolId + 4);
  assert(g_packets_sent == 4);

  cleanup_packet_processor(ctx);
  printf("PRACH generation passed!\n");
}

void test_hyper_frame_calculation()
{
  printf("Testing hyper-frame calculation...\n");
  int mu = 1; // 30kHz
  int slots_per_subframe = 1 << mu;
  void *ctx = init_packet_processor(mu,
                                    273,
                                    200,
                                    400,
                                    100,
                                    300,
                                    2,
                                    2,
                                    0,
                                    0,
                                    5,
                                    test_alloc_mbuf,
                                    test_send_mbuf,
                                    NULL,
                                    1500,
                                    0,
                                    FH_COMP_NONE,
                                    0);
  assert(ctx != NULL);

  int num_symbols_per_frame = 10 * slots_per_subframe * 14; // 280

  // One hyper-frame has 1024 frames. So 1024 * 280 = 286720 symbols.
  // Target absolute symbol index: 3 * 286720 + 5 * 280 + 1 * 14 + 7 = 860160 + 1400 + 14 + 7 = 861581.
  uint64_t target_sym = 861581;
  uint64_t current_sym = target_sym - 7;
  handle_absolute_symbol_tick(ctx, current_sym);

  // 1. Send C-plane packet for target_sym
  struct rte_mbuf *c_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_ecpri_hdr));
  ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0);

  struct xran_cp_radioapp_section1_header *apphdr =
      (struct xran_cp_radioapp_section1_header *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1_header));
  memset(apphdr, 0, sizeof(*apphdr));
  apphdr->cmnhdr.field.dataDirection = XRAN_DIR_DL;
  apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;

  apphdr->cmnhdr.field.frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  apphdr->cmnhdr.field.subframeId = slot_in_frame / slots_per_subframe;
  apphdr->cmnhdr.field.slotId = slot_in_frame % slots_per_subframe;
  apphdr->cmnhdr.field.startSymbolId = target_sym % 14;
  apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_1;
  apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);

  struct xran_cp_radioapp_section1 *sec =
      (struct xran_cp_radioapp_section1 *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1));
  memset(sec, 0, sizeof(*sec));
  sec->hdr.u.s1.numSymbol = 1;
  sec->hdr.u1.common.numPrbc = 1;
  *((uint64_t *)sec) = rte_be_to_cpu_64(*((uint64_t *)sec));

  handle_cplane_packet(ctx, c_mbuf);

  // 2. Send U-plane packet for target_sym
  current_sym += 3;
  handle_absolute_symbol_tick(ctx, current_sym);

  struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
  u_ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0);

  struct radio_app_common_hdr *u_app =
      (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
  u_app->frame_id = (target_sym / num_symbols_per_frame) % 256;
  u_app->sf_slot_sym.subframe_id = slot_in_frame / slots_per_subframe;
  u_app->sf_slot_sym.slot_id = slot_in_frame % slots_per_subframe;
  u_app->sf_slot_sym.symb_id = target_sym % 14;
  u_app->sf_slot_sym.value = rte_cpu_to_be_16(u_app->sf_slot_sym.value);

  struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
  u_data->fields.num_prbu = 1;
  u_data->fields.start_prbu = 0;
  u_data->fields.sect_id = 0;
  u_data->fields.all_bits = rte_cpu_to_be_32(u_data->fields.all_bits);

  // IQ Data
  uint16_t *iq = (uint16_t *)rte_pktmbuf_append(u_mbuf, 1 * 12 * 4);
  assert(iq != NULL);
  iq[0] = 0xAAAA;

  handle_uplane_packet(ctx, u_mbuf);

  // 3. Advance to trigger window expiry and job completion
  current_sym += 10;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t *txdataF[1] = {0};
  uint32_t output_iq[273 * 12] = {0};
  txdataF[0] = output_iq;

  int frame, slot, symbol;
  uint64_t hyper_frame = 0xFFFFFFFF;
  dl_iq_stream_t dl_streams[MAX_DL_IQ_STREAMS_PER_SYMBOL];
  uint32_t dl_iq_arena[MAX_DL_IQ_STREAMS_PER_SYMBOL * 273 * 12];
  do {
    int n = read_dl_iq_streams(ctx, dl_streams, dl_iq_arena, MAX_DL_IQ_STREAMS_PER_SYMBOL, &hyper_frame, &frame, &slot, &symbol);
    test_assemble_streams(txdataF, 1, 273, dl_streams, n, NULL);
  } while (!(frame == (target_sym / num_symbols_per_frame) % 1024 && slot == slot_in_frame && symbol == target_sym % 14));

  assert(hyper_frame == 3);
  assert(frame == 5);
  assert(slot == 1);
  assert(symbol == 7);

  cleanup_packet_processor(ctx);
  printf("Hyper-frame calculation test passed!\n");
}

void test_dl_bfp_decompression(void)
{
  printf("Testing DL BFP decompression...\n");
  int mu = 1;
  int slots_per_subframe = 1 << mu;
  const int n_prb = 1;
  const int iq_bits = 9;

  int16_t src_iq[FH_VALS_PER_PRB];
  for (int i = 0; i < FH_VALS_PER_PRB; i += 2) {
    src_iq[i] = 100;
    src_iq[i + 1] = -200;
  }
  int8_t pre_compressed[FH_COMP_PRB_BYTES(iq_bits)];
  fh_compress_prbs(FH_COMP_BFP, iq_bits, n_prb, src_iq, pre_compressed);

  void *ctx = init_packet_processor(mu, 273, 200, 400, 100, 300,
                                    2, 2, 0, 0, 5,
                                    test_alloc_mbuf, test_send_mbuf,
                                    NULL, 1500, 0,
                                    FH_COMP_BFP, 0);
  assert(ctx != NULL);

  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);
  uint64_t target_sym = current_sym + 7;

  int num_symbols_per_frame = 10 * slots_per_subframe * 14;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;

  // DL C-plane
  struct rte_mbuf *c_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_ecpri_hdr));
  ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0);
  struct xran_cp_radioapp_section1_header *apphdr =
      (struct xran_cp_radioapp_section1_header *)rte_pktmbuf_append(c_mbuf, sizeof(*apphdr));
  memset(apphdr, 0, sizeof(*apphdr));
  apphdr->cmnhdr.field.dataDirection = XRAN_DIR_DL;
  apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;
  apphdr->cmnhdr.field.frameId = (target_sym / num_symbols_per_frame) % 256;
  apphdr->cmnhdr.field.subframeId = slot_in_frame / slots_per_subframe;
  apphdr->cmnhdr.field.slotId = slot_in_frame % slots_per_subframe;
  apphdr->cmnhdr.field.startSymbolId = target_sym % 14;
  apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_1;
  apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);
  struct xran_cp_radioapp_section1 *sec =
      (struct xran_cp_radioapp_section1 *)rte_pktmbuf_append(c_mbuf, sizeof(*sec));
  memset(sec, 0, sizeof(*sec));
  sec->hdr.u.s1.numSymbol = 1;
  sec->hdr.u1.common.numPrbc = n_prb;
  *((uint64_t *)sec) = rte_be_to_cpu_64(*((uint64_t *)sec));
  handle_cplane_packet(ctx, c_mbuf);

  current_sym += 3;
  handle_absolute_symbol_tick(ctx, current_sym);

  // DL U-plane with BFP compression header + pre-compressed payload
  struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
  u_ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0);
  struct radio_app_common_hdr *u_app =
      (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
  u_app->frame_id = (target_sym / num_symbols_per_frame) % 256;
  u_app->sf_slot_sym.subframe_id = slot_in_frame / slots_per_subframe;
  u_app->sf_slot_sym.slot_id = slot_in_frame % slots_per_subframe;
  u_app->sf_slot_sym.symb_id = target_sym % 14;
  u_app->sf_slot_sym.value = rte_cpu_to_be_16(u_app->sf_slot_sym.value);
  struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
  u_data->fields.num_prbu = n_prb;
  u_data->fields.start_prbu = 0;
  u_data->fields.sect_id = 0;
  u_data->fields.all_bits = rte_cpu_to_be_32(u_data->fields.all_bits);
  struct data_section_compression_hdr *comp_hdr =
      (struct data_section_compression_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_compression_hdr));
  memset(comp_hdr, 0, sizeof(*comp_hdr));
  comp_hdr->ud_comp_hdr.ud_comp_meth = FH_COMP_BFP;
  comp_hdr->ud_comp_hdr.ud_iq_width = XRAN_CONVERT_IQWIDTH(iq_bits);
  int8_t *payload = (int8_t *)rte_pktmbuf_append(u_mbuf, FH_COMP_PRB_BYTES(iq_bits) * n_prb);
  assert(payload != NULL);
  memcpy(payload, pre_compressed, FH_COMP_PRB_BYTES(iq_bits) * n_prb);

  handle_uplane_packet(ctx, u_mbuf);

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.uplane_err_early == 0);
  assert(stats.uplane_err_late == 0);

  current_sym += 10;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t *txdataF[4] = {0};
  uint32_t output_iq[273 * 12] = {0};
  txdataF[0] = output_iq;

  int frame, slot, symbol;
  uint64_t hyper_frame;
  uint16_t beam_ids[1];
  dl_iq_stream_t dl_streams[MAX_DL_IQ_STREAMS_PER_SYMBOL];
  uint32_t dl_iq_arena[MAX_DL_IQ_STREAMS_PER_SYMBOL * 273 * 12];
  do {
    int n = read_dl_iq_streams(ctx, dl_streams, dl_iq_arena, MAX_DL_IQ_STREAMS_PER_SYMBOL, &hyper_frame, &frame, &slot, &symbol);
    test_assemble_streams(txdataF, 1, 273, dl_streams, n, beam_ids);
  } while (!(frame == (int)((target_sym / num_symbols_per_frame) % 1024) && slot == slot_in_frame && symbol == (int)(target_sym % 14)));

  // I=100, Q=-200 with exponent=0: exact round-trip through BFP
  int16_t *recovered = (int16_t *)output_iq;
  for (int i = 0; i < n_prb * FH_VALS_PER_PRB; i++)
    assert(recovered[i] == src_iq[i]);

  cleanup_packet_processor(ctx);
  printf("DL BFP decompression passed!\n");
}

void test_ul_bfp_compression(void)
{
  printf("Testing UL BFP compression...\n");
  int mu = 1;
  int slots_per_subframe = 1 << mu;
  const int iq_bits = 9;

  // I=100, Q=-200: exponent=0 means exact round-trip after compress/decompress
  for (int i = 0; i < g_ul_test_num_prb * FH_VALS_PER_PRB; i += 2) {
    g_ul_known_src[i] = 100;
    g_ul_known_src[i + 1] = -200;
  }
  memset(g_ul_recovered_iq, 0, sizeof(g_ul_recovered_iq));

  g_packets_sent = 0;
  void *ctx = init_packet_processor(mu, 100, 200, 400, 100, 300,
                                    2, 2, 0, 0, 5,
                                    test_alloc_mbuf, test_send_mbuf_ul_bfp,
                                    NULL, 1500, 0,
                                    FH_COMP_NONE,
                                    0);
  assert(ctx != NULL);

  uint64_t current_sym = 47; // 47 % 70 = 47 (UL)
  handle_absolute_symbol_tick(ctx, current_sym);

  uint64_t target_sym = current_sym + 5; // 52 % 70 = 52 (UL)
  int num_symbols_per_frame = 10 * slots_per_subframe * 14;
  int frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  int subframeId = slot_in_frame / slots_per_subframe;
  int slotId = slot_in_frame % slots_per_subframe;
  int startSymbolId = target_sym % 14;

  // UL C-plane with BFP compression fields
  struct rte_mbuf *c_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_ecpri_hdr));
  ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0);
  struct xran_cp_radioapp_section1_header *apphdr =
      (struct xran_cp_radioapp_section1_header *)rte_pktmbuf_append(c_mbuf, sizeof(*apphdr));
  memset(apphdr, 0, sizeof(*apphdr));
  apphdr->cmnhdr.field.dataDirection = XRAN_DIR_UL;
  apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;
  apphdr->cmnhdr.field.frameId = frameId;
  apphdr->cmnhdr.field.subframeId = subframeId;
  apphdr->cmnhdr.field.slotId = slotId;
  apphdr->cmnhdr.field.startSymbolId = startSymbolId;
  apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_1;
  apphdr->udComp.udCompMeth = FH_COMP_BFP;
  apphdr->udComp.udIqWidth = XRAN_CONVERT_IQWIDTH(iq_bits);
  apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);
  struct xran_cp_radioapp_section1 *sec =
      (struct xran_cp_radioapp_section1 *)rte_pktmbuf_append(c_mbuf, sizeof(*sec));
  memset(sec, 0, sizeof(*sec));
  sec->hdr.u.s1.numSymbol = 1;
  sec->hdr.u1.common.numPrbc = g_ul_test_num_prb;
  sec->hdr.u1.common.startPrbc = 0;
  sec->hdr.u1.common.sectionId = 99;
  *((uint64_t *)sec) = rte_be_to_cpu_64(*((uint64_t *)sec));
  handle_cplane_packet(ctx, c_mbuf);

  // Poll UL job and verify compression params propagated from C-plane
  ul_job_t job;
  int poll_ret = poll_ul_job(ctx, &job);
  assert(poll_ret == 0);
  assert(job.num_prb == g_ul_test_num_prb);
  assert(job.response_payload.comp_method == FH_COMP_BFP);
  assert(job.response_payload.iq_width == XRAN_CONVERT_IQWIDTH(iq_bits));

  // Build iq_input in OAI format: each uint32 = I(low16) | Q(high16)
  uint32_t iq_input[100 * 12];
  memset(iq_input, 0, sizeof(iq_input));
  for (int i = 0; i < g_ul_test_num_prb * 12; i++) {
    uint16_t I = (uint16_t)g_ul_known_src[i * 2];
    uint16_t Q = (uint16_t)g_ul_known_src[i * 2 + 1];
    iq_input[i] = (uint32_t)I | ((uint32_t)Q << 16);
  }

  write_ul_iq(ctx, iq_input, startSymbolId, &job);

  assert(g_packets_sent == 1);

  // Verify: decompressed IQ in stub matches original (exponent=0 -> exact)
  for (int i = 0; i < g_ul_test_num_prb * FH_VALS_PER_PRB; i++)
    assert(g_ul_recovered_iq[i] == g_ul_known_src[i]);

  cleanup_packet_processor(ctx);
  printf("UL BFP compression passed!\n");
}

void test_large_delay_profile()
{
  printf("Testing large delay profile (up to 10 slots lookahead)...\n");
  int mu = 1; // 30kHz, slot duration = 500 uS, symbol duration = 35.71 uS
  // 10 slots = 140 symbols = 5000 uS.
  // We configure T2a_cp_max = 5000 uS (140 symbols) and T2a_up_max = 5000 uS (140 symbols)
  // Let's set T2a_cp_min = 200 uS, T2a_cp_max = 5000 uS, T2a_up_min = 100 uS, T2a_up_max = 5000 uS.
  void *ctx = init_packet_processor(mu, 273, 200, 5000, 100, 5000, 2, 2, 0, 0, 5, test_alloc_mbuf, test_send_mbuf, NULL, 1500, 0, FH_COMP_NONE, 0);
  assert(ctx != NULL);

  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);

  // Target symbol is 135 symbols in the future (within the 10 slots / 140 symbols limit)
  uint64_t target_sym = current_sym + 135;

  // 1. Send C-plane packet for target_sym
  struct rte_mbuf *c_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_ecpri_hdr));
  ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0);

  struct xran_cp_radioapp_section1_header *apphdr =
      (struct xran_cp_radioapp_section1_header *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1_header));
  memset(apphdr, 0, sizeof(*apphdr));
  apphdr->cmnhdr.field.dataDirection = XRAN_DIR_DL;
  apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;

  int slots_per_subframe = 1 << mu;
  int num_symbols_per_frame = 10 * slots_per_subframe * 14;
  apphdr->cmnhdr.field.frameId = (target_sym / num_symbols_per_frame) % 256;
  int slot_in_frame = (target_sym % num_symbols_per_frame) / 14;
  apphdr->cmnhdr.field.subframeId = slot_in_frame / slots_per_subframe;
  apphdr->cmnhdr.field.slotId = slot_in_frame % slots_per_subframe;
  apphdr->cmnhdr.field.startSymbolId = target_sym % 14;
  apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_1;
  apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);

  struct xran_cp_radioapp_section1 *sec =
      (struct xran_cp_radioapp_section1 *)rte_pktmbuf_append(c_mbuf, sizeof(struct xran_cp_radioapp_section1));
  memset(sec, 0, sizeof(*sec));
  sec->hdr.u.s1.numSymbol = 1;
  sec->hdr.u1.common.numPrbc = 1;
  *((uint64_t *)sec) = rte_be_to_cpu_64(*((uint64_t *)sec));

  handle_cplane_packet(ctx, c_mbuf);

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_early == 0);
  assert(stats.cplane_err_late == 0);
  assert(stats.total_cplane == 1);

  // 2. Send U-plane packet for target_sym
  struct rte_mbuf *u_mbuf = rte_pktmbuf_alloc(mp);
  struct xran_ecpri_hdr *u_ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct xran_ecpri_hdr));
  u_ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, 0);

  struct radio_app_common_hdr *u_app =
      (struct radio_app_common_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct radio_app_common_hdr));
  u_app->frame_id = (target_sym / num_symbols_per_frame) % 256;
  u_app->sf_slot_sym.subframe_id = slot_in_frame / slots_per_subframe;
  u_app->sf_slot_sym.slot_id = slot_in_frame % slots_per_subframe;
  u_app->sf_slot_sym.symb_id = target_sym % 14;
  u_app->sf_slot_sym.value = rte_cpu_to_be_16(u_app->sf_slot_sym.value);

  struct data_section_hdr *u_data = (struct data_section_hdr *)rte_pktmbuf_append(u_mbuf, sizeof(struct data_section_hdr));
  u_data->fields.num_prbu = 1;
  u_data->fields.start_prbu = 0;
  u_data->fields.sect_id = 0;
  u_data->fields.all_bits = rte_cpu_to_be_32(u_data->fields.all_bits);

  // IQ Data
  uint16_t *iq = (uint16_t *)rte_pktmbuf_append(u_mbuf, 1 * 12 * 4);
  assert(iq != NULL);
  iq[0] = 0x1111;

  handle_uplane_packet(ctx, u_mbuf);

  get_packet_processor_stats(ctx, &stats);
  assert(stats.uplane_err_early == 0);
  assert(stats.uplane_err_late == 0);

  // 3. Advance to trigger window expiry and job completion
  current_sym = target_sym;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t *txdataF[1] = {0};
  uint32_t output_iq[273 * 12] = {0};
  txdataF[0] = output_iq;

  int frame, slot, symbol;
  uint64_t hyper_frame = 0;
  uint16_t beam_ids[1];
  dl_iq_stream_t dl_streams[MAX_DL_IQ_STREAMS_PER_SYMBOL];
  uint32_t dl_iq_arena[MAX_DL_IQ_STREAMS_PER_SYMBOL * 273 * 12];
  do {
    int n = read_dl_iq_streams(ctx, dl_streams, dl_iq_arena, MAX_DL_IQ_STREAMS_PER_SYMBOL, &hyper_frame, &frame, &slot, &symbol);
    test_assemble_streams(txdataF, 1, 273, dl_streams, n, beam_ids);
  } while (!(frame == (target_sym / num_symbols_per_frame) % 1024 && slot == slot_in_frame && symbol == target_sym % 14));

  uint16_t *out_iq = (uint16_t *)output_iq;
  assert(out_iq[0] == 0x1111);

  cleanup_packet_processor(ctx);
  printf("Large delay profile test passed!\n");
}

int main(int argc, char **argv)
{
  setup_dpdk(argc, argv);
  test_uplink_prb_offset();
  usleep(10000); // Delay is needed to let dpdk cleanup internal structures between cleanup/init calls
  test_init_cleanup();
  usleep(10000);
  test_cplane_timing_errors();
  usleep(10000);
  test_cplane_uplane_match();

  test_uplane_prb_range_rejected();
  usleep(10000);
  test_uplane_short_payload_rejected();
  usleep(10000);

  test_dl_multi_section_same_beam();
  usleep(10000);
  test_frame_wrap_around();
  usleep(10000);
  test_cplane_14_symbols();
  usleep(10000);
  test_other_bw_4ant_prb_offset();
  usleep(10000);

  test_uplink_basic();
  usleep(10000);
  test_uplink_fragmentation();
  usleep(10000);
  test_uplink_large_mtu();
  usleep(10000);
  test_prach_generation();
  usleep(10000);
  test_hyper_frame_calculation();
  usleep(10000);
  test_dl_bfp_decompression();
  usleep(10000);
  test_ul_bfp_compression();
  usleep(10000);
  test_large_delay_profile();
  usleep(10000);

  printf("All tests passed!\n");
  return 0;
}
