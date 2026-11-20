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
#define CONFIGURATION_UPDATE_ACK 0x01
#define CONFIGURATION_UPDATE_RED 0x02

/** @brief Decode Configuration Update Command (8.2.19 of 3GPP TS 24.501)
 *  All IEs are optional. The UE uses only the Configuration update indication and the 5G-GUTI, the first two
 *  optional IEs of table 8.2.19.1.1. IEs appear in table order (TS 24.007 11.2.6), so decoding stops at the first
 *  other IE: anything after the 5G-GUTI is unused, a repetition (TS 24.501 7.6.3) or out of sequence (7.6.2).
 *  A syntactically incorrect 5G-GUTI is treated as not present (TS 24.501 7.7.1).
 *  @return number of decoded bytes */
int decode_fgmm_configuration_update_command(fgmm_configuration_update_command_t *msg, const byte_array_t *buffer)
{
  *msg = (fgmm_configuration_update_command_t){0};
  byte_array_t ba = *buffer; // local copy of buffer
  bool seen_indication = false;
  while (ba.len > 0) {
    const uint8_t iei = ba.buf[0];
    switch (iei & 0x80 ? iei >> 4 : iei) {
      case IEI_CONFIGURATION_UPDATE_INDICATION: // type 1
        if (seen_indication) {
          PRINT_ERROR("Ignoring repeated Configuration Update Command IEI 0x%x\n", IEI_CONFIGURATION_UPDATE_INDICATION);
        } else {
          seen_indication = true;
          msg->ack_requested = iei & CONFIGURATION_UPDATE_ACK;
          msg->registration_requested = iei & CONFIGURATION_UPDATE_RED;
        }
        UPDATE_BYTE_ARRAY(ba, 1);
        break;

      case IEI_5G_GUTI: {
        FGSMobileIdentity mi = {0};
        const int decoded = decode_5gs_mobile_identity(&mi, IEI_5G_GUTI, ba.buf, ba.len);
        if (decoded < 0) {
          PRINT_ERROR("Configuration Update Command: syntactically incorrect 5G-GUTI treated as not present\n");
          return buffer->len - ba.len;
        }
        msg->guti = malloc_or_fail(sizeof(*msg->guti));
        *msg->guti = mi;
        UPDATE_BYTE_ARRAY(ba, decoded);
        return buffer->len - ba.len;
      }

      default: // IE not used by the UE
        return buffer->len - ba.len;
    }
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
