#include "du_tx_scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <rte_eal.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_byteorder.h>

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define MBUF_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)

void exit_function(const char *file, const char *function, const int line, const char *s, const int assertflag)
{
  abort();
}
void *uniqCfg = NULL;

struct rte_mempool *mbuf_pool;


struct rte_mbuf *test_mbufs_captured[1024];
uint32_t test_num_mbufs_captured = 0;
struct rte_mbuf *test_cp_mbufs_captured[1024];
uint32_t test_num_cp_mbufs_captured = 0;


void *mock_alloc_func(void *io_controller) {
  return rte_pktmbuf_alloc(mbuf_pool);
}

void mock_send_func
(void *io_controller, struct rte_mbuf **mbufs, uint32_t num_mbufs) {
  for (uint32_t i = 0; i < num_mbufs; i++) {
    struct xran_ecpri_hdr *ecpri;
    struct xran_recv_packet_info info;
    if (xran_parse_ecpri_hdr(mbufs[i], &ecpri, &info) == 0 && info.msg_type == ECPRI_RT_CONTROL_DATA) {
        test_cp_mbufs_captured[test_num_cp_mbufs_captured++] = mbufs[i];
    } else {
        test_mbufs_captured[test_num_mbufs_captured++] = mbufs[i];
    }
  }
}

void clear_captured_mbufs() {
  for (uint32_t i = 0; i < test_num_mbufs_captured; i++) {
    rte_pktmbuf_free(test_mbufs_captured[i]);
  }
  test_num_mbufs_captured = 0;
  for (uint32_t i = 0; i < test_num_cp_mbufs_captured; i++) {
    rte_pktmbuf_free(test_cp_mbufs_captured[i]);
  }
  test_num_cp_mbufs_captured = 0;
}


uint32_t **allocate_txdata(int nb_tx, int num_prb) {
  uint32_t **txdataF = malloc(nb_tx * sizeof(uint32_t *));
  for (int i = 0; i < nb_tx; i++) {
    txdataF[i] = calloc(num_prb * 12, sizeof(uint32_t));
  }
  return txdataF;
}

void free_txdata(uint32_t **txdataF, int nb_tx) {
  for (int i = 0; i < nb_tx; i++) {
    free(txdataF[i]);
  }
  free(txdataF);
}

void test_basic_scheduling() {
  printf("test_basic_scheduling...\n");
  struct xran_eaxcid_config eaxc = { .mask_cuPortId = 0xF000, .mask_bandSectorId = 0x0F00, .mask_ccId = 0x00F0, .mask_ruPortId = 0x000F, .bit_cuPortId = 12, .bit_bandSectorId = 8, .bit_ccId = 4, .bit_ruPortId = 0 };
  void *ctx = init_du_tx_scheduler(0 /* mu */, 106 /* num_prb */, 200, 300, 100, 400, 100, 500, 1500, FH_COMP_NONE, 16, eaxc, mock_alloc_func, mock_send_func, NULL);
  assert(ctx != NULL);
  
  int mu = 0;
  int slots_per_subframe = 1 << mu;
  uint32_t symbol_duration_uS = 1000 / slots_per_subframe / 14; // ~71uS
  uint32_t T1a_up_max_sym_diff = 300 / symbol_duration_uS; // 4
  
  uint32_t **txdataF = allocate_txdata(1, 106);
  du_tx_dl_section_t sections[1] = {{.beam_id = 0, .start_prb = 0, .num_prb = 10, .section_id = 1}};
  
  uint64_t hyper_frame = 0;
  int frame = 0, slot = 0, symbol = 10;
  // ota_absolute_symbol = 10
  // send_at_symbol = 10 - (4 - 1) = 7
  
  du_tx_handle_absolute_symbol_tick(ctx, 5); // Current symbol = 5
  
  du_tx_schedule_dl_iq(ctx, txdataF, 1, hyper_frame, frame, slot, symbol, sections, 1);
  
  du_tx_scheduler_stats_t stats;
  du_tx_get_stats(ctx, &stats);
  assert(stats.total_dl_scheduled == 1);
  
  du_tx_handle_absolute_symbol_tick(ctx, 6);
  assert(test_num_mbufs_captured == 0);
  
  du_tx_handle_absolute_symbol_tick(ctx, 7); // Should fire here
  assert(test_num_mbufs_captured == 1);
  
  du_tx_get_stats(ctx, &stats);
  assert(stats.total_dl_sent == 1);
  
  clear_captured_mbufs();
  free_txdata(txdataF, 1);
  cleanup_du_tx_scheduler(ctx);
}

