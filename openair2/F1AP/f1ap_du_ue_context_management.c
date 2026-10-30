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
#include "F1AP_ProtocolIE-Container.h"

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
  /* 0. Message Type */
  F1AP_F1AP_PDU_t pdu = {0};
  pdu.present = F1AP_F1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu.choice.initiatingMessage, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_UEContextModificationRequired;
  tmp->criticality = F1AP_Criticality_reject;
  tmp->value.present = F1AP_InitiatingMessage__value_PR_UEContextModificationRequired;
  F1AP_UEContextModificationRequired_t *out = (tmp->value.choice.UEContextModificationRequired = calloc_or_fail(1, sizeof(*tmp->value.choice.UEContextModificationRequired)));

  /* mandatory GNB_CU_UE_F1AP_ID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_UEContextModificationRequiredIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_UEContextModificationRequiredIEs__value_PR_GNB_CU_UE_F1AP_ID;
  asn1cCallocOne(ie1->value.choice.GNB_CU_UE_F1AP_ID, required->gNB_CU_ue_id);

  /* mandatory: GNB_DU_UE_F1AP_ID */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_UEContextModificationRequiredIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_UEContextModificationRequiredIEs__value_PR_GNB_DU_UE_F1AP_ID;
  asn1cCallocOne(ie2->value.choice.GNB_DU_UE_F1AP_ID, required->gNB_DU_ue_id);

  /* optional: Resource Coordination Transfer Container */
  /* not implemented!*/

  /* optional: DU-to-CU RRC Container */
  if (required->du_to_cu_rrc_information) {
    asn1cSequenceAdd(out->protocolIEs->list, F1AP_UEContextModificationRequiredIEs_t, ie3);
    ie3->id = F1AP_ProtocolIE_ID_id_DUtoCURRCInformation;
    ie3->criticality = F1AP_Criticality_reject;
    ie3->value.present = F1AP_UEContextModificationRequiredIEs__value_PR_DUtoCURRCInformation;

    const du_to_cu_rrc_information_t *du2cu = required->du_to_cu_rrc_information;
    AssertFatal(du2cu->cellGroupConfig != NULL, "du2cu cellGroupConfig is mandatory!\n");

    /* mandatorycellGroupConfig */
    if (!ie3->value.choice.DUtoCURRCInformation)
      ie3->value.choice.DUtoCURRCInformation = calloc_or_fail(1, sizeof(*ie3->value.choice.DUtoCURRCInformation));
    OCTET_STRING_fromBuf(&ie3->value.choice.DUtoCURRCInformation->cellGroupConfig,
                         (const char *)du2cu->cellGroupConfig,
                         du2cu->cellGroupConfig_length);

    /* optional: measGapConfig */
    if (du2cu->measGapConfig != NULL) {
      OCTET_STRING_fromBuf(ie3->value.choice.DUtoCURRCInformation->measGapConfig,
                           (const char *)du2cu->measGapConfig,
                           du2cu->measGapConfig_length);
    }

    /* optional: requestedP_MaxFR1 */
    if (du2cu->requestedP_MaxFR1 != NULL) {
      OCTET_STRING_fromBuf(ie3->value.choice.DUtoCURRCInformation->requestedP_MaxFR1,
                           (const char *)du2cu->requestedP_MaxFR1,
                           du2cu->requestedP_MaxFR1_length);
    }
  }

  /* optional: DRB required to be modified */
  /* not implemented */

  /* optional: SRB required to be released */
  /* not implemented */

  /* optional: DRB required to be released */
  /* not implemented */

  /* mandatory: cause */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_UEContextModificationRequiredIEs_t, ie4);
  ie4->id = F1AP_ProtocolIE_ID_id_Cause;
  ie4->criticality = F1AP_Criticality_reject;
  ie4->value.present = F1AP_UEContextModificationRequiredIEs__value_PR_Cause;
  F1AP_Cause_t *cause = (ie4->value.choice.Cause = calloc_or_fail(1, sizeof(*ie4->value.choice.Cause)));
  switch (required->cause) {
    case F1AP_CAUSE_RADIO_NETWORK:
      cause->present = F1AP_Cause_PR_radioNetwork;
      cause->choice.radioNetwork = required->cause_value;
      break;
    case F1AP_CAUSE_TRANSPORT:
      cause->present = F1AP_Cause_PR_transport;
      cause->choice.transport = required->cause_value;
      break;
    case F1AP_CAUSE_PROTOCOL:
      cause->present = F1AP_Cause_PR_protocol;
      cause->choice.protocol = required->cause_value;
      break;
    case F1AP_CAUSE_MISC:
      cause->present = F1AP_Cause_PR_misc;
      cause->choice.misc = required->cause_value;
      break;
    case F1AP_CAUSE_NOTHING:
    default:
      cause->present = F1AP_Cause_PR_NOTHING;
      break;
  } // switch

  /* optional: BH RLC Channel Required to be Released List */
  /* not implemented */

  /* optional: SL DRB Required to Be Modified List */
  /* not implemented */

  /* optional: SL DRB Required to be Released List */
  /* not implemented */

  /* optional: Candidate Cells To Be Cancelled List */
  /* not implemented */

  /* encode */
  uint8_t *buffer = NULL;
  uint32_t len = 0;
  if (f1ap_encode_pdu(&pdu, &buffer, &len) < 0) {
    LOG_E(F1AP, "Failed to encode F1 context release request\n");
    return -1;
  }

  f1ap_itti_send_sctp_data_req(assoc_id, buffer, len);
  return 0;
}

