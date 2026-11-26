/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include "f1ap_common.h"
#include "f1ap_encoder.h"
#include "f1ap_itti_messaging.h"
#include "f1ap_du_positioning.h"
#include "lib/f1ap_positioning.h"
#include "openair2/LAYER2/NR_MAC_gNB/mac_rrc_dl_handler.h"

int DU_handle_POS_ASSISTANCE_CONTROL(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  f1ap_pos_assistance_control_t control = {0};
  if (!decode_pos_assistance_control(pdu, &control)) {
    LOG_E(F1AP, "Cannot decode positioning assistance control\n");
    return -1;
  }
  pos_assistance_control(&control);
  free_pos_assistance_control(&control);
  return 0;
}

int DU_handle_TRP_INFORMATION_REQUEST(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  f1ap_trp_information_req_t req = {0};
  if (!decode_trp_information_req(pdu, &req)) {
    LOG_E(F1AP, "cannot decode F1 TRP Information Request\n");
    free_trp_information_req(&req);
    return -1;
  }

  trp_information_request(&req);
  free_trp_information_req(&req);

  return 0;
}

int DU_send_TRP_INFORMATION_RESPONSE(sctp_assoc_t assoc_id, f1ap_trp_information_resp_t *resp)
{
  F1AP_F1AP_PDU_t *pdu = encode_trp_information_resp(resp);

  uint8_t *buffer = NULL;
  uint32_t len = 0;
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 TRP INFORMATION RESPONSE\n");
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    return -1;
  }

  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  return 0;
}

int DU_send_TRP_INFORMATION_FAILURE(sctp_assoc_t assoc_id, f1ap_trp_information_failure_t *fail)
{
  F1AP_F1AP_PDU_t *pdu = encode_trp_information_failure(fail);

  uint8_t *buffer = NULL;
  uint32_t len = 0;
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 TRP INFORMATION FAILURE\n");
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    return -1;
  }

  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  return 0;
}

int DU_handle_POSITIONING_INFORMATION_REQUEST(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  f1ap_positioning_information_req_t req = {0};
  if (!decode_positioning_information_req(pdu, &req)) {
    LOG_E(F1AP, "cannot decode F1 Positioning Information Request\n");
    free_positioning_information_req(&req);
    return -1;
  }

  positioning_information_request(&req);
  free_positioning_information_req(&req);

  return 0;
}

int DU_send_POSITIONING_INFORMATION_RESPONSE(sctp_assoc_t assoc_id, f1ap_positioning_information_resp_t *resp)
{
  F1AP_F1AP_PDU_t *pdu = encode_positioning_information_resp(resp);

  uint8_t *buffer = NULL;
  uint32_t len = 0;
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 Positioning INFORMATION RESPONSE\n");
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    return -1;
  }

  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  return 0;
}

int DU_handle_POSITIONING_ACTIVATION_REQUEST(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  f1ap_positioning_activation_req_t req = {0};
  if (!decode_positioning_activation_req(pdu, &req)) {
    LOG_E(F1AP, "cannot decode F1 Positioning Activation Request\n");
    free_positioning_activation_req(&req);
    return -1;
  }

  positioning_activation_request(&req);
  free_positioning_activation_req(&req);

  return 0;
}

int DU_send_POSITIONING_ACTIVATION_RESPONSE(sctp_assoc_t assoc_id, f1ap_positioning_activation_resp_t *resp)
{
  F1AP_F1AP_PDU_t *pdu = encode_positioning_activation_resp(resp);

  uint8_t *buffer = NULL;
  uint32_t len = 0;
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 Positioning Activation Response\n");
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    return -1;
  }

  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  return 0;
}

int DU_handle_POSITIONING_MEASUREMENT_REQUEST(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  f1ap_positioning_measurement_req_t req = {0};
  if (!decode_positioning_measurement_req(pdu, &req)) {
    LOG_E(F1AP, "cannot decode F1 Positioning Measurement Request\n");
    free_positioning_measurement_req(&req);
    return -1;
  }

  positioning_measurement_request(&req);
  free_positioning_measurement_req(&req);

  return 0;
}

int DU_send_POSITIONING_MEASUREMENT_RESPONSE(sctp_assoc_t assoc_id, f1ap_positioning_measurement_resp_t *resp)
{
  F1AP_F1AP_PDU_t *pdu = encode_positioning_measurement_resp(resp);

  uint8_t *buffer = NULL;
  uint32_t len = 0;
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 Positioning Measurement Response\n");
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    return -1;
  }

  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  return 0;
}

int DU_send_POSITIONING_MEASUREMENT_FAILURE(sctp_assoc_t assoc_id, f1ap_positioning_measurement_failure_t *fail)
{
  F1AP_F1AP_PDU_t *pdu = encode_positioning_measurement_failure(fail);

  uint8_t *buffer = NULL;
  uint32_t len = 0;
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 Positioning Measurement Failure\n");
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    return -1;
  }

  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  return 0;
}
