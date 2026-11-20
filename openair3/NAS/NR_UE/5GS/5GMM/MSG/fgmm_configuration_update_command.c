/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "fgmm_configuration_update_command.h"
#include <stdlib.h>
#include "common/utils/utils.h"
#include "common/utils/eq_check.h"
#include "fgs_nas_utils.h"
#include "fgmm_lib.h"

#define IEI_CONFIGURATION_UPDATE_INDICATION 0xD
#define IEI_LOCAL_TIME_ZONE 0x46
#define IEI_UNIVERSAL_TIME_AND_LOCAL_TIME_ZONE 0x47
#define CONFIGURATION_UPDATE_ACK 0x01
#define CONFIGURATION_UPDATE_RED 0x02

/** @brief Length of the IE at the start of the buffer, IEI included, by its format (TS 24.007 11.2.4):
 *         type 1 (IEI bit 8 set), TV with a fixed length, TLV-E (IEI 0x7-), TLV otherwise.
 *  @return IE length, or -1 if the IE does not fit in the buffer */
static int configuration_update_command_ie_len(const byte_array_t *ba)
{
  const uint8_t iei = ba->buf[0];
  uint32_t len;
  if (iei & 0x80) { // type 1: 4-bit IEI and 4-bit value
    len = 1;
  } else {
    switch (iei) {
      case IEI_LOCAL_TIME_ZONE: // TV
        len = 2;
        break;
      case IEI_UNIVERSAL_TIME_AND_LOCAL_TIME_ZONE: // TV
        len = 8;
        break;
      default:
        if ((iei & 0xf0) == 0x70) { // TLV-E
          if (ba->len < 3)
            return -1;
          len = 3 + ((ba->buf[1] << 8) | ba->buf[2]);
        } else { // TLV
          if (ba->len < 2)
            return -1;
          len = 2 + ba->buf[1];
        }
        break;
    }
  }
  return len <= ba->len ? (int)len : -1;
}

/** @brief Decode Configuration Update Command (8.2.19 of 3GPP TS 24.501)
 *  All IEs are optional. The Configuration update indication and the 5G-GUTI are decoded, other IEs are not used
 *  by the UE and are skipped by their format. An optional IE that is syntactically incorrect is treated as not
 *  present (TS 24.501 7.7.1); of a repeated IE only the first one is handled (TS 24.501 7.6.3).
 *  @return number of decoded bytes */
int decode_fgmm_configuration_update_command(fgmm_configuration_update_command_t *msg, const byte_array_t *buffer)
{
  *msg = (fgmm_configuration_update_command_t){0};
  byte_array_t ba = *buffer; // local copy of buffer
  bool seen_indication = false;
  bool seen_guti = false;
  while (ba.len > 0) {
    const uint8_t iei = ba.buf[0];
    const int ie_len = configuration_update_command_ie_len(&ba);
    if (ie_len < 0) {
      // the IE exceeds the message: it and whatever follows cannot be parsed and is treated as not present
      PRINT_ERROR("Configuration Update Command: IE 0x%02x exceeds the message, ignoring the remaining %ld bytes\n",
                  iei,
                  (long)ba.len);
      break;
    }
    switch (iei & 0x80 ? iei >> 4 : iei) {
      case IEI_CONFIGURATION_UPDATE_INDICATION:
        if (seen_indication) {
          PRINT_ERROR("Ignoring repeated Configuration Update Command IEI 0x%x\n", IEI_CONFIGURATION_UPDATE_INDICATION);
          break;
        }
        seen_indication = true;
        msg->ack_requested = iei & CONFIGURATION_UPDATE_ACK;
        msg->registration_requested = iei & CONFIGURATION_UPDATE_RED;
        break;

      case IEI_5G_GUTI: {
        if (seen_guti) {
          PRINT_ERROR("Ignoring repeated Configuration Update Command IEI 0x%02x\n", IEI_5G_GUTI);
          break;
        }
        seen_guti = true;
        FGSMobileIdentity mi = {0};
        if (decode_5gs_mobile_identity(&mi, IEI_5G_GUTI, ba.buf, ie_len) < 0
            || mi.guti.typeofidentity != FGS_MOBILE_IDENTITY_5G_GUTI) {
          PRINT_ERROR("Configuration Update Command: syntactically incorrect 5G-GUTI treated as not present\n");
          break;
        }
        msg->guti = malloc_or_fail(sizeof(*msg->guti));
        *msg->guti = mi;
        break;
      }

      default: // IE not used by the UE
        break;
    }
    UPDATE_BYTE_ARRAY(ba, ie_len);
  }
  return buffer->len - ba.len;
}

/** @brief Encode Configuration Update Command (8.2.19 of 3GPP TS 24.501), the IEs of the struct only */
int encode_fgmm_configuration_update_command(byte_array_t *buffer, const fgmm_configuration_update_command_t *msg)
{
  uint32_t encoded = 0;
  if (msg->ack_requested || msg->registration_requested) {
    if (buffer->len < 1)
      return -1;
    buffer->buf[encoded++] = (IEI_CONFIGURATION_UPDATE_INDICATION << 4) | (msg->ack_requested ? CONFIGURATION_UPDATE_ACK : 0)
                             | (msg->registration_requested ? CONFIGURATION_UPDATE_RED : 0);
  }
  if (msg->guti) {
    int enc = encode_5gs_mobile_identity(msg->guti, IEI_5G_GUTI, buffer->buf + encoded, buffer->len - encoded);
    if (enc < 0)
      return -1;
    encoded += enc;
  }
  return encoded;
}

bool eq_fgmm_configuration_update_command(const fgmm_configuration_update_command_t *a,
                                          const fgmm_configuration_update_command_t *b)
{
  _EQ_CHECK_INT(a->ack_requested, b->ack_requested);
  _EQ_CHECK_INT(a->registration_requested, b->registration_requested);
  if (!a->guti != !b->guti) {
    PRINT_ERROR("NAS Equality Check failure: One of the two GUTIs is NULL\n");
    return false;
  }
  if (a->guti && !eq_fgs_guti(&a->guti->guti, &b->guti->guti))
    return false;
  return true;
}

void free_fgmm_configuration_update_command(fgmm_configuration_update_command_t *msg)
{
  free(msg->guti);
  msg->guti = NULL;
}
