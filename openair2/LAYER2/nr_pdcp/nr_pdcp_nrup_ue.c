/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_pdcp_nrup_ue.h"

#include <string.h>

#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "common/utils/utils.h"
#include "nr_pdcp_oai_api.h"
#include "nr_up/nr_up_rlc_queue.h"

static void deliver_pdu_drb_ue(void *data, ue_id_t ue_id, int rb_id, char *buf, int size, int sdu_id)
{
  UNUSED(data);
  protocol_ctxt_t ctxt = {.enb_flag = 0, .rntiMaybeUEid = ue_id};
  uint8_t *memblock = malloc16(size);
  memcpy(memblock, buf, size);
  LOG_D(PDCP, "%s(): drb %d enqueue size %d ue %ld\n", __func__, rb_id, size, ue_id);
  nr_up_enqueue_rlc_data_req(&ctxt, 0, rb_id, sdu_id, size, memblock, NR_UP_CU_UE_ID_NONE);
}

void nr_pdcp_nrup_ue_init(void)
{
  nr_up_rlc_queue_init();
  nr_pdcp_bind_drb_deliver(deliver_pdu_drb_ue);
}

void deliver_pdu_srb_rlc(void *deliver_pdu_data, ue_id_t ue_id, int srb_id, char *buf, int size, int sdu_id)
{
  UNUSED(deliver_pdu_data);
  protocol_ctxt_t ctxt = {.enb_flag = 1, .rntiMaybeUEid = ue_id};
  uint8_t *memblock = malloc16(size);
  memcpy(memblock, buf, size);
  nr_up_enqueue_rlc_data_req(&ctxt, 1, srb_id, sdu_id, size, memblock, NR_UP_CU_UE_ID_NONE);
}
