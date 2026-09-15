/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_pdcp_nrup_f1ap.h"

#include "assertions.h"
#include "common/ngran_types.h"
#include "nr_pdcp_nrup_handler.h"
#include "nr_pdcp_oai_api.h"
#include "nr_up/nr_up_f1u.h"
#include "nr_up/nr_up_pdcp_if.h"

void nr_pdcp_nrup_f1ap_init(ngran_node_t node_type)
{
  DevAssert(NODE_IS_CU(node_type));
  nr_up_init_f1u(get_nr_up_if());
  nr_pdcp_bind_drb_deliver(nr_pdcp_deliver_pdu_drb);
}
