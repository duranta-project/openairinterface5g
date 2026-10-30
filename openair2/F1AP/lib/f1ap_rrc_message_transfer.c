/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "f1ap_rrc_message_transfer.h"

#include "f1ap_lib_common.h"
#include "f1ap_lib_includes.h"
#include "f1ap_messages_types.h"

#include "common/utils/assertions.h"
#include "openair3/UTILS/conversions.h"
#include "common/utils/oai_asn1.h"
#include "common/utils/utils.h"
#include "F1AP_ProtocolIE-Container.h"

/* ====================================
 * F1AP Initial UL RRC Message Transfer
 * ==================================== */

/**
 * @brief Initial UL RRC Message Transfer encoding
 */
F1AP_F1AP_PDU_t *encode_initial_ul_rrc_message_transfer(const f1ap_initial_ul_rrc_message_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  /* 0. Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_InitialULRRCMessageTransfer;
  tmp->criticality = F1AP_Criticality_ignore;
  tmp->value.present = F1AP_InitiatingMessage__value_PR_InitialULRRCMessageTransfer;
  F1AP_InitialULRRCMessageTransfer_t *out = (tmp->value.choice.InitialULRRCMessageTransfer = calloc_or_fail(1, sizeof(*tmp->value.choice.InitialULRRCMessageTransfer)));

  /* mandatory */
  /* c1. GNB_DU_UE_F1AP_ID */
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_InitialULRRCMessageTransferIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_InitialULRRCMessageTransferIEs__value_PR_GNB_DU_UE_F1AP_ID;
  asn1cCallocOne(ie1->value.choice.GNB_DU_UE_F1AP_ID, msg->gNB_DU_ue_id);

  /* mandatory */
  /* c2. NRCGI */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_InitialULRRCMessageTransferIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_NRCGI;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_InitialULRRCMessageTransferIEs__value_PR_NRCGI;
  if (!ie2->value.choice.NRCGI)
    ie2->value.choice.NRCGI = calloc_or_fail(1, sizeof(*ie2->value.choice.NRCGI));
  MCC_MNC_TO_PLMNID(msg->plmn.mcc, msg->plmn.mnc, msg->plmn.mnc_digit_length, &ie2->value.choice.NRCGI->pLMN_Identity);
  NR_CELL_ID_TO_BIT_STRING(msg->nr_cellid, &ie2->value.choice.NRCGI->nRCellIdentity);

  /* mandatory */
  /* c3. C_RNTI */ // 16
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_InitialULRRCMessageTransferIEs_t, ie3);
  ie3->id = F1AP_ProtocolIE_ID_id_C_RNTI;
  ie3->criticality = F1AP_Criticality_reject;
  ie3->value.present = F1AP_InitialULRRCMessageTransferIEs__value_PR_C_RNTI;
  asn1cCallocOne(ie3->value.choice.C_RNTI, msg->crnti);

  /* mandatory */
  /* c4. RRCContainer */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_InitialULRRCMessageTransferIEs_t, ie4);
  ie4->id = F1AP_ProtocolIE_ID_id_RRCContainer;
  ie4->criticality = F1AP_Criticality_reject;
  ie4->value.present = F1AP_InitialULRRCMessageTransferIEs__value_PR_RRCContainer;
  OCTET_STRING_fromBuf((ie4->value.choice.RRCContainer = calloc_or_fail(1, sizeof(*ie4->value.choice.RRCContainer))), (const char *)msg->rrc_container, msg->rrc_container_length);

  /* optional */
  /* c5. DUtoCURRCContainer */
  if (msg->du2cu_rrc_container != NULL) {
    asn1cSequenceAdd(out->protocolIEs->list, F1AP_InitialULRRCMessageTransferIEs_t, ie5);
    ie5->id = F1AP_ProtocolIE_ID_id_DUtoCURRCContainer;
    ie5->criticality = F1AP_Criticality_reject;
    ie5->value.present = F1AP_InitialULRRCMessageTransferIEs__value_PR_DUtoCURRCContainer;
    OCTET_STRING_fromBuf((ie5->value.choice.DUtoCURRCContainer = calloc_or_fail(1, sizeof(*ie5->value.choice.DUtoCURRCContainer))),
                         (const char *)msg->du2cu_rrc_container,
                         msg->du2cu_rrc_container_length);
  }

  /* mandatory */
  /* c6. Transaction ID (integer value) */
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_InitialULRRCMessageTransferIEs_t, ie6);
  ie6->id = F1AP_ProtocolIE_ID_id_TransactionID;
  ie6->criticality = F1AP_Criticality_ignore;
  ie6->value.present = F1AP_InitialULRRCMessageTransferIEs__value_PR_TransactionID;
  asn1cCallocOne(ie6->value.choice.TransactionID, msg->transaction_id);

  return pdu;
}

