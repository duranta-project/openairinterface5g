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

/* \file        nr_slsch_scheduler.c
 * \brief       Routines for UE SLSCH scheduling
 * \author      R. Knopp
 * \date        Aug. 2023
 * \version     0.1
 * \company     EURECOM
 * \email       raymond.knopp@eurecom.fr
 */

#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#include <common/utils/nr/nr_common.h>

#include "NR_MAC_COMMON/nr_mac.h"
#include "NR_MAC_COMMON/nr_mac_common.h"
#include "NR_MAC_UE/mac_proto.h"
#include "NR_MAC_UE/mac_extern.h"
#include "NR_MAC_UE/nr_ue_sci.h"
#include <executables/nr-uesoftmodem.h>
#include "NR_MAC_UE/mac_defs_sl.h"
#include "NR_MAC_gNB/nr_mac_gNB.h"

#define LOWER_BLER 0.2344
#define UPPER_BLER 5.547
#define MAX_MCS 28

const uint8_t nr_rv_round_map[4] = {0, 2, 3, 1};

void reset_sl_harq_list(NR_SL_UE_sched_ctrl_t *sched_ctrl) {
  int harq;
  while ((harq = sched_ctrl->feedback_sl_harq.head) >= 0) {
    remove_front_nr_list(&sched_ctrl->feedback_sl_harq);
    add_tail_nr_list(&sched_ctrl->available_sl_harq, harq);
  }

  while ((harq = sched_ctrl->retrans_sl_harq.head) >= 0) {
    remove_front_nr_list(&sched_ctrl->retrans_sl_harq);
    add_tail_nr_list(&sched_ctrl->available_sl_harq, harq);
  }

  for (int i = 0; i < NR_MAX_HARQ_PROCESSES; i++) {
    sched_ctrl->sl_harq_processes[i].feedback_slot = -1;
    sched_ctrl->sl_harq_processes[i].round = 0;
    sched_ctrl->sl_harq_processes[i].is_waiting = false;
  }
}

void abort_nr_ue_sl_harq(NR_UE_MAC_INST_t *mac, int8_t harq_pid, NR_SL_UE_info_t *UE_info)
{
  NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE_info->UE_sched_ctrl;
  NR_UE_sl_harq_t *harq = &sched_ctrl->sl_harq_processes[harq_pid];

  harq->round = 0;
  UE_info->mac_sl_stats.sl.errors++;
  add_tail_nr_list(&sched_ctrl->available_sl_harq, harq_pid);
  /* the transmission failed: the UE won't send the data we expected initially,
   * so retrieve to correctly schedule after next BSR */
  sched_ctrl->sched_sl_bytes -= harq->sched_pssch.tb_size;
  if (sched_ctrl->sched_sl_bytes < 0)
    sched_ctrl->sched_sl_bytes = 0;
}

