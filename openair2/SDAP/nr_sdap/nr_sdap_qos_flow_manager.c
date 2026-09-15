/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_sdap_qos_flow_manager.h"
#include "common/utils/LOG/log.h"
#include "common/utils/utils.h"
#include "common/platform_types.h"
#include <string.h>
#include <stdlib.h>

/**
 * @brief Compare function for qsort, sort by precedence
 */
static int compare_precedence(const void *a, const void *b)
{
  const qos_flow_t *flow_a = (const qos_flow_t *)a;
  const qos_flow_t *flow_b = (const qos_flow_t *)b;
  return flow_a->precedence - flow_b->precedence;
}

void qos_flow_manager_init(qos_flow_manager_t *mgr, ue_id_t ue_id, int pdusession_id)
{
  memset(mgr, 0, sizeof(*mgr));
  mgr->ue_id = ue_id;
  mgr->pdusession_id = pdusession_id;
  pthread_mutex_init(&mgr->qos_flow_lock, NULL);
}

void qos_flow_manager_destroy(qos_flow_manager_t *mgr)
{
  pthread_mutex_destroy(&mgr->qos_flow_lock);
}

bool qos_flow_manager_add(qos_flow_manager_t *mgr,
                          uint8_t qfi,
                          uint8_t rule_id,
                          uint8_t precedence,
                          bool is_default,
                          const packet_filter_decoded_t *pf_list,
                          int num_pf)
{
  pthread_mutex_lock(&mgr->qos_flow_lock);

  if (mgr->num_flows >= MAX_QOS_FLOWS_PER_PDU_SESSION) {
    LOG_E(SDAP, "UE %lu PDU session %d: QoS flow manager full, cannot add QFI %d\n", mgr->ue_id, mgr->pdusession_id, qfi);
    pthread_mutex_unlock(&mgr->qos_flow_lock);
    return false;
  }

  qos_flow_t *flow = &mgr->flows[mgr->num_flows];
  flow->qfi = qfi;
  flow->rule_id = rule_id;
  flow->precedence = precedence;
  flow->is_default = is_default;
  flow->num_packet_filters = cmin(num_pf, MAX_PF_PER_QOS_FLOW);

  if (pf_list != NULL && num_pf > 0) {
    memcpy(flow->packet_filters, pf_list, flow->num_packet_filters * sizeof(packet_filter_decoded_t));
  }

  if (is_default) {
    mgr->default_qfi = qfi;
  }

  mgr->num_flows++;

  // Sort flows by precedence (lower value = higher priority)
  qsort(mgr->flows, mgr->num_flows, sizeof(qos_flow_t), compare_precedence);

  LOG_I(SDAP,
        "UE %lu PDU session %d: Added QoS flow - QFI %d, rule %d, precedence %d, %d filters%s\n",
        mgr->ue_id,
        mgr->pdusession_id,
        qfi,
        rule_id,
        precedence,
        flow->num_packet_filters,
        is_default ? " (default)" : "");

  pthread_mutex_unlock(&mgr->qos_flow_lock);
  return true;
}

bool qos_flow_manager_remove(qos_flow_manager_t *mgr, uint8_t rule_id)
{
  pthread_mutex_lock(&mgr->qos_flow_lock);

  for (int i = 0; i < mgr->num_flows; i++) {
    if (mgr->flows[i].rule_id == rule_id) {
      LOG_I(SDAP,
            "UE %lu PDU session %d: Removing QoS flow rule %d (QFI %d)\n",
            mgr->ue_id,
            mgr->pdusession_id,
            rule_id,
            mgr->flows[i].qfi);

      // Shift remaining flows down
      memmove(&mgr->flows[i], &mgr->flows[i + 1], (mgr->num_flows - i - 1) * sizeof(qos_flow_t));
      mgr->num_flows--;
      pthread_mutex_unlock(&mgr->qos_flow_lock);
      return true;
    }
  }

  LOG_W(SDAP, "UE %lu PDU session %d: QoS rule %d not found for removal\n", mgr->ue_id, mgr->pdusession_id, rule_id);
  pthread_mutex_unlock(&mgr->qos_flow_lock);
  return false;
}

bool qos_flow_manager_update(qos_flow_manager_t *mgr,
                             uint8_t rule_id,
                             uint8_t qfi,
                             uint8_t precedence,
                             bool is_default,
                             const packet_filter_decoded_t *pf_list,
                             int num_pf,
                             bool replace)
{
  pthread_mutex_lock(&mgr->qos_flow_lock);

  for (int i = 0; i < mgr->num_flows; i++) {
    if (mgr->flows[i].rule_id == rule_id) {
      qos_flow_t *flow = &mgr->flows[i];

      flow->qfi = qfi;
      flow->precedence = precedence;
      flow->is_default = is_default;
      if (is_default) {
        mgr->default_qfi = qfi;
      }

      if (replace) {
        flow->num_packet_filters = cmin(num_pf, MAX_PF_PER_QOS_FLOW);
        if (pf_list != NULL && num_pf > 0) {
          memcpy(flow->packet_filters, pf_list, flow->num_packet_filters * sizeof(packet_filter_decoded_t));
        }
        LOG_I(SDAP,
              "UE %lu PDU session %d: Replaced packet filters for rule %d (QFI %d) - now %d filters\n",
              mgr->ue_id,
              mgr->pdusession_id,
              rule_id,
              flow->qfi,
              flow->num_packet_filters);
      } else {
        // Add to existing filters
        int space_left = MAX_PF_PER_QOS_FLOW - flow->num_packet_filters;
        int to_add = cmin(num_pf, space_left);
        if (pf_list != NULL && to_add > 0) {
          memcpy(&flow->packet_filters[flow->num_packet_filters], pf_list, to_add * sizeof(packet_filter_decoded_t));
          flow->num_packet_filters += to_add;
        }
        LOG_I(SDAP,
              "UE %lu PDU session %d: Added %d packet filters to rule %d (QFI %d) - now %d filters\n",
              mgr->ue_id,
              mgr->pdusession_id,
              to_add,
              rule_id,
              flow->qfi,
              flow->num_packet_filters);
      }

      // Precedence may have changed, so re-sort flows
      qsort(mgr->flows, mgr->num_flows, sizeof(qos_flow_t), compare_precedence);
      pthread_mutex_unlock(&mgr->qos_flow_lock);
      return true;
    }
  }

  LOG_W(SDAP, "UE %lu PDU session %d: QoS rule %d not found for update\n", mgr->ue_id, mgr->pdusession_id, rule_id);
  pthread_mutex_unlock(&mgr->qos_flow_lock);
  return false;
}