void test_wire_format() {
  printf("test_wire_format...\n");
  struct xran_eaxcid_config eaxc = { .mask_cuPortId = 0xF000, .mask_bandSectorId = 0x0F00, .mask_ccId = 0x00F0, .mask_ruPortId = 0x000F, .bit_cuPortId = 12, .bit_bandSectorId = 8, .bit_ccId = 4, .bit_ruPortId = 0 };
  void *ctx = init_du_tx_scheduler(0, 106, 200, 300, 100, 400, 100, 500, 1500, FH_COMP_NONE, 16, eaxc, mock_alloc_func, mock_send_func, NULL);
  
  uint32_t **txdataF = allocate_txdata(1, 106);
  // Distinct marker per int16 slot so a wrong source offset (e.g. reading from the
  // middle of the buffer instead of PRB 5's start) is caught by the content check below.
  int16_t *txdata_i16 = (int16_t *)txdataF[0];
  for (int i = 0; i < 106 * 12 * 2; i++)
    txdata_i16[i] = (int16_t)i;
  du_tx_dl_section_t sections[1] = {{.beam_id = 0, .start_prb = 5, .num_prb = 20, .section_id = 42}};

  du_tx_handle_absolute_symbol_tick(ctx, 10);
  du_tx_schedule_dl_iq(ctx, txdataF, 1, 0, 1, 2, 5, sections, 1); // ota = 1 * 140 + 2 * 14 + 5 = 140 + 28 + 5 = 173
  // send_at = 173 - 3 = 170

  du_tx_handle_absolute_symbol_tick(ctx, 170);
  assert(test_num_mbufs_captured == 1);

  struct rte_mbuf *mbuf = test_mbufs_captured[0];

  struct xran_ecpri_hdr *ecpri;
  struct xran_recv_packet_info info;
  assert(xran_parse_ecpri_hdr(mbuf, &ecpri, &info) == 0);
  assert(info.msg_type == ECPRI_IQ_DATA);

  void *iq_data_start = NULL;
  uint8_t cc_id = 0, ant_id = 0, frame_id = 0, subframe_id = 0, slot_id = 0, symb_id = 0, filter_id = 0;
  union ecpri_seq_id seq;
  uint16_t num_prbu, start_prbu, sym_inc, rb, sect_id;
  uint8_t compMeth = 0, iqWidth = 0;

  int ret = xran_extract_iq_samples(mbuf, &eaxc, &iq_data_start, &cc_id, &ant_id, &frame_id, &subframe_id, &slot_id, &symb_id, &filter_id, &seq, &num_prbu, &start_prbu, &sym_inc, &rb, &sect_id, 0, 0, &compMeth, &iqWidth);
  assert(ret != 0);
  assert(frame_id == 1);
  assert(subframe_id == 2);
  assert(slot_id == 0);
  assert(symb_id == 5);
  assert(start_prbu == 5);
  assert(num_prbu == 20);
  assert(sect_id == 42);

  // Content check: the wire payload (network order) must match txdataF starting at
  // PRB 5, not some other offset -- this is what catches a wrong source pointer.
  const uint16_t *wire = (const uint16_t *)iq_data_start;
  int expected_start_i16 = 5 * 12 * 2;
  for (int i = 0; i < 20 * 12 * 2; i++) {
    int16_t got = (int16_t)rte_be_to_cpu_16(wire[i]);
    assert(got == txdata_i16[expected_start_i16 + i]);
  }

  clear_captured_mbufs();
  free_txdata(txdataF, 1);
  cleanup_du_tx_scheduler(ctx);
}

