/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "f1ap_common.h"
#include "f1ap_cu_interface_management.h"
#include "f1ap_du_interface_management.h"
#include "f1ap_cu_rrc_message_transfer.h"
#include "f1ap_du_rrc_message_transfer.h"
#include "f1ap_cu_ue_context_management.h"
#include "f1ap_du_ue_context_management.h"
#include "f1ap_du_paging.h"
#include "f1ap_du_positioning.h"
#include "f1ap_cu_positioning.h"

#include "F1AP_F1AP-PDU.h"
#include "F1AP_InitiatingMessage.h"
#include "F1AP_ProcedureCode.h"

#ifdef E2_AGENT
#include "openair2/E2AP/RAN_FUNCTION/setup_msg_store.h"
#endif

/* Handlers matrix. Only f1 related procedure present here */
static const f1ap_message_processing_t f1ap_messages_processing[][3] = {

    // TODO: How to handle RESET if CU/DU has their respective handlers? 
    // We need to check node type and call the right handler.
    {DU_handle_RESET, CU_handle_RESET_ACKNOWLEDGE, 0}, /* Reset */ 
    // {CU_handle_RESET, DU_handle_RESET_ACKNOWLEDGE, 0}, /* Reset */ 
    {CU_handle_F1_SETUP_REQUEST, DU_handle_F1_SETUP_RESPONSE, DU_handle_F1_SETUP_FAILURE}, /* F1Setup */
    {0, 0, 0}, /* ErrorIndication */
    {CU_handle_gNB_DU_CONFIGURATION_UPDATE, DU_handle_gNB_DU_CONFIGURATION_UPDATE_ACKNOWLEDGE, 0}, /* gNBDUConfigurationUpdate */
    {DU_handle_gNB_CU_CONFIGURATION_UPDATE, CU_handle_gNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE, 0}, /* gNBCUConfigurationUpdate */
    {DU_handle_UE_CONTEXT_SETUP_REQUEST, CU_handle_UE_CONTEXT_SETUP_RESPONSE, 0}, /* UEContextSetup */
    {DU_handle_UE_CONTEXT_RELEASE_COMMAND, CU_handle_UE_CONTEXT_RELEASE_COMPLETE, 0}, /* UEContextRelease */
    {DU_handle_UE_CONTEXT_MODIFICATION_REQUEST, CU_handle_UE_CONTEXT_MODIFICATION_RESPONSE, 0}, /* UEContextModification */
    {CU_handle_UE_CONTEXT_MODIFICATION_REQUIRED, DU_handle_UE_CONTEXT_MODIFICATION_CONFIRM, DU_handle_UE_CONTEXT_MODIFICATION_REFUSE}, /* UEContextModificationRequired */
    {0, 0, 0}, /* UEMobilityCommand */
    {CU_handle_UE_CONTEXT_RELEASE_REQUEST, 0, 0}, /* UEContextReleaseRequest */
    {CU_handle_INITIAL_UL_RRC_MESSAGE_TRANSFER, 0, 0}, /* InitialULRRCMessageTransfer */
    {DU_handle_DL_RRC_MESSAGE_TRANSFER, 0, 0}, /* DLRRCMessageTransfer */
    {CU_handle_UL_RRC_MESSAGE_TRANSFER, 0, 0}, /* ULRRCMessageTransfer */
    {0, 0, 0}, /* privateMessage */
    {0, 0, 0}, /* UEInactivityNotification */
    {0, 0, 0}, /* GNBDUResourceCoordination */
    {0, 0, 0}, /* SystemInformationDeliveryCommand */
    {DU_handle_Paging, 0, 0}, /* Paging */
    {0, 0, 0}, /* Notify */
    {0, 0, 0}, /* WriteReplaceWarning */
    {0, 0, 0}, /* PWSCancel */
    {0, 0, 0}, /* PWSRestartIndication */
    {0, 0, 0}, /* PWSFailureIndication */
    {0, 0, 0}, /* GNBDUStatusIndication */
    {0, 0, 0}, /* RRCDeliveryReport */
    {0, 0, 0}, /* F1Removal */
    {0, 0, 0}, /* NetworkAccessRateReduction */
    {0, 0, 0}, /* TraceStart */
    {0, 0, 0}, /* DeactivateTrace */
    {0, 0, 0}, /* DUCURadioInformationTransfer */
    {0, 0, 0}, /* CUDURadioInformationTransfer */
    {0, 0, 0}, /* BAPMappingConfiguration */
    {0, 0, 0}, /* GNBDUResourceConfiguration */
    {0, 0, 0}, /* IABTNLAddressAllocation */
    {0, 0, 0}, /* IABUPConfigurationUpdate */
    {0, 0, 0}, /* resourceStatusReportingInitiation */
    {0, 0, 0}, /* resourceStatusReporting */
    {0, 0, 0}, /* accessAndMobilityIndication */
    {0, 0, 0}, /* accessSuccess */
    {0, 0, 0}, /* cellTrafficTrace */
    {DU_handle_POSITIONING_MEASUREMENT_REQUEST, CU_handle_POSITIONING_MEASUREMENT_RESPONSE, 0}, /* PositioningMeasurementExchange */
    {0, 0, 0}, /* PositioningAssistanceInformationControl */
    {0, 0, 0}, /* PositioningAssistanceInformationFeedback */
    {0, 0, 0}, /* PositioningMeasurementReport */
    {0, 0, 0}, /* PositioningMeasurementAbort */
    {0, 0, 0}, /* PositioningMeasurementFailureIndication */
    {0, 0, 0}, /* PositioningMeasurementUpdate */
    {DU_handle_TRP_INFORMATION_REQUEST, CU_handle_TRP_INFORMATION_RESPONSE, 0}, /* TRPInformationExchange */
    {DU_handle_POSITIONING_INFORMATION_REQUEST, CU_handle_POSITIONING_INFORMATION_RESPONSE, 0}, /* PositioningInformationExchange */
    {DU_handle_POSITIONING_ACTIVATION_REQUEST, CU_handle_POSITIONING_ACTIVATION_RESPONSE, 0}, /* PositioningActivation */
    {0, 0, 0}, /* PositioningDeactivation */
    {0, 0, 0}, /* E_CIDMeasurementInitiation */
    {0, 0, 0}, /* E_CIDMeasurementFailureIndication */
    {0, 0, 0}, /* E_CIDMeasurementReport */
    {0, 0, 0}, /* E_CIDMeasurementTermination */
    {0, 0, 0}, /* PositioningInformationUpdate */
    {0, 0, 0}, /* ReferenceTimeInformationReport */
    {0, 0, 0}, /* ReferenceTimeInformationReportingControl */
};

