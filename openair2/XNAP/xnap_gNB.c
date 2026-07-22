/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <string.h>
#include <stdlib.h>
#include "common/platform_types.h"
#include "common/utils/LOG/log.h"
#include "common/utils/ocp_itti/intertask_interface.h"
#include "assertions.h"
#include "openair2/COMMON/sctp_messages_types.h"
#include "openair2/COMMON/xnap_messages_types.h"
#include "xnap_default_values.h"
#include "xnap_common.h"
#include "xnap_gNB.h"

/* Phase 1: bind local SCTP listener socket */
static void xnap_gNB_handle_register_gnb(instance_t instance, xnap_register_gnb_req_t *req)
{
  createXninst(instance, &req->ng_setup_info, &req->net_config);

  xnap_gnb_inst_t *inst = getCxtXn(instance);
  AssertFatal(inst != NULL, "Xn instance %ld not found after creation\n", instance);

  const char *local_ip = inst->net_config.gnb_xn_interface_ip_address;
  size_t addr_len = strlen(local_ip) + 1;

  MessageDef *msg = itti_alloc_new_message_sized(TASK_XNAP, instance, SCTP_INIT_MSG_MULTI_REQ, sizeof(sctp_init_t) + addr_len);
  sctp_init_t *init = &msg->ittiMsg.sctp_init_multi;
  init->port = XNAP_PORT_NUMBER;
  init->ppid = XNAP_SCTP_PPID;
  char *addr_buf = (char *)(init + 1);
  init->bind_address = addr_buf;
  memcpy(addr_buf, local_ip, addr_len);

  LOG_I(XNAP, "[gNB %ld] Binding SCTP listener on %s port %u\n", instance, local_ip, XNAP_PORT_NUMBER);

  itti_send_msg_to_task(TASK_SCTP, instance, msg);
}

/* Phase 2: socket bound — connect to each configured peer */
static void xnap_gNB_handle_sctp_init_msg_multi_cnf(instance_t instance, sctp_init_msg_multi_cnf_t *cnf)
{
  xnap_gnb_inst_t *inst = getCxtXn(instance);
  AssertFatal(inst != NULL, "Xn instance %ld not found\n", instance);

  inst->multi_sd = cnf->multi_sd;
  if (inst->multi_sd < 0) {
    LOG_E(XNAP, "[gNB %ld] SCTP_INIT_MSG_MULTI_CNF failed — check Xn IP/port config\n", instance);
    return;
  }

  LOG_I(XNAP, "[gNB %ld] SCTP listener ready (multi_sd %d), connecting to %u peer(s)\n", instance, inst->multi_sd, inst->nb_peers);

  xnap_peer_t *peer;
  RB_FOREACH(peer, xnap_peer_map, &inst->peers) {
    const char *remote_ip = inst->net_config.candidate_gnb_address_for_xnc[peer->cnx_id];

    MessageDef *msg = itti_alloc_new_message(TASK_XNAP, instance, SCTP_NEW_ASSOCIATION_REQ);
    sctp_new_association_req_t *req = &msg->ittiMsg.sctp_new_association_req;

    req->ulp_cnx_id  = peer->cnx_id;
    req->port        = XNAP_PORT_NUMBER;
    req->ppid        = XNAP_SCTP_PPID;
    req->in_streams  = inst->net_config.sctp_streams.sctp_in_streams;
    req->out_streams = inst->net_config.sctp_streams.sctp_out_streams;

    req->local_address.ipv4 = 1;
    strncpy(req->local_address.ipv4_address, inst->net_config.gnb_xn_interface_ip_address, sizeof(req->local_address.ipv4_address) - 1);

    req->remote_address.ipv4 = 1;
    strncpy(req->remote_address.ipv4_address, remote_ip, sizeof(req->remote_address.ipv4_address) - 1);

    LOG_I(XNAP, "[gNB %ld] Initiating SCTP connection to peer %u at %s port %u\n", instance, peer->cnx_id, remote_ip, XNAP_PORT_NUMBER);

    itti_send_msg_to_task(TASK_SCTP, instance, msg);
  }
}

void *xnap_task(void *args)
{
  UNUSED(args);
  LOG_I(XNAP, "Starting XnAP task\n");
  itti_mark_task_ready(TASK_XNAP);

  while (1) {
    MessageDef *msg = NULL;
    itti_receive_msg(TASK_XNAP, &msg);
    const instance_t instance = ITTI_MSG_DESTINATION_INSTANCE(msg);
    const int msgType = ITTI_MSG_ID(msg);
    LOG_D(XNAP, "XnAP received %s for instance %ld\n", ITTI_MSG_NAME(msg), instance);

    switch (msgType) {
      case XNAP_REGISTER_GNB_REQ:
        xnap_gNB_handle_register_gnb(instance, &XNAP_REGISTER_GNB_REQ(msg));
        break;

      case SCTP_INIT_MSG_MULTI_CNF:
        xnap_gNB_handle_sctp_init_msg_multi_cnf(instance, &SCTP_INIT_MSG_MULTI_CNF(msg));
        break;

      default:
        LOG_E(XNAP, "Unknown message type %d (%s)\n", msgType, ITTI_MSG_NAME(msg));
        break;
    }

    int result = itti_free(ITTI_MSG_ORIGIN_ID(msg), msg);
    AssertFatal(result == EXIT_SUCCESS, "Failed to free ITTI message (%d)\n", result);
    msg = NULL;
  }
}
