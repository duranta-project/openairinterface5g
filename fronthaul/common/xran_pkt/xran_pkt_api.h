/*
 * SPDX-License-Identifier: Apache-2.0
 * Original file: Copyright 2020 Intel.
 * Copyright 2026 OpenAirInterface Authors
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpacked-not-aligned"
#include "xran_pkt_up.h"
#include "xran_pkt_cp.h"
#include "xran_pkt.h"
#pragma GCC diagnostic pop

struct xran_eaxcid_config {
  uint16_t mask_cuPortId;
  uint16_t mask_bandSectorId;
  uint16_t mask_ccId;
  uint16_t mask_ruPortId;
  uint8_t bit_cuPortId;
  uint8_t bit_bandSectorId;
  uint8_t bit_ccId;
  uint8_t bit_ruPortId;
};

struct xran_recv_packet_info {
  int ecpri_version;
  enum ecpri_msg_type msg_type;
  int payload_len;
  int seq_id;
  int subseq_id;
  int ebit;
};

uint16_t xran_compose_cid(struct xran_eaxcid_config *eaxcid_config,
                          uint8_t CU_Port_ID,
                          uint8_t BandSector_ID,
                          uint8_t CC_ID,
                          uint8_t Ant_ID);

void xran_decompose_cid(uint16_t cid,
                        struct xran_eaxcid_config *eaxcid_config,
                        uint8_t *CU_Port_ID,
                        uint8_t *BandSector_ID,
                        uint8_t *CC_ID,
                        uint8_t *Ant_ID);

int32_t xran_extract_iq_samples(struct rte_mbuf *mbuf,
                                struct xran_eaxcid_config *conf,
                                void **iq_data_start,
                                uint8_t *CC_ID,
                                uint8_t *Ant_ID,
                                uint8_t *frame_id,
                                uint8_t *subframe_id,
                                uint8_t *slot_id,
                                uint8_t *symb_id,
                                uint8_t *filter_id,
                                union ecpri_seq_id *seq_id,
                                uint16_t *num_prbu,
                                uint16_t *start_prbu,
                                uint16_t *sym_inc,
                                uint16_t *rb,
                                uint16_t *sect_id,
                                int8_t expect_comp,
                                uint8_t staticComp,
                                uint8_t *compMeth,
                                uint8_t *iqWidth);

int xran_parse_ecpri_hdr(struct rte_mbuf *mbuf, struct xran_ecpri_hdr **ecpri_hdr, struct xran_recv_packet_info *pkt_info);

void fill_ecpri_header(struct xran_ecpri_hdr *ecpri_header,
                       struct xran_eaxcid_config *eaxcid_config,
                       uint8_t ecpri_mesg_type,
                       size_t ecpri_payload_size,
                       uint8_t CC_ID,
                       uint8_t Ant_ID,
                       uint8_t seq_id,
                       uint8_t oxu_port_id);

void fill_radio_app_header(struct radio_app_common_hdr *radio_app_header,
                           int filter_id,
                           int direction,
                           int frame,
                           int slot,
                           int symbol,
                           int mu);

void fill_data_section_header(struct data_section_hdr *data_section_hdr, int num_prb, int start_prb, int section_id);

void fill_cplane_section1(struct rte_mbuf *mbuf, struct xran_eaxcid_config *eaxcid_config,
                          uint8_t direction, uint8_t frame, uint8_t subframe, uint8_t slot, uint8_t start_symbol,
                          uint8_t filter_index, struct xran_radioapp_udComp_header udComp,
                          uint8_t cc_id, uint8_t ant_id, uint8_t seq_id,
                          uint16_t section_id, uint16_t beam_id, uint8_t num_symbol,
                          uint16_t start_prbc, uint8_t num_prbc, uint16_t reMask, uint8_t rb, uint8_t symInc);

void fill_cplane_section3(struct rte_mbuf *mbuf, struct xran_eaxcid_config *eaxcid_config,
                          uint8_t frame, uint8_t subframe, uint8_t slot, uint8_t start_symbol,
                          uint8_t filter_index, uint16_t time_offset, uint8_t frame_structure_uscs, uint8_t frame_structure_fftsize,
                          uint16_t cp_length, struct xran_radioapp_udComp_header udComp,
                          uint8_t cc_id, uint8_t ant_id, uint8_t seq_id,
                          uint16_t section_id, uint16_t beam_id, uint8_t num_symbol,
                          uint16_t start_prbc, uint8_t num_prbc, uint32_t freq_offset);
