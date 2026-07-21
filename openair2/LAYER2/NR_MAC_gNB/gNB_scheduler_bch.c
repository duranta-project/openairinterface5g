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

/*! \file gNB_scheduler_bch.c
 * \brief procedures related to gNB for the BCH transport channel
 * \author  Navid Nikaein and Raymond Knopp, WEI-TAI CHEN
 * \date 2010 - 2014, 2018
 * \email: navid.nikaein@eurecom.fr, kroempa@gmail.com
 * \version 1.0
 * \company Eurecom, NTUST
 * @ingroup _mac

 */

#include "assertions.h"
#include "NR_MAC_gNB/nr_mac_gNB.h"
#include "NR_MAC_gNB/mac_proto.h"
#include "common/utils/LOG/log.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "UTIL/OPT/opt.h"
#include "common/utils/nr/nr_common.h"

#define ENABLE_MAC_PAYLOAD_DEBUG

#include "common/ran_context.h"

#include "executables/softmodem-common.h"

static void schedule_ssb(frame_t frame,
                         slot_t slot,
                         NR_ServingCellConfigCommon_t *scc,
                         nfapi_nr_dl_tti_request_body_t *dl_req,
                         int i_ssb,
                         int beam_index,
                         uint8_t scoffset,
                         uint16_t offset_pointa,
                         uint32_t payload)
{
  nfapi_nr_dl_tti_request_pdu_t *dl_config_pdu = &dl_req->dl_tti_pdu_list[dl_req->nPDUs];
  memset((void *) dl_config_pdu, 0, sizeof(nfapi_nr_dl_tti_request_pdu_t));
  dl_config_pdu->PDUType = NFAPI_NR_DL_TTI_SSB_PDU_TYPE;
  dl_config_pdu->PDUSize = 2 + sizeof(nfapi_nr_dl_tti_ssb_pdu_rel15_t);

  AssertFatal(scc->physCellId != NULL,"ServingCellConfigCommon->physCellId is null\n");
  AssertFatal(*scc->physCellId < 1008 && *scc->physCellId >=0, "5G physicall cell id out of range: %ld\n", *scc->physCellId);
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.PhysCellId = *scc->physCellId;
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.BetaPss = 0;
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.SsbBlockIndex = i_ssb;
  AssertFatal(scc->downlinkConfigCommon,"scc->downlinkConfigCommonL is null\n");
  AssertFatal(scc->downlinkConfigCommon->frequencyInfoDL,"scc->downlinkConfigCommon->frequencyInfoDL is null\n");
  AssertFatal(scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencySSB,
              "scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencySSB is null\n");
  AssertFatal(scc->downlinkConfigCommon->frequencyInfoDL->frequencyBandList.list.count == 1,
              "Frequency Band list does not have 1 element (%d)\n",
              scc->downlinkConfigCommon->frequencyInfoDL->frequencyBandList.list.count);
  AssertFatal(scc->ssbSubcarrierSpacing,"ssbSubcarrierSpacing is null\n");
  AssertFatal(scc->downlinkConfigCommon->frequencyInfoDL->frequencyBandList.list.array[0], "band is null\n");

  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.SsbSubcarrierOffset = scoffset; //kSSB
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.ssbOffsetPointA = offset_pointa;
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.bchPayloadFlag = 1;
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.bchPayload = payload;
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.precoding_and_beamforming.num_prgs = 1;
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.precoding_and_beamforming.prg_size = 20; //SSB is always 20RBs
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.precoding_and_beamforming.dig_bf_interfaces = 1;
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.precoding_and_beamforming.prgs_list[0].pm_idx = 0;
  dl_config_pdu->ssb_pdu.ssb_pdu_rel15.precoding_and_beamforming.prgs_list[0].dig_bf_interface_list[0].beam_idx = beam_index;
  dl_req->nPDUs++;

  LOG_D(MAC,"Scheduling ssb %d at frame %d and slot %d\n", i_ssb, frame, slot);
}

static void fill_ssb_vrb_map(NR_COMMON_channels_t *cc,
                             int rbStart,
                             int ssb_subcarrier_offset,
                             uint16_t symStart,
                             int CC_id,
                             int beam)
{
  AssertFatal(*cc->ServingCellConfigCommon->ssbSubcarrierSpacing !=
              NR_SubcarrierSpacing_kHz240,
              "240kHZ subcarrier won't work with current VRB map because a single SSB might be across 2 slots\n");

  uint16_t *vrb_map = cc[CC_id].vrb_map[beam];
  const int extra_prb = ssb_subcarrier_offset > 0;
  for (int rb = 0; rb < 20 + extra_prb; rb++)
    vrb_map[rbStart + rb] = SL_to_bitmap(symStart % NR_NUMBER_OF_SYMBOLS_PER_SLOT, 4);
}

static int encode_mib(NR_BCCH_BCH_Message_t *mib, frame_t frame, uint8_t *buffer, int buf_size)
{
  int encode_size = 3;
  AssertFatal(buf_size >= encode_size, "buffer of size %d too small, need 3 bytes\n", buf_size);
  int encoded = encode_MIB_NR(mib, frame, buffer, encode_size);
  DevAssert(encoded == encode_size);
  return encode_size;
}

