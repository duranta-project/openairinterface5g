/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FGS_service_reject_H
#define FGS_service_reject_H

#include <stdint.h>
#include "fgmm_lib.h"

typedef struct {
  // 5GMM cause (Mandatory)
  cause_id_t cause;
  // T3346 Value (Optional)
  gprs_timer_t *t3346;
  // PDU Session Status (Optional)
  uint8_t psi_status[MAX_NUM_PSI];
  bool has_psi_status;
  // T3448 Value (Optional)
  gprs_timer_t *t3448;
  // EAP message (optional)
  byte_array_t *eap_msg;
} fgs_service_reject_msg_t;

int decode_fgs_service_reject(fgs_service_reject_msg_t *msg, const byte_array_t *buffer);
int encode_fgs_service_reject(byte_array_t *buffer, const fgs_service_reject_msg_t *msg);
void free_fgs_service_reject(fgs_service_reject_msg_t *msg);
bool eq_service_reject(const fgs_service_reject_msg_t *a, const fgs_service_reject_msg_t *b);

#endif /* FGS_service_reject_H */
