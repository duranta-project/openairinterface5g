/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * This header file defines structures, macros,
 * and functions for the handling of common NAS 5GSM IEs.
 * This library is intended for use within the 5GSM encode/decode library only.
 */

#ifndef FGSM_LIB_H
#define FGSM_LIB_H

#include <stdint.h>
#include <stdbool.h>
#include "common/5g_packet_filter.h"

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

#define MAX_NUM_QOS_RULES 64

/* Max packet filters per QoS rule - TS 24.501 clause 9.11.4.13 */
#define MAX_PF_PER_QOS_RULE 15

/* QoS Rule structure - TS 24.501 9.11.4.13 */

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

typedef struct ext_pP_t {
  uint16_t length;
} ext_pP_t; /* TS 24.008 10.5.6.3A - Ommited, only length is processed*/

typedef struct qos_fd_s {
  uint16_t length;
} qos_fd_t; /* TS 24.501 9.11.4.12 - Ommited, only length is processed*/

/**
 * @brief Decode QoS Rule (9.11.4.13 of 3GPP TS 24.501)
 * @param buf Buffer containing the QoS rule
 * @return Decoded QoS rule structure
 */
qos_rule_t decode_qos_rule(uint8_t *buf);

/**
 * @brief Returns the size of the single QoS rule IE
 * @param rule Pointer to QoS rule structure
 * @return Total length of the QoS rule IE in bytes
 */
uint16_t get_len_qos_rule(qos_rule_t *rule);

#endif /* FGSM_LIB_H */
