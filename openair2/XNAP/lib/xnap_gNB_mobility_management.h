/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef XNAP_GNB_MOBILITY_MANAGEMENT_H_
#define XNAP_GNB_MOBILITY_MANAGEMENT_H_

#include "xnap_messages_types.h"
typedef struct XNAP_XnAP_PDU XNAP_XnAP_PDU_t;

XNAP_XnAP_PDU_t *encode_xnap_handover_request(const xnap_handover_req_t *xnap_handover_req);
XNAP_XnAP_PDU_t *encode_xnap_handover_request_acknowledge(const xnap_handover_req_ack_t *xnap_handover_req_ack);
XNAP_XnAP_PDU_t *encode_xnap_handover_preparation_failure(const xnap_handover_preparation_failure_t *failure);
XNAP_XnAP_PDU_t *encode_xnap_sn_status_transfer(const xnap_sn_status_transfer_t *sn_status);
XNAP_XnAP_PDU_t *encode_xnap_ue_context_release(const xnap_ue_context_release_t *msg);
XNAP_XnAP_PDU_t *encode_xnap_handover_cancel(const xnap_handover_cancel_t *msg);
XNAP_XnAP_PDU_t *encode_xnap_handover_success(const xnap_handover_success_t *msg);
XNAP_XnAP_PDU_t *encode_xnap_ran_paging(const xnap_ran_paging_t *msg);

bool decode_xnap_handover_request(xnap_handover_req_t *req, const XNAP_XnAP_PDU_t *pdu);
bool decode_xnap_handover_request_acknowledge(xnap_handover_req_ack_t *out, const XNAP_XnAP_PDU_t *pdu);
bool decode_xnap_handover_preparation_failure(xnap_handover_preparation_failure_t *out, const XNAP_XnAP_PDU_t *pdu);
bool decode_xnap_sn_status_transfer(xnap_sn_status_transfer_t *out, const XNAP_XnAP_PDU_t *pdu);
bool decode_xnap_ue_context_release(xnap_ue_context_release_t *out, const XNAP_XnAP_PDU_t *pdu);
bool decode_xnap_handover_cancel(xnap_handover_cancel_t *out, const XNAP_XnAP_PDU_t *pdu);
bool decode_xnap_handover_success(xnap_handover_success_t *out, const XNAP_XnAP_PDU_t *pdu);
bool decode_xnap_ran_paging(xnap_ran_paging_t *out, const XNAP_XnAP_PDU_t *pdu);

bool eq_xnap_handover_request(const xnap_handover_req_t *a, const xnap_handover_req_t *b);
bool eq_xnap_handover_request_acknowledge(const xnap_handover_req_ack_t *a, const xnap_handover_req_ack_t *b);
bool eq_xnap_handover_preparation_failure(const xnap_handover_preparation_failure_t *a,
                                          const xnap_handover_preparation_failure_t *b);
bool eq_xnap_sn_status_transfer(const xnap_sn_status_transfer_t *a, const xnap_sn_status_transfer_t *b);
bool eq_xnap_ue_context_release(const xnap_ue_context_release_t *a, const xnap_ue_context_release_t *b);
bool eq_xnap_handover_cancel(const xnap_handover_cancel_t *a, const xnap_handover_cancel_t *b);
bool eq_xnap_handover_success(const xnap_handover_success_t *a, const xnap_handover_success_t *b);
bool eq_xnap_ran_paging(const xnap_ran_paging_t *a, const xnap_ran_paging_t *b);

void free_xnap_handover_request(xnap_handover_req_t *msg);
void free_xnap_handover_request_acknowledge(xnap_handover_req_ack_t *msg);
void free_xnap_handover_preparation_failure(xnap_handover_preparation_failure_t *msg);
void free_xnap_sn_status_transfer(xnap_sn_status_transfer_t *msg);
void free_xnap_ue_context_release(xnap_ue_context_release_t *msg);
void free_xnap_handover_cancel(xnap_handover_cancel_t *msg);
void free_xnap_handover_success(xnap_handover_success_t *msg);
void free_xnap_ran_paging(xnap_ran_paging_t *msg);

#endif // XNAP_GNB_MOBILITY_MANAGEMENT_H_
