/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#include "PHY/defs_gNB.h"
#include "sched_nr.h"
#include "PHY/NR_TRANSPORT/nr_transport_proto.h"
#include "PHY/NR_TRANSPORT/nr_dlsch.h"
#include "PHY/NR_TRANSPORT/nr_ulsch.h"
#include "PHY/NR_TRANSPORT/nr_dci.h"
#include "PHY/NR_ESTIMATION/nr_ul_estimation.h"
#include "nfapi/open-nFAPI/nfapi/public_inc/nfapi_interface.h"
#include "common/utils/LOG/log.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "PHY/INIT/nr_phy_init.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "T.h"
#include "executables/nr-softmodem.h"
#include "executables/softmodem-common.h"
#include "nfapi/oai_integration/vendor_ext.h"
#include "assertions.h"
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include <openair1/PHY/TOOLS/phy_scope_interface.h>
#include "PHY/log_tools.h"

//#define DEBUG_RXDATA
//#define SRS_IND_DEBUG
static void nr_fill_indication(PHY_VARS_gNB *gNB,
                               int frame,
                               int slot_rx,
                               int UE_id,
                               uint8_t harq_pid,
                               uint8_t crc_flag,
                               int dtx_flag,
                               nfapi_nr_crc_t *crc,
                               nfapi_nr_rx_data_pdu_t *pdu);

static void nr_fill_indication(PHY_VARS_gNB *gNB,
                               int frame,
                               int slot_rx,
                               int ULSCH_id,
                               uint8_t harq_pid,
                               uint8_t crc_flag,
                               int dtx_flag,
                               nfapi_nr_crc_t *crc,
                               nfapi_nr_rx_data_pdu_t *pdu);

int beam_index_allocation(bool das,
                          int fapi_beam_index,
                          NR_gNB_COMMON *common_vars,
                          int slot,
                          int symbols_per_slot,
                          int bitmap_symbols)
{
  if (!common_vars->beam_id)
    return 0;
  if (das)
    return fapi_beam_index;

  AssertFatal(IS_BIT_SET(fapi_beam_index, 15), "Can't handle preconfigured DBM yet\n");
  int ru_beam_idx = fapi_beam_index & 0x7fff;
  int idx = -1;
  for (int j = 0; j < common_vars->num_beams_period; j++) {
    // L2 analog beam implementation is slot based, so we need to verify occupancy for the whole slot
    for (int i = 0; i < symbols_per_slot; i++) {
      int current_beam = common_vars->beam_id[j][slot * symbols_per_slot + i];
      if (current_beam == -1 || current_beam == ru_beam_idx)
        idx = j;
      else {
        idx = -1;
        break;
      }
    }
    if (idx != -1)
      break;
  }
  AssertFatal(idx >= 0, "Couldn't allocate beam ID %d\n", ru_beam_idx);
  for (int j = 0; j < symbols_per_slot; j++) {
    if (((bitmap_symbols >> j) & 0x01))
      common_vars->beam_id[idx][slot * symbols_per_slot + j] = ru_beam_idx;
  }
  LOG_D(PHY, "Allocating beam_id[%d] %d in slot %d\n", idx, ru_beam_idx, slot);
  return idx;
}

void nr_common_signal_procedures(PHY_VARS_gNB *gNB, int frame, int slot, const nfapi_nr_dl_tti_ssb_pdu *ssb_pdu)
{
  NR_DL_FRAME_PARMS *fp = &gNB->frame_parms;
  const nfapi_nr_dl_tti_ssb_pdu_rel15_t *pdu = &ssb_pdu->ssb_pdu_rel15;
  uint8_t ssb_index = pdu->SsbBlockIndex;
  LOG_D(PHY,"common_signal_procedures: frame %d, slot %d ssb index %d\n", frame, slot, ssb_index);

  int ssb_start_symbol_abs = nr_get_ssb_start_symbol(fp, ssb_index); // computing the starting symbol for current ssb
  uint16_t ssb_start_symbol = ssb_start_symbol_abs % fp->symbols_per_slot;  // start symbol wrt slot

  // Setting the first subcarrier
  // 3GPP TS 38.211 sections 7.4.3.1 and 4.4.4.2
  // for FR1 offsetToPointA and k_SSB are expressed in terms of 15 kHz SCS
  // for FR2 offsetToPointA is expressed in terms of 60 kHz SCS and k_SSB expressed in terms of the subcarrier spacing provided
  // by the higher-layer parameter subCarrierSpacingCommon
  nfapi_nr_config_request_scf_t *cfg = &gNB->gNB_config;
  const int scs = cfg->ssb_config.scs_common.value;
  const int prb_offset = (fp->freq_range == FR1) ? pdu->ssbOffsetPointA >> scs : pdu->ssbOffsetPointA >> (scs - 2);
  const int sc_offset = (fp->freq_range == FR1) ? pdu->SsbSubcarrierOffset >> scs : pdu->SsbSubcarrierOffset;
  fp->ssb_start_subcarrier = (12 * prb_offset + sc_offset);

  if (fp->print_ue_help_cmdline_log && IS_SA_MODE(get_softmodem_params())) {
    fp->print_ue_help_cmdline_log = false;
    if (fp->dl_CarrierFreq != fp->ul_CarrierFreq)
      LOG_A(PHY,
            "Command line parameters for OAI UE: -C %lu --CO %ld -r %d --numerology %d --ssb %d %s\n",
            fp->dl_CarrierFreq,
            fp->ul_CarrierFreq - fp->dl_CarrierFreq,
            fp->N_RB_DL,
            scs,
            fp->ssb_start_subcarrier,
            fp->threequarter_fs ? "-E" : "");
    else
      LOG_A(PHY,
            "Command line parameters for OAI UE: -C %lu -r %d --numerology %d --ssb %d %s\n",
            fp->dl_CarrierFreq,
            fp->N_RB_DL,
            scs,
            fp->ssb_start_subcarrier,
            fp->threequarter_fs ? "-E" : "");
  }
  LOG_D(PHY,
        "ssbOffsetPointA %d SSB SsbSubcarrierOffset %d  prb_offset %d sc_offset %d scs %d ssb_start_subcarrier %d\n",
        pdu->ssbOffsetPointA,
        pdu->SsbSubcarrierOffset,
        prb_offset,
        sc_offset,
        scs,
        fp->ssb_start_subcarrier);

  LOG_D(PHY,"SS TX: frame %d, slot %d, start_symbol %d\n", frame, slot, ssb_start_symbol);
  const nfapi_nr_tx_precoding_and_beamforming_t *pb = &pdu->precoding_and_beamforming;
  c16_t ***txdataF = gNB->common_vars.txdataF;
  int txdataF_offset = slot * fp->samples_per_slot_wCP;
  // beam number in a scenario with multiple concurrent beams
  int bitmap = SL_to_bitmap(ssb_start_symbol, 4); // 4 ssb symbols
  int beam_nb = beam_index_allocation(gNB->enable_analog_das,
                                      pb->prgs_list[0].dig_bf_interface_list[0].beam_idx,
                                      &gNB->common_vars,
                                      slot,
                                      fp->symbols_per_slot,
                                      bitmap);

  nr_generate_pss(&txdataF[beam_nb][0][txdataF_offset], gNB->TX_AMP, ssb_start_symbol, cfg, fp);
  nr_generate_sss(&txdataF[beam_nb][0][txdataF_offset], gNB->TX_AMP, ssb_start_symbol, cfg, fp);

  uint16_t slots_per_hf = (fp->slots_per_frame) >> 1;
  int n_hf = slot < slots_per_hf ? 0 : 1;

  int hf = fp->Lmax == 4 ? n_hf : 0;
  nr_generate_pbch_dmrs(nr_gold_pbch(fp->Lmax, gNB->gNB_config.cell_config.phy_cell_id.value, hf, ssb_index & 7),
                        &txdataF[beam_nb][0][txdataF_offset],
                        gNB->TX_AMP,
                        ssb_start_symbol,
                        cfg,
                        fp);

#if T_TRACER
  if (T_ACTIVE(T_GNB_PHY_MIB)) {
    unsigned char bch[3];
    bch[0] = pdu->bchPayload & 0xff;
    bch[1] = (pdu->bchPayload >> 8) & 0xff;
    bch[2] = (pdu->bchPayload >> 16) & 0xff;
    T(T_GNB_PHY_MIB, T_INT(0) /* module ID */, T_INT(frame), T_INT(slot), T_BUFFER(bch, 3));
  }
#endif

  nr_generate_pbch(gNB,
                   ssb_pdu,
                   &txdataF[beam_nb][0][txdataF_offset],
                   ssb_start_symbol,
                   n_hf,
                   frame,
                   cfg,
                   fp);
}

// clearing beam information to be provided to RU for all slots (DL and UL)
void clear_slot_beamid(PHY_VARS_gNB *gNB, int slot)
{
  LOG_D(PHY, "Clearing beam_id structure for slot %d\n", slot);
  int slot_sz = gNB->frame_parms.symbols_per_slot;
  for (int i = 0; i < gNB->common_vars.num_beams_period; i++) {
    if (gNB->common_vars.beam_id)
      memset(&gNB->common_vars.beam_id[i][slot * slot_sz], -1, slot_sz * sizeof(**gNB->common_vars.beam_id));
  }
}

