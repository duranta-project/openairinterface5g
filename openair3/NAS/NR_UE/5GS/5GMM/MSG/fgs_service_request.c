/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "TLVEncoder.h"
#include "TLVDecoder.h"
#include "common/utils/eq_check.h"
#include "fgs_service_request.h"
#include "FGSMobileIdentity.h"
#include "NasKeySetIdentifier.h"
#include "ServiceType.h"

#define LEN_FGS_MOBILE_ID_CONTENTS_SIZE 2 // octets
#define MIN_LEN_FGS_SERVICE_REQUEST 10 // octets
#define TLV_IE_HEADER_LENGTH 2 // IEI + length
#define IEI_NULL 0x00

/** @brief Get the total encoded size of a (optional) TLV IE (IEI + length + value) */
static int get_tlv_ie_len(const uint8_t *buffer, uint32_t len)
{
  if (len < TLV_IE_HEADER_LENGTH)
    return -1;

  uint8_t ie_len = buffer[1];
  uint32_t total_ie_len = TLV_IE_HEADER_LENGTH + ie_len;
  if (len < total_ie_len)
    return -1;

  return total_ie_len;
}

/**
 * @brief Encode 5GMM NAS Service Request message (8.2.16 of 3GPP TS 24.501)
 */
int encode_fgs_service_request(uint8_t *buffer, const fgs_service_request_msg_t *service_request, uint32_t len)
{
  const uint32_t total_len = len;
  // Return if buffer is shorter than min length
  if (len < MIN_LEN_FGS_SERVICE_REQUEST)
    return -1;

  int encoded = 0;

  // Service type + ngKSI (1 octet) (M)
  *buffer = ((service_request->serviceType & 0x0f) << 4)
            | (encode_nas_key_set_identifier(&service_request->naskeysetidentifier, IEI_NULL) & 0x0f);
  encoded++;
  len -= 1;

  // 5GS Mobile Identity (M) type 5G-S-TMSI (9 octets)
  encoded += LEN_FGS_MOBILE_ID_CONTENTS_SIZE; // skip "Length of 5GS mobile identity contents" IE
  len -= LEN_FGS_MOBILE_ID_CONTENTS_SIZE;
  encoded += encode_stmsi_5gs_mobile_identity(buffer + encoded, &service_request->fiveg_s_tmsi, len);
  // encode length of 5GS mobile identity contents
  uint16_t tmp = htons(encoded - LEN_FGS_MOBILE_ID_CONTENTS_SIZE - 1);
  memcpy(buffer + 1, &tmp, sizeof(tmp));

  if (service_request->has_uplink_data_status) {
    byte_array_t ba = {.buf = buffer + encoded, .len = total_len - encoded};
    int encoded_rc = encode_pdu_session_ie(&ba, IEI_UPLINK_DATA_STATUS, service_request->uplink_data_status);
    if (encoded_rc < 0)
      return -1;
    encoded += encoded_rc;
  }

  if (service_request->has_pdu_session_status) {
    byte_array_t ba = {.buf = buffer + encoded, .len = total_len - encoded};
    int encoded_rc = encode_pdu_session_ie(&ba, IEI_PDU_SESSION_STATUS, service_request->pdu_session_status);
    if (encoded_rc < 0)
      return -1;
    encoded += encoded_rc;
  }

  if (service_request->fgsnasmessagecontainer != NULL) {
    int encoded_rc = encode_fgc_nas_message_container(service_request->fgsnasmessagecontainer,
                                                      IEI_FGS_NAS_MESSAGE_CONTAINER,
                                                      buffer + encoded,
                                                      total_len - encoded);
    if (encoded_rc < 0)
      return -1;
    encoded += encoded_rc;
  }

  return encoded;
}

/**
 * @brief Decode 5GMM NAS Service Request message (8.2.16 of 3GPP TS 24.501)
 */
