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

/*! \file       gNB_scheduler_dlsch.c
 * \brief       procedures related to gNB for the DLSCH transport channel
 * \author      Guido Casati
 * \date        2019
 * \email:      guido.casati@iis.fraunhofe.de
 * \version     1.0
 * @ingroup     _mac

 */

#include "common/utils/nr/nr_common.h"
/*MAC*/
#include "NR_MAC_COMMON/nr_mac.h"
#include "NR_MAC_gNB/nr_mac_gNB.h"
#include "LAYER2/NR_MAC_gNB/mac_proto.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h"

/*TAG*/
#include "NR_TAG-Id.h"

/*Softmodem params*/
#include "executables/softmodem-common.h"
#include "../../../nfapi/oai_integration/vendor_ext.h"

////////////////////////////////////////////////////////
/////* DLSCH MAC PDU generation (6.1.2 TS 38.321) */////
////////////////////////////////////////////////////////
#define OCTET 8
#define HALFWORD 16
#define WORD 32
//#define SIZE_OF_POINTER sizeof (void *)

#define MAX_NUM_DATA_REQ 1024

int get_dl_tda(const gNB_MAC_INST *nrmac, int slot)
{
  /* we assume that this function is mutex-protected from outside */
  const frame_structure_t *fs = &nrmac->frame_structure;

  // Use special TDA in case of CSI-RS
  if (nrmac->UE_info.sched_csirs > 0)
    return 1;

  if (fs->frame_type == TDD) {
    int s = get_slot_idx_in_period(slot, fs);
    // if there is a mixed slot where we can transmit DL
    const tdd_bitmap_t *tdd_slot_bitmap = fs->period_cfg.tdd_slot_bitmap;
    if (tdd_slot_bitmap[s].num_dl_symbols > 1 && is_mixed_slot(s, fs)) {
      return 2;
    }
  }
  return 0; // if FDD or not mixed slot in TDD, for now use default TDA
}