static void nr_generate_csi_rs_gNB(PHY_VARS_gNB *gNB,
                                   int slot,
                                   const nfapi_nr_config_request_scf_t *cfg,
                                   const nfapi_nr_dl_tti_csi_rs_pdu *csi_rs_pdu)
{
  const nfapi_nr_dl_tti_csi_rs_pdu_rel15_t *csi_params = &csi_rs_pdu->csi_rs_pdu_rel15;
  if (csi_params->csi_type == 2) // ZP-CSI
    return;

  csi_mapping_parms_t mapping_parms =
      get_csi_mapping_parms(csi_params->row, csi_params->freq_domain, csi_params->symb_l0, csi_params->symb_l1);
  const nfapi_nr_tx_precoding_and_beamforming_t *pb = &csi_params->precodingAndBeamforming;
  int csi_bitmap = 0;
  int lprime_num = mapping_parms.lprime + 1;
  for (int j = 0; j < mapping_parms.size; j++)
    csi_bitmap |= ((1 << lprime_num) - 1) << mapping_parms.loverline[j];
  int beam_nb = beam_index_allocation(gNB->enable_analog_das,
                                      pb->prgs_list[0].dig_bf_interface_list[0].beam_idx,
                                      &gNB->common_vars,
                                      slot,
                                      gNB->frame_parms.symbols_per_slot,
                                      csi_bitmap);

  nr_generate_csi_rs(&gNB->frame_parms,
                     &mapping_parms,
                     gNB->TX_AMP,
                     slot,
                     csi_params->freq_density,
                     csi_params->start_rb,
                     csi_params->nr_of_rbs,
                     csi_params->symb_l0,
                     csi_params->symb_l1,
                     csi_params->row,
                     csi_params->scramb_id,
                     csi_params->power_control_offset_ss,
                     csi_params->cdm_type,
                     gNB->common_vars.txdataF[beam_nb]);
}

void phy_procedures_gNB_TX(PHY_VARS_gNB *gNB,
                           const nfapi_nr_dl_tti_request_t *DL_req,
                           const nfapi_nr_tx_data_request_t *TX_req,
                           const nfapi_nr_ul_dci_request_t *UL_dci_req,
                           int frame,
                           int slot,
                           int do_meas)
{
  const NR_DL_FRAME_PARMS *fp = &gNB->frame_parms;
  nfapi_nr_config_request_scf_t *cfg = &gNB->gNB_config;
  const int txdataF_offset = slot * fp->samples_per_slot_wCP;

  if ((cfg->cell_config.frame_duplex_type.value == TDD) && (nr_slot_select(cfg,frame,slot) == NR_UPLINK_SLOT))
    return;

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_gNB_TX + gNB->CC_id, 1);

  // clear the transmit data array and beam index for the current slot
  for (int i = 0; i < gNB->common_vars.num_beams_period; i++) {
    for (int aa = 0; aa < cfg->carrier_config.num_tx_ant.value; aa++) {
      memset(&gNB->common_vars.txdataF[i][aa][txdataF_offset], 0, fp->samples_per_slot_wCP * sizeof(***gNB->common_vars.txdataF));
    }
  }

  // Check for PRS slot - section 7.4.1.7.4 in 3GPP rel16 38.211
  for(int rsc_id = 0; rsc_id < gNB->prs_vars.NumPRSResources; rsc_id++)
  {
    prs_config_t *prs_config = &gNB->prs_vars.prs_cfg[rsc_id];
    for (int i = 0; i < prs_config->PRSResourceRepetition; i++)
    {
      if( (((frame*fp->slots_per_frame + slot) - (prs_config->PRSResourceSetPeriod[1] + prs_config->PRSResourceOffset)+prs_config->PRSResourceSetPeriod[0])%prs_config->PRSResourceSetPeriod[0]) == i*prs_config->PRSResourceTimeGap )
      {
        int slot_prs = (slot - i * prs_config->PRSResourceTimeGap + fp->slots_per_frame) % fp->slots_per_frame;
        LOG_D(PHY,"gNB_TX: frame %d, slot %d, slot_prs %d, PRS Resource ID %d\n",frame, slot, slot_prs, rsc_id);
        nr_generate_prs(slot_prs, &gNB->common_vars.txdataF[0][0][txdataF_offset], AMP, prs_config, cfg, fp);
      }
    }
  }

  for (int i = 0; i < UL_dci_req->numPdus; ++i)
    nr_generate_dci(gNB, &UL_dci_req->ul_dci_pdu_list[i].pdcch_pdu.pdcch_pdu_rel15, txdataF_offset, &gNB->frame_parms, slot);

  int num_pdsch = 0;
  for (int i = 0; i < DL_req->dl_tti_request_body.nPDUs; ++i) {
    const nfapi_nr_dl_tti_request_pdu_t *dl_tti_pdu = &DL_req->dl_tti_request_body.dl_tti_pdu_list[i];
    switch (dl_tti_pdu->PDUType) {
      case NFAPI_NR_DL_TTI_SSB_PDU_TYPE:
        nr_common_signal_procedures(gNB, frame, slot, &dl_tti_pdu->ssb_pdu);
        break;
      case NFAPI_NR_DL_TTI_PDCCH_PDU_TYPE:
        nr_generate_dci(gNB, &dl_tti_pdu->pdcch_pdu.pdcch_pdu_rel15, txdataF_offset, &gNB->frame_parms, slot);
        break;
      case NFAPI_NR_DL_TTI_CSI_RS_PDU_TYPE:
        nr_generate_csi_rs_gNB(gNB, slot, cfg, &dl_tti_pdu->csi_rs_pdu);
        break;
      case NFAPI_NR_DL_TTI_PDSCH_PDU_TYPE: {
        int tx_data_idx = dl_tti_pdu->pdsch_pdu.pdsch_pdu_rel15.pduIndex;
        if (tx_data_idx < TX_req->Number_of_PDUs && TX_req->pdu_list[tx_data_idx].PDU_index == tx_data_idx) {
          // reuse dlsch variables, as there are multiple very large memory
          // buffers
          gNB->dlsch[num_pdsch].pdsch_pdu = &dl_tti_pdu->pdsch_pdu;
          const nfapi_nr_tx_data_request_tlv_t *tlv = &TX_req->pdu_list[tx_data_idx].TLVs[0];
          gNB->dlsch[num_pdsch].pdu = tlv->tag == 0 ? (uint8_t *)tlv->value.direct : (uint8_t *)tlv->value.ptr;
          DevAssert(num_pdsch < gNB->max_nb_pdsch);
          num_pdsch++;
        } else {
          LOG_E(NR_PHY,
                "%4d.%2d no corresponding tx_data.request for dl_tti.request index %d (out of %d)\n",
                frame,
                slot,
                tx_data_idx,
                TX_req->Number_of_PDUs);
        }
        } break;
    }
  }
 
  if (num_pdsch > 0) {
    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_GENERATE_DLSCH,1);
    LOG_D(PHY, "PDSCH generation started (%d) in frame %d.%d\n", num_pdsch, frame, slot);
    nr_generate_pdsch(gNB, num_pdsch, gNB->dlsch, frame, slot);
    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_GENERATE_DLSCH,0);
  }

  //apply the OFDM symbol rotation here
  start_meas(&gNB->phase_comp_stats);
  for (int i = 0; i < gNB->common_vars.num_beams_period; ++i) {
    for (int aa = 0; aa < cfg->carrier_config.num_tx_ant.value; aa++) {
      if (gNB->phase_comp) {
        apply_nr_rotation_TX(fp,
                             &gNB->common_vars.txdataF[i][aa][txdataF_offset],
                             fp->symbol_rotation[0],
                             slot,
                             fp->N_RB_DL,
                             0,
                             fp->Ncp == EXTENDED ? 12 : 14);
      }
      T(T_GNB_PHY_DL_OUTPUT_SIGNAL,
        T_INT(0),
        T_INT(frame),
        T_INT(slot),
        T_INT(aa),
        T_BUFFER(&gNB->common_vars.txdataF[i][aa][txdataF_offset], fp->samples_per_slot_wCP * sizeof(int32_t)));
    }
  }
  stop_meas(&gNB->phase_comp_stats);

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_gNB_TX + gNB->CC_id, 0);
}