void test_multi_section() {
  printf("test_multi_section...\n");
  struct xran_eaxcid_config eaxc = { .mask_cuPortId = 0xF000, .mask_bandSectorId = 0x0F00, .mask_ccId = 0x00F0, .mask_ruPortId = 0x000F, .bit_cuPortId = 12, .bit_bandSectorId = 8, .bit_ccId = 4, .bit_ruPortId = 0 };
  void *ctx = init_du_tx_scheduler(0, 106, 200, 300, 100, 400, 100, 500, 1500, FH_COMP_NONE, 16, eaxc, mock_alloc_func, mock_send_func, NULL);
  
  uint32_t **txdataF = allocate_txdata(1, 106);
  du_tx_dl_section_t sections[2] = {
    {.beam_id = 1, .start_prb = 0, .num_prb = 5, .section_id = 10},
    {.beam_id = 2, .start_prb = 10, .num_prb = 5, .section_id = 11}
  };
  
  du_tx_handle_absolute_symbol_tick(ctx, 10);
  du_tx_schedule_dl_iq(ctx, txdataF, 1, 0, 0, 0, 20, sections, 2);
  du_tx_handle_absolute_symbol_tick(ctx, 17); // send_at = 20 - 3 = 17
  
  assert(test_num_mbufs_captured == 2); // Should have generated 2 packets
  
  clear_captured_mbufs();
  free_txdata(txdataF, 1);
  cleanup_du_tx_scheduler(ctx);
}

void test_mtu_fragmentation() {
  printf("test_mtu_fragmentation...\n");
  struct xran_eaxcid_config eaxc = { .mask_cuPortId = 0xF000, .mask_bandSectorId = 0x0F00, .mask_ccId = 0x00F0, .mask_ruPortId = 0x000F, .bit_cuPortId = 12, .bit_bandSectorId = 8, .bit_ccId = 4, .bit_ruPortId = 0 };
  size_t overhead = sizeof(struct rte_ether_hdr) + sizeof(struct xran_ecpri_hdr) + sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr);
  size_t prb_size = 12 * 4; // 48 bytes uncompressed
  size_t max_prb = 10;
  size_t test_mtu = overhead + prb_size * max_prb;
  
  void *ctx = init_du_tx_scheduler(0, 106, 200, 300, 100, 400, 100, 500, test_mtu, FH_COMP_NONE, 16, eaxc, mock_alloc_func, mock_send_func, NULL);
  
  uint32_t **txdataF = allocate_txdata(1, 106);
  // Distinct marker per int16 slot -- catches a source pointer that drifts wrong
  // across fragments (e.g. doubling the offset on the 2nd/3rd fragment).
  int16_t *txdata_i16 = (int16_t *)txdataF[0];
  for (int i = 0; i < 106 * 12 * 2; i++)
    txdata_i16[i] = (int16_t)i;
  du_tx_dl_section_t sections[1] = {{.beam_id = 0, .start_prb = 0, .num_prb = 25, .section_id = 1}};

  du_tx_handle_absolute_symbol_tick(ctx, 10);
  du_tx_schedule_dl_iq(ctx, txdataF, 1, 0, 0, 0, 20, sections, 1);
  du_tx_handle_absolute_symbol_tick(ctx, 17);

  assert(test_num_mbufs_captured == 3); // 25 PRBs -> 10 + 10 + 5

  void *iq_data_start;
  uint8_t cc_id, ant_id, frame_id, subframe_id, slot_id, symb_id, filter_id;
  union ecpri_seq_id seq;
  uint16_t num_prbu, start_prbu, sym_inc, rb, sect_id;
  uint8_t compMeth, iqWidth;

  int expected_start_prb[3] = {0, 10, 20};
  int expected_num_prb[3] = {10, 10, 5};
  for (int f = 0; f < 3; f++) {
    xran_extract_iq_samples(test_mbufs_captured[f], &eaxc, &iq_data_start, &cc_id, &ant_id, &frame_id, &subframe_id, &slot_id, &symb_id, &filter_id, &seq, &num_prbu, &start_prbu, &sym_inc, &rb, &sect_id, 0, 0, &compMeth, &iqWidth);
    assert(start_prbu == expected_start_prb[f]);
    assert(num_prbu == expected_num_prb[f]);

    const uint16_t *wire = (const uint16_t *)iq_data_start;
    int expected_start_i16 = expected_start_prb[f] * 12 * 2;
    for (int i = 0; i < expected_num_prb[f] * 12 * 2; i++) {
      int16_t got = (int16_t)rte_be_to_cpu_16(wire[i]);
      assert(got == txdata_i16[expected_start_i16 + i]);
    }
  }

  clear_captured_mbufs();
  free_txdata(txdataF, 1);
  cleanup_du_tx_scheduler(ctx);
}