// Compute and write all MAC CEs and subheaders, and return number of written
// bytes
int nr_write_ce_dlsch_pdu(module_id_t module_idP,
                          const NR_UE_sched_ctrl_t *ue_sched_ctl,
                          unsigned char *mac_pdu,
                          unsigned char drx_cmd,
                          unsigned char *ue_cont_res_id)
{
  gNB_MAC_INST *gNB = RC.nrmac[module_idP];
  /* already mutex protected: called below and in _RA.c */
  NR_SCHED_ENSURE_LOCKED(&gNB->sched_lock);

  NR_MAC_SUBHEADER_FIXED *mac_pdu_ptr = (NR_MAC_SUBHEADER_FIXED *) mac_pdu;
  uint8_t last_size = 0;
  int offset = 0, mac_ce_size, i, timing_advance_cmd, tag_id = 0;
  // MAC CEs
  uint8_t mac_header_control_elements[16], *ce_ptr;
  ce_ptr = &mac_header_control_elements[0];

  // DRX command subheader (MAC CE size 0)
  if (drx_cmd != 255) {
    mac_pdu_ptr->R = 0;
    mac_pdu_ptr->LCID = DL_SCH_LCID_DRX;
    //last_size = 1;
    mac_pdu_ptr++;
  }

  // Timing Advance subheader
  /* This was done only when timing_advance_cmd != 31
  // now TA is always send when ta_timer resets regardless of its value
  // this is done to avoid issues with the timeAlignmentTimer which is
  // supposed to monitor if the UE received TA or not */
  if (ue_sched_ctl->ta_apply) {
    mac_pdu_ptr->R = 0;
    mac_pdu_ptr->LCID = DL_SCH_LCID_TA_COMMAND;
    //last_size = 1;
    mac_pdu_ptr++;
    // TA MAC CE (1 octet)
    timing_advance_cmd = ue_sched_ctl->ta_update;
    AssertFatal(timing_advance_cmd < 64, "timing_advance_cmd %d > 63\n", timing_advance_cmd);
    ((NR_MAC_CE_TA *) ce_ptr)->TA_COMMAND = timing_advance_cmd;    //(timing_advance_cmd+31)&0x3f;

    tag_id = gNB->tag->tag_Id;
    ((NR_MAC_CE_TA *) ce_ptr)->TAGID = tag_id;

    LOG_D(NR_MAC, "NR MAC CE timing advance command = %d (%d) TAG ID = %d\n", timing_advance_cmd, ((NR_MAC_CE_TA *) ce_ptr)->TA_COMMAND, tag_id);
    mac_ce_size = sizeof(NR_MAC_CE_TA);
    // Copying  bytes for MAC CEs to the mac pdu pointer
    memcpy((void *) mac_pdu_ptr, (void *) ce_ptr, mac_ce_size);
    ce_ptr += mac_ce_size;
    mac_pdu_ptr += (unsigned char) mac_ce_size;
  }

  // Contention resolution fixed subheader and MAC CE
  if (ue_cont_res_id) {
    mac_pdu_ptr->R = 0;
    mac_pdu_ptr->LCID = DL_SCH_LCID_CON_RES_ID;
    mac_pdu_ptr++;
    //last_size = 1;
    // contention resolution identity MAC ce has a fixed 48 bit size
    // this contains the UL CCCH SDU. If UL CCCH SDU is longer than 48 bits,
    // it contains the first 48 bits of the UL CCCH SDU
    LOG_D(NR_MAC,
          "[gNB ][RAPROC] Generate contention resolution msg: %x.%x.%x.%x.%x.%x\n",
          ue_cont_res_id[0],
          ue_cont_res_id[1],
          ue_cont_res_id[2],
          ue_cont_res_id[3],
          ue_cont_res_id[4],
          ue_cont_res_id[5]);
    // Copying bytes (6 octects) to CEs pointer
    mac_ce_size = 6;
    memcpy(ce_ptr, ue_cont_res_id, mac_ce_size);
    // Copying bytes for MAC CEs to mac pdu pointer
    memcpy((void *) mac_pdu_ptr, (void *) ce_ptr, mac_ce_size);
    ce_ptr += mac_ce_size;
    mac_pdu_ptr += (unsigned char) mac_ce_size;
  }

  //TS 38.321 Sec 6.1.3.15 TCI State indication for UE Specific PDCCH MAC CE SubPDU generation
  if (ue_sched_ctl->UE_mac_ce_ctrl.pdcch_state_ind.is_scheduled) {
    //filling subheader
    mac_pdu_ptr->R = 0;
    mac_pdu_ptr->LCID = DL_SCH_LCID_TCI_STATE_IND_UE_SPEC_PDCCH;
    mac_pdu_ptr++;
    //Creating the instance of CE structure
    NR_TCI_PDCCH  nr_UESpec_TCI_StateInd_PDCCH;
    //filling the CE structre
    nr_UESpec_TCI_StateInd_PDCCH.CoresetId1 = ((ue_sched_ctl->UE_mac_ce_ctrl.pdcch_state_ind.coresetId) & 0xF) >> 1; //extracting MSB 3 bits from LS nibble
    nr_UESpec_TCI_StateInd_PDCCH.ServingCellId = (ue_sched_ctl->UE_mac_ce_ctrl.pdcch_state_ind.servingCellId) & 0x1F; //extracting LSB 5 Bits
    nr_UESpec_TCI_StateInd_PDCCH.TciStateId = (ue_sched_ctl->UE_mac_ce_ctrl.pdcch_state_ind.tciStateId) & 0x7F; //extracting LSB 7 bits
    nr_UESpec_TCI_StateInd_PDCCH.CoresetId2 = (ue_sched_ctl->UE_mac_ce_ctrl.pdcch_state_ind.coresetId) & 0x1; //extracting LSB 1 bit
    LOG_D(NR_MAC, "NR MAC CE TCI state indication for UE Specific PDCCH = %d \n", nr_UESpec_TCI_StateInd_PDCCH.TciStateId);
    mac_ce_size = sizeof(NR_TCI_PDCCH);
    // Copying  bytes for MAC CEs to the mac pdu pointer
    memcpy((void *) mac_pdu_ptr, (void *)&nr_UESpec_TCI_StateInd_PDCCH, mac_ce_size);
    //incrementing the PDU pointer
    mac_pdu_ptr += (unsigned char) mac_ce_size;
  }

  //TS 38.321 Sec 6.1.3.16, SP CSI reporting on PUCCH Activation/Deactivation MAC CE
  if (ue_sched_ctl->UE_mac_ce_ctrl.SP_CSI_reporting_pucch.is_scheduled) {
    //filling the subheader
    mac_pdu_ptr->R = 0;
    mac_pdu_ptr->LCID = DL_SCH_LCID_SP_CSI_REP_PUCCH_ACT;
    mac_pdu_ptr++;
    //creating the instance of CE structure
    NR_PUCCH_CSI_REPORTING nr_PUCCH_CSI_reportingActDeact;
    //filling the CE structure
    nr_PUCCH_CSI_reportingActDeact.BWP_Id = (ue_sched_ctl->UE_mac_ce_ctrl.SP_CSI_reporting_pucch.bwpId) & 0x3; //extracting LSB 2 bibs
    nr_PUCCH_CSI_reportingActDeact.ServingCellId = (ue_sched_ctl->UE_mac_ce_ctrl.SP_CSI_reporting_pucch.servingCellId) & 0x1F; //extracting LSB 5 bits
    nr_PUCCH_CSI_reportingActDeact.S0 = ue_sched_ctl->UE_mac_ce_ctrl.SP_CSI_reporting_pucch.s0tos3_actDeact[0];
    nr_PUCCH_CSI_reportingActDeact.S1 = ue_sched_ctl->UE_mac_ce_ctrl.SP_CSI_reporting_pucch.s0tos3_actDeact[1];
    nr_PUCCH_CSI_reportingActDeact.S2 = ue_sched_ctl->UE_mac_ce_ctrl.SP_CSI_reporting_pucch.s0tos3_actDeact[2];
    nr_PUCCH_CSI_reportingActDeact.S3 = ue_sched_ctl->UE_mac_ce_ctrl.SP_CSI_reporting_pucch.s0tos3_actDeact[3];
    nr_PUCCH_CSI_reportingActDeact.R2 = 0;
    mac_ce_size = sizeof(NR_PUCCH_CSI_REPORTING);
    // Copying MAC CE data to the mac pdu pointer
    memcpy((void *) mac_pdu_ptr, (void *)&nr_PUCCH_CSI_reportingActDeact, mac_ce_size);
    //incrementing the PDU pointer
    mac_pdu_ptr += (unsigned char) mac_ce_size;
  }

  //TS 38.321 Sec 6.1.3.14, TCI State activation/deactivation for UE Specific PDSCH MAC CE
  if (ue_sched_ctl->UE_mac_ce_ctrl.pdsch_TCI_States_ActDeact.is_scheduled) {
    //Computing the number of octects to be allocated for Flexible array member
    //of MAC CE structure
    uint8_t num_octects = (ue_sched_ctl->UE_mac_ce_ctrl.pdsch_TCI_States_ActDeact.highestTciStateActivated) / 8 + 1; //Calculating the number of octects for allocating the memory
    //filling the subheader
    ((NR_MAC_SUBHEADER_SHORT *) mac_pdu_ptr)->R = 0;
    ((NR_MAC_SUBHEADER_SHORT *) mac_pdu_ptr)->F = 0;
    ((NR_MAC_SUBHEADER_SHORT *) mac_pdu_ptr)->LCID = DL_SCH_LCID_TCI_STATE_ACT_UE_SPEC_PDSCH;
    ((NR_MAC_SUBHEADER_SHORT *) mac_pdu_ptr)->L = sizeof(NR_TCI_PDSCH_APERIODIC_CSI) + num_octects * sizeof(uint8_t);
    last_size = 2;
    //Incrementing the PDU pointer
    mac_pdu_ptr += last_size;
    //allocating memory for CE Structure
    NR_TCI_PDSCH_APERIODIC_CSI *nr_UESpec_TCI_StateInd_PDSCH = (NR_TCI_PDSCH_APERIODIC_CSI *)malloc(sizeof(NR_TCI_PDSCH_APERIODIC_CSI) + num_octects * sizeof(uint8_t));
    //initializing to zero
    memset((void *)nr_UESpec_TCI_StateInd_PDSCH, 0, sizeof(NR_TCI_PDSCH_APERIODIC_CSI) + num_octects * sizeof(uint8_t));
    //filling the CE Structure
    nr_UESpec_TCI_StateInd_PDSCH->BWP_Id = (ue_sched_ctl->UE_mac_ce_ctrl.pdsch_TCI_States_ActDeact.bwpId) & 0x3; //extracting LSB 2 Bits
    nr_UESpec_TCI_StateInd_PDSCH->ServingCellId = (ue_sched_ctl->UE_mac_ce_ctrl.pdsch_TCI_States_ActDeact.servingCellId) & 0x1F; //extracting LSB 5 bits

    for(i = 0; i < (num_octects * 8); i++) {
      if(ue_sched_ctl->UE_mac_ce_ctrl.pdsch_TCI_States_ActDeact.tciStateActDeact[i])
        nr_UESpec_TCI_StateInd_PDSCH->T[i / 8] = nr_UESpec_TCI_StateInd_PDSCH->T[i / 8] | (1 << (i % 8));
    }

    mac_ce_size = sizeof(NR_TCI_PDSCH_APERIODIC_CSI) + num_octects * sizeof(uint8_t);
    //Copying  bytes for MAC CEs to the mac pdu pointer
    memcpy((void *) mac_pdu_ptr, (void *)nr_UESpec_TCI_StateInd_PDSCH, mac_ce_size);
    //incrementing the mac pdu pointer
    mac_pdu_ptr += (unsigned char) mac_ce_size;
    //freeing the allocated memory
    free(nr_UESpec_TCI_StateInd_PDSCH);
  }

  //TS38.321 Sec 6.1.3.13 Aperiodic CSI Trigger State Subselection MAC CE
  if (ue_sched_ctl->UE_mac_ce_ctrl.aperi_CSI_trigger.is_scheduled) {
    //Computing the number of octects to be allocated for Flexible array member
    //of MAC CE structure
    uint8_t num_octects = (ue_sched_ctl->UE_mac_ce_ctrl.aperi_CSI_trigger.highestTriggerStateSelected) / 8 + 1; //Calculating the number of octects for allocating the memory
    //filling the subheader
    ((NR_MAC_SUBHEADER_SHORT *) mac_pdu_ptr)->R = 0;
    ((NR_MAC_SUBHEADER_SHORT *) mac_pdu_ptr)->F = 0;
    ((NR_MAC_SUBHEADER_SHORT *) mac_pdu_ptr)->LCID = DL_SCH_LCID_APERIODIC_CSI_TRI_STATE_SUBSEL;
    ((NR_MAC_SUBHEADER_SHORT *) mac_pdu_ptr)->L = sizeof(NR_TCI_PDSCH_APERIODIC_CSI) + num_octects * sizeof(uint8_t);
    last_size = 2;
    //Incrementing the PDU pointer
    mac_pdu_ptr += last_size;
    //allocating memory for CE structure
    NR_TCI_PDSCH_APERIODIC_CSI *nr_Aperiodic_CSI_Trigger = (NR_TCI_PDSCH_APERIODIC_CSI *)malloc(sizeof(NR_TCI_PDSCH_APERIODIC_CSI) + num_octects * sizeof(uint8_t));
    //initializing to zero
    memset((void *)nr_Aperiodic_CSI_Trigger, 0, sizeof(NR_TCI_PDSCH_APERIODIC_CSI) + num_octects * sizeof(uint8_t));
    //filling the CE Structure
    nr_Aperiodic_CSI_Trigger->BWP_Id = (ue_sched_ctl->UE_mac_ce_ctrl.aperi_CSI_trigger.bwpId) & 0x3; //extracting LSB 2 bits
    nr_Aperiodic_CSI_Trigger->ServingCellId = (ue_sched_ctl->UE_mac_ce_ctrl.aperi_CSI_trigger.servingCellId) & 0x1F; //extracting LSB 5 bits
    nr_Aperiodic_CSI_Trigger->R = 0;

    for(i = 0; i < (num_octects * 8); i++) {
      if(ue_sched_ctl->UE_mac_ce_ctrl.aperi_CSI_trigger.triggerStateSelection[i])
        nr_Aperiodic_CSI_Trigger->T[i / 8] = nr_Aperiodic_CSI_Trigger->T[i / 8] | (1 << (i % 8));
    }

    mac_ce_size = sizeof(NR_TCI_PDSCH_APERIODIC_CSI) + num_octects * sizeof(uint8_t);
    // Copying  bytes for MAC CEs to the mac pdu pointer
    memcpy((void *) mac_pdu_ptr, (void *)nr_Aperiodic_CSI_Trigger, mac_ce_size);
    //incrementing the mac pdu pointer
    mac_pdu_ptr += (unsigned char) mac_ce_size;
    //freeing the allocated memory
    free(nr_Aperiodic_CSI_Trigger);
  }

  if (ue_sched_ctl->UE_mac_ce_ctrl.sp_zp_csi_rs.is_scheduled) {
    ((NR_MAC_SUBHEADER_FIXED *) mac_pdu_ptr)->R = 0;
    ((NR_MAC_SUBHEADER_FIXED *) mac_pdu_ptr)->LCID = DL_SCH_LCID_SP_ZP_CSI_RS_RES_SET_ACT;
    mac_pdu_ptr++;
    ((NR_MAC_CE_SP_ZP_CSI_RS_RES_SET *) mac_pdu_ptr)->A_D = ue_sched_ctl->UE_mac_ce_ctrl.sp_zp_csi_rs.act_deact;
    ((NR_MAC_CE_SP_ZP_CSI_RS_RES_SET *) mac_pdu_ptr)->CELLID = ue_sched_ctl->UE_mac_ce_ctrl.sp_zp_csi_rs.serv_cell_id & 0x1F; //5 bits
    ((NR_MAC_CE_SP_ZP_CSI_RS_RES_SET *) mac_pdu_ptr)->BWPID = ue_sched_ctl->UE_mac_ce_ctrl.sp_zp_csi_rs.bwpid & 0x3; //2 bits
    ((NR_MAC_CE_SP_ZP_CSI_RS_RES_SET *) mac_pdu_ptr)->CSIRS_RSC_ID = ue_sched_ctl->UE_mac_ce_ctrl.sp_zp_csi_rs.rsc_id & 0xF; //4 bits
    ((NR_MAC_CE_SP_ZP_CSI_RS_RES_SET *) mac_pdu_ptr)->R = 0;
    LOG_D(NR_MAC, "NR MAC CE of ZP CSIRS Serv cell ID = %d BWPID= %d Rsc set ID = %d\n", ue_sched_ctl->UE_mac_ce_ctrl.sp_zp_csi_rs.serv_cell_id, ue_sched_ctl->UE_mac_ce_ctrl.sp_zp_csi_rs.bwpid,
          ue_sched_ctl->UE_mac_ce_ctrl.sp_zp_csi_rs.rsc_id);
    mac_ce_size = sizeof(NR_MAC_CE_SP_ZP_CSI_RS_RES_SET);
    mac_pdu_ptr += (unsigned char) mac_ce_size;
  }

  if (ue_sched_ctl->UE_mac_ce_ctrl.csi_im.is_scheduled) {
    mac_pdu_ptr->R = 0;
    mac_pdu_ptr->LCID = DL_SCH_LCID_SP_CSI_RS_CSI_IM_RES_SET_ACT;
    mac_pdu_ptr++;
    CSI_RS_CSI_IM_ACT_DEACT_MAC_CE csi_rs_im_act_deact_ce;
    csi_rs_im_act_deact_ce.A_D = ue_sched_ctl->UE_mac_ce_ctrl.csi_im.act_deact;
    csi_rs_im_act_deact_ce.SCID = ue_sched_ctl->UE_mac_ce_ctrl.csi_im.serv_cellid & 0x3F;//gNB_PHY -> ssb_pdu.ssb_pdu_rel15.PhysCellId;
    csi_rs_im_act_deact_ce.BWP_ID = ue_sched_ctl->UE_mac_ce_ctrl.csi_im.bwp_id;
    csi_rs_im_act_deact_ce.R1 = 0;
    csi_rs_im_act_deact_ce.IM = ue_sched_ctl->UE_mac_ce_ctrl.csi_im.im;// IF set CSI IM Rsc id will presesent else CSI IM RSC ID is abscent
    csi_rs_im_act_deact_ce.SP_CSI_RSID = ue_sched_ctl->UE_mac_ce_ctrl.csi_im.nzp_csi_rsc_id;

    if ( csi_rs_im_act_deact_ce.IM ) { //is_scheduled if IM is 1 else this field will not present
      csi_rs_im_act_deact_ce.R2 = 0;
      csi_rs_im_act_deact_ce.SP_CSI_IMID = ue_sched_ctl->UE_mac_ce_ctrl.csi_im.csi_im_rsc_id;
      mac_ce_size = sizeof ( csi_rs_im_act_deact_ce ) - sizeof ( csi_rs_im_act_deact_ce.TCI_STATE );
    } else {
      mac_ce_size = sizeof ( csi_rs_im_act_deact_ce ) - sizeof ( csi_rs_im_act_deact_ce.TCI_STATE ) - 1;
    }

    memcpy ((void *) mac_pdu_ptr, (void *) & ( csi_rs_im_act_deact_ce), mac_ce_size);
    mac_pdu_ptr += (unsigned char) mac_ce_size;

    if (csi_rs_im_act_deact_ce.A_D ) { //Following IE is_scheduled only if A/D is 1
      mac_ce_size = sizeof ( struct TCI_S);

      for ( i = 0; i < ue_sched_ctl->UE_mac_ce_ctrl.csi_im.nb_tci_resource_set_id; i++) {
        csi_rs_im_act_deact_ce.TCI_STATE.R = 0;
        csi_rs_im_act_deact_ce.TCI_STATE.TCI_STATE_ID = ue_sched_ctl->UE_mac_ce_ctrl.csi_im.tci_state_id [i] & 0x7F;
        memcpy ((void *) mac_pdu_ptr, (void *) & (csi_rs_im_act_deact_ce.TCI_STATE), mac_ce_size);
        mac_pdu_ptr += (unsigned char) mac_ce_size;
      }
    }
  }

  // compute final offset
  offset = ((unsigned char *) mac_pdu_ptr - mac_pdu);
  //printf("Offset %d \n", ((unsigned char *) mac_pdu_ptr - mac_pdu));
  return offset;
}