static int nr_ulsch_procedures(PHY_VARS_gNB *gNB, int frame_rx, int slot_rx, bool *ulsch_to_decode, NR_UL_IND_t *UL_INFO)
{
  NR_DL_FRAME_PARMS *frame_parms = &gNB->frame_parms;

  int nb_pusch = 0;
  for (int ULSCH_id = 0; ULSCH_id < gNB->max_nb_pusch; ULSCH_id++) {
    if (ulsch_to_decode[ULSCH_id]) {
      nb_pusch++;
    }
  }

  if (nb_pusch == 0) {
    return 0;
  }

  uint8_t ULSCH_ids[nb_pusch];
  uint32_t G[nb_pusch];
  int pusch_id = 0;
  for (int ULSCH_id = 0; ULSCH_id < gNB->max_nb_pusch; ULSCH_id++) {

    if (ulsch_to_decode[ULSCH_id]) {

      ULSCH_ids[pusch_id] = ULSCH_id;

      nfapi_nr_pusch_pdu_t *pusch_pdu = &gNB->ulsch[ULSCH_id].harq_process->ulsch_pdu;

      uint16_t nb_re_dmrs;
      uint16_t start_symbol = pusch_pdu->start_symbol_index;
      uint16_t number_symbols = pusch_pdu->nr_of_symbols;
  
      uint8_t number_dmrs_symbols = 0;
      for (int l = start_symbol; l < start_symbol + number_symbols; l++)
        number_dmrs_symbols += ((pusch_pdu->ul_dmrs_symb_pos)>>l)&0x01;
  
      if (pusch_pdu->dmrs_config_type==pusch_dmrs_type1)
        nb_re_dmrs = 6*pusch_pdu->num_dmrs_cdm_grps_no_data;
      else
        nb_re_dmrs = 4*pusch_pdu->num_dmrs_cdm_grps_no_data;
  
      G[pusch_id] = nr_get_G(pusch_pdu->rb_size,
                            number_symbols,
                            nb_re_dmrs,
                            number_dmrs_symbols, // number of dmrs symbols irrespective of single or double symbol dmrs
                            gNB->ulsch[ULSCH_id].unav_res,
                            pusch_pdu->qam_mod_order,
                            pusch_pdu->nrOfLayers);
      AssertFatal(G[pusch_id]>0,"G is 0 : rb_size %u, number_symbols %d, nb_re_dmrs %d, number_dmrs_symbols %d, qam_mod_order %u, nrOfLayer %u\n",
                  pusch_pdu->rb_size,
                  number_symbols,
                  nb_re_dmrs,
                  number_dmrs_symbols, // number of dmrs symbols irrespective of single or double symbol dmrs
                  pusch_pdu->qam_mod_order,
                  pusch_pdu->nrOfLayers);
      LOG_D(PHY,"rb_size %d, number_symbols %d, nb_re_dmrs %d, dmrs symbol positions %d, number_dmrs_symbols %d, qam_mod_order %d, nrOfLayer %d\n",
            pusch_pdu->rb_size,
            number_symbols,
            nb_re_dmrs,
            pusch_pdu->ul_dmrs_symb_pos,
            number_dmrs_symbols, // number of dmrs symbols irrespective of single or double symbol dmrs
            pusch_pdu->qam_mod_order,
            pusch_pdu->nrOfLayers);
      pusch_id++;
    }
  }
  
  //----------------------------------------------------------
  //--------------------- ULSCH decoding ---------------------
  //----------------------------------------------------------

  int ret_nr_ulsch_decoding = nr_ulsch_decoding(gNB, frame_parms, frame_rx, slot_rx, G, ULSCH_ids, nb_pusch);

  // CRC check per uplink shared channel
  for (pusch_id = 0; pusch_id < nb_pusch; pusch_id++) {
    uint8_t ULSCH_id = ULSCH_ids[pusch_id];
    NR_gNB_ULSCH_t *ulsch = &gNB->ulsch[ULSCH_id];
    NR_gNB_PUSCH *pusch = &gNB->pusch_vars[ULSCH_id];
    NR_UL_gNB_HARQ_t *ulsch_harq = ulsch->harq_process;
    nfapi_nr_pusch_pdu_t *pusch_pdu = &ulsch_harq->ulsch_pdu;

    bool crc_valid = false;

    // if all segments are done
    if (ulsch_harq->processedSegments == ulsch_harq->C) {
      if (ulsch_harq->C > 1) {
        crc_valid = check_crc(ulsch_harq->b, lenWithCrc(1, (ulsch_harq->TBS) << 3), crcType(1, (ulsch_harq->TBS) << 3));
      } else {
        // When the number of code blocks is 1 (C = 1) and ulsch_harq->processedSegments = 1, we can assume a good TB because of the
        // CRC check made by the LDPC for early termination, so, no need to perform CRC check twice for a single code block
        crc_valid = true;
      }
    }
#if T_TRACER
    if (T_ACTIVE(T_GNB_PHY_UL_PAYLOAD_RX_BITS)) {
      // capture Rx Payload via T-Tracer for both CRC valid and invalid cases
      // Get Time Stamp for T-tracer messages
      char trace_rx_payload_time_stamp_str[30];
      get_time_stamp_usec(trace_rx_payload_time_stamp_str);
      // trace_rx_payload_time_stamp_str = 8 bytes timestamp = YYYYMMDD
      //                      + 9 bytes timestamp = HHMMSSMMM

      // Log GNB_PHY_UL_PAYLOAD_RX_BITS using T-Tracer if activated
      // FORMAT = int,frame : int,slot : int,datetime_yyyymmdd : int,datetime_hhmmssmmm :
      // int,frame_type : int,freq_range : int,subcarrier_spacing : int,cyclic_prefix : int,symbols_per_slot :
      // int,Nid_cell : int,rnti :
      // int,rb_size : int,rb_start : int,start_symbol_index : int,nr_of_symbols :
      // int,qam_mod_order : int,mcs_index : int,mcs_table : int,nrOfLayers :
      // int,transform_precoding : int,dmrs_config_type : int,ul_dmrs_symb_pos :  int,number_dmrs_symbols : int,dmrs_port :
      // int,dmrs_nscid : int,nb_antennas_rx : int,number_of_bits : buffer,data

      NR_DL_FRAME_PARMS *frame_parms = &gNB->frame_parms;
      int dmrs_port = get_dmrs_port(0, pusch_pdu->dmrs_ports);
      // int num_bytes = rdata->Kr_bytes - (ulsch_harq->F >> 3) - ((ulsch_harq->C > 1) ? 3 : 0);
      // printf("num_bytes %d, len with CRC %d, data len %d\n", num_bytes, lenWithCrc(1, rdata->A), rdata->A);
      //  calculate the number of dmrs symbols in the slot
      int number_dmrs_symbols = 0;
      for (int l = pusch_pdu->start_symbol_index; l < pusch_pdu->start_symbol_index + pusch_pdu->nr_of_symbols; l++)
        number_dmrs_symbols += ((pusch_pdu->ul_dmrs_symb_pos) >> l) & 0x01;

      // Log GNB_PHY_UL_PAYLOAD_RX_BITS using T-Tracer if activated
      T(T_GNB_PHY_UL_PAYLOAD_RX_BITS,
        T_INT((int)ulsch->frame),
        T_INT((int)ulsch->slot),
        T_INT((int)split_time_stamp_and_convert_to_int(trace_rx_payload_time_stamp_str, 0, 8)),
        T_INT((int)split_time_stamp_and_convert_to_int(trace_rx_payload_time_stamp_str, 8, 9)),
        T_INT((int)frame_parms->frame_type), // Frame type (0 FDD, 1 TDD)  frame_structure
        T_INT((int)frame_parms->freq_range), // Frequency range (0 FR1, 1 FR2)
        T_INT((int)pusch_pdu->subcarrier_spacing), // Subcarrier spacing (0 15kHz, 1 30kHz, 2 60kHz)
        T_INT((int)pusch_pdu->cyclic_prefix), // Normal or extended prefix (0 normal, 1 extended)
        T_INT((int)frame_parms->symbols_per_slot), // Number of symbols per slot
        T_INT((int)frame_parms->Nid_cell),
        T_INT((int)pusch_pdu->rnti),
        T_INT((int)pusch_pdu->rb_size),
        T_INT((int)pusch_pdu->rb_start),
        T_INT((int)pusch_pdu->start_symbol_index), // start_ofdm_symbol
        T_INT((int)pusch_pdu->nr_of_symbols), // num_ofdm_symbols
        T_INT((int)pusch_pdu->qam_mod_order), // modulation
        T_INT((int)pusch_pdu->mcs_index), // mcs
        T_INT((int)pusch_pdu->mcs_table), // mcs_table_index
        T_INT((int)pusch_pdu->nrOfLayers), // num_layer
        T_INT((int)pusch_pdu->transform_precoding), // transformPrecoder_enabled = 0, transformPrecoder_disabled = 1
        T_INT((int)pusch_pdu->dmrs_config_type), // dmrs_resource_map_config: pusch_dmrs_type1 = 0, pusch_dmrs_type2 = 1
        T_INT((int)pusch_pdu->ul_dmrs_symb_pos), // used to derive the DMRS symbol positions
        T_INT((int)number_dmrs_symbols),
        // dmrs_start_ofdm_symbol
        // dmrs_duration_num_ofdm_symbols
        // dmrs_num_add_positions
        T_INT((int)dmrs_port), // dmrs_antenna_port
        T_INT((int)pusch_pdu->scid), // dmrs_nscid
        T_INT((int)frame_parms->nb_antennas_rx), // rx antenna
        T_INT(((ulsch_harq->TBS) << 3)), // number_of_bits
        T_BUFFER((uint8_t *)((ulsch_harq->b)), ((ulsch_harq->TBS) << 3) / 8)); // data
    }
#endif

    nfapi_nr_crc_t *crc = &UL_INFO->crc_ind.crc_list[UL_INFO->crc_ind.number_crcs++];
    nfapi_nr_rx_data_pdu_t *pdu = &UL_INFO->rx_ind.pdu_list[UL_INFO->rx_ind.number_of_pdus++];
    if (crc_valid && !check_abort(&ulsch_harq->abort_decode) && !pusch->DTX) {
      LOG_D(NR_PHY,
            "[gNB %d] ULSCH %d: Setting ACK for SFN/SF %d.%d (rnti %x, pid %d, ndi %d, status %d, round %d, TBS %d, Max interation "
            "(all seg) %d)\n",
            gNB->Mod_id,
            ULSCH_id,
            ulsch->frame,
            ulsch->slot,
            ulsch->rnti,
            ulsch->harq_pid,
            pusch_pdu->pusch_data.new_data_indicator,
            ulsch->active,
            ulsch_harq->round,
            ulsch_harq->TBS,
            ulsch->max_ldpc_iterations);
      nr_fill_indication(gNB, ulsch->frame, ulsch->slot, ULSCH_id, ulsch->harq_pid, 0, 0, crc, pdu);
      LOG_D(PHY, "ULSCH received ok \n");
      ulsch->active = false;
      ulsch_harq->round = 0;
      ulsch->last_iteration_cnt = ulsch->max_ldpc_iterations - 1; // Setting to max_ldpc_iterations - 1 is sufficient given that this variable is only used for checking for failure
    } else {
      LOG_D(PHY,
            "[gNB %d] ULSCH: Setting NAK for SFN/SF %d/%d (pid %d, ndi %d, status %d, round %d, RV %d, prb_start %d, prb_size %d, "
            "TBS %d)\n",
            gNB->Mod_id,
            ulsch->frame,
            ulsch->slot,
            ulsch->harq_pid,
            pusch_pdu->pusch_data.new_data_indicator,
            ulsch->active,
            ulsch_harq->round,
            ulsch_harq->ulsch_pdu.pusch_data.rv_index,
            ulsch_harq->ulsch_pdu.rb_start,
            ulsch_harq->ulsch_pdu.rb_size,
            ulsch_harq->TBS);
      nr_fill_indication(gNB, ulsch->frame, ulsch->slot, ULSCH_id, ulsch->harq_pid, 1, 0, crc, pdu);
      gNBdumpScopeData(gNB, ulsch->slot, ulsch->frame, "ULSCH_NACK");
      ulsch->handled = 1;
      LOG_D(PHY, "ULSCH %d in error\n",ULSCH_id);
      ulsch->last_iteration_cnt = ulsch->max_ldpc_iterations; // Setting to max_ldpc_iterations is sufficient given that this variable is only used for checking for failure
    }
  }

  return ret_nr_ulsch_decoding;
}

