/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#define _GNU_SOURCE
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include "thread-pool.h"
#include "bounded_notified_fifo.h"
#include <sys/sysinfo.h>

typedef struct {
  tpool_t* tpool;
  int idx;
} task_thread_args_t;

void pushTpool(tpool_t* tpool, task_t task)
{
  DevAssert(tpool != NULL);
  if (tpool->len_thr == 0) {
    task.func(task.args);
    return;
  }

  size_t const index = tpool->index++;
  size_t const len_thr = tpool->len_thr;

  not_q_t* q_arr = (not_q_t*)tpool->q_arr;

  for (size_t i = 0; i < len_thr; ++i) {
    if (try_push_not_q(&q_arr[(i + index) % len_thr], task)) {
      return;
    }
  }

  push_not_q(&q_arr[index % len_thr], task);
}

// Same as above, but avoid certain tpool workers based on the mask
void pushTpool_mask(tpool_t* tpool, task_t task, uint64_t mask)
{
  DevAssert(tpool != NULL);
  if (tpool->len_thr == 0) {
    task.func(task.args);
    return;
  }

  size_t index = tpool->index++;
  size_t const len_thr = tpool->len_thr;

  not_q_t* q_arr = (not_q_t*)tpool->q_arr;

  for (size_t i = 0; i < len_thr; ++i) {
    int index = (i + tpool->index) % len_thr;
    if ((1UL << index) & mask)
      continue;
    if (try_push_not_q(&q_arr[index], task)) {
      return;
    }
  }

  while ((1ULL << (index % len_thr)) & mask)
    index++;
  push_not_q(&q_arr[index % len_thr], task);
}

__thread int tpool_worker_index = -1;
int get_tpool_worker_index(void)
{
  return tpool_worker_index;
}

static void* worker_thread(void* arg)
{
  DevAssert(arg != NULL);

  task_thread_args_t* args = (task_thread_args_t*)arg;
  int const idx = args->idx;
  tpool_worker_index = idx;
  tpool_t* tpool = args->tpool;

  uint32_t const len = tpool->len_thr;
  uint32_t const num_it = 2 * (tpool->len_thr + idx);

  not_q_t* q_arr = (not_q_t*)tpool->q_arr;

  init_not_q(&q_arr[idx], idx);
  if (tpool->thread_init)
    tpool->thread_init();
  // Synchronize all threads
  pthread_barrier_wait(&tpool->barrier);

  for (;;) {
    ret_try_t ret = {.success = false};

    for (uint32_t i = idx; i < num_it; ++i) {
      ret = try_pop_not_q(&q_arr[i % len]);
      if (ret.success == true)
        break;
    }

    if (ret.success == false) {
      if (pop_not_q(&q_arr[idx], &ret) == false)
        break;
    }

    if (ret.t.func == NULL && ret.t.args == NULL) {
      tpool->dead_mask |= 1ULL << idx;
      if (__builtin_popcountll(tpool->dead_mask) == tpool->len_thr)
        break;
      pushTpool_mask(tpool, (task_t){.args = NULL, .func = NULL}, tpool->dead_mask);
      break;
    }
    ret.t.func(ret.t.args);
  }

  free(args);
  return NULL;
}

void initNamedTpool(char* params, tpool_t* tpool, bool performanceMeas, char* name)
{
  initNamedTpoolInit(params, tpool, performanceMeas, name, NULL);
}

void initNamedTpoolInit(char* params, tpool_t* tpool, bool performanceMeas, char* name, void (*thread_init)(void))
{
  (void)performanceMeas;

  DevAssert(tpool != NULL);
  memset(tpool, 0, sizeof(*tpool));
  tpool->thread_init = thread_init;

  char* tname = (name == NULL ? "Tpool" : name);
  char *saveptr, *curptr;
  char* parms_cpy = strdup(params);
  curptr = strtok_r(parms_cpy, ",", &saveptr);
  int core_id[128] = {0};
  int num_workers = 0;
  while (curptr != NULL) {
    int c = toupper(curptr[0]);

    switch (c) {
      case 'N':
        break;

      default:
        core_id[num_workers++] = atoi(curptr);
    }

    curptr = strtok_r(NULL, ",", &saveptr);
  }
  free(parms_cpy);

  if (num_workers) {
    tpool->q_arr = calloc(num_workers, sizeof(not_q_t));
    AssertFatal(tpool->q_arr != NULL, "Memory exhausted");

    tpool->t_arr = calloc(num_workers, sizeof(pthread_t));
    AssertFatal(tpool->t_arr != NULL, "Memory exhausted");
  }
  tpool->len_thr = num_workers;

  tpool->index = 0;

  const pthread_barrierattr_t* barrier_attr = NULL;
  int rc = pthread_barrier_init(&tpool->barrier, barrier_attr, num_workers + 1);
  DevAssert(rc == 0);

  for (size_t i = 0; i < num_workers; ++i) {
    task_thread_args_t* args = malloc(sizeof(task_thread_args_t));
    AssertFatal(args != NULL, "Memory exhausted");
    args->idx = i;
    args->tpool = tpool;
    char name[64];
    sprintf(name, "%s%ld_%d", tname, i, core_id[i]);
    threadCreate(&tpool->t_arr[i], worker_thread, args, name, core_id[i], OAI_PRIORITY_RT_MAX);
  }

  // Syncronize thread pool threads. All the threads started
  pthread_barrier_wait(&tpool->barrier);
}

void initFloatingCoresTpool(int nbThreads, tpool_t* pool, bool performanceMeas, char* name)
{
  char threads[1024] = "n";
  if (nbThreads) {
    strcpy(threads, "-1");
    for (int i = 1; i < nbThreads; i++)
      strncat(threads, ",-1", sizeof(threads) - 1);
  }
  threads[sizeof(threads) - 1] = 0;
  initNamedTpool(threads, pool, performanceMeas, name);
}


void abortTpool(tpool_t* tpool)
{
  if (tpool->len_thr > 0) {
    not_q_t* q_arr = (not_q_t*)tpool->q_arr;

    pushTpool(tpool, (task_t){.args = NULL, .func = NULL});

    for (uint32_t i = 0; i < tpool->len_thr; ++i) {
      int rc = pthread_join(tpool->t_arr[i], NULL);
      DevAssert(rc == 0);
    }

    for (uint32_t i = 0; i < tpool->len_thr; ++i) {
      free_not_q(&q_arr[i]);
    }

    free(tpool->q_arr);
    free(tpool->t_arr);
  }

  int rc = pthread_barrier_destroy(&tpool->barrier);
  DevAssert(rc == 0);
}