static uint32_t update_dlsch_buffer(frame_t frame, slot_t slot, NR_UE_info_t *UE)
{
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  sched_ctrl->num_total_bytes = 0;
  sched_ctrl->dl_pdus_total = 0;

  logical_chan_id_t ch[NR_MAX_NUM_LCID] = {0};
  int n = 0;
  /* loop over all activated logical channels */
  FOR_EACH_SEQ_ARR(const nr_lc_config_t *, c, &sched_ctrl->lc_config ) {
    logical_chan_id_t lcid = c->lcid;
    if (c->suspended || (lcid == DL_SCH_LCID_DTCH && nr_timer_is_active(&sched_ctrl->transm_interrupt))) {
      memset(&sched_ctrl->rlc_status[lcid], 0, sizeof(sched_ctrl->rlc_status[lcid]));
      continue;
    }
    ch[n++] = lcid;
  }

  mac_rlc_status_resp_t ret[NR_MAX_NUM_LCID] = {0};
  nr_mac_rlc_status_ind(UE->rnti, frame, n, ch, ret);

  for (int i = 0; i < n; ++i) {
    logical_chan_id_t lcid = ch[i];

    if (ret[i].bytes_in_buffer == 0)
      continue;

    sched_ctrl->rlc_status[lcid] = ret[i];
    sched_ctrl->dl_pdus_total += sched_ctrl->rlc_status[lcid].pdus_in_buffer;
    sched_ctrl->num_total_bytes += sched_ctrl->rlc_status[lcid].bytes_in_buffer;
    LOG_D(MAC,
          "%4d.%2d UE %04x LCID %d status: %d bytes, %d PDUs, total buffer %d bytes %d PDUs\n",
          frame,
          slot,
          UE->rnti,
          lcid,
          ret[i].bytes_in_buffer,
          ret[i].pdus_in_buffer,
          sched_ctrl->num_total_bytes,
          sched_ctrl->dl_pdus_total);
  }
  return sched_ctrl->num_total_bytes;
}

void finish_nr_dl_harq(NR_UE_sched_ctrl_t *sched_ctrl, int harq_pid)
{
  NR_UE_harq_t *harq = &sched_ctrl->harq_processes[harq_pid];

  harq->ndi ^= 1;
  harq->round = 0;

  add_tail_nr_list(&sched_ctrl->available_dl_harq, harq_pid);
}

void abort_nr_dl_harq(NR_UE_info_t* UE, int8_t harq_pid)
{
  /* already mutex protected through handle_dl_harq() */
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;

  finish_nr_dl_harq(sched_ctrl, harq_pid);
  UE->mac_stats.dl.errors++;
}

bwp_info_t get_pdsch_bwp_start_size(gNB_MAC_INST *nr_mac, NR_UE_info_t *UE)
{
  bwp_info_t bwp_info;
  if (!UE) {
    bwp_info.bwpStart = nr_mac->cset0_bwp_start;
    bwp_info.bwpSize = nr_mac->cset0_bwp_size;
    return bwp_info;
  }
  NR_UE_DL_BWP_t *dl_bwp = &UE->current_DL_BWP;
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  // 3GPP TS 38.214 Section 5.1.2.2 Resource allocation in frequency domain
  // For a PDSCH scheduled with a DCI format 1_0 in any type of PDCCH common search space, regardless of which bandwidth part is the
  // active bandwidth part, RB numbering starts from the lowest RB of the CORESET in which the DCI was received; otherwise RB
  // numbering starts from the lowest RB in the determined downlink bandwidth part.
  //
  // 3GPP TS 38.214 Section 5.1.2.2.2 Downlink resource allocation type 1
  // In downlink resource allocation of type 1, the resource block assignment information indicates to a scheduled UE a set of
  // contiguously allocated non-interleaved or interleaved virtual resource blocks within the active bandwidth part of size   PRBs
  // except for the case when DCI format 1_0 is decoded in any common search space in which case the size of CORESET 0 shall be
  // used if CORESET 0 is configured for the cell and the size of initial DL bandwidth part shall be used if CORESET 0 is not
  // configured for the cell.
  if (dl_bwp->dci_format == NR_DL_DCI_FORMAT_1_0
      && sched_ctrl->search_space->searchSpaceType
      && sched_ctrl->search_space->searchSpaceType->present == NR_SearchSpace__searchSpaceType_PR_common) {
    if (sched_ctrl->coreset->controlResourceSetId == 0) {
      bwp_info.bwpStart = nr_mac->cset0_bwp_start;
    } else {
      int additional_offset = (dl_bwp->BWPStart + 5) / 6 * 6 - dl_bwp->BWPStart;
      bwp_info.bwpStart = dl_bwp->BWPStart + sched_ctrl->sched_pdcch.rb_start + additional_offset;
    }
    if (nr_mac->cset0_bwp_size > 0) {
      bwp_info.bwpSize = min(dl_bwp->BWPSize, nr_mac->cset0_bwp_size);
    } else {
      bwp_info.bwpSize = min(dl_bwp->BWPSize, UE->sc_info.initial_dl_BWPSize);
    }
  } else {
    bwp_info.bwpSize = dl_bwp->BWPSize;
    bwp_info.bwpStart = dl_bwp->BWPStart;
  }
  return bwp_info;
}