static void nr_fill_indication(PHY_VARS_gNB *gNB,
                               int frame,
                               int slot_rx,
                               int ULSCH_id,
                               uint8_t harq_pid,
                               uint8_t crc_flag,
                               int dtx_flag,
                               nfapi_nr_crc_t *crc,
                               nfapi_nr_rx_data_pdu_t *pdu)
{
  NR_UL_gNB_HARQ_t *harq_process = gNB->ulsch[ULSCH_id].harq_process;
  NR_gNB_PHY_STATS_t *stats = get_phy_stats(gNB, gNB->ulsch[ULSCH_id].rnti);
  NR_gNB_PUSCH *pusch = &gNB->pusch_vars[ULSCH_id];

  nfapi_nr_pusch_pdu_t *pusch_pdu = &harq_process->ulsch_pdu;

  // Get estimated timing advance for MAC
  const int sync_pos = gNB->ulsch[ULSCH_id].delay.est_delay;

  // scale the 16 factor in N_TA calculation in 38.213 section 4.2 according to the used FFT size
  uint16_t bw_scaling = 16 * gNB->frame_parms.ofdm_symbol_size / 2048;

  // do some integer rounding to improve TA accuracy
  int sync_pos_rounded;
  if (sync_pos > 0)
    sync_pos_rounded = sync_pos + (bw_scaling / 2) - 1;
  else
    sync_pos_rounded = sync_pos - (bw_scaling / 2) + 1;
  if (stats)
    stats->ulsch_stats.sync_pos = sync_pos;

  int timing_advance_update = sync_pos_rounded / bw_scaling;

  // put timing advance command in 0..63 range
  timing_advance_update += 31;
  timing_advance_update = max(timing_advance_update, 0);
  timing_advance_update = min(timing_advance_update, 63);

  if (crc_flag == 0)
    LOG_D(PHY,
          "%d.%d : Received PUSCH : Estimated timing advance PUSCH is  = %d, timing_advance_update is %d \n",
          frame,
          slot_rx,
          sync_pos,
          timing_advance_update);

  // estimate UL_CQI for MAC
  int SNRtimes10 = dB_fixed_x10(pusch->ulsch_power_tot) - dB_fixed_x10(pusch->ulsch_noise_power_tot);

  LOG_D(PHY,
        "%d.%d: Estimated SNR for PUSCH is = %f dB (ulsch_power %f, noise %f) delay %d\n",
        frame,
        slot_rx,
        SNRtimes10 / 10.0,
        dB_fixed_x10(pusch->ulsch_power_tot) / 10.0,
        dB_fixed_x10(pusch->ulsch_noise_power_tot) / 10.0,
        sync_pos);

  int cqi;
  if      (SNRtimes10 < -640) cqi=0;
  else if (SNRtimes10 >  635) cqi=255;
  else                        cqi=(640+SNRtimes10)/5;

  crc->handle = pusch_pdu->handle;
  crc->rnti = pusch_pdu->rnti;
  crc->harq_id = harq_pid;
  crc->tb_crc_status = crc_flag;
  crc->num_cb = pusch_pdu->pusch_data.num_cb;
  crc->ul_cqi = cqi;
  crc->timing_advance = timing_advance_update;
  // in terms of dBFS range -128 to 0 with 0.1 step
  crc->rssi = (dtx_flag == 0) ? 1280 - (10 * dB_fixed(32767 * 32767) - dB_fixed_times10(pusch->ulsch_power[0])) : 0;

  pdu->handle = pusch_pdu->handle;
  pdu->rnti = pusch_pdu->rnti;
  pdu->harq_id = harq_pid;
  pdu->ul_cqi = cqi;
  pdu->timing_advance = timing_advance_update;
  pdu->rssi = crc->rssi;
  if (crc_flag)
    pdu->pdu_length = 0;
  else {
    pdu->pdu_length = harq_process->TBS;
    pdu->pdu = harq_process->b;
  }
}

// Function to fill UL RB mask to be used for N0 measurements
static void fill_ul_rb_mask(PHY_VARS_gNB *gNB,
                            int frame_rx,
                            int slot_rx,
                            uint32_t rb_mask_ul[14][9],
                            nfapi_nr_max_num_of_symbol_per_slot_t *slot_conf)
{
  for (int symbol = 0; symbol < 14; symbol++) {
    for (int m = 0; m < 9; m++) {
      rb_mask_ul[symbol][m] = 0;
      for (int i = 0; i < 32; i++) {
        int prbpos = (m * 32) + i;
        if (prbpos > gNB->frame_parms.N_RB_UL)
          break;
        rb_mask_ul[symbol][m] |= (gNB->ulprbbl[prbpos] > 0 ? 1U : 0) << i;
      }
    }
  }

  for (int i = 0; i < gNB->max_nb_pucch; i++){
    const NR_gNB_PUCCH_t *pucch = gNB->pucch + i;
    if (!(pucch && pucch->active && pucch->frame == frame_rx && pucch->slot == slot_rx))
      continue;
    const nfapi_nr_pucch_pdu_t *pucch_pdu = &pucch->pucch_pdu;
    const int start = pucch_pdu->start_symbol_index;
    LOG_D(PHY,
          "%d.%d pucch %d : start_symbol %d, nb_symbols %d, prb_size %d\n",
          frame_rx,
          slot_rx,
          i,
          start,
          pucch_pdu->nr_of_symbols,
          pucch_pdu->prb_size);
    for (int symbol = start; symbol < (start + pucch_pdu->nr_of_symbols); symbol++) {
      if (gNB->frame_parms.frame_type == FDD || (gNB->frame_parms.frame_type == TDD && slot_conf[symbol].slot_config.value == 1)) {
        for (int rb = 0; rb < pucch_pdu->prb_size; rb++) {
          int rb2 =
              rb + pucch_pdu->bwp_start
              + ((symbol < start + (pucch_pdu->nr_of_symbols >> 1)) || (pucch_pdu->freq_hop_flag == 0) ? pucch_pdu->prb_start
                                                                                                       : pucch_pdu->second_hop_prb);
          rb_mask_ul[symbol][rb2 >> 5] |= 1U << (rb2 & 31);
        }
      }
    }
  }

  for (int ULSCH_id = 0; ULSCH_id < gNB->max_nb_pusch; ULSCH_id++) {
    NR_gNB_ULSCH_t *ulsch = &gNB->ulsch[ULSCH_id];
    NR_UL_gNB_HARQ_t *ulsch_harq = ulsch->harq_process;
    AssertFatal(ulsch_harq != NULL, "harq_pid %d is not allocated\n", ulsch->harq_pid);
    if (!(ulsch->active && ulsch->frame == frame_rx && ulsch->slot == slot_rx && !ulsch->handled))
      continue;
    uint8_t symbol_start = ulsch_harq->ulsch_pdu.start_symbol_index;
    uint8_t symbol_end = symbol_start + ulsch_harq->ulsch_pdu.nr_of_symbols;
    for (int symbol = symbol_start; symbol < symbol_end; symbol++) {
      if (gNB->frame_parms.frame_type == FDD || (gNB->frame_parms.frame_type == TDD && slot_conf[symbol].slot_config.value == 1)) {
        LOG_D(PHY, "symbol %d Filling rb_mask_ul rb_size %d\n", symbol, ulsch_harq->ulsch_pdu.rb_size);
        for (int rb = 0; rb < ulsch_harq->ulsch_pdu.rb_size; rb++) {
          int rb2 = rb + ulsch_harq->ulsch_pdu.rb_start + ulsch_harq->ulsch_pdu.bwp_start;
          rb_mask_ul[symbol][rb2 >> 5] |= 1U << (rb2 & 31);
        }
      }
    }
  }

  for (int i = 0; i < gNB->max_nb_srs; i++) {
    NR_gNB_SRS_t *srs = &gNB->srs[i];
    if (!(srs && srs->active && srs->frame == frame_rx && srs->slot == slot_rx))
      continue;
    nfapi_nr_srs_pdu_t *srs_pdu = &srs->srs_pdu;
    for (int symbol = 0; symbol < (1 << srs_pdu->num_symbols); symbol++) {
      for (int rb = srs_pdu->bwp_start; rb < (srs_pdu->bwp_start + srs_pdu->bwp_size); rb++) {
        rb_mask_ul[srs_pdu->time_start_position + symbol][rb >> 5] |= 1U << (rb & 31);
      }
    }
  }
}

