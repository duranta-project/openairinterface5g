/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <netinet/in.h>
#include <netinet/sctp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "common/utils/alg/find.h"
#include "common/utils/alg/foreach.h"
#include "RRC/NR/nr_rrc_proto.h"
#include "T.h"
#include "as_message.h"
#include "assertions.h"
#include "common/ran_context.h"
#include "common/utils/T/T.h"
#include "rrc_gNB_UE_context.h"
#include "rrc_gNB_du.h"
#include "e1ap_messages_types.h"
#include "intertask_interface.h"
#include "nr_rrc_defs.h"
#include "openair2/F1AP/f1ap_ids.h"
#include "rrc_messages_types.h"
#include "tree.h"
#include "e1ap_interface_management.h"
#include "rrc_gNB_NGAP.h"
#include "rrc_cell_management.h"

static int cuup_compare(const nr_rrc_cuup_container_t *a, const nr_rrc_cuup_container_t *b)
{
  if (a->assoc_id > b->assoc_id)
    return 1;
  if (a->assoc_id == b->assoc_id)
    return 0;
  return -1; /* a->assoc_id < b->assoc_id */
}

/* Tree management functions */
RB_GENERATE/*_STATIC*/(rrc_cuup_tree, nr_rrc_cuup_container_t, entries, cuup_compare);

static const nr_rrc_cuup_container_t *select_cuup_slice(const struct rrc_cuup_tree *t, const gNB_RRC_UE_t *ue, int sst, int sd)
{
  nr_rrc_cuup_container_t *second_best_match = NULL; /* if no NSSAI matches exactly */
  nr_rrc_cuup_container_t *cuup = NULL;
  RB_FOREACH(cuup, rrc_cuup_tree, (struct rrc_cuup_tree *)&t) {
    e1ap_setup_req_t *sr = cuup->setup_req;
    for (int p = 0; p < sr->supported_plmns; ++p) {
      /* actually we should also check that the PLMN selected by the UE matches */
      for (int s = 0; s < sr->plmn[p].supported_slices; ++s) {
        e1ap_nssai_t *nssai = &sr->plmn[p].slice[s];
        if (nssai->sst == sst && nssai->sd == sd) {
          LOG_UE_EVENT(ue, "selecting CU-UP ID %ld based on exact NSSAI match (%d:0x%06x)\n", sr->gNB_cu_up_id, sst, sd);
          return cuup; /* exact match */
        } else if (nssai->sst == sst && second_best_match == NULL) {
          LOG_UE_EVENT(ue, "second best match: CU-UP ID %ld matches SST %d\n", sr->gNB_cu_up_id, sst);
          second_best_match = cuup; /* only the SST matches -> "good enough" */
        }
      }
    }
  }
  return second_best_match;
}

static const nr_rrc_cuup_container_t *select_cuup_round_robin(size_t n_t, const struct rrc_cuup_tree *t, const gNB_RRC_UE_t *ue)
{
  /* pick the CU-UP following a "round-robin" fashion: select CU-UP M = RRC UE
   * ID % N with N number of CU-UPs */
  int m = (ue->rrc_ue_id - 1) % n_t;
  nr_rrc_cuup_container_t *cuup = NULL;
  RB_FOREACH(cuup, rrc_cuup_tree, (struct rrc_cuup_tree *)&t) {
    if (m == 0) {
      LOG_W(RRC, "round-robin match: select CU-UP ID %ld (no NSSAI match)\n", cuup->setup_req->gNB_cu_up_id);
      return cuup;
    }
    m--;
  }
  /* this should not happen: no CU-UP available? */
  return NULL;
}

bool is_cuup_associated(gNB_RRC_INST *rrc)
{
  nr_rrc_cuup_container_t *cuup = NULL;
  RB_FOREACH(cuup, rrc_cuup_tree, &rrc->cuups) {
    return true;
  }
  LOG_W(NR_RRC, "no CU-UP associated to CU-CP\n");
  return false;
}

bool ue_associated_to_cuup(const gNB_RRC_UE_t *ue)
{
  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue->rrc_ue_id);
  return ue_data.e1_assoc_id != 0;
}