static void schedule_one_ssb(gNB_MAC_INST *gNB,
                             nfapi_nr_dl_tti_request_body_t *dl_req,
                             const long band,
                             const NR_SubcarrierSpacing_t scs,
                             const int i_ssb,
                             const int rel_slot,
                             const frame_t frameP,
                             const slot_t slotP,
                             const int CC_id,
                             const uint8_t ssb_frame_periodicity,
                             const frequency_range_t FR)
{
  // if start symbol is in current slot, schedule current SSB, fill VRB map and call get_type0_PDCCH_CSS_config_parameters
  const int slots_per_frame = gNB->frame_structure.numb_slots_frame;
  const uint16_t offset_pointa = gNB->ssb_OffsetPointA;
  NR_COMMON_channels_t *cc = &gNB->common_channels[CC_id];
  NR_ServingCellConfigCommon_t *scc = cc->ServingCellConfigCommon;
  const NR_MIB_t *mib = cc->mib->message.choice.mib;
  uint32_t mib_pdu = (*(uint32_t *)cc->MIB_pdu) & ((1 << 24) - 1);
  uint8_t ssbSubcarrierOffset = gNB->ssb_SubcarrierOffset;

  uint16_t ssb_start_symbol = get_ssb_start_symbol(band, scs, i_ssb);
  NR_beam_alloc_t beam =
      beam_allocation_procedure(&gNB->beam_info, frameP, slotP, get_beam_from_ssbidx(gNB, i_ssb), slots_per_frame);
  AssertFatal(beam.idx >= 0, "Cannot allocate SSB %d in any available beam\n", i_ssb);
  const uint16_t beam_index =
      convert_to_fapi_beam(get_allocated_beam(&gNB->beam_info, frameP, slotP, slots_per_frame, beam.idx), gNB->beam_info.beam_mode);
  const int prb_offset = (FR == FR2) ? offset_pointa >> (scs - 2) : offset_pointa >> scs;
  schedule_ssb(frameP, slotP, scc, dl_req, i_ssb, beam_index, ssbSubcarrierOffset, offset_pointa, mib_pdu);
  fill_ssb_vrb_map(cc,
                   prb_offset,
                   (FR == FR2) ? ssbSubcarrierOffset >> (scs - 2) : ssbSubcarrierOffset,
                   ssb_start_symbol,
                   CC_id,
                   beam.idx);
  if (IS_SA_MODE(get_softmodem_params())) {
    get_type0_PDCCH_CSS_config_parameters(&gNB->type0_PDCCH_CSS_config[i_ssb],
                                          frameP,
                                          mib,
                                          slots_per_frame,
                                          ssbSubcarrierOffset,
                                          ssb_start_symbol,
                                          scs,
                                          FR,
                                          band,
                                          i_ssb,
                                          ssb_frame_periodicity,
                                          prb_offset);
    gNB->type0_PDCCH_CSS_config[i_ssb].active = true;
  }
}

void schedule_nr_mib(module_id_t module_idP, frame_t frameP, slot_t slotP, nfapi_nr_dl_tti_request_t *DL_req)
{
  gNB_MAC_INST *gNB = RC.nrmac[module_idP];
  /* already mutex protected: held in gNB_dlsch_ulsch_scheduler() */
  nfapi_nr_dl_tti_request_body_t *dl_req;

  for (int CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
    NR_COMMON_channels_t *cc = &gNB->common_channels[CC_id];
    NR_ServingCellConfigCommon_t *scc = cc->ServingCellConfigCommon;
    const int slots_per_frame = gNB->frame_structure.numb_slots_frame;
    dl_req = &DL_req->dl_tti_request_body;

    // get MIB every 8 frames
    if(((slotP == 0) && (frameP & 7) == 0) ||
       gNB->first_MIB) {
      int mib_sdu_length = encode_mib(cc->mib, frameP, cc->MIB_pdu, sizeof(cc->MIB_pdu));

      // flag to avoid sending an empty MIB in the first frames of execution since gNB doesn't get at the beginning in frame 0 slot 0
      gNB->first_MIB = false;

      LOG_D(MAC,
            "[gNB %d] Frame %d : MIB->BCH  CC_id %d, Received %d bytes\n",
            module_idP,
            frameP,
            CC_id,
            mib_sdu_length);
    }

    int8_t ssb_period = *scc->ssb_periodicityServingCell;
    uint8_t ssb_frame_periodicity = 1;  // every how many frames SSB are generated

    if (ssb_period > 1) // 0 is every half frame
      ssb_frame_periodicity = 1 << (ssb_period -1);

    if (!(frameP % ssb_frame_periodicity) && ((slotP < (slots_per_frame >> 1)) || (ssb_period == 0))) {
      // schedule SSB only for given frames according to SSB periodicity
      // and in first half frame unless periodicity of 5ms
      int rel_slot;
      if (ssb_period == 0) // scheduling every half frame
        rel_slot = slotP % (slots_per_frame >> 1);
      else
        rel_slot = slotP;

      NR_SubcarrierSpacing_t scs = *scc->ssbSubcarrierSpacing;
      const long band = *scc->downlinkConfigCommon->frequencyInfoDL->frequencyBandList.list.array[0];

      const BIT_STRING_t *shortBitmap = &scc->ssb_PositionsInBurst->choice.shortBitmap;
      const BIT_STRING_t *mediumBitmap = &scc->ssb_PositionsInBurst->choice.mediumBitmap;
      const BIT_STRING_t *longBitmap = &scc->ssb_PositionsInBurst->choice.longBitmap;

      switch (scc->ssb_PositionsInBurst->present) {
        case 1:
          // short bitmap (<3GHz) max 4 SSBs
          for (int i_ssb = 0; i_ssb < 4; i_ssb++) {
            if ((shortBitmap->buf[0] >> (7 - i_ssb)) & 0x01) {
              uint16_t ssb_start_symbol = get_ssb_start_symbol(band, scs, i_ssb);
              if ((ssb_start_symbol / 14) == rel_slot)
                schedule_one_ssb(gNB, dl_req, band, scs, i_ssb, rel_slot, frameP, slotP, CC_id, ssb_frame_periodicity, FR1);
            }
          }
          break;
        case 2:
          // medium bitmap (<6GHz) max 8 SSBs
          for (int i_ssb = 0; i_ssb < 8; i_ssb++) {
            if ((mediumBitmap->buf[0] >> (7 - i_ssb)) & 0x01) {
              uint16_t ssb_start_symbol = get_ssb_start_symbol(band, scs, i_ssb);
              if ((ssb_start_symbol / 14) == rel_slot)
                schedule_one_ssb(gNB, dl_req, band, scs, i_ssb, rel_slot, frameP, slotP, CC_id, ssb_frame_periodicity, FR1);
            }
          }
          break;
        case 3:
          // long bitmap FR2 max 64 SSBs
          for (int i_ssb = 0; i_ssb < 64; i_ssb++) {
            if ((longBitmap->buf[i_ssb / 8] >> (7 - (i_ssb % 8))) & 0x01) {
              uint16_t ssb_start_symbol = get_ssb_start_symbol(band, scs, i_ssb);
              if ((ssb_start_symbol / 14) == rel_slot)
                schedule_one_ssb(gNB, dl_req, band, scs, i_ssb, rel_slot, frameP, slotP, CC_id, ssb_frame_periodicity, FR2);
            }
          }
          break;
        default:
          AssertFatal(0,"SSB bitmap size value %d undefined (allowed values 1,2,3)\n",
                      scc->ssb_PositionsInBurst->present);
      }
    }
  }
}