static int fill_srs_reported_symbol(nfapi_nr_srs_reported_symbol_t *reported_symbol,
                                    const nfapi_nr_srs_pdu_t *srs_pdu,
                                    const int N_RB_UL,
                                    const int16_t *snr_per_rb,
                                    const int srs_est)
{
  reported_symbol->num_prgs = srs_pdu->beamforming.num_prgs;
  for (int prg_idx = 0; prg_idx < reported_symbol->num_prgs; prg_idx++) {
    uint8_t *snr = &reported_symbol->prg_list[prg_idx].rb_snr;
    if (srs_est < 0) {
      *snr = 0xFF;
    } else if (snr_per_rb[prg_idx] < -64) {
      *snr = 0;
    } else if (snr_per_rb[prg_idx] > 63) {
      *snr = 0xFE;
    } else {
      *snr = (snr_per_rb[prg_idx] + 64) << 1;
    }
  }

  return 0;
}

static int fill_srs_channel_matrix(
    nfapi_nr_srs_normalized_channel_iq_matrix_t *nr_srs_channel_iq_matrix,
    const nfapi_nr_srs_pdu_t *srs_pdu,
    const nr_srs_info_t *nr_srs_info,
    const uint8_t normalized_iq_representation,
    const uint16_t num_gnb_antenna_elements,
    const uint16_t num_ue_srs_ports,
    const uint16_t prg_size,
    const uint16_t num_prgs,
    const NR_DL_FRAME_PARMS *frame_parms,
    const c16_t srs_estimated_channel_freq[][1 << srs_pdu->num_ant_ports]
                                          [frame_parms->ofdm_symbol_size * (1 << srs_pdu->num_symbols)])
{
  nr_srs_channel_iq_matrix->normalized_iq_representation = normalized_iq_representation;
  nr_srs_channel_iq_matrix->num_gnb_antenna_elements = num_gnb_antenna_elements;
  nr_srs_channel_iq_matrix->num_ue_srs_ports = num_ue_srs_ports;
  nr_srs_channel_iq_matrix->prg_size = prg_size;
  nr_srs_channel_iq_matrix->num_prgs = num_prgs;
  uint8_t *channel_matrix = nr_srs_channel_iq_matrix->channel_matrix;
  const uint64_t subcarrier_offset = frame_parms->first_carrier_offset + srs_pdu->bwp_start*NR_NB_SC_PER_RB;
  const uint16_t step = prg_size*NR_NB_SC_PER_RB;

  c16_t *channel_matrix16 = (c16_t*)channel_matrix;
  c8_t *channel_matrix8 = (c8_t*)channel_matrix;

  for(int uI = 0; uI < num_ue_srs_ports; uI++) {
    for(int gI = 0; gI < num_gnb_antenna_elements; gI++) {

      uint16_t subcarrier = subcarrier_offset + nr_srs_info->k_0_p[uI][0];
      if (subcarrier >= frame_parms->ofdm_symbol_size) {
        subcarrier -= frame_parms->ofdm_symbol_size;
      }

      for(int pI = 0; pI < num_prgs; pI++) {
        const c16_t *srs_estimated_channel16 = srs_estimated_channel_freq[gI][uI] + subcarrier;
        uint16_t index = uI*num_gnb_antenna_elements*num_prgs + gI*num_prgs + pI;

        if (normalized_iq_representation == 0) {
          channel_matrix8[index].r = (int8_t)(srs_estimated_channel16->r>>8);
          channel_matrix8[index].i = (int8_t)(srs_estimated_channel16->i>>8);
        } else {
          channel_matrix16[index].r = srs_estimated_channel16->r;
          channel_matrix16[index].i = srs_estimated_channel16->i;
        }

        // Subcarrier increment
        subcarrier += step;
        if (subcarrier >= frame_parms->ofdm_symbol_size) {
          subcarrier -= frame_parms->ofdm_symbol_size;
        }
      }
    }
  }

  return 0;
}

static void copy_srs_info(const nfapi_nr_srs_pdu_t *srs_config_pdu, nr_srs_info_t *nr_srs_info)
{
  nr_srs_info->B_SRS = srs_config_pdu->bandwidth_index;
  nr_srs_info->C_SRS = srs_config_pdu->config_index;
  nr_srs_info->b_hop = srs_config_pdu->frequency_hopping;
  nr_srs_info->comb_size = srs_config_pdu->comb_size;
  nr_srs_info->K_TC_overbar = srs_config_pdu->comb_offset;
  nr_srs_info->n_SRS_cs = srs_config_pdu->cyclic_shift;
  nr_srs_info->n_ID_SRS = srs_config_pdu->sequence_id;
  // It adjusts the SRS allocation to align with the common resource block grid in multiples of four
  nr_srs_info->n_shift = srs_config_pdu->frequency_position;
  nr_srs_info->n_RRC = srs_config_pdu->frequency_shift;
  nr_srs_info->groupOrSequenceHopping = srs_config_pdu->group_or_sequence_hopping;
  nr_srs_info->l_offset = srs_config_pdu->time_start_position;
  nr_srs_info->T_SRS = srs_config_pdu->t_srs;
  nr_srs_info->T_offset = srs_config_pdu->t_offset;
  nr_srs_info->R = 1 << srs_config_pdu->num_repetitions;
  nr_srs_info->N_symb_SRS = 1 << srs_config_pdu->num_symbols; // Number of consecutive OFDM symbols
  nr_srs_info->n_srs_ports = 1 << srs_config_pdu->num_ant_ports; // Number of antenna port for transmission
  nr_srs_info->resource_type = srs_config_pdu->resource_type;
}

static bool srs_has_changed(const nfapi_nr_srs_pdu_t *srs_config_pdu, nr_srs_info_t *nr_srs_info)
{
  return nr_srs_info->B_SRS != srs_config_pdu->bandwidth_index
         || nr_srs_info->C_SRS != srs_config_pdu->config_index
         || nr_srs_info->b_hop != srs_config_pdu->frequency_hopping
         ||  nr_srs_info->comb_size != srs_config_pdu->comb_size
         || nr_srs_info->K_TC_overbar != srs_config_pdu->comb_offset
         || nr_srs_info->n_SRS_cs != srs_config_pdu->cyclic_shift
         || nr_srs_info->n_ID_SRS != srs_config_pdu->sequence_id
         || nr_srs_info->n_shift != srs_config_pdu->frequency_position
         || nr_srs_info->n_RRC != srs_config_pdu->frequency_shift
         || nr_srs_info->groupOrSequenceHopping != srs_config_pdu->group_or_sequence_hopping
         || nr_srs_info->l_offset != srs_config_pdu->time_start_position
         || nr_srs_info->T_SRS != srs_config_pdu->t_srs
         || nr_srs_info->T_offset != srs_config_pdu->t_offset
         || nr_srs_info->R != 1 << srs_config_pdu->num_repetitions
         || nr_srs_info->N_symb_SRS != 1 << srs_config_pdu->num_symbols
         || nr_srs_info->n_srs_ports != 1 << srs_config_pdu->num_ant_ports
         || nr_srs_info->resource_type != srs_config_pdu->resource_type;
}

static bool check_and_configure_srs_info(nfapi_nr_srs_pdu_t *srs_config_pdu, nr_srs_info_t *nr_srs_info)
{
  if (srs_has_changed(srs_config_pdu, nr_srs_info)) {
    copy_srs_info(srs_config_pdu, nr_srs_info);
    return true;
  }
  return false;
}

