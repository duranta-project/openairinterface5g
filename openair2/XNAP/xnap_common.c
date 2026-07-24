/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdlib.h>
#include <string.h>
#include "xnap_common.h"
#include "common/openairinterface5g_limits.h"
#include "common/utils/LOG/log.h"
#include "assertions.h"

/* Dual-key compare: ordered by cnx_id when assoc_id == -1 (pre-SCTP),
 * ordered by assoc_id once the SCTP association is up. */
int xnap_peer_compare(struct xnap_peer_s *p1, struct xnap_peer_s *p2)
{
  if (p1->assoc_id == -1) {
    if (p1->cnx_id < p2->cnx_id) return -1;
    if (p1->cnx_id > p2->cnx_id) return  1;
  } else {
    if (p1->assoc_id < p2->assoc_id) return -1;
    if (p1->assoc_id > p2->assoc_id) return  1;
  }
  return 0;
}

RB_GENERATE(xnap_peer_map, xnap_peer_s, entry, xnap_peer_compare);

static xnap_gnb_inst_t *xnap_inst[NUMBER_OF_gNB_MAX] = {0};

xnap_gnb_inst_t *getCxtXn(instance_t instance)
{
  AssertFatal(instance < sizeofArray(xnap_inst), "instance %ld exceeds limit\n", instance);
  return xnap_inst[instance];
}

xnap_peer_t *getXnPeerByAssoc(xnap_gnb_inst_t *inst, sctp_assoc_t assoc_id)
{
  xnap_peer_t temp = {.assoc_id = assoc_id};
  return RB_FIND(xnap_peer_map, &inst->peers, &temp);
}

xnap_peer_t *getXnPeerByCnxId(xnap_gnb_inst_t *inst, uint16_t cnx_id)
{
  xnap_peer_t temp = {.assoc_id = -1, .cnx_id = cnx_id};
  return RB_FIND(xnap_peer_map, &inst->peers, &temp);
}

void xnap_peer_set_assoc_id(xnap_gnb_inst_t *inst, xnap_peer_t *peer, sctp_assoc_t assoc_id)
{
  RB_REMOVE(xnap_peer_map, &inst->peers, peer);
  peer->assoc_id = assoc_id;
  RB_INSERT(xnap_peer_map, &inst->peers, peer);
}

static uint16_t global_cnx_id = 0;

uint16_t xnap_fetch_add_cnx_id(void)
{
  return global_cnx_id++;
}

void createXninst(instance_t instance, xnap_setup_req_t *setup_info, xnap_net_config_t *net_config)
{
  AssertFatal(instance < sizeofArray(xnap_inst), "instance %ld exceeds limit\n", instance);
  AssertFatal(xnap_inst[instance] == NULL, "Double call to Xn instance %d\n", (int)instance);

  xnap_gnb_inst_t *inst = calloc_or_fail(1, sizeof(*inst));

  inst->instance   = instance;
  inst->gnb_id     = setup_info->gNB_id;
  inst->setup_info = *setup_info;
  inst->net_config = *net_config;

  RB_INIT(&inst->peers);

  for (int i = 0; i < net_config->nb_of_candidate_gNBs; i++) {
    xnap_peer_t *peer = calloc_or_fail(1, sizeof(*peer));
    peer->cnx_id   = i;
    peer->assoc_id = -1;
    RB_INSERT(xnap_peer_map, &inst->peers, peer);
    inst->nb_peers++;
  }

  xnap_inst[instance] = inst;

  LOG_I(XNAP, "Created Xn instance %d (gNB_id 0x%x) with %u peer(s)\n", (int)instance, inst->gnb_id, inst->nb_peers);
}
