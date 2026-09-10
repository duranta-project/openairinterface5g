/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef XNAP_GNB_ENCODER_H_
#define XNAP_GNB_ENCODER_H_

#include <stdint.h>
#include "lib/xnap_lib_includes.h"

int xnap_gNB_encode_pdu(XNAP_XnAP_PDU_t *pdu, uint8_t **buffer, uint32_t *len)
__attribute__((warn_unused_result));

#endif /* XNAP_GNB_ENCODER_H_ */
