/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "time_timer.h"

#include <stdlib.h>
#include <stdbool.h>

#include "common/utils/ds/shared_pointer.h"
#include "common/utils/assertions.h"
#include "common/utils/system.h"
#include "common/utils/LOG/log.h"

#define MAX_DELAY_MS 32768    /* 32s max of timeout - change if more needed */

typedef struct {
  void (*callback)(void **p, uint64_t *v);
  void **p;
  uint64_t *v;
  _Atomic bool disabled;
  _Atomic bool callback_running;
  pthread_mutex_t m;
  pthread_cond_t c;
  shared_pointer_t *next;   /* this is a shared_pointer_t(time_timer_item_t) */
} time_timer_item_t;

static void lock_item(time_timer_item_t *t)
{
  DevAssert(pthread_mutex_lock(&t->m) == 0);
}

static void unlock_item(time_timer_item_t *t)
{
  DevAssert(pthread_mutex_unlock(&t->m) == 0);
}

static void do_wait_item(time_timer_item_t *t)
{
  DevAssert(pthread_cond_wait(&t->c, &t->m) == 0);
}

static void do_signal_item(time_timer_item_t *t)
{
  DevAssert(pthread_cond_signal(&t->c) == 0);
}

static void free_item(void *_item)
{
  time_timer_item_t *item = _item;
  free(item->p);
  free(item->v);
  free(item);
}

/* a list of shared_pointer_t(time_timer_item_t) */
typedef struct {
  shared_pointer_t *head;
  shared_pointer_t *tail;
} time_timer_list_t;

static void list_add(time_timer_list_t *l, shared_pointer_t *p)
{
  if (!l->head) {
    l->head = p;
    l->tail = p;
  } else {
    time_timer_item_t *last = l->tail->p;
    last->next = p;
    l->tail = p;
  }
}

/* a timer module */
typedef struct {
  pthread_t thread_id;
  pthread_mutex_t m;
  pthread_cond_t c;
  uint64_t cur_time;
  bool exit;
  time_timer_list_t timers[MAX_DELAY_MS];
} time_timer_t;

static void lock(time_timer_t *t)
{
  DevAssert(pthread_mutex_lock(&t->m) == 0);
}

static void unlock(time_timer_t *t)
{
  DevAssert(pthread_mutex_unlock(&t->m) == 0);
}

static void do_wait(time_timer_t *t)
{
  DevAssert(pthread_cond_wait(&t->c, &t->m) == 0);
}

static void do_signal(time_timer_t *t)
{
  DevAssert(pthread_cond_signal(&t->c) == 0);
}

static void *time_timer_thread(void *_t)
{
  time_timer_t *t = _t;
  int cur_pos = 0;
  uint64_t cur_time = 0;

  while (1) {
    lock(t);
    while (!(t->exit || cur_time != t->cur_time))
      do_wait(t);
    if (t->exit) {
      unlock(t);
      break;
    }
    uint64_t next_time = t->cur_time;
    unlock(t);

    /* process all ticks up to t->cur_time */
    while (cur_time != next_time) {
      lock(t);
      shared_pointer_t *l = t->timers[cur_pos].head;
      t->timers[cur_pos].head = 0;
      t->timers[cur_pos].tail = 0;
      unlock(t);
      while (l) {
        time_timer_item_t *p = l->p;
        shared_pointer_t *next = p->next;
        lock_item(p);
        if (!p->disabled) {
          p->callback_running = true;
          unlock_item(p);
          p->callback(p->p, p->v);
          lock_item(p);
          p->callback_running = false;
          do_signal_item(p);
        }
        unlock_item(p);
        unref_shared_pointer(l);
        l = next;
      }
      cur_pos++;
      cur_pos %= MAX_DELAY_MS;
      cur_time++;
    }
  }

  return 0;
}

/* global timer module API */
static time_timer_t timer_module;

void time_timer_init(void)
{
  pthread_mutex_init(&timer_module.m, 0);
  pthread_cond_init(&timer_module.c, 0);
  threadCreate(&timer_module.thread_id, time_timer_thread, &timer_module, "timer thread", -1, SCHED_OAI);
}

void time_timer_free(void)
{
  lock(&timer_module);
  timer_module.exit = true;
  do_signal(&timer_module);
  unlock(&timer_module);

  void *retval;
  int ret = pthread_join(timer_module.thread_id, &retval);
  if (ret) LOG_E(UTIL, "pthread_join failed for the time_timer module (%s)\n", strerror(ret));

  lock(&timer_module);
  for (int i = 0; i < MAX_DELAY_MS; i++) {
    time_timer_list_t *l = &timer_module.timers[i];
    shared_pointer_t *cur = l->head;
    while (cur) {
      time_timer_item_t *p = cur->p;
      shared_pointer_t *next = p->next;
      unref_shared_pointer(cur);
      cur = next;
    }
    l->head = 0;
    l->tail = 0;
  }
  unlock(&timer_module);

  (void)pthread_mutex_destroy(&timer_module.m);
  (void)pthread_cond_destroy(&timer_module.c);
  memset(&timer_module, 0, sizeof(timer_module));
}

void time_timer_ms_tick(void)
{
  lock(&timer_module);
  timer_module.cur_time++;
  do_signal(&timer_module);
  unlock(&timer_module);
}

/* start/stop a timer */

void *tick_timeout_start(int ms, void (*callback)(void **p, uint64_t *v), void **p, int psize, uint64_t *v, int vsize)
{
  AssertFatal(ms < MAX_DELAY_MS - 2, "cannot create timer with timeout %d ms, increase MAX_DELAY_MS in the code and recompile\n", ms);

  /* create new timer object */
  time_timer_item_t *new_timer = malloc_or_fail(sizeof(*new_timer));
  new_timer->callback = callback;
  new_timer->p = 0;
  new_timer->v = 0;
  if (psize) {
    new_timer->p = malloc_or_fail(sizeof(*new_timer->p) * psize);
    memcpy(new_timer->p, p, sizeof(*new_timer->p) * psize);
  }
  if (vsize) {
    new_timer->v = malloc_or_fail(sizeof(*new_timer->v) * vsize);
    memcpy(new_timer->v, v, sizeof(*new_timer->v) * vsize);
  }
  new_timer->disabled = false;
  new_timer->callback_running = false;
  pthread_mutex_init(&new_timer->m, 0);
  pthread_cond_init(&new_timer->c, 0);
  new_timer->next = 0;

  /* make a shared pointer around it */
  shared_pointer_t *shared_ptr = new_shared_pointer(new_timer, free_item);
  /* ref it, one local copy in the list, one to the caller, so 2 refs are needed */
  ref_shared_pointer(shared_ptr);

  /* put it in the list */
  lock(&timer_module);
  int pos = (timer_module.cur_time + ms) % MAX_DELAY_MS;
  time_timer_list_t *list = &timer_module.timers[pos];
  list_add(list, shared_ptr);
  unlock(&timer_module);

  return shared_ptr;
}

void tick_timeout_stop(void *_timer)
{
  shared_pointer_t *shared_ptr = _timer;
  time_timer_item_t *timer = shared_ptr->p;
  lock_item(timer);
  timer->disabled = true;
  /* to delete a timer, its callback must not be running */
  while (timer->callback_running)
    do_wait_item(timer);
  unlock_item(timer);
  unref_shared_pointer(shared_ptr);
}
