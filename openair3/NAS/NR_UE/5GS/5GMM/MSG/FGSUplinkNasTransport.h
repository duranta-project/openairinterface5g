/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief uplink nas transport procedures
 */

#include <stdbool.h>
#include <stdint.h>
#include "MessageType.h"
#include "OctetString.h"
#include "SecurityHeaderType.h"

#ifndef FGS_UPLINK_NAS_TRANSPORT_H_
#define FGS_UPLINK_NAS_TRANSPORT_H_

/*
 * Message name: uplink nas transpaort
 * Description: The UL NAS TRANSPORT message transports message payload and associated information to the AMF. See table 8.2.10.1.1.
 * Significance: dual
 * Direction: UE to network
 */

typedef struct PayloadContainerType_tag {
  uint8_t iei: 4;
  uint8_t type: 4;
} PayloadContainerType;
typedef struct FGSPayloadContainer_tag {
  OctetString payloadcontainercontents;
} FGSPayloadContainer;

typedef struct fgs_uplink_nas_transport_msg_tag {
  /* Mandatory fields */
  PayloadContainerType payloadcontainertype;
  FGSPayloadContainer fgspayloadcontainer;
  /* Optional fields */
  uint16_t pdusessionid;
  uint8_t requesttype;
  OctetString snssai;
  OctetString dnn;
  /* Presence flags for optional fields */
  bool requesttype_present;
  bool snssai_present;
  bool dnn_present;
} fgs_uplink_nas_transport_msg;

int encode_fgs_uplink_nas_transport(const fgs_uplink_nas_transport_msg *fgs_security_mode_comp, uint8_t *buffer, uint32_t len);

#endif /* ! defined(FGS_UPLINK_NAS_TRANSPORT_H_) */
