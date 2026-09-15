/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdlib.h>
#include "f1ap_common.h"
#include "f1ap_encoder.h"
#include "f1ap_itti_messaging.h"
#include "f1ap_cu_interface_management.h"
#include "f1ap_default_values.h"
#include "lib/f1ap_interface_management.h"

#include "F1AP_F1AP-PDU.h"

#ifdef E2_AGENT
#include "openair2/E2AP/RAN_FUNCTION/setup_msg_store.h"
#endif

int CU_handle_RESET_ACKNOWLEDGE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  DevAssert(pdu != NULL);
  UNUSED(stream);
  f1ap_reset_ack_t ack = {0};
  /* Decode */
  if (!decode_f1ap_reset_ack(pdu, &ack)) {
    LOG_E(F1AP, "cannot decode F1AP Reset acknowledgement\n");
    free_f1ap_reset_ack(&ack);
    return -1;
  }
  /* Send to RRC (ITTI) */
  MessageDef *message_p = itti_alloc_new_message(TASK_CU_F1, 0, F1AP_RESET_ACK);
  message_p->ittiMsgHeader.originInstance = assoc_id;
  F1AP_RESET_ACK(message_p) = ack; /* "move" message into ITTI, RRC thread will free it */
  itti_send_msg_to_task(TASK_RRC_GNB, GNB_MODULE_ID_TO_INSTANCE(instance), message_p);
  return 0;
}

int CU_send_RESET_ACKNOWLEDGE(sctp_assoc_t assoc_id, const f1ap_reset_ack_t *ack)
{
  UNUSED(assoc_id);
  UNUSED(ack);
  AssertFatal(1 == 0, "Not implemented yet\n");
}

void CU_send_RESET(sctp_assoc_t assoc_id, const f1ap_reset_t *reset)
{
  uint8_t *buffer = NULL;
  uint32_t len = 0;

  /* Encode F1 Reset */
  F1AP_F1AP_PDU_t *pdu = encode_f1ap_reset(reset);
  if (pdu == NULL) {
    LOG_E(F1AP, "failed to create asn.1 message for f1ap reset\n");
    return;
  }

  /* encode ASN.1 PER */
  int encoded = f1ap_encode_pdu(pdu, &buffer, &len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  if (encoded <= 0) {
    LOG_E(F1AP, "Failed to encode F1 reset\n");
    return;
  }
  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
}

/**
 * @brief F1AP Setup Request decoding (9.2.1.4 of 3GPP TS 38.473) and transfer to RRC
 */
int CU_handle_F1_SETUP_REQUEST(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  LOG_D(F1AP, "CU_handle_F1_SETUP_REQUEST\n");
  DevAssert(pdu != NULL);
  /* F1 Setup Request == Non UE-related procedure -> stream 0 */
  if (stream != 0) {
    LOG_W(F1AP, "[SCTP %d] Received f1 setup request on stream != 0 (%d)\n",
          assoc_id, stream);
  }
  f1ap_setup_req_t msg = {0};
  /* Decode */
  if (!decode_f1ap_setup_request(pdu, &msg)) {
    LOG_E(F1AP, "cannot decode F1AP Setup Request\n");
    free_f1ap_setup_request(&msg);
    return -1;
  }
  /* Send to RRC (ITTI) */
  MessageDef *message_p = itti_alloc_new_message(TASK_CU_F1, 0, F1AP_SETUP_REQ);
  message_p->ittiMsgHeader.originInstance = assoc_id;
  f1ap_setup_req_t *req = &F1AP_SETUP_REQ(message_p);
  *req = msg; /* "move" message into ITTI, RRC thread will free it */
  itti_send_msg_to_task(TASK_RRC_GNB, GNB_MODULE_ID_TO_INSTANCE(instance), message_p);
  return 0;
}

/**
 * @brief F1AP Setup Response encoding (9.2.1.5 of 3GPP TS 38.473) and message transfer
 */
int CU_send_F1_SETUP_RESPONSE(sctp_assoc_t assoc_id, f1ap_setup_resp_t *f1ap_setup_resp)
{
  uint8_t  *buffer=NULL;
  uint32_t len = 0;

  /* Encode F1 Setup Response */
  F1AP_F1AP_PDU_t *pdu = encode_f1ap_setup_response(f1ap_setup_resp);
  /* Free after encode */
  free_f1ap_setup_response(f1ap_setup_resp);

  /* encode */
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 setup response\n");
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    return -1;
  }
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
#ifdef E2_AGENT
  e2ap_store_setup_resp(E2AP_SETUP_MSG_F1AP, buffer, len);
#endif
  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  return 0;
}

/**
 * @brief F1 Setup Failure encoding and transmission
 */
int CU_send_F1_SETUP_FAILURE(sctp_assoc_t assoc_id, const f1ap_setup_failure_t *fail)
{
  LOG_D(F1AP, "CU_send_F1_SETUP_FAILURE\n");
  uint8_t *buffer = NULL;
  uint32_t len = 0;
  /* Encode F1 Setup Failure */
  F1AP_F1AP_PDU_t *pdu = encode_f1ap_setup_failure(fail);
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 setup failure\n");
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    return -1;
  }
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  return 0;
}

