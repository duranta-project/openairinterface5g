/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdlib.h>
#include "f1ap_common.h"
#include "f1ap_encoder.h"
#include "f1ap_itti_messaging.h"
#include "f1ap_du_interface_management.h"
#include "lib/f1ap_interface_management.h"
#include "openair2/LAYER2/NR_MAC_gNB/mac_rrc_dl_handler.h"
#include "assertions.h"

#include "GNB_APP/gnb_paramdef.h"

#include "F1AP_F1AP-PDU.h"
#include "F1AP_Reset.h"
#include "F1AP_ProtocolIE-Field.h"
#include "F1AP_InitiatingMessage.h"

#ifdef E2_AGENT
#include "openair2/E2AP/RAN_FUNCTION/setup_msg_store.h"
#endif

int DU_handle_RESET(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(instance);
  LOG_D(F1AP, "DU_handle_RESET\n");\
  F1AP_Reset_t  *container;
  F1AP_ResetIEs_t *ie;
  DevAssert(pdu != NULL);
  container = &pdu->choice.initiatingMessage->value.choice.Reset;

  /* Reset == Non UE-related procedure -> stream 0 */
  if (stream != 0) {
    LOG_W(F1AP, "[SCTP %d] Received Reset on stream != 0 (%d)\n",
        assoc_id, stream);
  }

  MessageDef *msg_p = itti_alloc_new_message(TASK_DU_F1, 0, F1AP_RESET);
  msg_p->ittiMsgHeader.originInstance = assoc_id;
  f1ap_reset_t *f1ap_reset = &F1AP_RESET(msg_p);

  /* Transaction ID */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_ResetIEs_t, ie, container, F1AP_ProtocolIE_ID_id_TransactionID, true);
  f1ap_reset->transaction_id = ie->value.choice.TransactionID;
  LOG_D(F1AP, "req->transaction_id %lu \n", f1ap_reset->transaction_id);
  
  /* Cause */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_ResetIEs_t, ie, container, F1AP_ProtocolIE_ID_id_Cause, true);
  switch(ie->value.choice.Cause.present) 
  {
    case F1AP_Cause_PR_radioNetwork:
      LOG_D(F1AP, "Cause: Radio Network\n");
      f1ap_reset->cause = F1AP_CAUSE_RADIO_NETWORK;
      f1ap_reset->cause_value = ie->value.choice.Cause.choice.radioNetwork;
      break;
    case F1AP_Cause_PR_transport:
      LOG_D(F1AP, "Cause: Transport\n");
      f1ap_reset->cause = F1AP_CAUSE_TRANSPORT;
      f1ap_reset->cause_value = ie->value.choice.Cause.choice.transport;
      break;
    case F1AP_Cause_PR_protocol:
      LOG_D(F1AP, "Cause: Protocol\n");
      f1ap_reset->cause = F1AP_CAUSE_PROTOCOL;
      f1ap_reset->cause_value = ie->value.choice.Cause.choice.protocol;
      break;
    case F1AP_Cause_PR_misc:
      LOG_D(F1AP, "Cause: Misc\n");
      f1ap_reset->cause = F1AP_CAUSE_MISC;
      f1ap_reset->cause_value = ie->value.choice.Cause.choice.misc;
      break;
    default:
      AssertFatal(1==0,"Unknown cause\n");
  }

  /* ResetType */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_ResetIEs_t, ie, container, F1AP_ProtocolIE_ID_id_ResetType, true);
  switch(ie->value.choice.ResetType.present) {
    case F1AP_ResetType_PR_f1_Interface:
      LOG_D(F1AP, "ResetType: F1 Interface\n");
      f1ap_reset->reset_type = F1AP_RESET_ALL;
      break;
    case F1AP_ResetType_PR_partOfF1_Interface:
      LOG_D(F1AP, "ResetType: Part of F1 Interface\n");
      f1ap_reset->reset_type = F1AP_RESET_PART_OF_F1_INTERFACE;
      break;
    default:
      AssertFatal(1==0,"Unknown reset type\n");
  }

  /* Part of F1 Interface */
  if (f1ap_reset->reset_type == F1AP_RESET_PART_OF_F1_INTERFACE) {
    AssertFatal(1==0, "Not implemented yet\n");
  }
  
  f1_reset_cu_initiated(f1ap_reset);
  return 0;
}

int DU_send_RESET_ACKNOWLEDGE(sctp_assoc_t assoc_id, const f1ap_reset_ack_t *ack)
{
  F1AP_F1AP_PDU_t *pdu = encode_f1ap_reset_ack(ack);
  if (!pdu) {
    LOG_E(F1AP, "failed to create ASN.1 message for reset acknowledge\n");
    return -1;
  }

  /* encode */
  uint8_t  *buffer;
  uint32_t  len;
  int encoded = f1ap_encode_pdu(pdu, &buffer, &len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  if (encoded <= 0) {
    LOG_E(F1AP, "Failed to encode F1ResetAcknowledge\n");
    return -1;
  }
  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  return 0;
}

/**
 * @brief F1AP Setup Request
 */
int DU_send_F1_SETUP_REQUEST(sctp_assoc_t assoc_id, const f1ap_setup_req_t *setup_req)
{
  LOG_D(F1AP, "DU_send_F1_SETUP_REQUEST\n");
  F1AP_F1AP_PDU_t *pdu = encode_f1ap_setup_request(setup_req);
  uint8_t  *buffer;
  uint32_t  len;
  /* encode */
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 setup request\n");
    /* free PDU */
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    return -1;
  }
  /* free PDU */
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
#ifdef E2_AGENT
  e2ap_store_setup_req(E2AP_SETUP_MSG_F1AP, buffer, len);
#endif
  /* Send to ITTI */
  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  return 0;
}