static bool allocate_dl_retransmission(gNB_MAC_INST *nr_mac,
                                       post_process_pdsch_t *pp_pdsch,
                                       int *n_rb_sched,
                                       NR_UE_info_t *UE,
                                       int beam_idx,
                                       int current_harq_pid)
{
  frame_t frame = pp_pdsch->frame;
  slot_t slot = pp_pdsch->slot;
  int CC_id = 0;

  const NR_ServingCellConfigCommon_t *scc = nr_mac->common_channels->ServingCellConfigCommon;
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  NR_UE_DL_BWP_t *dl_bwp = &UE->current_DL_BWP;
  NR_UE_UL_BWP_t *ul_bwp = &UE->current_UL_BWP;
  NR_sched_pdsch_t new_sched = sched_ctrl->harq_processes[current_harq_pid].sched_pdsch;
  int layers = get_dl_nrOfLayers(sched_ctrl, dl_bwp->dci_format);
  int pm_index = get_pm_index(nr_mac, UE, dl_bwp->dci_format, layers, nr_mac->radio_config.pdsch_AntennaPorts.XP);

  // If the RI changed between current rtx and a previous transmission
  // we need to verify if it is not decreased
  // othwise it wouldn't be possible to transmit the same TBS
  layers = (layers < new_sched.nrOfLayers) ? layers : new_sched.nrOfLayers;
  pm_index = (layers < new_sched.nrOfLayers) ? pm_index : new_sched.pm_index;

  const int coresetid = sched_ctrl->coreset->controlResourceSetId;
  const int tda = get_dl_tda(nr_mac, slot);
  AssertFatal(tda >= 0,"Unable to find PDSCH time domain allocation in list\n");

  /* Check first whether the old TDA can be reused
  * this helps allocate retransmission when TDA changes (e.g. new nrOfSymbols > old nrOfSymbols) */
  NR_tda_info_t temp_tda = get_dl_tda_info(dl_bwp,
                                           sched_ctrl->search_space->searchSpaceType->present,
                                           tda,
                                           scc->dmrs_TypeA_Position,
                                           1,
                                           TYPE_C_RNTI_,
                                           coresetid,
                                           false);
  if (!temp_tda.valid_tda)
    return false;

  bool reuse_old_tda = (new_sched.tda_info.startSymbolIndex == temp_tda.startSymbolIndex) && (new_sched.tda_info.nrOfSymbols <= temp_tda.nrOfSymbols);
  LOG_D(NR_MAC, "[UE %x] %s old TDA, %s number of layers\n",
        UE->rnti,
        reuse_old_tda ? "reuse" : "do not reuse",
        layers == new_sched.nrOfLayers ? "same" : "different");

  uint16_t *rballoc_mask = nr_mac->common_channels[CC_id].vrb_map[beam_idx];

  bwp_info_t bwp_info = get_pdsch_bwp_start_size(nr_mac, UE);
  int rbStart = bwp_info.bwpStart;
  int rbStop = bwp_info.bwpStart + bwp_info.bwpSize - 1;
  int rbSize = 0;

  if (reuse_old_tda && layers == new_sched.nrOfLayers) {
    /* Check that there are enough resources for retransmission */
    while (rbSize < new_sched.rbSize) {
      rbStart += rbSize; /* last iteration rbSize was not enough, skip it */
      rbSize = 0;

      const uint16_t slbitmap = SL_to_bitmap(new_sched.tda_info.startSymbolIndex, new_sched.tda_info.nrOfSymbols);
      while (rbStart < rbStop && (rballoc_mask[rbStart] & slbitmap))
        rbStart++;

      if (rbStart >= rbStop) {
        LOG_D(NR_MAC, "[UE %04x][%4d.%2d] could not allocate DL retransmission: no resources\n", UE->rnti, frame, slot);
        return false;
      }

      while (rbStart + rbSize <= rbStop && !(rballoc_mask[rbStart + rbSize] & slbitmap) && rbSize < new_sched.rbSize)
        rbSize++;
      DevAssert(rbSize > 0);
    }
  } else {
    /* the retransmission will use a different time domain allocation, check
     * that we have enough resources */
    NR_pdsch_dmrs_t temp_dmrs = get_dl_dmrs_params(scc, dl_bwp, &temp_tda, layers);

    const uint16_t slbitmap = SL_to_bitmap(temp_tda.startSymbolIndex, temp_tda.nrOfSymbols);
    while (rbStart < rbStop && (rballoc_mask[rbStart] & slbitmap))
      rbStart++;

    if (rbStart >= rbStop) {
      LOG_D(NR_MAC, "[UE %04x][%4d.%2d] could not allocate DL retransmission: no resources\n", UE->rnti, frame, slot);
      return false;
    }

    while (rbStart + rbSize <= rbStop && !(rballoc_mask[rbStart + rbSize] & slbitmap))
      rbSize++;
    DevAssert(rbSize > 0);

    uint32_t new_tbs;
    uint16_t new_rbSize;
    bool success = nr_find_nb_rb(new_sched.Qm,
                                 new_sched.R,
                                 1, // no transform precoding for DL
                                 layers,
                                 temp_tda.nrOfSymbols,
                                 temp_dmrs.N_PRB_DMRS * temp_dmrs.N_DMRS_SLOT,
                                 new_sched.tb_size,
                                 1, /* minimum of 1RB: need to find exact TBS, don't preclude any number */
                                 rbSize,
                                 &new_tbs,
                                 &new_rbSize);

    if (!success || new_tbs != new_sched.tb_size) {
      LOG_D(NR_MAC, "[UE %04x][%4d.%2d] allocation of DL retransmission failed: new TBS %d of new TDA does not match old TBS %d\n",
            UE->rnti,
            frame,
            slot,
            new_tbs,
            new_sched.tb_size);
      return false; /* the maximum TBsize we might have is smaller than what we need */
    }

    /* we can allocate it. Overwrite the time_domain_allocation, the number
     * of RBs, and the new TB size. The rest is done below */
    new_sched.tb_size = new_tbs;
    new_sched.rbSize = new_rbSize;
    new_sched.time_domain_allocation = tda;
    new_sched.nrOfLayers = layers;
    new_sched.pm_index = pm_index;
    new_sched.dmrs_parms = temp_dmrs;
    new_sched.tda_info = temp_tda;
  }

  /* Find a free CCE */
  int CCEIndex = get_cce_index(nr_mac,
                               CC_id,
                               slot,
                               UE->rnti,
                               &sched_ctrl->aggregation_level,
                               beam_idx,
                               sched_ctrl->search_space,
                               sched_ctrl->coreset,
                               &sched_ctrl->sched_pdcch,
                               sched_ctrl->pdcch_cl_adjust);
  if (CCEIndex<0) {
    sched_ctrl->dl_cce_fail++;
    LOG_D(NR_MAC, "[UE %04x][%4d.%2d] could not find free CCE for DL DCI retransmission\n", UE->rnti, frame, slot);
    return false;
  }

  /* Find PUCCH occasion: if it fails, undo CCE allocation (undoing PUCCH
   * allocation after CCE alloc fail would be more complex) */

  int alloc = -1;
  if (!get_FeedbackDisabled(UE->sc_info.downlinkHARQ_FeedbackDisabled_r17, current_harq_pid)) {
    int r_pucch = nr_get_pucch_resource(sched_ctrl->coreset, ul_bwp->pucch_Config, CCEIndex);
    alloc = nr_acknack_scheduling(nr_mac, UE, frame, slot, UE->UE_beam_index, r_pucch, 0);
    if (alloc < 0) {
      LOG_D(NR_MAC, "[UE %04x][%4d.%2d] could not find PUCCH for DL DCI retransmission\n", UE->rnti, frame, slot);
      return false;
    }
  }

  sched_ctrl->cce_index = CCEIndex;
  fill_pdcch_vrb_map(nr_mac, CC_id, &sched_ctrl->sched_pdcch, CCEIndex, sched_ctrl->aggregation_level, beam_idx);

  new_sched.rbStart = rbStart - bwp_info.bwpStart;
  new_sched.pucch_allocation = alloc;
  new_sched.bwp_info = bwp_info;

  post_process_dlsch(nr_mac, pp_pdsch, UE, &new_sched);

  /* retransmissions: directly allocate */
  *n_rb_sched -= new_sched.rbSize;

  for (int rb = rbStart; rb < new_sched.rbSize; rb++)
    rballoc_mask[rb] |= SL_to_bitmap(new_sched.tda_info.startSymbolIndex, new_sched.tda_info.nrOfSymbols);

  return true;
}

static void ack_reconfig(gNB_MAC_INST *mac, NR_UE_info_t *UE)
{
  if (!UE->reconfigCellGroup) {
    LOG_W(NR_MAC, "Received ACK for RRCReconfiguration, but nothing to apply!\n");
    return;
  }
  ASN_STRUCT_FREE(asn_DEF_NR_CellGroupConfig, UE->CellGroup);
  UE->CellGroup = UE->reconfigCellGroup;
  UE->reconfigCellGroup = NULL;
  NR_ServingCellConfigCommon_t *scc = mac->common_channels[0].ServingCellConfigCommon;
  /* clean BWP structures */
  clean_bwp_structures(UE->CellGroup->spCellConfig);
  configure_UE_BWP(mac, scc, UE, false, NR_SearchSpace__searchSpaceType_PR_common, -1, -1);
}

typedef struct UEsched_s {
  float coef;
  NR_UE_info_t * UE;
  int selected_mcs;
} UEsched_t;

static int comparator(const void *p, const void *q)
{
  const UEsched_t *pp = p;
  const UEsched_t *qq = q;
  if (pp->coef < qq->coef)
    return 1;
  else if (pp->coef > qq->coef)
    return -1;
  return 0;
}

