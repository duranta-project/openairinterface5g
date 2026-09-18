/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "ran_func_gtp.h"

#include <assert.h>

#include "common/ran_context.h"
#include "openair2/LAYER2/NR_MAC_gNB/mac_proto.h"
#include "openair2/E2AP/flexric/src/util/time_now_us.h"
#include "openair2/LAYER2/nr_pdcp/nr_pdcp_oai_api.h"
#include "ds/seq_arr.h"

#if defined (NGRAN_GNB_CUCP)
#include "openair2/RRC/NR/rrc_gNB_UE_context.h"
#include "openair2/RRC/NR/rrc_gNB_radio_bearers.h"
#endif

bool read_gtp_sm(void * data)
{
  assert(data != NULL);

  gtp_ind_data_t* gtp = (gtp_ind_data_t*)(data);
  //fill_gtp_ind_data(gtp);

  gtp->msg.tstamp = time_now_us();

  uint64_t ue_id_list[MAX_MOBILES_PER_GNB];
  size_t num_ues = nr_pdcp_get_num_ues(ue_id_list, MAX_MOBILES_PER_GNB);
  
  gtp->msg.len = num_ues;
  if(gtp->msg.len > 0){
    gtp->msg.ngut = calloc(gtp->msg.len, sizeof(gtp_ngu_t_stats_t) );
    assert(gtp->msg.ngut != NULL);
  }
  else {
    return false;
  }

  #if defined (NGRAN_GNB_CUCP) && defined (NGRAN_GNB_CUUP)
  if (RC.nrrrc[0]->node_type == ngran_gNB_DU || RC.nrrrc[0]->node_type == ngran_gNB_CUCP) return false;
  assert((RC.nrrrc[0]->node_type == ngran_gNB_CU || RC.nrrrc[0]->node_type == ngran_gNB) && "Expected node types: CU or gNB-mono");

  for (size_t i = 0; i < num_ues; i++) {
    rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(RC.nrrrc[0], ue_id_list[i]);
    // UE disconnected between nr_pdcp_get_num_ues() and this lookup
    if (ue_context_p == NULL)
      continue;

    gtp->msg.ngut[i].rnti = ue_id_list[i];
    FOR_EACH_SEQ_ARR(rrc_pdu_session_param_t*, session, &ue_context_p->ue_context.pduSessions) {
      gtp->msg.ngut[i].teidgnb = session->param.n3_outgoing.teid;
      gtp->msg.ngut[i].teidupf = session->param.n3_incoming.teid;
      // TODO: one PDU session has multiple QoS Flow
      DevAssert(seq_arr_size(&session->param.qos) == 1);
      FOR_EACH_SEQ_ARR(nr_rrc_qos_t *, qos, &session->param.qos) {
        gtp->msg.ngut[i].qfi = qos->qos.qfi;
      }
      break;
    }
  }

  return true;

  #elif defined (NGRAN_GNB_CUUP)
  // For the moment, CU-UP doesn't store PDU session information
  printf("GTP SM not yet implemented in CU-UP\n");
  return false;  
  #endif
}

void read_gtp_setup_sm(void* data)
{
  assert(data != NULL);
  assert(0 !=0 && "Not supported");
}

sm_ag_if_ans_t write_ctrl_gtp_sm(void const* src)
{
  assert(src != NULL);
  printf("write_ctrl callback for GTP SM: operation not supported\n");
  sm_ag_if_ans_t ans = {0};
  return ans;
}
