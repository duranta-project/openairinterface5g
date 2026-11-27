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
/**
 * @brief Decode QoS Rule (9.11.4.13 of 3GPP TS 24.501)
 */
static qos_rule_t decode_qos_rule(uint8_t *buf)
{
  qos_rule_t qos_rule = {0};

  // octet 4
  qos_rule.id = *buf++;
  // octet 5 - 6
  GET_SHORT(buf, qos_rule.length);
  buf += sizeof(qos_rule.length);
  uint8_t *rule_end = buf + qos_rule.length;

  // octet 7
  qos_rule.oc = (*(buf) & 0xE0) >> 5;
  qos_rule.dqr = (*(buf) & 0x10) >> 4;
  qos_rule.nb_pf = *buf++ & 0x0F;

  // octet 8 - m: decode packet filters
  for (int i = 0; i < qos_rule.nb_pf; i++) {
    if (qos_rule.oc == ROC_CREATE_NEW_QOS_RULE || qos_rule.oc == ROC_MODIFY_QOS_RULE_ADD_PF
        || qos_rule.oc == ROC_MODIFY_QOS_RULE_REPLACE_PF) {
      if (buf >= rule_end) {
        LOG_W(NAS, "QoS rule %d ends before packet filter %d header\n", qos_rule.id, i);
        return qos_rule;
      }
      uint8_t direction = (*buf & 0x30) >> 4;
      uint8_t pf_id = *buf++ & 0x0F;
      if (buf >= rule_end) {
        LOG_W(NAS, "QoS rule %d ends before packet filter %d content length\n", qos_rule.id, pf_id);
        return qos_rule;
      }
      uint8_t pf_content_len = *buf++;
      if (pf_content_len > rule_end - buf) {
        LOG_W(NAS,
              "Packet filter %d content length %d exceeds QoS rule %d remaining length %td\n",
              pf_id,
              pf_content_len,
              qos_rule.id,
              rule_end - buf);
        return qos_rule;
      }
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
      if (buf >= rule_end) {
        LOG_W(NAS, "QoS rule %d ends before packet filter %d identifier\n", qos_rule.id, i);
        return qos_rule;
      }
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
    if (rule_end - buf < 2) {
      LOG_W(NAS, "QoS rule %d ends before precedence and QFI\n", qos_rule.id);
      return qos_rule;
    }
    qos_rule.precedence = *buf++;
    qos_rule.qfi = *buf++ & 0x3F;
  }
  return qos_rule;
}

int decode_qos_rules(auth_qos_rule_t *rules, uint8_t *buffer, uint32_t length)
{
  if (buffer == NULL || length < 2)
    return -1;
  rules->num_rules = 0;
  rules->length = 0;
  GET_SHORT(buffer, rules->length);
  if (rules->length == 0 || rules->length > length - 2)
    return -1;
  uint8_t *cursor = buffer + 2;
  const uint8_t *end = cursor + rules->length;
  while (cursor < end) {
    if (rules->num_rules == MAX_NUM_QOS_RULES || end - cursor < 4)
      return -1;
    uint16_t content_length = 0;
    GET_SHORT(cursor + 1, content_length);
    // Delete operations have only an operation octet (TS 24.501 §9.11.4.13).
    if (content_length < 1 || content_length > end - cursor - 3)
      return -1;
    rules->rule[rules->num_rules++] = decode_qos_rule(cursor);
    cursor += content_length + 3;
  }
  return cursor - buffer;
}
