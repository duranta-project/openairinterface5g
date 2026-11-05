/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _SYSTEM_H_OAI_
#define _SYSTEM_H_OAI_
#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif


/****************************************************
 * send a command to the background process
 *     return -1 on error, 0 on success
 ****************************************************/

int background_system(char *command);

/****************************************************
 * initialize the background process
 *     to be called very early
 ****************************************************/

void start_background_system(void);

void lock_memory_to_ram(void);
void set_cpu_dma_latency(int latency_us);

bool has_cap_sys_nice(void);
void threadCreate(pthread_t *t, void *(*func)(void *), void *param, char *name, int affinity, int priority);

#define SCHED_OAI SCHED_RR
#define OAI_PRIORITY_RT_LOW sched_get_priority_min(SCHED_OAI)
#define OAI_PRIORITY_RT ((sched_get_priority_min(SCHED_OAI)+sched_get_priority_max(SCHED_OAI))/2)
#define OAI_PRIORITY_RT_MAX sched_get_priority_max(SCHED_OAI)-2

void thread_top_init(char *thread_name);

/****************************************************
 * Functions to check system at runtime.
 ****************************************************/

int rt_sleep_ns (uint64_t x);
#ifdef __cplusplus
}
#endif


#endif /* _SYSTEM_H_OAI_ */
