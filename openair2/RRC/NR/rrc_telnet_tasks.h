/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef RRC_TELNET_TASKS_H_
#define RRC_TELNET_TASKS_H_

#include "ngap_gNB_ue_context.h"
#include "rrc_gNB_du.h"
#include "ran_context.h"
#include "rrc_gNB_UE_context.h"
#include "rrc_gNB_NGAP.h"

void rrc_get_single_ue_rnti(MessageDef *msg_p, instance_t instance);
void rrc_get_ue_context_by_ue_id(MessageDef *msg_p, instance_t instance);
void rrc_get_du_id_by_ue_id(MessageDef *msg_p, instance_t instance);
void rrc_get_ue_context_by_rnti_any_du(MessageDef *msg_p, instance_t instance);
void rrc_trigger_f1_ho(MessageDef *msg_p, instance_t instance);
void rrc_trigger_n2_ho(MessageDef *msg_p, instance_t instance);
void rrc_get_ngap_ue_id(MessageDef *msg_p, instance_t instance);
void rrc_check_ue_context(MessageDef *msg_p, instance_t instance);
void rrc_gnb_generate_rrcrelease(MessageDef *msg_p, instance_t instance);
void rrc_gnb_generate_rrcrelease_all(MessageDef *msg_p, instance_t instance);
void rrc_gnb_trigger_ue_context_release_req(MessageDef *msg_p, instance_t instance);

#endif /* RRC_TELNET_TASKS_H_ */
