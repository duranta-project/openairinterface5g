/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "fgmm_registration_reject.h"
#include "common/platform_types.h"
#include "common/utils/ds/byte_array.h"
#include "common/utils/eq_check.h"
#include "fgmm_lib.h"

#define REGISTRATION_REJECT_MIN_LEN 1

/** @brief Encode Registration Reject (TS 24.501 clause 8.2.9). */
int encode_fgs_registration_reject(byte_array_t *buffer, const fgs_registration_reject_msg_t *msg)
{
  if (!buffer->buf || buffer->len < REGISTRATION_REJECT_MIN_LEN) {
    PRINT_ERROR("Insufficient buffer for Registration Reject\n");
    return -1;
  }

  byte_array_t ba = *buffer;
  int encoded = 0;
  ENCODE_NAS_IE(ba, encode_fgs_nas_cause(&ba, &msg->cause), encoded);
  if (msg->t3346) {
    ENCODE_NAS_IE(ba, encode_gprs_timer_ie(&ba, IEI_T3346_VALUE, msg->t3346), encoded);
  }
  if (msg->t3502) {
    ENCODE_NAS_IE(ba, encode_gprs_timer_ie(&ba, IEI_T3502_VALUE, msg->t3502), encoded);
  }
  return encoded;
}

/** @brief Decode Registration Reject (TS 24.501 clause 8.2.9). */
int decode_fgs_registration_reject(fgs_registration_reject_msg_t *msg, const byte_array_t *buffer)
{
  if (!buffer->buf || buffer->len < REGISTRATION_REJECT_MIN_LEN) {
    PRINT_ERROR("Nothing to decode: missing Cause IE!\n");
    return -1;
  }

  *msg = (fgs_registration_reject_msg_t){0};
  byte_array_t ba = *buffer;
  uint32_t decoded = 0;
  DECODE_NAS_IE(ba, decode_fgs_nas_cause(&msg->cause, &ba), decoded);
  bool seen_t3346 = false;
  bool seen_t3502 = false;

  while (decoded < buffer->len) {
    const uint8_t iei = buffer->buf[decoded++];
    UPDATE_BYTE_ARRAY(ba, 1);
    // Include 0x7c and 0x7d from the earlier comprehension-required range (TS 24.007 clause 11.2.5).
    if (iei < 0x10 || (iei >= 0x7c && iei <= 0x7f)) {
      PRINT_ERROR("Unknown comprehension-required Registration Reject IEI 0x%02x\n", iei);
      return -1;
    }
    if (iei & 0x80)
      continue;

    const size_t len_size = iei >= 0x70 ? 2 : 1;
    if (ba.len < len_size) {
      PRINT_ERROR("Registration Reject IEI 0x%02x: truncated optional length\n", iei);
      return buffer->len;
    }
    const size_t content_len = len_size == 2 ? (ba.buf[0] << 8) | ba.buf[1] : ba.buf[0];
    if (content_len > ba.len - len_size) {
      PRINT_ERROR("Registration Reject IEI 0x%02x: truncated optional contents\n", iei);
      return buffer->len;
    }

    switch (iei) {
      case IEI_T3346_VALUE:
      case IEI_T3502_VALUE: {
        const bool is_t3346 = iei == IEI_T3346_VALUE;
        bool *seen = is_t3346 ? &seen_t3346 : &seen_t3502;
        if (*seen) {
          PRINT_ERROR("Ignoring repeated Registration Reject timer IEI 0x%02x\n", iei);
          break;
        }
        *seen = true;
        gprs_timer_t timer;
        if (decode_gprs_timer_ie(&timer, &ba) < 0) {
          // TS 24.501 clause 7.7.1: a malformed optional IE is treated as absent.
          PRINT_ERROR("Ignoring malformed Registration Reject timer IEI 0x%02x\n", iei);
          break;
        }
        gprs_timer_t *value = malloc_or_fail(sizeof(*value));
        *value = timer;
        if (is_t3346)
          msg->t3346 = value;
        else
          msg->t3502 = value;
        break;
      }

      default:
        PRINT_ERROR("Ignoring unhandled Registration Reject IEI 0x%02x\n", iei);
        break;
    }
    decoded += len_size + content_len;
    UPDATE_BYTE_ARRAY(ba, len_size + content_len);
  }

  return decoded;
}

bool eq_registration_reject(const fgs_registration_reject_msg_t *a, const fgs_registration_reject_msg_t *b)
{
  _EQ_CHECK_INT(a->cause, b->cause);
  _EQ_CHECK_INT(a->t3346 != NULL, b->t3346 != NULL);
  _EQ_CHECK_INT(a->t3502 != NULL, b->t3502 != NULL);
  if (a->t3346 && !eq_gprs_timer(a->t3346, b->t3346))
    return false;
  if (a->t3502 && !eq_gprs_timer(a->t3502, b->t3502))
    return false;
  return true;
}

void free_fgs_registration_reject(fgs_registration_reject_msg_t *msg)
{
  free(msg->t3346);
  free(msg->t3502);
}
