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

// The wait lets DPDK release the internal structures of a test's cleanup_packet_processor() before the next init
#define RUN_TEST(call)                \
  do {                                \
    printf("Testing %s...\n", #call); \
    call;                             \
    printf("%s passed!\n", #call);    \
    usleep(10000);                    \
  } while (0)

#define ASSERT_NO_TDD_MISMATCH(stats) assert((stats).dl_tdd_mismatch == 0 && (stats).ul_tdd_mismatch == 0)

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

// init_packet_processor() arguments
typedef struct {
  int mu;
  int num_prb;
  uint32_t t2a_cp_min_us;
  uint32_t t2a_cp_max_us;
  uint32_t t2a_up_min_us;
  uint32_t t2a_up_max_us;
  int num_dl_slots;
  int num_ul_slots;
  int num_dl_symbols;
  int num_ul_symbols;
  int tdd_period_slots;
  void (*send)(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs);
  void *io_controller;
  size_t mtu;
  int prach_eaxc_offset;
  fh_comp_method_t dl_comp;
  int prach_kbar;
} test_ctx_cfg_t;

// Numerology 1, 273 PRBs, T2a 200-400 us (C-Plane) and 100-300 us (U-Plane). TDD pattern of 5 slots, 2 DL and 2 UL: of
// each 70 symbols, 0-27 are DL and 42-69 UL.
static const test_ctx_cfg_t test_ctx_defaults = {.mu = 1,
                                                 .num_prb = 273,
                                                 .t2a_cp_min_us = 200,
                                                 .t2a_cp_max_us = 400,
                                                 .t2a_up_min_us = 100,
                                                 .t2a_up_max_us = 300,
                                                 .num_dl_slots = 2,
                                                 .num_ul_slots = 2,
                                                 .tdd_period_slots = 5,
                                                 .send = test_send_mbuf,
                                                 .mtu = 1500,
                                                 .dl_comp = FH_COMP_NONE};

static void *test_init_ctx(const test_ctx_cfg_t *c)
{
  void *ctx = init_packet_processor(c->mu,
                                    c->num_prb,
                                    c->t2a_cp_min_us,
                                    c->t2a_cp_max_us,
                                    c->t2a_up_min_us,
                                    c->t2a_up_max_us,
                                    c->num_dl_slots,
                                    c->num_ul_slots,
                                    c->num_dl_symbols,
                                    c->num_ul_symbols,
                                    c->tdd_period_slots,
                                    test_alloc_mbuf,
                                    c->send,
                                    c->io_controller,
                                    c->mtu,
                                    c->prach_eaxc_offset,
                                    c->dl_comp,
                                    c->prach_kbar);
  assert(ctx != NULL);
  return ctx;
}

// Frame (0-1023), slot and symbol of absolute symbol sym at numerology mu
typedef struct {
  int frame;
  int slot_in_frame;
  int subframe;
  int slot;
  int symbol;
} test_sym_t;

static test_sym_t test_sym(int mu, uint64_t sym)
{
  const int num_symbols_per_frame = 10 * (1 << mu) * 14;
  const int slot_in_frame = (sym % num_symbols_per_frame) / 14;
  return (test_sym_t){.frame = (sym / num_symbols_per_frame) % 1024,
                      .slot_in_frame = slot_in_frame,
                      .subframe = slot_in_frame >> mu,
                      .slot = slot_in_frame % (1 << mu),
                      .symbol = sym % 14};
}

// A new C-Plane message for RU port ru_port at sym (numerology mu), with the eCPRI header and the application header of
// section_type. Returns the application header in *apphdr if apphdr isn't NULL, for type-specific fields such as udComp.
static struct rte_mbuf *
new_cplane_msg(int ru_port, int dir, int mu, uint64_t sym, int section_type, int num_sections, void **apphdr)
{
  struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mp);
  assert(mbuf != NULL);
  struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(mbuf, sizeof(struct xran_ecpri_hdr));
  ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, ru_port);
  const size_t hdr_len = section_type == XRAN_CP_SECTIONTYPE_3 ? sizeof(struct xran_cp_radioapp_section3_header)
                                                               : sizeof(struct xran_cp_radioapp_section1_header);
  struct xran_cp_radioapp_common_header *cmn = (struct xran_cp_radioapp_common_header *)rte_pktmbuf_append(mbuf, hdr_len);
  assert(cmn != NULL);
  memset(cmn, 0, hdr_len);
  const test_sym_t t = test_sym(mu, sym);
  cmn->field.dataDirection = dir;
  cmn->field.payloadVer = XRAN_PAYLOAD_VER;
  cmn->field.frameId = t.frame % 256;
  cmn->field.subframeId = t.subframe;
  cmn->field.slotId = t.slot;
  cmn->field.startSymbolId = t.symbol;
  if (section_type == XRAN_CP_SECTIONTYPE_3)
    cmn->field.filterIndex = XRAN_FILTERINDEX_PRACH_012;
  cmn->numOfSections = num_sections;
  cmn->sectionType = section_type;
  cmn->field.all_bits = rte_cpu_to_be_32(cmn->field.all_bits);
  if (apphdr)
    *apphdr = cmn;
  return mbuf;
}

static void append_ext(struct rte_mbuf *mbuf, const uint8_t *ext, size_t ext_len)
{
  if (ext_len == 0)
    return;
  uint8_t *dst = (uint8_t *)rte_pktmbuf_append(mbuf, ext_len);
  assert(dst != NULL);
  memcpy(dst, ext, ext_len);
}

// One section of a multi-section Type 1 C-Plane message. A non-empty ext sets ef and is appended after the section.
typedef struct {
  uint16_t section_id;
  uint16_t beam_id;
  int start_prbc;
  int num_prbc;
  const uint8_t *ext;
  size_t ext_len;
  int sym_inc;
  int num_symbol; // 0 means 1
  bool ef; // sets ef also without ext
} test_section1_t;

// A Type 1 C-Plane message for RU port ru_port at sym (numerology mu) carrying n sections, with numOfSections set to
// num_sections, which may differ from n to build a truncated message
static struct rte_mbuf *
new_cplane_sections(int ru_port, int dir, int mu, uint64_t sym, int num_sections, const test_section1_t *sec, int n)
{
  struct rte_mbuf *mbuf = new_cplane_msg(ru_port, dir, mu, sym, XRAN_CP_SECTIONTYPE_1, num_sections, NULL);
  for (int i = 0; i < n; i++) {
    struct xran_cp_radioapp_section1 *s =
        (struct xran_cp_radioapp_section1 *)rte_pktmbuf_append(mbuf, sizeof(struct xran_cp_radioapp_section1));
    assert(s != NULL);
    memset(s, 0, sizeof(*s));
    s->hdr.u.s1.numSymbol = sec[i].num_symbol > 0 ? sec[i].num_symbol : 1;
    s->hdr.u.s1.beamId = sec[i].beam_id;
    s->hdr.u.s1.ef = sec[i].ef || sec[i].ext_len > 0;
    s->hdr.u1.common.sectionId = sec[i].section_id;
    s->hdr.u1.common.symInc = sec[i].sym_inc;
    s->hdr.u1.common.startPrbc = sec[i].start_prbc;
    s->hdr.u1.common.numPrbc = sec[i].num_prbc;
    *((uint64_t *)s) = rte_be_to_cpu_64(*((uint64_t *)s));
    append_ext(mbuf, sec[i].ext, sec[i].ext_len);
  }
  return mbuf;
}

// Sends a Type 1 C-Plane message for eAxC 0, see new_cplane_sections()
static void send_cplane_sections(void *ctx, int dir, int mu, uint64_t sym, int num_sections, const test_section1_t *sec, int n)
{
  handle_cplane_packet(ctx, new_cplane_sections(0, dir, mu, sym, num_sections, sec, n));
}

// Sends a one-section Type 1 C-Plane message for eAxC 0
static void send_cplane_section(void *ctx, int dir, int mu, uint64_t sym, test_section1_t sec)
{
  send_cplane_sections(ctx, dir, mu, sym, 1, &sec, 1);
}

// One section of a Type 3 C-Plane message. A non-empty ext sets ef and is appended after the section.
typedef struct {
  uint16_t section_id;
  uint16_t beam_id;
  const uint8_t *ext;
  size_t ext_len;
  bool all_prbs; // numPrbc 0 (all carrier PRBs) instead of 20
  uint16_t start_prb;
} test_section3_t;

// A 4-symbol PRACH (Type 3) C-Plane message for RU port ru_port at symbol sym (numerology 1) carrying n sections, with
// numOfSections set to num_sections
static struct rte_mbuf *new_prach_cplane(int ru_port, uint64_t sym, int num_sections, const test_section3_t *sec, int n)
{
  struct rte_mbuf *mbuf = new_cplane_msg(ru_port, XRAN_DIR_UL, 1, sym, XRAN_CP_SECTIONTYPE_3, num_sections, NULL);
  for (int i = 0; i < n; i++) {
    struct xran_cp_radioapp_section3 *s =
        (struct xran_cp_radioapp_section3 *)rte_pktmbuf_append(mbuf, sizeof(struct xran_cp_radioapp_section3));
    assert(s != NULL);
    memset(s, 0, sizeof(*s));
    s->hdr.u.s3.numSymbol = 4;
    s->hdr.u.s3.beamId = sec[i].beam_id;
    s->hdr.u.s3.ef = sec[i].ext_len > 0;
    s->hdr.u1.common.sectionId = sec[i].section_id;
    s->hdr.u1.common.numPrbc = sec[i].all_prbs ? 0 : 20;
    s->hdr.u1.common.startPrbc = sec[i].start_prb;
    *((uint64_t *)s) = rte_be_to_cpu_64(*((uint64_t *)s));
    append_ext(mbuf, sec[i].ext, sec[i].ext_len);
  }
  return mbuf;
}

// Sends a PRACH C-Plane message for PRACH stream 0, see new_prach_cplane()
static void send_prach_cplane(void *ctx, uint64_t sym, int num_sections, const test_section3_t *sec, int n)
{
  handle_cplane_packet(ctx, new_prach_cplane(0, sym, num_sections, sec, n));
}

// A new uncompressed U-Plane packet for RU port ru_port at sym (numerology mu), with iq_len zeroed payload bytes after
// the data section header. Returns the payload in *iq if iq isn't NULL.
static struct rte_mbuf *
new_uplane_pkt(int ru_port, int mu, uint64_t sym, int section_id, int start_prbu, int num_prbu, size_t iq_len, void **iq)
{
  struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mp);
  assert(mbuf != NULL);
  struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(mbuf, sizeof(struct xran_ecpri_hdr));
  ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 0, 0, 0, ru_port);
  struct radio_app_common_hdr *app = (struct radio_app_common_hdr *)rte_pktmbuf_append(mbuf, sizeof(struct radio_app_common_hdr));
  memset(app, 0, sizeof(*app));
  const test_sym_t t = test_sym(mu, sym);
  app->frame_id = t.frame % 256;
  app->sf_slot_sym.subframe_id = t.subframe;
  app->sf_slot_sym.slot_id = t.slot;
  app->sf_slot_sym.symb_id = t.symbol;
  app->sf_slot_sym.value = rte_cpu_to_be_16(app->sf_slot_sym.value);
  struct data_section_hdr *data = (struct data_section_hdr *)rte_pktmbuf_append(mbuf, sizeof(struct data_section_hdr));
  memset(data, 0, sizeof(*data));
  data->fields.num_prbu = num_prbu;
  data->fields.start_prbu = start_prbu;
  data->fields.sect_id = section_id;
  data->fields.all_bits = rte_cpu_to_be_32(data->fields.all_bits);
  void *payload = rte_pktmbuf_append(mbuf, iq_len);
  assert(payload != NULL || iq_len == 0);
  memset(payload, 0, iq_len);
  if (iq)
    *iq = payload;
  return mbuf;
}

// Sends an uncompressed DL U-Plane packet for RU port ru_port at sym, every IQ value set to pattern
static void
send_dl_uplane_ru(void *ctx, int ru_port, int mu, uint64_t sym, int section_id, int start_prbu, int num_prbu, uint16_t pattern)
{
  uint16_t *iq;
  struct rte_mbuf *mbuf = new_uplane_pkt(ru_port, mu, sym, section_id, start_prbu, num_prbu, num_prbu * 12 * 4, (void **)&iq);
  for (int j = 0; j < num_prbu * 12 * 2; j++)
    iq[j] = rte_cpu_to_be_16(pattern);
  handle_uplane_packet(ctx, mbuf);
}

// Sends an uncompressed DL U-Plane packet for eAxC 0 at sym, every IQ value set to pattern
static void send_dl_uplane(void *ctx, int mu, uint64_t sym, int section_id, int start_prbu, int num_prbu, uint16_t pattern)
{
  send_dl_uplane_ru(ctx, 0, mu, sym, section_id, start_prbu, num_prbu, pattern);
}

// Sends a UL C-Plane section (sectionId 123, eAxC 0) of num_prb PRBs from start_prb at target_sym (numerology 1), polls
// its job and writes iq_input for it
static void send_ul_and_write(void *ctx, uint64_t target_sym, int start_prb, int num_prb, uint32_t *iq_input)
{
  const test_section1_t sec = {.section_id = 123, .start_prbc = start_prb, .num_prbc = num_prb};
  send_cplane_section(ctx, XRAN_DIR_UL, 1, target_sym, sec);
  ul_job_t job;
  int poll_ret = poll_ul_job(ctx, &job);
  assert(poll_ret == 0);
  assert(job.num_prb == num_prb);
  assert(job.start_prb == start_prb);
  assert(job.antenna_id == 0);
  write_ul_iq(ctx, iq_input, test_sym(1, target_sym).symbol, &job);
}

static dl_iq_stream_t g_dl_streams[MAX_DL_FRAGMENTS_PER_SYMBOL];
static uint64_t g_dl_hyper_frame;

// Reads DL symbols until the one at sym (numerology mu) comes out, and returns its stream count. Its streams are left in
// g_dl_streams and, if txdataF isn't NULL, assembled into txdataF (nb_tx antennas of num_prb PRBs).
static int read_dl_until(void *ctx, int mu, uint64_t sym, uint32_t **txdataF, int nb_tx, int num_prb, uint16_t *beam_ids)
{
  static uint32_t dl_iq_arena[DL_IQ_ARENA_PRBS(273) * 12];
  const test_sym_t t = test_sym(mu, sym);
  int n, frame, slot, symbol;
  do {
    n = read_dl_iq_streams(ctx, g_dl_streams, dl_iq_arena, MAX_DL_FRAGMENTS_PER_SYMBOL, &g_dl_hyper_frame, &frame, &slot, &symbol);
    if (txdataF)
      test_assemble_streams(txdataF, nb_tx, num_prb, g_dl_streams, n, beam_ids);
  } while (!(frame == t.frame && slot == t.slot_in_frame && symbol == t.symbol));
  return n;
}

void test_init_cleanup()
{
  void *ctx = test_init_ctx(&test_ctx_defaults);
  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  ASSERT_NO_TDD_MISMATCH(stats);
  cleanup_packet_processor(ctx);
}

