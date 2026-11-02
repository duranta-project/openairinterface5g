/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief x2ap procedures for both eNB and MME
 */

#include <stdint.h>

#include "x2ap_common.h"
#include "X2AP_X2AP-PDU.h"

ssize_t x2ap_generate_initiating_message(
  uint8_t               **buffer,
  uint32_t               *length,
  X2AP_ProcedureCode_t    procedureCode,
  X2AP_Criticality_t      criticality,
  asn_TYPE_descriptor_t  *td,
  void                   *sptr)
{
  X2AP_X2AP_PDU_t pdu;
  ssize_t    encoded;
  memset(&pdu, 0, sizeof(X2AP_X2AP_PDU_t));
  pdu.present = X2AP_X2AP_PDU_PR_initiatingMessage;
  if (!pdu.choice.initiatingMessage)
    pdu.choice.initiatingMessage = calloc_or_fail(1, sizeof(*pdu.choice.initiatingMessage));
  pdu.choice.initiatingMessage->procedureCode = procedureCode;
  pdu.choice.initiatingMessage->criticality   = criticality;
  ANY_fromType_aper((ANY_t *)&pdu.choice.initiatingMessage->value, td, sptr);

  if (LOG_DEBUGFLAG(DEBUG_ASN1)) {
    xer_fprint(stdout, &asn_DEF_X2AP_X2AP_PDU, (void *)&pdu);
  }

  /* We can safely free list of IE from sptr */
  ASN_STRUCT_FREE_CONTENTS_ONLY(*td, sptr);

  if ((encoded = aper_encode_to_new_buffer(&asn_DEF_X2AP_X2AP_PDU, 0, &pdu,
                 (void **)buffer)) < 0) {
    return -1;
  }

  *length = encoded;
  return encoded;
}

ssize_t x2ap_generate_successfull_outcome(
  uint8_t               **buffer,
  uint32_t               *length,
  X2AP_ProcedureCode_t         procedureCode,
  X2AP_Criticality_t           criticality,
  asn_TYPE_descriptor_t  *td,
  void                   *sptr)
{
  X2AP_X2AP_PDU_t pdu;
  ssize_t    encoded;
  memset(&pdu, 0, sizeof(X2AP_X2AP_PDU_t));
  pdu.present = X2AP_X2AP_PDU_PR_successfulOutcome;
  if (!pdu.choice.successfulOutcome)
    pdu.choice.successfulOutcome = calloc_or_fail(1, sizeof(*pdu.choice.successfulOutcome));
  pdu.choice.successfulOutcome->procedureCode = procedureCode;
  pdu.choice.successfulOutcome->criticality   = criticality;
  ANY_fromType_aper((ANY_t *)&pdu.choice.successfulOutcome->value, td, sptr);

  if (LOG_DEBUGFLAG(DEBUG_ASN1)) {
    xer_fprint(stdout, &asn_DEF_X2AP_X2AP_PDU, (void *)&pdu);
  }

  /* We can safely free list of IE from sptr */
  ASN_STRUCT_FREE_CONTENTS_ONLY(*td, sptr);

  if ((encoded = aper_encode_to_new_buffer(&asn_DEF_X2AP_X2AP_PDU, 0, &pdu,
                 (void **)buffer)) < 0) {
    return -1;
  }

  *length = encoded;
  return encoded;
}

ssize_t x2ap_generate_unsuccessfull_outcome(
  uint8_t               **buffer,
  uint32_t               *length,
  X2AP_ProcedureCode_t         procedureCode,
  X2AP_Criticality_t           criticality,
  asn_TYPE_descriptor_t  *td,
  void                   *sptr)
{
  X2AP_X2AP_PDU_t pdu;
  ssize_t    encoded;
  memset(&pdu, 0, sizeof(X2AP_X2AP_PDU_t));
  pdu.present = X2AP_X2AP_PDU_PR_unsuccessfulOutcome;
  if (!pdu.choice.successfulOutcome)
    pdu.choice.successfulOutcome = calloc_or_fail(1, sizeof(*pdu.choice.successfulOutcome));
  pdu.choice.successfulOutcome->procedureCode = procedureCode;
  pdu.choice.successfulOutcome->criticality   = criticality;
  ANY_fromType_aper((ANY_t *)&pdu.choice.successfulOutcome->value, td, sptr);

  if (LOG_DEBUGFLAG(DEBUG_ASN1)) {
    xer_fprint(stdout, &asn_DEF_X2AP_X2AP_PDU, (void *)&pdu);
  }

  /* We can safely free list of IE from sptr */
  ASN_STRUCT_FREE_CONTENTS_ONLY(*td, sptr);

  if ((encoded = aper_encode_to_new_buffer(&asn_DEF_X2AP_X2AP_PDU, 0, &pdu,
                 (void **)buffer)) < 0) {
    return -1;
  }

  *length = encoded;
  return encoded;
}

void x2ap_handle_criticality(X2AP_Criticality_t criticality)
{
}
