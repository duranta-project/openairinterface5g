/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _RRC_GNB_DRBS_H_
#define _RRC_GNB_DRBS_H_

#include <stdbool.h>
#include <stdint.h>
#include "e1ap_messages_types.h"
#include "nr_rrc_defs.h"

/// @brief retrieve the data structure representing DRB with ID drb_id of UE ue
drb_t *get_drb(seq_arr_t *seq, int id);

/// @brief retrieve PDU session of UE ue with ID id
rrc_pdu_session_param_t *find_pduSession(seq_arr_t *seq, int id);

/// @brief Add a new PDU session for UE @param ue and configuration @param in
rrc_pdu_session_param_t *add_pduSession(seq_arr_t *sessions_ptr, const pdusession_t *in);

/// @brief get PDU session of UE ue through the DRB drb_id
rrc_pdu_session_param_t *find_pduSession_from_drbId(gNB_RRC_UE_t *ue, int drb_id);

/// @brief Remove PDU Session from RRC list
/// Also removes all associated DRBs for this PDU session.
bool rm_pduSession(seq_arr_t *sessions, seq_arr_t *drbs, int pdusession_id);

/// @brief set PDCP configuration in E1 Bearer Context Management message
bearer_context_pdcp_config_t set_bearer_context_pdcp_config(const nr_pdcp_configuration_t pdcp,
                                                            bool um_on_default_drb,
                                                            const nr_redcap_ue_cap_t *redcap_cap);

void free_pdusession(void *ptr);

/// @brief Add DRB to RRC list
drb_t *nr_rrc_add_drb(seq_arr_t *drb_ptr, int pdusession_id, nr_pdcp_configuration_t *pdcp);

/// @brief Function to free DRB in RRC
void free_drb(void *ptr);

/// @brief retrieve QoS flow associated to @param qfi
nr_rrc_qos_t *find_qos(seq_arr_t *seq, int qfi);

/// @brief Add a new QoS to the list
nr_rrc_qos_t *add_qos(seq_arr_t *qos, const pdusession_level_qos_parameter_t *in);

/// @brief Remove a single QoS flow identified by @param qfi from the list
bool rm_qos(seq_arr_t *qos, int qfi);

/// @brief Whether any QoS flow of @param session is mapped to DRB @param drb_id
bool nr_rrc_drb_has_qos_flow(const rrc_pdu_session_param_t *session, int drb_id);

/// @brief Collect into @param out the DRBs of @param session that no QoS flow maps to
/// any more, i.e. the ones a session modification released
/// @return the number of DRB IDs written to @param out
int nr_rrc_collect_released_drbs(const seq_arr_t *drbs, const rrc_pdu_session_param_t *session, int out[MAX_DRBS_PER_UE]);

/// @brief Find the first DRB for a given PDU session ID
drb_t *find_drb_by_pdusession_id(seq_arr_t *seq, int pdusession_id);

/// @brief Remove DRB with given @param drb_id from the list
bool nr_rrc_remove_drb_by_id(seq_arr_t *drb_ptr, const int drb_id);

/// @brief Add PDU Sessions and DRBs to UE context list
void nr_rrc_add_bearers(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, int n, pdusession_t *sessions);

/// @brief Find existing DRB that can accept a QoS flow based on resource type and multiplexing limits
int nr_rrc_find_suitable_drb_for_qos(gNB_RRC_UE_t *UE,
                                     const int pdusession_id,
                                     const pdusession_level_qos_parameter_t *qos_params,
                                     const seq_arr_t *flows);

/// @brief Find or create a DRB for a QoS flow and assign it
bool nr_rrc_assign_drb_to_qos_flow(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, const pdusession_t *session, nr_rrc_qos_t *qos);

/// @brief Check if 5QI value is a standardized value (table 5.7.4-1 in 3GPP TS 23.501)
bool is_5qi_standardized(uint16_t five_qi);

/// @brief Check if a standardized 5QI corresponds to a Non-GBR resource type (from TS 23.501 Table 5.7.4-1)
bool nr_rrc_is_non_gbr_fiveqi(uint16_t five_qi);

#endif
