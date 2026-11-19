/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include "PduSessionEstablishmentAccept.h"
#include "fgsm_lib.h"
#include "common/utils/LOG/log.h"
#include "common/utils/utils.h" // text_info_t, TO_ENUM, TO_TEXT
#include "fgs_nas_utils.h"

static bool psea_read_length(uint8_t **cursor, const uint8_t *end, int width, uint16_t *length)
{
  if (end - *cursor < width)
    return false;
  *length = 0;
  if (width == 2) {
    GET_SHORT(*cursor, *length);
  } else {
    *length = **cursor;
  }
  *cursor += width;
  return *length <= end - *cursor;
}

/**
 * @brief PDU session establishment accept (8.3.2 of 3GPP TS 24.501)
 *        network to UE
 */
int decode_pdu_session_establishment_accept_msg(pdu_session_establishment_accept_msg_t *psea_msg, uint8_t *buffer, uint32_t msg_length)
{
  if (buffer == NULL || msg_length < 4)
    return -1;
  uint8_t *curPtr = buffer;
  const uint8_t *end = buffer + msg_length;

  /* Mandatory PDU Session Establishment Accept IEs */
  // PDU Session Type (half octet)
  psea_msg->pdu_type = *curPtr & 0x0f;
  // SSC Mode (half octet)
  psea_msg->ssc_mode = (*curPtr++ & 0xf0) >> 4;
  // Authorized QoS Rules
  auth_qos_rule_t *qos_rules = &psea_msg->qos_rules;
  // Length of the rule IEs (2 octets)
  qos_rules->num_rules = 0;
  if (!psea_read_length(&curPtr, end, 2, &qos_rules->length) || qos_rules->length == 0)
    return -1;
  /* Decode rule IEs as long as the total length
    of all IEs (qos_rules->length) is not reached */
  const uint8_t *rules_end = curPtr + qos_rules->length;
  while (curPtr < rules_end) {
    if (qos_rules->num_rules == MAX_NUM_QOS_RULES || rules_end - curPtr < 3)
      return -1;
    uint16_t content_len = 0;
    GET_SHORT(curPtr + 1, content_len);
    const size_t rule_len = (size_t)content_len + 3;
    if (content_len < 3 || rule_len > (size_t)(rules_end - curPtr))
      return -1;
    qos_rule_t *rule = &qos_rules->rule[qos_rules->num_rules];
    *rule = decode_qos_rule(curPtr);
    if (rule->oc != ROC_CREATE_NEW_QOS_RULE || rule->id == 0 || rule->qfi == 0 || rule->nb_pf == 0
        || rule->num_packet_filters != rule->nb_pf)
      return -1;
    qos_rules->num_rules++;
    curPtr += rule_len;
  }
  // Session-AMBR (M)
  uint16_t ie_length;
  if (!psea_read_length(&curPtr, end, 1, &ie_length))
    return -1;
  psea_msg->sess_ambr.length = ie_length;
  curPtr += ie_length; // skip content

  /* Optional Presence IEs */
  while (curPtr < end) {
    uint8_t psea_iei = *curPtr++;
    LOG_T(NAS, "PDU SESSION ESTABLISHMENT ACCEPT - Received IEI 0x%02x\n", psea_iei);
    switch (psea_iei) {
      case IEI_5GSM_CAUSE: /* Ommited */
        if (curPtr == end)
          return -1;
        curPtr++;
        break;

      case IEI_PDU_ADDRESS:
        if (!psea_read_length(&curPtr, end, 1, &ie_length) || ie_length < 1)
          return -1;
        psea_msg->pdu_addr_ie.pdu_length = ie_length;
        uint8_t *address_end = curPtr + ie_length;

        /* Octet 3 */
        // PDU type (3 bits)
        psea_msg->pdu_addr_ie.pdu_type = *curPtr & 0x07;
        // SMF's IPv6 link local address (1 bit)
        uint8_t si6lla = (*curPtr >> 3) & 0x01;
        if (si6lla) {
          LOG_E(NAS, "SMF's IPv6 link local address is not handled\n");
          return -1;
        }
        curPtr++;

        /* Octet 4 to n */
        // PDU address information
        uint8_t *addr = psea_msg->pdu_addr_ie.pdu_addr_oct;
        if (psea_msg->pdu_addr_ie.pdu_type == PDU_SESSION_TYPE_IPV4) {
          if (address_end - curPtr < IPv4_ADDRESS_LENGTH)
            return -1;
          for (int i = 0; i < IPv4_ADDRESS_LENGTH; ++i)
            addr[i] = *curPtr++;
          LOG_I(NAS, "Received PDU Session Establishment Accept, UE IPv4: %u.%u.%u.%u\n", addr[0], addr[1], addr[2], addr[3]);
        } else if (psea_msg->pdu_addr_ie.pdu_type == PDU_SESSION_TYPE_IPV6) {
          if (address_end - curPtr < IPv6_INTERFACE_ID_LENGTH)
            return -1;
          for (int i = 0; i < IPv6_INTERFACE_ID_LENGTH; ++i)
            addr[i] = *curPtr++;
          LOG_I(NAS, "Received PDU Session Establishment Accept, UE IPv6: %u.%u.%u.%u.%u.%u.%u.%u\n",
                addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
        } else if (psea_msg->pdu_addr_ie.pdu_type == PDU_SESSION_TYPE_IPV4V6) {
          if (address_end - curPtr < IPv4_ADDRESS_LENGTH + IPv6_INTERFACE_ID_LENGTH)
            return -1;
          // 24.501 Sec 9.11.4.10: "If the PDU session type value indicates
          // IPv4v6, the PDU address information in octet 4 to octet 11
          // contains an interface identifier for the IPv6 link local address
          // and in octet 12 to octet 15 contains an IPv4 address."
          for (int i = 0; i < IPv4_ADDRESS_LENGTH + IPv6_INTERFACE_ID_LENGTH; ++i)
            addr[i] = *curPtr++;
          LOG_I(NAS, "Received PDU Session Establishment Accept, UE IPv4v6: %u.%u.%u.%u/%u.%u.%u.%u.%u.%u.%u.%u\n",
                addr[8], addr[9], addr[10], addr[11],
                addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
        } else {
          LOG_E(NAS, "unknown/unhandled PDU session establishment accept PDU type %d\n", psea_msg->pdu_addr_ie.pdu_type);
        }
        curPtr = address_end;
        break;

      case IEI_RQ_TIMER_VALUE: /* Ommited */
        if (curPtr == end)
          return -1;
        curPtr++; /* TS 24.008 10.5.7.3 */
        break;

      case IEI_SNSSAI: {
        if (!psea_read_length(&curPtr, end, 1, &ie_length))
          return -1;
        curPtr += ie_length;
        break;
      }

      case IEI_ALWAYSON_PDU: /* Ommited */
      case IEI_ALWAYSON_PDU | 0x01:
        break;

      case IEI_MAPPED_EPS: {
        if (!psea_read_length(&curPtr, end, 2, &ie_length))
          return -1;
        curPtr += ie_length;
        break;
      }

      case IEI_EAP_MSG: {
        if (!psea_read_length(&curPtr, end, 2, &ie_length))
          return -1;
        curPtr += ie_length;
        break;
      }

      case IEI_AUTH_QOS_DESC: {
        if (!psea_read_length(&curPtr, end, 2, &psea_msg->qos_fd_ie.length))
          return -1;
        curPtr += psea_msg->qos_fd_ie.length;
        break;
      }

      case IEI_EXT_CONF_OPT: {
        if (!psea_read_length(&curPtr, end, 2, &psea_msg->ext_pp_ie.length))
          return -1;
        curPtr += psea_msg->ext_pp_ie.length;
        break;
      }

      case IEI_DNN: {
        if (!psea_read_length(&curPtr, end, 1, &ie_length))
          return -1;
        psea_msg->dnn_ie.dnn_length = ie_length;
        if (ie_length > APN_MAX_LEN || ie_length < APN_MIN_LEN) {
          LOG_E(NAS, "PDU SESSION ESTABLISHMENT ACCEPT - DNN IE has invalid length\n");
          return -1;
        }
        curPtr += ie_length;
        break;
      }

      default:
        PRINT_ERROR("Unknown IEI %d\n", psea_iei);
        curPtr = buffer + msg_length; // stop processing unsupported optional IEs
        break;
    }
  }
  return curPtr - buffer;
}