int DU_handle_F1_SETUP_RESPONSE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(instance);
  UNUSED(assoc_id);
  UNUSED(stream);
  LOG_D(F1AP, "DU_handle_F1_SETUP_RESPONSE\n");
  /* Decode */
  f1ap_setup_resp_t resp = {0};
  if (!decode_f1ap_setup_response(pdu, &resp)) {
    LOG_E(F1AP, "cannot decode F1AP Setup Response\n");
    free_f1ap_setup_response(&resp);
    return -1;
  }
  LOG_D(F1AP, "Sending F1AP_SETUP_RESP ITTI message\n");
  f1_setup_response(&resp);
  // free F1AP message after use
  free_f1ap_setup_response(&resp);
  return 0;
}

/**
 * @brief F1 Setup Failure handler (DU)
 */
int DU_handle_F1_SETUP_FAILURE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(instance);
  UNUSED(assoc_id);
  UNUSED(stream);
  f1ap_setup_failure_t fail;
  if (!decode_f1ap_setup_failure(pdu, &fail)) {
    LOG_E(F1AP, "Failed to decode F1AP Setup Failure\n");
    return -1;
  }
  f1_setup_failure(&fail);
  return 0;
}

/**
 * @brief F1 gNB-DU Configuration Update: encoding and ITTI transmission
 */
int DU_send_gNB_DU_CONFIGURATION_UPDATE(sctp_assoc_t assoc_id, f1ap_gnb_du_configuration_update_t *msg)
{
  uint8_t  *buffer=NULL;
  uint32_t  len=0;
  /* encode F1AP message */
  F1AP_F1AP_PDU_t *pdu = encode_f1ap_du_configuration_update(msg);
  /* free anfter encoding */
  free_f1ap_du_configuration_update(msg);
  /* encode F1AP pdu */
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 gNB-DU CONFIGURATION UPDATE\n");
    return -1;
  }
  /* transfer the message */
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  return 0;
}

/**
 * @brief F1 gNB-CU Configuration Update decoding and message transfer
 */
int DU_handle_gNB_CU_CONFIGURATION_UPDATE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(instance);
  UNUSED(stream);
  LOG_D(F1AP, "DU_handle_gNB_CU_CONFIGURATION_UPDATE\n");
  f1ap_gnb_cu_configuration_update_t in = {0};
  if (!decode_f1ap_cu_configuration_update(pdu, &in)) {
    LOG_E(F1AP, "Failed to decode F1AP Setup Failure\n");
    free_f1ap_cu_configuration_update(&in);
    return -1;
  }
  MessageDef *msg_p = itti_alloc_new_message (TASK_DU_F1, 0, F1AP_GNB_CU_CONFIGURATION_UPDATE);
  f1ap_gnb_cu_configuration_update_t *msg = &F1AP_GNB_CU_CONFIGURATION_UPDATE(msg_p); // RRC thread will free it
  *msg = in; // copy F1 message to ITTI
  LOG_D(F1AP, "Sending F1AP_GNB_CU_CONFIGURATION_UPDATE ITTI message \n");
  itti_send_msg_to_task(TASK_GNB_APP, GNB_MODULE_ID_TO_INSTANCE(assoc_id), msg_p);
  return 0;
}

/**
 * @brief F1 gNB-DU Configuration Update Acknowledge decoding and message transfer
 */
int DU_handle_gNB_DU_CONFIGURATION_UPDATE_ACKNOWLEDGE(instance_t instance,
                                                      sctp_assoc_t assoc_id,
                                                      uint32_t stream,
                                                      F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(instance);
  UNUSED(assoc_id);
  UNUSED(stream);
  // Decoding
  f1ap_gnb_du_configuration_update_acknowledge_t in = {0};
  if (!decode_f1ap_du_configuration_update_acknowledge(pdu, &in)) {
    LOG_E(F1AP, "Failed to decode gNB-DU Configuration Update Acknowledge\n");
    free_f1ap_du_configuration_update_acknowledge(&in);
    return -1;
  }

  gnb_du_configuration_update_acknowledge(&in);
  free_f1ap_du_configuration_update_acknowledge(&in);
  return 0;
}

int DU_send_gNB_CU_CONFIGURATION_UPDATE_FAILURE(sctp_assoc_t assoc_id, 
                                                f1ap_gnb_cu_configuration_update_failure_t *GNBCUConfigurationUpdateFailure)
{
  UNUSED(assoc_id);
  AssertFatal(1==0,"received gNB CU CONFIGURATION UPDATE FAILURE with cause %d\n",
              GNBCUConfigurationUpdateFailure->cause);
}

/**
 * @brief Encode and transfer F1 GNB CU Configuration Update Acknowledge message
 */
int DU_send_gNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE(sctp_assoc_t assoc_id, f1ap_gnb_cu_configuration_update_acknowledge_t *msg)
{
  uint8_t *buffer = NULL;
  uint32_t len = 0;
  /* encode F1 message */
  F1AP_F1AP_PDU_t *pdu = encode_f1ap_cu_configuration_update_acknowledge(msg);
  /* encode F1AP PDU */
  if (!pdu || f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode GNB-CU-Configuration-Update-Acknowledge\n");
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    return -1;
  }
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  return 0;
}
