/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef MAC_RRC_DL_HANDLER_H
#define MAC_RRC_DL_HANDLER_H

#include "common/platform_types.h"
#include "f1ap_messages_types.h"
#include "openair2/RRC/NR/MESSAGES/asn1_msg.h"
#include "nr_mac_gNB.h"

void f1_reset_cu_initiated(const f1ap_reset_t *reset);
void f1_reset_acknowledge_du_initiated(const f1ap_reset_ack_t *ack);
void f1_setup_response(const f1ap_setup_resp_t *resp);
void f1_setup_failure(const f1ap_setup_failure_t *failure);
void gnb_du_configuration_update_acknowledge(const f1ap_gnb_du_configuration_update_acknowledge_t *ack);
NR_CellGroupConfig_t *clone_CellGroupConfig(const NR_CellGroupConfig_t *orig);
void ue_context_setup_request(const f1ap_ue_context_setup_req_t *req);
void ue_context_modification_request(const f1ap_ue_context_mod_req_t *req);
void ue_context_modification_confirm(const f1ap_ue_context_modif_confirm_t *confirm);
void ue_context_modification_refuse(const f1ap_ue_context_modif_refuse_t *refuse);
void ue_context_release_command(const f1ap_ue_context_rel_cmd_t *cmd);

void dl_rrc_message_transfer(const f1ap_dl_rrc_message_t *dl_rrc);
void f1_paging(const f1ap_paging_t *paging);

void trp_information_request(const f1ap_trp_information_req_t *req);
void positioning_information_request(const f1ap_positioning_information_req_t *req);
void positioning_activation_request(const f1ap_positioning_activation_req_t *req);
void positioning_measurement_request(const f1ap_positioning_measurement_req_t *req);

NR_CG_ConfigInfo_t *get_cg_config_info(uint8_t *buf, uint32_t len);
NR_UE_NR_Capability_t *get_ue_nr_cap_from_cg_config_info(const NR_CG_ConfigInfo_t *cgci);
int handle_ue_context_drbs_setup(NR_UE_info_t *UE,
                                 int drbs_len,
                                 const f1ap_drb_to_setup_t *req_drbs,
                                 f1ap_drb_setup_t **resp_drbs,
                                 NR_CellGroupConfig_t *cellGroupConfig,
                                 const nr_rlc_configuration_t *rlc_config);

byte_array_t encode_cellgroup_config(const NR_CellGroupConfig_t *cellGroup);

#endif /* MAC_RRC_DL_HANDLER_H */
