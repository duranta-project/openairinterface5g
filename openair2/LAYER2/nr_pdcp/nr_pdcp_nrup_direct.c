/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_pdcp_nrup_direct.h"

#include "assertions.h"
#include "common/ngran_types.h"
#include "nr_pdcp_nrup_handler.h"
#include "nr_pdcp_oai_api.h"
#include "nr_up/nr_up_monolithic.h"
#include "nr_up/nr_up_pdcp_if.h"

static void deliver_pdu_drb_gnb_nr_up(void *data, ue_id_t ue_id, int rb_id, char *buf, int size, int sdu_id)
{
  UNUSED(data);
  const byte_array_t pdu = {
      .buf = (uint8_t *)buf,
      .len = size,
  };
  nr_pdcp_dl_transfer_gnb(ue_id, rb_id, &pdu, sdu_id);
}

void nr_pdcp_nrup_direct_init(ngran_node_t node_type)
{
  DevAssert(NODE_IS_MONOLITHIC(node_type));
  nr_pdcp_bind_drb_deliver(deliver_pdu_drb_gnb_nr_up);
  nr_up_init_monolithic(get_nr_up_if());
}
