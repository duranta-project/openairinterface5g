/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "fgmm_registration_reject.h"
#include "common/platform_types.h"
#include "common/utils/ds/byte_array.h"
#include "common/utils/eq_check.h"
#include "fgmm_lib.h"

#define REGISTRATION_REJECT_MIN_LEN 1
#define T3502_IEI 0x16

/** @brief Encode Registration Reject (TS 24.501 clause 8.2.9). */
int encode_fgs_registration_reject(byte_array_t *buffer, const fgs_registration_reject_msg_t *msg)
{
  const size_t required = REGISTRATION_REJECT_MIN_LEN + 3 * ((msg->t3346 != NULL) + (msg->t3502 != NULL));
  if (!buffer->buf || buffer->len < required) {
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
    ENCODE_NAS_IE(ba, encode_gprs_timer_ie(&ba, T3502_IEI, msg->t3502), encoded);
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
  int decoded = decode_fgs_nas_cause(&msg->cause, buffer);
  if (decoded < 0) {
    return -1;
  }

  while ((size_t)decoded < buffer->len) {
    const uint8_t iei = buffer->buf[decoded];
    const byte_array_t ba = {.buf = buffer->buf + decoded + 1, .len = buffer->len - decoded - 1};
    switch (iei) {
      case IEI_T3346_VALUE: {
        if (msg->t3346) {
          PRINT_ERROR("Repeated T3346 in Registration Reject\n");
          return decoded;
        }
        gprs_timer_t timer;
        int ret = decode_gprs_timer_ie(&timer, &ba);
        if (ret < 0)
          return decoded;
        msg->t3346 = malloc_or_fail(sizeof(*msg->t3346));
        *msg->t3346 = timer;
        decoded += 1 + ret;
        break;
      }

      case T3502_IEI: {
        if (msg->t3502) {
          PRINT_ERROR("Repeated T3502 in Registration Reject\n");
          return decoded;
        }
        gprs_timer_t timer;
        int ret = decode_gprs_timer_ie(&timer, &ba);
        if (ret < 0)
          return decoded;
        msg->t3502 = malloc_or_fail(sizeof(*msg->t3502));
        *msg->t3502 = timer;
        decoded += 1 + ret;
        break;
      }

      default:
        PRINT_ERROR("Unhandled Registration Reject IEI 0x%02x\n", iei);
        return decoded;
    }
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