/**
 * @brief Decode and send F1 gNB-DU Configuration Update message to RRC
 */
int CU_handle_gNB_DU_CONFIGURATION_UPDATE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  LOG_D(F1AP, "[SCTP %d] CU_handle_gNB_DU_CONFIGURATION_UPDATE\n", assoc_id);
  DevAssert(pdu != NULL);
  /* gNB DU Configuration Update == Non UE-related procedure -> stream 0 */
  if (stream != 0) {
    LOG_W(F1AP, "[SCTP %d] Received f1 setup request on stream != 0 (%d)\n", assoc_id, stream);
  }
  /* Decode */
  f1ap_gnb_du_configuration_update_t msg = {0};
  if (!decode_f1ap_du_configuration_update(pdu, &msg)) {
    LOG_E(F1AP, "cannot decode F1AP gNB-DU Configuration Update\n");
    free_f1ap_du_configuration_update(&msg);
    return -1;
  }
  /* Send to RRC */
  MessageDef *message_p = itti_alloc_new_message(TASK_CU_F1, 0, F1AP_GNB_DU_CONFIGURATION_UPDATE);
  message_p->ittiMsgHeader.originInstance = assoc_id;
  f1ap_gnb_du_configuration_update_t *req = &F1AP_GNB_DU_CONFIGURATION_UPDATE(message_p); // RRC thread will free it
  *req = msg; // copy F1 message to ITTI
  LOG_D(F1AP, "Sending F1AP_GNB_DU_CONFIGURATION_UPDATE ITTI message \n");
  itti_send_msg_to_task(TASK_RRC_GNB, GNB_MODULE_ID_TO_INSTANCE(instance), message_p);
  return 0;
}

int CU_send_gNB_DU_CONFIGURATION_UPDATE_ACKNOWLEDGE(sctp_assoc_t assoc_id, f1ap_gnb_du_configuration_update_acknowledge_t *msg)
{
  uint8_t *buffer;
  uint32_t len;
  /* Encode F1 gNB-CU Configuration Update message */
  F1AP_F1AP_PDU_t *pdu = encode_f1ap_du_configuration_update_acknowledge(msg);
  /* Encode F1AP PDU */
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    LOG_E(F1AP, "Failed to encode F1 gNB-DU Configuration Update Acknowledge\n");
    return -1;
  }
  LOG_DUMPMSG(F1AP, DEBUG_F1AP, buffer, len, "F1AP gNB-DU CONFIGURATION UPDATE : ");
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  return 0;
}

/**
 * @brief F1 gNB-CU Configuration Update message encoding and transfer
 */
int CU_send_gNB_CU_CONFIGURATION_UPDATE(sctp_assoc_t assoc_id, f1ap_gnb_cu_configuration_update_t *msg)
{
  /* complete F1AP message */
  msg->transaction_id = F1AP_get_next_transaction_identifier(0, 0); // note: has to be done in the caller
  uint8_t  *buffer;
  uint32_t  len;
  /* Encode F1 gNB-CU Configuration Update message */
  F1AP_F1AP_PDU_t *pdu = encode_f1ap_cu_configuration_update(msg);
  /* Free after encoding */
  free_f1ap_cu_configuration_update(msg);
  /* Encode F1AP PDU */
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    LOG_E(F1AP, "Failed to encode F1 gNB-CU Configuration Update\n");
    return -1;
  }
  LOG_DUMPMSG(F1AP, DEBUG_F1AP, buffer, len, "F1AP gNB-CU CONFIGURATION UPDATE : ");
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  return 0;
}

/**
 * @brief gNB CU Configuration Update Acknowledge decoding (9.2.1.11 of 3GPP TS 38.473)
 *        and handling by CU
 */
int CU_handle_gNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE(instance_t instance,
                                                      sctp_assoc_t assoc_id,
                                                      uint32_t stream,
                                                      F1AP_F1AP_PDU_t *pdu)
{
  DevAssert(pdu != NULL);
  if (stream != 0)
    LOG_W(F1AP, "[SCTP %d] Received gNB CU Configuration Update Acknowledge on stream != 0 (%d)\n", assoc_id, stream);
  // Decode
  f1ap_gnb_cu_configuration_update_acknowledge_t msg = {0};
  if (!decode_f1ap_cu_configuration_update_acknowledge(pdu, &msg)) {
    LOG_E(F1AP, "Failed to decode gNB CU Configuration Update Acknowledge\n");
    return -1;
  }
  // Allocate ITTI message and send to RRC
  MessageDef *message_p = itti_alloc_new_message(TASK_CU_F1, 0, F1AP_GNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE);
  message_p->ittiMsgHeader.originInstance = assoc_id;
  f1ap_gnb_cu_configuration_update_acknowledge_t *ack = &F1AP_GNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE(message_p);
  *ack = msg; // copy decoded message to ITTI, the RRC thread will free it
  itti_send_msg_to_task(TASK_RRC_GNB, GNB_MODULE_ID_TO_INSTANCE(instance), message_p);
  return 0;
}
