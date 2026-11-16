/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "openair2/COMMON/f1ap_messages_types.h"
/* To setup F1 at DU */
MESSAGE_DEF(F1AP_DU_REGISTER_REQ, MESSAGE_PRIORITY_MED, f1ap_du_register_req_t, f1ap_du_register_req)

/* RESET */
MESSAGE_DEF(F1AP_RESET, MESSAGE_PRIORITY_MED, f1ap_reset_t, f1ap_reset)
MESSAGE_DEF(F1AP_RESET_ACK, MESSAGE_PRIORITY_MED, f1ap_reset_ack_t, f1ap_reset_ack)

/* eNB_DU application layer -> F1AP messages or CU F1AP -> RRC*/
MESSAGE_DEF(F1AP_SETUP_REQ          , MESSAGE_PRIORITY_MED, f1ap_setup_req_t          , f1ap_setup_req)
MESSAGE_DEF(F1AP_BROADCAST_CONTEXT_SETUP_REQ, MESSAGE_PRIORITY_MED, f1ap_broadcast_ctx_setup_t, f1ap_broadcast_ctx_setup)
MESSAGE_DEF(F1AP_GNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE         , MESSAGE_PRIORITY_MED, f1ap_gnb_cu_configuration_update_acknowledge_t          , f1ap_gnb_cu_configuration_update_acknowledge)
MESSAGE_DEF(F1AP_GNB_CU_CONFIGURATION_UPDATE_FAILURE         , MESSAGE_PRIORITY_MED, f1ap_gnb_cu_configuration_update_failure_t          , f1ap_gnb_cu_configuration_update_failure)
MESSAGE_DEF(F1AP_GNB_DU_CONFIGURATION_UPDATE,                MESSAGE_PRIORITY_MED, f1ap_gnb_du_configuration_update_t, f1ap_gnb_du_configuration_update)

/* F1AP -> eNB_DU or eNB_CU_RRC -> F1AP application layer messages */
MESSAGE_DEF(F1AP_SETUP_RESP         , MESSAGE_PRIORITY_MED, f1ap_setup_resp_t          , f1ap_setup_resp)
MESSAGE_DEF(F1AP_SETUP_FAILURE         , MESSAGE_PRIORITY_MED, f1ap_setup_failure_t          , f1ap_setup_failure)
MESSAGE_DEF(F1AP_GNB_CU_CONFIGURATION_UPDATE         , MESSAGE_PRIORITY_MED, f1ap_gnb_cu_configuration_update_t          , f1ap_gnb_cu_configuration_update)
MESSAGE_DEF(F1AP_GNB_DU_CONFIGURATION_UPDATE_ACKNOWLEDGE                , MESSAGE_PRIORITY_MED, f1ap_gnb_du_configuration_update_acknowledge_t, f1ap_gnb_du_configuration_update_acknowledge)
MESSAGE_DEF(F1AP_GNB_DU_CONFIGURATION_UPDATE_FAILURE                , MESSAGE_PRIORITY_MED, f1ap_gnb_du_configuration_update_failure_t, f1ap_gnb_du_configuration_update_failure)

/* F1AP -> RRC to inform about lost connection */
MESSAGE_DEF(F1AP_LOST_CONNECTION, MESSAGE_PRIORITY_MED, f1ap_lost_connection_t, f1ap_lost_connection)

/* MAC -> F1AP messages */
MESSAGE_DEF(F1AP_INITIAL_UL_RRC_MESSAGE           , MESSAGE_PRIORITY_MED, f1ap_initial_ul_rrc_message_t             , f1ap_initial_ul_rrc_message)
MESSAGE_DEF(F1AP_UL_RRC_MESSAGE                , MESSAGE_PRIORITY_MED, f1ap_ul_rrc_message_t                , f1ap_ul_rrc_message)
//MESSAGE_DEF(F1AP_INITIAL_CONTEXT_SETUP_RESP, MESSAGE_PRIORITY_MED, f1ap_initial_context_setup_resp_t, f1ap_initial_context_setup_resp)
//MESSAGE_DEF(F1AP_INITIAL_CONTEXT_SETUP_FAILURE, MESSAGE_PRIORITY_MED, f1ap_initial_context_setup_failure_t, f1ap_initial_context_setup_failure)
MESSAGE_DEF(F1AP_UE_CONTEXT_RELEASE_REQ, MESSAGE_PRIORITY_MED, f1ap_ue_context_rel_req_t, f1ap_ue_context_release_req)
MESSAGE_DEF(F1AP_UE_CONTEXT_RELEASE_CMD, MESSAGE_PRIORITY_MED, f1ap_ue_context_rel_cmd_t, f1ap_ue_context_release_cmd)
MESSAGE_DEF(F1AP_UE_CONTEXT_RELEASE_COMPLETE, MESSAGE_PRIORITY_MED, f1ap_ue_context_rel_cplt_t, f1ap_ue_context_release_complete)
MESSAGE_DEF(F1AP_UE_CONTEXT_MODIFICATION_REQUIRED, MESSAGE_PRIORITY_MED, f1ap_ue_context_modif_required_t, f1ap_ue_context_modification_required)

