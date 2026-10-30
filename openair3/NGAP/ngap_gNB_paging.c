/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <string.h>
#include "assertions.h"
#include "common/utils/utils.h"
#include "common/utils/ds/byte_array.h"
#include "conversions.h"
#include "ngap_common.h"
#include "ngap_gNB_paging.h"
#include "ngap_msg_includes.h"
#include "oai_asn1.h"
#include "NGAP_ProtocolIE-Container.h"

/* Paging Attempt range (TS 38.413 §9.3.1.72) */
#define NGAP_PAGING_ATTEMPT_MIN 1
#define NGAP_PAGING_ATTEMPT_MAX 16

/** @brief Return text label at index in text_info array */
static const char *text_info_at(int index, const text_info_t *info_array, size_t array_size)
{
  DevAssert(index >= 0 && index < array_size);
  return info_array[index].text;
}

/** @brief Encode NGAP Paging message (9.2.4.1 of 3GPP TS 38.413)
 * @param msg Pointer to paging indication structure to encode
 * @return Pointer to allocated NGAP_NGAP_PDU_t */
NGAP_NGAP_PDU_t *encode_ng_paging(const ngap_paging_ind_t *msg)
{
  DevAssert(msg != NULL);

  NGAP_NGAP_PDU_t *pdu = calloc_or_fail(1, sizeof(NGAP_NGAP_PDU_t));

  pdu->present = NGAP_NGAP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, head);
  head->procedureCode = NGAP_ProcedureCode_id_Paging;
  head->criticality = NGAP_Criticality_ignore;
  head->value.present = NGAP_InitiatingMessage__value_PR_Paging;
  NGAP_Paging_t *container = (head->value.choice.Paging = calloc_or_fail(1, sizeof(*head->value.choice.Paging)));

  // UE Paging Identity (M)
  if (!container->protocolIEs)
    container->protocolIEs = calloc_or_fail(1, sizeof(*container->protocolIEs));
  asn1cSequenceAdd(container->protocolIEs->list, NGAP_PagingIEs_t, ie);
  ie->id = NGAP_ProtocolIE_ID_id_UEPagingIdentity;
  ie->criticality = NGAP_Criticality_reject;
  ie->value.present = NGAP_PagingIEs__value_PR_UEPagingIdentity;
  if (!ie->value.choice.UEPagingIdentity)
    ie->value.choice.UEPagingIdentity = calloc_or_fail(1, sizeof(*ie->value.choice.UEPagingIdentity));
  ie->value.choice.UEPagingIdentity->present = NGAP_UEPagingIdentity_PR_fiveG_S_TMSI;
  asn1cCalloc(ie->value.choice.UEPagingIdentity->choice.fiveG_S_TMSI, fiveg_s_tmsi);
  const fiveg_s_tmsi_t *s_tmsi = &msg->ue_paging_identity.s_tmsi;
  AMF_SETID_TO_BIT_STRING(s_tmsi->amf_set_id, &fiveg_s_tmsi->aMFSetID);
  AMF_POINTER_TO_BIT_STRING(s_tmsi->amf_pointer, &fiveg_s_tmsi->aMFPointer);
  M_TMSI_TO_OCTET_STRING(s_tmsi->m_tmsi, &fiveg_s_tmsi->fiveG_TMSI);

  // TAI List for Paging (M)
  asn1cSequenceAdd(container->protocolIEs->list, NGAP_PagingIEs_t, ie1);
  ie1->id = NGAP_ProtocolIE_ID_id_TAIListForPaging;
  ie1->criticality = NGAP_Criticality_reject;
  ie1->value.present = NGAP_PagingIEs__value_PR_TAIListForPaging;

  for (int i = 0; i < msg->n_tai; i++) {
    if (!ie1->value.choice.TAIListForPaging)
      ie1->value.choice.TAIListForPaging = calloc_or_fail(1, sizeof(*ie1->value.choice.TAIListForPaging));
    asn1cSequenceAdd(ie1->value.choice.TAIListForPaging->list, NGAP_TAIListForPagingItem_t, item);
    NGAP_TAI_t *tai = (item->tAI = calloc_or_fail(1, sizeof(*item->tAI)));
    const plmn_id_t *plmn = &msg->tai_list[i].plmn;
    MCC_MNC_TO_TBCD(plmn->mcc, plmn->mnc, plmn->mnc_digit_length, &tai->pLMNIdentity);
    INT24_TO_OCTET_STRING(msg->tai_list[i].tac, &tai->tAC);
  }

  // Paging DRX (O)
  if (msg->paging_drx != NULL) {
    asn1cSequenceAdd(container->protocolIEs->list, NGAP_PagingIEs_t, ie2);
    ie2->id = NGAP_ProtocolIE_ID_id_PagingDRX;
    ie2->criticality = NGAP_Criticality_ignore;
    ie2->value.present = NGAP_PagingIEs__value_PR_PagingDRX;
    asn1cCallocOne(ie2->value.choice.PagingDRX, *msg->paging_drx);
  }

  // Paging Priority (O)
  if (msg->paging_priority != NULL) {
    asn1cSequenceAdd(container->protocolIEs->list, NGAP_PagingIEs_t, ie3);
    ie3->id = NGAP_ProtocolIE_ID_id_PagingPriority;
    ie3->criticality = NGAP_Criticality_ignore;
    ie3->value.present = NGAP_PagingIEs__value_PR_PagingPriority;
    asn1cCallocOne(ie3->value.choice.PagingPriority, *msg->paging_priority);
  }

  // UERadioCapabilityForPaging (O)
  if (msg->ue_radio_capability != NULL && msg->ue_radio_capability->buf != NULL) {
    const byte_array_t *ue_radio_cap_ba = msg->ue_radio_capability;
    asn1cSequenceAdd(container->protocolIEs->list, NGAP_PagingIEs_t, ie4);
    ie4->id = NGAP_ProtocolIE_ID_id_UERadioCapabilityForPaging;
    ie4->criticality = NGAP_Criticality_ignore;
    ie4->value.present = NGAP_PagingIEs__value_PR_UERadioCapabilityForPaging;
    NGAP_UERadioCapabilityForPaging_t *ue_radio_cap = (ie4->value.choice.UERadioCapabilityForPaging = calloc_or_fail(1, sizeof(*ie4->value.choice.UERadioCapabilityForPaging)));
    // UERadioCapabilityForPaging is a SEQUENCE with optional OCTET_STRING fields (Encode as NR)
    asn1cCalloc(ue_radio_cap->uERadioCapabilityForPagingOfNR, nr_field);
    OCTET_STRING_fromBuf(nr_field, (const char *)ue_radio_cap_ba->buf, ue_radio_cap_ba->len);
  }

  // PagingOrigin (O)
  if (msg->origin != NULL) {
    asn1cSequenceAdd(container->protocolIEs->list, NGAP_PagingIEs_t, ie5);
    ie5->id = NGAP_ProtocolIE_ID_id_PagingOrigin;
    ie5->criticality = NGAP_Criticality_ignore;
    ie5->value.present = NGAP_PagingIEs__value_PR_PagingOrigin;
    asn1cCallocOne(ie5->value.choice.PagingOrigin, *msg->origin);
  }

  return pdu;
}

