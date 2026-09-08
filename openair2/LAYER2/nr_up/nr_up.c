/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up/nr_up.h"

static nr_up_if_t nr_up_if;

struct nr_up_if_s *get_nr_up_if(void)
{
  return &nr_up_if;
}
