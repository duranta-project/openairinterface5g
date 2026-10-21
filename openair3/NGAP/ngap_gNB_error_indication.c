/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "INTEGER.h"
#include "assertions.h"
#include "common/utils/utils.h"
#include "conversions.h"
#include "ngap_common.h"
#include "ngap_gNB_error_indication.h"
#include "ngap_msg_includes.h"
#include "oai_asn1.h"

/** @brief Encode NGAP Error Indication (TS 38.413 clause 8.7.5)
 * @param msg Pointer to Error Indication fields to encode
 * @return Heap-allocated NGAP PDU */
NGAP_NGAP_PDU_t *encode_ng_error_indication(const ngap_error_indication_t *msg)
{
  DevAssert(msg != NULL);
  /* 8.7.5.2: at least Cause or Criticality Diagnostics */
  DevAssert(msg->has_cause || msg->has_criticality_diagnostics);
  /* 8.7.5.2: UE-associated signalling includes both UE NGAP IDs */
  DevAssert(msg->has_amf_ue_ngap_id == msg->has_ran_ue_ngap_id);

  NGAP_NGAP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));
  pdu->present = NGAP_NGAP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, head);
  head->procedureCode = NGAP_ProcedureCode_id_ErrorIndication;
  head->criticality = NGAP_Criticality_ignore;
  head->value.present = NGAP_InitiatingMessage__value_PR_ErrorIndication;
  NGAP_ErrorIndication_t *out = &head->value.choice.ErrorIndication;

  /* AMF-UE-NGAP-ID (optional) */
  if (msg->has_amf_ue_ngap_id) {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_ErrorIndicationIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_AMF_UE_NGAP_ID;
    ie->criticality = NGAP_Criticality_ignore;
    ie->value.present = NGAP_ErrorIndicationIEs__value_PR_AMF_UE_NGAP_ID;
    asn_uint642INTEGER(&ie->value.choice.AMF_UE_NGAP_ID, msg->amf_ue_ngap_id);
  }

  /* RAN-UE-NGAP-ID (optional) */
  if (msg->has_ran_ue_ngap_id) {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_ErrorIndicationIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_RAN_UE_NGAP_ID;
    ie->criticality = NGAP_Criticality_ignore;
    ie->value.present = NGAP_ErrorIndicationIEs__value_PR_RAN_UE_NGAP_ID;
    ie->value.choice.RAN_UE_NGAP_ID = msg->ran_ue_ngap_id;
  }

  /* Cause (optional) */
  if (msg->has_cause) {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_ErrorIndicationIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_Cause;
    ie->criticality = NGAP_Criticality_ignore;
    ie->value.present = NGAP_ErrorIndicationIEs__value_PR_Cause;
    encode_ngap_cause(&ie->value.choice.Cause, &msg->cause);
  }

  /* Criticality Diagnostics (optional) */
  if (msg->has_criticality_diagnostics) {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_ErrorIndicationIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_CriticalityDiagnostics;
    ie->criticality = NGAP_Criticality_ignore;
    ie->value.present = NGAP_ErrorIndicationIEs__value_PR_CriticalityDiagnostics;
  }

  return pdu;
}

/** @brief Decode NGAP Error Indication (TS 38.413 clause 8.7.5)
 * @param out Filled on success
 * @param pdu NGAP PDU containing Error Indication
 * @return true on success, false on failure */
bool decode_ng_error_indication(ngap_error_indication_t *out, const NGAP_NGAP_PDU_t *pdu)
{
  DevAssert(out != NULL);
  DevAssert(pdu != NULL);

  if (pdu->present != NGAP_NGAP_PDU_PR_initiatingMessage) {
    NGAP_ERROR("Invalid PDU present value for Error Indication\n");
    return false;
  }
  if (pdu->choice.initiatingMessage->procedureCode != NGAP_ProcedureCode_id_ErrorIndication) {
    NGAP_ERROR("Unexpected procedureCode for Error Indication\n");
    return false;
  }

  memset(out, 0, sizeof(*out));
  const NGAP_ErrorIndication_t *container = &pdu->choice.initiatingMessage->value.choice.ErrorIndication;
  NGAP_ErrorIndicationIEs_t *ie = NULL;

  /* AMF-UE-NGAP-ID (optional) */
  NGAP_FIND_PROTOCOLIE_BY_ID(NGAP_ErrorIndicationIEs_t, ie, container, NGAP_ProtocolIE_ID_id_AMF_UE_NGAP_ID, false);
  if (ie != NULL) {
    out->has_amf_ue_ngap_id = true;
    asn_INTEGER2ulong(&ie->value.choice.AMF_UE_NGAP_ID, &out->amf_ue_ngap_id);
  }

  /* RAN-UE-NGAP-ID (optional) */
  NGAP_FIND_PROTOCOLIE_BY_ID(NGAP_ErrorIndicationIEs_t, ie, container, NGAP_ProtocolIE_ID_id_RAN_UE_NGAP_ID, false);
  if (ie != NULL) {
    out->has_ran_ue_ngap_id = true;
    out->ran_ue_ngap_id = ie->value.choice.RAN_UE_NGAP_ID;
  }

  /* Cause (optional) */
  NGAP_FIND_PROTOCOLIE_BY_ID(NGAP_ErrorIndicationIEs_t, ie, container, NGAP_ProtocolIE_ID_id_Cause, false);
  if (ie != NULL) {
    out->has_cause = true;
    out->cause = decode_ngap_cause(&ie->value.choice.Cause);
  }

  /* Criticality Diagnostics (optional) */
  NGAP_FIND_PROTOCOLIE_BY_ID(NGAP_ErrorIndicationIEs_t, ie, container, NGAP_ProtocolIE_ID_id_CriticalityDiagnostics, false);
  if (ie != NULL) {
    out->has_criticality_diagnostics = true;
  }

  /* 8.7.5.2: at least Cause or Criticality Diagnostics */
  if (!out->has_cause && !out->has_criticality_diagnostics) {
    NGAP_ERROR("Error Indication missing Cause and Criticality Diagnostics\n");
    return false;
  }
  return true;
}

/** @brief Compare two Error Indication structs */
bool eq_ng_error_indication(const ngap_error_indication_t *a, const ngap_error_indication_t *b)
{
  if (a == NULL || b == NULL) {
    return a == b;
  }

  _EQ_CHECK_INT(a->has_amf_ue_ngap_id, b->has_amf_ue_ngap_id);
  if (a->has_amf_ue_ngap_id) {
    _EQ_CHECK_LONG((long)a->amf_ue_ngap_id, (long)b->amf_ue_ngap_id);
  }
  _EQ_CHECK_INT(a->has_ran_ue_ngap_id, b->has_ran_ue_ngap_id);
  if (a->has_ran_ue_ngap_id) {
    _EQ_CHECK_UINT32(a->ran_ue_ngap_id, b->ran_ue_ngap_id);
  }
  _EQ_CHECK_INT(a->has_cause, b->has_cause);
  if (a->has_cause) {
    _EQ_CHECK_INT(a->cause.type, b->cause.type);
    _EQ_CHECK_INT(a->cause.value, b->cause.value);
  }
  _EQ_CHECK_INT(a->has_criticality_diagnostics, b->has_criticality_diagnostics);
  return true;
}