/**
 * @brief Initial UL RRC Message Transfer decoding
 */
bool decode_initial_ul_rrc_message_transfer(const F1AP_F1AP_PDU_t *pdu, f1ap_initial_ul_rrc_message_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  F1AP_InitialULRRCMessageTransfer_t *container = pdu->choice.initiatingMessage->value.choice.InitialULRRCMessageTransfer;
  F1AP_ProtocolIE_Container_10986P31_t *pIEs = container->protocolIEs;
  F1AP_InitialULRRCMessageTransferIEs_t *ie;

  F1AP_LIB_FIND_IE(F1AP_InitialULRRCMessageTransferIEs_t, ie, &pIEs->list, F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID, true);
  out->gNB_DU_ue_id = *ie->value.choice.GNB_DU_UE_F1AP_ID;

  F1AP_LIB_FIND_IE(F1AP_InitialULRRCMessageTransferIEs_t, ie, &pIEs->list, F1AP_ProtocolIE_ID_id_NRCGI, true);
  PLMNID_TO_MCC_MNC(&ie->value.choice.NRCGI->pLMN_Identity, out->plmn.mcc, out->plmn.mnc, out->plmn.mnc_digit_length);
  BIT_STRING_TO_NR_CELL_IDENTITY(&ie->value.choice.NRCGI->nRCellIdentity, out->nr_cellid);

  F1AP_LIB_FIND_IE(F1AP_InitialULRRCMessageTransferIEs_t, ie, &pIEs->list, F1AP_ProtocolIE_ID_id_C_RNTI, true);
  out->crnti = *ie->value.choice.C_RNTI;

  F1AP_LIB_FIND_IE(F1AP_InitialULRRCMessageTransferIEs_t, ie, &pIEs->list, F1AP_ProtocolIE_ID_id_RRCContainer, true);
  out->rrc_container = cp_octet_string(ie->value.choice.RRCContainer, &out->rrc_container_length);

  F1AP_LIB_FIND_IE(F1AP_InitialULRRCMessageTransferIEs_t, ie, &pIEs->list, F1AP_ProtocolIE_ID_id_DUtoCURRCContainer, false);
  if (ie != NULL)
    out->du2cu_rrc_container = cp_octet_string(ie->value.choice.DUtoCURRCContainer, &out->du2cu_rrc_container_length);

  F1AP_LIB_FIND_IE(F1AP_InitialULRRCMessageTransferIEs_t, ie, &pIEs->list, F1AP_ProtocolIE_ID_id_TransactionID, true);
  out->transaction_id = *ie->value.choice.TransactionID;

  return true;
}

/**
 * @brief Initial UL RRC Message Transfer deep copy
 */
