/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "xnap_gNB_encoder.h"
#include "asn_application.h"
#include "asn_codecs.h"
#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "xer_encoder.h"

static int xnap_gNB_encode_initiating(XNAP_XnAP_PDU_t *pdu, uint8_t **buffer, uint32_t *len)
{
  static const long codes[] = {
    XNAP_ProcedureCode_id_xnSetup,
    XNAP_ProcedureCode_id_handoverPreparation,
    XNAP_ProcedureCode_id_handoverCancel,
    XNAP_ProcedureCode_id_handoverSuccess,
    XNAP_ProcedureCode_id_sNStatusTransfer,
    XNAP_ProcedureCode_id_uEContextRelease,
    XNAP_ProcedureCode_id_rANPaging,
  };
  const long code = pdu->choice.initiatingMessage->procedureCode;
  int i;
  for (i = 0; i < sizeofArray(codes); i++)
    if (code == codes[i])
      break;
  if (i == sizeofArray(codes)) {
    LOG_E(XNAP, "Unknown procedure code %ld for initiating message\n", code);
    return -1;
  }

  asn_encode_to_new_buffer_result_t res =
      asn_encode_to_new_buffer(NULL, ATS_ALIGNED_CANONICAL_PER, &asn_DEF_XNAP_XnAP_PDU, pdu);
  AssertFatal(res.result.encoded > 0, "Failed to encode XNAP initiating message\n");
  *buffer = res.buffer;
  *len = res.result.encoded;
  return 0;
}

static int xnap_gNB_encode_successful_outcome(XNAP_XnAP_PDU_t *pdu, uint8_t **buffer, uint32_t *len)
{
  static const long codes[] = {
    XNAP_ProcedureCode_id_xnSetup,
    XNAP_ProcedureCode_id_handoverPreparation,
  };
  const long code = pdu->choice.successfulOutcome->procedureCode;
  int i;
  for (i = 0; i < sizeofArray(codes); i++)
    if (code == codes[i])
      break;
  if (i == sizeofArray(codes)) {
    LOG_E(XNAP, "Unknown procedure code %ld for successful outcome\n", code);
    return -1;
  }

  asn_encode_to_new_buffer_result_t res =
      asn_encode_to_new_buffer(NULL, ATS_ALIGNED_CANONICAL_PER, &asn_DEF_XNAP_XnAP_PDU, pdu);
  AssertFatal(res.result.encoded > 0, "Failed to encode XNAP successful outcome\n");
  *buffer = res.buffer;
  *len = res.result.encoded;
  return 0;
}

static int xnap_gNB_encode_unsuccessful_outcome(XNAP_XnAP_PDU_t *pdu, uint8_t **buffer, uint32_t *len)
{
  static const long codes[] = {
    XNAP_ProcedureCode_id_xnSetup,
    XNAP_ProcedureCode_id_handoverPreparation,
  };
  const long code = pdu->choice.unsuccessfulOutcome->procedureCode;
  int i;
  for (i = 0; i < sizeofArray(codes); i++)
    if (code == codes[i])
      break;
  if (i == sizeofArray(codes)) {
    LOG_E(XNAP, "Unknown procedure code %ld for unsuccessful outcome\n", code);
    return -1;
  }

  asn_encode_to_new_buffer_result_t res =
      asn_encode_to_new_buffer(NULL, ATS_ALIGNED_CANONICAL_PER, &asn_DEF_XNAP_XnAP_PDU, pdu);
  AssertFatal(res.result.encoded > 0, "Failed to encode XNAP unsuccessful outcome\n");
  *buffer = res.buffer;
  *len = res.result.encoded;
  return 0;
}

int xnap_gNB_encode_pdu(XNAP_XnAP_PDU_t *pdu, uint8_t **buffer, uint32_t *len)
{
  DevAssert(pdu != NULL);
  DevAssert(buffer != NULL);
  DevAssert(len != NULL);

  if (LOG_DEBUGFLAG(DEBUG_ASN1))
    xer_fprint(stdout, &asn_DEF_XNAP_XnAP_PDU, pdu);

  switch (pdu->present) {
    case XNAP_XnAP_PDU_PR_initiatingMessage:
      return xnap_gNB_encode_initiating(pdu, buffer, len);
    case XNAP_XnAP_PDU_PR_successfulOutcome:
      return xnap_gNB_encode_successful_outcome(pdu, buffer, len);
    case XNAP_XnAP_PDU_PR_unsuccessfulOutcome:
      return xnap_gNB_encode_unsuccessful_outcome(pdu, buffer, len);
    default:
      LOG_E(XNAP, "Unknown PDU type %d\n", pdu->present);
      return -1;
  }
}
