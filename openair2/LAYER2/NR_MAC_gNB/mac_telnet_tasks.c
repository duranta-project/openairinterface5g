/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "mac_proto.h"
#include "common/utils/ocp_itti/intertask_interface.h"

void mac_get_ue_rnti(MessageDef *msg_p, instance_t instance)
{
  UNUSED(instance);
  UNUSED(msg_p);
  MessageDef *resp_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_GET_UE_RNTI);

  if (!RC.nrmac || !RC.nrmac[0]) {
    LOG_I(NR_MAC, "MAC_GET_UE_RNTI: No MAC instance available, returning RNTI 0\n");
    resp_p->ittiMsg.mac_get_ue_rnti.rnti = 0;
    resp_p->ittiMsg.mac_get_ue_rnti.has_mac = false;
  } else {
    gNB_MAC_INST *mac = RC.nrmac[0];
    NR_SCHED_LOCK(&mac->sched_lock);
    NR_UE_info_t *ue = NULL;
    UE_iterator(mac->UE_info.connected_ue_list, it) {
      ue = it;
      break;
    }
    NR_SCHED_UNLOCK(&mac->sched_lock);

    resp_p->ittiMsg.mac_get_ue_rnti.rnti = ue ? ue->rnti : 0;
    resp_p->ittiMsg.mac_get_ue_rnti.has_mac = true;
    LOG_I(NR_MAC, "MAC_GET_UE_RNTI: Found UE with RNTI %x\n", resp_p->ittiMsg.mac_get_ue_rnti.rnti);
  }
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

void mac_get_ue_rnti_by_uid(MessageDef *msg_p, instance_t instance)
{
  UNUSED(instance);
  uid_t uid = msg_p->ittiMsg.mac_get_ue_rnti_by_uid.uid;
  MessageDef *resp_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_GET_UE_RNTI_BY_UID);
  resp_p->ittiMsg.mac_get_ue_rnti_by_uid.uid = uid;

  if (!RC.nrmac || !RC.nrmac[0]) {
    LOG_I(NR_MAC, "MAC_GET_UE_RNTI_BY_UID: No MAC instance available, returning RNTI 0\n");
    resp_p->ittiMsg.mac_get_ue_rnti_by_uid.rnti = 0;
    resp_p->ittiMsg.mac_get_ue_rnti_by_uid.has_mac = false;
  } else {
    gNB_MAC_INST *mac = RC.nrmac[0];
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
  UNUSED(instance);
  rnti_t rnti = msg_p->ittiMsg.mac_force_ul_failure.rnti;
  MessageDef *resp_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_FORCE_UL_FAILURE);

  if (!RC.nrmac || !RC.nrmac[0]) {
    LOG_E(NR_MAC, "MAC_FORCE_UL_FAILURE: No MAC instance available for RNTI %x\n", rnti);
    itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
    return;
  }

  gNB_MAC_INST *mac = RC.nrmac[0];

  NR_SCHED_LOCK(&mac->sched_lock);
  NR_UE_info_t *UE = find_nr_UE(&mac->UE_info, rnti);

  if (UE) {
    LOG_I(NR_MAC, "MAC_FORCE_UL_FAILURE: Found UE for RNTI %x, triggering UL failure\n", rnti);
    nr_mac_trigger_ul_failure(&UE->UE_sched_ctrl, UE->current_UL_BWP.scs);
  } else {
    LOG_E(NR_MAC, "MAC_FORCE_UL_FAILURE: UE not found for RNTI %x\n", rnti);
  }
  NR_SCHED_UNLOCK(&mac->sched_lock);

  LOG_I(NR_MAC, "Sending MAC_FORCE_UL_FAILURE response to TELNET\n");
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

void mac_force_ue_release(MessageDef *msg_p, instance_t instance)
{
  UNUSED(instance);
  rnti_t rnti = msg_p->ittiMsg.mac_force_ue_release.rnti;
  MessageDef *resp_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_FORCE_UE_RELEASE);

  if (!RC.nrmac || !RC.nrmac[0]) {
    LOG_E(NR_MAC, "MAC_FORCE_UE_RELEASE: No MAC instance available for RNTI %x\n", rnti);
    itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
    return;
  }

  gNB_MAC_INST *mac = RC.nrmac[0];

  NR_SCHED_LOCK(&mac->sched_lock);
  NR_UE_info_t *UE = find_nr_UE(&mac->UE_info, rnti);

  if (UE) {
    LOG_I(NR_MAC, "MAC_FORCE_UE_RELEASE: Found UE for RNTI %x, setting ul_failure_timer and checking UL failure\n", rnti);
    NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    sched_ctrl->ul_failure_timer = 2;
    nr_mac_check_ul_failure(mac, UE->rnti, sched_ctrl);
  } else {
    LOG_E(NR_MAC, "MAC_FORCE_UE_RELEASE: UE not found for RNTI %x\n", rnti);
  }
  NR_SCHED_UNLOCK(&mac->sched_lock);

  LOG_I(NR_MAC, "Sending MAC_FORCE_UE_RELEASE response to TELNET\n");
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