f1ap_initial_ul_rrc_message_t cp_initial_ul_rrc_message_transfer(const f1ap_initial_ul_rrc_message_t *msg)
{
  uint8_t *rrc_container = calloc_or_fail(msg->rrc_container_length, sizeof(*rrc_container));
  memcpy(rrc_container, msg->rrc_container, msg->rrc_container_length);
  uint8_t *du2cu_rrc_container = NULL;
  if (msg->du2cu_rrc_container_length > 0) {
    DevAssert(msg->du2cu_rrc_container != NULL);
    du2cu_rrc_container = calloc_or_fail(msg->du2cu_rrc_container_length, sizeof(*du2cu_rrc_container));
    memcpy(du2cu_rrc_container, msg->du2cu_rrc_container, msg->du2cu_rrc_container_length);
  }
  f1ap_initial_ul_rrc_message_t cp = {
    .gNB_DU_ue_id = msg->gNB_DU_ue_id,
    .plmn = msg->plmn,
    .nr_cellid = msg->nr_cellid,
    .crnti = msg->crnti,
    .rrc_container = rrc_container,
    .rrc_container_length = msg->rrc_container_length,
    .du2cu_rrc_container = du2cu_rrc_container,
    .du2cu_rrc_container_length = msg->du2cu_rrc_container_length,
    .transaction_id = msg->transaction_id
  };
  return cp;
}

/**
 * @brief Initial UL RRC Message Transfer equality check
 */
bool eq_initial_ul_rrc_message_transfer(const f1ap_initial_ul_rrc_message_t *a, const f1ap_initial_ul_rrc_message_t *b)
{
  _EQ_CHECK_INT(a->gNB_DU_ue_id, b->gNB_DU_ue_id);
  if (!eq_f1ap_plmn(&a->plmn, &b->plmn))
    return false;
  _EQ_CHECK_LONG(a->nr_cellid, b->nr_cellid);
  _EQ_CHECK_INT(a->crnti, b->crnti);
  _EQ_CHECK_INT(a->rrc_container_length, b->rrc_container_length);
  _EQ_CHECK_LONG(a->nr_cellid, b->nr_cellid);
  if (memcmp(a->rrc_container, b->rrc_container, a->rrc_container_length) != 0)
    return false;
  _EQ_CHECK_INT(a->du2cu_rrc_container_length, b->du2cu_rrc_container_length);
  if (memcmp(a->du2cu_rrc_container, b->du2cu_rrc_container, a->du2cu_rrc_container_length) != 0)
    return false;
  _EQ_CHECK_INT(a->transaction_id, b->transaction_id);
  return true;
}

/**
 * @brief Initial UL RRC Message Transfer memory management
 */
void free_initial_ul_rrc_message_transfer(const f1ap_initial_ul_rrc_message_t *msg)
{
  DevAssert(msg != NULL);
  free(msg->rrc_container);
  free(msg->du2cu_rrc_container);
}

/* ====================================
 * F1AP UL RRC Message Transfer
 * ==================================== */

/**
 * @brief UL RRC Message Transfer encoding (9.2.3.3 of 3GPP TS 38.473)
 *        gNB-DU → gNB-CU
 */
