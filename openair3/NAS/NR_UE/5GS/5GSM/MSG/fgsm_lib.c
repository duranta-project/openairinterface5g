/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "fgsm_lib.h"
#include "PacketFilter.h"
#include "common/utils/LOG/log.h"
#include "fgs_nas_utils.h"

/**
 * @brief Returns the size of the single QoS rule IE
 */
uint16_t get_len_qos_rule(qos_rule_t *rule)
{
  return rule->length + sizeof(rule->id) + sizeof(rule->length);
}

/**
 * @brief Decode QoS Rule (9.11.4.13 of 3GPP TS 24.501)
 */
qos_rule_t decode_qos_rule(uint8_t *buf)
{
  qos_rule_t qos_rule = {0};

  // octet 4
  qos_rule.id = *buf++;
  // octet 5 - 6
  GET_SHORT(buf, qos_rule.length);
  buf += sizeof(qos_rule.length);
  // octet 7
  qos_rule.oc = (*(buf) & 0xE0) >> 5;
  qos_rule.dqr = (*(buf) & 0x10) >> 4;
  qos_rule.nb_pf = *buf++ & 0x0F;

  // octet 8 - m: decode packet filters
  for (int i = 0; i < qos_rule.nb_pf; i++) {
    if (qos_rule.oc == ROC_CREATE_NEW_QOS_RULE || qos_rule.oc == ROC_MODIFY_QOS_RULE_ADD_PF
        || qos_rule.oc == ROC_MODIFY_QOS_RULE_REPLACE_PF) {
      uint8_t direction = (*buf & 0x30) >> 4;
      uint8_t pf_id = *buf++ & 0x0F;
      uint8_t pf_content_len = *buf++;
      if (qos_rule.num_packet_filters < MAX_PF_PER_QOS_RULE) {
        packet_filter_decoded_t *pf = &qos_rule.packet_filters[qos_rule.num_packet_filters];
        pf->direction = direction;
        pf->pf_id = pf_id;
        int decoded = decode_packet_filter_contents(buf, pf_content_len, pf);
        if (decoded < 0) {
          LOG_W(NAS, "Failed to decode packet filter contents for PF ID %d\n", pf_id);
        } else {
          qos_rule.num_packet_filters++;
          LOG_D(NAS, "Decoded packet filter ID %d, direction %d, %d components\n", pf_id, direction, pf->num_components);
        }
      } else {
        LOG_W(NAS, "Packet filter storage full, dropping PF ID %d\n", pf_id);
      }
      buf += pf_content_len;
    } else if (qos_rule.oc == ROC_MODIFY_QOS_RULE_DELETE_PF) {
      uint8_t pf_id = *buf++ & 0x0F;
      if (qos_rule.num_pf_delete < MAX_PF_PER_QOS_RULE) {
        qos_rule.pf_delete_ids[qos_rule.num_pf_delete++] = pf_id;
        LOG_D(NAS, "QoS rule operation: delete packet filter ID %d\n", pf_id);
      } else {
        LOG_W(NAS, "Packet filter ID to delete storage full, dropping PF ID %d\n", pf_id);
      }
    }
  }

  // octet m + 1 and m + 2
  if (qos_rule.oc != ROC_DELETE_QOS_RULE) {
    qos_rule.precendence = *buf++;
    qos_rule.qfi = *buf++ & 0x3F;
  }
  return qos_rule;
}