void handle_nr_ue_sl_harq(module_id_t mod_id,
                          frame_t frame,
                          sub_frame_t slot,
                          sl_nr_slsch_pdu_t *rx_slsch_pdu,
                          uint16_t src_id)
{
  NR_UE_MAC_INST_t *mac = get_mac_inst(mod_id);
  NR_UE_SL_SCHED_LOCK(&mac->sl_sched_lock);
  //NR_SL_UE_info_t **UE_SL_temp = (NR_SL_UE_info_t **)&mac->sl_info.list, *UE;  //Jin replace
  // TODO: update for multiple UEs   !!!Jin fix here!
  //UE=*(UE_SL_temp); //Jin replace
  //uint8_t num_ack_rcvd = rx_slsch_pdu->num_acks_rcvd;  //Jin replace

   /***********Jin add for multiple UEs ***********/
  // IMPORTANT: src_id must be host-order here.
  NR_SL_UE_info_t *UE = find_UE(mac, src_id);
  LOG_I(NR_MAC, "%s: JIN  debug &&&&&&&&&&&&&&&& me=%u src_id=%u\n", __FUNCTION__, mac->src_id, src_id);
  if (!UE) {
    LOG_W(NR_MAC, "%s: unknown peer src_id=%u (me=%u)\n",
          __FUNCTION__, src_id, mac->src_id);
    NR_UE_SL_SCHED_UNLOCK(&mac->sl_sched_lock);
    return;
  }
  uint8_t num_ack_rcvd = rx_slsch_pdu->num_acks_rcvd;
  /**************Jin end ********* */


  NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  NR_UE_sl_harq_t **matched_harqs = (NR_UE_sl_harq_t **) calloc(sched_ctrl->feedback_sl_harq.len, sizeof(NR_UE_sl_harq_t *));
  int k = find_current_slot_harqs(frame, slot, sched_ctrl, matched_harqs);
  LOG_D(NR_MAC, "Found %d matching HARQ processes vs. num. of received acks %d\n", k, num_ack_rcvd);
  for (int i = 0; i < num_ack_rcvd; i++) {
    uint8_t ack_nack = rx_slsch_pdu->ack_nack_rcvd[i];
    uint8_t rx_harq_id = matched_harqs[i]->sl_harq_pid;
    NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    int8_t harq_pid = sched_ctrl->feedback_sl_harq.head;
    LOG_D(NR_MAC, "Comparing %4u.%2u rx_harq_id vs feedback harq_pid = %d %d\n", frame, slot, rx_harq_id, harq_pid);
    while (rx_harq_id != harq_pid || harq_pid < 0) {
      LOG_W(NR_MAC,
            "Unexpected SLSCH HARQ PID %d (have %d) for src id %4d\n",
            rx_harq_id,
            harq_pid,
            src_id);
      if (harq_pid < 0) {
        NR_UE_SL_SCHED_UNLOCK(&mac->sl_sched_lock);
        return;
      }

      remove_front_nr_list(&sched_ctrl->feedback_sl_harq);
      sched_ctrl->sl_harq_processes[harq_pid].is_waiting = false;

      if(sched_ctrl->sl_harq_processes[harq_pid].round >= (HARQ_ROUND_MAX - 1)) {
        abort_nr_ue_sl_harq(mac, harq_pid, UE);
      } else {
        sched_ctrl->sl_harq_processes[harq_pid].round++;
        add_tail_nr_list(&sched_ctrl->retrans_sl_harq, harq_pid);
      }
      harq_pid = sched_ctrl->feedback_sl_harq.head;
    }
    remove_front_nr_list(&sched_ctrl->feedback_sl_harq);
    NR_UE_sl_harq_t *harq = &sched_ctrl->sl_harq_processes[harq_pid];
    DevAssert(harq->is_waiting);
    harq->feedback_slot = -1;
    harq->is_waiting = false;
    if (!ack_nack) {
      UE->mac_sl_stats.cumul_round[harq->round]++;
      harq->round = 0;
      LOG_D(NR_MAC,
            "%4u.%2u Slharq id %d crc passed for src id %4d\n",
            frame,
            slot,
            harq_pid,
            src_id);
      add_tail_nr_list(&sched_ctrl->available_sl_harq, harq_pid);
    } else if (harq->round >= (HARQ_ROUND_MAX - 1)) {
      UE->mac_sl_stats.cumul_round[HARQ_ROUND_MAX]++;
      LOG_D(NR_MAC,
            "src id %4d, Slharq id %d crc failed in all rounds\n",
            src_id,
            harq_pid);
      abort_nr_ue_sl_harq(mac, harq_pid, UE);
    } else {
      harq->round++;
      LOG_D(NR_MAC,
            "%4u.%2u Slharq id %d crc failed for src id %4d\n",
            frame,
            slot,
            harq_pid,
            src_id);
      add_tail_nr_list(&sched_ctrl->retrans_sl_harq, harq_pid);
    }
  }
  free(matched_harqs);
  matched_harqs = NULL;
  NR_UE_SL_SCHED_UNLOCK(&mac->sl_sched_lock);
}

uint32_t compute_TRIV(uint8_t N, uint8_t t1, uint8_t t2) {
  int32_t triv = 0;
  if (N == 1) {
    triv = 0;
  } else if (N == 2) {
    triv = t1;
  } else {
    if ((t2 - t1 - 1) <= 15) {
      triv = 30 * (t2 - t1 - 1) + t1 + 31;
    } else {
      triv = 30 * (31 - t2 + t1) + 62 - t1;
    }
  }
  return triv;
}

