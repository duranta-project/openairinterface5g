/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "wait.h"
#include <errno.h>
#include <stdatomic.h>
#include "common/utils/assertions.h"
#include "common/utils/threadPool/pthread_utils.h"

ue_low_phy_deadline_t ue_low_phy_deadline_in(int timeout_us)
{
  ue_low_phy_deadline_t d = {.none = timeout_us < 0};
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
  int ret = pthread_condattr_init(&attr);
  AssertFatal(ret == 0, "ret=%d\n", ret);
  ret = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
  AssertFatal(ret == 0, "ret=%d\n", ret);
  ret = pthread_cond_init(cond, &attr);
  AssertFatal(ret == 0, "ret=%d\n", ret);
  ret = pthread_condattr_destroy(&attr);
  AssertFatal(ret == 0, "ret=%d\n", ret);
}

// False once the deadline passed.
static bool cond_wait(pthread_cond_t *cond, pthread_mutex_t *lock, const ue_low_phy_deadline_t *deadline)
{
  if (deadline->none) {
    condwait(*cond, *lock);
    return true;
  }
  const int ret = pthread_cond_timedwait(cond, lock, &deadline->at);
  AssertFatal(ret == 0 || ret == ETIMEDOUT, "ret=%d\n", ret);
  return ret == 0;
}

// Many clients wait for whole slots while the value moves per symbol: waking them
// all on every step costs more than the work. Waiters still short re-arm wake_at.
static void wake(ue_low_phy_progress_t *p)
{
  if (p->value < p->wake_at)
    return;
  p->wake_at = UINT64_MAX;
  condbroadcast(p->cond);
}

void ue_low_phy_progress_init(ue_low_phy_progress_t *p, uint64_t value)
{
  mutexinit(p->lock);
  cond_init(&p->cond);
  p->value = value;
  p->wake_at = UINT64_MAX;
  p->closed = false;
}

void ue_low_phy_progress_destroy(ue_low_phy_progress_t *p)
{
  conddestroy(p->cond);
  mutexdestroy(p->lock);
}

uint64_t ue_low_phy_progress_get(ue_low_phy_progress_t *p)
{
  mutexlock(p->lock);
  const uint64_t v = p->value;
  mutexunlock(p->lock);
  return v;
}

void ue_low_phy_progress_raise(ue_low_phy_progress_t *p, uint64_t v)
{
  mutexlock(p->lock);
  if (v > p->value) {
    p->value = v;
    wake(p);
  }
  mutexunlock(p->lock);
}

uint64_t ue_low_phy_progress_add(ue_low_phy_progress_t *p, uint64_t n)
{
  mutexlock(p->lock);
  p->value += n;
  const uint64_t v = p->value;
  wake(p);
  mutexunlock(p->lock);
  return v;
}

ue_low_phy_wait_result_t ue_low_phy_progress_wait(ue_low_phy_progress_t *p, uint64_t target, int timeout_us)
{
  const ue_low_phy_deadline_t deadline = ue_low_phy_deadline_in(timeout_us);
  mutexlock(p->lock);
  while (p->value < target && !p->closed) {
    if (target < p->wake_at)
      p->wake_at = target;
    if (!cond_wait(&p->cond, &p->lock, &deadline))
      break;
  }
  const ue_low_phy_wait_result_t r = p->value >= target ? UE_LOW_PHY_REACHED : p->closed ? UE_LOW_PHY_CLOSED : UE_LOW_PHY_TIMEOUT;
  mutexunlock(p->lock);
  return r;
}

uint64_t ue_low_phy_progress_get_after_reads(ue_low_phy_progress_t *p)
{
  // Taking the lock is an acquire: it does not keep earlier loads from moving after it.
  atomic_thread_fence(memory_order_acquire);
  return ue_low_phy_progress_get(p);
}

void ue_low_phy_progress_close(ue_low_phy_progress_t *p)
{
  mutexlock(p->lock);
  p->closed = true;
  condbroadcast(p->cond);
  mutexunlock(p->lock);
}

void ue_low_phy_monitor_init(ue_low_phy_monitor_t *m)
{
  mutexinit(m->lock);
  cond_init(&m->cond);
}

void ue_low_phy_monitor_destroy(ue_low_phy_monitor_t *m)
{
  conddestroy(m->cond);
  mutexdestroy(m->lock);
}

void ue_low_phy_monitor_lock(ue_low_phy_monitor_t *m)
{
  mutexlock(m->lock);
}

void ue_low_phy_monitor_unlock(ue_low_phy_monitor_t *m)
{
  mutexunlock(m->lock);
}

void ue_low_phy_monitor_notify(ue_low_phy_monitor_t *m)
{
  condbroadcast(m->cond);
}

bool ue_low_phy_monitor_wait(ue_low_phy_monitor_t *m, const ue_low_phy_deadline_t *deadline)
{
  return cond_wait(&m->cond, &m->lock, deadline);
}