/** @brief Free memory allocated for NGAP paging indication
 * @param msg Pointer to paging indication structure to free */
void free_ng_paging(ngap_paging_ind_t *msg)
{
  DevAssert(msg != NULL);
  free(msg->paging_drx);
  free(msg->paging_priority);
  if (msg->ue_radio_capability != NULL) {
    free_byte_array(*msg->ue_radio_capability);
    free(msg->ue_radio_capability);
  }
  free(msg->origin);
  free(msg->paging_attempt_info);
}

/** @brief Decode NGAP Paging message (9.2.4.1 of 3GPP TS 38.413)
 * @param out Pointer to output structure to fill
 * @param pdu Pointer to NGAP PDU containing the Paging message
 * @return true on success, false on failure */
bool decode_ng_paging(ngap_paging_ind_t *out, const NGAP_NGAP_PDU_t *pdu)
{
  DevAssert(pdu != NULL);
  DevAssert(out != NULL);

  if (pdu->present != NGAP_NGAP_PDU_PR_initiatingMessage) {
    NGAP_ERROR("Invalid PDU present value for Paging message\n");
    return false;
  }

  const NGAP_Paging_t *container = pdu->choice.initiatingMessage->value.choice.Paging;
  NGAP_PagingIEs_t *ie;
  fiveg_s_tmsi_t *s_tmsi = &out->ue_paging_identity.s_tmsi;

  // Reset output structure
  memset(out, 0, sizeof(*out));

  // UE Paging Identity (M)
  NGAP_FIND_PROTOCOLIE_BY_ID(NGAP_PagingIEs_t, ie, container, NGAP_ProtocolIE_ID_id_UEPagingIdentity, true);

  if (ie->value.choice.UEPagingIdentity->present == NGAP_UEPagingIdentity_PR_fiveG_S_TMSI) {
    const struct NGAP_FiveG_S_TMSI *fiveG_S_TMSI = ie->value.choice.UEPagingIdentity->choice.fiveG_S_TMSI;
    s_tmsi->amf_set_id = BIT_STRING_to_uint16(&fiveG_S_TMSI->aMFSetID);
    s_tmsi->amf_pointer = BIT_STRING_to_uint8(&fiveG_S_TMSI->aMFPointer);
    OCTET_STRING_TO_INT32(&fiveG_S_TMSI->fiveG_TMSI, s_tmsi->m_tmsi);
  } else {
    NGAP_ERROR("Unsupported UE Paging Identity type\n");
    return false;
  }

  // TAI List for Paging (M)
  NGAP_FIND_PROTOCOLIE_BY_ID(NGAP_PagingIEs_t, ie, container, NGAP_ProtocolIE_ID_id_TAIListForPaging, true);

  // Validate TAI list count (1..maxnoofTAIforPaging)
  int tai_count = ie->value.choice.TAIListForPaging->list.count;
  if (tai_count < 1 || tai_count > NGAP_MAX_NO_TAI_PAGING) {
    NGAP_ERROR("Invalid TAI list count: %d (must be 1..%d)\n", tai_count, NGAP_MAX_NO_TAI_PAGING);
    return false;
  }

  out->n_tai = 0;
  for (int i = 0; i < tai_count; i++) {
    const NGAP_TAIListForPagingItem_t *item_p = ie->value.choice.TAIListForPaging->list.array[i];
    DevAssert(item_p != NULL);
    const NGAP_TAI_t *tai = item_p->tAI;
    nr_tai_t *tai_out = &out->tai_list[i];
    TBCD_TO_MCC_MNC(&tai->pLMNIdentity, tai_out->plmn.mcc, tai_out->plmn.mnc, tai_out->plmn.mnc_digit_length);
    OCTET_STRING_TO_INT24(&tai->tAC, tai_out->tac);
    NGAP_DEBUG("TAI[%d] for paging: MCC=%03d, MNC=%0*d, TAC=%d\n",
               i,
               tai_out->plmn.mcc,
               tai_out->plmn.mnc_digit_length,
               tai_out->plmn.mnc,
               tai_out->tac);
    out->n_tai++;
  }

  // Paging DRX (O)
  NGAP_FIND_PROTOCOLIE_BY_ID(NGAP_PagingIEs_t, ie, container, NGAP_ProtocolIE_ID_id_PagingDRX, false);
  if (ie != NULL) {
    out->paging_drx = malloc_or_fail(sizeof(ngap_paging_drx_t));
    *out->paging_drx = *ie->value.choice.PagingDRX;
    const char *s = text_info_at(*out->paging_drx, paging_drx_text, sizeofArray(paging_drx_text));
    NGAP_DEBUG("Decoded Paging DRX: %s", s);
  }

  // Paging Priority (O)
  NGAP_FIND_PROTOCOLIE_BY_ID(NGAP_PagingIEs_t, ie, container, NGAP_ProtocolIE_ID_id_PagingPriority, false);
  if (ie != NULL) {
    out->paging_priority = malloc_or_fail(sizeof(ngap_paging_priority_t));
    *out->paging_priority = *ie->value.choice.PagingPriority;
    const char *s = text_info_at(*out->paging_priority, paging_prio_text, sizeofArray(paging_prio_text));
    NGAP_DEBUG("Decoded Paging Priority: %s", s);
  }

  // UERadioCapabilityForPaging (O)
  NGAP_FIND_PROTOCOLIE_BY_ID(NGAP_PagingIEs_t, ie, container, NGAP_ProtocolIE_ID_id_UERadioCapabilityForPaging, false);
  if (ie != NULL) {
    const NGAP_UERadioCapabilityForPaging_t *ue_radio_cap = ie->value.choice.UERadioCapabilityForPaging;
    const OCTET_STRING_t *nr_field = ue_radio_cap->uERadioCapabilityForPagingOfNR;
    const OCTET_STRING_t *eutra_field = ue_radio_cap->uERadioCapabilityForPagingOfEUTRA;
    if (nr_field != NULL) {
      out->ue_radio_capability = malloc_or_fail(sizeof(byte_array_t));
      *out->ue_radio_capability = create_byte_array(nr_field->size, nr_field->buf);
      NGAP_DEBUG("Decoded UERadioCapabilityForPaging (NR): %zu bytes\n", nr_field->size);
    } else if (eutra_field != NULL) {
      out->ue_radio_capability = malloc_or_fail(sizeof(byte_array_t));
      *out->ue_radio_capability = create_byte_array(eutra_field->size, eutra_field->buf);
      NGAP_DEBUG("Decoded UERadioCapabilityForPaging (EUTRA): %zu bytes\n", eutra_field->size);
    } else {
      NGAP_WARN("UERadioCapabilityForPaging present but both NR and EUTRA fields are NULL\n");
    }
  }

  // PagingOrigin (O)
  NGAP_FIND_PROTOCOLIE_BY_ID(NGAP_PagingIEs_t, ie, container, NGAP_ProtocolIE_ID_id_PagingOrigin, false);
  if (ie != NULL) {
    // PagingOrigin shall be transferred to UE according to TS 38.331 and TS 36.331
    out->origin = calloc_or_fail(1, sizeof(*out->origin));
    *out->origin = *ie->value.choice.PagingOrigin;
  }

  // AssistanceDataForPaging (O) - contains PagingAttemptInformation
  NGAP_FIND_PROTOCOLIE_BY_ID(NGAP_PagingIEs_t, ie, container, NGAP_ProtocolIE_ID_id_AssistanceDataForPaging, false);
  if (ie != NULL) {
    const NGAP_AssistanceDataForPaging_t *assistance_data = ie->value.choice.AssistanceDataForPaging;

    // Extract PagingAttemptInformation if present (TS 38.413 §9.3.1.72)
    if (assistance_data->pagingAttemptInformation != NULL) {
      const NGAP_PagingAttemptInformation_t *paging_attempt_info = assistance_data->pagingAttemptInformation;
      out->paging_attempt_info = calloc_or_fail(1, sizeof(ngap_paging_attempt_info_t));

      // Paging Attempt Count (mandatory)
      uint8_t attempt_count = paging_attempt_info->pagingAttemptCount;
      DevAssert(attempt_count >= NGAP_PAGING_ATTEMPT_MIN && attempt_count <= NGAP_PAGING_ATTEMPT_MAX);
      out->paging_attempt_info->paging_attempt_count = attempt_count;
      NGAP_DEBUG("Decoded Paging Attempt Count: %u\n", out->paging_attempt_info->paging_attempt_count);

      // Intended Number of Paging Attempts (mandatory)
      uint8_t intended_attempts = paging_attempt_info->intendedNumberOfPagingAttempts;
      DevAssert(intended_attempts >= NGAP_PAGING_ATTEMPT_MIN && intended_attempts <= NGAP_PAGING_ATTEMPT_MAX);
      out->paging_attempt_info->intended_paging_attempts = intended_attempts;
      NGAP_DEBUG("Decoded Intended Number of Paging Attempts: %u\n", out->paging_attempt_info->intended_paging_attempts);
    }
  }

  NGAP_DEBUG("Decoded Paging: m_tmsi=0x%08x, TAI=%d", s_tmsi->m_tmsi, out->n_tai);

  return true;
}

