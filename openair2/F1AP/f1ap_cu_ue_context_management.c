/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "f1ap_common.h"
#include "f1ap_encoder.h"
#include "f1ap_itti_messaging.h"
#include "f1ap_cu_ue_context_management.h"
#include "lib/f1ap_ue_context.h"
#include <string.h>

#include "openair2/RRC/NR/rrc_gNB_NGAP.h"

#include "F1AP_InitiatingMessage.h"
#include "F1AP_SuccessfulOutcome.h"
#include "F1AP_UnsuccessfulOutcome.h"
#include "F1AP_ProtocolIE-Field.h"

#ifdef E2_AGENT
#include "openair2/RRC/NR/rrc_gNB_UE_context.h"
#include "openair2/E2AP/RAN_FUNCTION/O-RAN/ran_func_rc_extern.h"
#endif

int CU_send_UE_CONTEXT_SETUP_REQUEST(sctp_assoc_t assoc_id, const f1ap_ue_context_setup_req_t *req)
{
  F1AP_F1AP_PDU_t *pdu = encode_ue_context_setup_req(req);

  uint8_t *buffer = NULL;
  uint32_t len = 0;
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 UE CONTEXT SETUP REQUEST\n");
    return -1;
  }

  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);

#ifdef E2_AGENT
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(RC.nrrrc[0], req->gNB_CU_ue_id);
  signal_ue_id(&ue_context_p->ue_context, F1_NETWORK_INTERFACE_TYPE, 0);
#endif

  return 0;
}

int CU_handle_UE_CONTEXT_SETUP_RESPONSE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(stream);
  f1ap_ue_context_setup_resp_t resp = {0};
  if (!decode_ue_context_setup_resp(pdu, &resp)) {
    LOG_E(F1AP, "cannot decode F1 UE Context Setup Resp\n");
    free_ue_context_setup_resp(&resp);
    return -1;
  }

  MessageDef *msg_p = itti_alloc_new_message(TASK_DU_F1, 0, F1AP_UE_CONTEXT_SETUP_RESP);
  msg_p->ittiMsgHeader.originInstance = assoc_id;
  F1AP_UE_CONTEXT_SETUP_RESP(msg_p) = resp;
  itti_send_msg_to_task(TASK_RRC_GNB, instance, msg_p);
  return 0;
}

int CU_handle_UE_CONTEXT_SETUP_FAILURE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(instance);
  UNUSED(assoc_id);
  UNUSED(stream);
  UNUSED(pdu);
  AssertFatal(1==0,"Not implemented yet\n");
}

int CU_handle_UE_CONTEXT_RELEASE_REQUEST(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(stream);
  f1ap_ue_context_rel_req_t req = {0};
  if (!decode_ue_context_rel_req(pdu, &req)) {
    LOG_E(F1AP, "cannot decode F1 UE Context Release Request\n");
    free_ue_context_rel_req(&req);
    return -1;
  }

  MessageDef *msg = itti_alloc_new_message(TASK_CU_F1, 0,  F1AP_UE_CONTEXT_RELEASE_REQ);
  msg->ittiMsgHeader.originInstance = assoc_id;
  F1AP_UE_CONTEXT_RELEASE_REQ(msg) = req;
  itti_send_msg_to_task(TASK_RRC_GNB, instance, msg);
  return 0;
}

int CU_send_UE_CONTEXT_RELEASE_COMMAND(sctp_assoc_t assoc_id, f1ap_ue_context_rel_cmd_t *cmd)
{
  F1AP_F1AP_PDU_t *pdu = encode_ue_context_rel_cmd(cmd);

  uint8_t *buffer = NULL;
  uint32_t len = 0;
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 context release command\n");
    return -1;
  }

  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  return 0;
}

