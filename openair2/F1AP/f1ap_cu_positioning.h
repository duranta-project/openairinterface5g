/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#ifndef F1AP_CU_POSITIONING_H_
#define F1AP_CU_POSITIONING_H_

int CU_send_TRP_INFORMATION_REQUEST(sctp_assoc_t assoc_id, f1ap_trp_information_req_t *req);
int CU_send_POS_ASSISTANCE_CONTROL(sctp_assoc_t assoc_id, const f1ap_pos_assistance_control_t *control);
int CU_handle_TRP_INFORMATION_RESPONSE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu);
int CU_handle_TRP_INFORMATION_FAILURE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu);
int CU_send_POSITIONING_INFORMATION_REQUEST(sctp_assoc_t assoc_id, f1ap_positioning_information_req_t *req);
int CU_handle_POSITIONING_INFORMATION_RESPONSE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu);
int CU_send_POSITIONING_ACTIVATION_REQUEST(sctp_assoc_t assoc_id, f1ap_positioning_activation_req_t *req);
int CU_handle_POSITIONING_ACTIVATION_RESPONSE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu);
int CU_send_POSITIONING_MEASUREMENT_REQUEST(sctp_assoc_t assoc_id, f1ap_positioning_measurement_req_t *req);
int CU_handle_POSITIONING_MEASUREMENT_RESPONSE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu);
int CU_handle_POSITIONING_MEASUREMENT_FAILURE(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu);

#endif /* F1AP_CU_POSITIONING_H_ */
