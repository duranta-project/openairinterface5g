/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef F1AP_UE_CONTEXT_SETUP_REQ_H_
#define F1AP_UE_CONTEXT_SETUP_REQ_H_

#include <stdbool.h>
#include "f1ap_messages_types.h"

struct F1AP_F1AP_PDU;

struct F1AP_F1AP_PDU *encode_ue_context_setup_req(const f1ap_ue_context_setup_req_t *msg);
bool decode_ue_context_setup_req(const struct F1AP_F1AP_PDU *pdu, f1ap_ue_context_setup_req_t *out);
f1ap_ue_context_setup_req_t cp_ue_context_setup_req(const f1ap_ue_context_setup_req_t *orig);
bool eq_ue_context_setup_req(const f1ap_ue_context_setup_req_t *a, const f1ap_ue_context_setup_req_t *b);
void free_ue_context_setup_req(f1ap_ue_context_setup_req_t *req);

struct F1AP_F1AP_PDU *encode_ue_context_setup_resp(const f1ap_ue_context_setup_resp_t *msg);
bool decode_ue_context_setup_resp(const struct F1AP_F1AP_PDU *pdu, f1ap_ue_context_setup_resp_t *out);
f1ap_ue_context_setup_resp_t cp_ue_context_setup_resp(const f1ap_ue_context_setup_resp_t *orig);
bool eq_ue_context_setup_resp(const f1ap_ue_context_setup_resp_t *a, const f1ap_ue_context_setup_resp_t *b);
void free_ue_context_setup_resp(f1ap_ue_context_setup_resp_t *resp);

struct F1AP_F1AP_PDU *encode_ue_context_mod_req(const f1ap_ue_context_mod_req_t *msg);
bool decode_ue_context_mod_req(const struct F1AP_F1AP_PDU *pdu, f1ap_ue_context_mod_req_t *out);
f1ap_ue_context_mod_req_t cp_ue_context_mod_req(const f1ap_ue_context_mod_req_t *orig);
bool eq_ue_context_mod_req(const f1ap_ue_context_mod_req_t *a, const f1ap_ue_context_mod_req_t *b);
void free_ue_context_mod_req(f1ap_ue_context_mod_req_t *req);

struct F1AP_F1AP_PDU *encode_ue_context_mod_resp(const f1ap_ue_context_mod_resp_t *msg);
bool decode_ue_context_mod_resp(const struct F1AP_F1AP_PDU *pdu, f1ap_ue_context_mod_resp_t *out);
f1ap_ue_context_mod_resp_t cp_ue_context_mod_resp(const f1ap_ue_context_mod_resp_t *orig);
bool eq_ue_context_mod_resp(const f1ap_ue_context_mod_resp_t *a, const f1ap_ue_context_mod_resp_t *b);
void free_ue_context_mod_resp(f1ap_ue_context_mod_resp_t *resp);

struct F1AP_F1AP_PDU *encode_ue_context_modif_required(const f1ap_ue_context_modif_required_t *msg);
bool decode_ue_context_modif_required(const struct F1AP_F1AP_PDU *pdu, f1ap_ue_context_modif_required_t *out);
f1ap_ue_context_modif_required_t cp_ue_context_modif_required(const f1ap_ue_context_modif_required_t *orig);
bool eq_ue_context_modif_required(const f1ap_ue_context_modif_required_t *a, const f1ap_ue_context_modif_required_t *b);
void free_ue_context_modif_required(f1ap_ue_context_modif_required_t *required);

struct F1AP_F1AP_PDU *encode_ue_context_modif_confirm(const f1ap_ue_context_modif_confirm_t *msg);
bool decode_ue_context_modif_confirm(const struct F1AP_F1AP_PDU *pdu, f1ap_ue_context_modif_confirm_t *out);
f1ap_ue_context_modif_confirm_t cp_ue_context_modif_confirm(const f1ap_ue_context_modif_confirm_t *orig);
bool eq_ue_context_modif_confirm(const f1ap_ue_context_modif_confirm_t *a, const f1ap_ue_context_modif_confirm_t *b);
void free_ue_context_modif_confirm(f1ap_ue_context_modif_confirm_t *confirm);

struct F1AP_F1AP_PDU *encode_ue_context_rel_req(const f1ap_ue_context_rel_req_t *msg);
bool decode_ue_context_rel_req(const struct F1AP_F1AP_PDU *pdu, f1ap_ue_context_rel_req_t *out);
f1ap_ue_context_rel_req_t cp_ue_context_rel_req(const f1ap_ue_context_rel_req_t *orig);
bool eq_ue_context_rel_req(const f1ap_ue_context_rel_req_t *a, const f1ap_ue_context_rel_req_t *b);
void free_ue_context_rel_req(f1ap_ue_context_rel_req_t *req);

struct F1AP_F1AP_PDU *encode_ue_context_rel_cmd(const f1ap_ue_context_rel_cmd_t *msg);
bool decode_ue_context_rel_cmd(const struct F1AP_F1AP_PDU *pdu, f1ap_ue_context_rel_cmd_t *out);
f1ap_ue_context_rel_cmd_t cp_ue_context_rel_cmd(const f1ap_ue_context_rel_cmd_t *orig);
bool eq_ue_context_rel_cmd(const f1ap_ue_context_rel_cmd_t *a, const f1ap_ue_context_rel_cmd_t *b);
void free_ue_context_rel_cmd(f1ap_ue_context_rel_cmd_t *cmd);

struct F1AP_F1AP_PDU *encode_ue_context_rel_cplt(const f1ap_ue_context_rel_cplt_t *msg);
bool decode_ue_context_rel_cplt(const struct F1AP_F1AP_PDU *pdu, f1ap_ue_context_rel_cplt_t *out);
f1ap_ue_context_rel_cplt_t cp_ue_context_rel_cplt(const f1ap_ue_context_rel_cplt_t *orig);
bool eq_ue_context_rel_cplt(const f1ap_ue_context_rel_cplt_t *a, const f1ap_ue_context_rel_cplt_t *b);
void free_ue_context_rel_cplt(f1ap_ue_context_rel_cplt_t *cplt);

f1ap_qos_flow_param_t cp_qos_flow_param(const f1ap_qos_flow_param_t *orig);

#endif /* F1AP_UE_CONTEXT_SETUP_REQ_H_ */