F1AP_F1AP_PDU_t *encode_ul_rrc_message_transfer(const f1ap_ul_rrc_message_t *msg)
{

  F1AP_F1AP_PDU_t *pdu = calloc(1, sizeof(*pdu));
  AssertFatal(pdu != NULL, "out of memory\n");

  /* Create */
  /* 0. Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_ULRRCMessageTransfer;
  tmp->criticality = F1AP_Criticality_ignore;
  tmp->value.present = F1AP_InitiatingMessage__value_PR_ULRRCMessageTransfer;
  F1AP_ULRRCMessageTransfer_t *out = (tmp->value.choice.ULRRCMessageTransfer = calloc_or_fail(1, sizeof(*tmp->value.choice.ULRRCMessageTransfer)));
  // gNB-CU UE F1AP ID (M)
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_ULRRCMessageTransferIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_ULRRCMessageTransferIEs__value_PR_GNB_CU_UE_F1AP_ID;
  asn1cCallocOne(ie1->value.choice.GNB_CU_UE_F1AP_ID, msg->gNB_CU_ue_id);
  // gNB-DU UE F1AP ID (M)
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_ULRRCMessageTransferIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_ULRRCMessageTransferIEs__value_PR_GNB_DU_UE_F1AP_ID;
  asn1cCallocOne(ie2->value.choice.GNB_DU_UE_F1AP_ID, msg->gNB_DU_ue_id);
  // SRB ID (M)
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_ULRRCMessageTransferIEs_t, ie3);
  ie3->id = F1AP_ProtocolIE_ID_id_SRBID;
  ie3->criticality = F1AP_Criticality_reject;
  ie3->value.present = F1AP_ULRRCMessageTransferIEs__value_PR_SRBID;
  asn1cCallocOne(ie3->value.choice.SRBID, msg->srb_id);
  // RRC-Container (M)
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_ULRRCMessageTransferIEs_t, ie4);
  ie4->id = F1AP_ProtocolIE_ID_id_RRCContainer;
  ie4->criticality = F1AP_Criticality_reject;
  ie4->value.present = F1AP_ULRRCMessageTransferIEs__value_PR_RRCContainer;
  OCTET_STRING_fromBuf((ie4->value.choice.RRCContainer = calloc_or_fail(1, sizeof(*ie4->value.choice.RRCContainer))), (const char *)msg->rrc_container, msg->rrc_container_length);

  return pdu;
}

/**
 * @brief UL RRC Message Transfer decoding (9.2.3.3 of 3GPP TS 38.473)
 *        gNB-DU → gNB-CU
 */
bool decode_ul_rrc_message_transfer(const F1AP_F1AP_PDU_t *pdu, f1ap_ul_rrc_message_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));
  DevAssert(pdu != NULL);

  F1AP_ULRRCMessageTransfer_t *container = pdu->choice.initiatingMessage->value.choice.ULRRCMessageTransfer;
  F1AP_ProtocolIE_Container_10986P33_t *pIEs = container->protocolIEs;
  F1AP_ULRRCMessageTransferIEs_t *ie;

  // gNB-CU UE F1AP ID (M)
  F1AP_LIB_FIND_IE(F1AP_ULRRCMessageTransferIEs_t, ie, &pIEs->list, F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID, true);
  out->gNB_CU_ue_id = *ie->value.choice.GNB_CU_UE_F1AP_ID;
  // gNB-DU UE F1AP ID (M)
  F1AP_LIB_FIND_IE(F1AP_ULRRCMessageTransferIEs_t, ie, &pIEs->list, F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID, true);
  out->gNB_DU_ue_id = *ie->value.choice.GNB_DU_UE_F1AP_ID;
  // SRB ID (M)
  F1AP_LIB_FIND_IE(F1AP_ULRRCMessageTransferIEs_t, ie, &pIEs->list, F1AP_ProtocolIE_ID_id_SRBID, true);
  out->srb_id = *ie->value.choice.SRBID;
  // RRC-Container (M)
  F1AP_LIB_FIND_IE(F1AP_ULRRCMessageTransferIEs_t, ie, &pIEs->list, F1AP_ProtocolIE_ID_id_RRCContainer, true);
  out->rrc_container = cp_octet_string(ie->value.choice.RRCContainer, &out->rrc_container_length);

  return true;
}

/**
 * @brief UL RRC Message Transfer deep copy
 */
f1ap_ul_rrc_message_t cp_ul_rrc_message_transfer(const f1ap_ul_rrc_message_t *msg)
{
  uint8_t *rrc_container = calloc_or_fail(msg->rrc_container_length, sizeof(*rrc_container));
  memcpy(rrc_container, msg->rrc_container, msg->rrc_container_length);
  f1ap_ul_rrc_message_t cp = {
      .gNB_DU_ue_id = msg->gNB_DU_ue_id,
      .gNB_CU_ue_id = msg->gNB_CU_ue_id,
      .srb_id = msg->srb_id,
      .rrc_container = rrc_container,
      .rrc_container_length = msg->rrc_container_length,
  };
  return cp;
}

