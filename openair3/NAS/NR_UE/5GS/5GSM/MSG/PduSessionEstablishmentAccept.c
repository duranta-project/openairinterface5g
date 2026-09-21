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

/**
 * @brief PDU session establishment accept (8.3.2 of 3GPP TS 24.501)
 *        network to UE
 */
int decode_pdu_session_establishment_accept_msg(pdu_session_establishment_accept_msg_t *psea_msg, uint8_t *buffer, uint32_t msg_length)
{
  uint8_t *curPtr = buffer;

  /* Mandatory PDU Session Establishment Accept IEs */
  // PDU Session Type (half octet)
  psea_msg->pdu_type = *curPtr & 0x0f;
  // SSC Mode (half octet)
  psea_msg->ssc_mode = (*curPtr++ & 0xf0) >> 4;
  // Authorized QoS Rules
  auth_qos_rule_t *qos_rules = &psea_msg->qos_rules;
  // Length of the rule IEs (2 octets)
  GET_SHORT(curPtr, qos_rules->length);
  curPtr += sizeof(qos_rules->length);
  /* Decode rule IEs as long as the total length
    of all IEs (qos_rules->length) is not reached */
  uint16_t rules_tot_len = 0;
  while (rules_tot_len < qos_rules->length)
  {
    qos_rules->rule[0] = decode_qos_rule(curPtr);
    rules_tot_len += get_len_qos_rule(&qos_rules->rule[0]);
    curPtr += get_len_qos_rule(&qos_rules->rule[0]);
  }
  // Session-AMBR (M)
  psea_msg->sess_ambr.length = *curPtr++;
  curPtr += psea_msg->sess_ambr.length; // skip content

  uint8_t *resPtr = curPtr;

  /* Optional Presence IEs */
  while (curPtr < buffer + msg_length) {
    uint8_t psea_iei = *curPtr++;
    LOG_T(NAS, "PDU SESSION ESTABLISHMENT ACCEPT - Received %s IEI\n", iei_text_desc[psea_iei].text);
    switch (psea_iei) {
      case IEI_5GSM_CAUSE: /* Ommited */
        curPtr++;
        break;

      case IEI_PDU_ADDRESS:
        psea_msg->pdu_addr_ie.pdu_length = *curPtr++;

        /* Octet 3 */
        // PDU type (3 bits)
        psea_msg->pdu_addr_ie.pdu_type = *curPtr & 0x07;
        // SMF's IPv6 link local address (1 bit)
        uint8_t si6lla = (*curPtr >> 3) & 0x01;
        if (si6lla)
          LOG_E(NAS, "SMF's IPv6 link local address is not handled\n");
        curPtr++;

        /* Octet 4 to n */
        // PDU address information
        uint8_t *addr = psea_msg->pdu_addr_ie.pdu_addr_oct;
        if (psea_msg->pdu_addr_ie.pdu_type == PDU_SESSION_TYPE_IPV4) {
          for (int i = 0; i < IPv4_ADDRESS_LENGTH; ++i)
            addr[i] = *curPtr++;
          LOG_I(NAS, "Received PDU Session Establishment Accept, UE IPv4: %u.%u.%u.%u\n", addr[0], addr[1], addr[2], addr[3]);
        } else if (psea_msg->pdu_addr_ie.pdu_type == PDU_SESSION_TYPE_IPV6) {
          for (int i = 0; i < IPv6_INTERFACE_ID_LENGTH; ++i)
            addr[i] = *curPtr++;
          LOG_I(NAS, "Received PDU Session Establishment Accept, UE IPv6: %u.%u.%u.%u.%u.%u.%u.%u\n",
                addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
        } else if (psea_msg->pdu_addr_ie.pdu_type == PDU_SESSION_TYPE_IPV4V6) {
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
          curPtr += psea_msg->pdu_addr_ie.pdu_length;
        }
        break;

      case IEI_RQ_TIMER_VALUE: /* Ommited */
        curPtr++; /* TS 24.008 10.5.7.3 */
        break;

      case IEI_SNSSAI: {
        uint8_t snssai_length = *curPtr;
        curPtr += (snssai_length + sizeof(snssai_length));
        break;
      }

      case IEI_ALWAYSON_PDU: /* Ommited */
        curPtr++;
        break;

      case IEI_MAPPED_EPS: {
        uint16_t mapped_eps_length = 0;
        GET_SHORT(curPtr, mapped_eps_length);
        curPtr += mapped_eps_length;
        break;
      }

      case IEI_EAP_MSG: {
        uint16_t eap_length = 0;
        GET_SHORT(curPtr, eap_length);
        curPtr += (eap_length + sizeof(eap_length));
        break;
      }

      case IEI_AUTH_QOS_DESC: {
        GET_SHORT(curPtr, psea_msg->qos_fd_ie.length);
        curPtr += (psea_msg->qos_fd_ie.length + sizeof(psea_msg->qos_fd_ie.length));
        break;
      }

      case IEI_EXT_CONF_OPT: {
        GET_SHORT(curPtr, psea_msg->ext_pp_ie.length);
        curPtr += (psea_msg->ext_pp_ie.length + sizeof(psea_msg->ext_pp_ie.length));
        break;
      }

      case IEI_DNN: {
        psea_msg->dnn_ie.dnn_length = *curPtr++;
        char apn[APN_MAX_LEN];

        if (psea_msg->dnn_ie.dnn_length <= APN_MAX_LEN && psea_msg->dnn_ie.dnn_length >= APN_MIN_LEN) {
          memcpy(apn, curPtr, psea_msg->dnn_ie.dnn_length);
          LOG_T(NAS, "PDU SESSION ESTABLISHMENT ACCEPT - APN: %s\n", apn);
        } else
          LOG_E(NAS, "PDU SESSION ESTABLISHMENT ACCEPT - DNN IE has invalid length\n");
        resPtr = curPtr;
        curPtr = buffer + msg_length; // we force stop processing
        break;
      }

      default:
        PRINT_ERROR("Unknown IEI %d\n", psea_iei);
        curPtr = buffer + msg_length; // we force stop processing
        break;
    }
  }
  return resPtr - buffer;
}