sctp_assoc_t get_existing_cuup_for_ue(const gNB_RRC_UE_t *ue)
{
  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue->rrc_ue_id);
  AssertFatal(ue_data.e1_assoc_id != 0, "UE %d should be associated to CU-UP, but is not\n", ue->rrc_ue_id);
  LOG_D(RRC, "UE %d using CU-UP assoc_id %d\n", ue->rrc_ue_id, ue_data.e1_assoc_id);
  return ue_data.e1_assoc_id;
}

/* Any connected CU-UP.
 *
 * The per-UE selectors above cannot serve a broadcast session, which has no
 * UE to select for. One CU-UP is the assumption everywhere in this code
 * path; choosing between several is a question for when a session can be
 * confined to a service area. */
sctp_assoc_t get_any_cuup(void)
{
  const gNB_RRC_INST *rrc = RC.nrrrc[0];
  if (rrc == NULL || RB_EMPTY(&rrc->cuups))
    return 0;
  const nr_rrc_cuup_container_t *c = RB_ROOT(&rrc->cuups);
  return c != NULL ? c->assoc_id : 0;
}

sctp_assoc_t get_new_cuup_for_ue(const gNB_RRC_INST *rrc, const gNB_RRC_UE_t *ue, int sst, int sd)
{
  /* check if there is already a UE associated */
  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue->rrc_ue_id);
  if (ue_data.e1_assoc_id != 0) {
    LOG_I(RRC, "UE %d using CU-UP assoc_id %d\n", ue->rrc_ue_id, ue_data.e1_assoc_id);
    return ue_data.e1_assoc_id;
  }

  /* it is zero -> no CUUP for this UE yet, get the (only) CU-UP that is
   * connected */
  if (RB_EMPTY(&rrc->cuups)) {
    LOG_W(RRC, "no CU-UPs connected: UE %d cannot have a DRB\n", ue->rrc_ue_id);
    return 0; /* no CUUP connected */
  }

  const nr_rrc_cuup_container_t *selected = NULL;
  if (sst != 0) /* only do if there is slice information */
    selected = select_cuup_slice(&rrc->cuups, ue, sst, sd);
  if (selected == NULL) /* nothing found yet */
    selected = select_cuup_round_robin(rrc->num_cuups, &rrc->cuups, ue);
  if (selected == NULL) {
    selected = RB_ROOT(&rrc->cuups);
  }
  AssertFatal(selected != NULL, "logic error: could not select CU-UP, is one connected?\n");

  /* update the association for the UE so it will be picked up later */
  ue_data.e1_assoc_id = selected->assoc_id;
  bool success = cu_update_f1_ue_data(ue->rrc_ue_id, &ue_data);
  DevAssert(success);
  LOG_I(RRC, "UE %d associating to CU-UP assoc_id %d out of %ld CU-UPs\n", ue->rrc_ue_id, ue_data.e1_assoc_id, rrc->num_cuups);

  return ue_data.e1_assoc_id;
}

/* CU-CP Functions */

/**
 * @brief Trigger E1AP Setup Failure on CU-CP
*/
static void e1ap_setup_failure(sctp_assoc_t assoc_id, uint64_t transac_id, e1ap_cause_t cause)
{
  MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, 0, E1AP_SETUP_FAIL);
  msg_p->ittiMsgHeader.originInstance = assoc_id;
  e1ap_setup_fail_t *setup_fail = &E1AP_SETUP_FAIL(msg_p);
  setup_fail->transac_id = transac_id;
  setup_fail->cause = cause;
  LOG_I(NR_RRC, "Triggering E1AP Setup Failure for transac_id %ld, assoc_id %ld\n",
        transac_id,
        msg_p->ittiMsgHeader.originInstance);
  itti_send_msg_to_task(TASK_CUCP_E1, 0 /*unused by callee*/, msg_p);
}

static bool has_assoc_id(const void *vval, const void *vit)
{
  sctp_assoc_t val = *(sctp_assoc_t *)vval;
  sctp_assoc_t it = *(sctp_assoc_t *)vit;
  return val == it;
}

/* @brief Remove RRC UE contexts of UE without E1 connection.
 *
 * 38.463 sec 8.2.3. says that "[E1 Setup] procedure also re-initialises the
 * E1AP UE-related contexts (if any) and erases all related signalling
 * connections in the two nodes like a Reset procedure would do.". If the CU-UP
 * reconnects, it's previous UE contexts are here, but not associated. To
 * fulfil the above, we simply free all UE contexts that don't have a CU-UP,
 * and send a F1 Reset message to the corresponding DU(s), following the fact
 * that (38.473 sec 8.2.1.2.1) "a failure at the gNB-CU [...] has resulted in
 * the loss of some or all transaction reference information[...] a RESET
 * message shall be sent to the gNB-DU.". This might be overarching in that UEs
 * that are in the process of connecting or that are on another DU might be
 * affected by that reset procedure as well; we suppose they will connect later
 * again. */
