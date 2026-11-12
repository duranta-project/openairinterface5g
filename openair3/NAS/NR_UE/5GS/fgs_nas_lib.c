/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "NR_NAS_defs.h"
#include "nas_log.h"
#include "TLVEncoder.h"
#include "common/utils/eq_check.h"
#include "fgs_nas_utils.h"

int _nas_mm_msg_encode_header(const fgmm_msg_header_t *header, uint8_t *buffer, uint32_t len)
{
  int size = 0;

  /* Check the buffer length */
  if (len < sizeof(fgmm_msg_header_t)) {
    return (TLV_ENCODE_BUFFER_TOO_SHORT);
  }

  /* Check the protocol discriminator */
  if (header->ex_protocol_discriminator != FGS_MOBILITY_MANAGEMENT_MESSAGE) {
    LOG_TRACE(ERROR, "Unexpected extened protocol discriminator: 0x%x", header->ex_protocol_discriminator);
    return (TLV_ENCODE_PROTOCOL_NOT_SUPPORTED);
  }

  /* Encode the extendedprotocol discriminator */
  ENCODE_U8(buffer + size, header->ex_protocol_discriminator, size);
  /* Encode the security header type */
  ENCODE_U8(buffer + size, (header->security_header_type & 0xf), size);
  /* Encode the message type */
  ENCODE_U8(buffer + size, header->message_type, size);
  return (size);
}

int mm_msg_encode(const fgmm_nas_message_plain_t *p, uint8_t *buffer, uint32_t len)
{
  uint8_t msg_type = p->header.message_type;
  int enc_msg = 0;
  /* First encode the 5GMM message header */
  int enc_header = _nas_mm_msg_encode_header(&p->header, buffer, len);

  if (enc_header < 0) {
    PRINT_ERROR("Failed to encode 5GMM message header\n");
    return enc_header;
  }

  buffer += enc_header;
  len -= enc_header;

  switch (msg_type) {
    case FGS_REGISTRATION_REQUEST:
      enc_msg = encode_registration_request(&p->mm_msg.registration_request, buffer, len);
      break;
    case FGS_IDENTITY_RESPONSE:
      enc_msg = encode_fgmm_identity_response(buffer, &p->mm_msg.fgs_identity_response, len);
      break;
    case FGS_AUTHENTICATION_RESPONSE:
      enc_msg = encode_fgs_authentication_response(&p->mm_msg.fgs_auth_response, buffer, len);
      break;
    case FGS_AUTHENTICATION_FAILURE: {
      byte_array_t ba = {.buf = buffer, .len = len};
      enc_msg = encode_fgmm_auth_failure(&ba, &p->mm_msg.fgmm_auth_failure);
      break;
    }
    case FGS_SECURITY_MODE_COMPLETE:
      enc_msg = encode_fgs_security_mode_complete(&p->mm_msg.fgs_security_mode_complete, buffer, len);
      break;
    case FGS_SECURITY_MODE_REJECT: {
      byte_array_t ba = {.buf = buffer, .len = len};
      enc_msg = encode_fgs_security_mode_reject(&ba, &p->mm_msg.fgs_security_mode_reject);
      break;
    }
    case FGS_UPLINK_NAS_TRANSPORT:
      enc_msg = encode_fgs_uplink_nas_transport(&p->mm_msg.uplink_nas_transport, buffer, len);
      break;
    case FGS_DEREGISTRATION_REQUEST_UE_ORIGINATING:
      enc_msg =
          encode_fgs_deregistration_request_ue_originating(&p->mm_msg.fgs_deregistration_request_ue_originating, buffer, len);
      break;
    case FGS_SERVICE_REQUEST:
      enc_msg = encode_fgs_service_request(buffer, &p->mm_msg.service_request, len);
      break;
    default:
      LOG_TRACE(ERROR, " Unexpected message type: 0x%x", p->header.message_type);
      enc_msg = TLV_ENCODE_WRONG_MESSAGE_TYPE;
      break;
      /* TODO: Handle not standard layer 3 messages: SERVICE_REQUEST */
  }

  if (enc_msg < 0) {
    PRINT_ERROR("Failed to encode 5GMM message\n");
    return enc_msg;
  }

  return enc_header + enc_msg;
}

int nas_protected_security_header_encode(uint8_t *buffer, const fgs_nas_message_security_header_t *header, int length)
{
  int size = 0;

  if (length < sizeof(fgs_nas_message_security_header_t)) {
    PRINT_ERROR("Could not encode the NAS security header\n");
    return -1;
  }

  /* Encode the protocol discriminator) */
  ENCODE_U8(buffer, header->protocol_discriminator, size);

  /* Encode the security header type */
  ENCODE_U8(buffer + size, (header->security_header_type & 0xf), size);

  /* Encode the message authentication code */
  ENCODE_U32(buffer + size, header->message_authentication_code, size);
  /* Encode the sequence number */
  ENCODE_U8(buffer + size, header->sequence_number, size);

  return size;
}

/**
 * @brief Decode plain 5GMM header from buffer and return fgmm_msg_header_t instance
 */
int decode_5gmm_msg_header(fgmm_msg_header_t *mm_header, const uint8_t *buffer, uint32_t len)
{
  if (len < sizeof(fgmm_msg_header_t)) {
    PRINT_ERROR("Failed to decode plain 5GMM header: buffer length too short\n");
    return -1;
  }
  mm_header->ex_protocol_discriminator = *buffer++;
  mm_header->security_header_type = *buffer++;
  mm_header->message_type = *buffer++;
  return sizeof(*mm_header);
}

/**
 * @brief Decode plain 5GSM header from buffer and return fgsm_msg_header_t instance
 */
int decode_5gsm_msg_header(fgsm_msg_header_t *sm_header, const uint8_t *buffer, uint32_t len)
{
  if (len < sizeof(fgsm_msg_header_t)) {
    PRINT_ERROR("Failed to decode plain 5GSM header: buffer length too short\n");
    return -1;
  }
  sm_header->ex_protocol_discriminator = *buffer++;
  sm_header->pdu_session_id = *buffer++;
  sm_header->pti = *buffer++;
  sm_header->message_type = *buffer++;
  return sizeof(*sm_header);
}

/**
 * @brief Decode security protected 5GS header from buffer and return fgs_nas_message_security_header_t instance
 */
int decode_5gs_security_protected_header(fgs_nas_message_security_header_t *header, const uint8_t *buf, uint32_t len)
{
  int decoded = 0;
  if (len < sizeof(fgs_nas_message_security_header_t)) {
    PRINT_ERROR("Failed to decode security protected 5GS header: buffer length too short\n");
    return -1;
  }
  header->protocol_discriminator = *buf++;
  decoded++;
  header->security_header_type = *buf++;
  decoded++;
  uint32_t tmp;
  memcpy(&tmp, buf, sizeof(tmp));
  decoded += sizeof(tmp);
  header->message_authentication_code = htonl(tmp);
  header->sequence_number = *buf++;
  decoded++;
  return decoded;
}
