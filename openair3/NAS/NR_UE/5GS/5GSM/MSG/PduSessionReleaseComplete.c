/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "PduSessionReleaseComplete.h"
#include <string.h>

/**
 * @brief Encode PDU Session Release Complete (8.3.15 of 3GPP TS 24.501), including the 5GSM header
 * @return number of encoded bytes, or -1 if the buffer is too small
 */
int encode_pdu_session_release_complete(byte_array_t *buffer, const pdu_session_release_complete_msg_t *msg)
{
  // Extended protocol discriminator 0x2e (TS 24.501 9.2), message type 0xd4 (TS 24.501 Table 9.7.2)
  const uint8_t enc[] = {0x2e, msg->pdu_session_id, msg->pti, 0xd4};
  if (buffer->len < sizeof(enc))
    return -1;
  memcpy(buffer->buf, enc, sizeof(enc));
  return sizeof(enc);
}
