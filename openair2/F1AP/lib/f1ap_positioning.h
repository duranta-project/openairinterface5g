/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef F1AP_POSITIONING_H_
#define F1AP_POSITIONING_H_

#include <stdbool.h>
#include "f1ap_messages_types.h"

struct F1AP_F1AP_PDU;

struct F1AP_F1AP_PDU *encode_pos_assistance_control(const f1ap_pos_assistance_control_t *msg);
bool decode_pos_assistance_control(const struct F1AP_F1AP_PDU *pdu, f1ap_pos_assistance_control_t *out);
f1ap_pos_assistance_control_t cp_pos_assistance_control(const f1ap_pos_assistance_control_t *orig);
bool eq_pos_assistance_control(const f1ap_pos_assistance_control_t *a, const f1ap_pos_assistance_control_t *b);
void free_pos_assistance_control(f1ap_pos_assistance_control_t *msg);

// positioning information request
struct F1AP_F1AP_PDU *encode_positioning_information_req(const f1ap_positioning_information_req_t *msg);
bool decode_positioning_information_req(const struct F1AP_F1AP_PDU *pdu, f1ap_positioning_information_req_t *out);
f1ap_positioning_information_req_t cp_positioning_information_req(const f1ap_positioning_information_req_t *orig);
bool eq_positioning_information_req(const f1ap_positioning_information_req_t *a, const f1ap_positioning_information_req_t *b);
void free_positioning_information_req(f1ap_positioning_information_req_t *msg);

// positioning information response
struct F1AP_F1AP_PDU *encode_positioning_information_resp(const f1ap_positioning_information_resp_t *msg);
bool decode_positioning_information_resp(const struct F1AP_F1AP_PDU *pdu, f1ap_positioning_information_resp_t *out);
f1ap_positioning_information_resp_t cp_positioning_information_resp(const f1ap_positioning_information_resp_t *orig);
bool eq_positioning_information_resp(const f1ap_positioning_information_resp_t *a, const f1ap_positioning_information_resp_t *b);
void free_positioning_information_resp(f1ap_positioning_information_resp_t *msg);

// positioning information failure
struct F1AP_F1AP_PDU *encode_positioning_information_failure(const f1ap_positioning_information_failure_t *msg);
bool decode_positioning_information_failure(const struct F1AP_F1AP_PDU *pdu, f1ap_positioning_information_failure_t *out);
f1ap_positioning_information_failure_t cp_positioning_information_failure(const f1ap_positioning_information_failure_t *orig);
bool eq_positioning_information_failure(const f1ap_positioning_information_failure_t *a,
                                        const f1ap_positioning_information_failure_t *b);
void free_positioning_information_failure(f1ap_positioning_information_failure_t *msg);

// positioning activation request
struct F1AP_F1AP_PDU *encode_positioning_activation_req(const f1ap_positioning_activation_req_t *msg);
bool decode_positioning_activation_req(const struct F1AP_F1AP_PDU *pdu, f1ap_positioning_activation_req_t *out);
f1ap_positioning_activation_req_t cp_positioning_activation_req(const f1ap_positioning_activation_req_t *orig);
bool eq_positioning_activation_req(const f1ap_positioning_activation_req_t *a, const f1ap_positioning_activation_req_t *b);
void free_positioning_activation_req(f1ap_positioning_activation_req_t *msg);

// positioning activation response
struct F1AP_F1AP_PDU *encode_positioning_activation_resp(const f1ap_positioning_activation_resp_t *msg);
bool decode_positioning_activation_resp(const struct F1AP_F1AP_PDU *pdu, f1ap_positioning_activation_resp_t *out);
f1ap_positioning_activation_resp_t cp_positioning_activation_resp(const f1ap_positioning_activation_resp_t *orig);
bool eq_positioning_activation_resp(const f1ap_positioning_activation_resp_t *a, const f1ap_positioning_activation_resp_t *b);
void free_positioning_activation_resp(f1ap_positioning_activation_resp_t *msg);