void test_cplane_timing_errors()
{
  void *ctx = test_init_ctx(&test_ctx_defaults);

  // Set current symbol
  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);

  // Send early packet: a section 50 symbols ahead
  send_cplane_section(ctx, XRAN_DIR_DL, 1, current_sym + 50, (test_section1_t){0});

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_early == 1);
  ASSERT_NO_TDD_MISMATCH(stats);

  cleanup_packet_processor(ctx);
}

void test_cplane_uplane_match()
{
  int mu = 1; // 30kHz
  // T2a_cp ranges: 200 to 400 us (approx 5 to 11 symbols)
  // T2a_up ranges: 100 to 300 us (approx 2 to 8 symbols)
  void *ctx = test_init_ctx(&test_ctx_defaults);

  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint64_t target_sym = current_sym + 7; // Good for C-plane (between 5 and 11)

  // 1. C-plane packet
  send_cplane_section(ctx, XRAN_DIR_DL, mu, target_sym, (test_section1_t){.beam_id = 37, .num_prbc = 1});

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_early == 0);
  assert(stats.cplane_err_late == 0);
  assert(stats.total_cplane == 1);
  ASSERT_NO_TDD_MISMATCH(stats);

  // Advance time so U-plane is in valid range (target_sym = current_sym + 4 -> 4 is between 2 and 8)
  current_sym += 3;
  handle_absolute_symbol_tick(ctx, current_sym);

  // 2. U-plane packet, 1 PRB
  uint16_t *iq;
  struct rte_mbuf *u_mbuf = new_uplane_pkt(0, mu, target_sym, 0, 0, 1, 1 * 12 * 4, (void **)&iq);
  iq[0] = 0xFF00;
  iq[1] = 0xEE11;
  iq[2] = 0xDD22;
  iq[3] = 0xCC33;
  handle_uplane_packet(ctx, u_mbuf);

  get_packet_processor_stats(ctx, &stats);
  assert(stats.uplane_err_early == 0);
  assert(stats.uplane_err_late == 0);
  ASSERT_NO_TDD_MISMATCH(stats);

  // 3. Advance to trigger window expiry and job completion
  current_sym += 10;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t output_iq[273 * 12] = {0};
  uint32_t *txdataF[1] = {output_iq};
  uint16_t beam_ids[1] = {0};
  read_dl_until(ctx, mu, target_sym, txdataF, 1, 273, beam_ids);

  assert(beam_ids[0] == 37);
  uint16_t *out_iq = (uint16_t *)output_iq;
  assert(out_iq[0] == 0x00FF);
  assert(out_iq[1] == 0x11EE);
  assert(out_iq[2] == 0x22DD);
  assert(out_iq[3] == 0x33CC);

  cleanup_packet_processor(ctx);
}

// start_prbu/num_prbu are wire-controlled. combine_dl_streams() later writes num_prb PRBs at
// start_prb into a txDataF buffer sized for ctx->num_prb PRBs - a packet claiming a range beyond
// ctx->num_prb must be rejected up front, not handed through to that write.
void test_uplane_prb_range_rejected()
{
  int mu = 1;
  void *ctx = test_init_ctx(&test_ctx_defaults);

  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);

  // 270 + 10 = 280 > ctx->num_prb (273)
  handle_uplane_packet(ctx, new_uplane_pkt(0, mu, 0, 0, 270, 10, 10 * 12 * 4, NULL));

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.uplane_err_prb_range == 1);
  assert(stats.uplane_missing_cplane == 0); // rejected before the C-Plane stream lookup even ran
  assert(stats.uplane_err_short_payload == 0);

  cleanup_packet_processor(ctx);
}

// xran_extract_iq_samples() returns the packet length still remaining after stripping headers -
// a truncated packet (fewer bytes than num_prbu/iqWidth/compMeth implies) must be rejected before
// its iq_data pointer is handed off to the deferred unpack_iq() in read_dl_iq_streams().
void test_uplane_short_payload_rejected()
{
  int mu = 1;
  void *ctx = test_init_ctx(&test_ctx_defaults);

  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);

  // Uncompressed 1 PRB needs 12 * 2 * sizeof(uint16_t) = 48 bytes. Only 8 of them present - truncated packet.
  handle_uplane_packet(ctx, new_uplane_pkt(0, mu, 0, 0, 0, 1, 8, NULL));

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.uplane_err_short_payload == 1);
  assert(stats.uplane_err_prb_range == 0);
  assert(stats.uplane_missing_cplane == 0);

  cleanup_packet_processor(ctx);
}

// Two C-Plane sections for one beam (disjoint PRB ranges) must merge into one stream. A repeated
// section_id is rejected as a duplicate. A third section with a *different* beam on the same eaxc
// (sub-band precoding) is accepted as a genuinely separate stream, not merged or rejected.
void test_dl_multi_section_same_beam()
{
  int mu = 1; // 30kHz
  void *ctx = test_init_ctx(&test_ctx_defaults);

  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint64_t target_sym = current_sym + 7;
  const uint16_t beam_id = 42;
  const uint16_t beam_id2 = beam_id + 1;

  // One C-Plane message for each section:
  // - Two sections for the same eaxc/beam, disjoint PRB ranges, distinct sectionId.
  // - Re-declaring sectionId 0 must be rejected as a duplicate, not merged as a third section.
  // - A third section for the same eaxc, new sectionId, a *different* beamId, and a disjoint PRB range - accepted as a
  //   genuinely separate stream (sub-band precoding), not rejected.
  const test_section1_t sections[4] = {{0, beam_id, 0, 10}, {1, beam_id, 10, 10}, {0, beam_id, 0, 10}, {2, beam_id2, 20, 10}};
  for (int i = 0; i < 4; i++)
    send_cplane_section(ctx, XRAN_DIR_DL, mu, target_sym, sections[i]);

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_dup == 1);
  assert(stats.cplane_err_dup_dl == 1);
  assert(stats.dl_stream_pool_exhausted == 0);
  assert(stats.dl_stream_sections_exhausted == 0);

  current_sym += 3;
  handle_absolute_symbol_tick(ctx, current_sym);

  // Three U-Plane packets: two for beam_id's merged sections, one for beam_id2's separate section.
  const int accepted[3] = {0, 1, 3};
  const uint16_t patterns[3] = {0x1111, 0x2222, 0x3333};
  for (int i = 0; i < 3; i++) {
    const test_section1_t *s = &sections[accepted[i]];
    send_dl_uplane(ctx, mu, target_sym, s->section_id, s->start_prbc, s->num_prbc, patterns[i]);
  }

  get_packet_processor_stats(ctx, &stats);
  assert(stats.uplane_err_early == 0);
  assert(stats.uplane_err_late == 0);
  assert(stats.uplane_missing_cplane == 0);

  current_sym += 10;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t output_iq[273 * 12] = {0};
  uint32_t *txdataF[1] = {output_iq};
  uint16_t beam_ids[1] = {0};
  read_dl_until(ctx, mu, target_sym, txdataF, 1, 273, beam_ids);

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
  assert(stats.dl_fragments_pool_exhausted == 0);

  cleanup_packet_processor(ctx);
}

void test_frame_wrap_around()
{
  int mu = 1; // 30kHz
  int slots_per_subframe = 1 << mu;
  void *ctx = test_init_ctx(&test_ctx_defaults);

  // Total symbols = 5 slots * 14 = 70.
  // DL symbols: 0 to 2*14-1 = 27.
  // target_sym = 71680. 71680 % 70 = 0 (DL symbol)
  uint64_t current_sym = 256 * 14 * 10 * slots_per_subframe - 7;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint64_t target_sym = current_sym + 7;

  // 1. C-plane packet
  send_cplane_section(ctx, XRAN_DIR_DL, mu, target_sym, (test_section1_t){.num_prbc = 1});

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_early == 0);
  assert(stats.cplane_err_late == 0);
  assert(stats.total_cplane == 1);
  ASSERT_NO_TDD_MISMATCH(stats);

  // Advance time so U-plane is in valid range
  current_sym += 3; // 279
  handle_absolute_symbol_tick(ctx, current_sym);

  // 2. U-plane packet: 1 PRB, no IQ data
  handle_uplane_packet(ctx, new_uplane_pkt(0, mu, target_sym, 0, 0, 1, 0, NULL));

  get_packet_processor_stats(ctx, &stats);
  assert(stats.uplane_err_early == 0);
  assert(stats.uplane_err_late == 0);
  ASSERT_NO_TDD_MISMATCH(stats);

  // 3. Advance to trigger window expiry and job completion
  current_sym += 10;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t output_iq[273 * 12] = {0};
  uint32_t *txdataF[1] = {output_iq};
  read_dl_until(ctx, mu, target_sym, txdataF, 1, 273, NULL);

  cleanup_packet_processor(ctx);
}

void test_cplane_14_symbols()
{
  int mu = 1; // 30kHz
  // T2a_cp_max is widened to 900uS: a single section spanning 14 symbols needs its LAST symbol to
  // still be within T2a_min_cp of "now" too (each symbol has its own individual C-Plane deadline,
  // not just the section's first symbol), so the window has to be at least ~14 symbols wide.
  test_ctx_cfg_t cfg = test_ctx_defaults;
  cfg.t2a_cp_max_us = 900;
  cfg.num_dl_slots = 5;
  cfg.num_ul_slots = 0;
  void *ctx = test_init_ctx(&cfg);

  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);

  // T2a_min_cp_sym_diff=5, T2a_max_cp_sym_diff=25. Symbol i of the section sits at diff+i, so with
  // diff=10 the first symbol (i=0) is at 10 (>=5) and the last (i=13) is at 23 (<=25) - both stay
  // comfortably inside the window.
  uint64_t target_sym = current_sym + 10;
  const test_section1_t sec14 = {.num_prbc = 1, .num_symbol = 14};

  // 1. A section whose last symbol (diff 20 + 13 = 33) is past T2a_max_cp_sym_diff is early: none of its symbols
  // is registered, so the next section with the same sectionId and overlapping symbols isn't a duplicate
  send_cplane_section(ctx, XRAN_DIR_DL, mu, current_sym + 20, sec14);
  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_early == 1);

  // 2. C-plane packet with numSymbol = 14
  send_cplane_section(ctx, XRAN_DIR_DL, mu, target_sym, sec14);

  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_early == 1);
  assert(stats.cplane_err_late == 0);
  assert(stats.cplane_err_dup_dl == 0);
  assert(stats.total_cplane == 2);
  ASSERT_NO_TDD_MISMATCH(stats);

  // 3. Advance time so we can send U-plane for ALL 14 symbols. target_sym - current_sym must land
  // within U-Plane's own [T2a_min_up_dl_sym_diff, T2a_max_up_dl_sym_diff] = [2, 8] window and stay
  // there as both advance together below.
  current_sym += 6;
  handle_absolute_symbol_tick(ctx, current_sym);

  for (int i = 0; i < 14; i++) {
    uint16_t *iq;
    struct rte_mbuf *u_mbuf = new_uplane_pkt(0, mu, target_sym + i, 0, 0, 1, 1 * 12 * 4, (void **)&iq);
    iq[0] = rte_cpu_to_be_16(i);
    handle_uplane_packet(ctx, u_mbuf);

    // Advance tick so next symbol's U-plane is timely
    current_sym++;
    handle_absolute_symbol_tick(ctx, current_sym);
  }

  get_packet_processor_stats(ctx, &stats);
  assert(stats.uplane_err_early == 0);
  assert(stats.uplane_err_late == 0);
  ASSERT_NO_TDD_MISMATCH(stats);

  // 4. Advance to trigger window expiry and job completion
  current_sym += 15;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t output_iq[273 * 12] = {0};
  uint32_t *txdataF[1] = {output_iq};
  for (int i = 0; i < 14; i++) {
    read_dl_until(ctx, mu, target_sym + i, txdataF, 1, 273, NULL);
    uint16_t *out_iq = (uint16_t *)output_iq;
    assert(out_iq[0] == i);
  }

  cleanup_packet_processor(ctx);
}

void test_other_bw_4ant_prb_offset()
{
  int mu = 1; // 30kHz
  int num_prb = 106; // Different bandwidth
  test_ctx_cfg_t cfg = test_ctx_defaults;
  cfg.num_prb = num_prb;
  cfg.num_dl_slots = 4;
  cfg.num_ul_slots = 4;
  void *ctx = test_init_ctx(&cfg);

  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint64_t target_sym = current_sym + 7;

  // O-RAN allows 1 C-plane packet per antenna: send 4 C-plane packets, 20 PRBs from PRB 20
  const test_section1_t sec = {.start_prbc = 20, .num_prbc = 20};
  for (int ant = 0; ant < 4; ant++)
    handle_cplane_packet(ctx, new_cplane_sections(ant, XRAN_DIR_DL, mu, target_sym, 1, &sec, 1));

  current_sym += 3;
  handle_absolute_symbol_tick(ctx, current_sym);

  // Send U-plane packets for all 4 antennas, matching the C-plane, with a specific pattern per antenna
  for (int ant = 0; ant < 4; ant++)
    send_dl_uplane_ru(ctx, ant, mu, target_sym, 0, 20, 20, ant + 1);

  current_sym += 10;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t out_iq0[106 * 12] = {0};
  uint32_t out_iq1[106 * 12] = {0};
  uint32_t out_iq2[106 * 12] = {0};
  uint32_t out_iq3[106 * 12] = {0};
  uint32_t *txdataF[4] = {out_iq0, out_iq1, out_iq2, out_iq3};
  read_dl_until(ctx, mu, target_sym, txdataF, 4, 106, NULL);

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
  ASSERT_NO_TDD_MISMATCH(stats);

  cleanup_packet_processor(ctx);
}

// A 273-PRB symbol on 4 antennas, each antenna's IQ split into 10 uncompressed U-Plane fragments
// (what MTU 1500 forces): 40 fragments must all be stored and read back, none dropped.
void test_dl_wideband_fragments_4ant()
{
  const int num_prb = 273;
  const int nb_ant = 4;
  const int frag_prb = 28; // 9 x 28 + 21 = 273
  test_ctx_cfg_t cfg = test_ctx_defaults;
  cfg.num_prb = num_prb;
  cfg.num_dl_slots = nb_ant;
  cfg.num_ul_slots = nb_ant;
  void *ctx = test_init_ctx(&cfg);

  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);
  uint64_t target_sym = current_sym + 7;

  const test_section1_t all_prbs = {.num_prbc = 0}; // 0 = all PRBs
  for (int ant = 0; ant < nb_ant; ant++)
    handle_cplane_packet(ctx, new_cplane_sections(ant, XRAN_DIR_DL, 1, target_sym, 1, &all_prbs, 1));

  current_sym += 3;
  handle_absolute_symbol_tick(ctx, current_sym);

  int num_frags = 0;
  for (int ant = 0; ant < nb_ant; ant++) {
    for (int start = 0; start < num_prb; start += frag_prb) {
      const int n = start + frag_prb <= num_prb ? frag_prb : num_prb - start;
      uint16_t *iq;
      struct rte_mbuf *u_mbuf = new_uplane_pkt(ant, 1, target_sym, 0, start, n, n * 12 * 4, (void **)&iq);
      for (int j = 0; j < n * 12 * 2; j++) {
        iq[j] = rte_cpu_to_be_16(ant * 1000 + start + j / 24); // unique per antenna and PRB
      }
      handle_uplane_packet(ctx, u_mbuf);
      num_frags++;
    }
  }
  assert(num_frags == 40);

  current_sym += 10;
  handle_absolute_symbol_tick(ctx, current_sym);

  static uint32_t out_iq[4][273 * 12];
  uint32_t *txdataF[4] = {out_iq[0], out_iq[1], out_iq[2], out_iq[3]};
  const int n_read = read_dl_until(ctx, 1, target_sym, txdataF, nb_ant, num_prb, NULL);
  assert(n_read == 40);

  for (int ant = 0; ant < nb_ant; ant++) {
    const uint16_t *o = (const uint16_t *)txdataF[ant];
    for (int prb = 0; prb < num_prb; prb++) {
      for (int k = 0; k < 24; k++) {
        assert(o[prb * 24 + k] == ant * 1000 + prb);
      }
    }
  }

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.dl_fragments_pool_exhausted == 0);
  cleanup_packet_processor(ctx);
}

