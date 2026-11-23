/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "PduSessionReleaseCommand.h"
#include "common/utils/LOG/log.h"

/**
 * @brief Decode PDU Session Release Command (8.3.14 of 3GPP TS 24.501)
 * @param buffer the message after the 5GSM header
 * @return number of decoded bytes, or -1 if the message is truncated
 */
int decode_pdu_session_release_command(pdu_session_release_command_msg_t *msg, const byte_array_t *buffer)
{
  if (buffer->len < 1) {
    LOG_E(NAS, "PDU Session Release Command: missing 5GSM cause\n");
    return -1;
  }
  size_t pos = 0;
  msg->cause = buffer->buf[pos++];

  // The UE does not use the optional IEs: skip each one by the format its IEI encodes (TS 24.007 clause 11.2.4)
  while (pos < buffer->len) {
    const uint8_t iei = buffer->buf[pos];
    size_t ie_len = 1; // type 1 or type 2 (IEI bit 8 set): one octet
    if ((iei & 0xf0) == 0x70) {
      if (pos + 3 > buffer->len)
        return -1;
      ie_len = 3 + (buffer->buf[pos + 1] << 8 | buffer->buf[pos + 2]); // TLV-E
    } else if (!(iei & 0x80)) {
      if (pos + 2 > buffer->len)
        return -1;
      ie_len = 2 + buffer->buf[pos + 1]; // TLV
    }
    if (pos + ie_len > buffer->len) {
      LOG_E(NAS, "PDU Session Release Command: IEI 0x%02x exceeds the message\n", iei);
      return -1;
    }
    pos += ie_len;
  }
  return pos;
}