uint32_t compute_FRIV(uint8_t sl_max_num_per_reserve,
                      uint8_t L_sub_chan,
                      uint8_t n_start_subch1,
                      uint8_t n_start_subch2,
                      uint8_t N_sl_subch) {
  uint32_t friv = 0;
  int sum = 0;
  if (sl_max_num_per_reserve == NR_SL_UE_SelectedConfigRP_r16__sl_MaxNumPerReserve_r16_n2) {
    for (int i = 1; i < L_sub_chan; i++) {
      sum += N_sl_subch + 1 - i;
    }
    friv = n_start_subch1 + sum;
  } else if (sl_max_num_per_reserve == NR_SL_UE_SelectedConfigRP_r16__sl_MaxNumPerReserve_r16_n3) {
    for (int i = 1; i < L_sub_chan; i++) {
      sum += (N_sl_subch + 1 - i) * (N_sl_subch + 1 - i);
    }
    friv = n_start_subch1 + n_start_subch2 * (N_sl_subch + 1 - L_sub_chan) + sum;
  } else {
    AssertFatal(1 == 0, "sl_MaxNumPerReserve is configured with incorrect value");
  }

  return friv;
}

void nr_schedule_slsch(NR_UE_MAC_INST_t *mac, int frameP, int slotP, nr_sci_pdu_t *sci_pdu,
                       nr_sci_pdu_t *sci2_pdu, nr_sci_format_t format2,
                       NR_SL_UE_info_t *UE,
                       uint16_t *slsch_pdu_length_max, NR_UE_sl_harq_t *cur_harq,
                       mac_rlc_status_resp_t *rlc_status,
                       sl_resource_info_t *resource) {
//uid_t dest_id = UE->uid;//Jin problem : hardcoded as 1 for all UEs
    /* ----------------------------Jin replacement:--------- */
    uid_t dest_id = 0;
    const uint16_t src = mac->src_id;

    if (src == 0) {
      // alternate between 1 and 2 deterministically using (frame,slot)
      sl_nr_ue_mac_params_t *sl_mac_params = mac->SL_MAC_PARAMS;
      const int mu = sl_mac_params->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;
      const int spf = nr_slots_per_frame[mu];
      const int64_t abs = (int64_t)frameP * spf + slotP;   // use the function's frame/slot variables
      dest_id = (abs & 1) ? 2 : 1;
    } else {
      dest_id = 0;
    }

    // Optional: sanity log (remove once stable)
    LOG_D(NR_MAC, "[SL-TX] src_id=%u -> dest_id=%u (frame=%d slot=%d)\n",
          src, dest_id, frameP, slotP);
    /* ------------------------------------------------------ */



   NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  const NR_mac_dir_stats_t *stats = &UE->mac_sl_stats.sl;
  NR_sched_pssch_t *sched_pssch = &sched_ctrl->sched_pssch;
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;
  uint8_t mu = sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;
  uint8_t slots_per_frame = nr_slots_per_frame[mu];

  uint8_t psfch_period = 0;
  const uint8_t psfch_periods[] = {0,1,2,4};
  psfch_period = (mac->sl_tx_res_pool->sl_PSFCH_Config_r16 &&
                  mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16)
                  ? psfch_periods[*mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16] : 0;
  *slsch_pdu_length_max = 0;

  NR_TDD_UL_DL_Pattern_t *tdd = &sl_mac->sl_TDD_config->pattern1;
  int period = 0, offset = 0;
  bool csi_acq = !mac->SL_MAC_PARAMS->sl_CSI_Acquisition;
  SL_CSI_Report_t *sl_csi_report = set_nr_ue_sl_csi_meas_periodicity(tdd, sched_ctrl, mac, dest_id, false);
  nr_ue_sl_csi_period_offset(sl_csi_report,
                             &period,
                             &offset);
  // Determine current slot is csi-rs schedule slot
  bool csi_req_slot = !((slots_per_frame * frameP + slotP - offset) % period);

  uint8_t ri = 0;
  uint8_t cqi_Table = 0;
  uint8_t cqi = sched_ctrl->rx_csi_report.CQI;
  sched_pssch->mcs = sched_ctrl->sl_max_mcs;

  int mcs_tb_ind = 0;
  // we are using as a flag to indicate if csi report was received
  if (cqi) {
    if (sci_pdu->additional_mcs.nbits > 0)
      mcs_tb_ind = sci_pdu->additional_mcs.val;
    if (mcs_tb_ind == 0)
      cqi_Table = NR_CSI_ReportConfig__cqi_Table_table1;
    else if (mcs_tb_ind == 1)
      cqi_Table = NR_CSI_ReportConfig__cqi_Table_table2;
    else if (mcs_tb_ind == 2)
      cqi_Table = NR_CSI_ReportConfig__cqi_Table_table3;

    sched_pssch->mcs = get_mcs_from_cqi(mcs_tb_ind, cqi_Table, cqi, get_nrUE_params()->mcs);
    sched_ctrl->sl_max_mcs = sched_pssch->mcs;
    ri = sched_ctrl->rx_csi_report.RI;
  }

  /* Calculate coeff */
  NR_bler_options_t *sl_bo = &sl_mac->sl_bler;
  sl_bo->lower = LOWER_BLER;
  sl_bo->upper = UPPER_BLER;
  sl_bo->max_mcs = MAX_MCS;

  const int max_mcs_table = mcs_tb_ind == 1 ? 27 : 28;
  int max_mcs = min(sched_ctrl->sl_max_mcs, max_mcs_table);
  if (sl_bo->harq_round_max == 1)
    sched_pssch->mcs = max_mcs;
  else {
    sched_pssch->mcs = get_mcs_from_bler(sl_bo, stats, &sched_ctrl->sl_bler_stats, max_mcs, frameP);
  }

  uint16_t sl_max_num_reserve = *mac->sl_tx_res_pool->sl_UE_SelectedConfigRP_r16->sl_MaxNumPerReserve_r16;
  /*
  Following values are based on spec. 38214 section 8.1.5, N = 1 or 2 actual resources when sl-
  MaxNumPerReserve is 2, and N = 1 or 2 or 3 actual resources when sl-MaxNumPerReserve is 3.
  For N = 2, 1 <= t1 <= 31; and for N = 3, 1 <= t1 <= 30, t1 < t2 <= 31, We are taking N = 1; it represents only 1 reserved resource.
  */
  int N = 1;
  uint8_t t1 = 0, t2 = 0;

  long sl_num_subch = *mac->sl_tx_res_pool->sl_NumSubchannel_r16;
  uint8_t l_subch = resource->sl_subchan_len; // number of used sub channels; as in current setting, we have only 1 subchannel so l_subch is set to 1
  uint8_t n_start_subch1 = 0, n_start_subch2 = 0; // represent starting sub-channel index for the second resource and third resource;
                                                  // as we are considering only 1 subchannel, so we have initialized these variables with zeros.
  // Fill SCI1A
  sci_pdu->priority = 0;
  sci_pdu->frequency_resource_assignment.val = compute_FRIV(sl_max_num_reserve, l_subch, n_start_subch1, n_start_subch2, sl_num_subch);
  sci_pdu->time_resource_assignment.val = compute_TRIV(N, t1, t2);
  sci_pdu->resource_reservation_period.val = mac->SL_MAC_PARAMS->mac_tx_params.rri;
  sci_pdu->dmrs_pattern.val = 0;
  sci_pdu->second_stage_sci_format = 0;
  sci_pdu->number_of_dmrs_port = ri;
  // we are using as a flag to indicate if csi report was received
  sci_pdu->mcs = sched_pssch->mcs;
  sci_pdu->additional_mcs.val = 0;
  if (frameP % 5 == 0)
    LOG_D(NR_MAC, "cqi ---> %d Tx %4d.%2d dest: %d mcs %i\n",
          cqi, frameP, slotP, dest_id, sci_pdu->mcs);
  /*Following code will check whether SLSCH was received before and
  its feedback has scheduled for current slot
  */
  int scs = sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;
  const int nr_slots_frame = nr_slots_per_frame[scs];
  const int n_ul_slots_period = tdd ? tdd->nrofUplinkSlots + (tdd->nrofUplinkSymbols > 0 ? 1 : 0) : nr_slots_frame;

  uint16_t num_subch = sl_get_num_subch(mac->sl_tx_res_pool);
  bool is_feedback_slot = false;
  for (int i = 0; i < (n_ul_slots_period * num_subch); i++) {
    //SL_sched_feedback_t  *sched_psfch = &mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch[i]; //Jin replace
    //Jin implementation to repalce info.list
    NR_SL_UE_info_t *peer = find_UE(mac, dest_id);
    if (!peer || !peer->UE_sched_ctrl.sched_psfch) {
      LOG_W(NR_MAC, "PSSCH/PSFCH: no peer or no sched_psfch for dest_id=%d\n", dest_id);
      continue; // or return depending on context
    }
    SL_sched_feedback_t *sched_psfch = &peer->UE_sched_ctrl.sched_psfch[i];
    //Jin end


    if (slotP == sched_psfch->feedback_slot) {
        LOG_D(NR_MAC, "%4d.%2d i = %d sched_psfch %p feedback slot %d\n", frameP, slotP, i, sched_psfch, sched_psfch->feedback_slot);
        is_feedback_slot = true;
        frameslot_t frame_slot;
        frame_slot.frame = frameP;
        frame_slot.slot = slotP;
        validate_selected_sl_slot(true, false, mac->SL_MAC_PARAMS->sl_TDD_config, frame_slot);
        break;
    }
  }

  frameslot_t fs;
  fs.frame = frameP;
  fs.slot = slotP;
  uint8_t pool_id = 0;
  uint64_t tx_abs_slot = normalize(&fs, mu);
  SL_ResourcePool_params_t *sl_tx_rsrc_pool = sl_mac->sl_TxPool[pool_id];
  size_t phy_map_sz = ((sl_tx_rsrc_pool->phy_sl_bitmap.size << 3) - sl_tx_rsrc_pool->phy_sl_bitmap.bits_unused);
  bool sl_has_psfch = slot_has_psfch(mac, &sl_tx_rsrc_pool->phy_sl_bitmap, tx_abs_slot, psfch_period, phy_map_sz, mac->SL_MAC_PARAMS->sl_TDD_config);
  if ((psfch_period == 2 || psfch_period == 4) && (sl_has_psfch)) {
    if (is_feedback_slot) {
      sci_pdu->psfch_overhead.val =  1;
      LOG_D(NR_MAC, "%4d.%2d Setting psfch_overhead 1\n", frameP, slotP);
    } else {
        sci_pdu->psfch_overhead.val = 0;
        LOG_D(NR_MAC, "%4d.%2d Setting psfch_overhead 0\n", frameP, slotP);
    }
  } else if ((psfch_period == 2 || psfch_period == 4) && (!sl_has_psfch)) {
      sci_pdu->psfch_overhead.val = 0;
  }

  sci_pdu->reserved.val = mac->is_synced ? 1 : 0;
  sci_pdu->conflict_information_receiver.val = 0;
  sci_pdu->beta_offset_indicator = 0;
  sci2_pdu->harq_pid = cur_harq->sl_harq_pid;
  sci2_pdu->ndi = cur_harq->ndi;
  sci2_pdu->rv_index = nr_rv_round_map[cur_harq->round % 4];
  sci2_pdu->source_id = mac->src_id;
  /* --- JIN: sync-ref should reply to the UE that sent the last SCI2 --- */
  if (mac->src_id == 0) { // sync-ref
    const uint16_t last_rx_src = mac->sci_pdu_rx.source_id;  // decoded in nr_ue_process_sci2_indication_pdu
    if (last_rx_src != 0 && last_rx_src != mac->src_id) {
      dest_id = last_rx_src;
    }
    LOG_I(NR_MAC,
          "[JIN][SL-TX-DEST] sync-ref me=%u choose DST=%u (last_rx_src=%u)\n",
          mac->src_id, dest_id, last_rx_src);
  }
  /* --- end JIN --- */


  sci2_pdu->dest_id = dest_id;
  sci2_pdu->harq_feedback = cur_harq->is_waiting;

  LOG_I(NR_MAC, "[Jin Jin !!!!! slsch define sci2] SCI2 SRC=%d DST=%d\n",sci2_pdu->source_id ,sci2_pdu->dest_id );//Jin debug fixed srcID problem

  LOG_D(NR_MAC, "%4d.%2d Comparing Setting harq_feedback %d bytes_in_buffer %d sl_harq_pid %d\n", frameP, slotP, sci2_pdu->harq_feedback, rlc_status->bytes_in_buffer, cur_harq ? cur_harq->sl_harq_pid : 0);
  sci2_pdu->cast_type = 1;
  if (format2 == NR_SL_SCI_FORMAT_2C || format2 == NR_SL_SCI_FORMAT_2A) {
    sci2_pdu->csi_req = (csi_acq && csi_req_slot) ? 1 : 0;
    sci2_pdu->csi_req = (cur_harq->round > 0 || is_feedback_slot) ? 0 : sci2_pdu->csi_req;
    LOG_D(NR_MAC, "%4d.%2d Setting sci2_pdu->csi_req %d\n", frameP, slotP, sci2_pdu->csi_req);
  }
  if (format2 == NR_SL_SCI_FORMAT_2B)
    sci2_pdu->zone_id = 0;
  // Fill in for R17: communication_range
  sci2_pdu->communication_range.val = 0;
  if (format2 == NR_SL_SCI_FORMAT_2C) {
    sci2_pdu->providing_req_ind = 0;
    // Fill in for R17 : resource combinations
    sci2_pdu->resource_combinations.val = 0;
    sci2_pdu->first_resource_location = 0;
    // Fill in for R17 : reference_slot_location
    sci2_pdu->reference_slot_location.val = 0;
    sci2_pdu->resource_set_type = 0;
    // Fill in for R17 : lowest_subchannel_indices
    sci2_pdu->lowest_subchannel_indices.val = 0;
  }
  // Set SLSCH
  *slsch_pdu_length_max = rlc_status->bytes_in_buffer;
}