/* RRC -> F1AP  messages */
MESSAGE_DEF(F1AP_DL_RRC_MESSAGE              , MESSAGE_PRIORITY_MED, f1ap_dl_rrc_message_t              , f1ap_dl_rrc_message )
//MESSAGE_DEF(F1AP_INITIAL_CONTEXT_SETUP_REQ , MESSAGE_PRIORITY_MED, f1ap_initial_context_setup_req_t , f1ap_initial_context_setup_req )
MESSAGE_DEF(F1AP_UE_CONTEXT_SETUP_REQ,  MESSAGE_PRIORITY_MED, f1ap_ue_context_setup_req_t, f1ap_ue_context_setup_req)
MESSAGE_DEF(F1AP_UE_CONTEXT_SETUP_RESP, MESSAGE_PRIORITY_MED, f1ap_ue_context_setup_resp_t, f1ap_ue_context_setup_resp)
MESSAGE_DEF(F1AP_UE_CONTEXT_MODIFICATION_REQ, MESSAGE_PRIORITY_MED, f1ap_ue_context_mod_req_t, f1ap_ue_context_mod_req)
MESSAGE_DEF(F1AP_UE_CONTEXT_MODIFICATION_RESP, MESSAGE_PRIORITY_MED, f1ap_ue_context_mod_resp_t, f1ap_ue_context_modification_resp)
MESSAGE_DEF(F1AP_UE_CONTEXT_MODIFICATION_CONFIRM, MESSAGE_PRIORITY_MED, f1ap_ue_context_modif_confirm_t, f1ap_ue_context_modification_confirm)
MESSAGE_DEF(F1AP_UE_CONTEXT_MODIFICATION_REFUSE, MESSAGE_PRIORITY_MED, f1ap_ue_context_modif_refuse_t, f1ap_ue_context_modification_refuse)

/* CU -> DU*/
MESSAGE_DEF(F1AP_PAGING, MESSAGE_PRIORITY_MED, f1ap_paging_t, f1ap_paging)

/* CU -> DU*/
MESSAGE_DEF(F1AP_TRP_INFORMATION_REQ, MESSAGE_PRIORITY_MED, f1ap_trp_information_req_t, f1ap_trp_information_req)
MESSAGE_DEF(F1AP_POSITIONING_INFORMATION_REQ,
            MESSAGE_PRIORITY_MED,
            f1ap_positioning_information_req_t,
            f1ap_positioning_information_req)
MESSAGE_DEF(F1AP_POSITIONING_ACTIVATION_REQ,
            MESSAGE_PRIORITY_MED,
            f1ap_positioning_activation_req_t,
            f1ap_positioning_activation_req)
MESSAGE_DEF(F1AP_POSITIONING_MEASUREMENT_REQ,
            MESSAGE_PRIORITY_MED,
            f1ap_positioning_measurement_req_t,
            f1ap_positioning_measurement_req)

/* DU -> CU*/
MESSAGE_DEF(F1AP_TRP_INFORMATION_RESP, MESSAGE_PRIORITY_MED, f1ap_trp_information_resp_t, f1ap_trp_information_resp)
MESSAGE_DEF(F1AP_TRP_INFORMATION_FAILURE, MESSAGE_PRIORITY_MED, f1ap_trp_information_failure_t, f1ap_trp_information_failure)
MESSAGE_DEF(F1AP_POSITIONING_INFORMATION_RESP,
            MESSAGE_PRIORITY_MED,
            f1ap_positioning_information_resp_t,
            f1ap_positioning_information_resp)
MESSAGE_DEF(F1AP_POSITIONING_ACTIVATION_RESP,
            MESSAGE_PRIORITY_MED,
            f1ap_positioning_activation_resp_t,
            f1ap_positioning_activation_resp)
MESSAGE_DEF(F1AP_POSITIONING_MEASUREMENT_RESP,
            MESSAGE_PRIORITY_MED,
            f1ap_positioning_measurement_resp_t,
            f1ap_positioning_measurement_resp)
MESSAGE_DEF(F1AP_POSITIONING_MEASUREMENT_FAILURE,
            MESSAGE_PRIORITY_MED,
            f1ap_positioning_measurement_failure_t,
            f1ap_positioning_measurement_failure)