static void pf_dl(gNB_MAC_INST *mac,
                  post_process_pdsch_t *pp_pdsch,
                  NR_UE_info_t **UE_list,
                  int max_num_ue,
                  int num_beams,
                  int n_rb_sched[num_beams])
{
  frame_t frame = pp_pdsch->frame;
  slot_t slot = pp_pdsch->slot;

  NR_ServingCellConfigCommon_t *scc=mac->common_channels[0].ServingCellConfigCommon;
  // UEs that could be scheduled
  UEsched_t UE_sched[MAX_MOBILES_PER_GNB + 1] = {0};
  int remainUEs[num_beams];
  for (int i = 0; i < num_beams; i++)
    remainUEs[i] = max_num_ue;
  int numUE = 0;
  int CC_id = 0;
  int slots_per_frame = mac->frame_structure.numb_slots_frame;

  /* Loop UE_info->list to check retransmission */
  UE_iterator(UE_list, UE) {
    NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    NR_UE_DL_BWP_t *current_BWP = &UE->current_DL_BWP;

    if (!nr_mac_ue_is_active(UE))
      continue;

    NR_mac_dir_stats_t *stats = &UE->mac_stats.dl;
    /* get the PID of a HARQ process awaiting retrnasmission, or -1 otherwise */
    int harq_pid = sched_ctrl->retrans_dl_harq.head;

    /* Calculate Throughput */
    const float a = 0.01f;
    const uint32_t b = stats->current_bytes;
    UE->dl_thr_ue = (1 - a) * UE->dl_thr_ue + a * b;

    stats->current_bytes = 0;
    stats->current_rbs = 0;

    /* Check if this UE should get TA. If we add the CE, ta_apply will be reset */
    if (frame == sched_ctrl->ta_frame)
      sched_ctrl->ta_apply = true;

    int total_rem_ues = 0;
    for (int i = 0; i < num_beams; i++)
      total_rem_ues += remainUEs[i];
    if (total_rem_ues == 0)
      continue;

    /* retransmission */
    if (harq_pid >= 0) {
      NR_beam_alloc_t beam = beam_allocation_procedure(&mac->beam_info, frame, slot, UE->UE_beam_index, slots_per_frame);
      bool sch_ret = beam.idx >= 0;
      /* Allocate retransmission */
      if (sch_ret)
        sch_ret = allocate_dl_retransmission(mac, pp_pdsch, &n_rb_sched[beam.idx], UE, beam.idx, harq_pid);
      if (!sch_ret) {
        LOG_D(NR_MAC, "[UE %04x][%4d.%2d] DL retransmission could not be allocated\n", UE->rnti, frame, slot);
        reset_beam_status(&mac->beam_info, frame, slot, UE->UE_beam_index, slots_per_frame, beam.new_beam);
        continue;
      }
      /* reduce max_num_ue once we are sure UE can be allocated, i.e., has CCE */
      remainUEs[beam.idx]--;

    } else {
      /* skip this UE if there are no free HARQ processes. This can happen e.g.
       * if the UE disconnected in L2sim, in which case the gNB is not notified
       * (this can be considered a design flaw) */
      if (sched_ctrl->available_dl_harq.head < 0) {
        LOG_D(NR_MAC, "[UE %04x][%4d.%2d] UE has no free DL HARQ process, skipping\n",
              UE->rnti,
              frame,
              slot);
        continue;
      }

      update_dlsch_buffer(pp_pdsch->frame, pp_pdsch->slot, UE);

      /* Check DL buffer and skip this UE if no bytes and no TA necessary */
      if (sched_ctrl->num_total_bytes == 0 && sched_ctrl->ta_apply == false)
        continue;

      /* Calculate coeff */
      const NR_bler_options_t *bo = &mac->dl_bler;
      const int max_mcs_table = current_BWP->mcsTableIdx == 1 ? 27 : 28;
      const int max_mcs = min(sched_ctrl->dl_max_mcs, max_mcs_table);
      int selected_mcs;
      if (bo->harq_round_max == 1) {
        int new_mcs = min(bo->max_mcs, max_mcs);
        selected_mcs = max(bo->min_mcs, new_mcs);
        sched_ctrl->dl_bler_stats.mcs = selected_mcs;
      } else
        selected_mcs = get_mcs_from_bler(bo, stats, &sched_ctrl->dl_bler_stats, max_mcs, frame);
      int l = get_dl_nrOfLayers(sched_ctrl, current_BWP->dci_format);
      const uint8_t Qm = nr_get_Qm_dl(selected_mcs, current_BWP->mcsTableIdx);
      const uint16_t R = nr_get_code_rate_dl(selected_mcs, current_BWP->mcsTableIdx);
      uint32_t tbs = nr_compute_tbs(Qm,
                                    R,
                                    1, /* rbSize */
                                    10, /* hypothetical number of slots */
                                    0, /* N_PRB_DMRS * N_DMRS_SLOT */
                                    0 /* N_PRB_oh, 0 for initialBWP */,
                                    0 /* tb_scaling */,
                                    l) >> 3;
      float coeff_ue = (float) tbs / UE->dl_thr_ue;
      LOG_D(NR_MAC, "[UE %04x][%4d.%2d] b %d, thr_ue %f, tbs %d, coeff_ue %f\n",
            UE->rnti,
            frame,
            slot,
            b,
            UE->dl_thr_ue,
            tbs,
            coeff_ue);
      /* Create UE_sched list for UEs eligible for new transmission*/
      UE_sched[numUE].coef = coeff_ue;
      UE_sched[numUE].UE = UE;
      UE_sched[numUE].selected_mcs = selected_mcs;
      numUE++;
    }
  }

  qsort(UE_sched, numUE, sizeof(UEsched_t), comparator);
  UEsched_t *iterator = UE_sched;

  const int min_rbSize = 5;

  /* Loop UE_sched to find max coeff and allocate transmission */
  while (iterator->UE != NULL) {

    NR_UE_sched_ctrl_t *sched_ctrl = &iterator->UE->UE_sched_ctrl;
    const uint16_t rnti = iterator->UE->rnti;

    NR_UE_DL_BWP_t *dl_bwp = &iterator->UE->current_DL_BWP;
    NR_UE_UL_BWP_t *ul_bwp = &iterator->UE->current_UL_BWP;

    if (sched_ctrl->available_dl_harq.head < 0) {
      LOG_D(NR_MAC, "[UE %04x][%4d.%2d] UE has no free DL HARQ process, skipping\n",
            iterator->UE->rnti,
            frame,
            slot);
      iterator++;
      continue;
    }

    NR_beam_alloc_t beam = beam_allocation_procedure(&mac->beam_info, frame, slot, iterator->UE->UE_beam_index, slots_per_frame);

    if (beam.idx < 0) {
      // no available beam
      iterator++;
      continue;
    }
    if (remainUEs[beam.idx] == 0 || n_rb_sched[beam.idx] < min_rbSize) {
      reset_beam_status(&mac->beam_info, frame, slot, iterator->UE->UE_beam_index, slots_per_frame, beam.new_beam);
      iterator++;
      continue;
    }

    /* MCS has been set above */
    int tda = get_dl_tda(mac, slot);
    AssertFatal(tda >= 0,"Unable to find PDSCH time domain allocation in list\n");

    const int coresetid = sched_ctrl->coreset->controlResourceSetId;
    NR_tda_info_t tda_info = get_dl_tda_info(dl_bwp,
                                            sched_ctrl->search_space->searchSpaceType->present,
                                            tda,
                                            scc->dmrs_TypeA_Position,
                                            1,
                                            TYPE_C_RNTI_,
                                            coresetid,
                                            false);
    AssertFatal(tda_info.valid_tda, "Invalid TDA from get_dl_tda_info\n");

    const uint16_t slbitmap = SL_to_bitmap(tda_info.startSymbolIndex, tda_info.nrOfSymbols);

    uint16_t *rballoc_mask = mac->common_channels[CC_id].vrb_map[beam.idx];
    bwp_info_t bwp_info = get_pdsch_bwp_start_size(mac, iterator->UE);
    int rbStart = 0; // WRT BWP start
    int rbStop = bwp_info.bwpSize - 1;
    int bwp_start = bwp_info.bwpStart;
    // Freq-demain allocation
    while (rbStart < rbStop && (rballoc_mask[rbStart + bwp_start] & slbitmap))
      rbStart++;

    uint16_t max_rbSize = 1;

    while (rbStart + max_rbSize <= rbStop && !(rballoc_mask[rbStart + max_rbSize + bwp_start] & slbitmap))
      max_rbSize++;

    if (max_rbSize < min_rbSize) {
      LOG_D(NR_MAC,
            "(%d.%d) Cannot schedule RNTI %04x, rbStart %d, rbSize %d, rbStop %d\n",
            frame,
            slot,
            rnti,
            rbStart,
            max_rbSize,
            rbStop);
      reset_beam_status(&mac->beam_info, frame, slot, iterator->UE->UE_beam_index, slots_per_frame, beam.new_beam);
      iterator++;
      continue;
    }

    int CCEIndex = get_cce_index(mac,
                                 CC_id,
                                 slot,
                                 iterator->UE->rnti,
                                 &sched_ctrl->aggregation_level,
                                 beam.idx,
                                 sched_ctrl->search_space,
                                 sched_ctrl->coreset,
                                 &sched_ctrl->sched_pdcch,
                                 sched_ctrl->pdcch_cl_adjust);
    if (CCEIndex < 0) {
      sched_ctrl->dl_cce_fail++;
      LOG_D(NR_MAC, "[UE %04x][%4d.%2d] could not find free CCE for DL DCI\n", rnti, frame, slot);
      reset_beam_status(&mac->beam_info, frame, slot, iterator->UE->UE_beam_index, slots_per_frame, beam.new_beam);
      iterator++;
      continue;
    }

    /* Find PUCCH occasion: if it fails, undo CCE allocation (undoing PUCCH
    * allocation after CCE alloc fail would be more complex) */

    int alloc = -1;
    if (!get_FeedbackDisabled(iterator->UE->sc_info.downlinkHARQ_FeedbackDisabled_r17, sched_ctrl->available_dl_harq.head)) {
      int r_pucch = nr_get_pucch_resource(sched_ctrl->coreset, ul_bwp->pucch_Config, CCEIndex);
      alloc = nr_acknack_scheduling(mac, iterator->UE, frame, slot, iterator->UE->UE_beam_index, r_pucch, 0);
      if (alloc < 0) {
        LOG_D(NR_MAC, "[UE %04x][%4d.%2d] could not find PUCCH for DL DCI\n", rnti, frame, slot);
        reset_beam_status(&mac->beam_info, frame, slot, iterator->UE->UE_beam_index, slots_per_frame, beam.new_beam);
        iterator++;
        continue;
      }
    }

    sched_ctrl->cce_index = CCEIndex;
    fill_pdcch_vrb_map(mac, CC_id, &sched_ctrl->sched_pdcch, CCEIndex, sched_ctrl->aggregation_level, beam.idx);

    int l = get_dl_nrOfLayers(sched_ctrl, dl_bwp->dci_format);
    NR_sched_pdsch_t sched_pdsch = {
      // rbSize below
      .rbStart = rbStart,
      .mcs = iterator->selected_mcs,
      .R = nr_get_code_rate_dl(iterator->selected_mcs, dl_bwp->mcsTableIdx),
      .Qm = nr_get_Qm_dl(iterator->selected_mcs, dl_bwp->mcsTableIdx),
      // tb_size below
      .dl_harq_pid = sched_ctrl->available_dl_harq.head,
      .pucch_allocation = alloc,
      .pm_index = get_pm_index(mac, iterator->UE, dl_bwp->dci_format, l, mac->radio_config.pdsch_AntennaPorts.XP),
      .nrOfLayers = l,
      .bwp_info = bwp_info,
      .dmrs_parms = get_dl_dmrs_params(scc, dl_bwp, &tda_info, l),
      .time_domain_allocation = tda,
      .tda_info = tda_info,
    };

    sched_pdsch.action = NULL;
    int srb1 = 1;
    /* everything that's only 3 bytes is an ack. To be safe, use a bit more. */
    if (iterator->UE->reconfigCellGroup && sched_ctrl->rlc_status[srb1].bytes_in_buffer > 10)
      sched_pdsch.action = ack_reconfig;

    // Fix me: currently, the RLC does not give us the total number of PDUs
    // awaiting. Therefore, for the time being, we put a fixed overhead of 12
    // (for 4 PDUs) and optionally + 2 for TA. Once RLC gives the number of
    // PDUs, we replace with 3 * numPDUs
    const int oh = 3 * 4 + (sched_ctrl->ta_apply ? 2 : 0);
    //const int oh = 3 * sched_ctrl->dl_pdus_total + (sched_ctrl->ta_apply ? 2 : 0);
    nr_find_nb_rb(sched_pdsch.Qm,
                  sched_pdsch.R,
                  1, // no transform precoding for DL
                  sched_pdsch.nrOfLayers,
                  tda_info.nrOfSymbols,
                  sched_pdsch.dmrs_parms.N_PRB_DMRS * sched_pdsch.dmrs_parms.N_DMRS_SLOT,
                  sched_ctrl->num_total_bytes + oh,
                  min_rbSize,
                  max_rbSize,
                  &sched_pdsch.tb_size,
                  &sched_pdsch.rbSize);

    post_process_dlsch(mac, pp_pdsch, iterator->UE, &sched_pdsch);

    /* transmissions: directly allocate */
    n_rb_sched[beam.idx] -= sched_pdsch.rbSize;

    for (int rb = bwp_start; rb < sched_pdsch.rbSize; rb++)
      rballoc_mask[rb + sched_pdsch.rbStart] |= slbitmap;

    remainUEs[beam.idx]--;
    iterator++;
  }
}