void test_late_enqueue() {
  printf("test_late_enqueue...\n");
  struct xran_eaxcid_config eaxc = { .mask_cuPortId = 0xF000, .mask_bandSectorId = 0x0F00, .mask_ccId = 0x00F0, .mask_ruPortId = 0x000F, .bit_cuPortId = 12, .bit_bandSectorId = 8, .bit_ccId = 4, .bit_ruPortId = 0 };
  void *ctx = init_du_tx_scheduler(0, 106, 200, 300, 100, 400, 100, 500, 1500, FH_COMP_NONE, 16, eaxc, mock_alloc_func, mock_send_func, NULL);
  
  uint32_t **txdataF = allocate_txdata(1, 106);
  du_tx_dl_section_t sections[1] = {{.beam_id = 0, .start_prb = 0, .num_prb = 10, .section_id = 1}};
  
  du_tx_handle_absolute_symbol_tick(ctx, 20);
  // target = 20, send_at = 17. Current is 20 -> late!
  du_tx_schedule_dl_iq(ctx, txdataF, 1, 0, 0, 0, 20, sections, 1);
  
  du_tx_scheduler_stats_t stats;
  du_tx_get_stats(ctx, &stats);
  assert(stats.total_dl_scheduled == 0);
  assert(stats.app_too_late_tx == 1);
  
  clear_captured_mbufs();
  free_txdata(txdataF, 1);
  cleanup_du_tx_scheduler(ctx);
}

void test_compression() {
  printf("test_compression...\n");
  struct xran_eaxcid_config eaxc = { .mask_cuPortId = 0xF000, .mask_bandSectorId = 0x0F00, .mask_ccId = 0x00F0, .mask_ruPortId = 0x000F, .bit_cuPortId = 12, .bit_bandSectorId = 8, .bit_ccId = 4, .bit_ruPortId = 0 };
  void *ctx = init_du_tx_scheduler(0, 106, 200, 300, 100, 400, 100, 500, 1500, FH_COMP_BFP, 9, eaxc, mock_alloc_func, mock_send_func, NULL);
  
  uint32_t **txdataF = allocate_txdata(1, 106);
  du_tx_dl_section_t sections[1] = {{.beam_id = 0, .start_prb = 0, .num_prb = 10, .section_id = 1}};
  
  for (int i = 0; i < 10 * 12; i++) {
    ((int16_t*)txdataF[0])[i*2] = 1000;   // I
    ((int16_t*)txdataF[0])[i*2+1] = -1000; // Q
  }
  
  du_tx_handle_absolute_symbol_tick(ctx, 10);
  du_tx_schedule_dl_iq(ctx, txdataF, 1, 0, 0, 0, 20, sections, 1);
  du_tx_handle_absolute_symbol_tick(ctx, 17);
  
  assert(test_num_mbufs_captured == 1);
  
  void *iq_data_start;
  uint8_t cc_id, ant_id, frame_id, subframe_id, slot_id, symb_id, filter_id;
  union ecpri_seq_id seq;
  uint16_t num_prbu, start_prbu, sym_inc, rb, sect_id;
  uint8_t compMeth, iqWidth;
  
  xran_extract_iq_samples(test_mbufs_captured[0], &eaxc, &iq_data_start, &cc_id, &ant_id, &frame_id, &subframe_id, &slot_id, &symb_id, &filter_id, &seq, &num_prbu, &start_prbu, &sym_inc, &rb, &sect_id, 1, 0, &compMeth, &iqWidth);
  
  assert(compMeth == FH_COMP_BFP);
  assert(iqWidth == 9);
  
  int16_t decompressed[10 * 12 * 2];
  fh_decompress_prbs(FH_COMP_BFP, 9, 10, (int8_t*)iq_data_start, decompressed);
  
  // Verify approximate round-trip (BFP is lossy but 1000 should round-trip well within +/- a few tens)
  assert(decompressed[0] > 900 && decompressed[0] < 1100);
  assert(decompressed[1] < -900 && decompressed[1] > -1100);
  
  clear_captured_mbufs();
  free_txdata(txdataF, 1);
  cleanup_du_tx_scheduler(ctx);
}


