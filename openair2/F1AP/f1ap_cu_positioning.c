/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include "f1ap_common.h"
#include "f1ap_encoder.h"
#include "f1ap_itti_messaging.h"
#include "f1ap_cu_positioning.h"
#include "lib/f1ap_positioning.h"
#include "common/utils/ds/byte_array.h"

int CU_send_POS_ASSISTANCE_CONTROL(sctp_assoc_t assoc_id, const f1ap_pos_assistance_control_t *control)
{
  F1AP_F1AP_PDU_t *pdu = encode_pos_assistance_control(control);
  if (!pdu) {
    LOG_E(F1AP, "Invalid positioning assistance control\n");
    return -1;
  }
  uint8_t *buffer = NULL;
  uint32_t len = 0;
  int ret = f1ap_encode_pdu(pdu, &buffer, &len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  if (ret < 0) {
    LOG_E(F1AP, "Failed to encode positioning assistance control\n");
    return -1;
  }
  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  return 0;
}

int CU_send_TRP_INFORMATION_REQUEST(sctp_assoc_t assoc_id, f1ap_trp_information_req_t *req)
{
  F1AP_F1AP_PDU_t *pdu = encode_trp_information_req(req);
  if (pdu == NULL) {
    LOG_E(F1AP, "Failed to encode F1 TRP Information Request\n");
    return -1;
  }

  byte_array_t ba = {0};
  uint32_t encoded_len = 0;
  /* encode */
  if (f1ap_encode_pdu(pdu, &ba.buf, &encoded_len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 TRP Information Request PDU\n");
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    return -1;
  }
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  /* Ownership of ba.buf is transferred to SCTP task; do not free here. */
  f1ap_itti_send_sctp_data_req(assoc_id, ba.buf, encoded_len);
  return 0;
}

int CU_handle_TRP_INFORMATION_RESPONSE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  f1ap_trp_information_resp_t resp = {0};
  if (!decode_trp_information_resp(pdu, &resp)) {
    LOG_E(F1AP, "cannot decode F1 TRP Information Response\n");
    free_trp_information_resp(&resp);
    return -1;
  }

  MessageDef *msg_p = itti_alloc_new_message(TASK_DU_F1, 0, F1AP_TRP_INFORMATION_RESP);
  msg_p->ittiMsgHeader.originInstance = assoc_id;
  F1AP_TRP_INFORMATION_RESP(msg_p) = resp;
  itti_send_msg_to_task(TASK_RRC_GNB, instance, msg_p);
  return 0;
}

int CU_handle_TRP_INFORMATION_FAILURE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  f1ap_trp_information_failure_t fail = {0};
  if (!decode_trp_information_failure(pdu, &fail)) {
    LOG_E(F1AP, "cannot decode F1 TRP Information Failure\n");
    free_trp_information_failure(&fail);
    return -1;
  }

  MessageDef *msg_p = itti_alloc_new_message(TASK_DU_F1, 0, F1AP_TRP_INFORMATION_FAILURE);
  msg_p->ittiMsgHeader.originInstance = assoc_id;
  F1AP_TRP_INFORMATION_FAILURE(msg_p) = fail;
  itti_send_msg_to_task(TASK_RRC_GNB, instance, msg_p);
  return 0;
}

int CU_send_POSITIONING_INFORMATION_REQUEST(sctp_assoc_t assoc_id, f1ap_positioning_information_req_t *req)
{
  F1AP_F1AP_PDU_t *pdu = encode_positioning_information_req(req);
  if (pdu == NULL) {
    LOG_E(F1AP, "Failed to encode F1 Positioning Information Request\n");
    return -1;
  }

  byte_array_t ba = {0};
  uint32_t encoded_len = 0;
  /* encode */
  if (f1ap_encode_pdu(pdu, &ba.buf, &encoded_len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 Positioning Information Request PDU\n");
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    return -1;
  }
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  /* Ownership of ba.buf is transferred to SCTP task; do not free here. */
  f1ap_itti_send_sctp_data_req(assoc_id, ba.buf, encoded_len);
  return 0;
}

int CU_handle_POSITIONING_INFORMATION_RESPONSE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  f1ap_positioning_information_resp_t resp = {0};
  if (!decode_positioning_information_resp(pdu, &resp)) {
    LOG_E(F1AP, "cannot decode F1 Positioning Information Response\n");
    free_positioning_information_resp(&resp);
    return -1;
  }

  MessageDef *msg_p = itti_alloc_new_message(TASK_DU_F1, 0, F1AP_POSITIONING_INFORMATION_RESP);
  msg_p->ittiMsgHeader.originInstance = assoc_id;
  F1AP_POSITIONING_INFORMATION_RESP(msg_p) = resp;
  itti_send_msg_to_task(TASK_RRC_GNB, instance, msg_p);
  return 0;
}

