/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef F1AP_MBS_H_
#define F1AP_MBS_H_

#include <stdbool.h>
#include "common/platform_types.h"
#include "f1ap_messages_types.h"

struct F1AP_F1AP_PDU;

struct F1AP_F1AP_PDU *encode_f1ap_broadcast_context_setup_request(const f1ap_broadcast_ctx_setup_t *in);
bool decode_f1ap_broadcast_context_setup_request(f1ap_broadcast_ctx_setup_t *out, const struct F1AP_F1AP_PDU *pdu);

void CU_send_BROADCAST_CONTEXT_SETUP_REQUEST(sctp_assoc_t assoc_id, const f1ap_broadcast_ctx_setup_t *req);
int DU_handle_BROADCAST_CONTEXT_SETUP_REQUEST(instance_t instance,
                                              sctp_assoc_t assoc_id,
                                              uint32_t stream,
                                              struct F1AP_F1AP_PDU *pdu);

#endif /* F1AP_MBS_H_ */
