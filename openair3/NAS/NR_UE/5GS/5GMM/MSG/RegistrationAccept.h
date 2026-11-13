/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief 5GS registration accept procedures
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "SecurityHeaderType.h"
#include "MessageType.h"
#include "FGSMobileIdentity.h"
#include "ds/byte_array.h"

#ifndef REGISTRATION_ACCEPT_H_
#define REGISTRATION_ACCEPT_H_

typedef enum {
  FGS_REGISTRATION_RESULT_3GPP = 1,
  FGS_REGISTRATION_RESULT_NON_3GPP = 2,
  FGS_REGISTRATION_RESULT_3GPP_AND_NON_3GPP = 3
} fgs_registration_result_t;

#define NAS_MAX_NUMBER_SLICES 8

// 9.11.3.37 of 3GPP TS 24.501
typedef struct {
  int sst;
  int *hplmn_sst;
  int *sd;
  int *hplmn_sd;
} nr_nas_msg_snssai_t;

/*
 * Message name: Registration accept
 * Description: The REGISTRATION ACCEPT message is sent by the AMF to the UE. See table 8.2.7.1.1.
 * Significance: dual
 * Direction: network to UE
 */

typedef struct registration_accept_msg_tag {
  // 5GS registration result (Mandatory)
  fgs_registration_result_t result;
  bool sms_allowed;
  // 5G-GUTI (Optional)
  FGSMobileIdentity *guti;
  // Allowed NSSAI (Optional)
  nr_nas_msg_snssai_t nas_allowed_nssai[NAS_MAX_NUMBER_SLICES];
  uint8_t num_allowed_slices;
  // Configured NSSAI (Optional)
  nr_nas_msg_snssai_t config_nssai[NAS_MAX_NUMBER_SLICES];
  uint8_t num_configured_slices;
} registration_accept_msg;

int decode_registration_accept(registration_accept_msg *registrationaccept, const byte_array_t buffer);

int encode_registration_accept(const registration_accept_msg *registrationaccept, uint8_t *buffer, uint32_t len);

bool eq_fgmm_registration_accept(const registration_accept_msg *a, const registration_accept_msg *b);
void free_fgmm_registration_accept(registration_accept_msg *msg);

#endif /* ! defined(REGISTRATION_ACCEPT_H_) */