int phy_procedures_gNB_uespec_RX(PHY_VARS_gNB *gNB, int frame_rx, int slot_rx, NR_UL_IND_t *UL_INFO)
{
  /* those variables to log T_GNB_PHY_PUCCH_PUSCH_IQ only when we try to decode */
  int pucch_decode_done = 0;
  int pusch_decode_done = 0;
  int pusch_DTX = 0;

  const NR_DL_FRAME_PARMS *frame_parms = &gNB->frame_parms;
  const int symb_sz = frame_parms->ofdm_symbol_size;
  const int nb_symb = frame_parms->symbols_per_slot;
  const int ant = gNB->frame_parms.nb_antennas_rx;
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_gNB_UESPEC_RX,1);
  LOG_D(PHY,"phy_procedures_gNB_uespec_RX frame %d, slot %d\n",frame_rx,slot_rx);
  {
    // Mask of occupied RBs, per symbol and PRB
    uint32_t rb_mask_ul[14][9];
    nfapi_nr_max_num_of_symbol_per_slot_t *slot_conf = NULL;
    if (frame_parms->frame_type == TDD)
      slot_conf = gNB->gNB_config.tdd_table.max_tdd_periodicity_list[slot_rx].max_num_of_symbol_per_slot_list;
    fill_ul_rb_mask(gNB, frame_rx, slot_rx, rb_mask_ul, slot_conf);

    int first_symb = 0, num_symb = 0;
    if (frame_parms->frame_type == TDD)
      for (int symbol_count = 0; symbol_count < NR_NUMBER_OF_SYMBOLS_PER_SLOT; symbol_count++) {
        if (slot_conf[symbol_count].slot_config.value == 1) {
          if (num_symb == 0)
            first_symb = symbol_count;
          num_symb++;
        }
      }
    else
      num_symb = NR_NUMBER_OF_SYMBOLS_PER_SLOT;
    gNB_I0_measurements(gNB, slot_rx, first_symb, num_symb, rb_mask_ul);
  }

  const int soffset = (slot_rx & 3) * nb_symb * symb_sz;
  start_meas(&gNB->phy_proc_rx);

  for (int i = 0; i < gNB->max_nb_pucch; i++) {
    NR_gNB_PUCCH_t *pucch = &gNB->pucch[i];
    if (!(pucch && pucch->active && pucch->frame == frame_rx && pucch->slot == slot_rx))
      continue;
    c16_t **rxdataF = gNB->common_vars.rxdataF[pucch->beam_nb];
    pucch_decode_done = 1;
    nfapi_nr_pucch_pdu_t *pucch_pdu = &pucch->pucch_pdu;
    UL_INFO->uci_ind.uci_list = UL_INFO->uci_pdu_list;
    nfapi_nr_uci_t *uci = UL_INFO->uci_ind.uci_list + UL_INFO->uci_ind.num_ucis;
    UL_INFO->uci_ind.sfn = frame_rx;
    UL_INFO->uci_ind.slot = slot_rx;
    switch (pucch_pdu->format_type) {
      case 0:
        uci->pdu_type = NFAPI_NR_UCI_FORMAT_0_1_PDU_TYPE;
        uci->pdu_size = sizeof(nfapi_nr_uci_pucch_pdu_format_0_1_t);
        nfapi_nr_uci_pucch_pdu_format_0_1_t *uci_pdu_format0 = &uci->pucch_pdu_format_0_1;
        int offset = pucch_pdu->start_symbol_index * symb_sz + (frame_parms->first_carrier_offset + pucch_pdu->prb_start * 12);
        LOG_D(NR_PHY,
              "frame %d, slot %d: PUCCH signal energy %d\n",
              frame_rx,
              slot_rx,
              signal_energy_nodc(&rxdataF[0][soffset + offset], 12));
        nr_decode_pucch0(gNB, rxdataF, frame_rx, slot_rx, uci_pdu_format0, pucch_pdu);
        break;
      case 2:
        uci->pdu_type = NFAPI_NR_UCI_FORMAT_2_3_4_PDU_TYPE;
        uci->pdu_size = sizeof(nfapi_nr_uci_pucch_pdu_format_2_3_4_t);
        nfapi_nr_uci_pucch_pdu_format_2_3_4_t *uci_pdu_format2 = &uci->pucch_pdu_format_2_3_4;
        LOG_D(PHY, "%d.%d Calling nr_decode_pucch2\n", frame_rx, slot_rx);
        nr_decode_pucch2(gNB, rxdataF, frame_rx, slot_rx, uci_pdu_format2, pucch_pdu);
        break;
      default:
        AssertFatal(1 == 0, "Only PUCCH formats 0 and 2 are currently supported\n");
    }
    UL_INFO->uci_ind.num_ucis += 1;
    pucch->active = false;
  }

  UL_INFO->crc_ind.sfn = frame_rx;
  UL_INFO->crc_ind.slot = slot_rx;
  UL_INFO->crc_ind.crc_list = UL_INFO->crc_pdu_list;
  UL_INFO->rx_ind.sfn = frame_rx;
  UL_INFO->rx_ind.slot = slot_rx;
  UL_INFO->rx_ind.pdu_list = UL_INFO->rx_pdu_list;
  bool ulsch_to_decode[gNB->max_nb_pusch];
  bzero(ulsch_to_decode, sizeof(ulsch_to_decode));
  for (int ULSCH_id = 0; ULSCH_id < gNB->max_nb_pusch; ULSCH_id++) {
    NR_gNB_ULSCH_t *ulsch = &gNB->ulsch[ULSCH_id];
    NR_UL_gNB_HARQ_t *ulsch_harq = ulsch->harq_process;
    AssertFatal(ulsch_harq != NULL, "harq_pid %d is not allocated\n", ulsch->harq_pid);
    if (!(ulsch->active && ulsch->frame == frame_rx && ulsch->slot == slot_rx && !ulsch->handled))
      continue;
    LOG_D(PHY, "PUSCH ID %d with RNTI %x detection started in frame %d slot %d\n", ULSCH_id, ulsch->rnti, frame_rx, slot_rx);
    nfapi_nr_pusch_pdu_t *pdu = &ulsch_harq->ulsch_pdu;
    int num_dmrs = count_bits64_with_mask(pdu->ul_dmrs_symb_pos, 0, NR_NUMBER_OF_SYMBOLS_PER_SLOT);

#ifdef DEBUG_RXDATA
    RU_t *ru = gNB->RU_list[0];
    int slot_offset = frame_parms->get_samples_slot_timestamp(slot_rx, frame_parms, 0);
    slot_offset -= ru->N_TA_offset;
    int32_t sample_offset = gNB->common_vars.debugBuff_sample_offset;
    int16_t buf = (int16_t *)&gNB->common_vars.debugBuff[offset];
    buf[0] = (int16_t)ulsch->rnti;
    buf[1] = (int16_t)pdu->rb_size;
    buf[2] = (int16_t)pdu->rb_start;
    buf[3] = (int16_t)pdu->nr_of_symbols;
    buf[4] = (int16_t)pdu->start_symbol_index;
    buf[5] = (int16_t)pdu->mcs_index;
    buf[6] = (int16_t)pdu->pusch_data.rv_index;
    buf[7] = (int16_t)ulsch->harq_pid;
    memcpy(&gNB->common_vars.debugBuff[gNB->common_vars.debugBuff_sample_offset + 4],
           &ru->common.rxdata[0][slot_offset],
           get_samples_per_slot(slot_rx, frame_parms) * sizeof(int32_t));
    gNB->common_vars.debugBuff_sample_offset += (get_samples_per_slot(slot_rx, frame_parms) + 1000 + 4);
    if (gNB->common_vars.debugBuff_sample_offset > ((get_samples_per_slot(slot_rx, frame_parms) + 1000 + 2) * 20)) {
      FILE *f;
      f = fopen("rxdata_buff.raw", "w");
      if (f == NULL)
        exit(1);
      fwrite((int16_t *)gNB->common_vars.debugBuff, 2, (get_samples_per_slot(slot_rx, frame_parms) + 1000 + 4) * 20 * 2, f);
      fclose(f);
      exit(-1);
    }
#endif

    pusch_decode_done = 1;

    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_NR_RX_PUSCH, 1);
    start_meas(&gNB->rx_pusch_stats);
    nr_rx_pusch_tp(gNB, ULSCH_id, frame_rx, slot_rx, ulsch->harq_pid, ulsch->beam_nb);
    NR_gNB_PUSCH *pusch_vars = &gNB->pusch_vars[ULSCH_id];
    pusch_vars->ulsch_power_tot = 0;
    pusch_vars->ulsch_noise_power_tot = 0;
    for (int aarx = 0; aarx < ant; aarx++) {
      pusch_vars->ulsch_power[aarx] /= num_dmrs;
      pusch_vars->ulsch_power_tot += pusch_vars->ulsch_power[aarx];
      pusch_vars->ulsch_noise_power[aarx] /= num_dmrs;
      pusch_vars->ulsch_noise_power_tot += pusch_vars->ulsch_noise_power[aarx];
    }
    if (dB_fixed_x10(pusch_vars->ulsch_power_tot) < dB_fixed_x10(pusch_vars->ulsch_noise_power_tot) + gNB->pusch_thres) {
      NR_gNB_PHY_STATS_t *stats = get_phy_stats(gNB, ulsch->rnti);

      LOG_D(PHY,
            "PUSCH not detected in %d.%d (%d,%d,%d)\n",
            frame_rx,
            slot_rx,
            dB_fixed_x10(pusch_vars->ulsch_power_tot),
            dB_fixed_x10(pusch_vars->ulsch_noise_power_tot),
            gNB->pusch_thres);
      pusch_vars->ulsch_power_tot = pusch_vars->ulsch_noise_power_tot;
      pusch_vars->DTX = 1;
      if (stats)
        stats->ulsch_stats.DTX++;
      if (!get_softmodem_params()->phy_test) {
        /* in case of phy_test mode, we still want to decode to measure execution time.
           Therefore, we don't yet call nr_fill_indication, it will be called later */
        nfapi_nr_crc_t *crc = &UL_INFO->crc_ind.crc_list[UL_INFO->crc_ind.number_crcs++];
        nfapi_nr_rx_data_pdu_t *pdu = &UL_INFO->rx_ind.pdu_list[UL_INFO->rx_ind.number_of_pdus++];
        nr_fill_indication(gNB, frame_rx, slot_rx, ULSCH_id, ulsch->harq_pid, 1, 1, crc, pdu);
        pusch_DTX++;
        gNBdumpScopeData(gNB, ulsch->slot, ulsch->frame, "ULSCH_DTX");
        continue;
      }
    } else {
      LOG_D(PHY,
            "PUSCH detected in %d.%d (%d,%d,%d)\n",
            frame_rx,
            slot_rx,
            dB_fixed_x10(pusch_vars->ulsch_power_tot),
            dB_fixed_x10(pusch_vars->ulsch_noise_power_tot),
            gNB->pusch_thres);

      pusch_vars->DTX = 0;
    }
    ulsch_to_decode[ULSCH_id] = true;
    stop_meas(&gNB->rx_pusch_stats);
    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_NR_RX_PUSCH, 0);
    // LOG_M("rxdataF_comp.m","rxF_comp",gNB->pusch_vars[0]->rxdataF_comp[0],6900,1,1);
    // LOG_M("rxdataF_ext.m","rxF_ext",gNB->pusch_vars[0]->rxdataF_ext[0],6900,1,1);
  }

  /* Do ULSCH decoding time measurement only when number of PUSCH is limited to 1
   * (valid for unitary physical simulators). ULSCH processing lopp is then executed
   * only once, which ensures exactly one start and stop of the ULSCH decoding time
   * measurement per processed TB.*/
  if (gNB->max_nb_pusch == 1)
    start_meas(&gNB->ulsch_decoding_stats);

  int const ret_nr_ulsch_procedures = nr_ulsch_procedures(gNB, frame_rx, slot_rx, ulsch_to_decode, UL_INFO);
  if (ret_nr_ulsch_procedures != 0) {
    LOG_E(PHY,"Error in nr_ulsch_procedures, returned %d\n",ret_nr_ulsch_procedures);
  }

  /* Do ULSCH decoding time measurement only when number of PUSCH is limited to 1
   * (valid for unitary physical simulators). ULSCH processing loop is then executed
   * only once, which ensures exactly one start and stop of the ULSCH decoding time
   * measurement per processed TB.*/
  if (gNB->max_nb_pusch == 1)
    stop_meas(&gNB->ulsch_decoding_stats);

  for (int i = 0; i < gNB->max_nb_srs; i++) {
    NR_gNB_SRS_t *srs = &gNB->srs[i];
    if (!(srs && srs->active && srs->frame == frame_rx && srs->slot == slot_rx))
      continue;
    LOG_D(NR_PHY, "(%d.%d) gNB is waiting for SRS, id = %i\n", frame_rx, slot_rx, i);

    start_meas(&gNB->rx_srs_stats);

    nfapi_nr_srs_pdu_t *srs_pdu = &srs->srs_pdu;
    uint8_t N_symb_SRS = 1 << srs_pdu->num_symbols;
    c16_t srs_received_signal[ant][symb_sz * N_symb_SRS];
    c16_t srs_received_noise[ant][symb_sz * N_symb_SRS];
    c16_t srs_estimated_channel_freq[ant][1 << srs_pdu->num_ant_ports][symb_sz * N_symb_SRS] __attribute__((aligned(32)));
    c16_t srs_estimated_channel_time[ant][1 << srs_pdu->num_ant_ports][symb_sz] __attribute__((aligned(32)));
    c16_t srs_estimated_channel_time_shifted[ant][1 << srs_pdu->num_ant_ports][symb_sz];
    int16_t snr_per_rb[srs_pdu->bwp_size];

    start_meas(&gNB->generate_srs_stats);

    if (check_and_configure_srs_info(srs_pdu, gNB->nr_srs_info[i])) {
      generate_srs_nr(frame_parms,
                      gNB->nr_srs_info[i]->srs_generated_signal,
                      0,
                      srs_pdu->bwp_start,
                      gNB->nr_srs_info[i],
                      AMP,
                      frame_rx,
                      slot_rx);
    }
    stop_meas(&gNB->generate_srs_stats);
    c16_t **rxdataF = gNB->common_vars.rxdataF[srs->beam_nb];
    start_meas(&gNB->get_srs_signal_stats);
    int srs_est =
        nr_get_srs_signal(gNB, rxdataF, frame_rx, slot_rx, srs_pdu, gNB->nr_srs_info[i], srs_received_signal, srs_received_noise);
    stop_meas(&gNB->get_srs_signal_stats);

    if (srs_est >= 0) {
      start_meas(&gNB->srs_channel_estimation_stats);
      nr_srs_channel_estimation(gNB,
                                frame_rx,
                                slot_rx,
                                srs_pdu,
                                gNB->nr_srs_info[i],
                                (const c16_t **)gNB->nr_srs_info[i]->srs_generated_signal,
                                srs_received_signal,
                                srs_received_noise,
                                srs_estimated_channel_freq,
                                srs_estimated_channel_time,
                                srs_estimated_channel_time_shifted,
                                snr_per_rb,
                                &gNB->srs->snr);
      stop_meas(&gNB->srs_channel_estimation_stats);
    }

    if ((gNB->srs->snr * 10) < gNB->srs_thres) {
      srs_est = -1;
    }

    T(T_GNB_PHY_UL_FREQ_CHANNEL_ESTIMATE,
      T_INT(0),
      T_INT(srs_pdu->rnti),
      T_INT(frame_rx),
      T_INT(0),
      T_INT(0),
      T_BUFFER(srs_estimated_channel_freq[0][0], symb_sz * sizeof(int32_t)));

    T(T_GNB_PHY_UL_TIME_CHANNEL_ESTIMATE,
      T_INT(0),
      T_INT(srs_pdu->rnti),
      T_INT(frame_rx),
      T_INT(0),
      T_INT(0),
      T_BUFFER(srs_estimated_channel_time_shifted[0][0], symb_sz * sizeof(int32_t)));

    T(T_GNB_PHY_UL_SNR_ESTIMATE,
      T_INT(0),
      T_INT(srs_pdu->rnti),
      T_INT(frame_rx),
      T_INT(0),
      T_INT(0),
      T_BUFFER(snr_per_rb, srs_pdu->bwp_size * sizeof(int16_t)));

    UL_INFO->srs_ind.sfn = frame_rx;
    UL_INFO->srs_ind.slot = slot_rx;

    // data model difficult to understand, nfapi do malloc for this pointer
    UL_INFO->srs_ind.pdu_list = UL_INFO->srs_pdu_list;
    nfapi_nr_srs_indication_pdu_t *srs_indication = UL_INFO->srs_pdu_list + UL_INFO->srs_ind.number_of_pdus++;
    srs_indication->handle = srs_pdu->handle;
    srs_indication->rnti = srs_pdu->rnti;
    start_meas(&gNB->srs_timing_advance_stats);
    srs_indication->timing_advance_offset =
        srs_est >= 0 ? nr_est_timing_advance_srs(frame_parms, srs_estimated_channel_time[0]) : 0xFFFF;
    stop_meas(&gNB->srs_timing_advance_stats);
    srs_indication->timing_advance_offset_nsec =
        srs_est >= 0 ? (int16_t)((((int32_t)srs_indication->timing_advance_offset - 31) * ((int32_t)TC_NSEC_x32768)) >> 15)
                     : 0xFFFF;
    switch (srs_pdu->srs_parameters_v4.usage) {
      case 0:
        LOG_W(NR_PHY, "SRS report was not requested by MAC\n");
        return 0;
      case 1 << NFAPI_NR_SRS_BEAMMANAGEMENT:
        srs_indication->srs_usage = NFAPI_NR_SRS_BEAMMANAGEMENT;
        break;
      case 1 << NFAPI_NR_SRS_CODEBOOK:
        srs_indication->srs_usage = NFAPI_NR_SRS_CODEBOOK;
        break;
      case 1 << NFAPI_NR_SRS_NONCODEBOOK:
        srs_indication->srs_usage = NFAPI_NR_SRS_NONCODEBOOK;
        break;
      case 1 << NFAPI_NR_SRS_ANTENNASWITCH:
        srs_indication->srs_usage = NFAPI_NR_SRS_ANTENNASWITCH;
        break;
      default:
        LOG_E(NR_PHY, "Invalid srs_pdu->srs_parameters_v4.usage %i\n", srs_pdu->srs_parameters_v4.usage);
    }
    srs_indication->report_type = srs_pdu->srs_parameters_v4.report_type[0];

#ifdef SRS_IND_DEBUG
    LOG_I(NR_PHY, "UL_INFO->srs_ind.sfn = %i\n", UL_INFO->srs_ind.sfn);
    LOG_I(NR_PHY, "UL_INFO->srs_ind.slot = %i\n", UL_INFO->srs_ind.slot);
    LOG_I(NR_PHY, "srs_indication->rnti = %04x\n", srs_indication->rnti);
    LOG_I(NR_PHY, "srs_indication->timing_advance = %i\n", srs_indication->timing_advance_offset);
    LOG_I(NR_PHY, "srs_indication->timing_advance_offset_nsec = %i\n", srs_indication->timing_advance_offset_nsec);
    LOG_I(NR_PHY, "srs_indication->srs_usage = %i\n", srs_indication->srs_usage);
    LOG_I(NR_PHY, "srs_indication->report_type = %i\n", srs_indication->report_type);
#endif

    nfapi_srs_report_tlv_t *report_tlv = &srs_indication->report_tlv;
    report_tlv->tag = 0;
    report_tlv->length = 0;

    start_meas(&gNB->srs_report_tlv_stats);
    switch (srs_indication->srs_usage) {
      case NFAPI_NR_SRS_BEAMMANAGEMENT: {
        start_meas(&gNB->srs_beam_report_stats);
        nfapi_nr_srs_beamforming_report_t nr_srs_bf_report;
        nr_srs_bf_report.prg_size = srs_pdu->beamforming.prg_size;
        nr_srs_bf_report.num_symbols = 1 << srs_pdu->num_symbols;
        nr_srs_bf_report.wide_band_snr =
            srs_est >= 0 ? (gNB->srs->snr + 64) << 1 : 0xFF; // 0xFF will be set if this field is invalid
        nr_srs_bf_report.num_reported_symbols = 1 << srs_pdu->num_symbols;
        AssertFatal(nr_srs_bf_report.num_reported_symbols == 1,
                    "nr_srs_bf_report.num_reported_symbols %i not handled yet!\n",
                    nr_srs_bf_report.num_reported_symbols);
        fill_srs_reported_symbol(&nr_srs_bf_report.reported_symbol_list[0], srs_pdu, frame_parms->N_RB_UL, snr_per_rb, srs_est);

#ifdef SRS_IND_DEBUG
        LOG_I(NR_PHY, "nr_srs_bf_report.prg_size = %i\n", nr_srs_bf_report.prg_size);
        LOG_I(NR_PHY, "nr_srs_bf_report.num_symbols = %i\n", nr_srs_bf_report.num_symbols);
        LOG_I(NR_PHY,
              "nr_srs_bf_report.wide_band_snr = %i (%i dB)\n",
              nr_srs_bf_report.wide_band_snr,
              (nr_srs_bf_report.wide_band_snr >> 1) - 64);
        LOG_I(NR_PHY, "nr_srs_bf_report.num_reported_symbols = %i\n", nr_srs_bf_report.num_reported_symbols);
        LOG_I(NR_PHY,
              "nr_srs_bf_report.reported_symbol_list[0].num_prgs = %i\n",
              nr_srs_bf_report.reported_symbol_list[0].num_prgs);
        for (int prg_idx = 0; prg_idx < nr_srs_bf_report.reported_symbol_list[0].num_prgs; prg_idx++) {
          LOG_I(NR_PHY,
                "nr_srs_beamforming_report.reported_symbol_list[0].prg_list[%3i].rb_snr = %i (%i dB)\n",
                prg_idx,
                nr_srs_bf_report.reported_symbol_list[0].prg_list[prg_idx].rb_snr,
                (nr_srs_bf_report.reported_symbol_list[0].prg_list[prg_idx].rb_snr >> 1) - 64);
        }
#endif

        report_tlv->length = pack_nr_srs_beamforming_report(&nr_srs_bf_report, report_tlv->value, sizeof(report_tlv->value));
        stop_meas(&gNB->srs_beam_report_stats);
        break;
      }

      case NFAPI_NR_SRS_CODEBOOK: {
        start_meas(&gNB->srs_iq_matrix_stats);
        nfapi_nr_srs_normalized_channel_iq_matrix_t nr_srs_channel_iq_matrix;
        fill_srs_channel_matrix(&nr_srs_channel_iq_matrix,
                                srs_pdu,
                                gNB->nr_srs_info[i],
                                srs_pdu->srs_parameters_v4.iq_representation,
                                ant,
                                srs_pdu->srs_parameters_v4.num_total_ue_antennas,
                                srs_pdu->srs_parameters_v4.prg_size,
                                srs_pdu->srs_parameters_v4.srs_bandwidth_size / srs_pdu->srs_parameters_v4.prg_size,
                                frame_parms,
                                srs_estimated_channel_freq);

#ifdef SRS_IND_DEBUG
        LOG_I(NR_PHY,
              "nr_srs_channel_iq_matrix.normalized_iq_representation = %i\n",
              nr_srs_channel_iq_matrix.normalized_iq_representation);
        LOG_I(NR_PHY,
              "nr_srs_channel_iq_matrix.num_gnb_antenna_elements = %i\n",
              nr_srs_channel_iq_matrix.num_gnb_antenna_elements);
        LOG_I(NR_PHY, "nr_srs_channel_iq_matrix.num_ue_srs_ports = %i\n", nr_srs_channel_iq_matrix.num_ue_srs_ports);
        LOG_I(NR_PHY, "nr_srs_channel_iq_matrix.prg_size = %i\n", nr_srs_channel_iq_matrix.prg_size);
        LOG_I(NR_PHY, "nr_srs_channel_iq_matrix.num_prgs = %i\n", nr_srs_channel_iq_matrix.num_prgs);
        c16_t *channel_matrix16 = (c16_t *)nr_srs_channel_iq_matrix.channel_matrix;
        c8_t *channel_matrix8 = (c8_t *)nr_srs_channel_iq_matrix.channel_matrix;
        for (int uI = 0; uI < nr_srs_channel_iq_matrix.num_ue_srs_ports; uI++) {
          for (int gI = 0; gI < nr_srs_channel_iq_matrix.num_gnb_antenna_elements; gI++) {
            for (int pI = 0; pI < nr_srs_channel_iq_matrix.num_prgs; pI++) {
              uint16_t index = uI * nr_srs_channel_iq_matrix.num_gnb_antenna_elements * nr_srs_channel_iq_matrix.num_prgs
                               + gI * nr_srs_channel_iq_matrix.num_prgs + pI;
              LOG_I(
                  NR_PHY,
                  "(uI %i, gI %i, pI %i) channel_matrix --> real %i, imag %i\n",
                  uI,
                  gI,
                  pI,
                  nr_srs_channel_iq_matrix.normalized_iq_representation == 0 ? channel_matrix8[index].r : channel_matrix16[index].r,
                  nr_srs_channel_iq_matrix.normalized_iq_representation == 0 ? channel_matrix8[index].i
                                                                             : channel_matrix16[index].i);
            }
          }
        }
#endif

        report_tlv->length =
            pack_nr_srs_normalized_channel_iq_matrix(&nr_srs_channel_iq_matrix, report_tlv->value, sizeof(report_tlv->value));
        stop_meas(&gNB->srs_iq_matrix_stats);
        break;
      }

      case NFAPI_NR_SRS_NONCODEBOOK:
      case NFAPI_NR_SRS_ANTENNASWITCH:
        LOG_W(NR_PHY, "PHY procedures for this SRS usage are not implemented yet!\n");
        break;

      default:
        AssertFatal(1 == 0, "Invalid SRS usage\n");
    }
    stop_meas(&gNB->srs_report_tlv_stats);

#ifdef SRS_IND_DEBUG
    LOG_I(NR_PHY, "report_tlv->tag = %i\n", report_tlv->tag);
    LOG_I(NR_PHY, "report_tlv->length = %i\n", report_tlv->length);
    char *value = (char *)report_tlv->value;
    for (int b = 0; b < report_tlv->length; b++) {
      LOG_I(NR_PHY, "value[%i] = 0x%02x\n", b, value[b] & 0xFF);
    }
#endif
    srs->active = false;
    stop_meas(&gNB->rx_srs_stats);
  }

  stop_meas(&gNB->phy_proc_rx);

  if (pucch_decode_done || pusch_decode_done) {
    T(T_GNB_PHY_PUCCH_PUSCH_IQ,
      T_INT(frame_rx),
      T_INT(slot_rx),
      T_BUFFER(&gNB->common_vars.rxdataF[0][0][0], nb_symb * symb_sz * 4));
  }

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_gNB_UESPEC_RX,0);
  return pusch_DTX;
}

