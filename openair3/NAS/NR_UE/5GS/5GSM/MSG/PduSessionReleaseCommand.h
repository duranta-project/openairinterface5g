/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef PDU_SESSION_RELEASE_COMMAND_H_
#define PDU_SESSION_RELEASE_COMMAND_H_

#include <stdint.h>
#include "common/utils/ds/byte_array.h"

/* PDU Session Release Command message (network to UE) - TS 24.501 Table 8.3.14.1.1 */
typedef struct pdu_session_release_command_msg_s {
  // 5GSM cause (M) - TS 24.501 9.11.4.2
  uint8_t cause;
} pdu_session_release_command_msg_t;

int decode_pdu_session_release_command(pdu_session_release_command_msg_t *msg, const byte_array_t *buffer);

#endif /* PDU_SESSION_RELEASE_COMMAND_H_ */