void test_dl_cplane_auto_scheduling() {
  printf("test_dl_cplane_auto_scheduling...\n");
  struct xran_eaxcid_config eaxc = { .mask_cuPortId = 0xF000, .mask_bandSectorId = 0x0F00, .mask_ccId = 0x00F0, .mask_ruPortId = 0x000F, .bit_cuPortId = 12, .bit_bandSectorId = 8, .bit_ccId = 4, .bit_ruPortId = 0 };
  void *ctx = init_du_tx_scheduler(0, 106, 200, 300, 100, 400, 100, 500, 1500, FH_COMP_NONE, 16, eaxc, mock_alloc_func, mock_send_func, NULL);
  
  uint32_t **txdataF = allocate_txdata(1, 106);
  du_tx_dl_section_t sections[1] = {{.beam_id = 7, .start_prb = 0, .num_prb = 10, .section_id = 1}};
  
  du_tx_handle_absolute_symbol_tick(ctx, 4); 
  
  // ota = 10
  // up_max_sym = 300 / 71 = 4 -> send_at_up = 10 - 3 = 7
  // cp_dl_max_sym = 400 / 71 = 5 -> send_at_cp = 10 - 4 = 6
  du_tx_schedule_dl_iq(ctx, txdataF, 1, 0, 0, 0, 10, sections, 1);
  
  du_tx_handle_absolute_symbol_tick(ctx, 5);
  assert(test_num_mbufs_captured == 0);
  
  du_tx_handle_absolute_symbol_tick(ctx, 6);
  assert(test_num_cp_mbufs_captured == 1); assert(test_num_mbufs_captured == 0); // CP DL sent

  struct xran_ecpri_hdr *ecpri;
  struct xran_recv_packet_info info;
  assert(xran_parse_ecpri_hdr(test_cp_mbufs_captured[0], &ecpri, &info) == 0);
  assert(info.msg_type == ECPRI_RT_CONTROL_DATA);

  // Quick check of contents
  struct radio_app_common_hdr *radio_app = (struct radio_app_common_hdr *)(ecpri + 1);
  assert(radio_app->data_feature.data_direction == XRAN_DIR_DL);
  
  du_tx_handle_absolute_symbol_tick(ctx, 7);
  assert(test_num_mbufs_captured == 1); // U-Plane DL sent
  
  assert(xran_parse_ecpri_hdr(test_mbufs_captured[0], &ecpri, &info) == 0);
  assert(info.msg_type == ECPRI_IQ_DATA);
  
  clear_captured_mbufs();
  free_txdata(txdataF, 1);
  cleanup_du_tx_scheduler(ctx);
}