/**
 * @brief UL RRC Message Transfer equality check
 */
bool eq_ul_rrc_message_transfer(const f1ap_ul_rrc_message_t *a, const f1ap_ul_rrc_message_t *b)
{
  _EQ_CHECK_INT(a->gNB_DU_ue_id, b->gNB_DU_ue_id);
  _EQ_CHECK_INT(a->gNB_CU_ue_id, b->gNB_CU_ue_id);
  _EQ_CHECK_INT(a->rrc_container_length, b->rrc_container_length);
  _EQ_CHECK_INT(a->gNB_DU_ue_id, b->gNB_DU_ue_id);
  if (memcmp(a->rrc_container, b->rrc_container, a->rrc_container_length) != 0)
    return false;
  return true;
}

/**
 * @brief UL RRC Message Transfer memory management
 */
void free_ul_rrc_message_transfer(const f1ap_ul_rrc_message_t *msg)
{
  DevAssert(msg != NULL);
  free(msg->rrc_container);
}

/* ============================
 * F1AP DL RRC Message Transfer
 * ============================ */

/**
 * @brief DL RRC Message Transfer encoding (9.2.3.2 of 3GPP TS 38.473)
 *        gNB-CU → gNB-DU
 */
F1AP_F1AP_PDU_t *encode_dl_rrc_message_transfer(const f1ap_dl_rrc_message_t *msg)
{
  F1AP_F1AP_PDU_t *pdu = calloc(1, sizeof(*pdu));
  AssertFatal(pdu != NULL, "out of memory\n");

  /* Create */
  /* 0. Message Type */
  pdu->present = F1AP_F1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, tmp);
  tmp->procedureCode = F1AP_ProcedureCode_id_DLRRCMessageTransfer;
  tmp->criticality = F1AP_Criticality_ignore;
  tmp->value.present = F1AP_InitiatingMessage__value_PR_DLRRCMessageTransfer;
  F1AP_DLRRCMessageTransfer_t *out = (tmp->value.choice.DLRRCMessageTransfer = calloc_or_fail(1, sizeof(*tmp->value.choice.DLRRCMessageTransfer)));
  // gNB-CU UE F1AP ID (M)
  if (!out->protocolIEs)
    out->protocolIEs = calloc_or_fail(1, sizeof(*out->protocolIEs));
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_DLRRCMessageTransferIEs_t, ie1);
  ie1->id = F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID;
  ie1->criticality = F1AP_Criticality_reject;
  ie1->value.present = F1AP_DLRRCMessageTransferIEs__value_PR_GNB_CU_UE_F1AP_ID;
  asn1cCallocOne(ie1->value.choice.GNB_CU_UE_F1AP_ID, msg->gNB_CU_ue_id);
  // gNB-DU UE F1AP ID (M)
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_DLRRCMessageTransferIEs_t, ie2);
  ie2->id = F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID;
  ie2->criticality = F1AP_Criticality_reject;
  ie2->value.present = F1AP_DLRRCMessageTransferIEs__value_PR_GNB_DU_UE_F1AP_ID;
  asn1cCallocOne(ie2->value.choice.GNB_DU_UE_F1AP_ID, msg->gNB_DU_ue_id);
  // old gNB-DU UE F1AP ID (O)
  if (msg->old_gNB_DU_ue_id) {
    asn1cSequenceAdd(out->protocolIEs->list, F1AP_DLRRCMessageTransferIEs_t, ie3);
    ie3->id = F1AP_ProtocolIE_ID_id_oldgNB_DU_UE_F1AP_ID;
    ie3->criticality = F1AP_Criticality_reject;
    ie3->value.present = F1AP_DLRRCMessageTransferIEs__value_PR_GNB_DU_UE_F1AP_ID_1;
    asn1cCallocOne(ie3->value.choice.GNB_DU_UE_F1AP_ID_1, *msg->old_gNB_DU_ue_id);
  }
  // SRB ID (M)
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_DLRRCMessageTransferIEs_t, ie4);
  ie4->id = F1AP_ProtocolIE_ID_id_SRBID;
  ie4->criticality = F1AP_Criticality_reject;
  ie4->value.present = F1AP_DLRRCMessageTransferIEs__value_PR_SRBID;
  asn1cCallocOne(ie4->value.choice.SRBID, msg->srb_id);
  // Execute Duplication (O)
  if (msg->execute_duplication) {
    asn1cSequenceAdd(out->protocolIEs->list, F1AP_DLRRCMessageTransferIEs_t, ie5);
    ie5->id = F1AP_ProtocolIE_ID_id_ExecuteDuplication;
    ie5->criticality = F1AP_Criticality_ignore;
    ie5->value.present = F1AP_DLRRCMessageTransferIEs__value_PR_ExecuteDuplication;
    asn1cCallocOne(ie5->value.choice.ExecuteDuplication, F1AP_ExecuteDuplication_true);
  }
  // RRC-Container (M)
  asn1cSequenceAdd(out->protocolIEs->list, F1AP_DLRRCMessageTransferIEs_t, ie6);
  ie6->id = F1AP_ProtocolIE_ID_id_RRCContainer;
  ie6->criticality = F1AP_Criticality_reject;
  ie6->value.present = F1AP_DLRRCMessageTransferIEs__value_PR_RRCContainer;
  OCTET_STRING_fromBuf((ie6->value.choice.RRCContainer = calloc_or_fail(1, sizeof(*ie6->value.choice.RRCContainer))), (const char *)msg->rrc_container, msg->rrc_container_length);

  return pdu;
}