static bool update_rb_mcs_tbs(NR_sched_pdsch_t *pdsch, uint32_t num_total_bytes, uint16_t *vrb_map)
{
  const NR_tda_info_t *tda_info = &pdsch->tda_info;

  // Calculate number of PRB_DMRS
  uint8_t N_PRB_DMRS = pdsch->dmrs_parms.N_PRB_DMRS;
  LOG_D(MAC, "dlDmrsSymbPos %x\n", pdsch->dmrs_parms.dl_dmrs_symb_pos);
  int mcsTableIdx = 0;
  const uint16_t slbitmap = SL_to_bitmap(tda_info->startSymbolIndex, tda_info->nrOfSymbols);
  int bwpSize = pdsch->bwp_info.bwpSize;
  int bwpStart = pdsch->bwp_info.bwpStart;
  int rbStop = bwpSize - 1;
  while (pdsch->rbStart < rbStop) {
    if (vrb_map[pdsch->rbStart + bwpStart] & slbitmap)
      pdsch->rbStart++;
    else {
      int max_rbSize = 0;
      while (pdsch->rbStart + max_rbSize <= rbStop && !(vrb_map[pdsch->rbStart + max_rbSize + bwpStart] & slbitmap))
        max_rbSize++;

      bool res = false;
      while (res == false && pdsch->mcs < 10) {
        pdsch->Qm = nr_get_Qm_dl(pdsch->mcs, mcsTableIdx);
        pdsch->R = nr_get_code_rate_dl(pdsch->mcs, mcsTableIdx);
        res = nr_find_nb_rb(pdsch->Qm,
                            pdsch->R,
                            1, // no transform precoding for DL
                            1, // single layer
                            tda_info->nrOfSymbols,
                            pdsch->dmrs_parms.N_PRB_DMRS * pdsch->dmrs_parms.N_DMRS_SLOT,
                            num_total_bytes,
                            1, // min_rbSize
                            max_rbSize,
                            &pdsch->tb_size,
                            &pdsch->rbSize);
        if (!res)
          pdsch->mcs++;
      }
      break;
    }
  }
  if (pdsch->tb_size < num_total_bytes) {
    LOG_D(NR_MAC,
          "Couldn't allocate enough resources for %d bytes in SIB PDSCH (rbStart %d, rbSize %d, bwpSize %d)\n",
          num_total_bytes,
          pdsch->rbStart,
          pdsch->rbSize,
          bwpSize);
    return false;
  }

  LOG_D(NR_MAC,
        "mcs=%i, startSymbolIndex = %i, nrOfSymbols = %i, rbSize = %i, TBS = %i, dmrs_length %d, N_PRB_DMRS = %d, mappingtype = %d\n",
        pdsch->mcs,
        tda_info->startSymbolIndex,
        tda_info->nrOfSymbols,
        pdsch->rbSize,
        pdsch->tb_size,
        pdsch->dmrs_parms.N_DMRS_SLOT,
        N_PRB_DMRS,
        tda_info->mapping_type);
  return true;
}

static NR_sched_pdsch_t schedule_control_sib1(gNB_MAC_INST *gNB_mac,
                                              int CC_id,
                                              NR_sched_pdcch_t *pdcch,
                                              NR_Type0_PDCCH_CSS_config_t *type0_PDCCH_CSS_config,
                                              int time_domain_allocation,
                                              NR_pdsch_dmrs_t *dmrs_parms,
                                              NR_tda_info_t *tda_info,
                                              uint8_t candidate_idx,
                                              int beam,
                                              uint16_t num_total_bytes)
{
  AssertFatal(gNB_mac->sched_ctrlCommon, "sched_ctrlCommon is NULL\n");
  NR_COMMON_channels_t *cc = &gNB_mac->common_channels[CC_id];
  uint16_t *vrb_map = cc->vrb_map[beam];
  NR_sched_pdsch_t pdsch = {
      .bwp_info = get_pdsch_bwp_start_size(gNB_mac, NULL),
      .time_domain_allocation = time_domain_allocation,
      .dmrs_parms = *dmrs_parms,
      .tda_info = *tda_info,
      .nrOfLayers = 1,
      .pm_index = 0,
      .mcs = 0, // starting from mcs 0
  };
  gNB_mac->sched_ctrlCommon->num_total_bytes = num_total_bytes;

  uint8_t nr_of_candidates = 0;

  for (int i = 0; i < 3; i++) {
    find_aggregation_candidates(&gNB_mac->sched_ctrlCommon->aggregation_level, &nr_of_candidates, gNB_mac->sched_ctrlCommon->search_space,4<<i);
    if (nr_of_candidates > 0)
      break; // choosing the lower value of aggregation level available
  }
  AssertFatal(nr_of_candidates > 0, "nr_of_candidates is 0\n");
  gNB_mac->sched_ctrlCommon->cce_index = find_pdcch_candidate(gNB_mac,
                                                              CC_id,
                                                              gNB_mac->sched_ctrlCommon->aggregation_level,
                                                              nr_of_candidates,
                                                              beam,
                                                              pdcch,
                                                              gNB_mac->sched_ctrlCommon->coreset,
                                                              0);

  AssertFatal(gNB_mac->sched_ctrlCommon->cce_index >= 0, "Could not find CCE for coreset0\n");

  bool success = update_rb_mcs_tbs(&pdsch, gNB_mac->sched_ctrlCommon->num_total_bytes, vrb_map);
  if (success) {
    // Mark the corresponding RBs as used
    fill_pdcch_vrb_map(gNB_mac,
                       CC_id,
                       pdcch,
                       gNB_mac->sched_ctrlCommon->cce_index,
                       gNB_mac->sched_ctrlCommon->aggregation_level,
                       beam);
    for (int rb = 0; rb < pdsch.rbSize; rb++) {
      vrb_map[rb + type0_PDCCH_CSS_config->cset_start_rb] |= SL_to_bitmap(tda_info->startSymbolIndex, tda_info->nrOfSymbols);
    }
  } else {
    pdsch.tb_size = 0; // to signal we couldn't allocate TBS
    pdsch.time_domain_allocation = -1;
  }

  return pdsch;
}

