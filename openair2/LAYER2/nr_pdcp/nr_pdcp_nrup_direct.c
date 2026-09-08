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

void nr_pdcp_nrup_direct_init(ngran_node_t node_type)
{
  DevAssert(NODE_IS_MONOLITHIC(node_type));
  nr_up_init_monolithic(get_nr_up_if());
  nr_pdcp_bind_drb_deliver(nr_pdcp_deliver_pdu_drb);
}