SL_CSI_Report_t* set_nr_ue_sl_csi_meas_periodicity(const NR_TDD_UL_DL_Pattern_t *tdd,
                                                   NR_SL_UE_sched_ctrl_t *sched_ctrl,
                                                   NR_UE_MAC_INST_t *mac,
                                                   int uid,
                                                   bool is_rsrp) {
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;
  sl_nr_phy_config_request_t *sl_cfg = &sl_mac->sl_phy_config.sl_config_req;
  uint8_t mu = sl_cfg->sl_bwp_config.sl_scs;
  uint8_t n_slots_frame = nr_slots_per_frame[mu];
  const int n_ul_slots_period = tdd ? tdd->nrofUplinkSlots + (tdd->nrofUplinkSymbols > 0 ? 1 : 0) : n_slots_frame;
  const int nr_slots_period = tdd ? n_slots_frame / get_nb_periods_per_frame(tdd->dl_UL_TransmissionPeriodicity) : n_slots_frame;
  const int ideal_period = (CUR_SL_UE_CONNECTIONS * nr_slots_period) / n_ul_slots_period; //Jin replace
  const int first_ul_slot_period = tdd ? get_first_ul_slot(tdd->nrofDownlinkSlots, tdd->nrofDownlinkSymbols, tdd->nrofUplinkSymbols) : 0;
  const int idx = (uid << 1) + is_rsrp;
  SL_CSI_Report_t *csi_report = &sched_ctrl->sched_csi_report;
  const int offset = first_ul_slot_period + idx % n_ul_slots_period + (idx / n_ul_slots_period) * nr_slots_period;
  AssertFatal(offset < 320, "Not enough UL slots to accomodate all possible UEs. Need to rework the implementation\n");
  csi_report->slot_offset = offset;
  if (ideal_period < 5) {
    csi_report->slot_periodicity_offset = NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots4;
  } else if (ideal_period < 6) {
    csi_report->slot_periodicity_offset = NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots5;
  } else if (ideal_period < 9) {
    csi_report->slot_periodicity_offset = NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots8;
  } else if (ideal_period < 11) {
    csi_report->slot_periodicity_offset = NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots10;
  } else if (ideal_period < 17) {
    csi_report->slot_periodicity_offset = NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots16;
  } else if (ideal_period < 21) {
    csi_report->slot_periodicity_offset = NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots20;
  } else if (ideal_period < 41) {
    csi_report->slot_periodicity_offset = NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots40;
  } else if (ideal_period < 81) {
    csi_report->slot_periodicity_offset = NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots80;
  } else if (ideal_period < 161) {
    csi_report->slot_periodicity_offset = NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots160;
  } else {
    csi_report->slot_periodicity_offset = NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots320;
  }
  return csi_report;
}

void nr_ue_sl_csi_period_offset(SL_CSI_Report_t *sl_csi_report,
                                int *period,
                                int *offset) {
  *offset = sl_csi_report->slot_offset;
  switch(sl_csi_report->slot_periodicity_offset) {
    case NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots4:
      *period = 4;
      break;
    case NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots5:
      *period = 5;
      break;
    case NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots8:
      *period = 8;
      break;
    case NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots10:
      *period = 10;
      break;
    case NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots16:
      *period = 16;
      break;
    case NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots20:
      *period = 20;
      break;
    case NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots32:
      *period = 32;
      break;
    case NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots40:
      *period = 40;
      break;
    case NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots64:
      *period = 64;
      break;
    case NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots80:
      *period = 80;
      break;
    case NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots160:
      *period = 160;
      break;
    case NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots320:
      *period = 320;
      break;
    case NR_UE_SL_CSI_ResourcePeriodicityAndOffset_PR_slots640:
      *period = 640;
      break;
    default:
      AssertFatal(1 == 0, "No periodicity and offset found in CSI resource");
  }
}
