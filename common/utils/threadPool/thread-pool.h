/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <stdbool.h>
#include <stdint.h>
#include <malloc.h>
#include <stdalign.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include "assertions.h"
#include "common/utils/time_meas.h"
#include "common/utils/system.h"
#include "task.h"
#include "pthread_utils.h"

#ifdef DEBUG
  #define THREADINIT   PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
#else
  #define THREADINIT   PTHREAD_MUTEX_INITIALIZER
#endif

typedef struct {
  pthread_t* t_arr;
  size_t len_thr;

  _Atomic(uint64_t) index;

  void* q_arr;

  pthread_barrier_t barrier;
  _Atomic(uint64_t) dead_mask;
} tpool_t;

/// @brief Push job to threadpool. May run task inline in case there are no worker threads
///        defined for threadpool
/// @param tpool threadpool to use
/// @param task task description
void pushTpool(tpool_t *tpool, task_t task);

/// @brief Dispatch one task of a batch that the caller will immediately join on, running the
///        last task on the calling thread instead of handing it to a worker.
///
/// The caller of a push-loop-then-join_task_ans() sequence blocks in the join doing nothing
/// while a worker is woken to perform work the caller could have done in the same time.
/// Executing the final task inline removes one wakeup and one context switch per batch, and
/// makes the calling thread an executor: a batch of N tasks then needs only N-1 workers to
/// run N-ways parallel. That matters most where the per-task work is small or cores are
/// scarce -- a DU in a container with few cores, or low-core targets generally.
///
/// The caller must still init_task_ans(&ans, n) for the full batch and join_task_ans(&ans)
/// afterwards; the inline task signals completion through completed_task_ans() exactly as a
/// worker-run task does. With a NULL tpool every task runs inline, as before.
///
/// Caveat: the calling thread absorbs one whole task, so where task durations are uneven the
/// caller may draw a long one and finish after the workers. Prefer it where the tasks of a
/// batch are of comparable size.
///
/// @param tpool threadpool to use
/// @param task task description
/// @param is_last true if this is the final task of the batch
static inline void pushTpool_batch(tpool_t *tpool, task_t task, bool is_last)
{
  if (tpool == NULL || is_last)
    task.func(task.args);
  else
    pushTpool(tpool, task);
}

/// @brief Abort tpool, stop all threads and return
/// @param t 
void abortTpool(tpool_t *t);

/// @brief Initialize a threadPool.
/// @param params A string in a form of "<int>,<int>,<int>,...,<int>" or "n"
///               if params is "n" threadpool is disabled and all functions are called inline.
///               else if params is a list of comma separated integers, the number of threads
///               is equal to the list length and each thread is pinned to a core indicated by
///               the integer value. If -1 is specified thread is not pinned to any core.
/// @param pool Theadpool
/// @param performanceMeas unused
/// @param name Name of the theadpool, will be used as pthread names for the pool worker threads
void initNamedTpool(char *params,tpool_t *pool, bool performanceMeas, char *name);

/// @brief Initialize a threadpool with floating core worker threads only
/// @param nbThreads number of floating core worker threads
/// @param pool Threadpool
/// @param performanceMeas unused 
/// @param name Name of the theadpool, will be used as pthread names for the pool worker threads
void initFloatingCoresTpool(int nbThreads,tpool_t *pool, bool performanceMeas, char *name);

/// Convenience macro
#define  initTpool(PARAMPTR,TPOOLPTR, MEASURFLAG) initNamedTpool(PARAMPTR,TPOOLPTR, MEASURFLAG, NULL)

/// Returns the index of the worker thread in the thread pool
/// @return index of the worker thread in a thread pool or -1 if not called from a thread pool worker thread
int get_tpool_worker_index(void);
#endif