static void remove_unassociated_e1_connections(gNB_RRC_INST *rrc)
{
  seq_arr_t affected_du;
  seq_arr_init(&affected_du, sizeof(sctp_assoc_t));
  seq_arr_t ue_context_to_remove;
  seq_arr_init(&ue_context_to_remove, sizeof(rrc_gNB_ue_context_t *));

  rrc_gNB_ue_context_t *ue_context_p = NULL;
  RB_FOREACH(ue_context_p, rrc_nr_ue_tree_s, &rrc->rrc_ue_head) {
    gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
    uint32_t ue_id = UE->rrc_ue_id;
    if (ue_associated_to_cuup(UE))
      continue;
    f1_ue_data_t ue_data = cu_get_f1_ue_data(ue_id);
    elm_arr_t ret = find_if(&affected_du, &ue_data.du_assoc_id, has_assoc_id);
    if (!ret.found)
      seq_arr_push_back(&affected_du, &ue_data.du_assoc_id, sizeof(ue_data.du_assoc_id));

    /* remove UE context: we store the pointer, so pass pointer to ue_context_p */
    seq_arr_push_back(&ue_context_to_remove, &ue_context_p, sizeof(ue_context_p));
    LOG_I(NR_RRC, "remove E1-unassociated UE context ID %d\n", ue_id);
  }

  for (int i = 0; i < seq_arr_size(&ue_context_to_remove); ++i) {
    /* we retrieve a pointer (=iterator) to the UE context pointer
     * (ue_context_p), so dereference once */
    rrc_gNB_ue_context_t *p = *(rrc_gNB_ue_context_t **)seq_arr_at(&ue_context_to_remove, i);
    ngap_cause_t cause = {.type = NGAP_CAUSE_RADIO_NETWORK, .value = NGAP_CAUSE_RADIO_NETWORK_RADIO_CONNECTION_WITH_UE_LOST};
    rrc_gNB_send_NGAP_UE_CONTEXT_RELEASE_REQ(0, p, cause);
    rrc_remove_ue(rrc, p);
  }
  seq_arr_free(&ue_context_to_remove, NULL);

  for (int i = 0; i < seq_arr_size(&affected_du); ++i) {
    void *it = seq_arr_at(&affected_du, i);
    sctp_assoc_t du_assoc_id = *(sctp_assoc_t *)it;
    LOG_W(NR_RRC, "trigger F1 reset on DU %d due to E1 connection loss\n", du_assoc_id);
    trigger_f1_reset(rrc, du_assoc_id);
  }
  seq_arr_free(&affected_du, NULL);
}

