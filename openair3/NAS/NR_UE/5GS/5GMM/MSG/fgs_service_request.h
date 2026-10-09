/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "ProtocolDiscriminator.h"
#include "SecurityHeaderType.h"
#include "NasKeySetIdentifier.h"
#include "ServiceType.h"
#include "FGCNasMessageContainer.h"
#include "MessageType.h"
#include "FGSMobileIdentity.h"
#include "fgmm_lib.h"

#ifndef FGS_SERVICE_REQUEST_H_
#define FGS_SERVICE_REQUEST_H_

/**
 * Message name: Service request (8.2.16 of 3GPP TS 24.501)
 * Description: The SERVICE REQUEST message is sent by the UE to the AMF
 *              in order to request the establishment of an N1 NAS signalling
 *              connection and/or to request the establishment of user-plane
 *              resources for PDU sessions which are established without
 *              user-plane resources
 * Direction: UE to network
 */

typedef struct {
  /* Mandatory fields */
  NasKeySetIdentifier naskeysetidentifier;
  ServiceType serviceType: 4;
  Stmsi5GSMobileIdentity_t fiveg_s_tmsi;
  /* Optional fields */
  bool has_uplink_data_status;
  uint8_t uplink_data_status[MAX_NUM_PSI];
  bool has_pdu_session_status;
  uint8_t pdu_session_status[MAX_NUM_PSI];
  FGCNasMessageContainer *fgsnasmessagecontainer;
} fgs_service_request_msg_t;

int encode_fgs_service_request(uint8_t *buffer, const fgs_service_request_msg_t *servicerequest, uint32_t len);
int decode_fgs_service_request(fgs_service_request_msg_t *sr, const uint8_t *buffer, uint32_t len);
void free_fgs_service_request(fgs_service_request_msg_t *msg);
bool eq_fgs_service_request(const fgs_service_request_msg_t *a, const fgs_service_request_msg_t *b);

#endif /* ! defined(FGS_SERVICE_REQUEST_H_) */
