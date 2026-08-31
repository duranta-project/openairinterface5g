/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "mac_proto.h"
#include "common/utils/ocp_itti/intertask_interface.h"
#include "../nr_rlc/nr_rlc_oai_api.h"

extern int stop_L1(module_id_t gnb_id);

void mac_get_ue_rnti(MessageDef *msg_p, instance_t instance)
{
  UNUSED(msg_p);
  MessageDef *resp_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_GET_UE_RNTI);

  if (!RC.nrmac || instance >= RC.nb_nr_macrlc_inst || !RC.nrmac[instance]) {
    LOG_D(NR_MAC, "MAC_GET_UE_RNTI: No MAC instance available for instance %ld, returning RNTI 0\n", instance);
    resp_p->ittiMsg.mac_get_ue_rnti.rnti = 0;
    resp_p->ittiMsg.mac_get_ue_rnti.has_mac = false;
  } else {
    gNB_MAC_INST *mac = RC.nrmac[instance];
    NR_SCHED_LOCK(&mac->sched_lock);
    NR_UE_info_t *ue = NULL;
    UE_iterator(mac->UE_info.connected_ue_list, it) {
      ue = it;
      break;
    }
    NR_SCHED_UNLOCK(&mac->sched_lock);

    resp_p->ittiMsg.mac_get_ue_rnti.rnti = ue ? ue->rnti : 0;
    resp_p->ittiMsg.mac_get_ue_rnti.has_mac = true;
    LOG_D(NR_MAC, "MAC_GET_UE_RNTI: Found UE with RNTI %x\n", resp_p->ittiMsg.mac_get_ue_rnti.rnti);
  }
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

void mac_get_ue_rnti_by_uid(MessageDef *msg_p, instance_t instance)
{
  uid_t uid = msg_p->ittiMsg.mac_get_ue_rnti_by_uid.uid;
  MessageDef *resp_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_GET_UE_RNTI_BY_UID);
  resp_p->ittiMsg.mac_get_ue_rnti_by_uid.uid = uid;

  if (!RC.nrmac || instance >= RC.nb_nr_macrlc_inst || !RC.nrmac[instance]) {
    LOG_D(NR_MAC, "MAC_GET_UE_RNTI_BY_UID: No MAC instance available for instance %ld, returning RNTI 0\n", instance);
    resp_p->ittiMsg.mac_get_ue_rnti_by_uid.rnti = 0;
    resp_p->ittiMsg.mac_get_ue_rnti_by_uid.has_mac = false;
  } else {
    gNB_MAC_INST *mac = RC.nrmac[instance];
    NR_SCHED_LOCK(&mac->sched_lock);
    NR_UE_info_t *UE = NULL;
    UE_iterator(mac->UE_info.connected_ue_list, it) {
      if (it->uid == uid) {
        UE = it;
        break;
      }
    }
    NR_SCHED_UNLOCK(&mac->sched_lock);

    resp_p->ittiMsg.mac_get_ue_rnti_by_uid.rnti = UE ? UE->rnti : 0;
    resp_p->ittiMsg.mac_get_ue_rnti_by_uid.has_mac = true;
  }
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

void mac_force_ul_failure(MessageDef *msg_p, instance_t instance)
{
  rnti_t rnti = msg_p->ittiMsg.mac_force_ul_failure.rnti;
  MessageDef *resp_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_FORCE_UL_FAILURE);

  if (!RC.nrmac || instance >= RC.nb_nr_macrlc_inst || !RC.nrmac[instance]) {
    LOG_E(NR_MAC, "MAC_FORCE_UL_FAILURE: No MAC instance available for instance %ld, RNTI %x\n", instance, rnti);
    itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
    return;
  }

  gNB_MAC_INST *mac = RC.nrmac[instance];

  NR_SCHED_LOCK(&mac->sched_lock);
  NR_UE_info_t *UE = find_nr_UE(&mac->UE_info, rnti);

  if (UE) {
    LOG_D(NR_MAC, "MAC_FORCE_UL_FAILURE: Found UE for RNTI %x, triggering UL failure\n", rnti);
    nr_mac_trigger_ul_failure(&UE->UE_sched_ctrl, UE->current_UL_BWP.scs);
  } else {
    LOG_E(NR_MAC, "MAC_FORCE_UL_FAILURE: UE not found for RNTI %x\n", rnti);
  }
  NR_SCHED_UNLOCK(&mac->sched_lock);

  LOG_D(NR_MAC, "Sending MAC_FORCE_UL_FAILURE response to TELNET\n");
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

