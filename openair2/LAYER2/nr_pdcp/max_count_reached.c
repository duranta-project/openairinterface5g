/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "max_count_reached.h"

#include "nr_pdcp_entity.h"
#include "intertask_interface.h"

void nr_pdcp_max_count_reached_srb(nr_pdpc_max_count_reached_data_t max_count_reached_data)
{
  MessageDef *msg = itti_alloc_new_message(TASK_PDCP_GNB, 0, NR_RRC_PDCP_SRB_MAX_COUNT_REACHED);
  nr_rrc_pdcp_srb_max_count_reached_t *m = &NR_RRC_PDCP_SRB_MAX_COUNT_REACHED(msg);
  m->ue_id = max_count_reached_data.ue_id;
  m->srb_id = max_count_reached_data.rb_id;
  itti_send_msg_to_task(TASK_RRC_GNB, 0, msg);
}

void nr_pdcp_max_count_reached_drb(nr_pdpc_max_count_reached_data_t max_count_reached_data)
{
  /* todo */
}
