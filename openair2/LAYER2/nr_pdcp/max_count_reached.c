/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "max_count_reached.h"

#include "nr_pdcp_entity.h"
#include "cuup_cucp_if.h"
#include "openair2/COMMON/e1ap_messages_types.h"
#include "openair2/GNB_APP/gnb_paramdef.h"
#include "intertask_interface.h"

void nr_pdcp_max_count_reached_srb(max_count_reached_data_t max_count_reached_data)
{
  MessageDef *msg = itti_alloc_new_message(TASK_PDCP_GNB, 0, NR_RRC_PDCP_SRB_MAX_COUNT_REACHED);
  nr_rrc_pdcp_srb_max_count_reached_t *m = &NR_RRC_PDCP_SRB_MAX_COUNT_REACHED(msg);
  m->ue_id = max_count_reached_data.ue_id;
  m->srb_id = max_count_reached_data.rb_id;
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

void nr_pdcp_max_count_reached_drb(max_count_reached_data_t max_count_reached_data)
{
  e1ap_bearer_mod_required_t m = {
    .gNB_cu_cp_ue_id = max_count_reached_data.ue_id,
    .gNB_cu_up_ue_id = max_count_reached_data.ue_id,
    .numPDUSessionsMod = 1,
    .pduSessionMod = &(pdu_session_required_to_mod_t) {
      .sessionId = max_count_reached_data.pdu_session_id,
      .numDRB2Modify = 1,
      .DRBnGRanModList = {
        (DRB_nGRAN_required_to_mod_t) {
          .id = max_count_reached_data.rb_id,
          .cause = {
            .type = E1AP_CAUSE_RADIO_NETWORK,
            .value = E1AP_RADIO_CAUSE_PDCP_COUNT_WRAP_AROUND
          }
        }
      }
    },
  };
  get_e1_if()->bearer_mod_required(&m);
}
