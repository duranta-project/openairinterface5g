/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_SDAP_QOS_FLOW_MANAGER_H_
#define NR_SDAP_QOS_FLOW_MANAGER_H_

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "common/platform_types.h"
#include "PacketFilter.h"

/* Maximum QoS flows per PDU session */
#define MAX_QOS_FLOWS_PER_PDU_SESSION 64

/* Maximum number of packet filters per QoS rule. TS 24.501 Table 9.11.4.13.1 */
#define MAX_PF_PER_QOS_FLOW 15

/**
 * QoS flow with associated packet filters
 */
typedef struct qos_flow_s {
  uint8_t qfi; /* QoS Flow Identifier */
  uint8_t rule_id; /* QoS rule ID */
  uint8_t precedence; /* Rule precedence */
  bool is_default;
  int num_packet_filters;
  packet_filter_decoded_t packet_filters[MAX_PF_PER_QOS_FLOW];
} qos_flow_t;

/**
 * QoS flow manager per PDU session
 */
typedef struct qos_flow_manager_s {
  ue_id_t ue_id;
  int pdusession_id;
  int num_flows;
  qos_flow_t flows[MAX_QOS_FLOWS_PER_PDU_SESSION];
  uint8_t default_qfi; /* Default QFI for unmatched packets */
  pthread_mutex_t qos_flow_lock;
} qos_flow_manager_t;

/**
 * @brief Initialize QoS flow manager for a PDU session
 */
void qos_flow_manager_init(qos_flow_manager_t *mgr, ue_id_t ue_id, int pdusession_id);

/**
 * @brief Destroy QoS flow manager resources on SDAP entity deletion
 */
void qos_flow_manager_destroy(qos_flow_manager_t *mgr);

/**
 * @brief Add a QoS flow with packet filters
 * @param mgr QoS flow manager
 * @param qfi QoS Flow Identifier
 * @param rule_id QoS rule ID
 * @param precedence Rule precedence
 * @param is_default Flag for the default QoS rule
 * @param pf_list Array of packet filters
 * @param num_pf Number of packet filters
 * @return true on success, false if manager is full
 */
bool qos_flow_manager_add(qos_flow_manager_t *mgr,
                          uint8_t qfi,
                          uint8_t rule_id,
                          uint8_t precedence,
                          bool is_default,
                          const packet_filter_decoded_t *pf_list,
                          int num_pf);

/**
 * @brief Remove a QoS flow by rule ID
 * @param mgr QoS flow manager
 * @param rule_id QoS rule ID to remove
 * @return true if found and removed, false otherwise
 */
bool qos_flow_manager_remove(qos_flow_manager_t *mgr, uint8_t rule_id);

/**
 * @brief Update an existing QoS flow in place (QFI, precedence, default flag, and packet filters)
 * @param mgr QoS flow manager
 * @param rule_id QoS rule ID to update
 * @param qfi QoS Flow Identifier
 * @param precedence Rule precedence
 * @param is_default Flag for the default QoS rule
 * @param pf_list New packet filters
 * @param num_pf Number of packet filters
 * @param replace If true, replace all filters; if false, add to existing
 * @return true on success, false if rule not found
 */
bool qos_flow_manager_update(qos_flow_manager_t *mgr,
                             uint8_t rule_id,
                             uint8_t qfi,
                             uint8_t precedence,
                             bool is_default,
                             const packet_filter_decoded_t *pf_list,
                             int num_pf,
                             bool replace);

/**
 * @brief Remove specific packet filters (by ID) from an existing QoS flow
 * @param mgr QoS flow manager
 * @param rule_id QoS rule ID to update
 * @param pf_ids Array of packet filter IDs to remove
 * @param num_ids Number of packet filter IDs to remove
 * @return true if rule was found, false otherwise
 */
bool qos_flow_manager_delete_pf(qos_flow_manager_t *mgr, uint8_t rule_id, const uint8_t *pf_ids, int num_ids);

/**
 * @brief Match an uplink IP packet and return the matching QFI
 * @param mgr QoS flow manager
 * @param ip_pkt Pointer to IP packet
 * @param pkt_len Length of the packet
 * @return QFI of the matching QoS flow, or default QFI if no match
 */
uint8_t qos_flow_manager_match_ul_packet(qos_flow_manager_t *mgr, const uint8_t *ip_pkt, size_t pkt_len);

/**
 * @brief If the manager has at least one flow, atomically set the default QFI
 * @param mgr QoS flow manager
 * @param default_qfi QFI to use as default for unmatched packets
 * @return true if the manager had at least one flow (and default_qfi was set), false otherwise
 */
bool qos_flow_manager_set_default_qfi(qos_flow_manager_t *mgr, uint8_t default_qfi);

/**
 * @brief Check whether the manager currently has no flows
 * @param mgr QoS flow manager
 * @return true if the manager has zero flows
 */
bool is_qos_flow_manager_empty(qos_flow_manager_t *mgr);

#endif /* NR_SDAP_QOS_FLOW_MANAGER_H_ */