/**
 * @brief DL RRC Message Transfer decoding (9.2.3.2 of 3GPP TS 38.473)
 *        gNB-CU → gNB-DU
 */
bool decode_dl_rrc_message_transfer(const F1AP_F1AP_PDU_t *pdu, f1ap_dl_rrc_message_t *out)
{
  DevAssert(out != NULL);
  memset(out, 0, sizeof(*out));

  DevAssert(pdu != NULL);
  F1AP_DLRRCMessageTransfer_t *container = pdu->choice.initiatingMessage->value.choice.DLRRCMessageTransfer;
  F1AP_ProtocolIE_Container_10986P32_t *pIEs = container->protocolIEs;
  F1AP_DLRRCMessageTransferIEs_t *ie;
  // gNB-CU UE F1AP ID (M)
  F1AP_LIB_FIND_IE(F1AP_DLRRCMessageTransferIEs_t, ie, &pIEs->list, F1AP_ProtocolIE_ID_id_gNB_CU_UE_F1AP_ID, true);
  out->gNB_CU_ue_id = *ie->value.choice.GNB_CU_UE_F1AP_ID;
  // gNB-DU UE F1AP ID (M)
  F1AP_LIB_FIND_IE(F1AP_DLRRCMessageTransferIEs_t, ie, &pIEs->list, F1AP_ProtocolIE_ID_id_gNB_DU_UE_F1AP_ID, true);
  out->gNB_DU_ue_id = *ie->value.choice.GNB_DU_UE_F1AP_ID;
  // old gNB-DU UE F1AP ID (O)
  out->old_gNB_DU_ue_id = NULL;
  F1AP_LIB_FIND_IE(F1AP_DLRRCMessageTransferIEs_t, ie, &pIEs->list, F1AP_ProtocolIE_ID_id_oldgNB_DU_UE_F1AP_ID, false);
  if (ie) {
    out->old_gNB_DU_ue_id = malloc_or_fail(sizeof(*out->old_gNB_DU_ue_id));
    *out->old_gNB_DU_ue_id = *ie->value.choice.GNB_DU_UE_F1AP_ID_1;
  }
  // SRB ID (M)
  F1AP_LIB_FIND_IE(F1AP_DLRRCMessageTransferIEs_t, ie, &pIEs->list, F1AP_ProtocolIE_ID_id_SRBID, true);
  out->srb_id = *ie->value.choice.SRBID;
  // Execute Duplication (O)
  F1AP_LIB_FIND_IE(F1AP_DLRRCMessageTransferIEs_t, ie, &pIEs->list, F1AP_ProtocolIE_ID_id_ExecuteDuplication, false);
  if (ie) {
    out->execute_duplication = *ie->value.choice.ExecuteDuplication;
  }
  // RRC-Container (M)
  F1AP_LIB_FIND_IE(F1AP_DLRRCMessageTransferIEs_t, ie, &pIEs->list, F1AP_ProtocolIE_ID_id_RRCContainer, true);
  out->rrc_container = cp_octet_string(ie->value.choice.RRCContainer, &out->rrc_container_length);

  return true;
}