int CU_handle_UE_CONTEXT_RELEASE_COMPLETE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(stream);
  f1ap_ue_context_rel_cplt_t cplt = {0};
  if (!decode_ue_context_rel_cplt(pdu, &cplt)) {
    LOG_E(F1AP, "cannot decode F1 UE Context Release Complete\n");
    free_ue_context_rel_cplt(&cplt);
    return -1;
  }

  MessageDef *msg_p = itti_alloc_new_message(TASK_DU_F1, 0,  F1AP_UE_CONTEXT_RELEASE_COMPLETE);
  msg_p->ittiMsgHeader.originInstance = assoc_id;
  F1AP_UE_CONTEXT_RELEASE_COMPLETE(msg_p) = cplt;
  itti_send_msg_to_task(TASK_RRC_GNB, instance, msg_p);
  return 0;
}

int CU_send_UE_CONTEXT_MODIFICATION_REQUEST(sctp_assoc_t assoc_id, const f1ap_ue_context_mod_req_t *req)
{
  F1AP_F1AP_PDU_t *pdu = encode_ue_context_mod_req(req);

  uint8_t  *buffer=NULL;
  uint32_t  len=0;
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 UE Context Modification Request\n");
    return -1;
  }

  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  return 0;
}

int CU_handle_UE_CONTEXT_MODIFICATION_RESPONSE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(stream);
  f1ap_ue_context_mod_resp_t resp = {0};
  if (!decode_ue_context_mod_resp(pdu, &resp)) {
    LOG_E(F1AP, "cannot decode F1 UE Context Modification Response\n");
    free_ue_context_mod_resp(&resp);
    return -1;
  }
  MessageDef *msg_p = itti_alloc_new_message(TASK_DU_F1, 0, F1AP_UE_CONTEXT_MODIFICATION_RESP);
  msg_p->ittiMsgHeader.originInstance = assoc_id;
  F1AP_UE_CONTEXT_MODIFICATION_RESP(msg_p) = resp;
  itti_send_msg_to_task(TASK_RRC_GNB, instance, msg_p);
  return 0;
}

int CU_handle_UE_CONTEXT_MODIFICATION_FAILURE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(instance);
  UNUSED(assoc_id);
  UNUSED(stream);
  UNUSED(pdu);
  AssertFatal(1 == 0, "Not implemented yet\n");
}

int CU_handle_UE_CONTEXT_MODIFICATION_REQUIRED(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(stream);
  f1ap_ue_context_modif_required_t required = {0};
  if (!decode_ue_context_modif_required(pdu, &required)) {
    LOG_E(F1AP, "cannot decode F1 UE Context Modification Required\n");
    free_ue_context_modif_required(&required);
    return -1;
  }

  MessageDef *msg_p = itti_alloc_new_message(TASK_DU_F1, 0, F1AP_UE_CONTEXT_MODIFICATION_REQUIRED);
  msg_p->ittiMsgHeader.originInstance = assoc_id;
  F1AP_UE_CONTEXT_MODIFICATION_REQUIRED(msg_p) = required;
  itti_send_msg_to_task(TASK_RRC_GNB, instance, msg_p);
  return 0;
}

int CU_send_UE_CONTEXT_MODIFICATION_CONFIRM(sctp_assoc_t assoc_id, f1ap_ue_context_modif_confirm_t *confirm)
{
  F1AP_F1AP_PDU_t *pdu = encode_ue_context_modif_confirm(confirm);

  uint8_t *buffer = NULL;
  uint32_t len = 0;
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 UE Context Modification Confirm\n");
    return -1;
  }

  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  return 0;
}

int CU_send_UE_CONTEXT_MODIFICATION_REFUSE(sctp_assoc_t assoc_id, f1ap_ue_context_modif_refuse_t *refuse)
{
  F1AP_F1AP_PDU_t *pdu = encode_ue_context_modif_refuse(refuse);

  uint8_t *buffer = NULL;
  uint32_t len = 0;
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 UE Context Modification Refuse\n");
    return -1;
  }

  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  return 0;
}