int g_packets_sent = 0;

#define UPLANE_HDR_LEN (sizeof(struct xran_ecpri_hdr) + sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr))

// The data section header of a U-Plane packet the O-RU sent, converted to host order in place
static struct data_section_hdr *sent_section(struct rte_mbuf *mbuf)
{
  struct xran_ecpri_hdr *ecpri = rte_pktmbuf_mtod(mbuf, struct xran_ecpri_hdr *);
  struct data_section_hdr *sec = (struct data_section_hdr *)((struct radio_app_common_hdr *)(ecpri + 1) + 1);
  sec->fields.all_bits = rte_be_to_cpu_32(sec->fields.all_bits);
  return sec;
}

// A macro, not a function, so that a failure names the line of the check
#define ASSERT_SENT_PRBS(sec, start, num)                                 \
  do {                                                                    \
    assert((sec)->fields.num_prbu == (uint8_t)XRAN_CONVERT_NUMPRBC(num)); \
    assert((sec)->fields.start_prbu == (start));                          \
  } while (0)

void test_send_mbuf_no_frag(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++) {
    struct rte_mbuf *mbuf = mbufs[i];
    g_packets_sent++;
    struct data_section_hdr *sent_sec = sent_section(mbuf);
    // In this test, we expect exactly 20 PRBs in a single packet
    ASSERT_SENT_PRBS(sent_sec, 0, 20);
    assert(rte_pktmbuf_pkt_len(mbuf) == UPLANE_HDR_LEN + 20 * 12 * 4);
    rte_pktmbuf_free(mbuf);
  }
}

void test_send_mbuf_prach(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++) {
    struct rte_mbuf *mbuf = mbufs[i];
    g_packets_sent++;
    struct data_section_hdr *sent_sec = sent_section(mbuf);
    // In this test, we expect exactly 20 PRBs in the header
    ASSERT_SENT_PRBS(sent_sec, 0, 20);
    assert(rte_pktmbuf_pkt_len(mbuf) == UPLANE_HDR_LEN + 20 * 12 * 4);
    uint16_t *payload = (uint16_t *)(sent_sec + 1);
    for (int j = 0; j < TEST_PRACH_KBAR; j++)
      assert(payload[j] == 0);
    for (int j = 0; j < 139 * 2; j++)
      assert(rte_be_to_cpu_16(payload[TEST_PRACH_KBAR + j]) == j + 1);
    for (int j = TEST_PRACH_KBAR + 139 * 2; j < 20 * 12 * 2; j++)
      assert(payload[j] == 0);

    rte_pktmbuf_free(mbuf);
  }
}

void test_send_mbuf_frag(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++) {
    struct rte_mbuf *mbuf = mbufs[i];
    g_packets_sent++;
    struct data_section_hdr *sent_sec = sent_section(mbuf);
    if (g_packets_sent == 1) {
      // First fragment: 30 PRBs
      ASSERT_SENT_PRBS(sent_sec, 0, 30);
      assert(rte_pktmbuf_pkt_len(mbuf) == UPLANE_HDR_LEN + 30 * 12 * 4);
    } else if (g_packets_sent == 2) {
      // Second fragment: 10 PRBs
      ASSERT_SENT_PRBS(sent_sec, 30, 10);
      assert(rte_pktmbuf_pkt_len(mbuf) == UPLANE_HDR_LEN + 10 * 12 * 4);
    }
    rte_pktmbuf_free(mbuf);
  }
}

void test_send_mbuf_large_mtu(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++) {
    struct rte_mbuf *mbuf = mbufs[i];
    g_packets_sent++;
    struct data_section_hdr *sent_sec = sent_section(mbuf);
    // With MTU 9600, 100 PRBs should fit in a single packet
    assert(g_packets_sent == 1);
    ASSERT_SENT_PRBS(sent_sec, 0, 100);
    // size (with the 14-byte Ethernet header) should be trimmed to actual data, not full MTU
    size_t size = rte_pktmbuf_pkt_len(mbuf) + 14;
    assert(size == UPLANE_HDR_LEN + 100 * 12 * 4 + 14);
    assert(size < 9600);
    rte_pktmbuf_free(mbuf);
  }
}

void test_send_mbuf_prb_offset(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++) {
    struct rte_mbuf *mbuf = mbufs[i];
    g_packets_sent++;
    struct data_section_hdr *sent_sec = sent_section(mbuf);
    ASSERT_SENT_PRBS(sent_sec, 10, 20);
    // Check data: should match the offset in txdataF
    uint32_t *iq = (uint32_t *)(sent_sec + 1);
    assert(rte_be_to_cpu_32(iq[0]) == 0x10101010);
    rte_pktmbuf_free(mbuf);
  }
}

void test_send_mbuf_ul_bfp(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++) {
    struct rte_mbuf *mbuf = mbufs[i];
    g_packets_sent++;
    struct data_section_hdr *sent_sec = sent_section(mbuf);

    struct data_section_compression_hdr *sent_comp = (struct data_section_compression_hdr *)(sent_sec + 1);
    assert(sent_comp->ud_comp_hdr.ud_comp_meth == FH_COMP_BFP);
    assert(sent_comp->ud_comp_hdr.ud_iq_width == (uint8_t)XRAN_CONVERT_IQWIDTH(g_ul_test_iq_width));

    int num_prb = sent_sec->fields.num_prbu == 0 ? 273 : (int)sent_sec->fields.num_prbu;
    size_t payload_len = rte_pktmbuf_pkt_len(mbuf) - UPLANE_HDR_LEN - sizeof(struct data_section_compression_hdr);
    assert(payload_len == (size_t)FH_COMP_PRB_BYTES(g_ul_test_iq_width) * num_prb);

    const int8_t *payload = (const int8_t *)(sent_comp + 1);
    fh_decompress_prbs(FH_COMP_BFP, g_ul_test_iq_width, num_prb, payload, g_ul_recovered_iq);

    rte_pktmbuf_free(mbuf);
  }
}

void test_uplink_basic()
{
  g_packets_sent = 0;
  test_ctx_cfg_t cfg = test_ctx_defaults;
  cfg.num_prb = 100;
  cfg.send = test_send_mbuf_no_frag;
  void *ctx = test_init_ctx(&cfg);

  // Pattern: 5 slots, 2 DL, 2 UL. Symbols 0-27 DL, 42-69 UL.
  uint64_t current_sym = 47; // 47 % 70 = 47 (UL)
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t iq_input[100 * 12];
  for (int i = 0; i < 100 * 12; i++) {
    iq_input[i] = 0x11223344;
  }
  // 20 PRBs, 52 % 70 = 52 (UL)
  send_ul_and_write(ctx, current_sym + 5, 0, 20, iq_input);

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  ASSERT_NO_TDD_MISMATCH(stats);
  assert(g_packets_sent == 1);

  cleanup_packet_processor(ctx);
}

void test_uplink_fragmentation()
{
  g_packets_sent = 0;
  test_ctx_cfg_t cfg = test_ctx_defaults;
  cfg.num_prb = 100;
  cfg.send = test_send_mbuf_frag;
  void *ctx = test_init_ctx(&cfg);

  // Pattern: 5 slots, 2 DL, 2 UL. Symbols 0-27 DL, 42-69 UL.
  uint64_t current_sym = 1030; // 1030 % 70 = 50 (UL)
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t iq_input[100 * 12];
  for (int i = 0; i < 100 * 12; i++) {
    iq_input[i] = 0x11223344;
  }
  // 40 PRBs (should split into 30 + 10), 1035 % 70 = 55 (UL)
  send_ul_and_write(ctx, current_sym + 5, 0, 40, iq_input);

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  ASSERT_NO_TDD_MISMATCH(stats);
  assert(g_packets_sent == 2);

  cleanup_packet_processor(ctx);
}

void test_uplink_large_mtu()
{
  int num_prb = 273;
  g_packets_sent = 0;
  // Initialize with jumbo frame MTU
  test_ctx_cfg_t cfg = test_ctx_defaults;
  cfg.send = test_send_mbuf_large_mtu;
  cfg.mtu = 9600;
  void *ctx = test_init_ctx(&cfg);

  // Pattern: 5 slots, 2 DL, 2 UL. Symbols 0-27 DL, 42-69 UL.
  uint64_t current_sym = 1030; // 1030 % 70 = 50 (UL)
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t iq_input[num_prb * 12];
  for (int i = 0; i < num_prb * 12; i++) {
    iq_input[i] = 0x55667788;
  }
  // 100 PRBs, 1035 % 70 = 55 (UL)
  send_ul_and_write(ctx, current_sym + 5, 0, 100, iq_input);

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  ASSERT_NO_TDD_MISMATCH(stats);
  assert(g_packets_sent == 1);

  cleanup_packet_processor(ctx);
}

void test_uplink_prb_offset()
{
  g_packets_sent = 0;
  test_ctx_cfg_t cfg = test_ctx_defaults;
  cfg.num_prb = 100;
  cfg.send = test_send_mbuf_prb_offset;
  void *ctx = test_init_ctx(&cfg);

  // Pattern: 5 slots, 2 DL, 2 UL. Symbols 0-27 DL, 42-69 UL.
  uint64_t current_sym = 1030; // 1030 % 70 = 50 (UL)
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t iq_input[100 * 12];
  memset(iq_input, 0, sizeof(iq_input));
  iq_input[10 * 12] = 0x10101010; // Mark PRB 10
  // 20 PRBs starting at PRB 10, 1035 % 70 = 55 (UL)
  send_ul_and_write(ctx, current_sym + 5, 10, 20, iq_input);

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  ASSERT_NO_TDD_MISMATCH(stats);
  assert(g_packets_sent == 1);

  cleanup_packet_processor(ctx);
}

static void test_send_mbuf_sequence(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  uint8_t *expected = io_controller;
  for (uint32_t i = 0; i < num_mbufs; i++) {
    const struct xran_ecpri_hdr *hdr = rte_pktmbuf_mtod(mbufs[i], const struct xran_ecpri_hdr *);
    const uint16_t eaxc = rte_be_to_cpu_16(hdr->ecpri_xtc_id);
    assert(eaxc < 16);
    assert(hdr->ecpri_seq_id.bits.seq_id == expected[eaxc]);
    expected[eaxc]++;
    g_packets_sent++;
    rte_pktmbuf_free(mbufs[i]);
  }
}

void test_prach_generation(int prach_eaxc_offset, bool check_sequence)
{
  int mu = 1;
  int num_prb = 100;
  uint8_t expected_sequence[16] = {0};
  g_packets_sent = 0;
  test_ctx_cfg_t cfg = test_ctx_defaults;
  cfg.num_prb = num_prb;
  cfg.send = check_sequence ? test_send_mbuf_sequence : test_send_mbuf_prach;
  cfg.io_controller = expected_sequence;
  cfg.prach_eaxc_offset = prach_eaxc_offset;
  cfg.prach_kbar = TEST_PRACH_KBAR;
  void *ctx = test_init_ctx(&cfg);

  // Pattern: 5 slots, 2 DL, 2 UL. Symbols 0-27 DL, 42-69 UL.
  uint64_t current_sym = 1030; // 1030 % 70 = 50 (UL)
  handle_absolute_symbol_tick(ctx, current_sym);

  uint64_t target_sym = current_sym + 5; // 1035 % 70 = 55 (UL)
  const test_sym_t t = test_sym(mu, target_sym);
  const int frameId = t.frame % 256;
  const int slot_in_frame = t.slot_in_frame;
  const int startSymbolId = t.symbol;

  // 1. Send PRACH C-plane for 20 PRBs using section type 3
  const test_section3_t prach = {.section_id = 123, .beam_id = 5};
  handle_cplane_packet(ctx, new_prach_cplane(prach_eaxc_offset, target_sym, 1, &prach, 1));
  assert(get_prach_beam_id(ctx, slot_in_frame, 0, startSymbolId) == 5); // section type 3 beamId carried to the job
  assert(get_prach_beam_id(ctx, slot_in_frame, 1, startSymbolId) == -1); // no C-Plane for this stream

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

  if (check_sequence) {
    // Interleave both channel types, including an overlapping PUSCH antenna and sequence wrap.
    ul_job_t job = {.frame = frameId, .slot_in_frame = slot_in_frame, .symbol = startSymbolId, .num_symbols = 1, .num_prb = 1};
    for (int i = 0; i < 300; i++) {
      for (int antenna = 0; antenna < 2; antenna++) {
        job.antenna_id = antenna;
        write_ul_iq(ctx, iq_input, startSymbolId, &job);
      }
      write_prach_iq(ctx, txdataF, 1, frameId, slot_in_frame, startSymbolId);
    }
    assert(g_packets_sent == 4 + 300 * 3);
  }

  // A one-PRB section cannot hold the active sequence and must not emit a packet.
  struct rte_mbuf *short_mbuf = new_cplane_msg(prach_eaxc_offset, XRAN_DIR_UL, mu, target_sym - 1, XRAN_CP_SECTIONTYPE_3, 1, NULL);
  struct xran_cp_radioapp_section3 *sec = (struct xran_cp_radioapp_section3 *)rte_pktmbuf_append(short_mbuf, sizeof(*sec));
  assert(sec != NULL);
  memset(sec, 0, sizeof(*sec));
  sec->hdr.u.s3.numSymbol = 1;
  sec->hdr.u1.common.numPrbc = 1;
  *((uint64_t *)sec) = rte_cpu_to_be_64(*((uint64_t *)sec));
  handle_cplane_packet(ctx, short_mbuf);
  const int sent_before_short = g_packets_sent;
  unsigned available = rte_mempool_avail_count(mp);
  write_prach_iq(ctx, txdataF, 1, frameId, slot_in_frame, startSymbolId - 1);
  assert(g_packets_sent == sent_before_short);
  assert(rte_mempool_avail_count(mp) == available);

  cleanup_packet_processor(ctx);
}

