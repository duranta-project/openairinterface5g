/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// O-DU to O-RU loopback: the O-DU TX scheduler, the O-RU packet processor and the O-DU packet
// processor are wired back to back through their send callbacks, on one symbol clock. Each test
// pushes IQ through one direction and checks it arrives intact, inside the other side's timing
// windows.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <rte_eal.h>
#include <rte_mbuf.h>
#include <rte_byteorder.h>
#include "fh_compression.h"
#include "du_tx_scheduler.h"
#include "du_packet_processor.h"
#include "oru_packet_processor.h"
#include "log.h"
#include "common/config/config_userapi.h"

// OAI Linkage Satisfiers
void exit_function(const char *file, const char *function, const int line, const char *s, const int assertflag)
{
  fprintf(stderr, "Error at %s:%s:%d - %s\n", file, function, line, s ? s : "None");
  exit(1);
}
configmodule_interface_t *uniqCfg = NULL;

#define MU 1
#define NUM_PRB 106
#define NB_ANT 2
#define PRACH_EAXC_OFFSET 4
#define PRACH_KBAR 4
#define SYMBOLS_PER_FRAME (10 * (1 << MU) * 14)
// TDD pattern of two slots: even slots DL, odd slots UL
#define TDD_PATTERN_SLOTS 2

// With a 35 us symbol at mu=1, the O-DU sends DL C-Plane and UL C-Plane 11 symbols and DL U-Plane
// 7 symbols ahead of OTA. The O-RU accepts C-Plane 2..14 and U-Plane 1..10 symbols ahead, and the
// O-DU accepts UL U-Plane 0..5 symbols after OTA.
#define T1A_UP_US 100, 300
#define T1A_CP_DL_US 250, 450
#define T1A_CP_UL_US 250, 450
#define T2A_CP_US 100, 500
#define T2A_UP_US 50, 350
#define TA3_US 0, 200

struct xran_eaxcid_config g_eaxcid_config = {.mask_cuPortId = 0xF000,
                                             .mask_bandSectorId = 0x0F00,
                                             .mask_ccId = 0x00F0,
                                             .mask_ruPortId = 0x000F,
                                             .bit_cuPortId = 12,
                                             .bit_bandSectorId = 8,
                                             .bit_ccId = 4,
                                             .bit_ruPortId = 0};

static struct rte_mempool *mp;
static void *g_du_tx;
static void *g_du_rx;
static void *g_oru;

static void *alloc_mbuf(void *io_controller)
{
  return rte_pktmbuf_alloc(mp);
}

// O-DU -> O-RU wire, routed like oru_fh.c's rx_cb
static void du_to_oru(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++) {
    struct xran_ecpri_hdr *ecpri = rte_pktmbuf_mtod(mbufs[i], struct xran_ecpri_hdr *);
    if (ecpri->cmnhdr.bits.ecpri_mesg_type == ECPRI_RT_CONTROL_DATA)
      handle_cplane_packet(g_oru, mbufs[i]);
    else
      handle_uplane_packet(g_oru, mbufs[i]);
  }
}

// O-RU -> O-DU wire, routed like du_fh.c's rx_cb
static void oru_to_du(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++) {
    struct xran_ecpri_hdr *ecpri = rte_pktmbuf_mtod(mbufs[i], struct xran_ecpri_hdr *);
    uint8_t cu_port_id, band_sector_id, cc_id, ant_id;
    xran_decompose_cid(ecpri->ecpri_xtc_id, &g_eaxcid_config, &cu_port_id, &band_sector_id, &cc_id, &ant_id);
    if (ant_id >= PRACH_EAXC_OFFSET)
      du_packet_processor_handle_prach_uplane_packet(g_du_rx, mbufs[i]);
    else
      du_packet_processor_handle_uplane_packet(g_du_rx, mbufs[i]);
  }
}

static void setup(fh_comp_method_t comp, uint8_t iq_width)
{
  g_du_tx = init_du_tx_scheduler(MU,
                                 NUM_PRB,
                                 T1A_UP_US,
                                 T1A_CP_DL_US,
                                 T1A_CP_UL_US,
                                 1500,
                                 comp,
                                 iq_width,
                                 g_eaxcid_config,
                                 alloc_mbuf,
                                 du_to_oru,
                                 NULL);
  g_du_rx = init_du_packet_processor(MU, NUM_PRB, TA3_US, 1, 0, TDD_PATTERN_SLOTS, false, 1500, PRACH_EAXC_OFFSET);
  g_oru = init_packet_processor(MU,
                                NUM_PRB,
                                T2A_CP_US,
                                T2A_UP_US,
                                1,
                                1,
                                0,
                                0,
                                TDD_PATTERN_SLOTS,
                                alloc_mbuf,
                                oru_to_du,
                                NULL,
                                1500,
                                PRACH_EAXC_OFFSET,
                                comp,
                                PRACH_KBAR);
  assert(g_du_tx && g_du_rx && g_oru);
}

