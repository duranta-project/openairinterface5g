/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up.h"

static nr_up_if_t nr_up_if;

/** @brief Return the nr-up interface function-pointer table */
struct nr_up_if_s *get_nr_up_if(void)
{
  return &nr_up_if;
}