void mac_force_ue_release(MessageDef *msg_p, instance_t instance)
{
  rnti_t rnti = msg_p->ittiMsg.mac_force_ue_release.rnti;
  MessageDef *resp_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_FORCE_UE_RELEASE);

  if (!RC.nrmac || instance >= RC.nb_nr_macrlc_inst || !RC.nrmac[instance]) {
    LOG_E(NR_MAC, "MAC_FORCE_UE_RELEASE: No MAC instance available for instance %ld, RNTI %x\n", instance, rnti);
    itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
    return;
  }

  gNB_MAC_INST *mac = RC.nrmac[instance];

  NR_SCHED_LOCK(&mac->sched_lock);
  NR_UE_info_t *UE = find_nr_UE(&mac->UE_info, rnti);

  if (UE) {
    LOG_D(NR_MAC, "MAC_FORCE_UE_RELEASE: Found UE for RNTI %x, setting ul_failure_timer and checking UL failure\n", rnti);
    NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    sched_ctrl->ul_failure_timer = 2;
    nr_mac_check_ul_failure(mac, UE->rnti, sched_ctrl);
  } else {
    LOG_E(NR_MAC, "MAC_FORCE_UE_RELEASE: UE not found for RNTI %x\n", rnti);
  }
  NR_SCHED_UNLOCK(&mac->sched_lock);

  LOG_D(NR_MAC, "Sending MAC_FORCE_UE_RELEASE response to TELNET\n");
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

void mac_get_ue_bwp_info(MessageDef *msg_p, instance_t instance)
{
  rnti_t rnti = msg_p->ittiMsg.mac_get_ue_bwp_info.rnti;
  MessageDef *resp_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_GET_UE_BWP_INFO);
  resp_p->ittiMsg.mac_get_ue_bwp_info.rnti = rnti;
  resp_p->ittiMsg.mac_get_ue_bwp_info.dl_bwp_id = -1;
  resp_p->ittiMsg.mac_get_ue_bwp_info.ul_bwp_id = -1;

  if (!RC.nrmac || instance >= RC.nb_nr_macrlc_inst || !RC.nrmac[instance]) {
    LOG_E(NR_MAC, "MAC_GET_UE_BWP_INFO: No MAC instance available for instance %ld, RNTI %x\n", instance, rnti);
    itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
    return;
  }

  gNB_MAC_INST *mac = RC.nrmac[instance];

  NR_SCHED_LOCK(&mac->sched_lock);
  NR_UE_info_t *UE = find_nr_UE(&mac->UE_info, rnti);

  if (UE) {
    LOG_D(NR_MAC, "MAC_GET_UE_BWP_INFO: Found UE for RNTI %x\n", rnti);
    resp_p->ittiMsg.mac_get_ue_bwp_info.dl_bwp_id = UE->current_DL_BWP.bwp_id;
    resp_p->ittiMsg.mac_get_ue_bwp_info.ul_bwp_id = UE->current_UL_BWP.bwp_id;
  } else {
    LOG_E(NR_MAC, "MAC_GET_UE_BWP_INFO: UE not found for RNTI %x\n", rnti);
  }
  NR_SCHED_UNLOCK(&mac->sched_lock);

  LOG_D(NR_MAC, "Sending MAC_GET_UE_BWP_INFO response to TELNET\n");
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

void mac_set_pusch_target_snr(MessageDef *msg_p, instance_t instance)
{
  long target_snrx10 = msg_p->ittiMsg.mac_set_pusch_target_snr.target_snrx10;
  MessageDef *resp_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_SET_PUSCH_TARGET_SNR);

  if (!RC.nrmac || instance >= RC.nb_nr_macrlc_inst || !RC.nrmac[instance]) {
    LOG_E(NR_MAC, "MAC_SET_PUSCH_TARGET_SNR: No MAC instance available for instance %ld\n", instance);
    itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
    return;
  }

  gNB_MAC_INST *mac = RC.nrmac[instance];

  NR_SCHED_LOCK(&mac->sched_lock);
  UE_iterator(mac->UE_info.connected_ue_list, it) {
    nr_mac_set_target_snrx10(&it->UE_sched_ctrl.pusch_pc, target_snrx10);
  }
  NR_SCHED_UNLOCK(&mac->sched_lock);

  LOG_D(NR_MAC, "MAC_SET_PUSCH_TARGET_SNR: Set target SNR to %ld for all UEs\n", target_snrx10);
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