static void teardown(void)
{
  cleanup_du_tx_scheduler(g_du_tx);
  cleanup_du_packet_processor(g_du_rx);
  cleanup_packet_processor(g_oru);
}

typedef struct {
  uint64_t hyper_frame;
  int frame;
  int slot;
  int symbol;
} sym_time_t;

static sym_time_t to_time(uint64_t absolute_symbol)
{
  return (sym_time_t){.hyper_frame = absolute_symbol / SYMBOLS_PER_FRAME / 1024,
                      .frame = (absolute_symbol / SYMBOLS_PER_FRAME) % 1024,
                      .slot = (absolute_symbol % SYMBOLS_PER_FRAME) / 14,
                      .symbol = absolute_symbol % 14};
}

// One symbol of the shared clock. The O-RU and the O-DU RX tick first, so that what the O-DU
// TX sends from its own tick arrives at an O-RU that is already at this symbol.
static void tick(uint64_t absolute_symbol)
{
  handle_absolute_symbol_tick(g_oru, absolute_symbol);
  du_packet_processor_handle_absolute_symbol_tick(g_du_rx, absolute_symbol);
  du_tx_handle_absolute_symbol_tick(g_du_tx, absolute_symbol);
}

static void fill_pattern(uint32_t *buf, int ant)
{
  int16_t *iq = (int16_t *)buf;
  for (int i = 0; i < NUM_PRB * 12 * 2; i++)
    iq[i] = (int16_t)(((i * 37 + ant * 1001) % 4001) - 2000);
}

// What a lossy round trip through the given compression gives back
static void expected_after_compression(uint32_t *dst, const uint32_t *src, fh_comp_method_t comp, uint8_t iq_width)
{
  if (comp == FH_COMP_NONE) {
    memcpy(dst, src, NUM_PRB * 12 * sizeof(uint32_t));
    return;
  }
  int8_t packed[NUM_PRB * FH_VALS_PER_PRB * 2];
  fh_compress_prbs(comp, iq_width, NUM_PRB, (const int16_t *)src, packed);
  fh_decompress_prbs(comp, iq_width, NUM_PRB, packed, (int16_t *)dst);
}

static void check_oru_windows(void)
{
  oru_packet_processor_stats_t s;
  get_packet_processor_stats(g_oru, &s);
  assert(s.cplane_err_early == 0 && s.cplane_err_late == 0);
  assert(s.uplane_err_early == 0 && s.uplane_err_late == 0);
  assert(s.dl_tdd_mismatch == 0 && s.ul_tdd_mismatch == 0);
  assert(s.uplane_missing_cplane == 0);
}

static void check_du_windows(void)
{
  du_tx_scheduler_stats_t tx;
  du_tx_get_stats(g_du_tx, &tx);
  assert(tx.app_too_late_tx == 0 && tx.dl_cplane_too_late == 0 && tx.ul_grant_too_late == 0);
  du_packet_processor_stats_t rx;
  du_packet_processor_get_stats(g_du_rx, &rx);
  assert(rx.uplane_err_early == 0 && rx.uplane_err_late == 0 && rx.ul_tdd_mismatch == 0);
  assert(rx.prach_uplane_err_early == 0 && rx.prach_uplane_err_late == 0 && rx.prach_missing_expect_job == 0);
}

static const uint64_t start_symbol = 2000 * 14; // first symbol of an even (DL) slot