void test_ul_grant_scheduling() {
  printf("test_ul_grant_scheduling...\n");
  struct xran_eaxcid_config eaxc = { .mask_cuPortId = 0xF000, .mask_bandSectorId = 0x0F00, .mask_ccId = 0x00F0, .mask_ruPortId = 0x000F, .bit_cuPortId = 12, .bit_bandSectorId = 8, .bit_ccId = 4, .bit_ruPortId = 0 };
  void *ctx = init_du_tx_scheduler(0, 106, 200, 300, 100, 400, 100, 500, 1500, FH_COMP_NONE, 16, eaxc, mock_alloc_func, mock_send_func, NULL);
  
  du_tx_dl_section_t sections[1] = {{.beam_id = 9, .start_prb = 0, .num_prb = 20, .section_id = 2}};
  
  du_tx_handle_absolute_symbol_tick(ctx, 3);
  
  // ota = 10
  // cp_ul_max_sym = 500 / 71 = 7 -> send_at_cp = 10 - 6 = 4
  du_tx_schedule_ul_grant(ctx, 0, 0, 0, 10, 0, sections, 1);
  
  du_tx_handle_absolute_symbol_tick(ctx, 4);
  assert(test_num_cp_mbufs_captured == 1);
  assert(test_num_mbufs_captured == 0);
  
  struct xran_ecpri_hdr *ecpri;
  struct xran_recv_packet_info info;
  assert(xran_parse_ecpri_hdr(test_cp_mbufs_captured[0], &ecpri, &info) == 0);
  assert(info.msg_type == ECPRI_RT_CONTROL_DATA);

  struct radio_app_common_hdr *radio_app = (struct radio_app_common_hdr *)(ecpri + 1);
  assert(radio_app->data_feature.data_direction == XRAN_DIR_UL);
  
  du_tx_handle_absolute_symbol_tick(ctx, 10);
  assert(test_num_mbufs_captured == 0); // No UP packet sent
  
  clear_captured_mbufs();
  cleanup_du_tx_scheduler(ctx);
}

void test_dl_cplane_too_late() {
  printf("test_dl_cplane_too_late...\n");
  struct xran_eaxcid_config eaxc = { .mask_cuPortId = 0xF000, .mask_bandSectorId = 0x0F00, .mask_ccId = 0x00F0, .mask_ruPortId = 0x000F, .bit_cuPortId = 12, .bit_bandSectorId = 8, .bit_ccId = 4, .bit_ruPortId = 0 };
  void *ctx = init_du_tx_scheduler(0, 106, 200, 300, 100, 400, 100, 500, 1500, FH_COMP_NONE, 16, eaxc, mock_alloc_func, mock_send_func, NULL);
  
  uint32_t **txdataF = allocate_txdata(1, 106);
  du_tx_dl_section_t sections[1] = {{.beam_id = 7, .start_prb = 0, .num_prb = 10, .section_id = 1}};
  
  // CP DL send at 10 - 4 = 6
  // UP DL send at 10 - 3 = 7
  du_tx_handle_absolute_symbol_tick(ctx, 6); 
  
  du_tx_schedule_dl_iq(ctx, txdataF, 1, 0, 0, 0, 10, sections, 1);
  
  du_tx_scheduler_stats_t stats;
  du_tx_get_stats(ctx, &stats);
  assert(stats.dl_cplane_too_late == 1); // CP was too late (current symbol is 6, CP send was 6)
  assert(stats.total_dl_scheduled == 1); // UP was scheduled normally (send is 7)
  
  du_tx_handle_absolute_symbol_tick(ctx, 7);
  assert(test_num_mbufs_captured == 1); // Only UP sent
  
  struct xran_ecpri_hdr *ecpri;
  struct xran_recv_packet_info info;
  assert(xran_parse_ecpri_hdr(test_mbufs_captured[0], &ecpri, &info) == 0);
  assert(info.msg_type == ECPRI_IQ_DATA);
  
  clear_captured_mbufs();
  free_txdata(txdataF, 1);
  cleanup_du_tx_scheduler(ctx);
}