static void nr_fill_nfapi_dl_SIB_pdu(gNB_MAC_INST *gNB_mac,
                                     NR_sched_pdsch_t *pdsch,
                                     NR_sched_pdcch_t *pdcch,
                                     NR_SearchSpace_t *search_space,
                                     NR_ControlResourceSet_t *coreset,
                                     int aggregation_level,
                                     int cce_index,
                                     nfapi_nr_dl_tti_request_body_t *dl_req,
                                     int pdu_index,
                                     NR_Type0_PDCCH_CSS_config_t *type0_PDCCH_CSS_config,
                                     bool is_sib1,
                                     int beam_index)
{
  NR_COMMON_channels_t *cc = gNB_mac->common_channels;
  NR_ServingCellConfigCommon_t *scc = cc->ServingCellConfigCommon;

  nfapi_nr_dl_tti_request_pdu_t *dl_tti_pdcch_pdu = &dl_req->dl_tti_pdu_list[dl_req->nPDUs];
  memset(dl_tti_pdcch_pdu, 0, sizeof(*dl_tti_pdcch_pdu));
  dl_tti_pdcch_pdu->PDUType = NFAPI_NR_DL_TTI_PDCCH_PDU_TYPE;
  dl_tti_pdcch_pdu->PDUSize = (uint16_t)(4+sizeof(nfapi_nr_dl_tti_pdcch_pdu));
  dl_req->nPDUs += 1;
  nfapi_nr_dl_tti_pdcch_pdu_rel15_t *pdcch_pdu_rel15 = &dl_tti_pdcch_pdu->pdcch_pdu.pdcch_pdu_rel15;
  nr_configure_pdcch(pdcch_pdu_rel15, coreset, pdcch);

  nfapi_nr_dl_tti_request_pdu_t *dl_tti_pdsch_pdu = &dl_req->dl_tti_pdu_list[dl_req->nPDUs];
  memset(dl_tti_pdsch_pdu, 0, sizeof(*dl_tti_pdcch_pdu));
  dl_tti_pdsch_pdu->PDUType = NFAPI_NR_DL_TTI_PDSCH_PDU_TYPE;
  dl_tti_pdsch_pdu->PDUSize = (uint16_t)(4+sizeof(nfapi_nr_dl_tti_pdsch_pdu));
  dl_req->nPDUs += 1;

  const uint16_t fapi_beam = convert_to_fapi_beam(beam_index, gNB_mac->beam_info.beam_mode);
  nfapi_nr_dl_tti_pdsch_pdu_rel15_t *pdsch_pdu_rel15 =
      prepare_pdsch_pdu(dl_tti_pdsch_pdu, gNB_mac, NULL, pdsch, NULL, is_sib1, 0, SI_RNTI, fapi_beam, 1, pdu_index);
  LOG_D(NR_MAC,
        "OtherSI:bwpStart %d, bwpSize %d, rbStart %d, rbSize %d, dlDmrsSymbPos = 0x%x\n",
        pdsch_pdu_rel15->BWPStart,
        pdsch_pdu_rel15->BWPSize,
        pdsch_pdu_rel15->rbStart,
        pdsch_pdu_rel15->rbSize,
        pdsch_pdu_rel15->dlDmrsSymbPos);

  /* Fill PDCCH DL DCI PDU */
  nfapi_nr_dl_dci_pdu_t *dci_pdu =
      prepare_dci_pdu(pdcch_pdu_rel15, scc, search_space, coreset, aggregation_level, cce_index, fapi_beam, SI_RNTI);
  pdcch_pdu_rel15->numDlDci++;

  /* DCI payload */
  int dci_format = NR_DL_DCI_FORMAT_1_0;
  int rnti_type = TYPE_SI_RNTI_;
  dci_pdu_rel15_t dci_payload = prepare_dci_dl_payload(gNB_mac,
                                                       NULL,
                                                       rnti_type,
                                                       NR_SearchSpace__searchSpaceType_PR_common,
                                                       pdsch_pdu_rel15,
                                                       pdsch,
                                                       NULL,
                                                       0,
                                                       0,
                                                       is_sib1);

  fill_dci_pdu_rel15(NULL,
                     NULL,
                     NULL,
                     dci_pdu,
                     &dci_payload,
                     dci_format,
                     rnti_type,
                     search_space,
                     coreset,
                     0,
                     gNB_mac->cset0_bwp_size);

  LOG_D(MAC,
        "BWPSize: %3i, BWPStart: %3i, SubcarrierSpacing: %i, CyclicPrefix: %i, StartSymbolIndex: %i, DurationSymbols: %i, "
        "CceRegMappingType: %i, RegBundleSize: %i, InterleaverSize: %i, CoreSetType: %i, ShiftIndex: %i, precoderGranularity: %i, "
        "numDlDci: %i\n",
        pdcch_pdu_rel15->BWPSize,
        pdcch_pdu_rel15->BWPStart,
        pdcch_pdu_rel15->SubcarrierSpacing,
        pdcch_pdu_rel15->CyclicPrefix,
        pdcch_pdu_rel15->StartSymbolIndex,
        pdcch_pdu_rel15->DurationSymbols,
        pdcch_pdu_rel15->CceRegMappingType,
        pdcch_pdu_rel15->RegBundleSize,
        pdcch_pdu_rel15->InterleaverSize,
        pdcch_pdu_rel15->CoreSetType,
        pdcch_pdu_rel15->ShiftIndex,
        pdcch_pdu_rel15->precoderGranularity,
        pdcch_pdu_rel15->numDlDci);
}

