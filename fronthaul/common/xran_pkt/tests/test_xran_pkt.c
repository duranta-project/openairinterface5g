/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_byteorder.h>
#include "xran_pkt_api.h"

/* Global config for tests */
struct xran_eaxcid_config g_eaxcid_config = {.mask_cuPortId = 0xF000,
                                             .mask_bandSectorId = 0x0F00,
                                             .mask_ccId = 0x00F0,
                                             .mask_ruPortId = 0x000F,
                                             .bit_cuPortId = 12,
                                             .bit_bandSectorId = 8,
                                             .bit_ccId = 4,
                                             .bit_ruPortId = 0};

void test_cid_compose_decompose()
{
  printf("Testing CID compose/decompose...\n");
  uint8_t cu_port = 0xA;
  uint8_t band_sector = 0xB;
  uint8_t cc_id = 0xC;
  uint8_t ant_id = 0x1;

  uint16_t cid = xran_compose_cid(&g_eaxcid_config, cu_port, band_sector, cc_id, ant_id);

  uint8_t res_cu, res_bs, res_cc, res_ant;
  xran_decompose_cid(cid, &g_eaxcid_config, &res_cu, &res_bs, &res_cc, &res_ant);

  assert(res_cu == cu_port);
  assert(res_bs == band_sector);
  assert(res_cc == cc_id);
  assert(res_ant == ant_id);
  printf("CID compose/decompose passed!\n");
}

void test_parse_ecpri_hdr()
{
  printf("Testing eCPRI header parsing...\n");
  struct rte_mempool *mp = rte_pktmbuf_pool_create("test_pool", 1024, 0, 0, 2048, rte_socket_id());
  assert(mp != NULL);
  struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mp);
  assert(mbuf != NULL);

  struct xran_ecpri_hdr *hdr = (struct xran_ecpri_hdr *)rte_pktmbuf_append(mbuf, sizeof(struct xran_ecpri_hdr));
  hdr->cmnhdr.bits.ecpri_ver = 1;
  hdr->cmnhdr.bits.ecpri_mesg_type = ECPRI_IQ_DATA;
  hdr->cmnhdr.bits.ecpri_payl_size = rte_cpu_to_be_16(100);
  hdr->ecpri_seq_id.bits.seq_id = 0xAA;
  hdr->ecpri_seq_id.bits.sub_seq_id = 0x55;
  hdr->ecpri_seq_id.bits.e_bit = 1;

  struct xran_ecpri_hdr *parsed_hdr;
  struct xran_recv_packet_info info;
  int ret = xran_parse_ecpri_hdr(mbuf, &parsed_hdr, &info);
  assert(ret == 0);
  assert(info.ecpri_version == 1);
  assert(info.msg_type == ECPRI_IQ_DATA);
  assert(info.payload_len == 100);
  assert(info.seq_id == 0xAA);
  assert(info.subseq_id == 0x55);
  assert(info.ebit == 1);

  rte_pktmbuf_free(mbuf);
  rte_mempool_free(mp);
  printf("eCPRI header parsing passed!\n");
}

void test_extract_iq_samples()
{
  printf("Testing IQ sample extraction...\n");
  struct rte_mempool *mp = rte_pktmbuf_pool_create("test_pool_iq", 1024, 0, 0, 2048, rte_socket_id());
  assert(mp != NULL);
  struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mp);
  assert(mbuf != NULL);

  // Prepare packet: eCPRI Hdr + Radio App Hdr + Data Section Hdr + IQ Data
  struct xran_ecpri_hdr *ecpri = (struct xran_ecpri_hdr *)rte_pktmbuf_append(mbuf, sizeof(struct xran_ecpri_hdr));
  ecpri->ecpri_xtc_id = xran_compose_cid(&g_eaxcid_config, 1, 2, 3, 4);
  ecpri->ecpri_seq_id.bits.seq_id = 0xDE;

  struct radio_app_common_hdr *app = (struct radio_app_common_hdr *)rte_pktmbuf_append(mbuf, sizeof(struct radio_app_common_hdr));
  app->frame_id = 10;
  app->sf_slot_sym.subframe_id = 5;
  app->sf_slot_sym.slot_id = 2;
  app->sf_slot_sym.symb_id = 7;
  app->sf_slot_sym.value = rte_cpu_to_be_16(app->sf_slot_sym.value);

  struct data_section_hdr *data = (struct data_section_hdr *)rte_pktmbuf_append(mbuf, sizeof(struct data_section_hdr));
  data->fields.num_prbu = 10;
  data->fields.start_prbu = 0;
  data->fields.sect_id = 100;
  data->fields.all_bits = rte_cpu_to_be_32(data->fields.all_bits);

  uint8_t *iq_ptr = (uint8_t *)rte_pktmbuf_append(mbuf, 16);
  memset(iq_ptr, 0x55, 16);

  void *out_iq;
  uint8_t cc_id = 0xFF, ant_id = 0xFF, frame, sf, slot, sym, filter;
  union ecpri_seq_id seq;
  uint16_t num_prb, start_prb, sym_inc, rb, sect_id;
  uint8_t meth, width;

  int32_t ret = xran_extract_iq_samples(mbuf,
                                        &g_eaxcid_config,
                                        &out_iq,
                                        &cc_id,
                                        &ant_id,
                                        &frame,
                                        &sf,
                                        &slot,
                                        &sym,
                                        &filter,
                                        &seq,
                                        &num_prb,
                                        &start_prb,
                                        &sym_inc,
                                        &rb,
                                        &sect_id,
                                        0,
                                        0,
                                        &meth,
                                        &width);

  assert(ret > 0);
  assert(cc_id == 3);
  assert(ant_id == 4);
  assert(frame == 10);
  assert(sf == 5);
  assert(slot == 2);
  assert(sym == 7);
  assert(num_prb == 10);
  assert(sect_id == 100);
  assert(out_iq == iq_ptr);

  rte_pktmbuf_free(mbuf);
  rte_mempool_free(mp);
  printf("IQ sample extraction passed!\n");
}

int main(int argc, char *argv[])
{
  int ret = rte_eal_init(argc, argv);
  if (ret < 0)
    rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

  test_cid_compose_decompose();
  test_parse_ecpri_hdr();
  test_extract_iq_samples();

  printf("All tests passed!\n");
  return 0;
}
