/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef PDU_SESSION_RELEASE_REQUEST_H_
#define PDU_SESSION_RELEASE_REQUEST_H_

#include <stdbool.h>
#include <stdint.h>
#include "common/utils/ds/byte_array.h"

/* PDU Session Release Request message (UE to network) - TS 24.501 Table 8.3.12.1.1
 * The optional Extended protocol configuration options IE is not sent */
typedef struct pdu_session_release_request_msg_s {
  uint8_t pdu_session_id;
  uint8_t pti;
  // 5GSM cause (O) - TS 24.501 9.11.4.2
  bool cause_present;
  uint8_t cause;
} pdu_session_release_request_msg_t;

int encode_pdu_session_release_request(byte_array_t *buffer, const pdu_session_release_request_msg_t *msg);

#endif /* PDU_SESSION_RELEASE_REQUEST_H_ */