// positioning activation failure
struct F1AP_F1AP_PDU *encode_positioning_activation_failure(const f1ap_positioning_activation_failure_t *msg);
bool decode_positioning_activation_failure(const struct F1AP_F1AP_PDU *pdu, f1ap_positioning_activation_failure_t *out);
f1ap_positioning_activation_failure_t cp_positioning_activation_failure(const f1ap_positioning_activation_failure_t *orig);
bool eq_positioning_activation_failure(const f1ap_positioning_activation_failure_t *a,
                                       const f1ap_positioning_activation_failure_t *b);
void free_positioning_activation_failure(f1ap_positioning_activation_failure_t *msg);

// positioning deactivation
struct F1AP_F1AP_PDU *encode_positioning_deactivation(const f1ap_positioning_deactivation_t *msg);
bool decode_positioning_deactivation(const struct F1AP_F1AP_PDU *pdu, f1ap_positioning_deactivation_t *out);
f1ap_positioning_deactivation_t cp_positioning_deactivation(const f1ap_positioning_deactivation_t *orig);
bool eq_positioning_deactivation(const f1ap_positioning_deactivation_t *a, const f1ap_positioning_deactivation_t *b);
void free_positioning_deactivation(f1ap_positioning_deactivation_t *msg);

// positioning information update
struct F1AP_F1AP_PDU *encode_positioning_information_update(const f1ap_positioning_information_update_t *msg);
bool decode_positioning_information_update(const struct F1AP_F1AP_PDU *pdu, f1ap_positioning_information_update_t *out);
f1ap_positioning_information_update_t cp_positioning_information_update(const f1ap_positioning_information_update_t *orig);
bool eq_positioning_information_update(const f1ap_positioning_information_update_t *a,
                                       const f1ap_positioning_information_update_t *b);
void free_positioning_information_update(f1ap_positioning_information_update_t *msg);

// trp information request
struct F1AP_F1AP_PDU *encode_trp_information_req(const f1ap_trp_information_req_t *msg);
bool decode_trp_information_req(const struct F1AP_F1AP_PDU *pdu, f1ap_trp_information_req_t *out);
f1ap_trp_information_req_t cp_trp_information_req(const f1ap_trp_information_req_t *orig);
bool eq_trp_information_req(const f1ap_trp_information_req_t *a, const f1ap_trp_information_req_t *b);
void free_trp_information_req(f1ap_trp_information_req_t *msg);

// trp information response
struct F1AP_F1AP_PDU *encode_trp_information_resp(const f1ap_trp_information_resp_t *msg);
bool decode_trp_information_resp(const struct F1AP_F1AP_PDU *pdu, f1ap_trp_information_resp_t *out);
f1ap_trp_information_resp_t cp_trp_information_resp(const f1ap_trp_information_resp_t *orig);
bool eq_trp_information_resp(const f1ap_trp_information_resp_t *a, const f1ap_trp_information_resp_t *b);
void free_trp_information_resp(f1ap_trp_information_resp_t *msg);

// trp information failure
struct F1AP_F1AP_PDU *encode_trp_information_failure(const f1ap_trp_information_failure_t *msg);
bool decode_trp_information_failure(const struct F1AP_F1AP_PDU *pdu, f1ap_trp_information_failure_t *out);
f1ap_trp_information_failure_t cp_trp_information_failure(const f1ap_trp_information_failure_t *orig);
bool eq_trp_information_failure(const f1ap_trp_information_failure_t *a, const f1ap_trp_information_failure_t *b);
void free_trp_information_failure(f1ap_trp_information_failure_t *msg);

// positioning measurement request
struct F1AP_F1AP_PDU *encode_positioning_measurement_req(const f1ap_positioning_measurement_req_t *msg);
bool decode_positioning_measurement_req(const struct F1AP_F1AP_PDU *pdu, f1ap_positioning_measurement_req_t *out);
f1ap_positioning_measurement_req_t cp_positioning_measurement_req(const f1ap_positioning_measurement_req_t *orig);
bool eq_positioning_measurement_req(const f1ap_positioning_measurement_req_t *a, const f1ap_positioning_measurement_req_t *b);
void free_positioning_measurement_req(f1ap_positioning_measurement_req_t *msg);