static bool check_sib1_tda(gNB_MAC_INST *gNB_mac,
                           NR_sched_pdcch_t *sched_pdcch,
                           NR_beam_alloc_t *beam,
                           int tda,
                           int ssb_index,
                           int CC_id,
                           int candidate_idx)
{
  NR_COMMON_channels_t *cc = &gNB_mac->common_channels[CC_id];
  NR_ServingCellConfigCommon_t *scc = cc->ServingCellConfigCommon;
  NR_Type0_PDCCH_CSS_config_t *type0_PDCCH_CSS_config = &gNB_mac->type0_PDCCH_CSS_config[ssb_index];
  default_table_type_t table_type = get_default_table_type(type0_PDCCH_CSS_config->type0_pdcch_ss_mux_pattern);
  // If the PDSCH was scheduled with SI-RNTI in PDCCH Type0 common search space,
  // the UE may assume that this PDSCH resource allocation is not applied (Table 5.1.2.1.1-4 and 5 in 38.214)
  int row = tda + 1;
  if (table_type == defaultB && (row == 12 || row == 13 || row == 14))
    false;
  if (table_type == defaultC && (row == 1 || row == 6 || row == 7 ||row > 12))
    false;
  // assuming normal CP
  NR_tda_info_t tda_info = get_info_from_tda_tables(table_type, tda, scc->dmrs_TypeA_Position, true);
  // SIB1 TDA going to the last symbol would cause overlap with CSI-RS
  if (!tda_info.valid_tda || tda_info.startSymbolIndex + tda_info.nrOfSymbols == 14)
    false;
  if (tda_info.startSymbolIndex < type0_PDCCH_CSS_config->first_symbol_index + type0_PDCCH_CSS_config->num_symbols)
    false;

  NR_pdsch_dmrs_t dmrs_parms = get_dl_dmrs_params(scc, NULL, &tda_info, 1);
  // Configure sched_ctrlCommon for SIB1
  gNB_mac->sib1_pdsch[ssb_index] = schedule_control_sib1(gNB_mac,
                                                         CC_id,
                                                         sched_pdcch,
                                                         type0_PDCCH_CSS_config,
                                                         tda,
                                                         &dmrs_parms,
                                                         &tda_info,
                                                         candidate_idx,
                                                         beam->idx,
                                                         cc->sib1_bcch_length);
  if (gNB_mac->sib1_pdsch[ssb_index].tb_size > 0) {
    LOG_D(NR_MAC, "Found TDA for SIB1: %d\n", tda);
    return true;
  } else {
    return false;
  }
}