/**
 * @brief E1AP Setup Request processing on CU-CP
*/
int rrc_gNB_process_e1_setup_req(sctp_assoc_t assoc_id, const e1ap_setup_req_t *req)
{
  AssertFatal(req->supported_plmns <= PLMN_LIST_MAX_SIZE, "Supported PLMNs is more than PLMN_LIST_MAX_SIZE\n");
  gNB_RRC_INST *rrc = RC.nrrrc[0];

  nr_rrc_cuup_container_t *c = NULL;
  RB_FOREACH(c, rrc_cuup_tree, &rrc->cuups) {
    if (req->gNB_cu_up_id == c->setup_req->gNB_cu_up_id) {
      LOG_E(NR_RRC,
            "Connecting CU-UP ID %ld name %s (assoc_id %d) has same ID as existing CU-UP ID %ld name %s (assoc_id %d)\n",
            req->gNB_cu_up_id,
            req->gNB_cu_up_name,
            assoc_id,
            c->setup_req->gNB_cu_up_id,
            c->setup_req->gNB_cu_up_name,
            c->assoc_id);
      e1ap_cause_t cause = { .type = E1AP_CAUSE_RADIO_NETWORK, .value = E1AP_RADIO_CAUSE_UNKNOWN_ALREADY_ALLOCATED_GNB_CU_UP_UE_E1AP_ID};
      e1ap_setup_failure(assoc_id, req->transac_id, cause);
      return -1;
    }
  }

  for (int i = 0; i < req->supported_plmns; i++) {
    const PLMN_ID_t *id = &req->plmn[i].id;
    if (rrc->configuration.plmn[i].mcc != id->mcc || rrc->configuration.plmn[i].mnc != id->mnc) {
      LOG_E(NR_RRC,
            "PLMNs received from CUUP (mcc:%d, mnc:%d) did not match with PLMNs in RRC (mcc:%d, mnc:%d)\n",
            id->mcc,
            id->mnc,
            rrc->configuration.plmn[i].mcc,
            rrc->configuration.plmn[i].mnc);
      e1ap_cause_t cause = { .type = E1AP_CAUSE_RADIO_NETWORK, .value = E1AP_RADIO_CAUSE_OTHER};
      e1ap_setup_failure(assoc_id, req->transac_id, cause);
      return -1;
    }
  }

  LOG_I(NR_RRC, "Accepting new CU-UP ID %ld name %s (assoc_id %d)\n", req->gNB_cu_up_id, req->gNB_cu_up_name, assoc_id);
  nr_rrc_cuup_container_t *cuup = malloc_or_fail(sizeof(*cuup));
  cuup->setup_req = malloc_or_fail(sizeof(*cuup->setup_req));
  *cuup->setup_req = cp_e1ap_cuup_setup_request(req);
  cuup->assoc_id = assoc_id;
  RB_INSERT(rrc_cuup_tree, &rrc->cuups, cuup);
  rrc->num_cuups++;

  remove_unassociated_e1_connections(rrc);

  MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, 0, E1AP_SETUP_RESP);
  msg_p->ittiMsgHeader.originInstance = assoc_id;
  e1ap_setup_resp_t *resp = &E1AP_SETUP_RESP(msg_p);
  resp->transac_id = req->transac_id;
  itti_send_msg_to_task(TASK_CUCP_E1, 0, msg_p);

  return 0;
}

/* @brief invalidate (remove) all E1 assoc ID from UE contexts belonging to this CU-UP */
static void invalidate_cuup_connections(gNB_RRC_INST *rrc, sctp_assoc_t e1_assoc_id)
{
  rrc_gNB_ue_context_t *ue_context_p = NULL;
  RB_FOREACH(ue_context_p, rrc_nr_ue_tree_s, &rrc->rrc_ue_head) {
    gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
    uint32_t ue_id = UE->rrc_ue_id;
    f1_ue_data_t ue_data = cu_get_f1_ue_data(ue_id);
    if (ue_data.e1_assoc_id != e1_assoc_id)
      continue; /* UE is on another CU-UP */

    ue_data.e1_assoc_id = 0;
    bool success = cu_update_f1_ue_data(ue_id, &ue_data);
    DevAssert(success);
    LOG_W(NR_RRC, "CU-UP %d of UE %d is gone\n", e1_assoc_id, ue_id);
  }
}

/**
 * @brief RRC Processing of the indication of E1 connection loss on CU-CP
*/
void rrc_gNB_process_e1_lost_connection(gNB_RRC_INST *rrc, sctp_assoc_t assoc_id)
{
  LOG_I(NR_RRC, "Received E1 connection loss indication on RRC\n");
  AssertFatal(assoc_id != 0, "illegal assoc_id == 0: should be -1 (monolithic) or >0 (split)\n");
  invalidate_cuup_connections(rrc, assoc_id);
  nr_rrc_cuup_container_t e = {.assoc_id = assoc_id};
  nr_rrc_cuup_container_t *cuup = RB_FIND(rrc_cuup_tree, &rrc->cuups, &e);
  if (cuup == NULL) {
    LOG_W(NR_RRC, "CU-UP for assoc_id %d not found!\n", assoc_id);
    return;
  }
  if (cuup->setup_req != NULL) {
    e1ap_setup_req_t *req = cuup->setup_req;
    LOG_I(NR_RRC, "releasing CU-UP %s on assoc_id %d\n", req->gNB_cu_up_name, assoc_id);
    free_e1ap_cuup_setup_request(cuup->setup_req);
    free(cuup->setup_req);
  }
  nr_rrc_cuup_container_t *removed = RB_REMOVE(rrc_cuup_tree, &rrc->cuups, cuup);
  // Free relevant CU-UP structures
  free(cuup);
  DevAssert(removed != NULL);
  rrc->num_cuups--;
}