/** @brief Check equality of two NGAP paging indication structures
 * @param a First structure
 * @param b Second structure
 * @return true if equal, false otherwise */
bool eq_ng_paging(const ngap_paging_ind_t *a, const ngap_paging_ind_t *b)
{
  if (a == NULL || b == NULL) {
    return a == b;
  }

  // UE Paging Identity
  const fiveg_s_tmsi_t *a_tmsi = &a->ue_paging_identity.s_tmsi;
  const fiveg_s_tmsi_t *b_tmsi = &b->ue_paging_identity.s_tmsi;
  _EQ_CHECK_INT(a_tmsi->amf_set_id, b_tmsi->amf_set_id);
  _EQ_CHECK_INT(a_tmsi->amf_pointer, b_tmsi->amf_pointer);
  _EQ_CHECK_UINT32(a_tmsi->m_tmsi, b_tmsi->m_tmsi);

  // TAI List
  _EQ_CHECK_INT(a->n_tai, b->n_tai);
  for (int i = 0; i < a->n_tai; i++) {
    if (!eq_ngap_plmn(&a->tai_list[i].plmn, &b->tai_list[i].plmn)) {
      return false;
    }
    _EQ_CHECK_INT(a->tai_list[i].tac, b->tai_list[i].tac);
  }

  // Paging DRX (optional)
  _EQ_CHECK_OPTIONAL_IE(a, b, paging_drx, _EQ_CHECK_INT);

  // Paging Priority (optional)
  _EQ_CHECK_OPTIONAL_IE(a, b, paging_priority, _EQ_CHECK_INT);

  // UERadioCapabilityForPaging (optional)
  _EQ_CHECK_OPTIONAL_PTR(a, b, ue_radio_capability);
  if (a->ue_radio_capability != NULL && b->ue_radio_capability != NULL) {
    if (!eq_byte_array(a->ue_radio_capability, b->ue_radio_capability)) {
      NGAP_ERROR("Equality condition 'eq_byte_array' failed: byte array contents differ\n");
      return false;
    }
  }

  // PagingOrigin (optional)
  _EQ_CHECK_OPTIONAL_IE(a, b, origin, _EQ_CHECK_INT);

  // Assistance Data for Paging (optional)
  _EQ_CHECK_OPTIONAL_PTR(a, b, paging_attempt_info);
  if (a->paging_attempt_info != NULL && b->paging_attempt_info != NULL) {
    _EQ_CHECK_INT(a->paging_attempt_info->paging_attempt_count, b->paging_attempt_info->paging_attempt_count);
    _EQ_CHECK_INT(a->paging_attempt_info->intended_paging_attempts, b->paging_attempt_info->intended_paging_attempts);
  }

  return true;
}
