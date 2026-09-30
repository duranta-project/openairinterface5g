/*
 * SPDX-License-Identifier: Apache-2.0
 * Original file: Copyright 2020 Intel.
 * Copyright 2026 OpenAirInterface Authors
 */

#include "xran_pkt_api.h"

extern struct xran_eaxcid_config *xran_get_conf_eAxC(void *arg);

uint16_t xran_compose_cid(struct xran_eaxcid_config *eaxcid_config,
                          uint8_t CU_Port_ID,
                          uint8_t BandSector_ID,
                          uint8_t CC_ID,
                          uint8_t Ant_ID)
{
  uint16_t cid;
  cid = ((CU_Port_ID << eaxcid_config->bit_cuPortId) & eaxcid_config->mask_cuPortId)
        | ((BandSector_ID << eaxcid_config->bit_bandSectorId) & eaxcid_config->mask_bandSectorId)
        | ((CC_ID << eaxcid_config->bit_ccId) & eaxcid_config->mask_ccId)
        | ((Ant_ID << eaxcid_config->bit_ruPortId) & eaxcid_config->mask_ruPortId);
  return (rte_cpu_to_be_16(cid));
}

void xran_decompose_cid(uint16_t cid,
                        struct xran_eaxcid_config *eaxcid_config,
                        uint8_t *CU_Port_ID,
                        uint8_t *BandSector_ID,
                        uint8_t *CC_ID,
                        uint8_t *Ant_ID)
{
  cid = rte_be_to_cpu_16(cid);
  if (CU_Port_ID)
    *CU_Port_ID = (cid & eaxcid_config->mask_cuPortId) >> eaxcid_config->bit_cuPortId;
  if (BandSector_ID)
    *BandSector_ID = (cid & eaxcid_config->mask_bandSectorId) >> eaxcid_config->bit_bandSectorId;
  if (CC_ID)
    *CC_ID = (cid & eaxcid_config->mask_ccId) >> eaxcid_config->bit_ccId;
  if (Ant_ID)
    *Ant_ID = (cid & eaxcid_config->mask_ruPortId) >> eaxcid_config->bit_ruPortId;
}

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
                                uint8_t *iqWidth)
{
  if (!mbuf || !iq_data_start)
    return 0;

  struct xran_ecpri_hdr *ecpri_hdr = rte_pktmbuf_mtod(mbuf, struct xran_ecpri_hdr *);
  if (!ecpri_hdr)
    return 0;
  *seq_id = ecpri_hdr->ecpri_seq_id;
  xran_decompose_cid(ecpri_hdr->ecpri_xtc_id, conf, NULL, NULL, CC_ID, Ant_ID);

  struct radio_app_common_hdr *radio_hdr = (struct radio_app_common_hdr *)rte_pktmbuf_adj(mbuf, sizeof(*ecpri_hdr));
  if (!radio_hdr)
    return 0;
  radio_hdr->sf_slot_sym.value = rte_be_to_cpu_16(radio_hdr->sf_slot_sym.value);

  *frame_id = radio_hdr->frame_id;
  *subframe_id = radio_hdr->sf_slot_sym.subframe_id;
  *slot_id = radio_hdr->sf_slot_sym.slot_id;
  *symb_id = radio_hdr->sf_slot_sym.symb_id;
  *filter_id = radio_hdr->data_feature.filter_id;

  struct data_section_hdr *data_hdr = (struct data_section_hdr *)rte_pktmbuf_adj(mbuf, sizeof(*radio_hdr));
  if (!data_hdr)
    return 0;
  data_hdr->fields.all_bits = rte_be_to_cpu_32(data_hdr->fields.all_bits);
  *num_prbu = data_hdr->fields.num_prbu;
  *start_prbu = data_hdr->fields.start_prbu;
  *sym_inc = data_hdr->fields.sym_inc;
  *rb = data_hdr->fields.rb;
  *sect_id = data_hdr->fields.sect_id;

  if (expect_comp) {
    struct data_section_compression_hdr *compr_hdr =
        (struct data_section_compression_hdr *)rte_pktmbuf_adj(mbuf, sizeof(*data_hdr));
    if (!compr_hdr)
      return 0;
    *compMeth = compr_hdr->ud_comp_hdr.ud_comp_meth;
    *iqWidth = compr_hdr->ud_comp_hdr.ud_iq_width;
    *iq_data_start = (void *)rte_pktmbuf_adj(mbuf, sizeof(*compr_hdr));
  } else {
    *iq_data_start = (void *)rte_pktmbuf_adj(mbuf, sizeof(*data_hdr));
  }
  if (!*iq_data_start)
    return 0;
  return rte_pktmbuf_pkt_len(mbuf);
}

int xran_parse_ecpri_hdr(struct rte_mbuf *mbuf, struct xran_ecpri_hdr **ecpri_hdr, struct xran_recv_packet_info *pkt_info)
{
  *ecpri_hdr = rte_pktmbuf_mtod(mbuf, struct xran_ecpri_hdr *);
  if (*ecpri_hdr == NULL)
    return -1;

  pkt_info->ecpri_version = (*ecpri_hdr)->cmnhdr.bits.ecpri_ver;
  pkt_info->msg_type = (enum ecpri_msg_type)(*ecpri_hdr)->cmnhdr.bits.ecpri_mesg_type;
  pkt_info->payload_len = rte_be_to_cpu_16((*ecpri_hdr)->cmnhdr.bits.ecpri_payl_size);
  pkt_info->seq_id = (*ecpri_hdr)->ecpri_seq_id.bits.seq_id;
  pkt_info->subseq_id = (*ecpri_hdr)->ecpri_seq_id.bits.sub_seq_id;
  pkt_info->ebit = (*ecpri_hdr)->ecpri_seq_id.bits.e_bit;
  return 0;
}
