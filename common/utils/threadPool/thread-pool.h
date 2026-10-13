/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

/* C11 _Atomic(T) is not a keyword in C++; map it to std::atomic<T>. Guarded so
 * this stays idempotent if another header already defines it -- with -Werror,
 * an unconditional #define here would fail the build on macro redefinition. */
#ifdef __cplusplus
#include <atomic>
#ifndef _Atomic
#define _Atomic(T) std::atomic<T>
#endif
#endif

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
