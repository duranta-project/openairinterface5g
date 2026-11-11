/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef PDU_SESSION_ESTABLISHMENT_ACCEPT_H_
#define PDU_SESSION_ESTABLISHMENT_ACCEPT_H_

#include <stdint.h>
#include <stdbool.h>
#include "fgs_nas_utils.h"
#include "common/utils/utils.h" // text_info_t, TO_ENUM, TO_TEXT
#include "fgsm_lib.h"

/* PDU Session Establish Accept Optional IE Identifiers - TS 24.501 Table 8.3.2.1.1 */

#define FOREACH_IEI(IEI_DEF)                                                              \
  IEI_DEF(IEI_5GSM_CAUSE, 0x59) /* 5GSM cause 9.11.4.2  */                                \
  IEI_DEF(IEI_PDU_ADDRESS, 0x29) /* PDU address 9.11.4.10 */                              \
  IEI_DEF(IEI_RQ_TIMER_VALUE, 0x56) /* GPRS timer 9.11.2.3  */                            \
  IEI_DEF(IEI_SNSSAI, 0x22) /* S-NSSAI 9.11.2.8  */                                       \
  IEI_DEF(IEI_ALWAYSON_PDU, 0x80) /* Always-on PDU session indication 9.11.4.3 */         \
  IEI_DEF(IEI_MAPPED_EPS, 0x75) /* Mapped EPS bearer contexts 9.11.4.8  */                \
  IEI_DEF(IEI_EAP_MSG, 0x78) /* EAP message 9.11.2.2  */                                  \
  IEI_DEF(IEI_AUTH_QOS_DESC, 0x79) /* QoS flow descriptions 9.11.4.12 */                  \
  IEI_DEF(IEI_EXT_CONF_OPT, 0x7b) /* Extended protocol configuration options 9.11.4.6  */ \
  IEI_DEF(IEI_DNN, 0x25) /* DNN 9.11.2.1B  */

static const text_info_t iei_text_desc[] = {FOREACH_IEI(TO_TEXT)};

typedef enum { FOREACH_IEI(TO_ENUM) } pduSessionEstablishment_IEI_t;

/* PDU Session type value - TS 24.501 Table 9.11.4.10.1*/

#define PDU_SESSION_TYPE_IPV4 0b001
#define PDU_SESSION_TYPE_IPV6 0b010
#define PDU_SESSION_TYPE_IPV4V6 0b011
#define PDU_SESSION_TYPE_UNSTRUCT 0b100
#define PDU_SESSION_TYPE_ETHER 0b101
#define IPv4_ADDRESS_LENGTH 4 // length of the IPv4 address associated with a PDU session
#define IPv6_INTERFACE_ID_LENGTH 8 // interface identifier for the IPv6 link local address

/* DNN - APN
 * TS 23.003 9.1
 * The APN is composed of two parts, the APN Network Identifier (9.1.1) & The APN Operator Identifier (9.1.2).
 *
 * The DNN information element has a length in the range of 3 to 102 octets.
 * The Header is consisted of two octets, the DNN IEI and the Length of the DNN contents fields, each is 1 octet.
 * The DNN value payload starts from the 3rd octet.
 * The accumulated max length of APN payload is 100 octets.
 * The min length of the APN payload is 1 octet.
 */
#define APN_MAX_LEN 100
#define APN_MIN_LEN 1

/* Optional Presence IE - TS 24.501 Table 8.3.2.1.1 */

typedef struct pdu_address_s {
  // PDU address IEI (0x29) (octet 1)
  uint8_t pdu_iei;
  // Length of PDU address contents (octet 2)
  uint8_t pdu_length;
  // PDU session type value (9.11.4.11 of TS 24.501)
  uint8_t pdu_type;
  // PDU address IE (depending on type, up to 12 bytes)
  uint8_t pdu_addr_oct[IPv4_ADDRESS_LENGTH + IPv6_INTERFACE_ID_LENGTH];
} pdu_address_t; /* TS 24.501 9.11.4.10 */

typedef struct dnn_s {
  uint8_t dnn_iei; /* DNN IEI (0x25) */
  uint8_t dnn_length; /* Length of DNN contents */
} dnn_t; /* TS 24.501 9.11.2.1A */

typedef struct pdu_session_establishment_accept_msg_s {
  // PDU Session Type (M)
  uint8_t pdu_type;
  // Selected SSC Mode (M)
  uint8_t ssc_mode;
  // Authorized QoS rules (M)
  auth_qos_rule_t qos_rules;
  // Session-AMBR (M)
  session_ambr_t sess_ambr;
  // Data Network Name (O)
  dnn_t dnn_ie;
  // PDU Address (O)
  pdu_address_t pdu_addr_ie;
  // Extended Protocol Configuration Options (O)
  ext_pP_t ext_pp_ie;
  // QoS flow descriptions (O)
  qos_fd_t qos_fd_ie;
} pdu_session_establishment_accept_msg_t; /* 24.501 Table 8.3.2.1.1 */

int decode_pdu_session_establishment_accept_msg(pdu_session_establishment_accept_msg_t *psea_msg, uint8_t *buffer, uint32_t msg_length);

#endif
