/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#include <common/utils/nr/nr_common.h>

#include "NR_MAC_COMMON/nr_mac.h"
#include "NR_MAC_COMMON/nr_mac_common.h"
#include "NR_MAC_UE/mac_proto.h"
#include "NR_MAC_UE/nr_ue_sci.h"
#include <executables/nr-uesoftmodem.h>
#include "NR_MAC_UE/mac_defs_sl.h"
#include "NR_MAC_gNB/nr_mac_gNB.h"

#define LOWER_BLER 0.05
#define UPPER_BLER 0.15
#define MAX_MCS 28

const uint8_t nr_rv_round_map[4] = {0, 2, 3, 1};

static int get_sl_mcs_from_bler(const NR_bler_options_t *bler_options,
                                const NR_mac_dir_stats_t *stats,
                                NR_bler_stats_t *bler_stats,
                                int max_mcs,
                                frame_t frame)
{
  /* Link adaptation is implementation-specific, but it must not interpret a
   * low packet arrival rate as a decoding failure. Accumulate enough actual
   * transmissions for get_mcs_from_bler() to form a BLER sample. */
  const uint64_t new_transmissions = stats->rounds[0] - bler_stats->rounds[0];
  if (new_transmissions <= 3)
    return min(bler_stats->mcs, max_mcs);
  return get_mcs_from_bler(bler_options, stats, bler_stats, max_mcs, frame);
}

/* A HARQ TB counts against sched_sl_bytes from its initial transmission until
 * it is ACKed or discarded.  Keep the release operation in one place because
 * PSFCH and legacy SLSCH feedback use different completion paths. */
static void release_nr_ue_sl_harq(NR_SL_UE_sched_ctrl_t *sched_ctrl, int8_t harq_pid)
{
  NR_UE_sl_harq_t *harq = &sched_ctrl->sl_harq_processes[harq_pid];
  const uint32_t tb_size = harq->sched_pssch.tb_size;

  harq->feedback_frame = -1;
  harq->feedback_slot = -1;
  harq->is_waiting = false;
  harq->round = 0;
  sched_ctrl->sched_sl_bytes -= tb_size;
  if (sched_ctrl->sched_sl_bytes < 0) {
    LOG_E(NR_MAC,
          "sched_sl_bytes underflow (was %d, tb_size %u) releasing HARQ pid %d -- mismatched increment/decrement\n",
          sched_ctrl->sched_sl_bytes + (int)tb_size,
          tb_size,
          harq_pid);
    sched_ctrl->sched_sl_bytes = 0;
  }
  add_tail_nr_list(&sched_ctrl->available_sl_harq, harq_pid);
}

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
  memset(sched_ctrl->sl_rx_harq_processes, 0, sizeof(sched_ctrl->sl_rx_harq_processes));
  /* All in-flight HARQs were discarded above; reset the byte accounting so it
   * does not accumulate across resets (e.g. sync loss -> re-init). */
  sched_ctrl->sched_sl_bytes = 0;
}

void abort_nr_ue_sl_harq(NR_UE_MAC_INST_t *mac, int8_t harq_pid, NR_SL_UE_info_t *UE_info)
{
  NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE_info->UE_sched_ctrl;

  UE_info->mac_sl_stats.sl.errors++;
  /* The transmission failed: release both the HARQ process and its in-flight
   * byte accounting so data can be retrieved on the next RLC status poll. */
  release_nr_ue_sl_harq(sched_ctrl, harq_pid);
}

