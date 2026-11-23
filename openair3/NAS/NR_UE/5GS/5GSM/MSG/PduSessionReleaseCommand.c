/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "PduSessionReleaseCommand.h"
#include "fgsm_lib.h"

/**
 * @brief Decode PDU Session Release Command (8.3.14 of 3GPP TS 24.501)
 * @param buffer the message after the 5GSM header
 * @return number of decoded bytes, or -1 if the message is truncated
 */
int decode_pdu_session_release_command(pdu_session_release_command_msg_t *msg, const byte_array_t *buffer)
{
  return decode_5gsm_cause_skip_optional_ies(&msg->cause, buffer, "PDU Session Release Command");
}
