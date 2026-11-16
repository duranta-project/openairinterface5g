/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief ngap pdu decode procedures for gNB
 */
#include "ngap_gNB_decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include "ngap_msg_includes.h"
#include "T.h"
#include "aper_decoder.h"
#include "asn_application.h"
#include "asn_codecs.h"
#include "assertions.h"
#include "common/utils/T/T.h"
#include "ngap_common.h"

static int ngap_gNB_decode_initiating_message(NGAP_NGAP_PDU_t *pdu) {
  asn_encode_to_new_buffer_result_t res = { NULL, {0, NULL, NULL} };
  DevAssert(pdu != NULL);

  switch(pdu->choice.initiatingMessage->procedureCode) {
    case NGAP_ProcedureCode_id_DownlinkNASTransport:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      break;

    case NGAP_ProcedureCode_id_InitialContextSetup:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      break;

    case NGAP_ProcedureCode_id_UEContextRelease:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      break;

    case NGAP_ProcedureCode_id_Paging:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      NGAP_INFO("Paging initiating message\n");
      free(res.buffer);
      break;

    case NGAP_ProcedureCode_id_PDUSessionResourceSetup:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      NGAP_INFO("PDUSESSIONSetup initiating message\n");
      break;

    case NGAP_ProcedureCode_id_HandoverPreparation:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      NGAP_INFO("Handover Preparation initiating message\n");
      break;

    case NGAP_ProcedureCode_id_HandoverCancel:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      NGAP_INFO("Handover Cancel initiating message\n");
      break;

    case NGAP_ProcedureCode_id_PDUSessionResourceModify:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      NGAP_INFO("PDUSESSIONModify initiating message\n");
      break;

    case NGAP_ProcedureCode_id_PDUSessionResourceRelease:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      NGAP_INFO("PDUSESSIONRelease initiating message\n");
      break;

    case NGAP_ProcedureCode_id_ErrorIndication:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      NGAP_INFO("TODO ErrorIndication initiating message\n");
      break;

    case NGAP_ProcedureCode_id_HandoverResourceAllocation:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      NGAP_INFO("Handover Resource Allocation initiating message\n");
      break;

    case NGAP_ProcedureCode_id_DownlinkRANStatusTransfer:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      NGAP_INFO("DL RAN Status Transfer initiating message\n");
      break;

    case NGAP_ProcedureCode_id_DownlinkUEAssociatedNRPPaTransport:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      NGAP_INFO("DL UE Associated NRPPA Transport initiating message\n");
      break;

    case NGAP_ProcedureCode_id_DownlinkNonUEAssociatedNRPPaTransport:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      NGAP_INFO("DL NON UE Associated NRPPA Transport initiating message\n");
      break;

    case NGAP_ProcedureCode_id_BroadcastSessionSetup:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      NGAP_INFO("Broadcast Session Setup initiating message\n");
      /* Decoded here; the handler that acts on it arrives with the broadcast
       * session setup commit. */
      break;

    default:
      /* Unknown or wrong-direction initiating procedure: fail closed without
       * aborting the process (TS 38.413 §10.3.4.1). TODO: Send Error Indication */
      NGAP_ERROR("Unknown procedure ID (%d) for initiating message\n",
                 (int)pdu->choice.initiatingMessage->procedureCode);
      return -1;
  }

  return 0;
}

static int ngap_gNB_decode_successful_outcome(NGAP_NGAP_PDU_t *pdu) {
  asn_encode_to_new_buffer_result_t res = { NULL, {0, NULL, NULL} };
  DevAssert(pdu != NULL);

  switch(pdu->choice.successfulOutcome->procedureCode) {
    case NGAP_ProcedureCode_id_NGSetup:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      break;

    case NGAP_ProcedureCode_id_PathSwitchRequest:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      break;

    case NGAP_ProcedureCode_id_PDUSessionResourceModifyIndication:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      break;

    case NGAP_ProcedureCode_id_HandoverPreparation:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      break;

    default:
      NGAP_ERROR("Unknown procedure ID (%d) for successfull outcome message\n",
                 (int)pdu->choice.successfulOutcome->procedureCode);
      return -1;
  }

  return 0;
}

static int ngap_gNB_decode_unsuccessful_outcome(NGAP_NGAP_PDU_t *pdu) {
  asn_encode_to_new_buffer_result_t res = { NULL, {0, NULL, NULL} };
  DevAssert(pdu != NULL);

  switch(pdu->choice.unsuccessfulOutcome->procedureCode) {
    case NGAP_ProcedureCode_id_NGSetup:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      break;
   case NGAP_ProcedureCode_id_PathSwitchRequest:
      res = asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_NGAP_NGAP_PDU, pdu);
      free(res.buffer);
      break;

    default:
      NGAP_ERROR("Unknown procedure ID (%d) for unsuccessfull outcome message\n",
                 (int)pdu->choice.unsuccessfulOutcome->procedureCode);
      return -1;
  }

  return 0;
}

int ngap_gNB_decode_pdu(NGAP_NGAP_PDU_t *pdu, const uint8_t *const buffer,
                        const uint32_t length) {
  asn_dec_rval_t dec_ret;
  DevAssert(pdu != NULL);
  DevAssert(buffer != NULL);
  asn_codec_ctx_t st = {.max_stack_size = 100 * 1000}; // if we enable asn1c debug the stack size become large
  dec_ret = aper_decode(&st, &asn_DEF_NGAP_NGAP_PDU, (void **)&pdu, buffer, length, 0, 0);

  if (dec_ret.code != RC_OK) {
    NGAP_ERROR("Failed to decode pdu\n");
    return -1;
  }

  switch(pdu->present) {
    case NGAP_NGAP_PDU_PR_initiatingMessage:
      return ngap_gNB_decode_initiating_message(pdu);

    case NGAP_NGAP_PDU_PR_successfulOutcome:
      return ngap_gNB_decode_successful_outcome(pdu);

    case NGAP_NGAP_PDU_PR_unsuccessfulOutcome:
      return ngap_gNB_decode_unsuccessful_outcome(pdu);

    default:
      NGAP_DEBUG("Unknown presence (%d) or not implemented\n", (int)pdu->present);
      break;
  }

  return -1;
}
