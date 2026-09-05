/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef PDU_SESSION_MODIFICATION_COMMAND_H_
#define PDU_SESSION_MODIFICATION_COMMAND_H_

#include <stdint.h>
#include <stdbool.h>
#include "fgs_nas_utils.h"
#include "PduSessionEstablishmentAccept.h"

/* PDU Session Modification Command Optional IE Identifiers - TS 24.501 Table 8.3.9.1.1 */

#define FOREACH_MOD_IEI(IEI_DEF)                                                      \
  IEI_DEF(IEI_MOD_5GSM_CAUSE, 0x59) /* 5GSM cause 9.11.4.2  */                        \
  IEI_DEF(IEI_MOD_SESSION_AMBR, 0x2A) /* Session-AMBR 9.11.4.14 */                    \
  IEI_DEF(IEI_MOD_RQ_TIMER_VALUE, 0x56) /* GPRS timer 9.11.2.3  */                    \
  IEI_DEF(IEI_MOD_ALWAYSON_PDU, 0x80) /* Always-on PDU session indication 9.11.4.3 */ \
  IEI_DEF(IEI_MOD_AUTH_QOS_RULES, 0x7A) /* QoS rules 9.11.4.13 */                     \
  IEI_DEF(IEI_MOD_MAPPED_EPS, 0x75) /* Mapped EPS bearer contexts */                  \
  IEI_DEF(IEI_MOD_AUTH_QOS_DESC, 0x79) /* QoS flow descriptions 9.11.4.12 */          \
  IEI_DEF(IEI_MOD_EXT_CONF_OPT, 0x7B) /* Extended protocol config options */

static const text_info_t mod_iei_text_desc[] = {FOREACH_MOD_IEI(TO_TEXT)};

typedef enum { FOREACH_MOD_IEI(TO_ENUM) } pduSessionModification_IEI_t;

/* PDU Session Modification Command message - TS 24.501 Table 8.3.9.1.1 */
typedef struct pdu_session_modification_command_msg_s {
  // 5GSM cause (O)
  uint8_t cause;
  bool cause_present;
  // Session-AMBR (O)
  session_ambr_t sess_ambr;
  bool sess_ambr_present;
  // Authorized QoS rules (O)
  auth_qos_rule_t qos_rules;
  bool qos_rules_present;
  // QoS flow descriptions (O)
  qos_fd_t qos_fd_ie;
  bool qos_fd_present;
  // Extended Protocol Configuration Options (O)
  ext_pP_t ext_pp_ie;
  bool ext_pp_present;
} pdu_session_modification_command_msg_t;

int decode_pdu_session_modification_command(pdu_session_modification_command_msg_t *psmc_msg,
                                            uint8_t *buffer,
                                            uint32_t msg_length);

#endif /* PDU_SESSION_MODIFICATION_COMMAND_H_ */