/**
 * @brief DL RRC Message Transfer deep copy
 */
f1ap_dl_rrc_message_t cp_dl_rrc_message_transfer(const f1ap_dl_rrc_message_t *msg)
{
  uint8_t *rrc_container = calloc_or_fail(msg->rrc_container_length, sizeof(*rrc_container));
  memcpy(rrc_container, msg->rrc_container, msg->rrc_container_length);

  uint32_t *old_gNB_DU_ue_id = NULL;
  if (msg->old_gNB_DU_ue_id) {
    old_gNB_DU_ue_id = malloc_or_fail(sizeof(*old_gNB_DU_ue_id));
    *old_gNB_DU_ue_id = *(msg->old_gNB_DU_ue_id);
  }

  f1ap_dl_rrc_message_t cp = {
      .gNB_DU_ue_id = msg->gNB_DU_ue_id,
      .gNB_CU_ue_id = msg->gNB_CU_ue_id,
      .old_gNB_DU_ue_id = old_gNB_DU_ue_id,
      .srb_id = msg->srb_id,
      .execute_duplication = msg->execute_duplication,
      .rrc_container = rrc_container,
      .rrc_container_length = msg->rrc_container_length,
  };

  return cp;
}

/**
 * @brief DL RRC Message Transfer equality check
 */
bool eq_dl_rrc_message_transfer(const f1ap_dl_rrc_message_t *a, const f1ap_dl_rrc_message_t *b)
{
  _EQ_CHECK_INT(a->gNB_DU_ue_id, b->gNB_DU_ue_id);
  _EQ_CHECK_INT(a->gNB_CU_ue_id, b->gNB_CU_ue_id);
  _EQ_CHECK_INT(a->gNB_DU_ue_id, b->gNB_DU_ue_id);
  _EQ_CHECK_INT(a->gNB_DU_ue_id, b->gNB_DU_ue_id);
  _EQ_CHECK_INT(a->gNB_DU_ue_id, b->gNB_DU_ue_id);
  if (!(a->old_gNB_DU_ue_id == NULL && b->old_gNB_DU_ue_id == NULL))
    _EQ_CHECK_INT(*a->old_gNB_DU_ue_id, *b->old_gNB_DU_ue_id);
  _EQ_CHECK_INT(a->srb_id, b->srb_id);
  _EQ_CHECK_INT(a->execute_duplication, b->execute_duplication);
  _EQ_CHECK_INT(a->rrc_container_length, b->rrc_container_length);
  if (memcmp(a->rrc_container, b->rrc_container, a->rrc_container_length) != 0)
    return false;
  return true;
}

/**
 * @brief DL RRC Message Transfer memory management
 */
void free_dl_rrc_message_transfer(const f1ap_dl_rrc_message_t *msg)
{
  DevAssert(msg != NULL);
  free(msg->rrc_container);
  free(msg->old_gNB_DU_ue_id);
}