void nr_save_ul_tti_req(PHY_VARS_gNB *gNB, nfapi_nr_ul_tti_request_t *UL_tti_req)
{
  DevAssert(gNB != NULL);
  DevAssert(UL_tti_req != NULL);

  int frame = UL_tti_req->SFN;
  int slot = UL_tti_req->Slot;

  for (int i = 0; i < UL_tti_req->n_pdus; i++) {
    int type = UL_tti_req->pdus_list[i].pdu_type;
    LOG_D(NR_PHY,
          "frame %d, slot %d got %s for %d.%d\n",
          frame,
          slot,
          txt_nfapi_nr_ul_config_pdu_type[type],
          UL_tti_req->SFN,
          UL_tti_req->Slot);
    switch (type) {
      case NFAPI_NR_UL_CONFIG_PUSCH_PDU_TYPE:
        nr_fill_ulsch(gNB, UL_tti_req->SFN, UL_tti_req->Slot, &UL_tti_req->pdus_list[i].pusch_pdu);
        break;
      case NFAPI_NR_UL_CONFIG_PUCCH_PDU_TYPE:
        nr_fill_pucch(gNB, UL_tti_req->SFN, UL_tti_req->Slot, &UL_tti_req->pdus_list[i].pucch_pdu);
        break;
      case NFAPI_NR_UL_CONFIG_PRACH_PDU_TYPE: {
        nfapi_nr_prach_pdu_t *prach_pdu = &UL_tti_req->pdus_list[i].prach_pdu;
        prach_item_t *prach = nr_schedule_rx_prach(gNB, UL_tti_req->SFN, UL_tti_req->Slot, prach_pdu);
        if (!prach)
          LOG_W(NR_PHY_RACH, "Error in scheduling rach\n");
      } break;
      case NFAPI_NR_UL_CONFIG_SRS_PDU_TYPE:
        nr_fill_srs(gNB, UL_tti_req->SFN, UL_tti_req->Slot, &UL_tti_req->pdus_list[i].srs_pdu);
        break;
    }
  }
}
