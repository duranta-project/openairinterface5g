/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FIVEG_QOS_RULES_H_
#define FIVEG_QOS_RULES_H_

#include <stdbool.h>
#include <stdint.h>

#include "common/5g_packet_filter.h"
#include "common/platform_types.h"

/** @brief Create a new QoS flow */
void nr_sdap_qos_rule_add(ue_id_t ue_id,
                          int pdusession_id,
                          uint8_t rule_id,
                          uint8_t qfi,
                          uint8_t precedence,
                          bool is_default,
                          const packet_filter_decoded_t *pf_list,
                          int num_pf);

/** @brief Remove a QoS flow */
void nr_sdap_qos_rule_remove(ue_id_t ue_id, int pdusession_id, uint8_t rule_id);

/** @brief Update an existing QoS flow's packet filters
 * @param replace If true, replace all packet filters; if false, add pf_list to the existing ones */
void nr_sdap_qos_rule_update(ue_id_t ue_id,
                             int pdusession_id,
                             uint8_t rule_id,
                             uint8_t qfi,
                             uint8_t precedence,
                             bool is_default,
                             const packet_filter_decoded_t *pf_list,
                             int num_pf,
                             bool replace);

/** @brief Remove specific packet filters (by ID) from an existing QoS flow */
void nr_sdap_qos_rule_delete_pf(ue_id_t ue_id, int pdusession_id, uint8_t rule_id, const uint8_t *pf_ids, int num_ids);

#endif
