/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!\file core_mem.h
 * \brief Per-thread scratch memory local to the thread's core (tightly coupled memory).
 *
 * A thread pinned to a single core that has core-local memory can claim it once, typically when a thread-pool worker
 * starts (initNamedTpoolInit() with core_mem_claim), and then use it as scratch for work that runs on that thread
 * only. A core's memory goes to at most one thread of the process. Supported: SpacemiT K3, A100 cores 8-15, one 384 KB
 * TCM block each (block c - 8 of /dev/tcm); elsewhere, and for unpinned threads, nothing is claimed.
 */

#ifndef CORE_MEM_H
#define CORE_MEM_H

#include <stddef.h>

/// Claims the core-local memory of the core the calling thread is pinned to. Returns its size, 0 if none: no such
/// memory on that core, thread not pinned to exactly one core, or already claimed by another thread.
size_t core_mem_claim(void);

/// core_mem_claim() as a thread-pool worker init (initNamedTpoolInit())
static inline void core_mem_thread_init(void)
{
  core_mem_claim();
}

/// The calling thread's core-local memory, NULL if it claimed none; *size is set to its size when non-NULL.
void *core_mem_get(size_t *size);

#endif