// positioning measurement response
struct F1AP_F1AP_PDU *encode_positioning_measurement_resp(const f1ap_positioning_measurement_resp_t *msg);
bool decode_positioning_measurement_resp(const struct F1AP_F1AP_PDU *pdu, f1ap_positioning_measurement_resp_t *out);
f1ap_positioning_measurement_resp_t cp_positioning_measurement_resp(const f1ap_positioning_measurement_resp_t *orig);
bool eq_positioning_measurement_resp(const f1ap_positioning_measurement_resp_t *a, const f1ap_positioning_measurement_resp_t *b);
void free_positioning_measurement_resp(f1ap_positioning_measurement_resp_t *msg);

// positioning measurement failure
struct F1AP_F1AP_PDU *encode_positioning_measurement_failure(const f1ap_positioning_measurement_failure_t *msg);
bool decode_positioning_measurement_failure(const struct F1AP_F1AP_PDU *pdu, f1ap_positioning_measurement_failure_t *out);
f1ap_positioning_measurement_failure_t cp_positioning_measurement_failure(const f1ap_positioning_measurement_failure_t *orig);
bool eq_positioning_measurement_failure(const f1ap_positioning_measurement_failure_t *a,
                                        const f1ap_positioning_measurement_failure_t *b);
void free_positioning_measurement_failure(f1ap_positioning_measurement_failure_t *msg);

// positioning measurement report
struct F1AP_F1AP_PDU *encode_positioning_measurement_report(const f1ap_positioning_measurement_report_t *msg);
bool decode_positioning_measurement_report(const struct F1AP_F1AP_PDU *pdu, f1ap_positioning_measurement_report_t *out);
f1ap_positioning_measurement_report_t cp_positioning_measurement_report(const f1ap_positioning_measurement_report_t *orig);
bool eq_positioning_measurement_report(const f1ap_positioning_measurement_report_t *a,
                                       const f1ap_positioning_measurement_report_t *b);
void free_positioning_measurement_report(f1ap_positioning_measurement_report_t *msg);

// positioning measurement abort
struct F1AP_F1AP_PDU *encode_positioning_measurement_abort(const f1ap_positioning_measurement_abort_t *msg);
bool decode_positioning_measurement_abort(const struct F1AP_F1AP_PDU *pdu, f1ap_positioning_measurement_abort_t *out);
f1ap_positioning_measurement_abort_t cp_positioning_measurement_abort(const f1ap_positioning_measurement_abort_t *orig);
bool eq_positioning_measurement_abort(const f1ap_positioning_measurement_abort_t *a, const f1ap_positioning_measurement_abort_t *b);
void free_positioning_measurement_abort(f1ap_positioning_measurement_abort_t *msg);

// positioning measurement failure indication
struct F1AP_F1AP_PDU *encode_positioning_measurement_failure_indication(
    const f1ap_positioning_measurement_failure_indication_t *msg);
bool decode_positioning_measurement_failure_indication(const struct F1AP_F1AP_PDU *pdu,
                                                       f1ap_positioning_measurement_failure_indication_t *out);
f1ap_positioning_measurement_failure_indication_t cp_positioning_measurement_failure_indication(
    const f1ap_positioning_measurement_failure_indication_t *orig);
bool eq_positioning_measurement_failure_indication(const f1ap_positioning_measurement_failure_indication_t *a,
                                                   const f1ap_positioning_measurement_failure_indication_t *b);
void free_positioning_measurement_failure_indication(f1ap_positioning_measurement_failure_indication_t *msg);

// positioning measurement update
struct F1AP_F1AP_PDU *encode_positioning_measurement_update(const f1ap_positioning_measurement_update_t *msg);
bool decode_positioning_measurement_update(const struct F1AP_F1AP_PDU *pdu, f1ap_positioning_measurement_update_t *out);
f1ap_positioning_measurement_update_t cp_positioning_measurement_update(const f1ap_positioning_measurement_update_t *orig);
bool eq_positioning_measurement_update(const f1ap_positioning_measurement_update_t *a,
                                       const f1ap_positioning_measurement_update_t *b);
void free_positioning_measurement_update(f1ap_positioning_measurement_update_t *msg);
#endif /* F1AP_POSITIONING_H_ */
