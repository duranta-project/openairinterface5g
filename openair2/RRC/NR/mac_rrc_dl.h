/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef MAC_RRC_DL_H
#define MAC_RRC_DL_H

#include "common/platform_types.h"
#include "f1ap_messages_types.h"

typedef void (*f1_reset_cu_initiated_func_t)(sctp_assoc_t assoc_id, const f1ap_reset_t *reset);
typedef void (*f1_reset_acknowledge_du_initiated_func_t)(sctp_assoc_t assoc_id, const f1ap_reset_ack_t *ack);

typedef void (*f1_setup_response_func_t)(sctp_assoc_t assoc_id, const f1ap_setup_resp_t *resp);
typedef void (*f1_setup_failure_func_t)(sctp_assoc_t assoc_id, const f1ap_setup_failure_t *fail);
typedef void (*gnb_du_configuration_update_ack_func_t)(sctp_assoc_t assoc_id,
                                                       const f1ap_gnb_du_configuration_update_acknowledge_t *ack);

typedef void (*ue_context_setup_request_func_t)(sctp_assoc_t assoc_id, const f1ap_ue_context_setup_req_t *req);
typedef void (*ue_context_modification_request_func_t)(sctp_assoc_t assoc_id, const f1ap_ue_context_mod_req_t *req);
typedef void (*ue_context_modification_confirm_func_t)(sctp_assoc_t assoc_id, const f1ap_ue_context_modif_confirm_t *confirm);
typedef void (*ue_context_modification_refuse_func_t)(sctp_assoc_t assoc_id, const f1ap_ue_context_modif_refuse_t *refuse);
typedef void (*ue_context_release_command_func_t)(sctp_assoc_t assoc_id, const f1ap_ue_context_rel_cmd_t *cmd);

typedef void (*dl_rrc_message_transfer_func_t)(sctp_assoc_t assoc_id, const f1ap_dl_rrc_message_t *dl_rrc);
typedef void (*f1_paging_transfer_func_t)(sctp_assoc_t assoc_id, const f1ap_paging_t *paging);

/* A broadcast session has no UE, so this is not a UE context procedure. */
typedef void (*broadcast_context_setup_func_t)(sctp_assoc_t assoc_id, const f1ap_broadcast_ctx_setup_t *req);

typedef void (*trp_information_request_func_t)(sctp_assoc_t assoc_id, const f1ap_trp_information_req_t *req);
typedef void (*positioning_information_request_func_t)(sctp_assoc_t assoc_id, const f1ap_positioning_information_req_t *req);
typedef void (*positioning_activation_request_func_t)(sctp_assoc_t assoc_id, const f1ap_positioning_activation_req_t *req);
typedef void (*positioning_measurement_request_func_t)(sctp_assoc_t assoc_id, const f1ap_positioning_measurement_req_t *req);

struct nr_mac_rrc_dl_if_s;
void mac_rrc_dl_direct_init(struct nr_mac_rrc_dl_if_s *mac_rrc);
void mac_rrc_dl_f1ap_init(struct nr_mac_rrc_dl_if_s *mac_rrc);

#endif /* MAC_RRC_DL_H */
