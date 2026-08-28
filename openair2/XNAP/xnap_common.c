/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdlib.h>
#include <string.h>
#include "xnap_common.h"
#include "common/openairinterface5g_limits.h"
#include "common/utils/LOG/log.h"
#include "assertions.h"

/* Single key: assoc_id. A peer only ever exists in the tree while its SCTP
 * association is up, so assoc_id is always valid here. */
int xnap_peer_compare(struct xnap_peer_s *p1, struct xnap_peer_s *p2)
{
  if (p1->assoc_id < p2->assoc_id) return -1;
  if (p1->assoc_id > p2->assoc_id) return  1;
  return 0;
}

RB_GENERATE(xnap_peer_map, xnap_peer_s, entry, xnap_peer_compare);

static xnap_gnb_inst_t *xnap_inst[NUMBER_OF_gNB_MAX] = {0};

xnap_gnb_inst_t *xnap_get_inst(instance_t instance)
{
  AssertFatal(instance < sizeofArray(xnap_inst), "instance %ld exceeds limit\n", instance);
  return xnap_inst[instance];
}

void xnap_create_inst(instance_t instance, xnap_setup_req_t *setup_info, xnap_net_config_t *net_config)
{
  AssertFatal(instance < sizeofArray(xnap_inst), "instance %ld exceeds limit\n", instance);
  AssertFatal(xnap_inst[instance] == NULL, "Double call to Xn instance %d\n", (int)instance);

  xnap_gnb_inst_t *inst = calloc_or_fail(1, sizeof(*inst));

  inst->instance   = instance;
  inst->gnb_id     = setup_info->gNB_id;
  inst->setup_info = *setup_info;
  inst->net_config = *net_config;

  RB_INIT(&inst->peers);

  xnap_inst[instance] = inst;

  LOG_I(XNAP, "Created Xn instance %d (gNB_id 0x%x) with %u candidate(s)\n",
        (int)instance, inst->gnb_id, inst->net_config.nb_of_candidate_gNBs);
}
