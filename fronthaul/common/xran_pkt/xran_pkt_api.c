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

void fill_ecpri_header(struct xran_ecpri_hdr *ecpri_header,
                       struct xran_eaxcid_config *eaxcid_config,
                       uint8_t ecpri_mesg_type,
                       size_t ecpri_payload_size,
                       uint8_t CC_ID,
                       uint8_t Ant_ID,
                       uint8_t seq_id,
                       uint8_t oxu_port_id)
{
  ecpri_header->cmnhdr.data.data_num_1 = 0x0;
  ecpri_header->cmnhdr.bits.ecpri_ver = XRAN_ECPRI_VER;
  ecpri_header->cmnhdr.bits.ecpri_mesg_type = ecpri_mesg_type;
  ecpri_header->cmnhdr.bits.ecpri_payl_size = rte_cpu_to_be_16(ecpri_payload_size);
  ecpri_header->ecpri_xtc_id = xran_compose_cid(eaxcid_config, 0, 0, CC_ID, Ant_ID);
  ecpri_header->ecpri_seq_id.bits.seq_id = seq_id;
  ecpri_header->ecpri_seq_id.bits.e_bit = 1;
  ecpri_header->ecpri_seq_id.bits.sub_seq_id = 0;
}

void fill_radio_app_header(struct radio_app_common_hdr *radio_app_header,
                           int filter_id,
                           int direction,
                           int frame,
                           int slot,
                           int symbol,
                           int mu)
{
  radio_app_header->frame_id = frame & 0xff;
  radio_app_header->sf_slot_sym.slot_id = slot % (1 << mu);
  radio_app_header->sf_slot_sym.subframe_id = slot / (1 << mu);
  radio_app_header->sf_slot_sym.symb_id = symbol;
  radio_app_header->sf_slot_sym.value = rte_cpu_to_be_16(radio_app_header->sf_slot_sym.value);
  radio_app_header->data_feature.data_direction = direction;
  radio_app_header->data_feature.payl_ver = 1;
  radio_app_header->data_feature.filter_id = filter_id;
}

void fill_data_section_header(struct data_section_hdr *data_section_hdr, int num_prb, int start_prb, int section_id)
{
  data_section_hdr->fields.all_bits = 0;
  data_section_hdr->fields.num_prbu = (uint8_t)XRAN_CONVERT_NUMPRBC(num_prb);
  data_section_hdr->fields.start_prbu = (start_prb & 0x03ff);
  data_section_hdr->fields.sect_id = section_id;
  data_section_hdr->fields.all_bits = rte_cpu_to_be_32(data_section_hdr->fields.all_bits);
}

void fill_cplane_section1(struct rte_mbuf *mbuf, struct xran_eaxcid_config *eaxcid_config,
                          uint8_t direction, uint8_t frame, uint8_t subframe, uint8_t slot, uint8_t start_symbol,
                          uint8_t filter_index, struct xran_radioapp_udComp_header udComp,
                          uint8_t cc_id, uint8_t ant_id, uint8_t seq_id,
                          uint16_t section_id, uint16_t beam_id, uint8_t num_symbol,
                          uint16_t start_prbc, uint8_t num_prbc, uint16_t reMask, uint8_t rb, uint8_t symInc)
{
  size_t app_hdr_len = sizeof(struct xran_cp_radioapp_section1_header);
  size_t section_len = sizeof(struct xran_cp_radioapp_section1);
  size_t total_len = sizeof(struct xran_ecpri_hdr) + app_hdr_len + section_len;
  char *buf = rte_pktmbuf_append(mbuf, (uint16_t)total_len);
  if (!buf) return;

  struct xran_ecpri_hdr *ecpri_hdr = (struct xran_ecpri_hdr *)buf;
  fill_ecpri_header(ecpri_hdr, eaxcid_config, ECPRI_RT_CONTROL_DATA, app_hdr_len + section_len, cc_id, ant_id, seq_id, 0);

  struct xran_cp_radioapp_section1_header *apphdr = (struct xran_cp_radioapp_section1_header *)(ecpri_hdr + 1);
  apphdr->cmnhdr.field.all_bits = 0;
  apphdr->cmnhdr.field.dataDirection = direction;
  apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;
  apphdr->cmnhdr.field.filterIndex = filter_index;
  apphdr->cmnhdr.field.frameId = frame;
  apphdr->cmnhdr.field.subframeId = subframe;
  apphdr->cmnhdr.field.slotId = slot;
  apphdr->cmnhdr.field.startSymbolId = start_symbol;
  apphdr->cmnhdr.numOfSections = 1;
  apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_1;
  apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);
  apphdr->udComp = udComp;
  apphdr->reserved = 0;

  struct xran_cp_radioapp_section1 *section = (struct xran_cp_radioapp_section1 *)(apphdr + 1);
  section->hdr.u.first_4byte = 0;
  section->hdr.u.s1.beamId = beam_id;
  section->hdr.u.s1.ef = 0;
  section->hdr.u.s1.numSymbol = num_symbol;
  section->hdr.u.s1.reMask = reMask;

  section->hdr.u1.second_4byte = 0;
  section->hdr.u1.common.numPrbc = num_prbc;
  section->hdr.u1.common.startPrbc = start_prbc;
  section->hdr.u1.common.symInc = symInc;
  section->hdr.u1.common.rb = rb;
  section->hdr.u1.common.sectionId = section_id;

  *((uint64_t *)section) = rte_cpu_to_be_64(*((uint64_t *)section));
}

