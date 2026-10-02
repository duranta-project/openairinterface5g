/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include "PduSessionModificationCommand.h"
#include "fgsm_lib.h"
#include "common/utils/LOG/log.h"
#include "common/utils/utils.h"
#include "fgs_nas_utils.h"

/**
 * @brief PDU session modification command (8.3.9 of 3GPP TS 24.501)
 *        network to UE
 */
int decode_pdu_session_modification_command(pdu_session_modification_command_msg_t *psmc_msg,
                                            uint8_t *buffer,
                                            uint32_t msg_length)
{
  uint8_t *curPtr = buffer;

  /* All IEs are optional in PDU Session Modification Command */
  while (curPtr < buffer + msg_length) {
    uint8_t psmc_iei = *curPtr++;
    LOG_T(NAS, "PDU SESSION MODIFICATION COMMAND - Received IEI 0x%02x\n", psmc_iei);

    switch (psmc_iei) {
      case IEI_MOD_5GSM_CAUSE:
        psmc_msg->cause = *curPtr++;
        psmc_msg->cause_present = true;
        LOG_I(NAS, "Received PDU Session Modification Command, 5GSM cause: 0x%02x\n", psmc_msg->cause);
        break;

      case IEI_MOD_SESSION_AMBR:
        psmc_msg->sess_ambr.length = *curPtr++;
        psmc_msg->sess_ambr.unit_dl = *curPtr++;
        GET_SHORT(curPtr, psmc_msg->sess_ambr.sess_dl);
        curPtr += sizeof(psmc_msg->sess_ambr.sess_dl);
        psmc_msg->sess_ambr.unit_ul = *curPtr++;
        GET_SHORT(curPtr, psmc_msg->sess_ambr.sess_ul);
        curPtr += sizeof(psmc_msg->sess_ambr.sess_ul);
        psmc_msg->sess_ambr_present = true;
        LOG_I(NAS,
              "Received PDU Session Modification Command, Session-AMBR: DL unit=%d rate=%d, UL unit=%d rate=%d\n",
              psmc_msg->sess_ambr.unit_dl,
              psmc_msg->sess_ambr.sess_dl,
              psmc_msg->sess_ambr.unit_ul,
              psmc_msg->sess_ambr.sess_ul);
        break;

      case IEI_MOD_RQ_TIMER_VALUE:
        curPtr++; /* TS 24.008 10.5.7.3 - skip for now */
        break;

      case IEI_MOD_ALWAYSON_PDU:
      case IEI_MOD_ALWAYSON_PDU | 0x01: /* APSI flag (bit 1) set */
        break;

      case IEI_MOD_AUTH_QOS_RULES: {
        auth_qos_rule_t *qos_rules = &psmc_msg->qos_rules;
        // Length of the rule IEs (2 octets)
        GET_SHORT(curPtr, qos_rules->length);
        curPtr += sizeof(qos_rules->length);

        /* Decode rule IEs as long as the total length of all IEs is not reached */
        uint16_t rules_tot_len = 0;
        int rule_idx = 0;

        while (rules_tot_len < qos_rules->length && rule_idx < MAX_NUM_QOS_RULES) {
          qos_rules->rule[rule_idx] = decode_qos_rule(curPtr);

          uint16_t rule_len = get_len_qos_rule(&qos_rules->rule[rule_idx]);
          rules_tot_len += rule_len;
          curPtr += rule_len;

          LOG_I(NAS,
                "Received PDU Session Modification Command, QoS Rule %d: op=%d qfi=%d dqr=%d nb_pf=%d (decoded %d filters)\n",
                qos_rules->rule[rule_idx].id,
                qos_rules->rule[rule_idx].oc,
                qos_rules->rule[rule_idx].qfi,
                qos_rules->rule[rule_idx].dqr,
                qos_rules->rule[rule_idx].nb_pf,
                qos_rules->rule[rule_idx].num_packet_filters);
          rule_idx++;
        }
        psmc_msg->qos_rules.num_rules = rule_idx;
        psmc_msg->qos_rules_present = true;
        break;
      }

      case IEI_MOD_MAPPED_EPS: {
        uint16_t mapped_eps_length = 0;
        GET_SHORT(curPtr, mapped_eps_length);
        curPtr += (mapped_eps_length + sizeof(mapped_eps_length));
        break;
      }

      case IEI_MOD_AUTH_QOS_DESC: {
        GET_SHORT(curPtr, psmc_msg->qos_fd_ie.length);
        curPtr += (psmc_msg->qos_fd_ie.length + sizeof(psmc_msg->qos_fd_ie.length));
        psmc_msg->qos_fd_present = true;
        break;
      }

      case IEI_MOD_EXT_CONF_OPT: {
        GET_SHORT(curPtr, psmc_msg->ext_pp_ie.length);
        curPtr += (psmc_msg->ext_pp_ie.length + sizeof(psmc_msg->ext_pp_ie.length));
        psmc_msg->ext_pp_present = true;
        break;
      }

      default:
        LOG_W(NAS, "PDU SESSION MODIFICATION COMMAND - Unknown IEI 0x%02x\n", psmc_iei);
        return -1;
    }
  }

  return curPtr - buffer;
}
