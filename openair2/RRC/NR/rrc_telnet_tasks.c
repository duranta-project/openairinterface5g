/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "./rrc_telnet_tasks.h"

static void rrc_get_single_ue_rnti_helper(MessageDef **msg_p, instance_t instance)
{
  if(!RC.nrrrc){
    (*msg_p)->ittiMsg.rrc_get_single_ue_rnti.has_rrc = false;
    return;
  }
  if (RC.nrrrc[instance] != NULL) {
    rrc_gNB_ue_context_t *ue = NULL;
    int count = 0;

    RB_FOREACH (ue, rrc_nr_ue_tree_s, &RC.nrrrc[instance]->rrc_ue_head) {
      count++;

      if (count == 1) {
        (*msg_p)->ittiMsg.rrc_get_single_ue_rnti.rnti = ue->ue_context.rnti;
        (*msg_p)->ittiMsg.rrc_get_single_ue_rnti.id = ue->ue_context.rrc_ue_id;
        (*msg_p)->ittiMsg.rrc_get_single_ue_rnti.ue_reestablishment_counter = ue->ue_context.ue_reestablishment_counter;
        (*msg_p)->ittiMsg.rrc_get_single_ue_rnti.ue_reconfiguration_counter = ue->ue_context.ue_reconfiguration_counter;
        (*msg_p)->ittiMsg.rrc_get_single_ue_rnti.is_single = true;
        (*msg_p)->ittiMsg.rrc_get_single_ue_rnti.has_rrc = true;
      }

      if (count >= 2) {
        (*msg_p)->ittiMsg.rrc_get_single_ue_rnti.is_single = false;
        break;
      }
    }
  }
}

void rrc_get_single_ue_rnti(MessageDef *msg_p, instance_t instance)
{
  MessageDef *resp_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_SINGLE_UE_RNTI);
  rrc_get_single_ue_rnti_helper(&resp_p, instance);
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

void rrc_get_ue_context_by_rnti_any_du(MessageDef *msg_p, instance_t instance)
{
  MessageDef *resp_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_UE_CONTEXT_BY_RNTI_ANY_DU);
  resp_p->ittiMsg.rrc_get_ue_context_by_rnti_any_du.rnti = msg_p->ittiMsg.rrc_get_ue_context_by_rnti_any_du.rnti;
  resp_p->ittiMsg.rrc_get_ue_context_by_rnti_any_du.ue_context_exists =
      (rrc_gNB_get_ue_context_by_rnti_any_du(RC.nrrrc[instance], msg_p->ittiMsg.rrc_get_ue_context_by_rnti_any_du.rnti) != NULL);
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

void rrc_check_ue_context(MessageDef *msg_p, instance_t instance)
{
  MessageDef *resp_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_CHECK_UE_CONTEXT);
  resp_p->ittiMsg.rrc_check_ue_context.id = msg_p->ittiMsg.rrc_check_ue_context.id;
  if (RC.nrrrc[instance] != NULL) {
    rrc_gNB_ue_context_t *ue = rrc_gNB_get_ue_context(RC.nrrrc[instance], msg_p->ittiMsg.rrc_check_ue_context.id);
    if (!ue) {
      resp_p->ittiMsg.rrc_check_ue_context.check = false;
    } else {
      resp_p->ittiMsg.rrc_check_ue_context.check = true;
    }
  }
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

static void rrc_get_ue_context_by_ue_id_helper(MessageDef **msg_p, ue_id_t ue_id, instance_t instance)
{
  if (ue_id == -1) {
    rrc_get_single_ue_rnti_helper(msg_p, instance);
    (*msg_p)->ittiMsg.rrc_get_ue_context_by_ue_id.id = (*msg_p)->ittiMsg.rrc_get_single_ue_rnti.id;
  } else {
    (*msg_p)->ittiMsg.rrc_get_ue_context_by_ue_id.id = ue_id;
  }
  if ((*msg_p)->ittiMsg.rrc_get_ue_context_by_ue_id.id != -1) {
    rrc_gNB_ue_context_t *ue = NULL;
    ue = rrc_gNB_get_ue_context(RC.nrrrc[instance], (*msg_p)->ittiMsg.rrc_get_ue_context_by_ue_id.id);
    if (ue) {
      (*msg_p)->ittiMsg.rrc_get_ue_context_by_ue_id.rnti = ue->ue_context.rnti;
      (*msg_p)->ittiMsg.rrc_get_ue_context_by_ue_id.ue_reestablishment_counter = ue->ue_context.ue_reestablishment_counter;
      (*msg_p)->ittiMsg.rrc_get_ue_context_by_ue_id.ue_reconfiguration_counter = ue->ue_context.ue_reconfiguration_counter;
      (*msg_p)->ittiMsg.rrc_get_ue_context_by_ue_id.rrc_ue_id = ue->ue_context.rrc_ue_id;
      (*msg_p)->ittiMsg.rrc_get_ue_context_by_ue_id.is_single = true;
    } else {
      LOG_E(RRC, "Could not find UE context associated with UE ID %lu\n", ue_id);
    }
  }
}