void fill_cplane_section3(struct rte_mbuf *mbuf, struct xran_eaxcid_config *eaxcid_config,
                          uint8_t frame, uint8_t subframe, uint8_t slot, uint8_t start_symbol,
                          uint8_t filter_index, uint16_t time_offset, uint8_t frame_structure_uscs, uint8_t frame_structure_fftsize,
                          uint16_t cp_length, struct xran_radioapp_udComp_header udComp,
                          uint8_t cc_id, uint8_t ant_id, uint8_t seq_id,
                          uint16_t section_id, uint16_t beam_id, uint8_t num_symbol,
                          uint16_t start_prbc, uint8_t num_prbc, uint32_t freq_offset)
{
  size_t app_hdr_len = sizeof(struct xran_cp_radioapp_section3_header);
  size_t section_len = sizeof(struct xran_cp_radioapp_section3);
  size_t total_len = sizeof(struct xran_ecpri_hdr) + app_hdr_len + section_len;
  char *buf = rte_pktmbuf_append(mbuf, (uint16_t)total_len);
  if (!buf) return;

  struct xran_ecpri_hdr *ecpri_hdr = (struct xran_ecpri_hdr *)buf;
  fill_ecpri_header(ecpri_hdr, eaxcid_config, ECPRI_RT_CONTROL_DATA, app_hdr_len + section_len, cc_id, ant_id, seq_id, 0);

  struct xran_cp_radioapp_section3_header *apphdr = (struct xran_cp_radioapp_section3_header *)(ecpri_hdr + 1);
  apphdr->cmnhdr.field.all_bits = 0;
  apphdr->cmnhdr.field.dataDirection = XRAN_DIR_UL;
  apphdr->cmnhdr.field.payloadVer = XRAN_PAYLOAD_VER;
  apphdr->cmnhdr.field.filterIndex = filter_index;
  apphdr->cmnhdr.field.frameId = frame;
  apphdr->cmnhdr.field.subframeId = subframe;
  apphdr->cmnhdr.field.slotId = slot;
  apphdr->cmnhdr.field.startSymbolId = start_symbol;
  apphdr->cmnhdr.numOfSections = 1;
  apphdr->cmnhdr.sectionType = XRAN_CP_SECTIONTYPE_3;
  apphdr->cmnhdr.field.all_bits = rte_cpu_to_be_32(apphdr->cmnhdr.field.all_bits);

  apphdr->timeOffset = rte_cpu_to_be_16(time_offset);
  apphdr->frameStructure.uScs = frame_structure_uscs;
  apphdr->frameStructure.fftSize = frame_structure_fftsize;
  apphdr->cpLength = rte_cpu_to_be_16(cp_length);
  apphdr->udComp = udComp;

  struct xran_cp_radioapp_section3 *section = (struct xran_cp_radioapp_section3 *)(apphdr + 1);
  section->hdr.u.first_4byte = 0;
  section->hdr.u.s3.beamId = beam_id;
  section->hdr.u.s3.ef = 0;
  section->hdr.u.s3.numSymbol = num_symbol;
  section->hdr.u.s3.reMask = 0xfff;

  section->hdr.u1.second_4byte = 0;
  section->hdr.u1.common.numPrbc = num_prbc;
  section->hdr.u1.common.startPrbc = start_prbc;
  section->hdr.u1.common.symInc = 0;
  section->hdr.u1.common.rb = 0;
  section->hdr.u1.common.sectionId = section_id;

  uint32_t freq_word = (freq_offset & 0xFFFFFF);
  *((uint32_t *)((uint8_t *)section + 8)) = rte_cpu_to_be_32(freq_word);

  *((uint64_t *)section) = rte_cpu_to_be_64(*((uint64_t *)section));
}