int decode_fgs_service_request(fgs_service_request_msg_t *sr, const uint8_t *buffer, uint32_t len)
{
  uint32_t decoded = 0;
  int decoded_rc = 0;
  memset(sr, 0, sizeof(*sr));

  if (len < 1)
    return -1;

  // Service type (1/2 octet) (M)
  sr->serviceType = *buffer >> 4;
  // KSI (1/2 octet) (M)
  if ((decoded_rc = decode_nas_key_set_identifier(&sr->naskeysetidentifier, IEI_NULL, *buffer & 0x0f)) < 0) {
    return decoded_rc;
  }
  decoded++;

  // 5GS Mobile Identity (M) type 5G-S-TMSI (9 octets) (M)
  if (len < decoded + LEN_FGS_MOBILE_ID_CONTENTS_SIZE)
    return -1;
  decoded += LEN_FGS_MOBILE_ID_CONTENTS_SIZE; // skip "Length of 5GS mobile identity contents" IE
  if ((decoded_rc = decode_stmsi_5gs_mobile_identity(&sr->fiveg_s_tmsi, buffer + decoded, len - decoded)) < 0) {
    return decoded_rc;
  }
  decoded += decoded_rc;

  while (decoded < len) {
    uint8_t iei = buffer[decoded];

    switch (iei) {
      case IEI_UPLINK_DATA_STATUS: {
        decoded++;
        byte_array_t ba = create_byte_array(len - decoded, buffer + decoded);
        decoded_rc = decode_pdu_session_ie(sr->uplink_data_status, &ba);
        free_byte_array(ba);
        if (decoded_rc < 0) {
          LOG_E(NAS, "Failed to decode Uplink data status\n");
          return -1;
        }
        decoded += decoded_rc;
        sr->has_uplink_data_status = true;
        break;
      }

      case IEI_PDU_SESSION_STATUS: {
        decoded++;
        byte_array_t ba = create_byte_array(len - decoded, buffer + decoded);
        decoded_rc = decode_pdu_session_ie(sr->pdu_session_status, &ba);
        free_byte_array(ba);
        if (decoded_rc < 0) {
          LOG_E(NAS, "Failed to decode PDU Session Status\n");
          return -1;
        }
        decoded += decoded_rc;
        sr->has_pdu_session_status = true;
        break;
      }

      case IEI_FGS_NAS_MESSAGE_CONTAINER: {
        if (sr->fgsnasmessagecontainer != NULL)
          return -1;
        sr->fgsnasmessagecontainer = calloc_or_fail(1, sizeof(*sr->fgsnasmessagecontainer));
        byte_array_t ba = create_byte_array(len - decoded, buffer + decoded);
        decoded_rc = decode_fgc_nas_message_container(sr->fgsnasmessagecontainer, IEI_FGS_NAS_MESSAGE_CONTAINER, ba.buf, ba.len);
        free_byte_array(ba);
        if (decoded_rc < 0) {
          LOG_E(NAS, "Failed to decode FGS NAS Message Container\n");
          free(sr->fgsnasmessagecontainer);
          return -1;
        }
        decoded += decoded_rc;
        break;
      }

      case IEI_ALLOWED_PDU_SESSION_STATUS:
      case IEI_UE_REQUEST_TYPE:
      case IEI_PAGING_RESTRICTION:
        // Skip the TLV IE (IEI + length + value)
        LOG_D(NAS, "Skipping TLV IE 0x%02x\n", iei);
        decoded_rc = get_tlv_ie_len(buffer + decoded, len - decoded);
        if (decoded_rc < 0)
          return -1;
        decoded += decoded_rc;
        break;

      default:
        return -1;
    }
  }

  return decoded;
}

void free_fgs_service_request(fgs_service_request_msg_t *msg)
{
  DevAssert(msg);
  if (msg->fgsnasmessagecontainer) {
    free(msg->fgsnasmessagecontainer->nasmessagecontainercontents.value);
    msg->fgsnasmessagecontainer->nasmessagecontainercontents.length = 0;
    free(msg->fgsnasmessagecontainer);
  }
}

bool eq_fgs_service_request(const fgs_service_request_msg_t *a, const fgs_service_request_msg_t *b)
{
  _EQ_CHECK_INT(a->naskeysetidentifier.tsc, b->naskeysetidentifier.tsc);
  _EQ_CHECK_INT(a->naskeysetidentifier.naskeysetidentifier, b->naskeysetidentifier.naskeysetidentifier);
  _EQ_CHECK_INT(a->serviceType, b->serviceType);
  _EQ_CHECK_INT(a->fiveg_s_tmsi.digit1, b->fiveg_s_tmsi.digit1);
  _EQ_CHECK_INT(a->fiveg_s_tmsi.spare, b->fiveg_s_tmsi.spare);
  _EQ_CHECK_INT(a->fiveg_s_tmsi.typeofidentity, b->fiveg_s_tmsi.typeofidentity);
  _EQ_CHECK_INT(a->fiveg_s_tmsi.amfsetid, b->fiveg_s_tmsi.amfsetid);
  _EQ_CHECK_INT(a->fiveg_s_tmsi.amfpointer, b->fiveg_s_tmsi.amfpointer);
  _EQ_CHECK_UINT32(a->fiveg_s_tmsi.tmsi, b->fiveg_s_tmsi.tmsi);
  _EQ_CHECK_INT(a->has_uplink_data_status, b->has_uplink_data_status);
  if (a->has_uplink_data_status && b->has_uplink_data_status) {
    for (int i = 0; i < MAX_NUM_PSI; i++)
      _EQ_CHECK_INT(a->uplink_data_status[i], b->uplink_data_status[i]);
  }
  _EQ_CHECK_INT(a->has_pdu_session_status, b->has_pdu_session_status);
  if (a->has_pdu_session_status && b->has_pdu_session_status) {
    for (int i = 0; i < MAX_NUM_PSI; i++)
      _EQ_CHECK_INT(a->pdu_session_status[i], b->pdu_session_status[i]);
  }
  _EQ_CHECK_OPTIONAL_PTR(a, b, fgsnasmessagecontainer);
  if (a->fgsnasmessagecontainer != NULL) {
    const OctetString *a_contents = &a->fgsnasmessagecontainer->nasmessagecontainercontents;
    const OctetString *b_contents = &b->fgsnasmessagecontainer->nasmessagecontainercontents;
    _EQ_CHECK_UINT32(a_contents->length, b_contents->length);
    if (a_contents->length > 0)
      EQ_CHECK_GENERIC(memcmp(a_contents->value, b_contents->value, a_contents->length) == 0, "%d", 1, 0);
  }
  return true;
}