void rrc_get_ue_context_by_ue_id(MessageDef *msg_p, instance_t instance)
{
  MessageDef *resp_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_UE_CONTEXT_BY_UE_ID);
  rrc_get_ue_context_by_ue_id_helper(&resp_p, msg_p->ittiMsg.rrc_get_ue_context_by_ue_id.id, instance);
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

static void rrc_get_du_id_by_ue_id_helper(MessageDef **resp_p, int ue_id, instance_t instance)
{
  if (ue_id != -1) {
    (*resp_p)->ittiMsg.rrc_get_du_id_by_ue_id.ue_id = ue_id;
  } else {
    rrc_get_single_ue_rnti_helper(resp_p, instance);
    if ((*resp_p)->ittiMsg.rrc_get_single_ue_rnti.id > 0) {
      (*resp_p)->ittiMsg.rrc_get_du_id_by_ue_id.ue_id = (*resp_p)->ittiMsg.rrc_get_single_ue_rnti.id;
    }
  }
  if ((*resp_p)->ittiMsg.rrc_get_du_id_by_ue_id.ue_id != -1) {
    nr_rrc_du_container_t *du = get_du_for_ue(RC.nrrrc[instance], (*resp_p)->ittiMsg.rrc_get_du_id_by_ue_id.ue_id);
    if (du != NULL && du->gNB_DU_id != 0) {
      (*resp_p)->ittiMsg.rrc_get_du_id_by_ue_id.du_id = du->gNB_DU_id;
      (*resp_p)->ittiMsg.rrc_get_du_id_by_ue_id.no_du = false;
    } else {
      (*resp_p)->ittiMsg.rrc_get_du_id_by_ue_id.no_du = true;
    }
  }
}

void rrc_get_du_id_by_ue_id(MessageDef *msg_p, instance_t instance)
{
  MessageDef *resp_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_DU_ID_BY_UE_ID);
  rrc_get_du_id_by_ue_id_helper(&resp_p, msg_p->ittiMsg.rrc_get_du_id_by_ue_id.ue_id, instance);
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

extern void nr_F1_HO_trigger_telnet(gNB_RRC_INST *rrc, uint32_t rrc_ue_id);
extern void nr_N2_HO_trigger_telnet(gNB_RRC_INST *rrc, uint32_t neighbour_pci, uint32_t rrc_ue_id);

void rrc_trigger_f1_ho(MessageDef *msg_p, instance_t instance)
{
  nr_F1_HO_trigger_telnet(RC.nrrrc[instance], msg_p->ittiMsg.rrc_trigger_f1_ho.id);
}

void rrc_trigger_n2_ho(MessageDef *msg_p, instance_t instance)
{
  nr_N2_HO_trigger_telnet(RC.nrrrc[instance], msg_p->ittiMsg.rrc_trigger_n2_ho.neighbour_pci, msg_p->ittiMsg.rrc_trigger_n2_ho.id);
}

void rrc_get_ngap_ue_id(MessageDef *msg_p, instance_t instance)
{
  if (msg_p->ittiMsg.rrc_get_ngap_ue_id.gNB_ue_ngap_id == -1) {
    MessageDef *resp_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_SINGLE_UE_RNTI);
    rrc_get_single_ue_rnti_helper(&resp_p, instance);
    msg_p->ittiMsg.rrc_get_ngap_ue_id.gNB_ue_ngap_id = resp_p->ittiMsg.rrc_get_single_ue_rnti.id;
    free(resp_p);
  }
  MessageDef *resp_p2 = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_NGAP_UE_ID);
  resp_p2->ittiMsg.rrc_get_ngap_ue_id.gNB_ue_ngap_id = msg_p->ittiMsg.rrc_get_ngap_ue_id.gNB_ue_ngap_id;
  ngap_gNB_ue_context_t *ngap_ue_context = ngap_get_ue_context(msg_p->ittiMsg.rrc_get_ngap_ue_id.gNB_ue_ngap_id);
  if (ngap_ue_context) {
    resp_p2->ittiMsg.rrc_get_ngap_ue_id.amf_ue_ngap_id = ngap_ue_context->amf_ue_ngap_id;
    resp_p2->ittiMsg.rrc_get_ngap_ue_id.gNB_ue_ngap_id = ngap_ue_context->gNB_ue_ngap_id;
  } else {
    resp_p2->ittiMsg.rrc_get_ngap_ue_id.amf_ue_ngap_id = 0;
  }
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p2);
}

