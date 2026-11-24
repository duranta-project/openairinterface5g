/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* DL slot scheduler: at most num_parallel jobs in flight, parking, every job released once */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nr_ue_dl_slots.h"
#include "log.h"

#define MAX_SCRATCH 8

typedef struct {
  nr_ue_dl_job_t job;
  int runs;
  int releases;
} test_job_t;

static tpool_t *pool;
static int in_flight, max_in_flight, released, scratch_conflict, inline_mode;
static int scratch_busy[MAX_SCRATCH];
static int num_decodes;

static int failures;
#define CHECK(cond, ...)                                   \
  do {                                                     \
    if (!(cond)) {                                         \
      fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
      fprintf(stderr, __VA_ARGS__);                        \
      fprintf(stderr, "\n");                               \
      failures++;                                          \
    }                                                      \
  } while (0)

static void decode_task(void *arg)
{
  test_job_t *j = arg;
  usleep(200);
  nr_ue_dl_job_drop(&j->job);
}

static void run(nr_ue_dl_job_t *job)
{
  test_job_t *j = (test_job_t *)job;
  __atomic_add_fetch(&j->runs, 1, __ATOMIC_SEQ_CST);
  if (__atomic_exchange_n(&scratch_busy[job->scratch_idx], 1, __ATOMIC_SEQ_CST))
    __atomic_store_n(&scratch_conflict, 1, __ATOMIC_SEQ_CST);
  const int f = __atomic_add_fetch(&in_flight, 1, __ATOMIC_SEQ_CST);
  int m = __atomic_load_n(&max_in_flight, __ATOMIC_SEQ_CST);
  while (f > m && !__atomic_compare_exchange_n(&max_in_flight, &m, f, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
    ;
  usleep(100);
  // decodings complete in other tasks
  for (int d = 0; d < num_decodes; d++) {
    nr_ue_dl_job_hold(job);
    pushTpool(pool, (task_t){.func = decode_task, .args = j});
  }
}

static void release(nr_ue_dl_job_t *job)
{
  test_job_t *j = (test_job_t *)job;
  __atomic_add_fetch(&j->releases, 1, __ATOMIC_SEQ_CST);
  __atomic_store_n(&scratch_busy[job->scratch_idx], 0, __ATOMIC_SEQ_CST);
  __atomic_sub_fetch(&in_flight, 1, __ATOMIC_SEQ_CST);
  __atomic_add_fetch(&released, 1, __ATOMIC_SEQ_CST);
}

/// returns the largest number of jobs submitted and not ended
static int run_case(const char *name, char *pool_config, int num_parallel, int num_jobs, int decodes)
{
  tpool_t tp;
  initNamedTpool(pool_config, &tp, false, "test");
  pool = &tp;
  in_flight = max_in_flight = released = scratch_conflict = 0;
  memset(scratch_busy, 0, sizeof(scratch_busy));
  num_decodes = decodes;
  inline_mode = num_parallel == 0;

  nr_ue_dl_slots_t slots;
  const int num_scratch = num_parallel > 0 ? num_parallel : 1;
  nr_ue_dl_slots_init(&slots, num_parallel, num_scratch, pool);
  test_job_t *jobs = calloc(num_jobs, sizeof(*jobs));
  int max_busy = 0;
  for (int i = 0; i < num_jobs; i++) {
    jobs[i].job = (nr_ue_dl_job_t){.run = run, .release = release};
    nr_ue_dl_slots_submit(&slots, &jobs[i].job);
    pthread_mutex_lock(&slots.mutex);
    if (slots.busy > max_busy)
      max_busy = slots.busy;
    pthread_mutex_unlock(&slots.mutex);
    if (inline_mode)
      CHECK(__atomic_load_n(&released, __ATOMIC_SEQ_CST) == i + 1, "%s: inline submit returned before job %d ended", name, i);
  }
  nr_ue_dl_slots_wait_idle(&slots);

  CHECK(released == num_jobs, "%s: %d of %d jobs released", name, released, num_jobs);
  for (int i = 0; i < num_jobs; i++)
    CHECK(jobs[i].runs == 1 && jobs[i].releases == 1,
          "%s: job %d ran %d times, released %d times",
          name,
          i,
          jobs[i].runs,
          jobs[i].releases);
  CHECK(max_in_flight <= num_scratch, "%s: %d jobs in flight, at most %d allowed", name, max_in_flight, num_scratch);
  CHECK(!scratch_conflict, "%s: a scratch was used by two jobs in flight", name);
  printf("%s: %d jobs, at most %d in flight, at most %d submitted and not ended\n", name, num_jobs, max_in_flight, max_busy);

  nr_ue_dl_slots_free(&slots);
  free(jobs);
  abortTpool(&tp);
  return max_busy;
}

int main(void)
{
  logInit();
  alarm(120); // a deadlock fails the test

  // submitted faster than they end: parked
  int busy = run_case("parallel 2, 4 workers", "-1,-1,-1,-1", 2, 200, 2);
  CHECK(max_in_flight == 2, "parallel 2: the 2 slots were never used together");
  CHECK(busy > 2, "parallel 2: no job was parked");
  // everything on a single worker
  busy = run_case("parallel 3, 1 worker", "-1", 3, 100, 2);
  CHECK(busy > 3, "parallel 3: no job was parked");
  // no decoding: the slot task drops the last reference
  run_case("parallel 4, 4 workers, no decode", "-1,-1,-1,-1", 4, 200, 0);
  run_case("inline", "-1,-1", 0, 50, 2);
  // no worker: tasks run in the pushing thread
  run_case("parallel 2, no worker", "n", 2, 50, 2);

  if (failures) {
    fprintf(stderr, "%d failures\n", failures);
    return 1;
  }
  printf("all passed\n");
  return 0;
}
