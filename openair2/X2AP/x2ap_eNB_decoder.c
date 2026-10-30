/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief x2ap decoder procedures for eNB
 */

#include <stdio.h>

#include "assertions.h"
#include "intertask_interface.h"
#include "x2ap_common.h"
#include "x2ap_eNB_decoder.h"

static int x2ap_eNB_decode_initiating_message(X2AP_X2AP_PDU_t *pdu)
{
  DevAssert(pdu != NULL);

  switch(pdu->choice.initiatingMessage->procedureCode) {

    case X2AP_ProcedureCode_id_x2Setup:
      //asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_X2AP_X2AP_PDU, pdu);
      X2AP_INFO("x2ap_eNB_decode_initiating_message!\n");
      break;

    case X2AP_ProcedureCode_id_handoverPreparation:
      //asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_X2AP_X2AP_PDU, pdu);
      X2AP_INFO("x2ap_eNB_decode_initiating_message!\n");
      break;

    case X2AP_ProcedureCode_id_uEContextRelease:
      //asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_X2AP_X2AP_PDU, pdu);
      X2AP_INFO("x2ap_eNB_decode_initiating_message!\n");
      break;

    case X2AP_ProcedureCode_id_handoverCancel:
      //asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_X2AP_X2AP_PDU, pdu);
      X2AP_INFO("x2ap_eNB_decode_initiating_message!\n");
      break;

    case X2AP_ProcedureCode_id_endcX2Setup:
    	X2AP_INFO("X2AP_ProcedureCode_id_endcX2Setup message!\n");
    	break;
    case X2AP_ProcedureCode_id_sgNBAdditionPreparation:
    	X2AP_INFO("X2AP_ProcedureCode_id_sgNBAdditionPreparation message!\n");
    break;
    case X2AP_ProcedureCode_id_sgNBReconfigurationCompletion:
        	X2AP_INFO("X2AP_ProcedureCode_id_sgNBReconfigurationCompletion message!\n");
        break;

    case X2AP_ProcedureCode_id_meNBinitiatedSgNBRelease:
      X2AP_INFO("X2AP_ProcedureCode_id_meNBinitiatedSgNBRelease message!\n");
      break;

    case X2AP_ProcedureCode_id_sgNBinitiatedSgNBRelease:
      X2AP_INFO("X2AP_ProcedureCode_id_sgNBinitiatedSgNBRelease message!\n");
      break;

    default:
      X2AP_ERROR("Unknown procedure ID (%d) for initiating message\n",
                  (int)pdu->choice.initiatingMessage->procedureCode);
      AssertFatal( 0, "Unknown procedure ID (%d) for initiating message\n",
                   (int)pdu->choice.initiatingMessage->procedureCode);
      return -1;
  }

  return 0;
}

static int x2ap_eNB_decode_successful_outcome(X2AP_X2AP_PDU_t *pdu)
{
  DevAssert(pdu != NULL);

  switch(pdu->choice.successfulOutcome->procedureCode) {
    case X2AP_ProcedureCode_id_x2Setup:
      //asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_X2AP_X2AP_PDU, pdu);
      X2AP_INFO("x2ap_eNB_decode_successfuloutcome_message!\n");
      break;

    case X2AP_ProcedureCode_id_handoverPreparation:
      //asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_X2AP_X2AP_PDU, pdu);
      X2AP_INFO("x2ap_eNB_decode_successfuloutcome_message!\n");
      break;

    case X2AP_ProcedureCode_id_endcX2Setup:
    	X2AP_INFO("x2ap_eNB_decode_successfuloutcome_message!\n");
    	break;

    case X2AP_ProcedureCode_id_sgNBAdditionPreparation:
    	X2AP_INFO("x2ap_eNB_decode_successfuloutcome_message!\n");
    	break;

    case X2AP_ProcedureCode_id_meNBinitiatedSgNBRelease:
      X2AP_INFO("meNBinitiatedSgNBRelease successful outcome!\n");
      break;

    default:
      X2AP_ERROR("Unknown procedure ID (%d) for successfull outcome message\n",
                  (int)pdu->choice.successfulOutcome->procedureCode);
      return -1;
  }

  return 0;
}

static int x2ap_eNB_decode_unsuccessful_outcome(X2AP_X2AP_PDU_t *pdu)
{
  DevAssert(pdu != NULL);

  switch(pdu->choice.unsuccessfulOutcome->procedureCode) {
    case X2AP_ProcedureCode_id_x2Setup:
      //asn_encode_to_new_buffer(NULL, ATS_CANONICAL_XER, &asn_DEF_X2AP_X2AP_PDU, pdu);
      X2AP_INFO("x2ap_eNB_decode_unsuccessfuloutcome_message!\n");
      break;

    default:
       X2AP_ERROR("Unknown procedure ID (%d) for unsuccessfull outcome message\n",
                  (int)pdu->choice.unsuccessfulOutcome->procedureCode);
      return -1;
  }

  return 0;
}

int x2ap_eNB_decode_pdu(X2AP_X2AP_PDU_t *pdu, const uint8_t *const buffer, uint32_t length)
{
  asn_dec_rval_t dec_ret;

  DevAssert(buffer != NULL);

  dec_ret = aper_decode(NULL,
                        &asn_DEF_X2AP_X2AP_PDU,
                        (void **)&pdu,
                        buffer,
                        length,
                        0,
                        0);
  if (LOG_DEBUGFLAG(DEBUG_ASN1)) {
    xer_fprint(stdout, &asn_DEF_X2AP_X2AP_PDU, pdu);
  }

  if (dec_ret.code != RC_OK) {
    X2AP_ERROR("Failed to decode pdu\n");
    return -1;
  }

  switch(pdu->present) {
    case X2AP_X2AP_PDU_PR_initiatingMessage:
      return x2ap_eNB_decode_initiating_message(pdu);

    case X2AP_X2AP_PDU_PR_successfulOutcome:
      return x2ap_eNB_decode_successful_outcome(pdu);

    case X2AP_X2AP_PDU_PR_unsuccessfulOutcome:
      return x2ap_eNB_decode_unsuccessful_outcome(pdu);

    default:
      X2AP_DEBUG("Unknown presence (%d) or not implemented\n", (int)pdu->present);
      break;
  }


  return -1;
}
