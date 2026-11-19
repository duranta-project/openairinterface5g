/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_ue_dl_slots.h"
#include "assertions.h"
#include "common/utils/threadPool/thread-pool.h"
#include "common/utils/threadPool/pthread_utils.h"
#include "common/utils/utils.h"

void nr_ue_dl_slots_init(nr_ue_dl_slots_t *slots, int num_parallel, int num_scratch, tpool_t *tpool)
{
  AssertFatal(num_parallel >= 0 && num_scratch >= max(num_parallel, 1),
              "%d DL slots in parallel need as many PDSCH scratches, have %d\n",
              num_parallel,
              num_scratch);
  *slots = (nr_ue_dl_slots_t){.tpool = tpool, .num_parallel = num_parallel};
  pthread_mutex_init(&slots->mutex, NULL);
  pthread_cond_init(&slots->idle, NULL);
  slots->num_free = max(num_parallel, 1);
  slots->free_scratch = calloc_or_fail(slots->num_free, sizeof(*slots->free_scratch));
  for (int i = 0; i < slots->num_free; i++)
    slots->free_scratch[i] = i;
}

void nr_ue_dl_slots_free(nr_ue_dl_slots_t *slots)
{
  nr_ue_dl_slots_wait_idle(slots);
  free_and_zero(slots->free_scratch);
  pthread_cond_destroy(&slots->idle);
  pthread_mutex_destroy(&slots->mutex);
}

static void run_job(void *arg)
{
  nr_ue_dl_job_t *job = arg;
  job->refs = 1;
  job->run(job);
  nr_ue_dl_job_drop(job);
}

static void start_job(nr_ue_dl_slots_t *slots, nr_ue_dl_job_t *job, int scratch_idx)
{
  job->scratch_idx = scratch_idx;
  if (slots->num_parallel == 0)
    run_job(job);
  else
    pushTpool(slots->tpool, (task_t){.func = run_job, .args = job});
}

void nr_ue_dl_slots_submit(nr_ue_dl_slots_t *slots, nr_ue_dl_job_t *job)
{
  job->slots = slots;
  job->next = NULL;
  mutexlock(slots->mutex);
  slots->busy++;
  if (slots->num_free == 0) {
    if (slots->parked_last)
      slots->parked_last->next = job;
    else
      slots->parked_first = job;
    slots->parked_last = job;
    mutexunlock(slots->mutex);
    return;
  }
  const int scratch_idx = slots->free_scratch[--slots->num_free];
  mutexunlock(slots->mutex);
  start_job(slots, job, scratch_idx);
  // inline: also wait for the decodings
  if (slots->num_parallel == 0)
    nr_ue_dl_slots_wait_idle(slots);
}

/// The scratch goes to the oldest parked job, or back to the free ones
static void return_scratch(nr_ue_dl_slots_t *slots, int scratch_idx)
{
  mutexlock(slots->mutex);
  slots->busy--;
  nr_ue_dl_job_t *job = slots->parked_first;
  if (job) {
    slots->parked_first = job->next;
    if (!slots->parked_first)
      slots->parked_last = NULL;
  } else {
    slots->free_scratch[slots->num_free++] = scratch_idx;
  }
  if (slots->busy == 0)
    pthread_cond_broadcast(&slots->idle);
  mutexunlock(slots->mutex);
  if (job)
    start_job(slots, job, scratch_idx);
}

void nr_ue_dl_slots_wait_idle(nr_ue_dl_slots_t *slots)
{
  mutexlock(slots->mutex);
  while (slots->busy > 0)
    condwait(slots->idle, slots->mutex);
  mutexunlock(slots->mutex);
}

void nr_ue_dl_job_hold(nr_ue_dl_job_t *job)
{
  __atomic_add_fetch(&job->refs, 1, __ATOMIC_RELAXED);
}

void nr_ue_dl_job_drop(nr_ue_dl_job_t *job)
{
  if (__atomic_sub_fetch(&job->refs, 1, __ATOMIC_ACQ_REL) > 0)
    return;
  nr_ue_dl_slots_t *slots = job->slots;
  const int scratch_idx = job->scratch_idx;
  job->release(job);
  if (slots)
    return_scratch(slots, scratch_idx);
}