void test_hyper_frame_calculation()
{
  int mu = 1; // 30kHz
  void *ctx = test_init_ctx(&test_ctx_defaults);

  // One hyper-frame has 1024 frames. So 1024 * 280 = 286720 symbols.
  // Target absolute symbol index: 3 * 286720 + 5 * 280 + 1 * 14 + 7 = 860160 + 1400 + 14 + 7 = 861581.
  uint64_t target_sym = 861581;
  uint64_t current_sym = target_sym - 7;
  handle_absolute_symbol_tick(ctx, current_sym);

  // 1. Send C-plane packet for target_sym
  send_cplane_section(ctx, XRAN_DIR_DL, mu, target_sym, (test_section1_t){.num_prbc = 1});

  // 2. Send U-plane packet for target_sym
  current_sym += 3;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint16_t *iq;
  struct rte_mbuf *u_mbuf = new_uplane_pkt(0, mu, target_sym, 0, 0, 1, 1 * 12 * 4, (void **)&iq);
  iq[0] = 0xAAAA;
  handle_uplane_packet(ctx, u_mbuf);

  // 3. Advance to trigger window expiry and job completion
  current_sym += 10;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t output_iq[273 * 12] = {0};
  uint32_t *txdataF[1] = {output_iq};
  read_dl_until(ctx, mu, target_sym, txdataF, 1, 273, NULL);

  // Frame 5, slot 1, symbol 7 of hyper-frame 3
  assert(g_dl_hyper_frame == 3);

  cleanup_packet_processor(ctx);
}

void test_dl_bfp_decompression(void)
{
  int mu = 1;
  const int n_prb = 1;
  const int iq_bits = 9;

  int16_t src_iq[FH_VALS_PER_PRB];
  for (int i = 0; i < FH_VALS_PER_PRB; i += 2) {
    src_iq[i] = 100;
    src_iq[i + 1] = -200;
  }
  int8_t pre_compressed[FH_COMP_PRB_BYTES(iq_bits)];
  fh_compress_prbs(FH_COMP_BFP, iq_bits, n_prb, src_iq, pre_compressed);

  test_ctx_cfg_t cfg = test_ctx_defaults;
  cfg.dl_comp = FH_COMP_BFP;
  void *ctx = test_init_ctx(&cfg);

  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);
  uint64_t target_sym = current_sym + 7;

  // DL C-plane
  send_cplane_section(ctx, XRAN_DIR_DL, mu, target_sym, (test_section1_t){.num_prbc = n_prb});

  current_sym += 3;
  handle_absolute_symbol_tick(ctx, current_sym);

  // DL U-plane with BFP compression header + pre-compressed payload
  struct rte_mbuf *u_mbuf = new_uplane_pkt(0, mu, target_sym, 0, 0, n_prb, 0, NULL);
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

  uint32_t output_iq[273 * 12] = {0};
  uint32_t *txdataF[1] = {output_iq};
  read_dl_until(ctx, mu, target_sym, txdataF, 1, 273, NULL);

  // I=100, Q=-200 with exponent=0: exact round-trip through BFP
  int16_t *recovered = (int16_t *)output_iq;
  for (int i = 0; i < n_prb * FH_VALS_PER_PRB; i++)
    assert(recovered[i] == src_iq[i]);

  cleanup_packet_processor(ctx);
}

void test_ul_bfp_compression(void)
{
  int mu = 1;
  const int iq_bits = 9;

  // I=100, Q=-200: exponent=0 means exact round-trip after compress/decompress
  for (int i = 0; i < g_ul_test_num_prb * FH_VALS_PER_PRB; i += 2) {
    g_ul_known_src[i] = 100;
    g_ul_known_src[i + 1] = -200;
  }
  memset(g_ul_recovered_iq, 0, sizeof(g_ul_recovered_iq));

  g_packets_sent = 0;
  test_ctx_cfg_t cfg = test_ctx_defaults;
  cfg.num_prb = 100;
  cfg.send = test_send_mbuf_ul_bfp;
  void *ctx = test_init_ctx(&cfg);

  uint64_t current_sym = 47; // 47 % 70 = 47 (UL)
  handle_absolute_symbol_tick(ctx, current_sym);

  uint64_t target_sym = current_sym + 5; // 52 % 70 = 52 (UL)

  // UL C-plane with BFP compression fields
  const test_section1_t sec = {.section_id = 99, .num_prbc = g_ul_test_num_prb};
  struct xran_cp_radioapp_section1_header *apphdr;
  struct rte_mbuf *c_mbuf = new_cplane_sections(0, XRAN_DIR_UL, mu, target_sym, 1, &sec, 1);
  apphdr = rte_pktmbuf_mtod_offset(c_mbuf, struct xran_cp_radioapp_section1_header *, sizeof(struct xran_ecpri_hdr));
  apphdr->udComp.udCompMeth = FH_COMP_BFP;
  apphdr->udComp.udIqWidth = XRAN_CONVERT_IQWIDTH(iq_bits);
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

  write_ul_iq(ctx, iq_input, test_sym(mu, target_sym).symbol, &job);

  assert(g_packets_sent == 1);

  // Verify: decompressed IQ in stub matches original (exponent=0 -> exact)
  for (int i = 0; i < g_ul_test_num_prb * FH_VALS_PER_PRB; i++)
    assert(g_ul_recovered_iq[i] == g_ul_known_src[i]);

  cleanup_packet_processor(ctx);
}

void test_large_delay_profile()
{
  int mu = 1; // 30kHz, slot duration = 500 uS, symbol duration = 35.71 uS
  // 10 slots = 140 symbols = 5000 uS.
  // We configure T2a_cp_max = 5000 uS (140 symbols) and T2a_up_max = 5000 uS (140 symbols)
  // Let's set T2a_cp_min = 200 uS, T2a_cp_max = 5000 uS, T2a_up_min = 100 uS, T2a_up_max = 5000 uS.
  test_ctx_cfg_t cfg = test_ctx_defaults;
  cfg.t2a_cp_max_us = 5000;
  cfg.t2a_up_max_us = 5000;
  void *ctx = test_init_ctx(&cfg);

  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);

  // Target symbol is 135 symbols in the future (within the 10 slots / 140 symbols limit)
  uint64_t target_sym = current_sym + 135;

  // 1. Send C-plane packet for target_sym
  send_cplane_section(ctx, XRAN_DIR_DL, mu, target_sym, (test_section1_t){.num_prbc = 1});

  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_early == 0);
  assert(stats.cplane_err_late == 0);
  assert(stats.total_cplane == 1);

  // 2. Send U-plane packet for target_sym
  uint16_t *iq;
  struct rte_mbuf *u_mbuf = new_uplane_pkt(0, mu, target_sym, 0, 0, 1, 1 * 12 * 4, (void **)&iq);
  iq[0] = 0x1111;
  handle_uplane_packet(ctx, u_mbuf);

  get_packet_processor_stats(ctx, &stats);
  assert(stats.uplane_err_early == 0);
  assert(stats.uplane_err_late == 0);

  // 3. Advance to trigger window expiry and job completion
  current_sym = target_sym;
  handle_absolute_symbol_tick(ctx, current_sym);

  uint32_t output_iq[273 * 12] = {0};
  uint32_t *txdataF[1] = {output_iq};
  read_dl_until(ctx, mu, target_sym, txdataF, 1, 273, NULL);

  uint16_t *out_iq = (uint16_t *)output_iq;
  assert(out_iq[0] == 0x1111);

  cleanup_packet_processor(ctx);
}

// Sends a DL section-1 C-plane packet for num_prbc PRBs (0 = all) with the given raw extension bytes appended
// after the section. It targets DL symbol 1006, inside the C-Plane window of the extension tests (numerology 1, now is
// symbol 1000, T2a 200-400 us = 5-11 symbols): an out-of-window message is dropped before its extensions are parsed.
static void send_cplane_with_ext(void *ctx, int num_prbc, const uint8_t *ext, size_t ext_len)
{
  const test_section1_t sec = {.num_prbc = num_prbc, .ext = ext, .ext_len = ext_len, .ef = true};
  send_cplane_section(ctx, XRAN_DIR_DL, 1, 1006, sec);
}

void test_cplane_section_extension_1(void)
{
  void *ctx = test_init_ctx(&test_ctx_defaults);
  handle_absolute_symbol_tick(ctx, 1000);
  set_num_bf_weights(ctx, 2);

  // ext1, BFP 8-bit, exponent 0, 2 weights: 3 hdr + 1 param + 4 IQ bytes = 2 words, no padding
  const uint8_t ext1[] = {XRAN_CP_SECTIONEXTCMD_1, 2, 0x81, 0x00, 4, (uint8_t)-4, 100, (uint8_t)-100};
  oru_packet_processor_stats_t stats;

  send_cplane_with_ext(ctx, 0, ext1, sizeof(ext1));
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext1_received == 1 && stats.cplane_err_sect_ext1 == 0);

  // Unsupported extension (ef=1, 1 word) chained in front of ext1: skipped, ext1 still parsed
  uint8_t chained[4 + sizeof(ext1)] = {0x80 | XRAN_CP_SECTIONEXTCMD_6, 1, 0, 0};
  memcpy(chained + 4, ext1, sizeof(ext1));
  send_cplane_with_ext(ctx, 0, chained, sizeof(chained));
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext1_received == 2 && stats.cplane_err_sect_ext1 == 0);

  // ext1 carrying fewer weights than configured
  set_num_bf_weights(ctx, 4);
  send_cplane_with_ext(ctx, 0, ext1, sizeof(ext1));
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext1_received == 3 && stats.cplane_err_sect_ext1 == 1);

  // num_bf_weights = 0: ext1 counted but not decoded
  set_num_bf_weights(ctx, 0);
  send_cplane_with_ext(ctx, 0, ext1, sizeof(ext1));
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext1_received == 4 && stats.cplane_err_sect_ext1 == 1);

  // extLen = 0 and extLen past the end of the packet: counted as ext1 errors
  const uint8_t zero_len[] = {XRAN_CP_SECTIONEXTCMD_1, 0, 0x81, 0x00};
  send_cplane_with_ext(ctx, 0, zero_len, sizeof(zero_len));
  // extLen past the end of the packet
  const uint8_t overrun[] = {XRAN_CP_SECTIONEXTCMD_1, 3, 0x81, 0x00, 4, (uint8_t)-4, 100, (uint8_t)-100};
  send_cplane_with_ext(ctx, 0, overrun, sizeof(overrun));
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_sect_ext1 == 3);
  // Chain errors, not ext1 errors: ef set on the section but no extension present, and the last extension claiming
  // another one follows
  assert(stats.cplane_err_sect_ext_chain == 0);
  send_cplane_with_ext(ctx, 0, NULL, 0);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_sect_ext_chain == 1 && stats.cplane_err_sect_ext1 == 3);
  // Last extension claims another one follows
  uint8_t dangling[sizeof(ext1)];
  memcpy(dangling, ext1, sizeof(ext1));
  dangling[0] |= 0x80;
  send_cplane_with_ext(ctx, 0, dangling, sizeof(dangling));
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext1_received == 5 && stats.cplane_err_sect_ext1 == 3 && stats.cplane_err_sect_ext11 == 0);
  assert(stats.cplane_err_sect_ext_chain == 2);

  cleanup_packet_processor(ctx);
}

// No extension sent through the test mempool (10000-byte data room) can be larger
#define EXT11_TEST_BUF_SIZE 10000

// Builds an uncompressed 16-bit ext11 (or beamIds only if disable_bfws) for n_bundles bundles of n_weights weights,
// beamId 100 + b and weights (b, -w) into a buffer of EXT11_TEST_BUF_SIZE bytes. Returns the padded length.
static size_t build_ext11(uint8_t *buf, bool ef, bool disable_bfws, int num_bund_prb, int n_bundles, int n_weights)
{
  size_t len = 5;
  assert(6 + (size_t)n_bundles * (2 + 4 * (disable_bfws ? 0 : n_weights)) + 3 <= EXT11_TEST_BUF_SIZE);
  memset(buf, 0, EXT11_TEST_BUF_SIZE);
  buf[0] = (ef ? 0x80 : 0) | XRAN_CP_SECTIONEXTCMD_11;
  buf[3] = (disable_bfws ? 0x80 : 0) | 0x40; // RAD = 1
  buf[4] = num_bund_prb;
  if (!disable_bfws)
    buf[len++] = 0; // bfwIqWidth 16, bfwCompMeth NONE
  for (int b = 0; b < n_bundles; b++) {
    buf[len++] = 0;
    buf[len++] = 100 + b;
    for (int w = 0; w < n_weights && !disable_bfws; w++) {
      buf[len++] = 0;
      buf[len++] = b;
      buf[len++] = 0xFF;
      buf[len++] = (uint8_t)-w;
    }
  }
  len = (len + 3) & ~(size_t)3;
  buf[1] = (len / 4) >> 8;
  buf[2] = (len / 4) & 0xFF;
  return len;
}

void test_cplane_section_extension_11(void)
{
  void *ctx = test_init_ctx(&test_ctx_defaults);
  handle_absolute_symbol_tick(ctx, 1000);
  set_num_bf_weights(ctx, 2);
  oru_packet_processor_stats_t stats;
  uint8_t ext[EXT11_TEST_BUF_SIZE];

  // 3 PRBs, 2 per bundle: 2 bundles (the second one an orphan)
  size_t len = build_ext11(ext, false, false, 2, 2, 2);
  send_cplane_with_ext(ctx, 3, ext, len);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext11_received == 1 && stats.cplane_err_sect_ext11 == 0);

  // numPrbc 0 is all 273 PRBs: 2 bundles of up to 255 PRBs, beamIds only
  len = build_ext11(ext, false, true, 255, 2, 0);
  send_cplane_with_ext(ctx, 0, ext, len);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext11_received == 2 && stats.cplane_err_sect_ext11 == 0);

  // extLen above 255 words (110 bundles: 1106 bytes, 277 words) followed by an ext1: the walk must use the
  // 16-bit extLen to find the ext1
  len = build_ext11(ext, true, false, 1, 110, 2);
  assert(ext[1] == 1);
  const uint8_t ext1[] = {XRAN_CP_SECTIONEXTCMD_1, 2, 0x81, 0x00, 4, (uint8_t)-4, 100, (uint8_t)-100};
  memcpy(ext + len, ext1, sizeof(ext1));
  send_cplane_with_ext(ctx, 110, ext, len + sizeof(ext1));
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext11_received == 3 && stats.cplane_ext1_received == 1);
  assert(stats.cplane_err_sect_ext11 == 0 && stats.cplane_err_sect_ext1 == 0);

  // Bundles for 3 PRBs but the section has 5 (3 bundles expected)
  len = build_ext11(ext, false, false, 2, 2, 2);
  send_cplane_with_ext(ctx, 5, ext, len);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext11_received == 4 && stats.cplane_err_sect_ext11 == 1);

  // Fewer weights per bundle than configured
  set_num_bf_weights(ctx, 4);
  send_cplane_with_ext(ctx, 3, ext, len);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext11_received == 5 && stats.cplane_err_sect_ext11 == 2);

  // num_bf_weights = 0: ext11 counted but not decoded
  set_num_bf_weights(ctx, 0);
  send_cplane_with_ext(ctx, 5, ext, len);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext11_received == 6 && stats.cplane_err_sect_ext11 == 2);

  // 16-bit extLen past the end of the packet: an ext11 error, not counted as received
  ext[1] = 1;
  send_cplane_with_ext(ctx, 3, ext, len);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext11_received == 6 && stats.cplane_err_sect_ext11 == 3 && stats.cplane_err_sect_ext1 == 0);
  assert(stats.cplane_err_sect_ext_chain == 0);

  cleanup_packet_processor(ctx);
}

