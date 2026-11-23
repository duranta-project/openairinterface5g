/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef PDU_SESSION_RELEASE_REJECT_H_
#define PDU_SESSION_RELEASE_REJECT_H_

#include <stdint.h>
#include "common/utils/ds/byte_array.h"

/* PDU Session Release Reject message (network to UE) - TS 24.501 Table 8.3.13.1.1 */
typedef struct pdu_session_release_reject_msg_s {
  // 5GSM cause (M) - TS 24.501 9.11.4.2
  uint8_t cause;
} pdu_session_release_reject_msg_t;

int decode_pdu_session_release_reject(pdu_session_release_reject_msg_t *msg, const byte_array_t *buffer);

#endif /* PDU_SESSION_RELEASE_REJECT_H_ */
