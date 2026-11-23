/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef PDU_SESSION_RELEASE_COMPLETE_H_
#define PDU_SESSION_RELEASE_COMPLETE_H_

#include <stdint.h>
#include "common/utils/ds/byte_array.h"

/* PDU Session Release Complete message (UE to network) - TS 24.501 Table 8.3.15.1.1
 * The optional IEs (5GSM cause, Extended protocol configuration options) are not sent */
typedef struct pdu_session_release_complete_msg_s {
  uint8_t pdu_session_id;
  uint8_t pti;
} pdu_session_release_complete_msg_t;

int encode_pdu_session_release_complete(byte_array_t *buffer, const pdu_session_release_complete_msg_t *msg);

#endif /* PDU_SESSION_RELEASE_COMPLETE_H_ */