void test_cplane_section_extension_11_buffer_size(void)
{
  // 24-PRB carrier: at most 24 + 63 (bundleOffset) bundles
  test_ctx_cfg_t cfg = test_ctx_defaults;
  cfg.num_prb = 24;
  void *ctx = test_init_ctx(&cfg);
  handle_absolute_symbol_tick(ctx, 1000);
  set_num_bf_weights(ctx, 2);
  oru_packet_processor_stats_t stats;
  uint8_t ext[EXT11_TEST_BUF_SIZE];

  // numPrbc 0 (all 24 PRBs), bundleOffset 63, 1 PRB per bundle: 87 bundles, the maximum
  size_t len = build_ext11(ext, false, true, 1, 87, 0);
  ext[3] |= 63;
  send_cplane_with_ext(ctx, 0, ext, len);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext11_received == 1 && stats.cplane_err_sect_ext11 == 0);

  // A section of 25 PRBs on this carrier would need 88 bundles: rejected, not written past the buffers
  len = build_ext11(ext, false, true, 1, 88, 0);
  ext[3] |= 63;
  send_cplane_with_ext(ctx, 25, ext, len);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext11_received == 2 && stats.cplane_err_sect_ext11 == 1);

  // Changing the weight count keeps decoding (2 weights -> 4 weights, 12 bundles)
  set_num_bf_weights(ctx, 4);
  len = build_ext11(ext, false, false, 2, 12, 4);
  send_cplane_with_ext(ctx, 0, ext, len);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext11_received == 3 && stats.cplane_err_sect_ext11 == 1);

  // The largest ext11 of ORU_MAX_BF_WEIGHTS weights that fits a 9600-byte MTU: 37 bundles (6 + 37 * 258 bytes, plus
  // 24 bytes of eCPRI, application and section headers = 9576), here all 24 PRBs with bundleOffset 13
  set_num_bf_weights(ctx, ORU_MAX_BF_WEIGHTS);
  len = build_ext11(ext, false, false, 1, 37, ORU_MAX_BF_WEIGHTS);
  assert(sizeof(struct xran_ecpri_hdr) + sizeof(struct xran_cp_radioapp_section1_header) + sizeof(struct xran_cp_radioapp_section1)
             + len
         <= 9600);
  ext[3] |= 13;
  send_cplane_with_ext(ctx, 0, ext, len);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext11_received == 4 && stats.cplane_err_sect_ext11 == 1);

  cleanup_packet_processor(ctx);
}

void test_cplane_section_extension_other(void)
{
  void *ctx = test_init_ctx(&test_ctx_defaults);
  handle_absolute_symbol_tick(ctx, 1000);
  set_num_bf_weights(ctx, 2);
  oru_packet_processor_stats_t stats;

  // ext6 with extLen 0
  const uint8_t zero_len[] = {XRAN_CP_SECTIONEXTCMD_6, 0, 0, 0};
  send_cplane_with_ext(ctx, 0, zero_len, sizeof(zero_len));
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_sect_ext_other == 1);
  assert(stats.cplane_err_sect_ext1 == 0 && stats.cplane_err_sect_ext11 == 0 && stats.cplane_err_sect_ext_chain == 0);

  // ext6 whose 8-bit extLen runs past the packet
  const uint8_t overrun[] = {XRAN_CP_SECTIONEXTCMD_6, 2, 0, 0};
  send_cplane_with_ext(ctx, 0, overrun, sizeof(overrun));
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_sect_ext_other == 2);

  // ext19 (16-bit extLen, no constant defined) whose extLen runs past the packet
  const uint8_t overrun19[] = {19, 0, 2, 0};
  send_cplane_with_ext(ctx, 0, overrun19, sizeof(overrun19));
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_sect_ext_other == 3);

  // Malformed ext6 chained before a valid ext1: the walk stops, ext1 not reached
  const uint8_t ext1[] = {XRAN_CP_SECTIONEXTCMD_1, 2, 0x81, 0x00, 4, (uint8_t)-4, 100, (uint8_t)-100};
  uint8_t chained[4 + sizeof(ext1)] = {0x80 | XRAN_CP_SECTIONEXTCMD_6, 0, 0, 0};
  memcpy(chained + 4, ext1, sizeof(ext1));
  send_cplane_with_ext(ctx, 0, chained, sizeof(chained));
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_sect_ext_other == 4 && stats.cplane_ext1_received == 0);
  assert(stats.cplane_err_sect_ext1 == 0 && stats.cplane_err_sect_ext11 == 0 && stats.cplane_err_sect_ext_chain == 0);

  cleanup_packet_processor(ctx);
}

// Issue 759: every section of a multi-section DL message is registered, so U-Plane for each sectionId is accepted
void test_dl_multi_section_message(void)
{
  const int mu = 1;
  void *ctx = test_init_ctx(&test_ctx_defaults);
  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);
  const uint64_t target_sym = current_sym + 7;

  // Two sections on beam 10 and one on beam 11, as a DU packing several channels into one message
  const test_section1_t sections[3] = {{0, 10, 0, 10}, {1, 10, 10, 10}, {2, 11, 20, 10}};
  send_cplane_sections(ctx, XRAN_DIR_DL, mu, target_sym, 3, sections, 3);
  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.total_cplane == 1 && stats.cplane_received_dl == 1);
  assert(stats.cplane_err_hdr == 0 && stats.cplane_err_dup == 0);

  // The same message again: its sections are duplicates, counted once for the message
  send_cplane_sections(ctx, XRAN_DIR_DL, mu, target_sym, 3, sections, 3);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.total_cplane == 2 && stats.cplane_err_dup == 1 && stats.cplane_err_dup_dl == 1);

  current_sym += 3;
  handle_absolute_symbol_tick(ctx, current_sym);
  const uint16_t patterns[3] = {0x1111, 0x2222, 0x3333};
  for (int i = 0; i < 3; i++)
    send_dl_uplane(ctx, mu, target_sym, sections[i].section_id, sections[i].start_prbc, sections[i].num_prbc, patterns[i]);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.uplane_missing_cplane == 0);

  current_sym += 10;
  handle_absolute_symbol_tick(ctx, current_sym);
  uint32_t output_iq[273 * 12] = {0};
  uint32_t *txdataF[1] = {output_iq};
  const int n = read_dl_until(ctx, mu, target_sym, txdataF, 1, 273, NULL);

  // Each PRB run comes back under the beam its section declared
  assert(n == 3);
  for (int i = 0; i < n; i++) {
    const int k = g_dl_streams[i].start_prb / 10;
    assert(g_dl_streams[i].num_prb == 10 && g_dl_streams[i].beam_id == sections[k].beam_id);
  }
  const uint16_t *out_iq = (const uint16_t *)output_iq;
  for (int re = 0; re < 30 * 12 * 2; re++)
    assert(out_iq[re] == patterns[re / (10 * 12 * 2)]);

  cleanup_packet_processor(ctx);
}

static void *init_ul_test_ctx(void)
{
  test_ctx_cfg_t cfg = test_ctx_defaults;
  cfg.num_prb = 100;
  cfg.send = test_send_mbuf_no_frag;
  void *ctx = test_init_ctx(&cfg);
  // Pattern: 5 slots, 2 DL, 2 UL. Symbols 0-27 DL, 42-69 UL.
  handle_absolute_symbol_tick(ctx, 47);
  return ctx;
}

// Every section of a multi-section UL message gets its own job
void test_ul_multi_section_message(void)
{
  void *ctx = init_ul_test_ctx();
  const test_section1_t sections[2] = {{5, 3, 0, 20}, {6, 4, 20, 30}};
  send_cplane_sections(ctx, XRAN_DIR_UL, 1, 52, 2, sections, 2);

  for (int i = 0; i < 2; i++) {
    ul_job_t job;
    assert(poll_ul_job(ctx, &job) == 0);
    assert(job.response_payload.section_id == sections[i].section_id);
    assert(job.beam_id == sections[i].beam_id);
    assert(job.start_prb == sections[i].start_prbc && job.num_prb == sections[i].num_prbc);
  }
  ul_job_t job;
  assert(poll_ul_job(ctx, &job) != 0);
  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_received_ul == 1 && stats.cplane_err_hdr == 0);

  cleanup_packet_processor(ctx);
}

static int count_ul_jobs(void *ctx)
{
  int n = 0;
  ul_job_t job;
  while (poll_ul_job(ctx, &job) == 0)
    n++;
  return n;
}

// Timing is a message field: a late or early multi-section message counts once, like a single-section one, and
// registers none of its sections
void test_multi_section_timing_counted_once(void)
{
  void *ctx = init_ul_test_ctx();
  const test_section1_t sections[3] = {{0, 1, 0, 4}, {1, 2, 4, 4}, {2, 3, 8, 4}};
  oru_packet_processor_stats_t stats;

  // Now is symbol 47; the C-Plane window is 2 to 11 symbols ahead
  send_cplane_sections(ctx, XRAN_DIR_UL, 1, 48, 3, sections, 3);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_late == 1 && stats.cplane_err_early == 0);
  send_cplane_sections(ctx, XRAN_DIR_UL, 1, 60, 3, sections, 3);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_late == 1 && stats.cplane_err_early == 1);
  assert(count_ul_jobs(ctx) == 0);

  // In the window: every section is registered and nothing is counted
  send_cplane_sections(ctx, XRAN_DIR_UL, 1, 52, 3, sections, 3);
  assert(count_ul_jobs(ctx) == 3);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_late == 1 && stats.cplane_err_early == 1 && stats.ul_tdd_mismatch == 0);

  cleanup_packet_processor(ctx);
}

// Sections after one carrying extensions are found past its chain. A message shorter than numOfSections, or a section
// with a malformed chain, keeps the sections before the error and drops the rest.
void test_multi_section_extensions_and_truncation(void)
{
  void *ctx = init_ul_test_ctx();
  set_num_bf_weights(ctx, 2);
  oru_packet_processor_stats_t stats;

  // ext1, BFP 8-bit, exponent 0, 2 weights: 2 words
  const uint8_t ext1[] = {XRAN_CP_SECTIONEXTCMD_1, 2, 0x81, 0x00, 4, (uint8_t)-4, 100, (uint8_t)-100};
  // ext11 for a 4-PRB section, 2 bundles of 2 PRBs
  uint8_t ext11[EXT11_TEST_BUF_SIZE];
  const size_t ext11_len = build_ext11(ext11, false, false, 2, 2, 2);
  const test_section1_t with_ext[3] = {{0, 1, 0, 4, ext1, sizeof(ext1)}, {1, 2, 4, 4, ext11, ext11_len}, {2, 3, 8, 4}};
  send_cplane_sections(ctx, XRAN_DIR_UL, 1, 52, 3, with_ext, 3);
  assert(count_ul_jobs(ctx) == 3);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext1_received == 1 && stats.cplane_ext11_received == 1);
  assert(stats.cplane_err_sect_ext1 == 0 && stats.cplane_err_sect_ext11 == 0 && stats.cplane_err_hdr == 0);

  // numOfSections promises 3 sections, the message holds 2
  const test_section1_t plain[2] = {{0, 1, 0, 4}, {1, 2, 4, 4}};
  send_cplane_sections(ctx, XRAN_DIR_UL, 1, 52, 3, plain, 2);
  assert(count_ul_jobs(ctx) == 2);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_hdr == 1);

  // extLen = 0 on section 0: section 0 is kept, section 1 can't be located
  const uint8_t zero_len[] = {XRAN_CP_SECTIONEXTCMD_1, 0, 0x81, 0x00};
  const test_section1_t malformed[2] = {{0, 1, 0, 4, zero_len, sizeof(zero_len)}, {1, 2, 4, 4}};
  send_cplane_sections(ctx, XRAN_DIR_UL, 1, 52, 2, malformed, 2);
  ul_job_t job;
  assert(poll_ul_job(ctx, &job) == 0 && job.response_payload.section_id == 0);
  assert(poll_ul_job(ctx, &job) != 0);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_sect_ext1 == 1 && stats.cplane_err_hdr == 1);

  // numOfSections = 0: nothing is registered, counted as a header error
  send_cplane_sections(ctx, XRAN_DIR_UL, 1, 52, 0, plain, 2);
  assert(count_ul_jobs(ctx) == 0);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_hdr == 2 && stats.total_cplane == 4);

  cleanup_packet_processor(ctx);
}

// O-RAN CUS 7.5.3.1: a sectionId cited twice in one message (e.g. CSI-RS and PDSCH on complementary reMasks) is a
// legal repeat, not a duplicate. The first citation is used, and UL gets one job per sectionId. sectionId 4095 means no
// sectionId coupling, so its sections aren't repeats.
void test_section_id_repeated(void)
{
  const int mu = 1;
  void *ctx = test_init_ctx(&test_ctx_defaults);
  uint64_t current_sym = 1000;
  handle_absolute_symbol_tick(ctx, current_sym);
  const uint64_t target_sym = current_sym + 7;

  // sectionId 0 on beam 10, cited again on beam 11 over the same PRBs
  const test_section1_t dl[3] = {{0, 10, 0, 10}, {0, 11, 0, 10}, {1, 10, 10, 10}};
  send_cplane_sections(ctx, XRAN_DIR_DL, mu, target_sym, 3, dl, 3);
  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_section_repeated == 1 && stats.cplane_err_dup == 0);

  current_sym += 3;
  handle_absolute_symbol_tick(ctx, current_sym);
  send_dl_uplane(ctx, mu, target_sym, 0, 0, 10, 0x1111);
  send_dl_uplane(ctx, mu, target_sym, 1, 10, 10, 0x2222);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.uplane_missing_cplane == 0 && stats.uplane_err_dup == 0);

  current_sym += 10;
  handle_absolute_symbol_tick(ctx, current_sym);
  const int n = read_dl_until(ctx, mu, target_sym, NULL, 0, 0, NULL);
  // Both sections come back under the first citation's beam
  assert(n == 2);
  for (int i = 0; i < n; i++)
    assert(g_dl_streams[i].num_prb == 10 && g_dl_streams[i].beam_id == 10);
  cleanup_packet_processor(ctx);

  ctx = init_ul_test_ctx();
  const test_section1_t ul[3] = {{5, 3, 0, 20}, {5, 4, 0, 20}, {6, 3, 20, 20}};
  send_cplane_sections(ctx, XRAN_DIR_UL, 1, 52, 3, ul, 3);
  ul_job_t job;
  assert(poll_ul_job(ctx, &job) == 0 && job.response_payload.section_id == 5 && job.beam_id == 3);
  assert(poll_ul_job(ctx, &job) == 0 && job.response_payload.section_id == 6);
  assert(poll_ul_job(ctx, &job) != 0);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_section_repeated == 1);

  const test_section1_t uncoupled[2] = {{4095, 3, 0, 20}, {4095, 4, 20, 20}};
  send_cplane_sections(ctx, XRAN_DIR_UL, 1, 52, 2, uncoupled, 2);
  assert(count_ul_jobs(ctx) == 2);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_section_repeated == 1);
  cleanup_packet_processor(ctx);
}

