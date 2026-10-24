/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "fgmm_configuration_update_command.h"
#include <stdlib.h>
#include "common/utils/utils.h"
#include "common/utils/eq_check.h"
#include "fgs_nas_utils.h"

#define IEI_CONFIGURATION_UPDATE_INDICATION 0xD
#define IEI_5G_GUTI 0x77
#define IEI_LOCAL_TIME_ZONE 0x46
#define IEI_UNIVERSAL_TIME_AND_LOCAL_TIME_ZONE 0x47
#define CONFIGURATION_UPDATE_ACK 0x01
#define CONFIGURATION_UPDATE_RED 0x02

/** @brief Decode Configuration Update Command (8.2.19 of 3GPP TS 24.501)
 *  All IEs are optional. The Configuration update indication and the 5G-GUTI are decoded,
 *  other IEs are not used by the UE and are skipped by their format (TS 24.007 11.2.4). */
int decode_fgmm_configuration_update_command(fgmm_configuration_update_command_t *msg, const byte_array_t *buffer)
{
  *msg = (fgmm_configuration_update_command_t){0};
  uint32_t decoded = 0;
  while (decoded < buffer->len) {
    const uint8_t *ie = buffer->buf + decoded;
    const uint32_t left = buffer->len - decoded;
    uint32_t ie_len;
    if (ie[0] & 0x80) { // type 1 IE: 4-bit IEI and 4-bit value
      if ((ie[0] >> 4) == IEI_CONFIGURATION_UPDATE_INDICATION) {
        msg->ack_requested = ie[0] & CONFIGURATION_UPDATE_ACK;
        msg->registration_requested = ie[0] & CONFIGURATION_UPDATE_RED;
      }
      ie_len = 1;
    } else if (ie[0] == IEI_LOCAL_TIME_ZONE) { // TV
      ie_len = 2;
    } else if (ie[0] == IEI_UNIVERSAL_TIME_AND_LOCAL_TIME_ZONE) { // TV
      ie_len = 8;
    } else if ((ie[0] & 0xf0) == 0x70) { // TLV-E
      if (left < 3) {
        PRINT_ERROR("Configuration Update Command: truncated TLV-E IE 0x%02x\n", ie[0]);
        return -1;
      }
      ie_len = 3 + ((ie[1] << 8) | ie[2]);
      if (ie[0] == IEI_5G_GUTI && ie_len <= left) {
        FGSMobileIdentity mi = {0};
        if (decode_5gs_mobile_identity(&mi, IEI_5G_GUTI, ie, ie_len) < 0) {
          PRINT_ERROR("Configuration Update Command: failed to decode the 5G-GUTI\n");
          return -1;
        }
        if (mi.guti.typeofidentity == FGS_MOBILE_IDENTITY_5G_GUTI) {
          free(msg->guti);
          msg->guti = malloc_or_fail(sizeof(*msg->guti));
          *msg->guti = mi;
        }
      }
    } else { // TLV
      if (left < 2) {
        PRINT_ERROR("Configuration Update Command: truncated TLV IE 0x%02x\n", ie[0]);
        return -1;
      }
      ie_len = 2 + ie[1];
    }
    if (ie_len > left) {
      PRINT_ERROR("Configuration Update Command: IE 0x%02x exceeds the message\n", ie[0]);
      return -1;
    }
    decoded += ie_len;
  }
  return decoded;
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
  if (a->guti) {
    _EQ_CHECK_INT(a->guti->guti.typeofidentity, b->guti->guti.typeofidentity);
    _EQ_CHECK_INT(a->guti->guti.mccdigit1, b->guti->guti.mccdigit1);
    _EQ_CHECK_INT(a->guti->guti.mccdigit2, b->guti->guti.mccdigit2);
    _EQ_CHECK_INT(a->guti->guti.mccdigit3, b->guti->guti.mccdigit3);
    _EQ_CHECK_INT(a->guti->guti.mncdigit1, b->guti->guti.mncdigit1);
    _EQ_CHECK_INT(a->guti->guti.mncdigit2, b->guti->guti.mncdigit2);
    _EQ_CHECK_INT(a->guti->guti.mncdigit3, b->guti->guti.mncdigit3);
    _EQ_CHECK_INT(a->guti->guti.amfregionid, b->guti->guti.amfregionid);
    _EQ_CHECK_INT(a->guti->guti.amfsetid, b->guti->guti.amfsetid);
    _EQ_CHECK_INT(a->guti->guti.amfpointer, b->guti->guti.amfpointer);
    _EQ_CHECK_INT(a->guti->guti.tmsi, b->guti->guti.tmsi);
  }
  return true;
}

void free_fgmm_configuration_update_command(fgmm_configuration_update_command_t *msg)
{
  free(msg->guti);
  msg->guti = NULL;
}