void mac_set_bwconfig_handler(MessageDef *msg_p, instance_t instance)
{
  int bw_value = msg_p->ittiMsg.mac_set_bwconfig.bw_value;
  MessageDef *resp_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_SET_BWCONFIG);

  if (!RC.nrmac || instance >= RC.nb_nr_macrlc_inst || !RC.nrmac[instance]) {
    LOG_E(NR_MAC, "MAC_SET_BWCONFIG: No MAC instance available for instance %ld\n", instance);
    itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
    return;
  }

  gNB_MAC_INST *mac = RC.nrmac[instance];
  NR_ServingCellConfigCommon_t *scc = mac->cells[0].common_channels.ServingCellConfigCommon;
  NR_FrequencyInfoDL_t *frequencyInfoDL = scc->downlinkConfigCommon->frequencyInfoDL;
  NR_BWP_t *initialDL = &scc->downlinkConfigCommon->initialDownlinkBWP->genericParameters;
  NR_FrequencyInfoUL_t *frequencyInfoUL = scc->uplinkConfigCommon->frequencyInfoUL;
  NR_BWP_t *initialUL = &scc->uplinkConfigCommon->initialUplinkBWP->genericParameters;

   if (bw_value == 40) {
    *scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencySSB = 641280;
    frequencyInfoDL->absoluteFrequencyPointA = 640008;
    AssertFatal(frequencyInfoUL->absoluteFrequencyPointA == NULL, "only handle TDD\n");
    frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth = 106;
    initialDL->locationAndBandwidth = 28875;
    frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth = 106;
    initialUL->locationAndBandwidth = 28875;
    get_softmodem_params()->threequarter_fs = 1;
  } else if (bw_value == 20) {
    *scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencySSB = 641280;
    frequencyInfoDL->absoluteFrequencyPointA = 640596;
    AssertFatal(frequencyInfoUL->absoluteFrequencyPointA == NULL, "only handle TDD\n");
    frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth = 51;
    initialDL->locationAndBandwidth = 13750;
    frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth = 51;
    initialUL->locationAndBandwidth = 13750;
    get_softmodem_params()->threequarter_fs = 0;
  } else if (bw_value == 100) {
    *scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencySSB = 646668;
    frequencyInfoDL->absoluteFrequencyPointA = 643392;
    AssertFatal(frequencyInfoUL->absoluteFrequencyPointA == NULL, "only handle TDD\n");
    frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth = 273;
    initialDL->locationAndBandwidth = 1099;
    frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth = 273;
    initialUL->locationAndBandwidth = 1099;
    get_softmodem_params()->threequarter_fs = 0;
  } else if (bw_value == 60) {
    *scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencySSB = 621984;
    frequencyInfoDL->absoluteFrequencyPointA = 620040;
    AssertFatal(frequencyInfoUL->absoluteFrequencyPointA == NULL, "only handle TDD\n");
    frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth = 162;
    initialDL->locationAndBandwidth = 31624;
    frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth = 162;
    initialUL->locationAndBandwidth = 31624;
    get_softmodem_params()->threequarter_fs = 0;
  } else {
    LOG_E(NR_MAC, "MAC_SET_BWCONFIG: unhandled bw_value %d\n", bw_value);
    itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
    return;
  }

  nr_cell_sched_t *cell = nr_mac_get_cell_by_phy_id(RC.nrmac[0], 0);
  free(cell->sched_ctrlSIB1);
  cell->sched_ctrlSIB1 = NULL;

  free_MIB_NR(cell->common_channels.mib);
  cell->common_channels.mib = get_new_MIB_NR(scc);

  const f1ap_served_cell_info_t *info = &mac->f1_config.setup_req->cell[0].info;
  nr_mac_configure_sib1(cell, &info->plmn, info->nr_cellid, *info->tac);

  LOG_D(NR_MAC, "MAC_SET_BWCONFIG: Bandwidth configuration updated to %d MHz\n", bw_value);
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

