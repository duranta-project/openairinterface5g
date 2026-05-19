/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up/nr_up.h"
#include "nr_up/nr_up_direct.h"
#include "assertions.h"
#include "common/utils/utils.h"

static nr_up_if_t nr_up_if;

/** @brief Return the nr-up interface function-pointer table */
struct nr_up_if_s *get_nr_up_if(void)
{
  return &nr_up_if;
}

/** @brief Stub of nr_up_init_direct when nr_up_direct is not linked
 * Softmodem links the strong version from nr_up_direct, nr-cuup keeps
 * this stub for linking PDCP shared lib */
void __attribute__((weak)) nr_up_init_direct(nr_up_if_t *iface)
{
  UNUSED(iface);
  AssertFatal(0, "nr_up_init_direct() requires nr_up_direct\n");
}
