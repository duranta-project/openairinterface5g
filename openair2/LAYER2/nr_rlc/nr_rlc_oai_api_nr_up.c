/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_rlc_oai_api_nr_up.h"

static bool nr_rlc_bearer_do_drop = true;

void nr_rlc_set_do_drop(bool do_drop)
{
  nr_rlc_bearer_do_drop = do_drop;
}

bool nr_rlc_get_do_drop(void)
{
  return nr_rlc_bearer_do_drop;
}
