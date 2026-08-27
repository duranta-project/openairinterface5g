/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * mac_messages_def.h
 */

//-------------------------------------------------------------------------------------------//
// Messages between RRC and MAC layers
MESSAGE_DEF(RRC_MAC_IN_SYNC_IND,        MESSAGE_PRIORITY_MED_PLUS, RrcMacInSyncInd,             rrc_mac_in_sync_ind)
MESSAGE_DEF(RRC_MAC_OUT_OF_SYNC_IND,    MESSAGE_PRIORITY_MED_PLUS, RrcMacOutOfSyncInd,          rrc_mac_out_of_sync_ind)
MESSAGE_DEF(NR_RRC_MAC_SYNC_IND,        MESSAGE_PRIORITY_MED_PLUS, NRRrcMacSyncInd,             nr_rrc_mac_sync_ind)

MESSAGE_DEF(RRC_MAC_BCCH_DATA_REQ,      MESSAGE_PRIORITY_MED_PLUS, RrcMacBcchDataReq,           rrc_mac_bcch_data_req)
MESSAGE_DEF(RRC_MAC_BCCH_DATA_IND,      MESSAGE_PRIORITY_MED_PLUS, RrcMacBcchDataInd,           rrc_mac_bcch_data_ind)

MESSAGE_DEF(RRC_MAC_BCCH_MBMS_DATA_REQ,      MESSAGE_PRIORITY_MED_PLUS, RrcMacBcchMbmsDataReq,           rrc_mac_bcch_mbms_data_req)
MESSAGE_DEF(RRC_MAC_BCCH_MBMS_DATA_IND,      MESSAGE_PRIORITY_MED_PLUS, RrcMacBcchMbmsDataInd,           rrc_mac_bcch_mbms_data_ind)

MESSAGE_DEF(RRC_MAC_CCCH_DATA_REQ,      MESSAGE_PRIORITY_MED_PLUS, RrcMacCcchDataReq,           rrc_mac_ccch_data_req)
MESSAGE_DEF(RRC_MAC_CCCH_DATA_CNF,      MESSAGE_PRIORITY_MED_PLUS, RrcMacCcchDataCnf,           rrc_mac_ccch_data_cnf)
MESSAGE_DEF(RRC_MAC_CCCH_DATA_IND,      MESSAGE_PRIORITY_MED_PLUS, RrcMacCcchDataInd,           rrc_mac_ccch_data_ind)


MESSAGE_DEF(RRC_MAC_MCCH_DATA_REQ,      MESSAGE_PRIORITY_MED_PLUS, RrcMacMcchDataReq,           rrc_mac_mcch_data_req)
MESSAGE_DEF(RRC_MAC_MCCH_DATA_IND,      MESSAGE_PRIORITY_MED_PLUS, RrcMacMcchDataInd,           rrc_mac_mcch_data_ind)

MESSAGE_DEF(RRC_MAC_PCCH_DATA_REQ,      MESSAGE_PRIORITY_MED_PLUS, RrcMacPcchDataReq,           rrc_mac_pcch_data_req)

MESSAGE_DEF(NR_RRC_MAC_RA_IND,          MESSAGE_PRIORITY_MED_PLUS, NRRrcMacRaInd,               nr_rrc_mac_ra_ind)
MESSAGE_DEF(NR_RRC_MAC_MSG3_IND,        MESSAGE_PRIORITY_MED_PLUS, NRRrcMacMsg3Ind,             nr_rrc_mac_msg3_ind)
MESSAGE_DEF(NR_RRC_MAC_INAC_IND,        MESSAGE_PRIORITY_MED_PLUS, NRRrcMacInacInd,             nr_rrc_mac_inac_ind)
MESSAGE_DEF(NR_RRC_MAC_VERIFY,          MESSAGE_PRIORITY_MED_PLUS, NRRrcMacVerify,              nr_rrc_mac_verify)

/* RRC configures DRX context (MAC timers) of a UE */
MESSAGE_DEF(RRC_MAC_DRX_CONFIG_REQ, MESSAGE_PRIORITY_MED, rrc_mac_drx_config_req_t, rrc_mac_drx_config_req)

