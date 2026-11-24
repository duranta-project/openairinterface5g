/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! \file nr_ue_dl_slots.h
 * \brief UE DL slots in the thread pool, at most num_parallel in flight, each with its own PDSCH scratch
 */

#ifndef NR_UE_DL_SLOTS_H
#define NR_UE_DL_SLOTS_H

#include <pthread.h>
#include "common/utils/threadPool/thread-pool.h"

/// One DL slot. It holds a PDSCH scratch until its last reference is dropped: the slot task holds one, each
/// DLSCH decoding in flight one.
typedef struct nr_ue_dl_job_s {
  struct nr_ue_dl_job_s *next; // parked jobs
  /// slot task
  void (*run)(struct nr_ue_dl_job_s *job);
  /// frees the job
  void (*release)(struct nr_ue_dl_job_s *job);
  /// NULL when not scheduled (simulators)
  struct nr_ue_dl_slots_s *slots;
  int scratch_idx;
  int refs;
} nr_ue_dl_job_t;

/// A slot without free scratch is parked until one is returned
typedef struct nr_ue_dl_slots_s {
  pthread_mutex_t mutex;
  pthread_cond_t idle;
  tpool_t *tpool;
  /// 0: inline, in the submitting thread
  int num_parallel;
  /// started or parked
  int busy;
  int num_free;
  int *free_scratch;
  nr_ue_dl_job_t *parked_first;
  nr_ue_dl_job_t *parked_last;
} nr_ue_dl_slots_t;

void nr_ue_dl_slots_init(nr_ue_dl_slots_t *slots, int num_parallel, int num_scratch, tpool_t *tpool);
void nr_ue_dl_slots_free(nr_ue_dl_slots_t *slots);
/// Starts job in a free scratch, or parks it. With num_parallel 0, runs it and waits for its end.
void nr_ue_dl_slots_submit(nr_ue_dl_slots_t *slots, nr_ue_dl_job_t *job);
/// Waits for the end of every submitted job
void nr_ue_dl_slots_wait_idle(nr_ue_dl_slots_t *slots);
void nr_ue_dl_job_hold(nr_ue_dl_job_t *job);
/// The last reference releases the job and returns its scratch
void nr_ue_dl_job_drop(nr_ue_dl_job_t *job);

#endif