static int g_prach_section_ids[16];

// Records the sectionId of each PRACH U-Plane packet sent
// O-RAN CUS 7.5.3.3: symInc = 1 moves the message's symbol number to the symbol after the previous section, for that
// section and the next ones. The OAI DU sends one one-symbol section per symbol this way (RunSlotPrbMapBySymbol). A
// section moved out of the DL/UL symbols is a TDD mismatch; the sections before it are kept.
void test_section_sym_inc(void)
{
  const int mu = 1;
  void *ctx = test_init_ctx(&test_ctx_defaults);
  // Symbols 1050-1077 are DL (pattern of 70 symbols, 0-27 DL); the C-Plane window is 5 to 11 symbols ahead
  uint64_t current_sym = 1050;
  handle_absolute_symbol_tick(ctx, current_sym);
  const uint64_t target_sym = current_sym + 6;
  // One section per symbol: 1056, 1057, 1058, 1059
  const test_section1_t per_symbol[4] = {{0, 10, 0, 10},
                                         {1, 10, 0, 10, NULL, 0, 1},
                                         {2, 10, 0, 10, NULL, 0, 1},
                                         {3, 10, 0, 10, NULL, 0, 1}};
  send_cplane_sections(ctx, XRAN_DIR_DL, mu, target_sym, 4, per_symbol, 4);
  // A 2-symbol section (1056-1057), then symInc: the next section is at 1058, not 1057
  const test_section1_t multi_symbol[2] = {{10, 11, 20, 10, NULL, 0, 0, 2}, {11, 11, 20, 10, NULL, 0, 1}};
  send_cplane_sections(ctx, XRAN_DIR_DL, mu, target_sym, 2, multi_symbol, 2);
  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_early == 0 && stats.cplane_err_dup == 0 && stats.dl_tdd_mismatch == 0);

  // The U-Plane window is 2 to 8 symbols ahead: every symbol finds its section
  current_sym += 3;
  handle_absolute_symbol_tick(ctx, current_sym);
  for (int i = 0; i < 4; i++)
    send_dl_uplane(ctx, mu, target_sym + i, i, 0, 10, 0x1111);
  send_dl_uplane(ctx, mu, target_sym, 10, 20, 10, 0x2222);
  send_dl_uplane(ctx, mu, target_sym + 1, 10, 20, 10, 0x2222);
  send_dl_uplane(ctx, mu, target_sym + 2, 11, 20, 10, 0x2222);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.uplane_err_late == 0 && stats.uplane_missing_cplane == 0 && stats.uplane_err_dup == 0);
  cleanup_packet_processor(ctx);

  // UL symbols are 42-69 and now is 47: sections at 52, 53, 54, 55 are symbols 10-13 of slot 3
  ctx = init_ul_test_ctx();
  const test_section1_t ul[4] = {{0, 3, 0, 10}, {1, 3, 0, 10, NULL, 0, 1}, {2, 3, 0, 10, NULL, 0, 1}, {3, 3, 0, 10, NULL, 0, 1}};
  send_cplane_sections(ctx, XRAN_DIR_UL, mu, 52, 4, ul, 4);
  ul_job_t job;
  for (int i = 0; i < 4; i++) {
    assert(poll_ul_job(ctx, &job) == 0);
    assert(job.response_payload.section_id == i && job.slot_in_frame == 3 && job.symbol == 10 + i && job.num_symbols == 1);
  }
  assert(poll_ul_job(ctx, &job) != 0);
  // From 58, the 13th section is at 70, a DL symbol: it alone is dropped
  test_section1_t past_ul[13];
  for (int i = 0; i < 13; i++)
    past_ul[i] = (test_section1_t){i, 3, 0, 10, NULL, 0, i > 0};
  send_cplane_sections(ctx, XRAN_DIR_UL, mu, 58, 13, past_ul, 13);
  assert(count_ul_jobs(ctx) == 12);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.ul_tdd_mismatch == 1);
  cleanup_packet_processor(ctx);
}

static void test_send_mbuf_prach_section_id(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++) {
    assert(g_packets_sent < (int)(sizeof(g_prach_section_ids) / sizeof(g_prach_section_ids[0])));
    g_prach_section_ids[g_packets_sent++] = sent_section(mbufs[i])->fields.sect_id;
    rte_pktmbuf_free(mbufs[i]);
  }
}

static void *init_prach_test_ctx(void)
{
  test_ctx_cfg_t cfg = test_ctx_defaults;
  cfg.num_prb = 100;
  cfg.send = test_send_mbuf_prach_section_id;
  cfg.prach_kbar = TEST_PRACH_KBAR;
  void *ctx = test_init_ctx(&cfg);
  // Pattern: 5 slots, 2 DL, 2 UL. Symbols 0-27 DL, 42-69 UL.
  handle_absolute_symbol_tick(ctx, 1030);
  g_packets_sent = 0;
  return ctx;
}

// PRACH sections of one stream starting at different symbols of a slot are TDM occasions, each served at its own
// symbols under its own beam. nr-oru extracts one FDM occasion, so a second section at the same start symbol is dropped
// and counted. Sections of an earlier frame that were never served are counted when the slot is reused.
void test_prach_occasions(void)
{
  void *ctx = init_prach_test_ctx();
  const uint64_t slot_sym = 1036; // symbol 0 of a slot
  const int num_symbols_per_frame = 10 * 2 * 14;
  const int frame = (slot_sym / num_symbols_per_frame) % 256;
  const int slot_in_frame = (slot_sym % num_symbols_per_frame) / 14;
  oru_packet_processor_stats_t stats;

  // Three 4-symbol TDM occasions: symbols 0-3, 4-7 and 8-11
  const test_section3_t occasions[3] = {{1, 5}, {2, 6}, {3, 7}};
  for (int i = 0; i < 3; i++)
    send_prach_cplane(ctx, slot_sym + 4 * i, 1, &occasions[i], 1);
  for (int s = 0; s < 12; s++)
    assert(get_prach_beam_id(ctx, slot_in_frame, 0, s) == occasions[s / 4].beam_id);
  assert(get_prach_beam_id(ctx, slot_in_frame, 0, 12) == -1);

  // A second section at symbol 0 is a second FDM occasion: dropped, the first one is kept
  const test_section3_t fdm = {4, 8};
  send_prach_cplane(ctx, slot_sym, 1, &fdm, 1);
  assert(get_prach_beam_id(ctx, slot_in_frame, 0, 0) == 5);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.prach_fdm_unsupported == 1 && stats.cplane_err_dup_prach == 0);

  // Same sectionId again: a duplicate
  send_prach_cplane(ctx, slot_sym, 1, &occasions[0], 1);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_dup_prach == 1 && stats.prach_fdm_unsupported == 1);

  // Each symbol is served from the occasion covering it
  uint32_t iq_input[100 * 12] = {0};
  uint32_t *txdataF[1] = {iq_input};
  for (int s = 0; s < 8; s++)
    write_prach_iq(ctx, txdataF, 1, frame, slot_in_frame, s);
  assert(g_packets_sent == 8);
  for (int i = 0; i < 8; i++)
    assert(g_prach_section_ids[i] == occasions[i / 4].section_id);

  // A fourth start symbol fills the pool, a fifth doesn't fit
  const test_section3_t extra[2] = {{5, 9}, {6, 10}};
  send_prach_cplane(ctx, slot_sym + 12, 1, &extra[0], 1);
  send_prach_cplane(ctx, slot_sym + 13, 1, &extra[1], 1);
  assert(get_prach_beam_id(ctx, slot_in_frame, 0, 12) == 9);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.prach_jobs_pool_exhausted == 1);

  // The same slot of the next frame releases the earlier jobs: sectionIds 3 and 5 were never served
  const test_section3_t next = {9, 11};
  handle_absolute_symbol_tick(ctx, slot_sym + num_symbols_per_frame - 6); // beam lookups resolve the symbol nearest to now
  send_prach_cplane(ctx, slot_sym + num_symbols_per_frame, 1, &next, 1);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.prach_sections_unserved == 2);
  assert(get_prach_beam_id(ctx, slot_in_frame, 0, 0) == 11);
  assert(get_prach_beam_id(ctx, slot_in_frame, 0, 4) == -1);

  cleanup_packet_processor(ctx);
}

// The sections of a multi-section PRACH message are found past each section's extension chain. They all start at the
// message's startSymbolId, so only the first is kept and the rest count as unsupported FDM occasions. Repeats,
// truncated messages and malformed chains are handled as for Type 1.
void test_prach_multi_section_message(void)
{
  void *ctx = init_prach_test_ctx();
  const int num_symbols_per_frame = 10 * 2 * 14;
  const int slot_in_frame = (1035 % num_symbols_per_frame) / 14;
  const int symbol = 1035 % 14;
  // Each case uses the same slot of a later frame, which releases the previous case's jobs
  uint64_t target_sym = 1035;
  oru_packet_processor_stats_t stats;

  // Two sections, the first with an ext1 and an unsupported extension chained after it
  const uint8_t ext[] =
      {0x80 | XRAN_CP_SECTIONEXTCMD_1, 2, 0x81, 0x00, 4, (uint8_t)-4, 100, (uint8_t)-100, XRAN_CP_SECTIONEXTCMD_6, 1, 0, 0};
  const test_section3_t two[2] = {{1, 5, ext, sizeof(ext)}, {2, 6}};
  send_prach_cplane(ctx, target_sym, 2, two, 2);
  assert(get_prach_beam_id(ctx, slot_in_frame, 0, symbol) == 5);
  uint32_t iq_input[100 * 12] = {0};
  uint32_t *txdataF[1] = {iq_input};
  const int frame = (target_sym / num_symbols_per_frame) % 256;
  write_prach_iq(ctx, txdataF, 1, frame, slot_in_frame, symbol);
  assert(g_packets_sent == 1 && g_prach_section_ids[0] == 1);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.prach_fdm_unsupported == 1);
  assert(stats.cplane_err_hdr == 0 && stats.cplane_err_sect_ext1 == 0 && stats.cplane_err_sect_ext_other == 0);

  // A sectionId cited twice: a repeat, not an FDM occasion or a duplicate
  target_sym += num_symbols_per_frame;
  handle_absolute_symbol_tick(ctx, target_sym - 5); // beam lookups resolve the symbol nearest to now
  const test_section3_t repeated[2] = {{3, 7}, {3, 8}};
  send_prach_cplane(ctx, target_sym, 2, repeated, 2);
  assert(get_prach_beam_id(ctx, slot_in_frame, 0, symbol) == 7);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_section_repeated == 1 && stats.cplane_err_dup_prach == 0 && stats.prach_fdm_unsupported == 1);
  assert(stats.prach_sections_unserved == 0);

  // numOfSections promises 3 sections, the message holds 2
  target_sym += num_symbols_per_frame;
  handle_absolute_symbol_tick(ctx, target_sym - 5); // beam lookups resolve the symbol nearest to now
  send_prach_cplane(ctx, target_sym, 3, two, 2);
  assert(get_prach_beam_id(ctx, slot_in_frame, 0, symbol) == 5);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_hdr == 1 && stats.prach_fdm_unsupported == 2);

  // extLen = 0 on section 0: section 0 is kept, section 1 can't be located
  target_sym += num_symbols_per_frame;
  handle_absolute_symbol_tick(ctx, target_sym - 5); // beam lookups resolve the symbol nearest to now
  const uint8_t zero_len[] = {XRAN_CP_SECTIONEXTCMD_1, 0, 0x81, 0x00};
  const test_section3_t malformed[2] = {{4, 9, zero_len, sizeof(zero_len)}, {5, 10}};
  send_prach_cplane(ctx, target_sym, 2, malformed, 2);
  assert(get_prach_beam_id(ctx, slot_in_frame, 0, symbol) == 9);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_sect_ext1 == 1 && stats.prach_fdm_unsupported == 2);

  // numOfSections = 0: nothing is registered, counted as a header error
  target_sym += num_symbols_per_frame;
  handle_absolute_symbol_tick(ctx, target_sym - 5); // beam lookups resolve the symbol nearest to now
  send_prach_cplane(ctx, target_sym, 0, two, 2);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_hdr == 2 && stats.cplane_received_prach == 5);

  // Section errors count once per message: two extra FDM sections count one FDM drop, and the same message resent
  // counts one duplicate
  target_sym += num_symbols_per_frame;
  handle_absolute_symbol_tick(ctx, target_sym - 5); // beam lookups resolve the symbol nearest to now
  const test_section3_t three[3] = {{10, 1}, {11, 2}, {12, 3}};
  oru_packet_processor_stats_t before;
  get_packet_processor_stats(ctx, &before);
  send_prach_cplane(ctx, target_sym, 3, three, 3);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.prach_fdm_unsupported == before.prach_fdm_unsupported + 1);
  send_prach_cplane(ctx, target_sym, 3, three, 3);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_dup_prach == before.cplane_err_dup_prach + 1 && stats.cplane_err_dup == before.cplane_err_dup + 1);
  assert(stats.prach_fdm_unsupported == before.prach_fdm_unsupported + 2);
  assert(get_prach_beam_id(ctx, slot_in_frame, 0, symbol) == 1);

  cleanup_packet_processor(ctx);
}

