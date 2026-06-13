/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up.h"

#include <stdatomic.h>

static nr_up_if_t nr_up_if;

/** @brief Return the nr-up interface function-pointer table */
struct nr_up_if_s *get_nr_up_if(void)
{
  return &nr_up_if;
}

static _Atomic uint64_t nr_up_time_ms;

/** Time-manager callback, once per real or simulated millisecond */
void nr_up_ms_tick(void)
{
  atomic_fetch_add_explicit(&nr_up_time_ms, 1, memory_order_relaxed);
}

/** Elapsed managed milliseconds */
uint64_t nr_up_current_time_ms(void)
{
  return atomic_load_explicit(&nr_up_time_ms, memory_order_relaxed);
}