void schedule_nr_sib1(module_id_t module_idP,
                      frame_t frameP,
                      slot_t slotP,
                      nfapi_nr_dl_tti_request_t *DL_req,
                      nfapi_nr_tx_data_request_t *TX_req)
{
  /* already mutex protected: held in gNB_dlsch_ulsch_scheduler() */
  // TODO: Get these values from RRC
  const int CC_id = 0;
  uint8_t candidate_idx = 0;
  gNB_MAC_INST *gNB_mac = RC.nrmac[module_idP];
  NR_COMMON_channels_t *cc = &gNB_mac->common_channels[CC_id];
  NR_ServingCellConfigCommon_t *scc = cc->ServingCellConfigCommon;
  int L_max;
  switch (scc->ssb_PositionsInBurst->present) {
    case 1:
      L_max = 4;
      break;
    case 2:
      L_max = 8;
      break;
    case 3:
      L_max = 64;
      break;
    default:
      AssertFatal(false, "SSB bitmap size value %d undefined (allowed values 1,2,3)\n", scc->ssb_PositionsInBurst->present);
  }

  for (int i = 0; i < L_max; i++) {

    NR_Type0_PDCCH_CSS_config_t *type0_PDCCH_CSS_config = &gNB_mac->type0_PDCCH_CSS_config[i];

    if((frameP % 2 == type0_PDCCH_CSS_config->sfn_c) &&
       (slotP == type0_PDCCH_CSS_config->n_0) &&
       (type0_PDCCH_CSS_config->num_rbs > 0) &&
       (type0_PDCCH_CSS_config->active == true)) {

      AssertFatal(is_dl_slot(slotP, &gNB_mac->frame_structure),
                  "Trying to schedule SIB1 in slot %d which is not DL. Check searchSpaceZero configuration.\n",
                  slotP);
      const int n_slots_frame = gNB_mac->frame_structure.numb_slots_frame;
      int beam_index = get_beam_from_ssbidx(gNB_mac, i);
      NR_beam_alloc_t beam = beam_allocation_procedure(&gNB_mac->beam_info, frameP, slotP, beam_index, n_slots_frame);
      AssertFatal(beam.idx >= 0, "Cannot allocate SIB1 corresponding to SSB %d in any available beam\n", i);
      LOG_D(NR_MAC,"(%d.%d) SIB1 transmission: ssb_index %d\n", frameP, slotP, type0_PDCCH_CSS_config->ssb_index);
      NR_sched_pdcch_t sched_pdcch = set_pdcch_structure(NULL,
                                                         gNB_mac->sched_ctrlCommon->search_space,
                                                         gNB_mac->sched_ctrlCommon->coreset,
                                                         scc,
                                                         NULL,
                                                         type0_PDCCH_CSS_config);

      bool res = false;
      if (gNB_mac->sib1_pdsch[i].time_domain_allocation < 0) {
        for (int t = 0; t < 16; t++) {
          if (check_sib1_tda(gNB_mac, &sched_pdcch, &beam, t, i, CC_id, candidate_idx)) {
            res = true;
            break;
          }
        }
      }
      AssertFatal(gNB_mac->sib1_pdsch[i].time_domain_allocation >= 0, "Couldn't select any TDA for SIB1\n");
      if (!res)
        res = check_sib1_tda(gNB_mac, &sched_pdcch, &beam, gNB_mac->sib1_pdsch[i].time_domain_allocation, i, CC_id, candidate_idx);
      int tb_size = gNB_mac->sib1_pdsch[i].tb_size;
      AssertFatal(res && tb_size > 0, "Couldn't allocate TB for SIB1 for an already allocated TDA\n");
      nfapi_nr_dl_tti_request_body_t *dl_req = &DL_req->dl_tti_request_body;
      int pdu_index = gNB_mac->pdu_index[0]++;
      nr_fill_nfapi_dl_SIB_pdu(gNB_mac,
                               &gNB_mac->sib1_pdsch[i],
                               &sched_pdcch,
                               gNB_mac->sched_ctrlCommon->search_space,
                               gNB_mac->sched_ctrlCommon->coreset,
                               gNB_mac->sched_ctrlCommon->aggregation_level,
                               gNB_mac->sched_ctrlCommon->cce_index,
                               dl_req,
                               pdu_index,
                               type0_PDCCH_CSS_config,
                               true,
                               beam_index);

      const int ntx_req = TX_req->Number_of_PDUs;
      nfapi_nr_pdu_t *tx_req = &TX_req->pdu_list[ntx_req];

      // Data to be transmitted
      memcpy(tx_req->TLVs[0].value.direct, cc->sib1_bcch_pdu, tb_size);

      tx_req->PDU_index  = pdu_index;
      tx_req->num_TLV = 1;
      tx_req->TLVs[0].tag = 0;
      tx_req->TLVs[0].length = tb_size;
      tx_req->PDU_length = compute_PDU_length(tx_req->num_TLV, tx_req->TLVs[0].length);
      TX_req->Number_of_PDUs++;
      TX_req->SFN = frameP;
      TX_req->Slot = slotP;

      type0_PDCCH_CSS_config->active = false;

      T(T_GNB_MAC_DL_PDU_WITH_DATA,
        T_INT(module_idP),
        T_INT(CC_id),
        T_INT(0xffff),
        T_INT(frameP),
        T_INT(slotP),
        T_INT(0 /* harq_pid */),
        T_BUFFER(cc->sib1_bcch_pdu, cc->sib1_bcch_length));
    }
  }
}

struct NR_SchedulingInfo2_r17 *find_sib19_sched_info(const struct NR_SI_SchedulingInfo_v1700 *si_schedulinginfo2_r17)
{
  for (int i = 0; i < si_schedulinginfo2_r17->schedulingInfoList2_r17.list.count; i++) {
    const struct NR_SIB_Mapping_v1700 *sib_MappingInfo_r17 =
        &si_schedulinginfo2_r17->schedulingInfoList2_r17.list.array[i]->sib_MappingInfo_r17;
    for (int j = 0; j < sib_MappingInfo_r17->list.count; j++) {
      if (sib_MappingInfo_r17->list.array[j]->sibType_r17.choice.type1_r17
          == NR_SIB_TypeInfo_v1700__sibType_r17__type1_r17_sibType19) {
        return si_schedulinginfo2_r17->schedulingInfoList2_r17.list.array[i];
      }
    }
  }
  return NULL;
}

