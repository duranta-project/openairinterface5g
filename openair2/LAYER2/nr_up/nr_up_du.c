/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up/nr_up_du.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api_nr_up.h"

void nr_up_init_du(void)
{
  /* CU already decided drop-under-load before PDCP SN, do not reject again here. */
  nr_rlc_set_do_drop(false);
}