// ext1 and ext11 on the sections of a Section Type 3 (PRACH) message are decoded, with the same walk Type 1 uses and
// ext11 bundles counted in the section's PRBs. A malformed extension is counted and the section is still registered with
// its beamId.
void test_prach_section_extensions(void)
{
  void *ctx = init_prach_test_ctx();
  set_num_bf_weights(ctx, 2);
  const int num_symbols_per_frame = 10 * 2 * 14;
  const int slot_in_frame = (1035 % num_symbols_per_frame) / 14;
  const int symbol = 1035 % 14;
  // Each case uses the same slot of a later frame, which releases the previous case's job
  uint64_t target_sym = 1035;
  oru_packet_processor_stats_t stats;

  // ext1, BFP 8-bit, exponent 0, 2 weights
  const uint8_t ext1[] = {XRAN_CP_SECTIONEXTCMD_1, 2, 0x81, 0x00, 4, (uint8_t)-4, 100, (uint8_t)-100};
  const test_section3_t with_ext1 = {1, 5, ext1, sizeof(ext1)};
  send_prach_cplane(ctx, target_sym, 1, &with_ext1, 1);
  assert(get_prach_beam_id(ctx, slot_in_frame, 0, symbol) == 5);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext1_received == 1 && stats.cplane_err_sect_ext1 == 0);

  // ext1 carrying fewer weights than configured: an ext1 error, the section is kept
  set_num_bf_weights(ctx, 4);
  target_sym += num_symbols_per_frame;
  handle_absolute_symbol_tick(ctx, target_sym - 5); // beam lookups resolve the symbol nearest to now
  send_prach_cplane(ctx, target_sym, 1, &with_ext1, 1);
  assert(get_prach_beam_id(ctx, slot_in_frame, 0, symbol) == 5);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext1_received == 2 && stats.cplane_err_sect_ext1 == 1);
  set_num_bf_weights(ctx, 2);

  // ext11 with weights: 20 PRBs, 2 per bundle, 10 bundles
  uint8_t ext[EXT11_TEST_BUF_SIZE];
  size_t len = build_ext11(ext, false, false, 2, 10, 2);
  test_section3_t with_ext11 = {2, 6, ext, len};
  target_sym += num_symbols_per_frame;
  handle_absolute_symbol_tick(ctx, target_sym - 5); // beam lookups resolve the symbol nearest to now
  send_prach_cplane(ctx, target_sym, 1, &with_ext11, 1);
  assert(get_prach_beam_id(ctx, slot_in_frame, 0, symbol) == 6);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext11_received == 1 && stats.cplane_err_sect_ext11 == 0);

  // disableBFWs = 1: beamIds only, 4 PRBs per bundle, 5 bundles
  with_ext11.ext_len = build_ext11(ext, false, true, 4, 5, 0);
  target_sym += num_symbols_per_frame;
  handle_absolute_symbol_tick(ctx, target_sym - 5); // beam lookups resolve the symbol nearest to now
  send_prach_cplane(ctx, target_sym, 1, &with_ext11, 1);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext11_received == 2 && stats.cplane_err_sect_ext11 == 0);

  // bundleOffset 1 adds a bundle: 11 bundles decode, 10 don't
  with_ext11.ext_len = build_ext11(ext, false, false, 2, 11, 2);
  ext[3] |= 1;
  target_sym += num_symbols_per_frame;
  handle_absolute_symbol_tick(ctx, target_sym - 5); // beam lookups resolve the symbol nearest to now
  send_prach_cplane(ctx, target_sym, 1, &with_ext11, 1);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext11_received == 3 && stats.cplane_err_sect_ext11 == 0);
  with_ext11.ext_len = build_ext11(ext, false, false, 2, 10, 2);
  ext[3] |= 1;
  target_sym += num_symbols_per_frame;
  handle_absolute_symbol_tick(ctx, target_sym - 5); // beam lookups resolve the symbol nearest to now
  send_prach_cplane(ctx, target_sym, 1, &with_ext11, 1);
  assert(get_prach_beam_id(ctx, slot_in_frame, 0, symbol) == 6);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext11_received == 4 && stats.cplane_err_sect_ext11 == 1);

  // numPrbc 0 is all 100 carrier PRBs: 10 bundles of 10 PRBs
  with_ext11.ext_len = build_ext11(ext, false, true, 10, 10, 0);
  with_ext11.all_prbs = true;
  target_sym += num_symbols_per_frame;
  handle_absolute_symbol_tick(ctx, target_sym - 5); // beam lookups resolve the symbol nearest to now
  send_prach_cplane(ctx, target_sym, 1, &with_ext11, 1);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext11_received == 5 && stats.cplane_err_sect_ext11 == 1);
  with_ext11.all_prbs = false;

  // 16-bit extLen past the end of the packet: an ext11 error, not counted as received, the section is kept
  with_ext11.ext_len = build_ext11(ext, false, false, 2, 10, 2);
  ext[1] = 1;
  target_sym += num_symbols_per_frame;
  handle_absolute_symbol_tick(ctx, target_sym - 5); // beam lookups resolve the symbol nearest to now
  send_prach_cplane(ctx, target_sym, 1, &with_ext11, 1);
  assert(get_prach_beam_id(ctx, slot_in_frame, 0, symbol) == 6);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_ext11_received == 5 && stats.cplane_err_sect_ext11 == 2);
  assert(stats.cplane_err_sect_ext1 == 1 && stats.cplane_err_sect_ext_chain == 0 && stats.cplane_err_hdr == 0);

  cleanup_packet_processor(ctx);
}

// Sends a section-1 C-plane packet for target_sym (numerology 1) with the given raw extension bytes after the section
static void send_section1(void *ctx,
                          int dir,
                          uint64_t target_sym,
                          uint16_t section_id,
                          uint16_t beam_id,
                          int start_prb,
                          int num_prbc,
                          const uint8_t *ext,
                          size_t ext_len)
{
  send_cplane_section(ctx, dir, 1, target_sym, (test_section1_t){section_id, beam_id, start_prb, num_prbc, ext, ext_len});
}

// Sends a DL U-plane packet of zero IQ for target_sym (numerology 1)
// Uncompressed 16-bit ext1 carrying the 2 weights w
static size_t build_ext1_2w(uint8_t *buf, const c16_t w[2])
{
  memset(buf, 0, 12);
  buf[0] = XRAN_CP_SECTIONEXTCMD_1;
  buf[1] = 3; // 3 hdr + 8 IQ bytes, padded to 3 words
  buf[2] = 0; // bfwIqWidth 16, bfwCompMeth NONE
  const int16_t v[4] = {w[0].r, w[0].i, w[1].r, w[1].i};
  for (int i = 0; i < 4; i++) {
    buf[3 + 2 * i] = (uint16_t)v[i] >> 8;
    buf[4 + 2 * i] = v[i] & 0xFF;
  }
  return 12;
}

// Weight w of bundle b as build_ext11() writes it
static c16_t ext11_weight(int b, int w)
{
  return (c16_t){.r = (int16_t)b, .i = (int16_t)(0xFF00 | (uint8_t)-w)};
}

// Rewrites the beamIds of an ext11 from build_ext11() (weights present) to first_beam + b
static void set_ext11_beams(uint8_t *buf, int n_bundles, int n_weights, int first_beam)
{
  for (int b = 0; b < n_bundles; b++) {
    uint8_t *beam = buf + 6 + b * (2 + 4 * n_weights);
    beam[0] = (first_beam + b) >> 8;
    beam[1] = (first_beam + b) & 0xFF;
  }
}

static bool weights_equal(const c16_t *a, const c16_t *b, int n)
{
  return memcmp(a, b, n * sizeof(*a)) == 0;
}

// Sends a UL section for a symbol 7 ahead of cur_sym (in the UL part of the test TDD pattern) and returns its job
static ul_job_t send_ul_section(void *ctx, uint64_t cur_sym, uint16_t beam_id, int num_prbc, const uint8_t *ext, size_t ext_len)
{
  send_section1(ctx, XRAN_DIR_UL, cur_sym + 7, 0, beam_id, 0, num_prbc, ext, ext_len);
  ul_job_t job;
  assert(poll_ul_job(ctx, &job) == 0);
  assert(job.beam_id == beam_id);
  return job;
}

// DL: each section's weights reach the dl_iq_stream_t of its PRB runs, by section, also when two sections share a
// (eaxc, beam) stream
void test_dl_dynamic_bf_weights(void)
{
  void *ctx = test_init_ctx(&test_ctx_defaults);
  set_num_bf_weights(ctx, 2);
  uint64_t cur_sym = 1000;
  handle_absolute_symbol_tick(ctx, cur_sym);
  uint64_t target_sym = cur_sym + 7;

  // Section 1: beam 5 with ext1 weights, PRBs 0-1. Section 2: ext11, PRBs 10-12 in 2-PRB bundles (beams 100, 101).
  // Section 3: beam 5 again without an extension, PRBs 20-21, sharing section 1's stream. Section 4: beam 6, never
  // given weights.
  const c16_t w1[2] = {{1000, -1000}, {-2000, 2000}};
  uint8_t ext1[12];
  size_t ext1_len = build_ext1_2w(ext1, w1);
  static uint8_t ext11[EXT11_TEST_BUF_SIZE];
  size_t ext11_len = build_ext11(ext11, false, false, 2, 2, 2);
  send_section1(ctx, XRAN_DIR_DL, target_sym, 1, 5, 0, 2, ext1, ext1_len);
  send_section1(ctx, XRAN_DIR_DL, target_sym, 2, 9, 10, 3, ext11, ext11_len);
  send_section1(ctx, XRAN_DIR_DL, target_sym, 3, 5, 20, 2, NULL, 0);
  send_section1(ctx, XRAN_DIR_DL, target_sym, 4, 6, 30, 1, NULL, 0);
  oru_packet_processor_stats_t stats;
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_late == 0 && stats.cplane_err_early == 0 && stats.dl_tdd_mismatch == 0);
  assert(stats.bfw_updates == 3);

  cur_sym += 3;
  handle_absolute_symbol_tick(ctx, cur_sym);
  send_dl_uplane(ctx, 1, target_sym, 1, 0, 2, 0);
  // Section 2 split into two U-plane fragments: both carry the section's bundles
  send_dl_uplane(ctx, 1, target_sym, 2, 10, 1, 0);
  send_dl_uplane(ctx, 1, target_sym, 2, 11, 2, 0);
  send_dl_uplane(ctx, 1, target_sym, 3, 20, 2, 0);
  send_dl_uplane(ctx, 1, target_sym, 4, 30, 1, 0);
  cur_sym += 10;
  handle_absolute_symbol_tick(ctx, cur_sym);

  const int n = read_dl_until(ctx, 1, target_sym, NULL, 0, 0, NULL);
  assert(n == 5);

  int checked = 0;
  for (int i = 0; i < n; i++) {
    const dl_iq_stream_t *st = &g_dl_streams[i];
    uint16_t beam_id;
    c16_t w[2];
    if (st->start_prb == 0 || st->start_prb == 20) {
      // ext1, and the later section of the same beam reusing those weights
      assert(st->beam_id == 5);
      // Each section of the shared stream has its own set
      assert(st->bfw.num_bundles == 1 && st->bfw.first_prb == st->start_prb && st->bfw.bundle_prbs == 2);
      assert(oru_bfw_bundle(&st->bfw, st->start_prb + 1) == 0);
      assert(get_bf_weights(ctx, &st->bfw, 0, &beam_id, w) == 2);
      assert(beam_id == 5 && weights_equal(w, w1, 2));
    } else if (st->start_prb == 10 || st->start_prb == 11) {
      assert(st->bfw.num_bundles == 2 && st->bfw.bundle_prbs == 2 && st->bfw.first_prb == 10);
      assert(oru_bfw_bundle(&st->bfw, 11) == 0 && oru_bfw_bundle(&st->bfw, 12) == 1);
      for (int b = 0; b < 2; b++) {
        assert(get_bf_weights(ctx, &st->bfw, b, &beam_id, w) == 2);
        assert(beam_id == 100 + b);
        assert(weights_equal(w, (c16_t[]){ext11_weight(b, 0), ext11_weight(b, 1)}, 2));
      }
    } else {
      // No weights were ever set for beam 6: static codebook
      assert(st->start_prb == 30 && st->beam_id == 6 && st->bfw.num_bundles == 0);
    }
    checked++;
  }
  assert(checked == 5);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.bfw_stale == 0);

  cleanup_packet_processor(ctx);
}

// The beamId -> weights cache: hits, misses, snapshots that outlive an update, beamId 0, pool exhaustion, ring wrap
// and a weight count change
void test_bf_weight_cache(void)
{
  void *ctx = test_init_ctx(&test_ctx_defaults);
  // 1030 + 7 is in the UL slots of the 5-slot pattern
  const uint64_t cur_sym = 1030;
  handle_absolute_symbol_tick(ctx, cur_sym);
  oru_packet_processor_stats_t stats;
  uint16_t beam_id;
  c16_t w[2];
  const c16_t w1[2] = {{1, 2}, {3, 4}};
  const c16_t w2[2] = {{5, 6}, {7, 8}};
  uint8_t ext1[12];
  size_t ext1_len = build_ext1_2w(ext1, w1);

  // num_bf_weights 0: no dynamic weights at all, even with ext1
  ul_job_t job = send_ul_section(ctx, cur_sym, 7, 4, ext1, ext1_len);
  assert(job.bfw.num_bundles == 0);

  set_num_bf_weights(ctx, 2);
  // Never-set beamId: static codebook
  job = send_ul_section(ctx, cur_sym, 7, 4, NULL, 0);
  assert(job.bfw.num_bundles == 0);

  // ext1 sets beam 7; a later section citing beam 7 without an extension gets the same weights
  ul_job_t job_w1 = send_ul_section(ctx, cur_sym, 7, 4, ext1, ext1_len);
  assert(job_w1.bfw.num_bundles == 1 && job_w1.bfw.bundle_prbs == 4 && job_w1.bfw.first_prb == 0);
  assert(get_bf_weights(ctx, &job_w1.bfw, 0, &beam_id, w) == 2 && beam_id == 7 && weights_equal(w, w1, 2));
  ul_job_t job_hit = send_ul_section(ctx, cur_sym, 7, 4, NULL, 0);
  assert(get_bf_weights(ctx, &job_hit.bfw, 0, &beam_id, w) == 2 && weights_equal(w, w1, 2));
  // The same weights again are not an update
  send_ul_section(ctx, cur_sym, 7, 4, ext1, ext1_len);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.bfw_updates == 1);

  // New weights for beam 7: new sections get them, the earlier section keeps the ones it was sent with
  build_ext1_2w(ext1, w2);
  ul_job_t job_w2 = send_ul_section(ctx, cur_sym, 7, 4, ext1, ext1_len);
  assert(get_bf_weights(ctx, &job_w2.bfw, 0, &beam_id, w) == 2 && weights_equal(w, w2, 2));
  assert(get_bf_weights(ctx, &job_hit.bfw, 0, &beam_id, w) == 2 && weights_equal(w, w1, 2));
  job = send_ul_section(ctx, cur_sym, 7, 4, NULL, 0);
  assert(get_bf_weights(ctx, &job.bfw, 0, &beam_id, w) == 2 && weights_equal(w, w2, 2));

  // beamId 0 is no beamforming: its ext1 weights are not stored
  job = send_ul_section(ctx, cur_sym, 0, 4, ext1, ext1_len);
  assert(job.bfw.num_bundles == 0);

  // ext11 without weights uses the stored weights of each bundle's beamId: beam 100 has none, beam 7 has w2
  static uint8_t ext11[EXT11_TEST_BUF_SIZE];
  size_t len = build_ext11(ext11, false, true, 2, 2, 0);
  ext11[8] = 7; // second bundle's beamId (5-byte header, then 2 bytes per bundle)
  job = send_ul_section(ctx, cur_sym, 3, 4, ext11, len);
  assert(job.bfw.num_bundles == 2);
  assert(get_bf_weights(ctx, &job.bfw, 0, &beam_id, w) == 0 && beam_id == 100);
  assert(get_bf_weights(ctx, &job.bfw, 1, &beam_id, w) == 2 && beam_id == 7 && weights_equal(w, w2, 2));
  get_packet_processor_stats(ctx, &stats);
  assert(stats.bfw_updates == 2 && stats.bfw_stale == 0);

  // cuBB gives every bundle of every layer a new beamId in each slot: 16 ext11 x 273 bundles of new beams are all stored
  len = build_ext11(ext11, false, false, 1, 273, 2);
  for (int p = 0; p < 16; p++) {
    set_ext11_beams(ext11, 273, 2, 1000 + 273 * p);
    send_ul_section(ctx, cur_sym, 3, 0, ext11, len);
  }
  get_packet_processor_stats(ctx, &stats);
  assert(stats.bfw_updates == 2 + 273 * 16);
  // New weights for beam 7: the section that read the old ones keeps them, as their slot goes to the back of the free
  // list
  build_ext1_2w(ext1, w1);
  job = send_ul_section(ctx, cur_sym, 7, 4, ext1, ext1_len);
  assert(get_bf_weights(ctx, &job.bfw, 0, &beam_id, w) == 2 && weights_equal(w, w1, 2));
  assert(get_bf_weights(ctx, &job_w2.bfw, 0, &beam_id, w) == 2 && weights_equal(w, w2, 2));
  // ...until the free slots ahead of it are used, at least 32768. New weights in each pass over the dynamic beamIds
  // of cuBB's sample config (16528..32767), as cuBB sends from slot to slot.
  int stores = 0;
  for (int pass = 1; get_bf_weights(ctx, &job_w2.bfw, 0, &beam_id, w) == 2; pass++) {
    for (int b = 0; b < 273; b++)
      ext11[6 + b * 10 + 3] = pass; // first weight of bundle b
    for (int first = 16528; first + 273 <= 32768; first += 273, stores += 273) {
      set_ext11_beams(ext11, 273, 2, first);
      send_ul_section(ctx, cur_sym, 3, 0, ext11, len);
    }
  }
  assert(stores >= 32768);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.bfw_stale == 1);

  // Ring wrap: after 2^18 more bundle refs, an old section's refs are overwritten
  ul_job_t job_old = send_ul_section(ctx, cur_sym, 7, 4, NULL, 0);
  len = build_ext11(ext11, false, true, 1, 273, 0);
  for (int p = 0; p < (1 << 18) / 273 + 1; p++)
    send_ul_section(ctx, cur_sym, 3, 0, ext11, len);
  assert(get_bf_weights(ctx, &job_old.bfw, 0, &beam_id, w) == -1);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.bfw_stale == 2);

  // Changing the weight count drops all stored weights
  job = send_ul_section(ctx, cur_sym, 7, 4, NULL, 0);
  set_num_bf_weights(ctx, 4);
  assert(get_bf_weights(ctx, &job.bfw, 0, &beam_id, w) == -1);
  job = send_ul_section(ctx, cur_sym, 7, 4, NULL, 0);
  assert(job.bfw.num_bundles == 0);

  cleanup_packet_processor(ctx);
}