void test_cplane_dl_wire_fields() {
  printf("test_cplane_dl_wire_fields...\n");
  struct xran_eaxcid_config eaxc = { .mask_cuPortId = 0xF000, .mask_bandSectorId = 0x0F00, .mask_ccId = 0x00F0, .mask_ruPortId = 0x000F, .bit_cuPortId = 12, .bit_bandSectorId = 8, .bit_ccId = 4, .bit_ruPortId = 0 };
  int mu = 1; // slots_per_subframe=2, so a non-trivial slot value exercises the
              // subframe/slotId split (invisible at mu=0, where it's always 0/N).
  void *ctx = init_du_tx_scheduler(mu, 106, 200, 300, 100, 400, 100, 500, 1500, FH_COMP_NONE, 16, eaxc, mock_alloc_func, mock_send_func, NULL);

  uint32_t **txdataF = allocate_txdata(1, 106);
  du_tx_dl_section_t sections[1] = {{.beam_id = 11, .start_prb = 8, .num_prb = 15, .section_id = 99}};

  int frame = 1, slot = 3, symbol = 5; // slot=3 at mu=1 -> subframe=1, slotId=1
  int num_symbols_per_frame = 10 * (1 << mu) * 14; // 280
  uint64_t ota = (uint64_t)frame * num_symbols_per_frame + (uint64_t)slot * 14 + symbol; // 327

  int symbol_duration_uS = 1000 / (1 << mu) / 14; // 35
  int cp_dl_max_sym_diff = 400 / symbol_duration_uS; // 11
  uint64_t send_at_cp = ota - (cp_dl_max_sym_diff - 1); // 317

  du_tx_handle_absolute_symbol_tick(ctx, send_at_cp - 5);
  du_tx_schedule_dl_iq(ctx, txdataF, 1, 0, frame, slot, symbol, sections, 1);
  du_tx_handle_absolute_symbol_tick(ctx, send_at_cp);

  assert(test_num_cp_mbufs_captured == 1);

  struct rte_mbuf *mbuf = test_cp_mbufs_captured[0];
  struct xran_ecpri_hdr *ecpri;
  struct xran_recv_packet_info info;
  assert(xran_parse_ecpri_hdr(mbuf, &ecpri, &info) == 0);
  assert(info.msg_type == ECPRI_RT_CONTROL_DATA);

  // Read into local copies rather than swapping in place, matching the
  // established discipline elsewhere in this codebase for not mutating a
  // packet's bytes while other checks may still need the original layout.
  struct xran_cp_radioapp_section1_header *apphdr = (struct xran_cp_radioapp_section1_header *)(ecpri + 1);
  struct xran_cp_radioapp_common_header hdr_copy = apphdr->cmnhdr;
  hdr_copy.field.all_bits = rte_be_to_cpu_32(apphdr->cmnhdr.field.all_bits);

  assert(hdr_copy.field.dataDirection == XRAN_DIR_DL);
  assert(hdr_copy.field.frameId == frame);
  assert(hdr_copy.field.subframeId == (slot / (1 << mu))); // == 1
  assert(hdr_copy.field.slotId == (slot % (1 << mu)));     // == 1
  assert(hdr_copy.field.startSymbolId == symbol);
  assert(hdr_copy.numOfSections == 1);
  assert(hdr_copy.sectionType == XRAN_CP_SECTIONTYPE_1);

  struct xran_cp_radioapp_section1 *section = (struct xran_cp_radioapp_section1 *)(apphdr + 1);
  struct xran_cp_radioapp_section1 section_copy = *section;
  *((uint64_t *)&section_copy) = rte_be_to_cpu_64(*((uint64_t *)&section_copy));

  assert(section_copy.hdr.u.s1.beamId == 11);
  assert(section_copy.hdr.u.s1.numSymbol == 1);
  assert(section_copy.hdr.u1.common.sectionId == 99);
  assert(section_copy.hdr.u1.common.startPrbc == 8);
  assert(section_copy.hdr.u1.common.numPrbc == 15);

  clear_captured_mbufs();
  free_txdata(txdataF, 1);
  cleanup_du_tx_scheduler(ctx);
}