// gNB
MESSAGE_DEF(NR_RRC_MAC_CCCH_DATA_IND,    MESSAGE_PRIORITY_MED_PLUS, NRRrcMacCcchDataInd,           nr_rrc_mac_ccch_data_ind)
MESSAGE_DEF(NR_RRC_MAC_BCCH_DATA_IND,    MESSAGE_PRIORITY_MED_PLUS, NRRrcMacBcchDataInd,           nr_rrc_mac_bcch_data_ind)
MESSAGE_DEF(NR_RRC_MAC_SBCCH_DATA_IND,    MESSAGE_PRIORITY_MED_PLUS, NRRrcMacSBcchDataInd,         nr_rrc_mac_sbcch_data_ind)
MESSAGE_DEF(NR_RRC_MAC_PCCH_DATA_IND, MESSAGE_PRIORITY_MED_PLUS, NRRrcMacPcchDataInd, nr_rrc_mac_pcch_data_ind)

// Message to GNB_APP to update SIB19 satellite position information
MESSAGE_DEF(GNB_SAT_POSITION_UPDATE, MESSAGE_PRIORITY_MED, gnb_sat_position_update_t, gnb_sat_position_update)

// nrUE
MESSAGE_DEF(NR_RRC_MAC_MEAS_DATA_IND,    MESSAGE_PRIORITY_MED_PLUS, NRRrcMacMeasDataInd,           nr_rrc_mac_meas_data_ind)

MESSAGE_DEF(RRC_GET_SINGLE_UE_RNTI, MESSAGE_PRIORITY_MED_PLUS, Rrc_get_single_ue_rnti, rrc_get_single_ue_rnti)
MESSAGE_DEF(RRC_GET_UE_CONTEXT_BY_UE_ID, MESSAGE_PRIORITY_MED_PLUS, Rrc_get_single_ue_rnti, rrc_get_ue_context_by_ue_id)
MESSAGE_DEF(RRC_GET_UE_CONTEXT_BY_RNTI_ANY_DU,
            MESSAGE_PRIORITY_MED_PLUS,
            Rrc_get_ue_context_by_rnti_any_du,
            rrc_get_ue_context_by_rnti_any_du)
MESSAGE_DEF(RRC_GET_DU_ID_BY_UE_ID, MESSAGE_PRIORITY_MED_PLUS, Rrc_get_du_id_by_ue_id, rrc_get_du_id_by_ue_id)
MESSAGE_DEF(RRC_NR_F1_HO_TRIGGER, MESSAGE_PRIORITY_MED_PLUS, Rrc_trigger_ho, rrc_trigger_f1_ho)
MESSAGE_DEF(RRC_GET_NGAP_UE_ID, MESSAGE_PRIORITY_MED_PLUS, Rrc_get_ngap_ue_id, rrc_get_ngap_ue_id)
MESSAGE_DEF(RRC_NR_N2_HO_TRIGGER, MESSAGE_PRIORITY_MED_PLUS, Rrc_trigger_ho, rrc_trigger_n2_ho)
MESSAGE_DEF(RRC_CHECK_UE_CONTEXT, MESSAGE_PRIORITY_MED_PLUS, Rrc_check_ue_context, rrc_check_ue_context)
MESSAGE_DEF(RRC_GNB_GENERATE_RRCRELEASE, MESSAGE_PRIORITY_MED_PLUS, Rrc_gnb_generate_rrcrelease, rrc_gnb_generate_rrcrelease)
MESSAGE_DEF(RRC_GNB_GENERATE_RRCRELEASE_ALL,
            MESSAGE_PRIORITY_MED_PLUS,
            Rrc_gnb_generate_rrcrelease_all,
            rrc_gnb_generate_rrcrelease_all)
MESSAGE_DEF(RRC_GNB_TRIGGER_UE_CONTEXT_RELEASE_REQ,
            MESSAGE_PRIORITY_MED_PLUS,
            Rrc_gnb_trigger_ue_context_release_req,
            rrc_gnb_trigger_ue_context_release_req)
MESSAGE_DEF(MAC_FORCE_UL_FAILURE, MESSAGE_PRIORITY_MED_PLUS, Mac_force_ul_failure, mac_force_ul_failure)
MESSAGE_DEF(MAC_GET_UE_RNTI, MESSAGE_PRIORITY_MED_PLUS, Mac_get_ue_rnti, mac_get_ue_rnti)
MESSAGE_DEF(MAC_GET_UE_RNTI_BY_UID, MESSAGE_PRIORITY_MED_PLUS, Mac_get_ue_rnti_by_uid, mac_get_ue_rnti_by_uid)