static void nr_dlsch_preprocessor(gNB_MAC_INST *mac, post_process_pdsch_t *pp_pdsch)
{
  NR_UEs_t *UE_info = &mac->UE_info;

  if (UE_info->connected_ue_list[0] == NULL)
    return;

  NR_ServingCellConfigCommon_t *scc = mac->common_channels[0].ServingCellConfigCommon;
  int bw = scc->downlinkConfigCommon->frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth;
  int num_beams = mac->beam_info.beam_allocation ? mac->beam_info.beams_per_period : 1;
  int n_rb_sched[num_beams];
  for (int i = 0; i < num_beams; i++)
    n_rb_sched[i] = bw;

  int average_agg_level = 4; // TODO find a better estimation
  int max_sched_ues = bw / (average_agg_level * NR_NB_REG_PER_CCE);

  // FAPI cannot handle more than MAX_DCI_CORESET DCIs
  max_sched_ues = min(max_sched_ues, MAX_DCI_CORESET);

  /* proportional fair scheduling algorithm */
  pf_dl(mac, pp_pdsch, UE_info->connected_ue_list, max_sched_ues, num_beams, n_rb_sched);
}

nr_pp_impl_dl nr_init_dlsch_preprocessor(int CC_id)
{
  return nr_dlsch_preprocessor;
}

nfapi_nr_dl_tti_pdsch_pdu_rel15_t *prepare_pdsch_pdu(nfapi_nr_dl_tti_request_pdu_t *dl_tti_pdsch_pdu,
                                                     const gNB_MAC_INST *mac,
                                                     const NR_UE_info_t *UE,
                                                     const NR_sched_pdsch_t *sched_pdsch,
                                                     const NR_PDSCH_Config_t *pdsch_Config,
                                                     bool is_sib1,
                                                     int harq_round,
                                                     int rnti,
                                                     int beam_index,
                                                     int nl_tbslbrm,
                                                     int pdu_index)
{
  const NR_UE_DL_BWP_t *dl_bwp = UE ? &UE->current_DL_BWP : NULL;
  const NR_ServingCellConfigCommon_t *scc = mac->common_channels[0].ServingCellConfigCommon;
  nfapi_nr_dl_tti_pdsch_pdu_rel15_t *pdsch_pdu = &dl_tti_pdsch_pdu->pdsch_pdu.pdsch_pdu_rel15;
  pdsch_pdu->pduBitmap = 0;
  pdsch_pdu->rnti = rnti;
  pdsch_pdu->pduIndex = pdu_index;
  pdsch_pdu->BWPSize  = sched_pdsch->bwp_info.bwpSize;
  pdsch_pdu->BWPStart = sched_pdsch->bwp_info.bwpStart;
  pdsch_pdu->SubcarrierSpacing = dl_bwp ? dl_bwp->scs : *scc->ssbSubcarrierSpacing;
  pdsch_pdu->CyclicPrefix = dl_bwp && dl_bwp->cyclicprefix ? *dl_bwp->cyclicprefix : 0;
  // Codeword information
  pdsch_pdu->NrOfCodewords = 1;
  pdsch_pdu->targetCodeRate[0] = sched_pdsch->R;
  pdsch_pdu->qamModOrder[0] = sched_pdsch->Qm;
  pdsch_pdu->mcsIndex[0] = sched_pdsch->mcs;
  pdsch_pdu->mcsTable[0] = dl_bwp ? dl_bwp->mcsTableIdx : 0;
  pdsch_pdu->rvIndex[0] = nr_get_rv(harq_round % 4);
  pdsch_pdu->TBSize[0] = sched_pdsch->tb_size;
  pdsch_pdu->dataScramblingId = pdsch_Config && pdsch_Config->dataScramblingIdentityPDSCH ? *pdsch_Config->dataScramblingIdentityPDSCH : *scc->physCellId;
  pdsch_pdu->nrOfLayers = sched_pdsch->nrOfLayers;
  pdsch_pdu->transmissionScheme = 0;
  pdsch_pdu->refPoint = is_sib1;
  // DMRS
  const NR_pdsch_dmrs_t *dmrs_parms = &sched_pdsch->dmrs_parms;
  pdsch_pdu->dlDmrsSymbPos = dmrs_parms->dl_dmrs_symb_pos;
  pdsch_pdu->dmrsConfigType = dmrs_parms->dmrsConfigType;
  pdsch_pdu->SCID = dmrs_parms->n_scid;
  pdsch_pdu->dlDmrsScramblingId = dmrs_parms->scrambling_id;
  pdsch_pdu->numDmrsCdmGrpsNoData = dmrs_parms->numDmrsCdmGrpsNoData;
  pdsch_pdu->dmrsPorts = (1 << sched_pdsch->nrOfLayers) - 1;  // FIXME with a better implementation
  // Pdsch Allocation in frequency domain
  pdsch_pdu->resourceAlloc = 1;
  pdsch_pdu->rbStart = sched_pdsch->rbStart;
  pdsch_pdu->rbSize = sched_pdsch->rbSize;
  pdsch_pdu->VRBtoPRBMapping = 0; // non-interleaved
  // Resource Allocation in time domain
  const NR_tda_info_t *tda_info = &sched_pdsch->tda_info;
  pdsch_pdu->StartSymbolIndex = tda_info->startSymbolIndex;
  pdsch_pdu->NrOfSymbols = tda_info->nrOfSymbols;
  /* Check and validate PTRS values */
  if (dmrs_parms->phaseTrackingRS) {
    bool valid_ptrs_setup = set_dl_ptrs_values(dmrs_parms->phaseTrackingRS,
                                               pdsch_pdu->rbSize,
                                               pdsch_pdu->mcsIndex[0],
                                               pdsch_pdu->mcsTable[0],
                                               &pdsch_pdu->PTRSFreqDensity,
                                               &pdsch_pdu->PTRSTimeDensity,
                                               &pdsch_pdu->PTRSPortIndex,
                                               &pdsch_pdu->nEpreRatioOfPDSCHToPTRS,
                                               &pdsch_pdu->PTRSReOffset,
                                               pdsch_pdu->NrOfSymbols);
    if (valid_ptrs_setup)
      pdsch_pdu->pduBitmap |= 0x1; // Bit 0: pdschPtrs - Indicates PTRS included (FR2)
  }
  int dl_bw_tbslbrm = UE ? UE->sc_info.dl_bw_tbslbrm : sched_pdsch->bwp_info.bwpSize;
  pdsch_pdu->maintenance_parms_v3.tbSizeLbrmBytes = nr_compute_tbslbrm(pdsch_pdu->mcsTable[0], dl_bw_tbslbrm, nl_tbslbrm);
  pdsch_pdu->maintenance_parms_v3.ldpcBaseGraph = get_BG(sched_pdsch->tb_size << 3, sched_pdsch->R);
  // Precoding and beamforming
  pdsch_pdu->precodingAndBeamforming.num_prgs = 1;
  pdsch_pdu->precodingAndBeamforming.prg_size = pdsch_pdu->rbSize;
  pdsch_pdu->precodingAndBeamforming.dig_bf_interfaces = 1;
  pdsch_pdu->precodingAndBeamforming.prgs_list[0].pm_idx = sched_pdsch->pm_index; 
  pdsch_pdu->precodingAndBeamforming.prgs_list[0].dig_bf_interface_list[0].beam_idx = beam_index;
  return pdsch_pdu;
}