bool qos_flow_manager_delete_pf(qos_flow_manager_t *mgr, uint8_t rule_id, const uint8_t *pf_ids, int num_ids)
{
  pthread_mutex_lock(&mgr->qos_flow_lock);

  for (int i = 0; i < mgr->num_flows; i++) {
    if (mgr->flows[i].rule_id != rule_id)
      continue;

    qos_flow_t *flow = &mgr->flows[i];
    int removed = 0;
    for (int k = 0; k < num_ids; k++) {
      for (int j = 0; j < flow->num_packet_filters; j++) {
        if (flow->packet_filters[j].pf_id != pf_ids[k])
          continue;
        memmove(&flow->packet_filters[j], &flow->packet_filters[j + 1], (flow->num_packet_filters - j - 1) * sizeof(*flow->packet_filters));
        flow->num_packet_filters--;
        removed++;
        break;
      }
    }

    LOG_I(SDAP,
          "UE %lu PDU session %d: Deleted %d/%d packet filters from rule %d (QFI %d) - now %d filters\n",
          mgr->ue_id,
          mgr->pdusession_id,
          removed,
          num_ids,
          rule_id,
          flow->qfi,
          flow->num_packet_filters);
    pthread_mutex_unlock(&mgr->qos_flow_lock);
    return true;
  }

  LOG_W(SDAP, "UE %lu PDU session %d: QoS rule %d not found for packet filter deletion\n", mgr->ue_id, mgr->pdusession_id, rule_id);
  pthread_mutex_unlock(&mgr->qos_flow_lock);
  return false;
}

uint8_t qos_flow_manager_match_ul_packet(qos_flow_manager_t *mgr, const uint8_t *ip_pkt, size_t pkt_len)
{
  pthread_mutex_lock(&mgr->qos_flow_lock);

  if (mgr->num_flows == 0) {
    uint8_t default_qfi = mgr->default_qfi;
    pthread_mutex_unlock(&mgr->qos_flow_lock);
    return default_qfi;
  }

  // Match packet against filters in precedence order
  for (int i = 0; i < mgr->num_flows; i++) {
    const qos_flow_t *flow = &mgr->flows[i];

    // Skip default flow (with match-all filter) and flows with no filters
    if (flow->num_packet_filters == 0 || flow->is_default) {
      continue;
    }

    // Check if packet matches UL or Bidirectional filter in this flow
    for (int j = 0; j < flow->num_packet_filters; j++) {
      uint8_t direction = flow->packet_filters[j].direction;
      if (direction != PF_DIR_UPLINK && direction != PF_DIR_BIDIRECTIONAL) {
        continue;
      }
      if (packet_filter_match(&flow->packet_filters[j], ip_pkt, pkt_len)) {
        LOG_D(SDAP,
              "UE %lu PDU session %d: UL packet matched QFI %d (rule %d, filter %d)\n",
              mgr->ue_id,
              mgr->pdusession_id,
              flow->qfi,
              flow->rule_id,
              flow->packet_filters[j].pf_id);
        uint8_t qfi = flow->qfi;
        pthread_mutex_unlock(&mgr->qos_flow_lock);
        return qfi;
      }
    }
  }

  // No match found - use default QFI
  LOG_D(SDAP,
        "UE %lu PDU session %d: UL packet did not match any filter, using default QFI %d\n",
        mgr->ue_id,
        mgr->pdusession_id,
        mgr->default_qfi);
  uint8_t default_qfi = mgr->default_qfi;
  pthread_mutex_unlock(&mgr->qos_flow_lock);
  return default_qfi;
}

bool qos_flow_manager_set_default_qfi(qos_flow_manager_t *mgr, uint8_t default_qfi)
{
  pthread_mutex_lock(&mgr->qos_flow_lock);
  bool has_flows = mgr->num_flows > 0;
  if (has_flows) {
    mgr->default_qfi = default_qfi;
  }
  pthread_mutex_unlock(&mgr->qos_flow_lock);
  return has_flows;
}

bool is_qos_flow_manager_empty(qos_flow_manager_t *mgr)
{
  pthread_mutex_lock(&mgr->qos_flow_lock);
  bool empty = mgr->num_flows == 0;
  pthread_mutex_unlock(&mgr->qos_flow_lock);
  return empty;
}
