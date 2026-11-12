/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef F1AP_INTERFACE_MANAGEMENT_H_
#define F1AP_INTERFACE_MANAGEMENT_H_

#include <stdbool.h>
#include "f1ap_messages_types.h"

struct F1AP_F1AP_PDU;

/* F1 Reset */
struct F1AP_F1AP_PDU *encode_f1ap_reset(const f1ap_reset_t *msg);
bool decode_f1ap_reset(const struct F1AP_F1AP_PDU *pdu, f1ap_reset_t *out);
void free_f1ap_reset(f1ap_reset_t *msg);
bool eq_f1ap_reset(const f1ap_reset_t *a, const f1ap_reset_t *b);
f1ap_reset_t cp_f1ap_reset(const f1ap_reset_t *orig);

/* F1 Reset Ack */
struct F1AP_F1AP_PDU *encode_f1ap_reset_ack(const f1ap_reset_ack_t *msg);
bool decode_f1ap_reset_ack(const struct F1AP_F1AP_PDU *pdu, f1ap_reset_ack_t *out);
void free_f1ap_reset_ack(f1ap_reset_ack_t *msg);
bool eq_f1ap_reset_ack(const f1ap_reset_ack_t *a, const f1ap_reset_ack_t *b);
f1ap_reset_ack_t cp_f1ap_reset_ack(const f1ap_reset_ack_t *orig);

struct F1AP_F1AP_PDU *encode_f1ap_setup_request(const f1ap_setup_req_t *msg);
bool decode_f1ap_setup_request(const struct F1AP_F1AP_PDU *pdu, f1ap_setup_req_t *out);
f1ap_setup_req_t cp_f1ap_setup_request(const f1ap_setup_req_t *msg);
bool eq_f1ap_setup_request(const f1ap_setup_req_t *a, const f1ap_setup_req_t *b);
void free_f1ap_setup_request(const f1ap_setup_req_t *msg);

/* F1 Setup Response */
struct F1AP_F1AP_PDU *encode_f1ap_setup_response(const f1ap_setup_resp_t *msg);
bool decode_f1ap_setup_response(const struct F1AP_F1AP_PDU *pdu, f1ap_setup_resp_t *out);
void free_f1ap_setup_response(const f1ap_setup_resp_t *msg);
f1ap_setup_resp_t cp_f1ap_setup_response(const f1ap_setup_resp_t *msg);
bool eq_f1ap_setup_response(const f1ap_setup_resp_t *a, const f1ap_setup_resp_t *b);

/* F1 Setup Failure */
struct F1AP_F1AP_PDU *encode_f1ap_setup_failure(const f1ap_setup_failure_t *msg);
bool decode_f1ap_setup_failure(const struct F1AP_F1AP_PDU *pdu, f1ap_setup_failure_t *out);
bool eq_f1ap_setup_failure(const f1ap_setup_failure_t *a, const f1ap_setup_failure_t *b);
f1ap_setup_failure_t cp_f1ap_setup_failure(const f1ap_setup_failure_t *msg);
void free_f1ap_setup_failure(const f1ap_setup_failure_t *msg);

/* F1 gNB-DU Configuration Update */
struct F1AP_F1AP_PDU *encode_f1ap_du_configuration_update(const f1ap_gnb_du_configuration_update_t *msg);
bool decode_f1ap_du_configuration_update(const struct F1AP_F1AP_PDU *pdu, f1ap_gnb_du_configuration_update_t *out);
bool eq_f1ap_du_configuration_update(const f1ap_gnb_du_configuration_update_t *a, const f1ap_gnb_du_configuration_update_t *b);
f1ap_gnb_du_configuration_update_t cp_f1ap_du_configuration_update(const f1ap_gnb_du_configuration_update_t *msg);
void free_f1ap_du_configuration_update(const f1ap_gnb_du_configuration_update_t *msg);

/* F1 gNB-CU Configuration Update */
struct F1AP_F1AP_PDU *encode_f1ap_cu_configuration_update(const f1ap_gnb_cu_configuration_update_t *msg);
bool decode_f1ap_cu_configuration_update(const struct F1AP_F1AP_PDU *pdu, f1ap_gnb_cu_configuration_update_t *out);
bool eq_f1ap_cu_configuration_update(const f1ap_gnb_cu_configuration_update_t *a, const f1ap_gnb_cu_configuration_update_t *b);
f1ap_gnb_cu_configuration_update_t cp_f1ap_cu_configuration_update(const f1ap_gnb_cu_configuration_update_t *msg);
void free_f1ap_cu_configuration_update(const f1ap_gnb_cu_configuration_update_t *msg);

/* F1 gNB-CU Configuration Update Acknowledge */
struct F1AP_F1AP_PDU *encode_f1ap_cu_configuration_update_acknowledge(const f1ap_gnb_cu_configuration_update_acknowledge_t *msg);
bool decode_f1ap_cu_configuration_update_acknowledge(const struct F1AP_F1AP_PDU *pdu,
                                                     f1ap_gnb_cu_configuration_update_acknowledge_t *out);
bool eq_f1ap_cu_configuration_update_acknowledge(const f1ap_gnb_cu_configuration_update_acknowledge_t *a,
                                                 const f1ap_gnb_cu_configuration_update_acknowledge_t *b);
f1ap_gnb_cu_configuration_update_acknowledge_t cp_f1ap_cu_configuration_update_acknowledge(
    const f1ap_gnb_cu_configuration_update_acknowledge_t *msg);
void free_f1ap_cu_configuration_update_acknowledge(const f1ap_gnb_cu_configuration_update_acknowledge_t *msg);

/* F1 gNB-DU Configuration Update Acknowledge */
struct F1AP_F1AP_PDU *encode_f1ap_du_configuration_update_acknowledge(const f1ap_gnb_du_configuration_update_acknowledge_t *msg);
bool decode_f1ap_du_configuration_update_acknowledge(const struct F1AP_F1AP_PDU *pdu,
                                                     f1ap_gnb_du_configuration_update_acknowledge_t *out);
bool eq_f1ap_du_configuration_update_acknowledge(const f1ap_gnb_du_configuration_update_acknowledge_t *a,
                                                 const f1ap_gnb_du_configuration_update_acknowledge_t *b);
f1ap_gnb_du_configuration_update_acknowledge_t cp_f1ap_du_configuration_update_acknowledge(
    const f1ap_gnb_du_configuration_update_acknowledge_t *msg);
void free_f1ap_du_configuration_update_acknowledge(const f1ap_gnb_du_configuration_update_acknowledge_t *msg);

#endif /* F1AP_INTERFACE_MANAGEMENT_H_ */
