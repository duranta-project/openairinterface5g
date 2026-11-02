/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef E2AP_SETUP_MSG_STORE_H
#define E2AP_SETUP_MSG_STORE_H

#include "../flexric/src/util/byte_array.h"
#include <stdint.h>

/* Interfaces whose Setup Request/Response PDUs can be captured and later
 * reported to the E2 Node Component Configuration Addition List
 * (read_setup_ran()). */
typedef enum {
  E2AP_SETUP_MSG_NGAP = 0,
  E2AP_SETUP_MSG_F1AP,
  E2AP_SETUP_MSG_E1AP,
  E2AP_SETUP_MSG_IFACE_END
} e2ap_setup_msg_iface_t;

/* Store a copy of the encoded (ASN.1 PER) Setup Request/Response PDU for the
 * given interface. Called from F1AP/NGAP/E1AP right after encoding a Setup
 * Request/Response to send, or right after receiving one (re-encoded from
 * the already-decoded PDU). Thread-safe; overwrites any previously stored
 * message for that interface. */
void e2ap_store_setup_req(e2ap_setup_msg_iface_t iface, const uint8_t *buf, uint32_t len);
void e2ap_store_setup_resp(e2ap_setup_msg_iface_t iface, const uint8_t *buf, uint32_t len);

/* Return an owned copy of the last stored Setup Request/Response for the
 * given interface, or a zero-length byte_array_t {0} if none has been
 * captured yet. Caller must free_byte_array() the result. */
byte_array_t e2ap_get_setup_req(e2ap_setup_msg_iface_t iface);
byte_array_t e2ap_get_setup_resp(e2ap_setup_msg_iface_t iface);

#endif /* E2AP_SETUP_MSG_STORE_H */
