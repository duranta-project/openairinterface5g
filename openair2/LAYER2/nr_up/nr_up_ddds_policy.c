/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up_ddds_policy.h"

#include "assertions.h"

/* TS 38.425 clause 5.4.2.1 autonomous refresh while DBS is changing */
#define NR_UP_DDDS_TIMER_MS 20u

/** @brief Whether to send DDDS because RLC TX space hit or left zero.
 * CU treats Desired Buffer Size 0 as "stop DL" and a later non-zero as
 * "may send again" (TS 38.425 clause 5.4.2.1). */
static bool nr_up_ddds_zero_trigger(const nr_up_ddds_t *st, uint32_t space)
{
  if (!st->has_last_report)
    return space == 0;
  return (space == 0) != (st->last_desired_buffer_size == 0);
}

/** @brief Refresh if DBS changed and the report timer elapsed (skip unchanged) */
static bool nr_up_ddds_timer_trigger(const nr_up_ddds_t *st, uint32_t space, uint64_t now)
{
  if (!st->has_last_report || space == st->last_desired_buffer_size)
    return false;
  return (now - st->last_report_time) >= NR_UP_DDDS_TIMER_MS;
}

/** @brief Decide whether DDDS must go out (TS 38.425 clause 5.4.2.1) */
bool nr_up_ddds_precheck(nr_up_ddds_t *st, uint32_t space, bool is_poll)
{
  DevAssert(st != NULL);
  const uint64_t now = nr_up_current_time_ms();
  const bool send = is_poll || nr_up_ddds_zero_trigger(st, space) || nr_up_ddds_timer_trigger(st, space, now);
  if (!send)
    return false;
  st->has_last_report = true;
  st->last_desired_buffer_size = space;
  st->last_report_time = now;
  return true;
}