void mac_get_ue_bwp_info(MessageDef *msg_p, instance_t instance)
{
  UNUSED(instance);
  rnti_t rnti = msg_p->ittiMsg.mac_get_ue_bwp_info.rnti;
  MessageDef *resp_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_GET_UE_BWP_INFO);
  resp_p->ittiMsg.mac_get_ue_bwp_info.rnti = rnti;
  resp_p->ittiMsg.mac_get_ue_bwp_info.dl_bwp_id = -1;
  resp_p->ittiMsg.mac_get_ue_bwp_info.ul_bwp_id = -1;

  if (!RC.nrmac || !RC.nrmac[0]) {
    LOG_E(NR_MAC, "MAC_GET_UE_BWP_INFO: No MAC instance available for RNTI %x\n", rnti);
    itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
    return;
  }

  gNB_MAC_INST *mac = RC.nrmac[0];

  NR_SCHED_LOCK(&mac->sched_lock);
  NR_UE_info_t *UE = find_nr_UE(&mac->UE_info, rnti);

  if (UE) {
    LOG_I(NR_MAC, "MAC_GET_UE_BWP_INFO: Found UE for RNTI %x\n", rnti);
    resp_p->ittiMsg.mac_get_ue_bwp_info.dl_bwp_id = UE->current_DL_BWP.bwp_id;
    resp_p->ittiMsg.mac_get_ue_bwp_info.ul_bwp_id = UE->current_UL_BWP.bwp_id;
  } else {
    LOG_E(NR_MAC, "MAC_GET_UE_BWP_INFO: UE not found for RNTI %x\n", rnti);
  }
  NR_SCHED_UNLOCK(&mac->sched_lock);

  LOG_I(NR_MAC, "Sending MAC_GET_UE_BWP_INFO response to TELNET\n");
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

void mac_set_pusch_target_snr(MessageDef *msg_p, instance_t instance)
{
  UNUSED(instance);
  long target_snrx10 = msg_p->ittiMsg.mac_set_pusch_target_snr.target_snrx10;
  MessageDef *resp_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_SET_PUSCH_TARGET_SNR);

  if (!RC.nrmac || !RC.nrmac[0]) {
    LOG_E(NR_MAC, "MAC_SET_PUSCH_TARGET_SNR: No MAC instance available\n");
    itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
    return;
  }

  gNB_MAC_INST *mac = RC.nrmac[0];

  NR_SCHED_LOCK(&mac->sched_lock);
  UE_iterator(mac->UE_info.connected_ue_list, it) {
    nr_mac_set_target_snrx10(&it->UE_sched_ctrl.pusch_pc, target_snrx10);
  }
  NR_SCHED_UNLOCK(&mac->sched_lock);

  LOG_I(NR_MAC, "MAC_SET_PUSCH_TARGET_SNR: Set target SNR to %ld for all UEs\n", target_snrx10);
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

void *mac_gnb_task(void *args_p)
{
  UNUSED(args_p);
  MessageDef *msg_p;
  instance_t instance;
  int result;

  itti_mark_task_ready(TASK_MAC_GNB);
  LOG_I(NR_MAC, "Entering main loop of NR_MAC message task\n");

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
        LOG_I(NR_MAC, "[gNB %ld] Received %s\n", instance, ITTI_MSG_NAME(msg_p));
        break;

      case MAC_GET_UE_RNTI:
        LOG_I(NR_MAC, "MAC Task: Processing MAC_GET_UE_RNTI message\n");
        mac_get_ue_rnti(msg_p, instance);
        break;

      case MAC_GET_UE_RNTI_BY_UID:
        LOG_I(NR_MAC, "MAC Task: Processing MAC_GET_UE_RNTI_BY_UID message\n");
        mac_get_ue_rnti_by_uid(msg_p, instance);
        break;

      case MAC_FORCE_UL_FAILURE:
        LOG_I(NR_MAC, "MAC Task: Processing MAC_FORCE_UL_FAILURE message for RNTI %d\n", msg_p->ittiMsg.mac_force_ul_failure.rnti);
        mac_force_ul_failure(msg_p, instance);
        break;

      case MAC_FORCE_UE_RELEASE:
        LOG_I(NR_MAC, "MAC Task: Processing MAC_FORCE_UE_RELEASE message for RNTI %d\n", msg_p->ittiMsg.mac_force_ue_release.rnti);
        mac_force_ue_release(msg_p, instance);
        break;

      case MAC_GET_UE_BWP_INFO:
        LOG_I(NR_MAC, "MAC Task: Processing MAC_GET_UE_BWP_INFO message for RNTI %d\n", msg_p->ittiMsg.mac_get_ue_bwp_info.rnti);
        mac_get_ue_bwp_info(msg_p, instance);
        break;

      case MAC_SET_PUSCH_TARGET_SNR:
        LOG_I(NR_MAC, "MAC Task: Processing MAC_SET_PUSCH_TARGET_SNR message\n");
        mac_set_pusch_target_snr(msg_p, instance);
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
