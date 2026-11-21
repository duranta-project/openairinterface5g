/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_mac_gNB.h"
#include "intertask_interface.h"

#include "mac_rrc_ul.h"
#include "lib/f1ap_rrc_message_transfer.h"
#include "lib/f1ap_interface_management.h"
#include "lib/f1ap_ue_context.h"
#include "lib/f1ap_positioning.h"

static void f1_reset_du_initiated_direct(const f1ap_reset_t *reset)
{
  (void) reset;
  AssertFatal(false, "%s() not implemented yet\n", __func__);
}

static void f1_reset_acknowledge_cu_initiated_direct(const f1ap_reset_ack_t *ack)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_RESET_ACK);
  msg->ittiMsgHeader.originInstance = -1; // means monolithic
  F1AP_RESET_ACK(msg) = cp_f1ap_reset_ack(ack);
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void f1_setup_request_direct(const f1ap_setup_req_t *req)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_SETUP_REQ);
  msg->ittiMsgHeader.originInstance = -1; // means monolithic
  f1ap_setup_req_t cp = cp_f1ap_setup_request(req);
  F1AP_SETUP_REQ(msg) = cp;
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void gnb_du_configuration_update_direct(const f1ap_gnb_du_configuration_update_t *upd)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_GNB_DU_CONFIGURATION_UPDATE);
  msg->ittiMsgHeader.originInstance = -1; // means monolithic
  /* transfer to RRC via ITTI */
  F1AP_GNB_DU_CONFIGURATION_UPDATE(msg) = cp_f1ap_du_configuration_update(upd);
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void ue_context_setup_response_direct(const f1ap_ue_context_setup_resp_t *resp)
{
  MessageDef *msg = itti_alloc_new_message (TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_SETUP_RESP);
  msg->ittiMsgHeader.originInstance = -1; // means monolithic
  F1AP_UE_CONTEXT_SETUP_RESP(msg) = cp_ue_context_setup_resp(resp);
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void ue_context_modification_response_direct(const f1ap_ue_context_mod_resp_t *resp)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_MODIFICATION_RESP);
  msg->ittiMsgHeader.originInstance = -1; // means monolithic
  F1AP_UE_CONTEXT_MODIFICATION_RESP(msg) = cp_ue_context_mod_resp(resp);
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void ue_context_modification_required_direct(const f1ap_ue_context_modif_required_t *required)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_MODIFICATION_REQUIRED);
  msg->ittiMsgHeader.originInstance = -1; // means monolithic
  F1AP_UE_CONTEXT_MODIFICATION_REQUIRED(msg) = cp_ue_context_modif_required(required);
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void ue_context_release_request_direct(const f1ap_ue_context_rel_req_t* req)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_RELEASE_REQ);
  msg->ittiMsgHeader.originInstance = -1; // means monolithic
  F1AP_UE_CONTEXT_RELEASE_REQ(msg) = cp_ue_context_rel_req(req);
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void ue_context_release_complete_direct(const f1ap_ue_context_rel_cplt_t *complete)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_RELEASE_COMPLETE);
  msg->ittiMsgHeader.originInstance = -1; // means monolithic
  F1AP_UE_CONTEXT_RELEASE_COMPLETE(msg) = cp_ue_context_rel_cplt(complete);
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void initial_ul_rrc_message_transfer_direct(module_id_t module_id, const f1ap_initial_ul_rrc_message_t *ul_rrc)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_INITIAL_UL_RRC_MESSAGE);
  msg->ittiMsgHeader.originInstance = -1; // means monolithic
  f1ap_initial_ul_rrc_message_t *f1ap_msg = &F1AP_INITIAL_UL_RRC_MESSAGE(msg);
  *f1ap_msg = cp_initial_ul_rrc_message_transfer(ul_rrc);
  itti_send_msg_to_task(TASK_RRC_GNB, module_id, msg);
}

static void trp_information_response_direct(const f1ap_trp_information_resp_t *resp)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_TRP_INFORMATION_RESP);
  msg->ittiMsgHeader.originInstance = -1; // means monolithic
  F1AP_TRP_INFORMATION_RESP(msg) = cp_trp_information_resp(resp);
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void trp_information_failure_direct(const f1ap_trp_information_failure_t *fail)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_TRP_INFORMATION_FAILURE);
  msg->ittiMsgHeader.originInstance = -1; // means monolithic
  F1AP_TRP_INFORMATION_FAILURE(msg) = cp_trp_information_failure(fail);
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void positioning_information_response_direct(const f1ap_positioning_information_resp_t *resp)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_POSITIONING_INFORMATION_RESP);
  msg->ittiMsgHeader.originInstance = -1; // means monolithic
  F1AP_POSITIONING_INFORMATION_RESP(msg) = cp_positioning_information_resp(resp);
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void positioning_activation_response_direct(const f1ap_positioning_activation_resp_t *resp)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_POSITIONING_ACTIVATION_RESP);
  msg->ittiMsgHeader.originInstance = -1; // means monolithic
  F1AP_POSITIONING_ACTIVATION_RESP(msg) = cp_positioning_activation_resp(resp);
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void positioning_measurement_response_direct(const f1ap_positioning_measurement_resp_t *resp)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_POSITIONING_MEASUREMENT_RESP);
  msg->ittiMsgHeader.originInstance = -1; // means monolithic
  F1AP_POSITIONING_MEASUREMENT_RESP(msg) = cp_positioning_measurement_resp(resp);
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

static void positioning_measurement_failure_direct(const f1ap_positioning_measurement_failure_t *fail)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_POSITIONING_MEASUREMENT_FAILURE);
  msg->ittiMsgHeader.originInstance = -1; // means monolithic
  F1AP_POSITIONING_MEASUREMENT_FAILURE(msg) = cp_positioning_measurement_failure(fail);
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

void mac_rrc_ul_direct_init(struct nr_mac_rrc_ul_if_s *mac_rrc)
{
  mac_rrc->f1_reset = f1_reset_du_initiated_direct;
  mac_rrc->f1_reset_acknowledge = f1_reset_acknowledge_cu_initiated_direct;
  mac_rrc->f1_setup_request = f1_setup_request_direct;
  mac_rrc->gnb_du_configuration_update = gnb_du_configuration_update_direct;
  mac_rrc->ue_context_setup_response = ue_context_setup_response_direct;
  mac_rrc->ue_context_modification_response = ue_context_modification_response_direct;
  mac_rrc->ue_context_modification_required = ue_context_modification_required_direct;
  mac_rrc->ue_context_release_request = ue_context_release_request_direct;
  mac_rrc->ue_context_release_complete = ue_context_release_complete_direct;
  mac_rrc->initial_ul_rrc_message_transfer = initial_ul_rrc_message_transfer_direct;
  mac_rrc->trp_information_response = trp_information_response_direct;
  mac_rrc->trp_information_failure = trp_information_failure_direct;
  mac_rrc->positioning_information_response = positioning_information_response_direct;
  mac_rrc->positioning_activation_response = positioning_activation_response_direct;
  mac_rrc->positioning_measurement_response = positioning_measurement_response_direct;
  mac_rrc->positioning_measurement_failure = positioning_measurement_failure_direct;
}
