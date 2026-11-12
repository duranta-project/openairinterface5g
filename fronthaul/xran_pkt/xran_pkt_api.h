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
#include <string.h>
#include <rte_byteorder.h>

/* Convert, in place, the 64-bit section header word at p between big endian and CPU order. Packet section headers are not
 * 8-byte aligned in the mbuf, so go through memcpy instead of dereferencing a uint64_t pointer. */
static inline void xran_swap_be64_inplace(void *p)
{
  uint64_t w;
  memcpy(&w, p, sizeof(w));
  w = rte_be_to_cpu_64(w);
  memcpy(p, &w, sizeof(w));
}

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
