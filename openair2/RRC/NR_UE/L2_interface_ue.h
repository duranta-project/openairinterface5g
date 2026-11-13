/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "common/utils/ocp_itti/intertask_interface.h"

typedef uint32_t channel_t;

#ifndef _L2_INTERFACE_UE_H_
#define _L2_INTERFACE_UE_H_
void nr_mac_rrc_sync_ind(const module_id_t module_id, const frame_t frame, const bool in_sync);
void nr_mac_rrc_data_ind_ue(const module_id_t module_id,
                            const uint8_t gNB_index,
                            const int hfn,
                            const frame_t frame,
                            const int slot,
                            const uint32_t cellid,
                            const long arfcn,
                            const channel_t channel,
                            const uint8_t* pduP,
                            const sdu_size_t pdu_len);
void nr_mac_rrc_meas_ind_ue(module_id_t module_id,
                            uint32_t gNB_index,
                            uint16_t Nid_cell,
                            bool csi_meas,
                            bool is_neighboring_cell,
                            int rsrp_dBm);

void nr_mac_rrc_inactivity_timer_ind(const module_id_t mod_id);
void nr_mac_rrc_msg3_ind(const module_id_t mod_id, const int rnti, bool prepare_payload);
void nr_ue_rrc_timer_trigger(int instance, int hfn, int frame, int gnb_id);
void nr_mac_rrc_ra_ind(const module_id_t mod_id, bool success);
void process_msg_rcc_to_mac(nr_mac_rrc_message_t *msg, int instance_id);
void nr_mac_rrc_verification_failed(const module_id_t mod_id);
#endif

