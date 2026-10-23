/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "split7_wait.h"
#include <errno.h>
#include <stdatomic.h>

split7_deadline_t split7_deadline_in(int timeout_us)
{
  split7_deadline_t d = {.none = timeout_us < 0};
  if (d.none)
    return d;
  clock_gettime(CLOCK_MONOTONIC, &d.at);
  d.at.tv_sec += timeout_us / 1000000;
  d.at.tv_nsec += (timeout_us % 1000000) * 1000L;
  if (d.at.tv_nsec >= 1000000000L) {
    d.at.tv_sec++;
    d.at.tv_nsec -= 1000000000L;
  }
  return d;
}

static void cond_init(pthread_cond_t *cond)
{
  pthread_condattr_t attr;
  pthread_condattr_init(&attr);
  pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
  pthread_cond_init(cond, &attr);
  pthread_condattr_destroy(&attr);
}

// False once the deadline passed.
static bool cond_wait(pthread_cond_t *cond, pthread_mutex_t *lock, const split7_deadline_t *deadline)
{
  if (deadline->none) {
    pthread_cond_wait(cond, lock);
    return true;
  }
  return pthread_cond_timedwait(cond, lock, &deadline->at) != ETIMEDOUT;
}

void split7_progress_init(split7_progress_t *p, uint64_t value)
{
  pthread_mutex_init(&p->lock, NULL);
  cond_init(&p->cond);
  p->value = value;
  p->closed = false;
}

void split7_progress_destroy(split7_progress_t *p)
{
  pthread_cond_destroy(&p->cond);
  pthread_mutex_destroy(&p->lock);
}

uint64_t split7_progress_get(split7_progress_t *p)
{
  pthread_mutex_lock(&p->lock);
  const uint64_t v = p->value;
  pthread_mutex_unlock(&p->lock);
  return v;
}

void split7_progress_raise(split7_progress_t *p, uint64_t v)
{
  pthread_mutex_lock(&p->lock);
  if (v > p->value) {
    p->value = v;
    pthread_cond_broadcast(&p->cond);
  }
  pthread_mutex_unlock(&p->lock);
}

uint64_t split7_progress_add(split7_progress_t *p, uint64_t n)
{
  pthread_mutex_lock(&p->lock);
  p->value += n;
  const uint64_t v = p->value;
  pthread_cond_broadcast(&p->cond);
  pthread_mutex_unlock(&p->lock);
  return v;
}

split7_wait_result_t split7_progress_wait(split7_progress_t *p, uint64_t target, int timeout_us)
{
  const split7_deadline_t deadline = split7_deadline_in(timeout_us);
  pthread_mutex_lock(&p->lock);
  while (p->value < target && !p->closed && cond_wait(&p->cond, &p->lock, &deadline)) {
  }
  const split7_wait_result_t r = p->value >= target ? SPLIT7_REACHED : p->closed ? SPLIT7_CLOSED : SPLIT7_TIMEOUT;
  pthread_mutex_unlock(&p->lock);
  return r;
}

uint64_t split7_progress_get_after_reads(split7_progress_t *p)
{
  // Taking the lock is an acquire: it does not keep earlier loads from moving after it.
  atomic_thread_fence(memory_order_acquire);
  return split7_progress_get(p);
}

void split7_progress_close(split7_progress_t *p)
{
  pthread_mutex_lock(&p->lock);
  p->closed = true;
  pthread_cond_broadcast(&p->cond);
  pthread_mutex_unlock(&p->lock);
}

void split7_monitor_init(split7_monitor_t *m)
{
  pthread_mutex_init(&m->lock, NULL);
  cond_init(&m->cond);
}

void split7_monitor_destroy(split7_monitor_t *m)
{
  pthread_cond_destroy(&m->cond);
  pthread_mutex_destroy(&m->lock);
}

void split7_monitor_lock(split7_monitor_t *m)
{
  pthread_mutex_lock(&m->lock);
}

void split7_monitor_unlock(split7_monitor_t *m)
{
  pthread_mutex_unlock(&m->lock);
}

void split7_monitor_notify(split7_monitor_t *m)
{
  pthread_cond_broadcast(&m->cond);
}

bool split7_monitor_wait(split7_monitor_t *m, const split7_deadline_t *deadline)
{
  return cond_wait(&m->cond, &m->lock, deadline);
}
