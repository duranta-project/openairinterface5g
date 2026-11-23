/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "PduSessionReleaseRequest.h"
#include <string.h>

#define IEI_5GSM_CAUSE 0x59 // TS 24.501 Table 8.3.12.1.1

/**
 * @brief Encode PDU Session Release Request (8.3.12 of 3GPP TS 24.501), including the 5GSM header
 * @return number of encoded bytes, or -1 if the buffer is too small
 */
int encode_pdu_session_release_request(byte_array_t *buffer, const pdu_session_release_request_msg_t *msg)
{
  // Extended protocol discriminator 0x2e (TS 24.501 9.2), message type 0xd1 (TS 24.501 Table 9.7.2)
  uint8_t enc[6] = {0x2e, msg->pdu_session_id, msg->pti, 0xd1};
  size_t len = 4;
  if (msg->cause_present) {
    enc[len++] = IEI_5GSM_CAUSE;
    enc[len++] = msg->cause;
  }
  if (buffer->len < len)
    return -1;
  memcpy(buffer->buf, enc, len);
  return len;
}
