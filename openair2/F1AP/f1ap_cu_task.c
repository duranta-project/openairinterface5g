/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "f1ap_common.h"
#include "f1ap_cu_interface_management.h"
#include "f1ap_cu_rrc_message_transfer.h"
#include "f1ap_cu_ue_context_management.h"
#include "lib/f1ap_rrc_message_transfer.h"
#include "lib/f1ap_interface_management.h"
#include "lib/f1ap_ue_context.h"
#include "f1ap_cu_paging.h"
#include "lib/f1ap_paging.h"
#include "f1ap_cu_task.h"
#include "f1ap_mbs.h"
#include "openair2/RRC/NR/nr_rrc_defs.h"
#include <openair3/ocp-gtpu/gtp_itf.h>
#include "common_lib.h"
#include "f1ap_cu_positioning.h"

static instance_t cu_task_create_gtpu_instance(const char *bind_addr, uint16_t local_port, uint16_t remote_port)
{
  openAddr_t tmp= {0};
  strncpy(tmp.originHost, bind_addr, sizeof(tmp.originHost)-1);
  sprintf(tmp.originService, "%d", local_port);
  sprintf(tmp.destinationService, "%d", remote_port);
  return gtpv1Init(tmp);
}

static void cu_task_handle_sctp_association_ind(instance_t instance, sctp_new_association_ind_t *sctp_new_association_ind)
{
  // save the assoc id
  f1ap_cudu_inst_t *f1ap_cu_data = getCxt(instance);
  // we don't need the assoc_id, subsequent messages (the first being F1 Setup
  // Request), will deliver the assoc_id
  f1ap_cu_data->sctp_in_streams  = sctp_new_association_ind->in_streams;
  f1ap_cu_data->sctp_out_streams = sctp_new_association_ind->out_streams;
}

static void cu_task_handle_sctp_association_resp(instance_t instance, sctp_new_association_resp_t *sctp_new_association_resp) {
  DevAssert(sctp_new_association_resp != NULL);

  sctp_state_e state = sctp_new_association_resp->sctp_state;
  if (state != SCTP_STATE_ESTABLISHED) {
    f1ap_cudu_inst_t *f1ap_cu_data = getCxt(instance);
    AssertFatal(f1ap_cu_data != NULL, "illegal state: SCTP shutdown for non-existing F1AP endpoint\n");
    LOG_I(F1AP, "Received SCTP state %d for assoc_id %d, removing endpoint\n", state, sctp_new_association_resp->assoc_id);
    /* inform RRC that the DU is gone */
    MessageDef *message_p = itti_alloc_new_message(TASK_CU_F1, 0, F1AP_LOST_CONNECTION);
    message_p->ittiMsgHeader.originInstance = sctp_new_association_resp->assoc_id;
    itti_send_msg_to_task(TASK_RRC_GNB, instance, message_p);
    return;
  }
}

