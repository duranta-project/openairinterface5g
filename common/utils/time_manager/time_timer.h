/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef COMMON_UTIL_TIME_MANAGER_TIME_TIMER
#define COMMON_UTIL_TIME_MANAGER_TIME_TIMER

#include <stdint.h>

void time_timer_init(void);
void time_timer_free(void);

void time_timer_ms_tick(void);

/* returns a timer pointer to be passed to tick_timeout_stop()
 * p, psize, v, vsize are parameters for the callback (we have
 * as many parameters as the user needs)
 */
void *tick_timeout_start(int ms, void (*callback)(void **p, uint64_t *v), void **p, int psize, uint64_t *v, int vsize);
void tick_timeout_stop(void *timer);

#endif /* COMMON_UTIL_TIME_MANAGER_TIME_TIMER */
