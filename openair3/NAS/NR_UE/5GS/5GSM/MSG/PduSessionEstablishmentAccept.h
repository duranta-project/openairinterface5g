/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef PDU_SESSION_ESTABLISHMENT_ACCEPT_H_
#define PDU_SESSION_ESTABLISHMENT_ACCEPT_H_

#include <stdint.h>
#include <stdbool.h>
#include "fgs_nas_utils.h"
#include "common/utils/utils.h" // text_info_t, TO_ENUM, TO_TEXT
#include "PacketFilter.h"

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

/* Rule operation codes - TS 24.501 Table 9.11.4.13.1 */

// clang-format off
#define ROC_RESERVED_0                  0b000 /* Reserved */
#define ROC_CREATE_NEW_QOS_RULE         0b001 /* Create new QoS rule */
#define ROC_DELETE_QOS_RULE             0b010 /* Delete existing QoS rule */
#define ROC_MODIFY_QOS_RULE_ADD_PF      0b011 /* Modify existing QoS rule and add packet filters */
#define ROC_MODIFY_QOS_RULE_REPLACE_PF  0b100 /* Modify existing QoS rule and replace all packet filters */
#define ROC_MODIFY_QOS_RULE_DELETE_PF   0b101 /* Modify existing QoS rule and delete packet filters */
#define ROC_MODIFY_QOS_RULE_WITHOUT_PF  0b110 /* Modify existing QoS rule without modifying packet filters */
#define ROC_RESERVED_1                  0b111 /* Reserved */
// clang-format on

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

#define MAX_NUM_QOS_RULES 64

/* Max packet filters per QoS rule - TS 24.501 clause 9.11.4.13 */
#define MAX_PF_PER_QOS_RULE 15

/* Mandatory Presence IE - TS 24.501 Table 8.3.2.1.1 */

typedef struct packet_filter_create_qos_rule_s {
  uint8_t pf_dir; /* Packet filter direction */
  uint8_t pf_id; /* Packet filter identifier */
  uint8_t length; /* Length of packet filter contents */
} packet_filter_type1_t; /* TS 24.501 Figure 9.11.4.13.3 */

typedef struct packet_filter_modify_qos_rule_s {
  uint8_t pf_id; /* Packet filter identifier */
} packet_filter_type2_t; /* TS 24.501 Figure 9.11.4.13.4 */

typedef struct packet_filter_s {
  union pf_type {
    packet_filter_type1_t type_1;
    packet_filter_type2_t type_2;
  } pf_type;
} packet_filter_t;

typedef struct qos_rule_s {
  // QoS rule identifier
  uint8_t id;
  // Length of QoS Rule
  uint16_t length;
  // Rule operation code
  uint8_t oc;
  // Default QoS Rule
  bool dqr;
  // Number of packet filters
  uint8_t nb_pf;
  // QoS rule precedence
  uint8_t precendence;
  // QoS Flow Identifier
  uint8_t qfi;
  // Decoded packet filters
  packet_filter_decoded_t packet_filters[MAX_PF_PER_QOS_RULE];
  int num_packet_filters;
  // Packet filter IDs to delete
  uint8_t pf_delete_ids[MAX_PF_PER_QOS_RULE];
  int num_pf_delete;
} qos_rule_t;

typedef struct auth_qos_rules_s {
  uint16_t length; /* Length of QoS rules IE */
  int num_rules; /* Number of decoded rules */
  // QoS rules (M)
  qos_rule_t rule[MAX_NUM_QOS_RULES];
} auth_qos_rule_t; /* QoS Rule as defined in 24.501 Figure 9.11.4.13.2 */

typedef struct session_ambr_s {
  uint8_t length; /* Length of Session-AMBR contents */
  uint8_t unit_dl; /* Unit for Session-AMBR for downlink */
  uint16_t sess_dl; /* Session-AMBR for downlink */
  uint8_t unit_ul; /* Unit for Session-AMBR for uplink */
  uint16_t sess_ul; /* Session-AMBR for uplink */
} session_ambr_t; /* TS 24.501 Figure 9.11.4.14.1 */

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

typedef struct ext_pP_t {
  uint16_t length;
} ext_pP_t; /* TS 24.008 10.5.6.3A - Ommited, only length is processed*/

typedef struct qos_fd_s {
  uint16_t length;
} qos_fd_t; /* TS 24.501 9.11.4.12 - Ommited, only length is processed*/

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