static void cu_task_handle_sctp_data_ind(instance_t instance, sctp_data_ind_t *sctp_data_ind) {
  int result;
  DevAssert(sctp_data_ind != NULL);
  f1ap_handle_message(instance, sctp_data_ind->assoc_id, sctp_data_ind->stream,
                      sctp_data_ind->buffer, sctp_data_ind->buffer_length);
  result = itti_free(TASK_UNKNOWN, sctp_data_ind->buffer);
  AssertFatal (result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
}

static void cu_task_send_sctp_init_req(instance_t instance, char *my_addr)
{
  // 1. get the itti msg, and retrive the nb_id from the message
  // 2. use RC.rrc[nb_id] to fill the sctp_init_t with the ip, port
  // 3. creat an itti message to init
  size_t addr_len = strlen(my_addr) + 1;
  LOG_I(F1AP, "F1AP_CU_SCTP_REQ(create socket) for %s len %ld\n", my_addr, addr_len);
  MessageDef *message_p = itti_alloc_new_message_sized(TASK_CU_F1, 0, SCTP_INIT_MSG, sizeof(sctp_init_t) + addr_len);
  sctp_init_t *init = &SCTP_INIT_MSG(message_p);
  init->port = F1AP_PORT_NUMBER;
  init->ppid = F1AP_SCTP_PPID;
  char *addr_buf = (char *) (init + 1); // address after ITTI message end, allocated above
  init->bind_address = addr_buf;
  memcpy(addr_buf, my_addr, addr_len);
  itti_send_msg_to_task(TASK_SCTP, instance, message_p);
}

void *F1AP_CU_task(void *arg)
{
  DevAssert(arg != NULL);
  MessageDef *received_msg = NULL;
  int         result;
  LOG_I(F1AP, "Starting F1AP at CU\n");
  // no RLC in CU, initialize mem pool for PDCP
  itti_mark_task_ready(TASK_CU_F1);
  f1ap_cu_conf_t *c = arg;
  DevAssert(c->type == ngran_gNB_CU || c->type == ngran_gNB_CUCP);

  const int instance = 0;
  createF1inst(instance, NULL, NULL);
  cu_task_send_sctp_init_req(instance, c->bind_addr);

  if (c->type != ngran_gNB_CUCP) {
    getCxt(instance)->gtpInst = cu_task_create_gtpu_instance(c->bind_addr, c->local_f1u_port, c->remote_f1u_port);
    AssertFatal(getCxt(instance)->gtpInst > 0, "Failed to create CU F1-U UDP listener");
  } else {
    LOG_I(F1AP, "In F1AP connection, don't start GTP-U, as we have also E1AP\n");
  }

  free(c->bind_addr);
  free(c);

  while (1) {
    itti_receive_msg(TASK_CU_F1, &received_msg);
    sctp_assoc_t assoc_id = ITTI_MSG_ORIGIN_INSTANCE(received_msg);
    LOG_D(F1AP, "CU Task Received %s for instance %ld: sending SCTP message via assoc_id %d\n",
          ITTI_MSG_NAME(received_msg), ITTI_MSG_DESTINATION_INSTANCE(received_msg), assoc_id);
    switch (ITTI_MSG_ID(received_msg)) {
      case SCTP_NEW_ASSOCIATION_IND:
        cu_task_handle_sctp_association_ind(ITTI_MSG_ORIGIN_INSTANCE(received_msg),
                                            &received_msg->ittiMsg.sctp_new_association_ind);
        break;

      case SCTP_NEW_ASSOCIATION_RESP:
        cu_task_handle_sctp_association_resp(ITTI_MSG_DESTINATION_INSTANCE(received_msg),
                                             &received_msg->ittiMsg.sctp_new_association_resp);
        break;

      case SCTP_DATA_IND:
        cu_task_handle_sctp_data_ind(ITTI_MSG_DESTINATION_INSTANCE(received_msg),
                                     &received_msg->ittiMsg.sctp_data_ind);
        break;

      case F1AP_RESET:
        CU_send_RESET(assoc_id, &F1AP_RESET(received_msg));
        free_f1ap_reset(&F1AP_RESET(received_msg));
        break;

      case F1AP_RESET_ACK:
        CU_send_RESET_ACKNOWLEDGE(assoc_id, &F1AP_RESET_ACK(received_msg));
        break;

      case F1AP_BROADCAST_CONTEXT_SETUP_REQ: // from rrc
        CU_send_BROADCAST_CONTEXT_SETUP_REQUEST(assoc_id, &F1AP_BROADCAST_CONTEXT_SETUP_REQ(received_msg));
        break;

      case F1AP_SETUP_RESP: // from rrc
        CU_send_F1_SETUP_RESPONSE(assoc_id,
                                  &F1AP_SETUP_RESP(received_msg));
        break;

      case F1AP_SETUP_FAILURE:
        CU_send_F1_SETUP_FAILURE(assoc_id, &F1AP_SETUP_FAILURE(received_msg));
        break;

      case F1AP_GNB_CU_CONFIGURATION_UPDATE: // from rrc
        CU_send_gNB_CU_CONFIGURATION_UPDATE(assoc_id,
                                            &F1AP_GNB_CU_CONFIGURATION_UPDATE(received_msg));
        break;

      case F1AP_GNB_DU_CONFIGURATION_UPDATE_ACKNOWLEDGE:
        CU_send_gNB_DU_CONFIGURATION_UPDATE_ACKNOWLEDGE(assoc_id, &F1AP_GNB_DU_CONFIGURATION_UPDATE_ACKNOWLEDGE(received_msg));
        break;
      case F1AP_DL_RRC_MESSAGE: // from rrc
        CU_send_DL_RRC_MESSAGE_TRANSFER(assoc_id,
                                        &F1AP_DL_RRC_MESSAGE(received_msg));
        free_dl_rrc_message_transfer(&F1AP_DL_RRC_MESSAGE(received_msg));
        break;

      case F1AP_UE_CONTEXT_SETUP_REQ: // from rrc
        CU_send_UE_CONTEXT_SETUP_REQUEST(assoc_id, &F1AP_UE_CONTEXT_SETUP_REQ(received_msg));
        free_ue_context_setup_req(&F1AP_UE_CONTEXT_SETUP_REQ(received_msg));
        break;

      case F1AP_UE_CONTEXT_MODIFICATION_REQ:
        CU_send_UE_CONTEXT_MODIFICATION_REQUEST(assoc_id, &F1AP_UE_CONTEXT_MODIFICATION_REQ(received_msg));
        free_ue_context_mod_req(&F1AP_UE_CONTEXT_MODIFICATION_REQ(received_msg));
        break;

      case F1AP_UE_CONTEXT_RELEASE_CMD: // from rrc
        CU_send_UE_CONTEXT_RELEASE_COMMAND(assoc_id,
                                           &F1AP_UE_CONTEXT_RELEASE_CMD(received_msg));
        free_ue_context_rel_cmd(&F1AP_UE_CONTEXT_RELEASE_CMD(received_msg));
        break;

      case F1AP_PAGING:
        CU_send_Paging(assoc_id, &F1AP_PAGING(received_msg));
        free_f1ap_paging(&F1AP_PAGING(received_msg));
        break;

      case F1AP_UE_CONTEXT_MODIFICATION_CONFIRM:
        CU_send_UE_CONTEXT_MODIFICATION_CONFIRM(assoc_id,
                                                &F1AP_UE_CONTEXT_MODIFICATION_CONFIRM(received_msg));
        break;

      case F1AP_UE_CONTEXT_MODIFICATION_REFUSE:
        CU_send_UE_CONTEXT_MODIFICATION_REFUSE(assoc_id,
                                               &F1AP_UE_CONTEXT_MODIFICATION_REFUSE(received_msg));
        break;

      case F1AP_TRP_INFORMATION_REQ:
        CU_send_TRP_INFORMATION_REQUEST(assoc_id, &F1AP_TRP_INFORMATION_REQ(received_msg));
        break;

      case F1AP_POSITIONING_INFORMATION_REQ:
        CU_send_POSITIONING_INFORMATION_REQUEST(assoc_id, &F1AP_POSITIONING_INFORMATION_REQ(received_msg));
        break;

      case F1AP_POSITIONING_ACTIVATION_REQ:
        CU_send_POSITIONING_ACTIVATION_REQUEST(assoc_id, &F1AP_POSITIONING_ACTIVATION_REQ(received_msg));
        break;

      case F1AP_POSITIONING_MEASUREMENT_REQ:
        CU_send_POSITIONING_MEASUREMENT_REQUEST(assoc_id, &F1AP_POSITIONING_MEASUREMENT_REQ(received_msg));
        break;

      case TERMINATE_MESSAGE:
        LOG_W(F1AP, " *** Exiting F1AP thread\n");
        itti_exit_task();
        break;

      default:
        LOG_E(F1AP, "CU Received unhandled message: %d:%s\n",
              ITTI_MSG_ID(received_msg), ITTI_MSG_NAME(received_msg));
        break;
    } // switch

    result = itti_free (ITTI_MSG_ORIGIN_ID(received_msg), received_msg);
    AssertFatal (result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
    received_msg = NULL;
  } // while

  destroyF1inst(instance);

  return NULL;
}