void mac_stop_modem_handler(MessageDef *msg_p, instance_t instance)
{
  UNUSED(msg_p);
  MessageDef *resp_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_STOP_MODEM);

  if (!RC.nrmac || instance >= RC.nb_nr_macrlc_inst || !RC.nrmac[instance]) {
    LOG_E(NR_MAC, "MAC_STOP_MODEM: No MAC instance available for instance %ld\n", instance);
    itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
    return;
  }

  gNB_MAC_INST *mac = RC.nrmac[instance];

  /* make UEs out of sync and wait 50ms to ensure no PUCCH is scheduled. After
   * a restart, the frame/slot numbers will be different, which "confuses" the
   * scheduler, which has many PUCCH structures filled with expected frame/slot
   * combinations that won't happen. */
  UE_iterator((NR_UE_info_t **)mac->UE_info.connected_ue_list, it) {
    nr_mac_trigger_ul_failure(&it->UE_sched_ctrl, 1);
  }
  usleep(50000);

  stop_L1(0);
  LOG_D(NR_MAC, "MAC_STOP_MODEM: L1 stopped\n");
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

void mac_get_o1_stats_handler(MessageDef *msg_p, instance_t instance)
{
  LOG_D(NR_MAC, "MAC_GET_O1_STATS: Handler started\n");

  MessageDef *resp_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_GET_O1_STATS);
  if (!resp_p) {
    LOG_E(NR_MAC, "MAC_GET_O1_STATS: Failed to allocate ITTI message\n");
    return;
  }
  LOG_D(NR_MAC, "MAC_GET_O1_STATS: ITTI message allocated\n");

  Mac_get_o1_stats *stats = &resp_p->ittiMsg.mac_get_o1_stats;
  memset(stats, 0, sizeof(*stats));

  if (!RC.nrmac || instance >= RC.nb_nr_macrlc_inst || !RC.nrmac[instance]) {
    LOG_E(NR_MAC, "MAC_GET_O1_STATS: No MAC instance available for instance %ld\n", instance);
    itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
    return;
  }

  LOG_D(NR_MAC, "MAC_GET_O1_STATS: MAC instance found\n");
  gNB_MAC_INST *mac = RC.nrmac[instance];

  LOG_D(NR_MAC, "MAC_GET_O1_STATS: Reading F1 config\n");
  if (!mac->f1_config.setup_req) {
    LOG_E(NR_MAC, "MAC_GET_O1_STATS: F1 setup_req is NULL\n");
    itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
    return;
  }

  const f1ap_setup_req_t *sr = mac->f1_config.setup_req;
  const f1ap_served_cell_info_t *cell_info = &sr->cell[0].info;

  stats->gNB_DU_id = sr->gNB_DU_id;
  strncpy(stats->gNB_DU_name, sr->gNB_DU_name, sizeof(stats->gNB_DU_name) - 1);
  LOG_D(NR_MAC, "MAC_GET_O1_STATS: F1 config read (gNB_DU_id=%u, gNB_DU_name=%s)\n", stats->gNB_DU_id, stats->gNB_DU_name);

  LOG_D(NR_MAC, "MAC_GET_O1_STATS: Reading cell config\n");
  if (!mac->cells[0].common_channels.ServingCellConfigCommon) {
    LOG_E(NR_MAC, "MAC_GET_O1_STATS: ServingCellConfigCommon is NULL\n");
    itti_send_msg_to_task(TASK_TELNET, 0, resp_p);

    return;
  }

  const NR_ServingCellConfigCommon_t *scc = mac->cells[0].common_channels.ServingCellConfigCommon;
  if (!scc->downlinkConfigCommon || !scc->downlinkConfigCommon->frequencyInfoDL || !scc->uplinkConfigCommon || !scc->uplinkConfigCommon->frequencyInfoUL) {
    LOG_E(NR_MAC, "MAC_GET_O1_STATS: frequencyInfoDL or frequencyInfoUL is NULL\n");
    itti_send_msg_to_task(TASK_TELNET, 0, resp_p);

    return;
  }

  const NR_FrequencyInfoDL_t *frequencyInfoDL = scc->downlinkConfigCommon->frequencyInfoDL;
  const NR_FrequencyInfoUL_t *frequencyInfoUL = scc->uplinkConfigCommon->frequencyInfoUL;

  stats->frame_type = get_frame_type(*frequencyInfoDL->frequencyBandList.list.array[0], *scc->ssbSubcarrierSpacing);

  stats->ssbFrequency = *scc->downlinkConfigCommon->frequencyInfoDL->absoluteFrequencySSB;
  stats->arfcnDL = frequencyInfoDL->absoluteFrequencyPointA;
  stats->arfcnUL = frequencyInfoUL->absoluteFrequencyPointA ? *frequencyInfoUL->absoluteFrequencyPointA : frequencyInfoDL->absoluteFrequencyPointA;

  int scs = scc->downlinkConfigCommon->initialDownlinkBWP->genericParameters.subcarrierSpacing;
  stats->scs = scs;

  int band = *frequencyInfoDL->frequencyBandList.list.array[0];
  int nrb = frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth;
  frequency_range_t fr = band > 256 ? FR2 : FR1;
  int bw_index = get_supported_band_index(scs, fr, nrb);
  stats->bw_mhz = get_supported_bw_mhz(fr, bw_index);

  stats->pci = *scc->physCellId;
  stats->tac = *cell_info->tac;
  stats->mcc = cell_info->plmn.mcc;
  stats->mnc = cell_info->plmn.mnc;
  stats->mnc_digit_length = cell_info->plmn.mnc_digit_length;
  stats->sd = cell_info->nssai[0].sd;
  stats->sst = cell_info->nssai[0].sst;
  stats->band = band;

  LOG_D(NR_MAC, "MAC_GET_O1_STATS: Cell config read (pci=%lu, tac=%lu, band=%d, scs=%d, bw_mhz=%lu)\n",
        stats->pci, stats->tac, stats->band, stats->scs, stats->bw_mhz);

  LOG_D(NR_MAC, "MAC_GET_O1_STATS: Reading BWP config\n");
  const NR_BWP_t *initialDL = &scc->downlinkConfigCommon->initialDownlinkBWP->genericParameters;
  const NR_BWP_t *initialUL = &scc->uplinkConfigCommon->initialUplinkBWP->genericParameters;

  stats->dl_numrbs = NRRIV2BW(initialDL->locationAndBandwidth, MAX_BWP_SIZE);
  stats->dl_startrb = NRRIV2PRBOFFSET(initialDL->locationAndBandwidth, MAX_BWP_SIZE);
  stats->dl_bwpscs = 15 * (1U << scs);
  stats->ul_numrbs = NRRIV2BW(initialUL->locationAndBandwidth, MAX_BWP_SIZE);
  stats->ul_startrb = NRRIV2PRBOFFSET(initialUL->locationAndBandwidth, MAX_BWP_SIZE);
  stats->ul_bwpscs = 15 * (1U << scs);

  LOG_D(NR_MAC, "MAC_GET_O1_STATS: BWP config read (dl_numrbs=%lu, ul_numrbs=%lu)\n", stats->dl_numrbs, stats->ul_numrbs);

  stats->dl_total_prb_aggregate = mac->mac_stats.dl.total_prb_aggregate;
  stats->dl_used_prb_aggregate = mac->mac_stats.dl.used_prb_aggregate;
  LOG_D(NR_MAC, "MAC_GET_O1_STATS: MAC stats read (dl_total_prb=%llu, dl_used_prb=%llu)\n",
        (unsigned long long)stats->dl_total_prb_aggregate, (unsigned long long)stats->dl_used_prb_aggregate);

  LOG_D(NR_MAC, "MAC_GET_O1_STATS: Collecting UE RLC stats\n");
  stats->num_ues = 0;
  UE_iterator((NR_UE_info_t **)mac->UE_info.connected_ue_list, it) {
    if (stats->num_ues >= MAX_MOBILES_PER_GNB) {
      LOG_D(NR_MAC, "MAC_GET_O1_STATS: Max UEs reached (%d)\n", MAX_MOBILES_PER_GNB);
      break;
    }

    LOG_D(NR_MAC, "MAC_GET_O1_STATS: Processing UE with RNTI %x, calling nr_rlc_get_statistics\n", it->rnti);
    nr_rlc_statistics_t rlc = {0};
    nr_rlc_get_statistics(it->rnti, 0, 1, &rlc); // srb_flag=0, rb_id=1
    LOG_D(NR_MAC, "MAC_GET_O1_STATS: nr_rlc_get_statistics returned for RNTI %x\n", it->rnti);

    stats->ue_rlc_stats[stats->num_ues].rnti = it->rnti;
    stats->ue_rlc_stats[stats->num_ues].txpdu_bytes = rlc.txpdu_bytes;
    stats->ue_rlc_stats[stats->num_ues].rxpdu_bytes = rlc.rxpdu_bytes;
    LOG_D(NR_MAC, "MAC_GET_O1_STATS: UE %d: RNTI=%x, txpdu_bytes=%llu, rxpdu_bytes=%llu\n",
          stats->num_ues, it->rnti, (unsigned long long)rlc.txpdu_bytes, (unsigned long long)rlc.rxpdu_bytes);
    stats->num_ues++;
  }
  LOG_D(NR_MAC, "MAC_GET_O1_STATS: Processed %d UEs\n", stats->num_ues);

  clock_gettime(CLOCK_MONOTONIC, &stats->tp_now);
  LOG_D(NR_MAC, "MAC_GET_O1_STATS: Timestamp set\n");

  LOG_D(NR_MAC, "MAC_GET_O1_STATS: Sending response to TASK_TELNET\n");

  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
  LOG_D(NR_MAC, "MAC_GET_O1_STATS: Handler completed successfully\n");
}

