/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief PDU session modification complete procedures
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "MessageType.h"

#ifndef PDU_SESSION_MODIFICATION_COMPLETE_H_
#define PDU_SESSION_MODIFICATION_COMPLETE_H_

/*
 * Message name: PDU session modification complete
 * Description: The PDU SESSION MODIFICATION COMPLETE message is sent by the UE to the SMF
 *              to acknowledge the PDU SESSION MODIFICATION COMMAND message.
 *              See table 8.3.10.1.1 of 3GPP TS 24.501.
 * Significance: dual
 * Direction: UE to network
 */

typedef struct pdu_session_modification_complete_msg_tag {
  /* Mandatory fields */
  uint8_t protocoldiscriminator;
  uint8_t pdusessionid;
  uint8_t pti;
  MessageType pdusessionmodificationcompletemsgtype;
  /* Optional fields */
} pdu_session_modification_complete_msg;

int encode_pdu_session_modification_complete(pdu_session_modification_complete_msg *pdusessionmodificationcomplete,
                                             uint8_t *buffer,
                                             uint32_t len);

#endif /* ! defined(PDU_SESSION_MODIFICATION_COMPLETE_H_) */
