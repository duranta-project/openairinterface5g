/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "nr_mac_gNB.h"
#include "intertask_interface.h"
#include "openair3/ocp-gtpu/gtp_itf.h"

#include "mac_rrc_ul.h"

#include "lib/f1ap_rrc_message_transfer.h"
#include "lib/f1ap_interface_management.h"
#include "lib/f1ap_ue_context.h"
#include "lib/f1ap_positioning.h"

static f1ap_net_config_t cp_net_config(const f1ap_net_config_t* c)
{
  f1ap_net_config_t nc = {
    .CU_f1_ip_address = strdup(c->CU_f1_ip_address),
    .DU_f1c_ip_address = strdup(c->DU_f1c_ip_address),
    .DU_f1u_ip_address = strdup(c->DU_f1u_ip_address),
    .CUport = c->CUport,
    .DUport = c->DUport,
  };
  return nc;
}

static void f1_reset_du_initiated_f1ap(const f1ap_reset_t *reset)
{
  (void) reset;
  AssertFatal(false, "%s() not implemented yet\n", __func__);
}

static void f1_reset_acknowledge_cu_initiated_f1ap(const f1ap_reset_ack_t *ack)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_RESET_ACK);
  F1AP_RESET_ACK(msg) = cp_f1ap_reset_ack(ack);
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void f1_setup_request_f1ap(const f1ap_setup_req_t *req)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_DU_REGISTER_REQ);
  f1ap_setup_req_t f1ap_setup = cp_f1ap_setup_request(req);
  F1AP_DU_REGISTER_REQ(msg).setup_req = f1ap_setup;
  F1AP_DU_REGISTER_REQ(msg).net_config = cp_net_config(&RC.nrmac[0]->net_config);
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void gnb_du_configuration_update_f1ap(const f1ap_gnb_du_configuration_update_t *upd)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_GNB_DU_CONFIGURATION_UPDATE);
  /* copy F1AP message */
  f1ap_gnb_du_configuration_update_t cp = cp_f1ap_du_configuration_update(upd);
  /* transfer to ITTI message */
  F1AP_GNB_DU_CONFIGURATION_UPDATE(msg) = cp;
  /* send to RRC task */
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void ue_context_setup_response_f1ap(const f1ap_ue_context_setup_resp_t *resp)
{
  MessageDef *msg = itti_alloc_new_message (TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_SETUP_RESP);
  F1AP_UE_CONTEXT_SETUP_RESP(msg) = cp_ue_context_setup_resp(resp);
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void ue_context_modification_response_f1ap(const f1ap_ue_context_mod_resp_t *resp)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_MODIFICATION_RESP);
  F1AP_UE_CONTEXT_MODIFICATION_RESP(msg) = cp_ue_context_mod_resp(resp);
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void ue_context_modification_required_f1ap(const f1ap_ue_context_modif_required_t *required)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_MODIFICATION_REQUIRED);
  F1AP_UE_CONTEXT_MODIFICATION_REQUIRED(msg) = cp_ue_context_modif_required(required);
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void ue_context_release_request_f1ap(const f1ap_ue_context_rel_req_t* req)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_RELEASE_REQ);
  F1AP_UE_CONTEXT_RELEASE_REQ(msg) = cp_ue_context_rel_req(req);
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void ue_context_release_complete_f1ap(const f1ap_ue_context_rel_cplt_t *complete)
{
  newGtpuDeleteAllTunnels(0, complete->gNB_DU_ue_id);

  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_UE_CONTEXT_RELEASE_COMPLETE);
  F1AP_UE_CONTEXT_RELEASE_COMPLETE(msg) = cp_ue_context_rel_cplt(complete);
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void initial_ul_rrc_message_transfer_f1ap(module_id_t module_id, const f1ap_initial_ul_rrc_message_t *ul_rrc)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_INITIAL_UL_RRC_MESSAGE);
  f1ap_initial_ul_rrc_message_t *f1ap_msg = &F1AP_INITIAL_UL_RRC_MESSAGE(msg);
  *f1ap_msg = cp_initial_ul_rrc_message_transfer(ul_rrc);
  itti_send_msg_to_task(TASK_DU_F1, module_id, msg);
}

static void trp_information_response_f1ap(const f1ap_trp_information_resp_t *resp)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_TRP_INFORMATION_RESP);
  F1AP_TRP_INFORMATION_RESP(msg) = cp_trp_information_resp(resp);
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void trp_information_failure_f1ap(const f1ap_trp_information_failure_t *fail)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_TRP_INFORMATION_FAILURE);
  F1AP_TRP_INFORMATION_FAILURE(msg) = cp_trp_information_failure(fail);
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void positioning_information_response_f1ap(const f1ap_positioning_information_resp_t *resp)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_POSITIONING_INFORMATION_RESP);
  F1AP_POSITIONING_INFORMATION_RESP(msg) = cp_positioning_information_resp(resp);
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void positioning_activation_response_f1ap(const f1ap_positioning_activation_resp_t *resp)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_POSITIONING_ACTIVATION_RESP);
  F1AP_POSITIONING_ACTIVATION_RESP(msg) = cp_positioning_activation_resp(resp);
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void positioning_measurement_response_f1ap(const f1ap_positioning_measurement_resp_t *resp)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_POSITIONING_MEASUREMENT_RESP);
  F1AP_POSITIONING_MEASUREMENT_RESP(msg) = cp_positioning_measurement_resp(resp);
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

static void positioning_measurement_failure_f1ap(const f1ap_positioning_measurement_failure_t *fail)
{
  MessageDef *msg = itti_alloc_new_message(TASK_MAC_GNB, 0, F1AP_POSITIONING_MEASUREMENT_FAILURE);
  F1AP_POSITIONING_MEASUREMENT_FAILURE(msg) = cp_positioning_measurement_failure(fail);
  itti_send_msg_to_task(TASK_DU_F1, 0, msg);
}

void mac_rrc_ul_f1ap_init(struct nr_mac_rrc_ul_if_s *mac_rrc)
{
  mac_rrc->f1_reset = f1_reset_du_initiated_f1ap;
  mac_rrc->f1_reset_acknowledge = f1_reset_acknowledge_cu_initiated_f1ap;
  mac_rrc->f1_setup_request = f1_setup_request_f1ap;
  mac_rrc->gnb_du_configuration_update = gnb_du_configuration_update_f1ap;
  mac_rrc->ue_context_setup_response = ue_context_setup_response_f1ap;
  mac_rrc->ue_context_modification_response = ue_context_modification_response_f1ap;
  mac_rrc->ue_context_modification_required = ue_context_modification_required_f1ap;
  mac_rrc->ue_context_release_request = ue_context_release_request_f1ap;
  mac_rrc->ue_context_release_complete = ue_context_release_complete_f1ap;
  mac_rrc->initial_ul_rrc_message_transfer = initial_ul_rrc_message_transfer_f1ap;
  mac_rrc->trp_information_response = trp_information_response_f1ap;
  mac_rrc->trp_information_failure = trp_information_failure_f1ap;
  mac_rrc->positioning_information_response = positioning_information_response_f1ap;
  mac_rrc->positioning_activation_response = positioning_activation_response_f1ap;
  mac_rrc->positioning_measurement_response = positioning_measurement_response_f1ap;
  mac_rrc->positioning_measurement_failure = positioning_measurement_failure_f1ap;
}

