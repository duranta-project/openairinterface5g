/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// initNamedTpoolInit(): every worker runs thread_init() in its own context before the call returns; core_mem_claim()
// claims nothing for a thread that is not pinned to one core.

#include <stdatomic.h>
#include <stdio.h>
#include "thread-pool.h"
#include "core_mem.h"
#include "log.h"

static atomic_int inits;
static atomic_int inits_in_worker;

static void count_init(void)
{
  inits++;
  if (get_tpool_worker_index() >= 0)
    inits_in_worker++;
  core_mem_claim();
}

int main(void)
{
  logInit();
  tpool_t pool;
  char params[] = "-1,-1,-1";
  initNamedTpoolInit(params, &pool, false, "test-init", count_init);
  AssertFatal(inits == 3 && inits_in_worker == 3, "thread_init ran %d times, %d in workers, expected 3\n", inits, inits_in_worker);
  abortTpool(&pool);

  size_t size = 1;
  AssertFatal(core_mem_claim() == 0 && core_mem_get(&size) == NULL && size == 0, "unpinned thread got core-local memory\n");

  tpool_t plain;
  char params2[] = "-1";
  initNamedTpool(params2, &plain, false, "test-plain");
  AssertFatal(plain.thread_init == NULL, "initNamedTpool() set a thread_init\n");
  abortTpool(&plain);
  printf("thread_init: OK\n");
  return 0;
}