// Regression test for a use-after-scope bug: du_fhi_south_out() (the real caller, in
// radio/fhi_72_native/du_fhi_isolate.c) passes a stack-local buffer that is only valid for the
// duration of the du_tx_schedule_dl_iq() call -- actual transmission happens later, once
// send_at_symbol's T1a_up-windowed time arrives. The scheduler must copy the IQ samples at
// enqueue time; if it only stores the caller's pointer, the dispatched packet reads back
// whatever now occupies that (by then reused/overwritten) memory instead of the real samples.
void test_source_buffer_reused_after_schedule() {
  printf("test_source_buffer_reused_after_schedule...\n");
  struct xran_eaxcid_config eaxc = { .mask_cuPortId = 0xF000, .mask_bandSectorId = 0x0F00, .mask_ccId = 0x00F0, .mask_ruPortId = 0x000F, .bit_cuPortId = 12, .bit_bandSectorId = 8, .bit_ccId = 4, .bit_ruPortId = 0 };
  void *ctx = init_du_tx_scheduler(0, 106, 200, 300, 100, 400, 100, 500, 1500, FH_COMP_NONE, 16, eaxc, mock_alloc_func, mock_send_func, NULL);

  uint32_t **txdataF = allocate_txdata(1, 106);
  int16_t *txdata_i16 = (int16_t *)txdataF[0];
  int16_t expected[106 * 12 * 2];
  for (int i = 0; i < 106 * 12 * 2; i++) {
    txdata_i16[i] = (int16_t)(i * 7 + 3); // arbitrary non-zero marker pattern
    expected[i] = txdata_i16[i];
  }
  // 20 PRBs (as in test_wire_format) fits in one packet under the default 1500-byte MTU --
  // keeps this test focused on the use-after-scope bug, not MTU fragmentation.
  du_tx_dl_section_t sections[1] = {{.beam_id = 0, .start_prb = 5, .num_prb = 20, .section_id = 1}};

  // Mirrors test_basic_scheduling's timing: T1a_up_max_sym_diff = 300uS / 71uS = 4, so
  // scheduling symbol 10 (ota_absolute_symbol=10) gives send_at_symbol = 10 - (4-1) = 7. The
  // clock must be ticked to *before* 7 first, or the schedule call itself would immediately
  // reject the job as already-too-late (defeating the point of this test).
  du_tx_handle_absolute_symbol_tick(ctx, 5); // Current symbol = 5
  du_tx_schedule_dl_iq(ctx, txdataF, 1, 0, 0, 0, 10, sections, 1);
  // send_at_symbol = 7; dispatch is deferred, not immediate.

  // Simulate the real caller: the source buffer is torn down (or, as in production, simply goes
  // out of scope and gets reused by later stack frames) right after scheduling, well before the
  // deferred dispatch below actually reads it.
  memset(txdataF[0], 0xAA, 106 * 12 * sizeof(uint32_t));
  free_txdata(txdataF, 1);

  du_tx_handle_absolute_symbol_tick(ctx, 7); // reaches send_at_symbol, triggers dispatch
  assert(test_num_mbufs_captured == 1);

  struct rte_mbuf *mbuf = test_mbufs_captured[0];
  struct xran_ecpri_hdr *ecpri;
  struct xran_recv_packet_info info;
  assert(xran_parse_ecpri_hdr(mbuf, &ecpri, &info) == 0);

  void *iq_data_start = NULL;
  uint8_t cc_id = 0, ant_id = 0, frame_id = 0, subframe_id = 0, slot_id = 0, symb_id = 0, filter_id = 0;
  union ecpri_seq_id seq;
  uint16_t num_prbu, start_prbu, sym_inc, rb, sect_id;
  uint8_t compMeth = 0, iqWidth = 0;
  int ret = xran_extract_iq_samples(mbuf, &eaxc, &iq_data_start, &cc_id, &ant_id, &frame_id, &subframe_id, &slot_id, &symb_id, &filter_id, &seq, &num_prbu, &start_prbu, &sym_inc, &rb, &sect_id, 0, 0, &compMeth, &iqWidth);
  assert(ret != 0);

  const uint16_t *wire = (const uint16_t *)iq_data_start;
  int expected_start_i16 = 5 * 12 * 2;
  for (int i = 0; i < 20 * 12 * 2; i++) {
    int16_t got = (int16_t)rte_be_to_cpu_16(wire[i]);
    assert(got == expected[expected_start_i16 + i]);
  }

  clear_captured_mbufs();
  cleanup_du_tx_scheduler(ctx);
}

int main(int argc, char **argv) {
  int ret = rte_eal_init(argc, argv);
  if (ret < 0) rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

  mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS, MBUF_CACHE_SIZE, 0, MBUF_SIZE, rte_socket_id());
  if (mbuf_pool == NULL) rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

  test_source_buffer_reused_after_schedule();
  test_basic_scheduling();
  test_wire_format();
  test_multi_section();
  test_mtu_fragmentation();
  test_late_enqueue();
  test_compression();
  test_dl_cplane_auto_scheduling();
  test_ul_grant_scheduling();
  test_dl_cplane_too_late();
  test_cplane_dl_wire_fields();

  printf("All tests passed.\n");
  return 0;
}