static void test_dl(fh_comp_method_t comp, uint8_t iq_width)
{
  printf("Testing O-DU -> O-RU DL, comp %d iq_width %d...\n", comp, iq_width);
  setup(comp, iq_width);
  tick(start_symbol);

  // Symbol 3 of the DL slot two slots ahead, with two beams that split the carrier
  uint64_t ota = start_symbol + 2 * 14 + 3;
  sym_time_t t = to_time(ota);
  static uint32_t tx_iq[NB_ANT][NUM_PRB * 12];
  uint32_t *txdataF[NB_ANT];
  for (int a = 0; a < NB_ANT; a++) {
    fill_pattern(tx_iq[a], a);
    txdataF[a] = tx_iq[a];
  }
  du_tx_dl_section_t sections[2] = {{.beam_id = 1, .start_prb = 0, .num_prb = 50, .section_id = 1},
                                    {.beam_id = 2, .start_prb = 50, .num_prb = NUM_PRB - 50, .section_id = 2}};
  du_tx_schedule_dl_iq(g_du_tx, txdataF, NB_ANT, t.hyper_frame, t.frame, t.slot, t.symbol, sections, 2);
  // the O-DU keeps its own copy, so the caller's buffer may be reused right away
  static uint32_t sent[NB_ANT][NUM_PRB * 12];
  memcpy(sent, tx_iq, sizeof(sent));
  memset(tx_iq, 0x5a, sizeof(tx_iq));

  static uint32_t rx_iq[NB_ANT][NUM_PRB * 12];
  memset(rx_iq, 0, sizeof(rx_iq));
  static dl_iq_stream_t streams[MAX_DL_FRAGMENTS_PER_SYMBOL];
  static uint32_t arena[DL_IQ_ARENA_PRBS(NUM_PRB) * 12];
  bool found = false;
  int num_fragments = 0;
  for (uint64_t s = start_symbol + 1; s <= ota + 1; s++) {
    tick(s);
    while (get_ready_job_count(g_oru) > 0) {
      uint64_t hf;
      int frame, slot, symbol;
      int n = read_dl_iq_streams(g_oru, streams, arena, MAX_DL_FRAGMENTS_PER_SYMBOL, &hf, &frame, &slot, &symbol);
      if (frame != t.frame || slot != t.slot || symbol != t.symbol)
        continue;
      found = true;
      num_fragments = n;
      for (int i = 0; i < n; i++) {
        assert(streams[i].ant_id < NB_ANT);
        assert(streams[i].beam_id == (streams[i].start_prb < 50 ? 1 : 2));
        memcpy(&rx_iq[streams[i].ant_id][streams[i].start_prb * 12], streams[i].iq, streams[i].num_prb * 12 * sizeof(uint32_t));
      }
    }
  }
  assert(found);
  assert(num_fragments >= 2 * NB_ANT); // at least one fragment per section and antenna
  for (int a = 0; a < NB_ANT; a++) {
    static uint32_t expected[NUM_PRB * 12];
    expected_after_compression(expected, sent[a], comp, iq_width);
    assert(memcmp(rx_iq[a], expected, sizeof(expected)) == 0);
  }
  check_oru_windows();
  check_du_windows();
  teardown();
  printf("O-DU -> O-RU DL passed!\n");
}

static void test_ul(fh_comp_method_t comp, uint8_t iq_width)
{
  printf("Testing O-RU -> O-DU UL, comp %d iq_width %d...\n", comp, iq_width);
  setup(comp, iq_width);
  tick(start_symbol);

  // Symbol 5 of the UL slot three slots ahead
  uint64_t ota = start_symbol + 3 * 14 + 5;
  sym_time_t t = to_time(ota);
  du_tx_dl_section_t section = {.beam_id = 3, .start_prb = 0, .num_prb = NUM_PRB, .section_id = 7};
  for (int a = 0; a < NB_ANT; a++) {
    du_tx_schedule_ul_grant(g_du_tx, t.hyper_frame, t.frame, t.slot, t.symbol, a, &section, 1);
    du_packet_processor_expect_ul_symbol(g_du_rx, ota, a, section.section_id, 0, NUM_PRB, comp, iq_width);
  }

  static uint32_t ul_iq[NB_ANT][NUM_PRB * 12];
  for (int a = 0; a < NB_ANT; a++)
    fill_pattern(ul_iq[a], a + 2);
  static uint32_t rx_iq[NB_ANT][NUM_PRB * 12];
  uint32_t *rxdataF[NB_ANT] = {rx_iq[0], rx_iq[1]};
  int num_ul_jobs = 0;
  bool found = false;
  for (uint64_t s = start_symbol + 1; s <= ota + 6 && !found; s++) {
    tick(s);
    // the O-RU answers its UL C-Plane one symbol after OTA, inside the O-DU's Ta3 window
    if (s == ota + 1) {
      ul_job_t job;
      while (poll_ul_job(g_oru, &job) == 0) {
        assert(job.frame == t.frame && job.slot_in_frame == t.slot && job.symbol == t.symbol);
        assert(job.start_prb == 0 && job.num_prb == NUM_PRB && job.beam_id == 3);
        assert(job.response_payload.section_id == 7);
        write_ul_iq(g_oru, ul_iq[job.antenna_id], t.symbol, &job);
        num_ul_jobs++;
      }
    }
    uint64_t abs;
    while (du_packet_processor_read_ul_iq_upto(g_du_rx, rxdataF, NB_ANT, ota, &abs)) {
      if (abs == ota)
        found = true;
    }
  }
  assert(num_ul_jobs == NB_ANT);
  assert(found);
  for (int a = 0; a < NB_ANT; a++) {
    static uint32_t expected[NUM_PRB * 12];
    expected_after_compression(expected, ul_iq[a], comp, iq_width);
    assert(memcmp(rx_iq[a], expected, sizeof(expected)) == 0);
  }
  check_oru_windows();
  check_du_windows();
  teardown();
  printf("O-RU -> O-DU UL passed!\n");
}