void post_process_dlsch(gNB_MAC_INST *nr_mac, post_process_pdsch_t *pdsch, NR_UE_info_t *UE, NR_sched_pdsch_t *sched_pdsch)
{
  int CC_id = 0;
  int module_id = nr_mac->Mod_id;
  frame_t frame = pdsch->frame;
  slot_t slot = pdsch->slot;

  const NR_ServingCellConfigCommon_t *scc = nr_mac->common_channels[CC_id].ServingCellConfigCommon;
  NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  NR_UE_DL_BWP_t *current_BWP = &UE->current_DL_BWP;

  const rnti_t rnti = UE->rnti;

  /* POST processing */
  const uint8_t nrOfLayers = sched_pdsch->nrOfLayers;
  const uint32_t TBS = sched_pdsch->tb_size;
  int8_t current_harq_pid = sched_pdsch->dl_harq_pid;

  if (current_harq_pid < 0) {
    /* PP has not selected a specific HARQ Process, get a new one */
    current_harq_pid = sched_ctrl->available_dl_harq.head;
    AssertFatal(current_harq_pid >= 0,
                "no free HARQ process available for UE %04x\n",
                UE->rnti);
    remove_front_nr_list(&sched_ctrl->available_dl_harq);
    sched_pdsch->dl_harq_pid = current_harq_pid;
  } else {
    /* PP selected a specific HARQ process. Check whether it will be a new
     * transmission or a retransmission, and remove from the corresponding
     * list */
    if (sched_ctrl->harq_processes[current_harq_pid].round == 0)
      remove_nr_list(&sched_ctrl->available_dl_harq, current_harq_pid);
    else
      remove_nr_list(&sched_ctrl->retrans_dl_harq, current_harq_pid);
  }

  NR_tda_info_t *tda_info = &sched_pdsch->tda_info;
  NR_pdsch_dmrs_t *dmrs_parms = &sched_pdsch->dmrs_parms;
  NR_UE_harq_t *harq = &sched_ctrl->harq_processes[current_harq_pid];
  NR_sched_pucch_t *pucch = NULL;
  DevAssert(!harq->is_waiting);
  if (sched_pdsch->pucch_allocation < 0) {
    finish_nr_dl_harq(sched_ctrl, current_harq_pid);
  } else {
    pucch = &sched_ctrl->sched_pucch[sched_pdsch->pucch_allocation];
    add_tail_nr_list(&sched_ctrl->feedback_dl_harq, current_harq_pid);
    harq->feedback_frame = pucch->frame;
    harq->feedback_slot = pucch->ul_slot;
    harq->is_waiting = true;
  }
  UE->mac_stats.dl.rounds[harq->round]++;
  LOG_D(NR_MAC,
        "%4d.%2d [DLSCH/PDSCH/PUCCH] RNTI %04x DCI L %d start %3d RBs %3d startSymbol %2d nb_symbol %2d dmrspos %x MCS %2d nrOfLayers %d TBS %4d HARQ PID %2d round %d RV %d NDI %d dl_data_to_ULACK %d (%d.%d) PUCCH allocation %d TPC %d\n",
        frame,
        slot,
        rnti,
        sched_ctrl->aggregation_level,
        sched_pdsch->rbStart,
        sched_pdsch->rbSize,
        tda_info->startSymbolIndex,
        tda_info->nrOfSymbols,
        dmrs_parms->dl_dmrs_symb_pos,
        sched_pdsch->mcs,
        nrOfLayers,
        TBS,
        current_harq_pid,
        harq->round,
        nr_get_rv(harq->round % 4),
        harq->ndi,
        pucch ? pucch->timing_indicator : 0,
        pucch ? pucch->frame : 0,
        pucch ? pucch->ul_slot : 0,
        sched_pdsch->pucch_allocation,
        sched_ctrl->tpc1);
  DevAssert(sched_pdsch->rbSize > 0);

  const int bwp_id = current_BWP->bwp_id;
  const int coresetid = sched_ctrl->coreset->controlResourceSetId;

  /* look up the PDCCH PDU for this CC, BWP, and CORESET. If it does not exist, create it */
  nfapi_nr_dl_tti_pdcch_pdu_rel15_t *pdcch_pdu = nr_mac->pdcch_pdu_idx[CC_id][coresetid];

  if (!pdcch_pdu) {
    LOG_D(NR_MAC, "creating pdcch pdu, pdcch_pdu = NULL. \n");
    nfapi_nr_dl_tti_request_pdu_t *dl_tti_pdcch_pdu = &pdsch->dl_req->dl_tti_pdu_list[pdsch->dl_req->nPDUs];
    memset(dl_tti_pdcch_pdu, 0, sizeof(nfapi_nr_dl_tti_request_pdu_t));
    dl_tti_pdcch_pdu->PDUType = NFAPI_NR_DL_TTI_PDCCH_PDU_TYPE;
    dl_tti_pdcch_pdu->PDUSize = (uint8_t)(2+sizeof(nfapi_nr_dl_tti_pdcch_pdu));
    pdsch->dl_req->nPDUs += 1;
    pdcch_pdu = &dl_tti_pdcch_pdu->pdcch_pdu.pdcch_pdu_rel15;
    LOG_D(NR_MAC,"Trying to configure DL pdcch for UE %04x, bwp %d, cs %d\n", UE->rnti, bwp_id, coresetid);
    NR_ControlResourceSet_t *coreset = sched_ctrl->coreset;
    nr_configure_pdcch(pdcch_pdu, coreset, &sched_ctrl->sched_pdcch);
    nr_mac->pdcch_pdu_idx[CC_id][coresetid] = pdcch_pdu;
  }

  nfapi_nr_dl_tti_request_pdu_t *dl_tti_pdsch_pdu = &pdsch->dl_req->dl_tti_pdu_list[pdsch->dl_req->nPDUs];
  memset(dl_tti_pdsch_pdu, 0, sizeof(nfapi_nr_dl_tti_request_pdu_t));
  dl_tti_pdsch_pdu->PDUType = NFAPI_NR_DL_TTI_PDSCH_PDU_TYPE;
  dl_tti_pdsch_pdu->PDUSize = (uint8_t)(2 + sizeof(nfapi_nr_dl_tti_pdsch_pdu));
  pdsch->dl_req->nPDUs += 1;

  /* SCF222: PDU index incremented for each PDSCH PDU sent in TX control
   * message. This is used to associate control information to data and is
   * reset every slot. */
  const int pduindex = nr_mac->pdu_index[CC_id]++;
  // TODO: verify the case where maxMIMO_Layers is NULL, in which case
  //       in principle maxMIMO_layers should be given by the maximum number of layers
  //       for PDSCH supported by the UE for the serving cell (5.4.2.1 of 38.212)
  long ue_supp_nl = ue_supported_dl_layers(scc, UE->capability);
  long maxMIMO_Layers = UE->sc_info.maxMIMO_Layers_PDSCH ? *UE->sc_info.maxMIMO_Layers_PDSCH : ue_supp_nl;
  if (maxMIMO_Layers < 1) {
    LOG_D(NR_MAC, "Both maxMIMO_Layers_PDSCH and UE supported layers are not present, defaulting to 1\n");
    maxMIMO_Layers = 1;
  }
  const int nl_tbslbrm = min(maxMIMO_Layers, 4);
  const uint16_t fapi_beam = convert_to_fapi_beam(UE->UE_beam_index, nr_mac->beam_info.beam_mode);
  nfapi_nr_dl_tti_pdsch_pdu_rel15_t *pdsch_pdu = prepare_pdsch_pdu(dl_tti_pdsch_pdu,
                                                                   nr_mac,
                                                                   UE,
                                                                   sched_pdsch,
                                                                   current_BWP->pdsch_Config,
                                                                   false,
                                                                   harq->round,
                                                                   rnti,
                                                                   fapi_beam,
                                                                   nl_tbslbrm,
                                                                   pduindex);


  LOG_D(NR_MAC,"Configuring DCI/PDCCH in %d.%d at CCE %d, rnti %x\n", frame,slot,sched_ctrl->cce_index,rnti);
  /* Fill PDCCH DL DCI PDU */
  nfapi_nr_dl_dci_pdu_t *dci_pdu = prepare_dci_pdu(pdcch_pdu,
                                                   scc,
                                                   sched_ctrl->search_space,
                                                   sched_ctrl->coreset,
                                                   sched_ctrl->aggregation_level,
                                                   sched_ctrl->cce_index,
                                                   fapi_beam,
                                                   rnti);
  pdcch_pdu->numDlDci++;

  /* DCI payload */
  const int rnti_type = TYPE_C_RNTI_;
  dci_pdu_rel15_t dci_payload = prepare_dci_dl_payload(nr_mac,
                                                       UE,
                                                       rnti_type,
                                                       sched_ctrl->search_space->searchSpaceType->present,
                                                       pdsch_pdu,
                                                       sched_pdsch,
                                                       pucch,
                                                       current_harq_pid,
                                                       0,
                                                       false);

  // Reset TPC to 0 dB to not request new gain multiple times before computing new value for SNR
  sched_ctrl->tpc1 = 1;
  NR_PDSCH_Config_t *pdsch_Config = current_BWP->pdsch_Config;
  AssertFatal(pdsch_Config == NULL
              || pdsch_Config->resourceAllocation == NR_PDSCH_Config__resourceAllocation_resourceAllocationType1,
              "Only frequency resource allocation type 1 is currently supported\n");

  LOG_D(NR_MAC,
        "%4d.%2d DCI type 1 payload: freq_alloc %d (%d,%d,%d), "
        "nrOfLayers %d, time_alloc %d, vrb to prb %d, mcs %d tb_scaling %d ndi %d rv %d tpc %d ti %d\n",
        frame,
        slot,
        dci_payload.frequency_domain_assignment.val,
        pdsch_pdu->rbStart,
        pdsch_pdu->rbSize,
        pdsch_pdu->BWPSize,
        pdsch_pdu->nrOfLayers,
        dci_payload.time_domain_assignment.val,
        dci_payload.vrb_to_prb_mapping.val,
        dci_payload.mcs,
        dci_payload.tb_scaling,
        dci_payload.ndi,
        dci_payload.rv,
        dci_payload.tpc,
        pucch ? pucch->timing_indicator : 0);

  fill_dci_pdu_rel15(&UE->sc_info,
                     current_BWP,
                     &UE->current_UL_BWP,
                     dci_pdu,
                     &dci_payload,
                     current_BWP->dci_format,
                     rnti_type,
                     sched_ctrl->search_space,
                     sched_ctrl->coreset,
                     UE->pdsch_HARQ_ACK_Codebook,
                     nr_mac->cset0_bwp_size);

  LOG_D(NR_MAC,
        "coreset params: FreqDomainResource %llx, start_symbol %d  n_symb %d\n",
        (unsigned long long)pdcch_pdu->FreqDomainResource,
        pdcch_pdu->StartSymbolIndex,
        pdcch_pdu->DurationSymbols);

  if (harq->round != 0) { /* retransmission */
    /* we do not have to do anything, since we do not require to get data
     * from RLC or encode MAC CEs. The TX_req structure is filled below
     * or copy data to FAPI structures */
    LOG_D(NR_MAC,
          "%d.%2d DL retransmission RNTI %04x HARQ PID %d round %d NDI %d\n",
          frame,
          slot,
          rnti,
          current_harq_pid,
          harq->round,
          harq->ndi);
    if (!get_softmodem_params()->phy_test)
      AssertFatal(harq->sched_pdsch.tb_size == TBS,
                  "UE %04x mismatch between scheduled TBS and buffered TB for HARQ PID %d\n",
                  UE->rnti,
                  current_harq_pid);
    else if (harq->sched_pdsch.tb_size != TBS)
      LOG_E(NR_MAC,
            "Mismatch between scheduled TBS and buffered TB for HARQ PID %d. No RTX control in phy-test mode. "
            "Possible causes: presence of CSI-RS or DLSCH scheduled in the mixed slot.\n",
            current_harq_pid);
    T(T_GNB_MAC_RETRANSMISSION_DL_PDU_WITH_DATA, T_INT(module_id), T_INT(CC_id), T_INT(rnti),
      T_INT(frame), T_INT(slot), T_INT(current_harq_pid), T_INT(harq->round), T_BUFFER(harq->transportBlock.buf, TBS));
    UE->mac_stats.dl.total_rbs_retx += sched_pdsch->rbSize;
    nr_mac->mac_stats.dl.used_prb_aggregate += sched_pdsch->rbSize;
  } else { /* initial transmission */
    LOG_D(NR_MAC, "Initial HARQ transmission in %d.%d\n", frame, slot);
    uint8_t *buf = allocate_transportBlock_buffer(&harq->transportBlock, TBS);
    /* first, write all CEs that might be there */
    int written = nr_write_ce_dlsch_pdu(module_id,
                                        sched_ctrl,
                                        (unsigned char *)buf,
                                        255, // no drx
                                        NULL); // contention res id
    buf += written;
    uint8_t *bufEnd = buf + TBS - written;
    DevAssert(TBS > written);
    int dlsch_total_bytes = 0;
    /* next, get RLC data */
    start_meas(&nr_mac->rlc_data_req);
    int sdus = 0;

    if (sched_ctrl->num_total_bytes > 0) {
      /* loop over all activated logical channels */
      for (int i = 0; i < seq_arr_size(&sched_ctrl->lc_config); ++i) {
        const nr_lc_config_t *c = seq_arr_at(&sched_ctrl->lc_config, i);
        const int lcid = c->lcid;

        if (sched_ctrl->rlc_status[lcid].bytes_in_buffer == 0)
          continue; // no data for this LC        tbs_size_t len = 0;


        tb_size_t pdu_siz[MAX_NUM_DATA_REQ];
        int num = nr_mac_rlc_multi_data_req(module_id, rnti, true, lcid, bufEnd - buf, (char *)buf, pdu_siz, sizeofArray(pdu_siz));
        DevAssert(num <= sizeofArray(pdu_siz));

        sdus += num;
        int lcid_bytes = 0;
        for (int j = 0; j < num; ++j) {
          NR_MAC_SUBHEADER_LONG *header = (NR_MAC_SUBHEADER_LONG *) buf;
          header->R = 0;
          header->F = 1;
          header->LCID = lcid;
          header->L = htons(pdu_siz[j]);
          buf += pdu_siz[j] + sizeof(NR_MAC_SUBHEADER_LONG);
          dlsch_total_bytes += pdu_siz[j];
          lcid_bytes += pdu_siz[j];
        }

        T(T_GNB_MAC_LCID_DL, T_INT(rnti), T_INT(frame), T_INT(slot), T_INT(lcid), T_INT(lcid_bytes), T_INT(nr_rlc_tx_list_occupancy(rnti, lcid)));
        UE->mac_stats.dl.lc_bytes[lcid] += lcid_bytes;
      }
    } else if (get_softmodem_params()->phy_test || get_softmodem_params()->do_ra) {
      /* we will need the large header, phy-test typically allocates all
       * resources and fills to the last byte below */
      LOG_D(NR_MAC, "Configuring DL_TX in %d.%d: TBS %d of random data\n", frame, slot, TBS);

      if (bufEnd-buf > sizeof(NR_MAC_SUBHEADER_LONG) ) {
        NR_MAC_SUBHEADER_LONG *header = (NR_MAC_SUBHEADER_LONG *) buf;
        // fill dlsch_buffer with random data
        header->R = 0;
        header->F = 1;
        header->LCID = DL_SCH_LCID_PADDING;
        buf += sizeof(NR_MAC_SUBHEADER_LONG);
        header->L = htons(bufEnd-buf);

        for (; ((intptr_t)buf) % 4; buf++)
          *buf = lrand48() & 0xff;
        for (; buf < bufEnd - 3; buf += 4) {
          uint32_t *buf32 = (uint32_t *)buf;
          *buf32 = lrand48();
        }
        for (; buf < bufEnd; buf++)
          *buf = lrand48() & 0xff;
        sdus +=1;
      }
    }

    stop_meas(&nr_mac->rlc_data_req);

    // Add padding header and zero rest out if there is space left
    if (bufEnd-buf > 0) {
      NR_MAC_SUBHEADER_FIXED *padding = (NR_MAC_SUBHEADER_FIXED *) buf;
      padding->R = 0;
      padding->LCID = DL_SCH_LCID_PADDING;
      buf += 1;
      memset(buf,0,bufEnd-buf);
      buf=bufEnd;
    }

    UE->mac_stats.dl.total_bytes += TBS;
    UE->mac_stats.dl.current_bytes = TBS;
    UE->mac_stats.dl.total_rbs += sched_pdsch->rbSize;
    UE->mac_stats.dl.num_mac_sdu += sdus;
    UE->mac_stats.dl.current_rbs = sched_pdsch->rbSize;
    UE->mac_stats.dl.total_sdu_bytes += dlsch_total_bytes;
    nr_mac->mac_stats.dl.used_prb_aggregate += sched_pdsch->rbSize;

    /* save retransmission information */
    harq->sched_pdsch = *sched_pdsch;
    /* save which time allocation has been used, to be used on
     * retransmissions */
    harq->sched_pdsch.time_domain_allocation = sched_pdsch->time_domain_allocation;

    // ta command is sent, values are reset
    if (sched_ctrl->ta_apply) {
      sched_ctrl->ta_apply = false;
      sched_ctrl->ta_update = 31;
      sched_ctrl->ta_frame = (frame + 100) % MAX_FRAME_NUMBER;
      LOG_D(NR_MAC, "%d.%2d UE %04x TA scheduled, setting next TA frame to %d\n", frame, slot, UE->rnti, sched_ctrl->ta_frame);
    }

    T(T_GNB_MAC_DL_PDU_WITH_DATA, T_INT(module_id), T_INT(CC_id), T_INT(rnti),
      T_INT(frame), T_INT(slot), T_INT(current_harq_pid), T_BUFFER(harq->transportBlock.buf, TBS));
    T(T_GNB_MAC_DL, T_INT(rnti), T_INT(frame), T_INT(slot), T_INT(sched_pdsch->mcs), T_INT(TBS));
  }

  const int ntx_req = pdsch->TX_req->Number_of_PDUs;
  nfapi_nr_pdu_t *tx_req = &pdsch->TX_req->pdu_list[ntx_req];
  tx_req->PDU_index  = pduindex;
  tx_req->num_TLV = 1;
  tx_req->TLVs[0].length = TBS;
  tx_req->PDU_length = compute_PDU_length(tx_req->num_TLV, tx_req->TLVs[0].length);
  tx_req->TLVs[0].tag = 1; // means ptr carries for payload
  DevAssert((uintptr_t) harq->transportBlock.buf % 4 == 0); // check alignment: FAPI uses u32
  tx_req->TLVs[0].value.ptr = (uint32_t *)harq->transportBlock.buf;
  pdsch->TX_req->Number_of_PDUs++;
  pdsch->TX_req->SFN = frame;
  pdsch->TX_req->Slot = slot;
}

void nr_schedule_ue_spec(module_id_t module_id,
                         frame_t frame,
                         slot_t slot,
                         nfapi_nr_dl_tti_request_t *DL_req,
                         nfapi_nr_tx_data_request_t *TX_req)
{
  gNB_MAC_INST *gNB_mac = RC.nrmac[module_id];
  int CC_id = 0;

  /* already mutex protected: held in gNB_dlsch_ulsch_scheduler() */
  AssertFatal(pthread_mutex_trylock(&gNB_mac->sched_lock) == EBUSY,
              "this function should be called with the scheduler mutex locked\n");

  if (!is_dl_slot(slot, &gNB_mac->frame_structure))
    return;

  NR_ServingCellConfigCommon_t *scc = gNB_mac->common_channels[CC_id].ServingCellConfigCommon;
  int bw = scc->downlinkConfigCommon->frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth;
  gNB_mac->mac_stats.dl.total_prb_aggregate += bw;

  nfapi_nr_dl_tti_request_body_t *dl_req = &DL_req->dl_tti_request_body;
  post_process_pdsch_t pdsch = { frame, slot, dl_req, TX_req };

  /* PREPROCESSOR */
  gNB_mac->pre_processor_dl(gNB_mac, &pdsch);
}
