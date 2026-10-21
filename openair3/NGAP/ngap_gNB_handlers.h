/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
 
/*!
 * \brief ngap messages handlers for gNB part
 */
 
#ifndef NGAP_GNB_HANDLERS_H_
#define NGAP_GNB_HANDLERS_H_

#include <stdint.h>
#include "ngap_gNB_defs.h"

struct ngap_error_indication_s;

void ngap_handle_ng_setup_message(ngap_gNB_amf_data_t *amf_desc_p, int sctp_shutdown);

int ngap_gNB_send_error_indication(sctp_assoc_t assoc_id, uint16_t stream, const struct ngap_error_indication_s *msg);

int ngap_gNB_handle_message(sctp_assoc_t assoc_id, int32_t stream, const uint8_t *const data, const uint32_t data_length);

#endif /* NGAP_GNB_HANDLERS_H_ */