static void other_sib_sched_control(module_id_t module_idP,
                                    frame_t frame,
                                    slot_t slot,
                                    int beam_index,
                                    NR_SearchSpace_t *ss,
                                    nfapi_nr_dl_tti_request_t *DL_req,
                                    nfapi_nr_tx_data_request_t *TX_req,
                                    int payload_idx)
{
  gNB_MAC_INST *gNB_mac = RC.nrmac[module_idP];
  AssertFatal(is_dl_slot(slot, &gNB_mac->frame_structure),
              "Trying to schedule otherSIB in slot %d which is not DL. Wrong Configuration\n",
              slot);
  NR_ServingCellConfigCommon_t *scc = gNB_mac->common_channels[0].ServingCellConfigCommon;
  int n_slots_frame = gNB_mac->frame_structure.numb_slots_frame;
  beam_index = get_beam_from_ssbidx(gNB_mac, beam_index);
  NR_beam_alloc_t beam = beam_allocation_procedure(&gNB_mac->beam_info, frame, slot, beam_index, n_slots_frame);
  AssertFatal(beam.idx >= 0, "Cannot allocate otherSIB corresponding for SSB number %d in any available beam\n", beam_index);
  LOG_D(NR_MAC, "(%d.%d) otherSIB payload %d transmission for ssb number %d\n", frame, slot, payload_idx, beam_index);

  NR_COMMON_channels_t *cc = &gNB_mac->common_channels[0];
  NR_Type0_PDCCH_CSS_config_t *type0_PDCCH_CSS_config = &gNB_mac->type0_PDCCH_CSS_config[cc->ssb_index[beam_index]];
  AssertFatal(gNB_mac->sched_ctrlCommon, "sched_ctrlCommon is NULL\n");
  NR_PDSCH_ConfigCommon_t *pdsch_ConfigCommon = scc->downlinkConfigCommon->initialDownlinkBWP->pdsch_ConfigCommon->choice.setup;
  int time_domain_allocation = 1;
  NR_tda_info_t tda_info = set_tda_info_from_list(pdsch_ConfigCommon->pdsch_TimeDomainAllocationList, time_domain_allocation);
  LOG_D(NR_MAC,"tda_info.startSymbolIndex: %d, tda_info.nrOfSymbols: %d\n", tda_info.startSymbolIndex, tda_info.nrOfSymbols);

  NR_pdsch_dmrs_t dmrs_parms = get_dl_dmrs_params(scc, NULL, &tda_info, 1);

  uint8_t aggregation_level = 0;
  uint8_t nr_of_candidates = 0;
  for (int i = 0; i < 5; i++) {
    find_aggregation_candidates(&aggregation_level, &nr_of_candidates, ss, 16 >> i);
    if (nr_of_candidates > 0)
      break; // choosing the higher value of aggregation level available
  }

  AssertFatal(nr_of_candidates > 0, "nr_of_candidates is 0\n");
  AssertFatal(ss->controlResourceSetId, "ss->controlResourceSetId is NULL\n");
  NR_ControlResourceSet_t *coreset = get_coreset(gNB_mac, scc, NULL, *ss->controlResourceSetId);
  if (!gNB_mac->sched_pdcch_otherSI) {
    gNB_mac->sched_pdcch_otherSI = calloc(1, sizeof(*gNB_mac->sched_pdcch_otherSI));
    *gNB_mac->sched_pdcch_otherSI = set_pdcch_structure(gNB_mac, ss, coreset, scc, NULL, type0_PDCCH_CSS_config);
  }
  int cce_index = find_pdcch_candidate(gNB_mac,
                                       0,
                                       aggregation_level,
                                       nr_of_candidates,
                                       beam.idx,
                                       gNB_mac->sched_pdcch_otherSI,
                                       coreset,
                                       0);

  AssertFatal(cce_index >= 0, "Could not find CCE for otherSIB DCI\n");

  // Mark the corresponding RBs as used
  fill_pdcch_vrb_map(gNB_mac, 0, gNB_mac->sched_pdcch_otherSI, cce_index, aggregation_level, beam.idx);

  NR_sched_pdsch_t sched_pdsch_otherSI = {0};
  sched_pdsch_otherSI.time_domain_allocation = time_domain_allocation;
  sched_pdsch_otherSI.bwp_info = get_pdsch_bwp_start_size(gNB_mac, NULL);
  sched_pdsch_otherSI.dmrs_parms = dmrs_parms;
  sched_pdsch_otherSI.tda_info = tda_info;
  sched_pdsch_otherSI.nrOfLayers = 1;
  sched_pdsch_otherSI.pm_index = 0;
  sched_pdsch_otherSI.mcs = 0; // starting from mcs 0

  uint16_t *vrb_map = cc->vrb_map[beam.idx];
  uint8_t *sib_bcch_pdu = cc->other_sib_bcch_pdu[payload_idx];
  int num_total_bytes = cc->other_sib_bcch_length[payload_idx];
  bool success = update_rb_mcs_tbs(&sched_pdsch_otherSI, num_total_bytes, vrb_map);
  AssertFatal(success, "Couldn't allocate TBS for other SIB\n");

  for (int rb = 0; rb < sched_pdsch_otherSI.rbSize; rb++) {
    vrb_map[rb + type0_PDCCH_CSS_config->cset_start_rb] |= SL_to_bitmap(tda_info.startSymbolIndex, tda_info.nrOfSymbols);
  }

  int pdu_index = gNB_mac->pdu_index[0]++;
  nfapi_nr_dl_tti_request_body_t *dl_req = &DL_req->dl_tti_request_body;
  nr_fill_nfapi_dl_SIB_pdu(gNB_mac,
                           &sched_pdsch_otherSI,
                           gNB_mac->sched_pdcch_otherSI,
                           ss,
                           coreset,
                           aggregation_level,
                           cce_index,
                           dl_req,
                           pdu_index,
                           type0_PDCCH_CSS_config,
                           false,
                           beam_index);

  const int ntx_req = TX_req->Number_of_PDUs;
  nfapi_nr_pdu_t *tx_req = &TX_req->pdu_list[ntx_req];

  // Data to be transmitted
  memcpy(tx_req->TLVs[0].value.direct, sib_bcch_pdu, sched_pdsch_otherSI.tb_size);

  tx_req->PDU_index = pdu_index;
  tx_req->num_TLV = 1;
  tx_req->TLVs[0].tag = 0;
  tx_req->TLVs[0].length = sched_pdsch_otherSI.tb_size;
  tx_req->PDU_length = compute_PDU_length(tx_req->num_TLV, tx_req->TLVs[0].length);
  TX_req->Number_of_PDUs++;
  TX_req->SFN = frame;
  TX_req->Slot = slot;

  T(T_GNB_MAC_DL_PDU_WITH_DATA,
    T_INT(module_idP),
    T_INT(0),
    T_INT(SI_RNTI),
    T_INT(frame),
    T_INT(slot),
    T_INT(0), // harq_pid
    T_BUFFER(sib_bcch_pdu, num_total_bytes));
}