static void test_prach(fh_comp_method_t comp, uint8_t iq_width)
{
  printf("Testing O-RU -> O-DU PRACH, comp %d iq_width %d...\n", comp, iq_width);
  setup(comp, iq_width);
  tick(start_symbol);

  // Two-symbol occasion at the start of the UL slot three slots ahead
  const int num_symbols = 2;
  uint64_t ota = start_symbol + 3 * 14;
  sym_time_t t = to_time(ota);
  du_tx_prach_section_t prach = {.filter_index = 1,
                                 .time_offset = 0,
                                 .fft_size = 8,
                                 .scs = 1,
                                 .section_id = 9,
                                 .beam_id = 0,
                                 .num_symbol = num_symbols,
                                 .start_prb = 10,
                                 .num_prb = FH_PRACH_NUM_PRBS,
                                 .freq_offset = 0};
  du_tx_schedule_prach(g_du_tx, t.hyper_frame, t.frame, t.slot, t.symbol, PRACH_EAXC_OFFSET, &prach);
  du_packet_processor_expect_prach_occasion(g_du_rx, ota, num_symbols, t.slot, 0, 9, 10, FH_PRACH_NUM_PRBS, comp, iq_width, PRACH_KBAR);

  // One PRACH sequence, padded the way the O-RU expects its input
  static int16_t seq[FH_PRACH_NUM_PRBS * FH_VALS_PER_PRB];
  for (int i = 0; i < FH_PRACH_NUM_SUBCARRIERS * 2; i++)
    seq[i] = (int16_t)((i * 53 % 1001) - 500);
  uint32_t *prach_in[1] = {(uint32_t *)seq};

  int16_t rx[FH_PRACH_NUM_SUBCARRIERS * 2];
  bool found = false;
  for (uint64_t s = start_symbol + 1; s <= ota + num_symbols + 6 && !found; s++) {
    tick(s);
    if (s == ota + 1) {
      bool active = false;
      int section_id = -1, num_prb = -1;
      test_get_prach_cplane_info(g_oru, t.slot, PRACH_EAXC_OFFSET, &active, &section_id, &num_prb);
      assert(active && section_id == 9 && num_prb == FH_PRACH_NUM_PRBS);
    }
    // each PRACH symbol is sent one symbol after its OTA
    if (s > ota && s <= ota + num_symbols)
      write_prach_iq(g_oru, prach_in, 1, t.frame, t.slot, (int)(s - 1 - ota) + t.symbol);
    if (du_packet_processor_get_ready_prach_job_count(g_du_rx) > 0) {
      uint64_t hf;
      int frame, slot, antenna, section_id;
      du_packet_processor_read_prach_iq(g_du_rx, rx, &hf, &frame, &slot, &antenna, &section_id);
      assert(frame == t.frame && slot == t.slot && antenna == 0);
      found = true;
    }
  }
  assert(found);

  // The O-DU accumulates the repetitions
  int16_t expected[FH_PRACH_NUM_PRBS * FH_VALS_PER_PRB] = {0};
  if (comp == FH_COMP_NONE) {
    memcpy(expected, seq, FH_PRACH_NUM_SUBCARRIERS * 2 * sizeof(int16_t));
  } else {
    int8_t packed[FH_PRACH_NUM_PRBS * FH_VALS_PER_PRB * 2];
    int16_t unpacked[FH_PRACH_NUM_PRBS * FH_VALS_PER_PRB];
    fh_compress_prach(comp, iq_width, PRACH_KBAR, seq, packed);
    fh_decompress_prbs(comp, iq_width, FH_PRACH_NUM_PRBS, packed, unpacked);
    memcpy(expected, &unpacked[PRACH_KBAR], FH_PRACH_NUM_SUBCARRIERS * 2 * sizeof(int16_t));
  }
  for (int i = 0; i < FH_PRACH_NUM_SUBCARRIERS * 2; i++)
    assert(rx[i] == (int16_t)(num_symbols * expected[i]));

  oru_packet_processor_stats_t s;
  get_packet_processor_stats(g_oru, &s);
  assert(s.prach_cplane_missing == 0);
  check_du_windows();
  teardown();
  printf("O-RU -> O-DU PRACH passed!\n");
}

int main(int argc, char **argv)
{
  int ret = rte_eal_init(argc, argv);
  assert(ret >= 0);
  logInit();
  mp = rte_pktmbuf_pool_create("test_pool_loopback", 8191, 0, 0, 10000, rte_socket_id());
  assert(mp != NULL);

  test_dl(FH_COMP_NONE, 16);
  test_dl(FH_COMP_BFP, 9);
  test_ul(FH_COMP_NONE, 16);
  test_ul(FH_COMP_BFP, 9);
  test_prach(FH_COMP_NONE, 16);
  test_prach(FH_COMP_BFP, 9);

  printf("All O-DU <-> O-RU loopback tests passed!\n");
  rte_eal_cleanup();
  return 0;
}