const char *f1ap_direction2String(int f1ap_dir) {
  static const char *const f1ap_direction_String[] = {
      "", /* Nothing */
      "Initiating message", /* initiating message */
      "Successfull outcome", /* successfull outcome */
      "UnSuccessfull outcome", /* successfull outcome */
  };
  return(f1ap_direction_String[f1ap_dir]);
}

static F1AP_F1AP_PDU_t *f1ap_decode_pdu(const uint8_t *const buffer, uint32_t length)
{
  DevAssert(buffer != NULL);
  asn_codec_ctx_t st = {.max_stack_size = 100 * 1000};
  F1AP_F1AP_PDU_t *pdu = NULL;
  asn_dec_rval_t dec_ret = aper_decode(&st, &asn_DEF_F1AP_F1AP_PDU, (void **)&pdu, buffer, length, 0, 0);

  if (LOG_DEBUGFLAG(DEBUG_ASN1)) {
    LOG_E(F1AP, "----------------- ASN1 DECODER PRINT START----------------- \n");
    xer_fprint(stdout, &asn_DEF_F1AP_F1AP_PDU, pdu);
    LOG_E(F1AP, "----------------- ASN1 DECODER PRINT END ----------------- \n");
  }

  return dec_ret.code == RC_OK ? pdu : NULL;
}

int f1ap_handle_message(instance_t instance,
                        sctp_assoc_t assoc_id,
                        int32_t stream,
                        const uint8_t *const data,
                        const uint32_t data_length)
{
  DevAssert(data != NULL);

  F1AP_F1AP_PDU_t *pdu = f1ap_decode_pdu(data, data_length);
  if (pdu == NULL) {
    LOG_E(F1AP, "Failed to decode PDU\n");
    return -1;
  }

#ifdef E2_AGENT
  if (pdu->choice.initiatingMessage->procedureCode == F1AP_ProcedureCode_id_F1Setup) {
    if (pdu->present == F1AP_F1AP_PDU_PR_initiatingMessage)
      e2ap_store_setup_req(E2AP_SETUP_MSG_F1AP, data, data_length);
    else if (pdu->present == F1AP_F1AP_PDU_PR_successfulOutcome)
      e2ap_store_setup_resp(E2AP_SETUP_MSG_F1AP, data, data_length);
  }
#endif

  /* Checking procedure Code and direction of message */
  if (pdu->choice.initiatingMessage->procedureCode >= sizeof(f1ap_messages_processing) / (3 * sizeof(f1ap_message_processing_t))
      || (pdu->present > F1AP_F1AP_PDU_PR_unsuccessfulOutcome)) {
    LOG_E(F1AP,
          "[SCTP %d] Either procedureCode %ld or direction %d exceed expected\n",
          assoc_id,
          pdu->choice.initiatingMessage->procedureCode,
          pdu->present);
    ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
    return -1;
  }

  int ret;
  if (f1ap_messages_processing[pdu->choice.initiatingMessage->procedureCode][pdu->present - 1] == NULL) {
    // No handler present. This can mean not implemented or no procedure for eNB (wrong direction).
    LOG_E(F1AP,
          "[SCTP %d] No handler for procedureCode %ld in %s\n",
          assoc_id,
          pdu->choice.initiatingMessage->procedureCode,
          f1ap_direction2String(pdu->present - 1));
    ret=-1;
  } else {
    /* Calling the right handler */
    LOG_D(F1AP, "Calling handler with instance %ld\n",instance);
    ret = (*f1ap_messages_processing[pdu->choice.initiatingMessage->procedureCode][pdu->present - 1])(instance,
                                                                                                      assoc_id,
                                                                                                      stream,
                                                                                                      pdu);
  }

  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  return ret;
}