// Vectors of more than one weight count, as cuBB sends num_rx_ants dynamic weights and 64 static ones
void test_bf_weight_counts(void)
{
  void *ctx = test_init_ctx(&test_ctx_defaults);
  // 1030 + 7 is in the UL slots of the 5-slot pattern
  const uint64_t cur_sym = 1030;
  handle_absolute_symbol_tick(ctx, cur_sym);
  const int counts[2] = {2, 6};
  set_bf_weight_counts(ctx, counts, 2);
  oru_packet_processor_stats_t stats;
  uint16_t beam_id;
  c16_t w[ORU_MAX_BF_WEIGHTS];

  // ext1 with 2 weights for beam 7
  const c16_t w1[2] = {{1, 2}, {3, 4}};
  uint8_t ext1[12];
  size_t ext1_len = build_ext1_2w(ext1, w1);
  ul_job_t job = send_ul_section(ctx, cur_sym, 7, 4, ext1, ext1_len);
  assert(get_bf_weights(ctx, &job.bfw, 0, &beam_id, w) == 2 && beam_id == 7 && weights_equal(w, w1, 2));

  // ext11 with 6 weights for beams 100 and 101
  static uint8_t ext11[EXT11_TEST_BUF_SIZE];
  size_t len = build_ext11(ext11, false, false, 2, 2, 6);
  job = send_ul_section(ctx, cur_sym, 3, 4, ext11, len);
  assert(job.bfw.num_bundles == 2);
  for (int b = 0; b < 2; b++) {
    assert(get_bf_weights(ctx, &job.bfw, b, &beam_id, w) == 6 && beam_id == 100 + b);
    for (int k = 0; k < 6; k++)
      assert(w[k].r == ext11_weight(b, k).r && w[k].i == ext11_weight(b, k).i);
  }

  // ext11 without weights: each bundle gets the weights of its beamId, whatever their count
  len = build_ext11(ext11, false, true, 2, 2, 0);
  ext11[8] = 7; // second bundle's beamId (5-byte header, then 2 bytes per bundle)
  job = send_ul_section(ctx, cur_sym, 3, 4, ext11, len);
  assert(get_bf_weights(ctx, &job.bfw, 0, &beam_id, w) == 6 && beam_id == 100);
  assert(get_bf_weights(ctx, &job.bfw, 1, &beam_id, w) == 2 && beam_id == 7 && weights_equal(w, w1, 2));

  // 6 weights for beam 7 replace its 2
  len = build_ext11(ext11, false, false, 4, 1, 6);
  set_ext11_beams(ext11, 1, 6, 7);
  job = send_ul_section(ctx, cur_sym, 3, 4, ext11, len);
  assert(get_bf_weights(ctx, &job.bfw, 0, &beam_id, w) == 6 && beam_id == 7);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.bfw_updates == 4 && stats.cplane_err_sect_ext11 == 0);

  // 4 weights is no configured count: the ext11 doesn't decode, and the section uses its beamId
  len = build_ext11(ext11, false, false, 2, 2, 4);
  job = send_ul_section(ctx, cur_sym, 7, 4, ext11, len);
  get_packet_processor_stats(ctx, &stats);
  assert(stats.cplane_err_sect_ext11 == 1 && stats.bfw_updates == 4);
  assert(job.bfw.num_bundles == 1 && get_bf_weights(ctx, &job.bfw, 0, &beam_id, w) == 6 && beam_id == 7);

  cleanup_packet_processor(ctx);
}

// Section Type 3: ext1/ext11 weights are stored as for Type 1 and each PRACH job carries its own set, read by symbol with
// get_prach_bfw_set() and get_bf_weights()
void test_prach_bf_weights(void)
{
  void *ctx = init_prach_test_ctx();
  set_num_bf_weights(ctx, 2);
  const int num_symbols_per_frame = 10 * 2 * 14;
  const int slot_in_frame = (1035 % num_symbols_per_frame) / 14;
  const int symbol = 1035 % 14;
  // Each case uses the same slot of a later frame, which releases the previous case's jobs
  uint64_t target_sym = 1035;
  oru_packet_processor_stats_t stats;
  oru_bfw_set_t set;
  int start_prb;
  uint16_t beam_id;
  c16_t w[2];

  // ext1: one bundle covering the section's 20 PRBs from PRB 3, with beam 5's new weights
  const c16_t w1[2] = {{1000, -1000}, {-2000, 2000}};
  uint8_t ext1[12];
  size_t ext1_len = build_ext1_2w(ext1, w1);
  const test_section3_t with_ext1 = {1, 5, ext1, ext1_len, false, 3};
  send_prach_cplane(ctx, target_sym, 1, &with_ext1, 1);
  assert(get_prach_bfw_set(ctx, slot_in_frame, 0, symbol, &set, &start_prb) == 5);
  assert(start_prb == 3 && set.num_bundles == 1 && set.bundle_prbs == 20 && set.first_prb == 3);
  assert(get_bf_weights(ctx, &set, 0, &beam_id, w) == 2 && beam_id == 5 && weights_equal(w, w1, 2));
  // No C-Plane for this stream, or for a symbol before the section
  assert(get_prach_bfw_set(ctx, slot_in_frame, 1, symbol, &set, &start_prb) == -1);
  assert(get_prach_bfw_set(ctx, slot_in_frame, 0, symbol - 1, &set, &start_prb) == -1);

  // Two TDM occasions (symbols 5-8 and 9-12), without extensions: beam 5 gets the weights stored above, beam 9 has none
  // (static codebook)
  const test_section3_t occasion_a = {2, 5}, occasion_b = {3, 9};
  target_sym += num_symbols_per_frame;
  handle_absolute_symbol_tick(ctx, target_sym - 5); // beam lookups resolve the symbol nearest to now
  send_prach_cplane(ctx, target_sym - symbol + 5, 1, &occasion_a, 1);
  send_prach_cplane(ctx, target_sym - symbol + 9, 1, &occasion_b, 1);
  assert(get_prach_bfw_set(ctx, slot_in_frame, 0, 5, &set, &start_prb) == 5);
  assert(set.num_bundles == 1 && set.bundle_prbs == 20 && set.first_prb == 0);
  assert(get_bf_weights(ctx, &set, 0, &beam_id, w) == 2 && beam_id == 5 && weights_equal(w, w1, 2));
  assert(get_prach_bfw_set(ctx, slot_in_frame, 0, 9, &set, &start_prb) == 9);
  assert(set.num_bundles == 0);

  // ext11 with weights: 10 bundles of 2 PRBs from PRB 4, beamIds 100 + b; the section beamId 6 is ignored
  uint8_t ext[EXT11_TEST_BUF_SIZE];
  test_section3_t with_ext11 = {4, 6, ext, build_ext11(ext, false, false, 2, 10, 2), false, 4};
  target_sym += num_symbols_per_frame;
  handle_absolute_symbol_tick(ctx, target_sym - 5); // beam lookups resolve the symbol nearest to now
  send_prach_cplane(ctx, target_sym, 1, &with_ext11, 1);
  assert(get_prach_bfw_set(ctx, slot_in_frame, 0, symbol, &set, &start_prb) == 6);
  assert(set.num_bundles == 10 && set.bundle_prbs == 2 && set.first_prb == 4);
  assert(oru_bfw_bundle(&set, 23) == 9);
  for (int b = 0; b < 10; b++) {
    assert(get_bf_weights(ctx, &set, b, &beam_id, w) == 2 && beam_id == 100 + b);
    assert(weights_equal(w, (c16_t[]){ext11_weight(b, 0), ext11_weight(b, 1)}, 2));
  }

  // ext11 with disableBFWs and bundleOffset 1: 6 bundles of 4 PRBs from PRB 3, using the weights just stored for beamIds
  // 100-104, and none for beamId 200 of the last bundle
  with_ext11.ext_len = build_ext11(ext, false, true, 4, 6, 0);
  ext[3] |= 1;
  ext[16] = 200; // last bundle's beamId (5-byte header, then 2 bytes per bundle)
  target_sym += num_symbols_per_frame;
  handle_absolute_symbol_tick(ctx, target_sym - 5); // beam lookups resolve the symbol nearest to now
  send_prach_cplane(ctx, target_sym, 1, &with_ext11, 1);
  assert(get_prach_bfw_set(ctx, slot_in_frame, 0, symbol, &set, &start_prb) == 6);
  assert(start_prb == 4 && set.num_bundles == 6 && set.bundle_prbs == 4 && set.first_prb == 3);
  for (int b = 0; b < 5; b++) {
    assert(get_bf_weights(ctx, &set, b, &beam_id, w) == 2 && beam_id == 100 + b);
    assert(weights_equal(w, (c16_t[]){ext11_weight(b, 0), ext11_weight(b, 1)}, 2));
  }
  assert(get_bf_weights(ctx, &set, 5, &beam_id, w) == 0 && beam_id == 200);

  // A malformed ext11 (extLen past the end of the packet) on beam 5: the section gets beam 5's stored weights
  with_ext11.ext_len = build_ext11(ext, false, false, 2, 10, 2);
  ext[1] = 1;
  with_ext11.beam_id = 5;
  target_sym += num_symbols_per_frame;
  handle_absolute_symbol_tick(ctx, target_sym - 5); // beam lookups resolve the symbol nearest to now
  send_prach_cplane(ctx, target_sym, 1, &with_ext11, 1);
  assert(get_prach_bfw_set(ctx, slot_in_frame, 0, symbol, &set, &start_prb) == 5);
  assert(set.num_bundles == 1 && get_bf_weights(ctx, &set, 0, &beam_id, w) == 2 && weights_equal(w, w1, 2));

  get_packet_processor_stats(ctx, &stats);
  assert(stats.bfw_updates == 11 && stats.cplane_err_sect_ext11 == 1 && stats.bfw_stale == 0);
  cleanup_packet_processor(ctx);
}

int main(int argc, char **argv)
{
  setup_dpdk(argc, argv);
  RUN_TEST(test_uplink_prb_offset());
  RUN_TEST(test_init_cleanup());
  RUN_TEST(test_cplane_timing_errors());
  RUN_TEST(test_cplane_section_extension_1());
  RUN_TEST(test_cplane_section_extension_11());
  RUN_TEST(test_cplane_section_extension_11_buffer_size());
  RUN_TEST(test_cplane_section_extension_other());
  RUN_TEST(test_dl_dynamic_bf_weights());
  RUN_TEST(test_bf_weight_cache());
  RUN_TEST(test_bf_weight_counts());
  RUN_TEST(test_dl_multi_section_message());
  RUN_TEST(test_ul_multi_section_message());
  RUN_TEST(test_multi_section_timing_counted_once());
  RUN_TEST(test_multi_section_extensions_and_truncation());
  RUN_TEST(test_section_id_repeated());
  RUN_TEST(test_section_sym_inc());
  RUN_TEST(test_cplane_uplane_match());
  RUN_TEST(test_uplane_prb_range_rejected());
  RUN_TEST(test_uplane_short_payload_rejected());
  RUN_TEST(test_dl_multi_section_same_beam());
  RUN_TEST(test_frame_wrap_around());
  RUN_TEST(test_cplane_14_symbols());
  RUN_TEST(test_other_bw_4ant_prb_offset());
  RUN_TEST(test_dl_wideband_fragments_4ant());
  RUN_TEST(test_uplink_basic());
  RUN_TEST(test_uplink_fragmentation());
  RUN_TEST(test_uplink_large_mtu());
  RUN_TEST(test_prach_generation(4, false));
  const int offsets[] = {0, 1, 4, 15};
  for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++)
    RUN_TEST(test_prach_generation(offsets[i], true));
  RUN_TEST(test_prach_occasions());
  RUN_TEST(test_prach_multi_section_message());
  RUN_TEST(test_prach_section_extensions());
  RUN_TEST(test_prach_bf_weights());
  RUN_TEST(test_hyper_frame_calculation());
  RUN_TEST(test_dl_bfp_decompression());
  RUN_TEST(test_ul_bfp_compression());
  RUN_TEST(test_large_delay_profile());

  printf("All tests passed!\n");
  return 0;
}
