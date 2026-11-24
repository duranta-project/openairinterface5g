/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "f1ap_common.h"
#include "f1ap_encoder.h"
#include "f1ap_itti_messaging.h"
#include "f1ap_du_ue_context_management.h"
#include "lib/f1ap_ue_context.h"
#include "openair2/LAYER2/NR_MAC_gNB/mac_rrc_dl_handler.h"

#include "openair2/LAYER2/NR_MAC_gNB/nr_mac_gNB.h"
#include "openair2/LAYER2/nr_pdcp/nr_pdcp_oai_api.h"

#include "F1AP_F1AP-PDU.h"
#include "F1AP_InitiatingMessage.h"
#include "F1AP_SuccessfulOutcome.h"
#include "F1AP_UnsuccessfulOutcome.h"
#include "F1AP_ProtocolIE-Field.h"

int DU_handle_UE_CONTEXT_SETUP_REQUEST(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(instance);
  UNUSED(assoc_id);
  UNUSED(stream);
  f1ap_ue_context_setup_req_t req = {0};
  if (!decode_ue_context_setup_req(pdu, &req)) {
    LOG_E(F1AP, "cannot decode F1 UE Context Setup Request\n");
    free_ue_context_setup_req(&req);
    return -1;
  }

  ue_context_setup_request(&req);
  free_ue_context_setup_req(&req);

  return 0;
}

int DU_send_UE_CONTEXT_SETUP_RESPONSE(sctp_assoc_t assoc_id, f1ap_ue_context_setup_resp_t *resp)
{
  F1AP_F1AP_PDU_t *pdu = encode_ue_context_setup_resp(resp);

  uint8_t *buffer = NULL;
  uint32_t len = 0;
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 UE CONTEXT SETUP RESPONSE\n");
    return -1;
  }

  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  return 0;
}

int DU_send_UE_CONTEXT_SETUP_FAILURE(sctp_assoc_t assoc_id)
{
  UNUSED(assoc_id);
  AssertFatal(1==0,"Not implemented yet\n");
}

int DU_send_UE_CONTEXT_RELEASE_REQUEST(sctp_assoc_t assoc_id, f1ap_ue_context_rel_req_t *req)
{
  F1AP_F1AP_PDU_t *pdu = encode_ue_context_rel_req(req);

  uint8_t *buffer = NULL;
  uint32_t len = 0;
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 context release request\n");
    return -1;
  }

  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  return 0;
}

int DU_handle_UE_CONTEXT_RELEASE_COMMAND(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(instance);
  UNUSED(assoc_id);
  UNUSED(stream);
  f1ap_ue_context_rel_cmd_t cmd = {0};
  if (!decode_ue_context_rel_cmd(pdu, &cmd)) {
    LOG_E(F1AP, "cannot decode F1 UE Context Release Command\n");
    free_ue_context_rel_cmd(&cmd);
    return -1;
  }

  ue_context_release_command(&cmd);
  free_ue_context_rel_cmd(&cmd);
  return 0;
}

int DU_send_UE_CONTEXT_RELEASE_COMPLETE(sctp_assoc_t assoc_id, f1ap_ue_context_rel_cplt_t *cplt)
{
  F1AP_F1AP_PDU_t *pdu = encode_ue_context_rel_cplt(cplt);

  uint8_t  *buffer;
  uint32_t  len;
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 context release complete\n");
    return -1;
  }

  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  return 0;
}

int DU_handle_UE_CONTEXT_MODIFICATION_REQUEST(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(instance);
  UNUSED(assoc_id);
  UNUSED(stream);
  f1ap_ue_context_mod_req_t req = {0};
  if (!decode_ue_context_mod_req(pdu, &req)) {
    LOG_E(F1AP, "cannot decode F1 UE Context Modification Request\n");
    free_ue_context_mod_req(&req);
    return -1;
  }

  ue_context_modification_request(&req);
  free_ue_context_mod_req(&req);

  return 0;
}