int DU_handle_UE_CONTEXT_MODIFICATION_CONFIRM(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(instance);
  UNUSED(assoc_id);
  UNUSED(stream);
  F1AP_UEContextModificationConfirm_t *container = pdu->choice.successfulOutcome->value.choice.UEContextModificationConfirm;
  f1ap_ue_context_modif_confirm_t confirm = {0};
  F1AP_UEContextModificationConfirmIEs_t *ie = NULL;

  /* mandatory: GNB_CU_UE_F1AP_ID */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_UEContextModificationConfirmIEs_t, ie, container, F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID, true);
  confirm.gNB_CU_ue_id = *ie->value.choice.GNB_CU_UE_F1AP_ID;

  /* mandatory: GNB_DU_UE_F1AP_ID */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_UEContextModificationConfirmIEs_t, ie, container, F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID, true);
  confirm.gNB_DU_ue_id = *ie->value.choice.GNB_DU_UE_F1AP_ID;

  /* optional: Resource Coordination Transfer Container */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_UEContextModificationConfirmIEs_t,
                             ie,
                             container,
                             F1AP_ProtocolIE_ID_id_ResourceCoordinationTransferContainer,
                             false);
  AssertFatal(ie == NULL, "handling of Resource Coordination Transfer Container not implemented\n");

  /* optional: DRBS Modified List */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_UEContextModificationConfirmIEs_t,
                             ie,
                             container,
                             F1AP_ProtocolIE_ID_id_DRBs_Modified_List,
                             false);
  AssertFatal(ie == NULL, "handling of DRBs Modified List not implemented\n");

  /* optional: RRC Container */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_UEContextModificationConfirmIEs_t, ie, container, F1AP_ProtocolIE_ID_id_RRCContainer, false);
  if (ie != NULL) {
    F1AP_RRCContainer_t *rrc_container = ie->value.choice.RRCContainer;
    confirm.rrc_container = malloc(rrc_container->size);
    AssertFatal(confirm.rrc_container != NULL, "memory allocation failed\n");
    memcpy(confirm.rrc_container, rrc_container->buf, rrc_container->size);
    confirm.rrc_container_length = rrc_container->size;
  }

  /* optional: Criticality Diagnostics */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_UEContextModificationConfirmIEs_t,
                             ie,
                             container,
                             F1AP_ProtocolIE_ID_id_CriticalityDiagnostics,
                             false);
  AssertFatal(ie == NULL, "handling of DRBs Modified List not implemented\n");

  /* optional: Execute Duplication */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_UEContextModificationConfirmIEs_t,
                             ie,
                             container,
                             F1AP_ProtocolIE_ID_id_ExecuteDuplication,
                             false);
  AssertFatal(ie == NULL, "handling of DRBs Modified List not implemented\n");

  /* optional: Resource Coordination Transfer Information */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_UEContextModificationConfirmIEs_t,
                             ie,
                             container,
                             F1AP_ProtocolIE_ID_id_ResourceCoordinationTransferInformation,
                             false);
  AssertFatal(ie == NULL, "handling of DRBs Modified List not implemented\n");

  /* optional: SL DRB Modified List */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_UEContextModificationConfirmIEs_t,
                             ie,
                             container,
                             F1AP_ProtocolIE_ID_id_SLDRBs_Modified_List,
                             false);
  AssertFatal(ie == NULL, "handling of DRBs Modified List not implemented\n");

  ue_context_modification_confirm(&confirm);
  return 0;
}

int DU_handle_UE_CONTEXT_MODIFICATION_REFUSE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu)
{
  UNUSED(instance);
  UNUSED(assoc_id);
  UNUSED(stream);
  F1AP_UEContextModificationRefuse_t *container = pdu->choice.unsuccessfulOutcome->value.choice.UEContextModificationRefuse;
  f1ap_ue_context_modif_refuse_t refuse = {0};
  F1AP_UEContextModificationRefuseIEs_t *ie = NULL;

  /* mandatory: GNB_CU_UE_F1AP_ID */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_UEContextModificationRefuseIEs_t, ie, container, F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID, true);
  refuse.gNB_CU_ue_id = *ie->value.choice.GNB_CU_UE_F1AP_ID;

  /* mandatory: GNB_DU_UE_F1AP_ID */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_UEContextModificationRefuseIEs_t, ie, container, F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID, true);
  refuse.gNB_DU_ue_id = *ie->value.choice.GNB_DU_UE_F1AP_ID;

  /* mandatory: Cause */
  F1AP_FIND_PROTOCOLIE_BY_ID(F1AP_UEContextModificationRefuseIEs_t, ie, container, F1AP_ProtocolIE_ID_id_Cause, true);
  switch (ie->value.choice.Cause->present) {
    case F1AP_Cause_PR_radioNetwork:
      refuse.cause = F1AP_CAUSE_RADIO_NETWORK;
      refuse.cause_value = ie->value.choice.Cause->choice.radioNetwork;
      break;
    case F1AP_Cause_PR_transport:
      refuse.cause = F1AP_CAUSE_TRANSPORT;
      refuse.cause_value = ie->value.choice.Cause->choice.transport;
      break;
    case F1AP_Cause_PR_protocol:
      refuse.cause = F1AP_CAUSE_PROTOCOL;
      refuse.cause_value = ie->value.choice.Cause->choice.protocol;
      break;
    case F1AP_Cause_PR_misc:
      refuse.cause = F1AP_CAUSE_MISC;
      refuse.cause_value = ie->value.choice.Cause->choice.misc;
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
