/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#ifndef F1AP_DU_POSITIONING_H_
#define F1AP_DU_POSITIONING_H_

int DU_handle_POS_ASSISTANCE_CONTROL(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu);

int DU_handle_TRP_INFORMATION_REQUEST(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu);
int DU_send_TRP_INFORMATION_RESPONSE(sctp_assoc_t assoc_id, f1ap_trp_information_resp_t *resp);
int DU_send_TRP_INFORMATION_FAILURE(sctp_assoc_t assoc_id, f1ap_trp_information_failure_t *fail);
int DU_handle_POSITIONING_INFORMATION_REQUEST(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu);
int DU_send_POSITIONING_INFORMATION_RESPONSE(sctp_assoc_t assoc_id, f1ap_positioning_information_resp_t *resp);
int DU_handle_POSITIONING_ACTIVATION_REQUEST(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu);
int DU_send_POSITIONING_ACTIVATION_RESPONSE(sctp_assoc_t assoc_id, f1ap_positioning_activation_resp_t *resp);
int DU_handle_POSITIONING_MEASUREMENT_REQUEST(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu);
int DU_send_POSITIONING_MEASUREMENT_RESPONSE(sctp_assoc_t assoc_id, f1ap_positioning_measurement_resp_t *resp);
int DU_send_POSITIONING_MEASUREMENT_FAILURE(sctp_assoc_t assoc_id, f1ap_positioning_measurement_failure_t *fail);

#endif /* F1AP_DU_POSITIONING_H_ */
