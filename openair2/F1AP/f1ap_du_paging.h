/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef F1AP_DU_PAGING_H_
#define F1AP_DU_PAGING_H_

#include <stdint.h>
#include "f1ap_common.h"
#include "f1ap_messages_types.h"

/* Paging (gNB-CU initiated) */
int DU_handle_Paging(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *pdu);

#endif /* F1AP_DU_PAGING_H_ */