// This function verifies if we need to schedule otherSIB in frame and slot
static bool test_other_sib_sched_occasion(int window_pos,
                                          int window_len,
                                          int period,
                                          int n_slots_frame,
                                          int frame,
                                          int slot,
                                          int rel_frame,
                                          int rel_slot)
{
  int x = (window_pos - 1) * window_len;
  int T = 8 << period;
  int test_frame = (frame - rel_frame) % MAX_FRAME_NUMBER;
  int si_slot = (x % n_slots_frame) + rel_slot;
  bool res = ((test_frame % T) != (x / n_slots_frame)) || (si_slot != slot);
  return res;
}

void schedule_nr_other_sib(module_id_t module_idP,
                           frame_t frame,
                           slot_t slot,
                           nfapi_nr_dl_tti_request_t *DL_req,
                           nfapi_nr_tx_data_request_t *TX_req)
{
  gNB_MAC_INST *gNB_mac = RC.nrmac[module_idP];
  NR_COMMON_channels_t *cc = &gNB_mac->common_channels[0];
  NR_SIB1_t *sib1 = cc->sib1->message.choice.c1->choice.systemInformationBlockType1;
  NR_SI_SchedulingInfo_t *schedInfo = sib1->si_SchedulingInfo;
  if (!schedInfo)
    return;

  NR_ServingCellConfigCommon_t *scc = gNB_mac->common_channels[0].ServingCellConfigCommon;
  NR_PDCCH_ConfigCommon_t *pdcch_common = scc->downlinkConfigCommon->initialDownlinkBWP->pdcch_ConfigCommon->choice.setup;
  NR_SearchSpaceId_t *ss_id = pdcch_common->searchSpaceOtherSystemInformation;
  AssertFatal(ss_id, "searchSpaceOtherSystemInformation not present\n");
  NR_SearchSpace_t *ss = NULL;
  for (int i = 0; i < pdcch_common->commonSearchSpaceList->list.count; i++) {
    if (pdcch_common->commonSearchSpaceList->list.array[i]->searchSpaceId == *ss_id)
      ss = pdcch_common->commonSearchSpaceList->list.array[i];
  }
  AssertFatal(ss, "searchSpaceOtherSystemInformation not found\n");

  NR_SI_SchedulingInfo_v1700_t *schedInfo17 = NULL;
  if (sib1->nonCriticalExtension) {
    NR_SIB1_v1610_IEs_t *sib1_v1610 = sib1->nonCriticalExtension;
    if (sib1_v1610 && sib1_v1610->nonCriticalExtension && sib1_v1610->nonCriticalExtension->nonCriticalExtension) {
      NR_SIB1_v1700_IEs_t *sib1_v17 = sib1_v1610->nonCriticalExtension->nonCriticalExtension;
      schedInfo17 = sib1_v17->si_SchedulingInfo_v1700;
    }
  }

  int n_slots_frame = gNB_mac->frame_structure.numb_slots_frame;
  int window_length_sl = 5 << schedInfo->si_WindowLength;
  int window_length_f = (window_length_sl / n_slots_frame) + ((window_length_sl % n_slots_frame) > 0);
  int num_ssb = cc->num_active_ssb;
  // number of PDCCH monitoring occasions in SI-window
  int period, offset;
  get_monitoring_period_offset(ss, &period, &offset);
  // The UE assumes that, in the SI window, PDCCH for an SI message is transmitted
  // in at least one PDCCH monitoring occasion corresponding to each transmitted SSB
  // frame and slot relative to start of window
  // The [x×N+K]th PDCCH monitoring occasion(s) for SI message in SI-window corresponds to the Kth transmitted SSB
  // in our implmentation we transmit only once per SSB per window so N = 1
  // Section 5.2.2.3.2 of 38.331
  int temp_slot = offset % n_slots_frame;
  int temp_frame = offset / n_slots_frame;
  int rel_slot[num_ssb];
  int rel_frame[num_ssb];
  int ssb = 0;
  while (ssb < num_ssb) {
    AssertFatal(temp_frame < window_length_f, "Couldn't fit %d SSB in window length of %d slots\n", num_ssb, window_length_sl);
    if (is_dl_slot(temp_slot, &gNB_mac->frame_structure)) {
      rel_slot[ssb] = temp_slot;
      rel_frame[ssb] = temp_frame;
      ssb++;
    }
    temp_frame += (temp_slot + period) / n_slots_frame;
    temp_slot = (temp_slot + period) % n_slots_frame;
  }

  for (int ssb = 0; ssb < num_ssb; ssb++) {
    for (int i = 0; i < schedInfo->schedulingInfoList.list.count; i++) {
      NR_SchedulingInfo_t *schedulingInfo = schedInfo->schedulingInfoList.list.array[i];
      if (test_other_sib_sched_occasion(i + 1,
                                        window_length_sl,
                                        schedulingInfo->si_Periodicity,
                                        n_slots_frame,
                                        frame,
                                        slot,
                                        rel_frame[ssb],
                                        rel_slot[ssb]))
        continue;

      other_sib_sched_control(module_idP, frame, slot, ssb, ss, DL_req, TX_req, 0);
    }
    if (!schedInfo17)
      continue;
    for (int i = 0; i < schedInfo17->schedulingInfoList2_r17.list.count; i++) {
      NR_SchedulingInfo2_r17_t *schedulingInfo2_r17 = schedInfo17->schedulingInfoList2_r17.list.array[i];
      if (test_other_sib_sched_occasion(schedulingInfo2_r17->si_WindowPosition_r17,
                                        window_length_sl,
                                        schedulingInfo2_r17->si_Periodicity_r17,
                                        n_slots_frame,
                                        frame,
                                        slot,
                                        rel_frame[ssb],
                                        rel_slot[ssb]))
        continue;

      other_sib_sched_control(module_idP, frame, slot, ssb, ss, DL_req, TX_req, 1);
    }
  }
}
