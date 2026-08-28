/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* gNB application layer -> XNAP messages */
MESSAGE_DEF(XNAP_REGISTER_GNB_REQ, MESSAGE_PRIORITY_MED, xnap_register_gnb_req_t, xnap_register_gnb_req)

/* RRC -> XNAP messages */
MESSAGE_DEF(XNAP_F1_SETUP_DONE_IND, MESSAGE_PRIORITY_MED, xnap_f1_setup_done_ind_t, xnap_f1_setup_done_ind)

/* XNAP -> RRC messages */
MESSAGE_DEF(XNAP_SETUP_IND,          MESSAGE_PRIORITY_MED, xnap_setup_ind_t,          xnap_setup_ind)
MESSAGE_DEF(XNAP_PEER_SHUTDOWN_IND,  MESSAGE_PRIORITY_MED, xnap_peer_shutdown_ind_t,  xnap_peer_shutdown_ind)
