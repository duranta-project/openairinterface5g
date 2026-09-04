/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NRDC_MAC_RRC_DL_HANDLER_H
#define NRDC_MAC_RRC_DL_HANDLER_H

#include "f1ap_messages_types.h"

void nrdc_ue_context_setup_request(const f1ap_ue_context_setup_req_t *req);
void nrdc_ue_context_modification_request(const f1ap_ue_context_mod_req_t *req);

#endif /* NRDC_MAC_RRC_DL_HANDLER_H */