int DU_send_UE_CONTEXT_MODIFICATION_RESPONSE(sctp_assoc_t assoc_id, f1ap_ue_context_mod_resp_t *resp)
{
  F1AP_F1AP_PDU_t *pdu = encode_ue_context_mod_resp(resp);

  uint8_t *buffer = NULL;
  uint32_t len = 0;
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 UE CONTEXT SETUP RESPONSE\n");
    return -1;
  }
  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  return 0;
}

int DU_send_UE_CONTEXT_MODIFICATION_FAILURE(sctp_assoc_t assoc_id)
{
  UNUSED(assoc_id);
  AssertFatal(1==0,"Not implemented yet\n");
}

int DU_send_UE_CONTEXT_MODIFICATION_REQUIRED(sctp_assoc_t assoc_id, f1ap_ue_context_modif_required_t *required)
{
  F1AP_F1AP_PDU_t *pdu = encode_ue_context_modif_required(required);

  uint8_t *buffer = NULL;
  uint32_t len = 0;
  if (f1ap_encode_pdu(pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 UE Context Modification Required\n");
    return -1;
  }

  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
  return 0;
}

int DU_handle_UE_CONTEXT_MODIFICATION_CONFIRM(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(instance);
  UNUSED(assoc_id);
  UNUSED(stream);
  f1ap_ue_context_modif_confirm_t confirm = {0};
  if (!decode_ue_context_modif_confirm(pdu, &confirm)) {
    LOG_E(F1AP, "cannot decode F1 UE Context Modification Confirm\n");
    free_ue_context_modif_confirm(&confirm);
    return -1;
  }

  ue_context_modification_confirm(&confirm);
  free_ue_context_modif_confirm(&confirm);
  return 0;
}

int DU_handle_UE_CONTEXT_MODIFICATION_REFUSE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(instance);
  UNUSED(assoc_id);
  UNUSED(stream);
  F1AP_UEContextModificationRefuse_t *container = &pdu->choice.unsuccessfulOutcome->value.choice.UEContextModificationRefuse;
  f1ap_ue_context_modif_refuse_t refuse = {0};
  F1AP_UEContextModificationRefuseIEs_t *ie = NULL;

  /* mandatory: GNB_CU_UE_F1AP_ID */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_UEContextModificationRefuseIEs_t, ie, container, F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID, true);
  refuse.gNB_CU_ue_id = ie->value.choice.GNB_CU_UE_F1AP_ID;

  /* mandatory: GNB_DU_UE_F1AP_ID */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_UEContextModificationRefuseIEs_t, ie, container, F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID, true);
  refuse.gNB_DU_ue_id = ie->value.choice.GNB_DU_UE_F1AP_ID;

  /* mandatory: Cause */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_UEContextModificationRefuseIEs_t, ie, container, F1AP_ProtocolIE_ID_id_Cause, true);
  switch (ie->value.choice.Cause.present) {
    case F1AP_Cause_PR_radioNetwork:
      refuse.cause = F1AP_CAUSE_RADIO_NETWORK;
      refuse.cause_value = ie->value.choice.Cause.choice.radioNetwork;
      break;
    case F1AP_Cause_PR_transport:
      refuse.cause = F1AP_CAUSE_TRANSPORT;
      refuse.cause_value = ie->value.choice.Cause.choice.transport;
      break;
    case F1AP_Cause_PR_protocol:
      refuse.cause = F1AP_CAUSE_PROTOCOL;
      refuse.cause_value = ie->value.choice.Cause.choice.protocol;
      break;
    case F1AP_Cause_PR_misc:
      refuse.cause = F1AP_CAUSE_MISC;
      refuse.cause_value = ie->value.choice.Cause.choice.misc;
      break;
    default:
      LOG_W(F1AP, "Unknown cause for UE Context Modification Refuse message\n");
      /* fall through */
    case F1AP_Cause_PR_NOTHING:
      refuse.cause = F1AP_CAUSE_NOTHING;
      break;
  }

  /* optional: Criticality Diagnostics */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_UEContextModificationRefuseIEs_t,
                             ie,
                             container,
                             F1AP_ProtocolIE_ID_id_CriticalityDiagnostics,
                             false);
  AssertFatal(ie == NULL, "handling of DRBs Modified List not implemented\n");

  ue_context_modification_refuse(&refuse);
  return 0;
}