int CU_send_POSITIONING_ACTIVATION_REQUEST(sctp_assoc_t assoc_id, f1ap_positioning_activation_req_t *req)
{
  F1AP_F1AP_PDU_t *pdu = encode_positioning_activation_req(req);
  if (pdu == NULL) {
    LOG_E(F1AP, "Failed to encode F1 Positioning Activation Request\n");
    return -1;
  }

  byte_array_t ba = {0};
  uint32_t encoded_len = 0;
  /* encode */
  if (f1ap_encode_pdu(pdu, &ba.buf, &encoded_len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 Positioning Activation Request PDU\n");
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    return -1;
  }
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  /* Ownership of ba.buf is transferred to SCTP task; do not free here. */
  f1ap_itti_send_sctp_data_req(assoc_id, ba.buf, encoded_len);
  return 0;
}

int CU_handle_POSITIONING_ACTIVATION_RESPONSE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  f1ap_positioning_activation_resp_t resp = {0};
  if (!decode_positioning_activation_resp(pdu, &resp)) {
    LOG_E(F1AP, "cannot decode F1 Positioning Activation Response\n");
    free_positioning_activation_resp(&resp);
    return -1;
  }

  MessageDef *msg_p = itti_alloc_new_message(TASK_DU_F1, 0, F1AP_POSITIONING_ACTIVATION_RESP);
  msg_p->ittiMsgHeader.originInstance = assoc_id;
  F1AP_POSITIONING_ACTIVATION_RESP(msg_p) = resp;
  itti_send_msg_to_task(TASK_RRC_GNB, instance, msg_p);
  return 0;
}

int CU_send_POSITIONING_MEASUREMENT_REQUEST(sctp_assoc_t assoc_id, f1ap_positioning_measurement_req_t *req)
{
  F1AP_F1AP_PDU_t *pdu = encode_positioning_measurement_req(req);
  if (pdu == NULL) {
    LOG_E(F1AP, "Failed to encode F1 Positioning Measurement Request\n");
    return -1;
  }

  byte_array_t ba = {0};
  uint32_t encoded_len = 0;
  /* encode */
  if (f1ap_encode_pdu(pdu, &ba.buf, &encoded_len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 Positioning Measurement Request PDU\n");
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    return -1;
  }
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  /* Ownership of ba.buf is transferred to SCTP task; do not free here. */
  f1ap_itti_send_sctp_data_req(assoc_id, ba.buf, encoded_len);
  return 0;
}

int CU_handle_POSITIONING_MEASUREMENT_RESPONSE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  f1ap_positioning_measurement_resp_t resp = {0};
  if (!decode_positioning_measurement_resp(pdu, &resp)) {
    LOG_E(F1AP, "cannot decode F1 Positioning Measurement Response\n");
    free_positioning_measurement_resp(&resp);
    return -1;
  }

  MessageDef *msg_p = itti_alloc_new_message(TASK_DU_F1, 0, F1AP_POSITIONING_MEASUREMENT_RESP);
  msg_p->ittiMsgHeader.originInstance = assoc_id;
  F1AP_POSITIONING_MEASUREMENT_RESP(msg_p) = resp;
  itti_send_msg_to_task(TASK_RRC_GNB, instance, msg_p);
  return 0;
}

int CU_handle_POSITIONING_MEASUREMENT_FAILURE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  f1ap_positioning_measurement_failure_t fail = {0};
  if (!decode_positioning_measurement_failure(pdu, &fail)) {
    LOG_E(F1AP, "cannot decode F1 Positioning Measurement Failure\n");
    free_positioning_measurement_failure(&fail);
    return -1;
  }

  MessageDef *msg_p = itti_alloc_new_message(TASK_DU_F1, 0, F1AP_POSITIONING_MEASUREMENT_FAILURE);
  msg_p->ittiMsgHeader.originInstance = assoc_id;
  F1AP_POSITIONING_MEASUREMENT_FAILURE(msg_p) = fail;
  itti_send_msg_to_task(TASK_RRC_GNB, instance, msg_p);
  return 0;
}