void *mac_gnb_task(void *args_p)
{
  UNUSED(args_p);
  MessageDef *msg_p;
  instance_t instance;
  int result;

  itti_mark_task_ready(TASK_MAC_GNB);
  LOG_D(NR_MAC, "Entering main loop of NR_MAC message task\n");

  while (1) {
    itti_receive_msg(TASK_MAC_GNB, &msg_p);
    instance = ITTI_MSG_DESTINATION_INSTANCE(msg_p);
    LOG_D(NR_MAC,
          "MAC GNB Task Received %s for instance %ld from task %s\n",
          ITTI_MSG_NAME(msg_p),
          instance,
          ITTI_MSG_ORIGIN_NAME(msg_p));

    switch (ITTI_MSG_ID(msg_p)) {
      case TERMINATE_MESSAGE:
        LOG_W(NR_MAC, " *** Exiting NR_MAC thread\n");
        itti_exit_task();
        break;

      case MESSAGE_TEST:
        LOG_D(NR_MAC, "[gNB %ld] Received %s\n", instance, ITTI_MSG_NAME(msg_p));
        break;

      case MAC_GET_UE_RNTI:
        LOG_D(NR_MAC, "MAC Task: Processing MAC_GET_UE_RNTI message\n");
        mac_get_ue_rnti(msg_p, instance);
        break;

      case MAC_GET_UE_RNTI_BY_UID:
        LOG_D(NR_MAC, "MAC Task: Processing MAC_GET_UE_RNTI_BY_UID message\n");
        mac_get_ue_rnti_by_uid(msg_p, instance);
        break;

      case MAC_FORCE_UL_FAILURE:
        LOG_D(NR_MAC, "MAC Task: Processing MAC_FORCE_UL_FAILURE message for RNTI %d\n", msg_p->ittiMsg.mac_force_ul_failure.rnti);
        mac_force_ul_failure(msg_p, instance);
        break;

      case MAC_FORCE_UE_RELEASE:
        LOG_D(NR_MAC, "MAC Task: Processing MAC_FORCE_UE_RELEASE message for RNTI %d\n", msg_p->ittiMsg.mac_force_ue_release.rnti);
        mac_force_ue_release(msg_p, instance);
        break;

      case MAC_GET_UE_BWP_INFO:
        LOG_D(NR_MAC, "MAC Task: Processing MAC_GET_UE_BWP_INFO message for RNTI %d\n", msg_p->ittiMsg.mac_get_ue_bwp_info.rnti);
        mac_get_ue_bwp_info(msg_p, instance);
        break;

      case MAC_SET_PUSCH_TARGET_SNR:
        LOG_D(NR_MAC, "MAC Task: Processing MAC_SET_PUSCH_TARGET_SNR message\n");
        mac_set_pusch_target_snr(msg_p, instance);
        break;

      case MAC_GET_O1_STATS:
        LOG_D(NR_MAC, "MAC Task: Processing MAC_GET_O1_STATS message\n");
        mac_get_o1_stats_handler(msg_p, instance);
        break;

      case MAC_SET_BWCONFIG:
        LOG_D(NR_MAC, "MAC Task: Processing MAC_SET_BWCONFIG message\n");
        mac_set_bwconfig_handler(msg_p, instance);
        break;

      case MAC_STOP_MODEM:
        LOG_D(NR_MAC, "MAC Task: Processing MAC_STOP_MODEM message\n");
        mac_stop_modem_handler(msg_p, instance);
        break;

      default:
        LOG_E(NR_MAC, "[gNB %ld] Received unexpected message %s\n", instance, ITTI_MSG_NAME(msg_p));
        break;
    }

    result = itti_free(ITTI_MSG_ORIGIN_ID(msg_p), msg_p);
    AssertFatal(result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
    msg_p = NULL;
  }

  return NULL;
}