void handle_nr_ue_sl_harq(module_id_t mod_id,
                          frame_t frame,
                          sub_frame_t slot,
                          sl_nr_slsch_pdu_t *rx_slsch_pdu,
                          uint16_t src_id)
{
  NR_UE_MAC_INST_t *mac = get_mac_inst(mod_id);
  NR_UE_SL_SCHED_LOCK(&mac->sl_sched_lock);
  NR_SL_UE_info_t *UE = find_UE(mac, src_id);
  if (UE == NULL) {
    LOG_W(NR_MAC, "Ignoring PSFCH feedback from unknown sidelink source ID %u\n", src_id);
    NR_UE_SL_SCHED_UNLOCK(&mac->sl_sched_lock);
    return;
  }
  uint8_t num_ack_rcvd = rx_slsch_pdu->num_acks_rcvd;

  NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  // TS 38.321 Section 5.22.1.2.1: purge stale entries (feedback_slot < current slot)
  // before searching for current-slot HARQs. Without this, a HARQ that was mapped to
  // an earlier PSFCH period but whose PSFCH was never received (DTX/NACK) remains in
  // the feedback list instead of being queued for retransmission.
  // update_harq_lists() is also called from the TX scheduler (preprocess()), but that
  // runs after PSFCH RX in the same slot -- calling it here first is safe and idempotent.
  update_harq_lists(mac, frame, slot, UE);
  NR_UE_sl_harq_t **matched_harqs = (NR_UE_sl_harq_t **) calloc(sched_ctrl->feedback_sl_harq.len, sizeof(NR_UE_sl_harq_t *));
  int k = find_current_slot_harqs(frame, slot, sched_ctrl, matched_harqs);
  LOG_D(NR_MAC, "Found %d matching HARQ processes vs. num. of received acks %d\n", k, num_ack_rcvd);
  if (num_ack_rcvd > k)
    LOG_W(NR_MAC,
          "Received %u PSFCH results but only %d HARQ processes expect feedback in %4u.%2u\n",
          num_ack_rcvd,
          k,
          frame,
          slot);

  const int num_matched_acks = min((int)num_ack_rcvd, k);
  for (int i = 0; i < num_matched_acks; i++) {
    uint8_t ack_nack = rx_slsch_pdu->ack_nack_rcvd[i];
    int8_t harq_pid = matched_harqs[i]->sl_harq_pid;
    remove_nr_list(&sched_ctrl->feedback_sl_harq, harq_pid);
    NR_UE_sl_harq_t *harq = &sched_ctrl->sl_harq_processes[harq_pid];
    DevAssert(harq->is_waiting);
    harq->feedback_slot = -1;
    harq->is_waiting = false;
    if (!ack_nack) {
      UE->mac_sl_stats.cumul_round[harq->round]++;
      release_nr_ue_sl_harq(sched_ctrl, harq_pid);
      LOG_D(NR_MAC,
            "%4u.%2u Slharq id %d crc passed for src id %4d\n",
            frame,
            slot,
            harq_pid,
            src_id);
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
  // TS 38.321 Section 5.22.1.2.1 / TS 38.213 Section 16.3: when sl-PSFCH-Period > 1,
  // multiple PSSCH transmissions map to the same PSFCH slot. If fewer PSFCH PDUs were
  // decoded than expected (k > num_ack_rcvd), the remaining HARQs received no feedback
  // (DTX) and must be treated as NACK. When num_ack_rcvd == k this loop runs zero
  // times -- all matched HARQs had feedback, nothing to do.
  for (int i = num_matched_acks; i < k; i++) {
    int8_t harq_pid = matched_harqs[i]->sl_harq_pid;
    NR_UE_sl_harq_t *harq = &sched_ctrl->sl_harq_processes[harq_pid];
    UE->mac_sl_stats.slsch_DTX++;
    LOG_D(NR_MAC, "%4u.%2u DTX: no PSFCH received for HARQ PID %d, treating as NACK\n", frame, slot, harq_pid);
    remove_nr_list(&sched_ctrl->feedback_sl_harq, harq_pid);
    harq->is_waiting = false;
    harq->feedback_slot = -1;
    if (harq->round >= (HARQ_ROUND_MAX - 1)) {
      UE->mac_sl_stats.cumul_round[HARQ_ROUND_MAX]++;
      abort_nr_ue_sl_harq(mac, harq_pid, UE);
    } else {
      harq->round++;
      add_tail_nr_list(&sched_ctrl->retrans_sl_harq, harq_pid);
    }
  }
  free(matched_harqs);
  matched_harqs = NULL;
  NR_UE_SL_SCHED_UNLOCK(&mac->sl_sched_lock);
}

static bool nr_list_contains(const NR_list_t *list, int id)
{
  for (int cur = list->head; cur >= 0; cur = list->next[cur]) {
    if (cur == id)
      return true;
  }
  return false;
}

void handle_nr_ue_sl_psfch(module_id_t mod_id,
                           frame_t frame,
                           sub_frame_t slot,
                           const sl_nr_psfch_pdu_t *rx_psfch_pdu)
{
  NR_UE_MAC_INST_t *mac = get_mac_inst(mod_id);
  NR_UE_SL_SCHED_LOCK(&mac->sl_sched_lock);

  for (int i = 0; i < rx_psfch_pdu->num_results; i++) {
    const sl_nr_psfch_result_t *result = &rx_psfch_pdu->results[i];
    NR_SL_UE_info_t *UE = find_UE(mac, result->peer_id);
    if (UE == NULL) {
      LOG_W(NR_MAC, "%4u.%2u ignoring PSFCH for unknown peer %u\n", frame, slot, result->peer_id);
      continue;
    }
    if (result->harq_pid >= NR_MAX_HARQ_PROCESSES) {
      LOG_E(NR_MAC,
            "%4u.%2u ignoring PSFCH for invalid HARQ PID %u (peer %u)\n",
            frame,
            slot,
            result->harq_pid,
            result->peer_id);
      continue;
    }

    NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    NR_UE_sl_harq_t *harq = &sched_ctrl->sl_harq_processes[result->harq_pid];
    const bool in_feedback_list = nr_list_contains(&sched_ctrl->feedback_sl_harq, result->harq_pid);
    if (!harq->is_waiting || harq->feedback_frame != frame || harq->feedback_slot != slot || !in_feedback_list) {
      LOG_W(NR_MAC,
            "%4u.%2u ignoring stale/unexpected PSFCH for peer %u HARQ %u "
            "(waiting=%d listed=%d expected=%u.%u)\n",
            frame,
            slot,
            result->peer_id,
            result->harq_pid,
            harq->is_waiting,
            in_feedback_list,
            harq->feedback_frame,
            harq->feedback_slot);
      continue;
    }

    remove_nr_list(&sched_ctrl->feedback_sl_harq, result->harq_pid);
    harq->feedback_frame = -1;
    harq->feedback_slot = -1;
    harq->is_waiting = false;

    if (result->ack_nack < 0)
      UE->mac_sl_stats.slsch_DTX++;

    /* nr_ue_decode_pucch0() uses 0 for ACK and 1 for NACK.  A negative
     * decoder result is DTX/invalid and follows the NACK retransmission path. */
    if (result->ack_nack == 0) {
      UE->mac_sl_stats.cumul_round[harq->round]++;
      LOG_D(NR_MAC,
            "%4u.%2u PSFCH ACK: peer %u HARQ %u completed in round %u\n",
            frame,
            slot,
            result->peer_id,
            result->harq_pid,
            harq->round);
      release_nr_ue_sl_harq(sched_ctrl, result->harq_pid);
    } else if (harq->round >= HARQ_ROUND_MAX - 1) {
      UE->mac_sl_stats.cumul_round[HARQ_ROUND_MAX]++;
      LOG_W(NR_MAC,
            "%4u.%2u PSFCH %s: peer %u HARQ %u exhausted all rounds\n",
            frame,
            slot,
            result->ack_nack < 0 ? "DTX" : "NACK",
            result->peer_id,
            result->harq_pid);
      abort_nr_ue_sl_harq(mac, result->harq_pid, UE);
    } else {
      harq->round++;
      LOG_D(NR_MAC,
            "%4u.%2u PSFCH %s: peer %u HARQ %u queued for round %u (RV%u)\n",
            frame,
            slot,
            result->ack_nack < 0 ? "DTX" : "NACK",
            result->peer_id,
            result->harq_pid,
            harq->round,
            nr_rv_round_map[harq->round]);
      add_tail_nr_list(&sched_ctrl->retrans_sl_harq, result->harq_pid);
    }
  }

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
  uid_t dest_id = UE->uid;
  NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
  const NR_mac_dir_stats_t *stats = &UE->mac_sl_stats.sl;
  NR_sched_pssch_t *sched_pssch = &sched_ctrl->sched_pssch;
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;
  uint8_t mu = sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;

  uint8_t psfch_period = 0;
  const uint8_t psfch_periods[] = {0,1,2,4};
  psfch_period = (mac->sl_tx_res_pool->sl_PSFCH_Config_r16 &&
                  mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16)
                  ? psfch_periods[*mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16] : 0;
  *slsch_pdu_length_max = 0;

  //nr_ue_sl_csi_period_offset()
  // Determine current slot is csi-rs schedule slot
  bool csi_req_slot = false;
  bool csi_acq = false;

  uint8_t ri = 0;
  sched_pssch->mcs = sched_ctrl->sl_max_mcs;
  int mcs_tb_ind = 0;

  /* Calculate coeff */
  NR_bler_options_t *sl_bo = &sl_mac->sl_bler;
  sl_bo->lower = LOWER_BLER;
  sl_bo->upper = UPPER_BLER;
  sl_bo->max_mcs = MAX_MCS;

  const int max_mcs_table = mcs_tb_ind == 1 ? 27 : 28;
  int max_mcs = min(sched_ctrl->sl_max_mcs, max_mcs_table);
  if (cur_harq->round > 0) {
    // Start the retransmission TBS search with the round-0 MCS. A different
    // MCS is allowed, but the selected grant must derive the same TBS so that
    // the stored MAC PDU remains the transport block for this HARQ process.
    sched_pssch->mcs = cur_harq->sched_pssch.mcs;
  } else if (sl_bo->harq_round_max == 1) {
    sched_pssch->mcs = max_mcs;
  } else {
    sched_pssch->mcs = get_sl_mcs_from_bler(sl_bo, stats, &sched_ctrl->sl_bler_stats, max_mcs, frameP);
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
  uint8_t l_subch = resource->sl_subchan_len;
  uint8_t n_start_subch1 = resource->sl_subchan_start;
  uint8_t n_start_subch2 = 0; // Used only when a third resource is indicated.
  // Fill SCI1A
  sci_pdu->priority = 0;
  sci_pdu->frequency_resource_assignment.val = compute_FRIV(sl_max_num_reserve, l_subch, n_start_subch1, n_start_subch2, sl_num_subch);
  sci_pdu->time_resource_assignment.val = compute_TRIV(N, t1, t2);
  /* TS 38.212 8.3.1.1: this field is an index into sl-ResourceReservePeriodList,
   * not a period in ms.  With a single configured entry the index is always 0
   * and nbits is 0 (field absent); set val = 0 to be explicit. */
  sci_pdu->resource_reservation_period.val = 0;
  sci_pdu->dmrs_pattern.val = 0;
  sci_pdu->second_stage_sci_format = 0;
  sci_pdu->number_of_dmrs_port = ri;
  // we are using as a flag to indicate if csi report was received
  sci_pdu->mcs = sched_pssch->mcs;
  sci_pdu->additional_mcs.val = 0;
  /*Following code will check whether SLSCH was received before and
  its feedback has scheduled for current slot
  */
  bool is_feedback_slot = false;
  const int sched_psfch_size = mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch_size;
  for (int i = 0; i < sched_psfch_size; i++) {
    SL_sched_feedback_t  *sched_psfch = &mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch[i];
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
  /* TS 38.214 8.1.3.2: for periods 2 and 4, SCI 1-A signals whether
   * this PSSCH allocation has the three-symbol PSFCH overhead. */
  sci_pdu->psfch_overhead.val =
      (psfch_period == 2 || psfch_period == 4) && sl_has_psfch;
    LOG_D(NR_MAC, "%4d.%2d Setting psfch_overhead %d\n", frameP, slotP, sci_pdu->psfch_overhead.val);

  sci_pdu->reserved.val = mac->is_synced ? 1 : 0;
  sci_pdu->conflict_information_receiver.val = 0;
  sci_pdu->beta_offset_indicator = 0;
  sci2_pdu->harq_pid = cur_harq->sl_harq_pid;
  sci2_pdu->ndi = cur_harq->ndi;
  sci2_pdu->rv_index = nr_rv_round_map[cur_harq->round % 4];
  sci2_pdu->source_id = mac->src_id;
  sci2_pdu->dest_id = dest_id;
  /* TS 38.212 8.4.1.2: harq_feedback_enabled signals whether this pool has
   * PSFCH configured.  It is a pool property, not a per-process flag; using
   * cur_harq->is_waiting (which is false at TX time) would cause the receiver
   * to classify the transmission as NACK-only or no-feedback and suppress
   * PSFCH even when psfch_period > 0. */
  sci2_pdu->harq_feedback = (psfch_period > 0);
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
  /* tb_size is derived from MCS + N_RE in fill_pssch_pscch_pdu(); assigning
   * rlc_status->bytes_in_buffer here was a dead write -- the value is never
   * read by the caller (it uses pscch_pssch_pdu->tb_size instead). */
}
