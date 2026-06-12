/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_pdcp_nrup_cu.h"

#include <openair3/ocp-gtpu/gtp_itf.h>

#include "assertions.h"
#include "common/ngran_types.h"
#include "common/utils/LOG/log.h"
#include "nr_pdcp_oai_api.h"
#include "openair2/F1AP/f1ap_common.h"

static void deliver_pdu_drb_gnb_gtp(void *data, ue_id_t ue_id, int rb_id, char *buf, int size, int sdu_id)
{
  UNUSED(data);
  UNUSED(sdu_id);
  LOG_D(PDCP, "%s() (drb %d) sending message to gtp size %d\n", __func__, rb_id, size);
  const f1ap_cudu_inst_t *inst = getCxt(0);
  DevAssert(inst);
  gtpv1uSendDirectWithNRUSeqNum(inst->gtpInst, ue_id, rb_id, (uint8_t *)buf, size);
}

void nr_pdcp_nrup_cu_init(ngran_node_t node_type)
{
  DevAssert(NODE_IS_CU(node_type));
  nr_pdcp_bind_drb_deliver(deliver_pdu_drb_gnb_gtp);
}