void rrc_gnb_generate_rrcrelease(MessageDef *msg_p, instance_t instance)
{
  if (msg_p->ittiMsg.rrc_gnb_generate_rrcrelease.ue_id == -1) {
    MessageDef *resp_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GET_SINGLE_UE_RNTI);
    rrc_get_single_ue_rnti_helper(&resp_p, instance);
    msg_p->ittiMsg.rrc_gnb_generate_rrcrelease.ue_id = resp_p->ittiMsg.rrc_get_single_ue_rnti.id;
    free(resp_p);
  }
  rrc_gNB_ue_context_t *ue = rrc_gNB_get_ue_context(RC.nrrrc[instance], msg_p->ittiMsg.rrc_gnb_generate_rrcrelease.ue_id);
  if (ue != NULL) {
    gNB_RRC_UE_t *UE = &ue->ue_context;
    rrc_gNB_generate_RRCRelease(RC.nrrrc[instance], UE);
  } else {
    LOG_E(RRC, "UE context not found for ue_id %lu\n", msg_p->ittiMsg.rrc_gnb_generate_rrcrelease.ue_id);
  }
}

void rrc_gnb_generate_rrcrelease_all(MessageDef *msg_p, instance_t instance)
{
  MessageDef * resp_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GNB_GENERATE_RRCRELEASE_ALL);
  rrc_gNB_ue_context_t *ue_context_p = NULL;
  int i=0;
  RB_FOREACH (ue_context_p, rrc_nr_ue_tree_s, &RC.nrrrc[instance]->rrc_ue_head) {
    gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
    rrc_gNB_generate_RRCRelease(RC.nrrrc[instance], UE);
    resp_p->ittiMsg.rrc_gnb_generate_rrcrelease_all.nb_releases++;
    resp_p->ittiMsg.rrc_gnb_generate_rrcrelease_all.rrc_gnb_generate_rrcreleases[i].ue_id = ue_context_p->ue_context.rrc_ue_id;
  }
  itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
}

void rrc_gnb_trigger_ue_context_release_req(MessageDef *msg_p, instance_t instance)
{
  MessageDef *resp_p = itti_alloc_new_message(TASK_RRC_GNB, 0, RRC_GNB_TRIGGER_UE_CONTEXT_RELEASE_REQ);
  gNB_RRC_INST *rrc = RC.nrrrc[0];
  resp_p->ittiMsg.rrc_gnb_trigger_ue_context_release_req.ue_id = msg_p->ittiMsg.rrc_gnb_trigger_ue_context_release_req.ue_id;
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, msg_p->ittiMsg.rrc_gnb_trigger_ue_context_release_req.ue_id);
  if (!ue_context_p) {
    itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
  } else if (!ngap_get_ue_context(msg_p->ittiMsg.rrc_gnb_trigger_ue_context_release_req.ue_id)) {
    resp_p->ittiMsg.rrc_gnb_trigger_ue_context_release_req.rrc_ue_context = true;
    itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
  } else {
    resp_p->ittiMsg.rrc_gnb_trigger_ue_context_release_req.rrc_ue_context = true;
    resp_p->ittiMsg.rrc_gnb_trigger_ue_context_release_req.ngap_ue_context = true;
    ngap_cause_t cause = {
        .type = NGAP_CAUSE_RADIO_NETWORK,
        .value = NGAP_CAUSE_RADIO_NETWORK_USER_INACTIVITY,
    };
    rrc_gNB_send_NGAP_UE_CONTEXT_RELEASE_REQ(0, ue_context_p, cause);
    itti_send_msg_to_task(TASK_TELNET, 0, resp_p);
  }
}
