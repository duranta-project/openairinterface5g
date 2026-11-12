/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FGS_REGISTRATION_REJECT_H
#define FGS_REGISTRATION_REJECT_H

#include "fgmm_lib.h"
#include "common/utils/ds/byte_array.h"

typedef struct {
  cause_id_t cause;
  // Optional timers; NULL denotes absence, independently of the timer value.
  gprs_timer_t *t3346;
  gprs_timer_t *t3502;
} fgs_registration_reject_msg_t;

int decode_fgs_registration_reject(fgs_registration_reject_msg_t *msg, const byte_array_t *buffer);
int encode_fgs_registration_reject(byte_array_t *buffer, const fgs_registration_reject_msg_t *msg);
bool eq_registration_reject(const fgs_registration_reject_msg_t *a, const fgs_registration_reject_msg_t *b);
void free_fgs_registration_reject(fgs_registration_reject_msg_t *msg);

#endif /* FGS_REGISTRATION_REJECT_H */
