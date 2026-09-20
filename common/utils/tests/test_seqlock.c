/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Exercises the seqlock in the way its users rely on: a single writer that
 * never blocks, and readers that must never observe a half-written payload.
 *
 * The payload here is deliberately wider than any atomic type -- an array whose
 * elements are all written to the same value -- so a torn read is detectable:
 * if a reader ever returns a snapshot whose elements disagree, the protocol is
 * broken. */

#include "common/utils/seqlock.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define PAYLOAD_WORDS 64
#define CONCURRENT_WRITES 200000
#define READER_THREADS 3

typedef struct {
  _Atomic uint32_t seq;
  uint32_t payload[PAYLOAD_WORDS];
} shared_t;

static shared_t g_shared;
static _Atomic int g_writer_done;
static _Atomic long g_torn_reads;
static _Atomic long g_completed_reads;

/* Single-threaded: the counter is even at rest, odd while a write is in
 * progress, and a completed write always advances it. */
static void test_counter_parity(void)
{
  _Atomic uint32_t seq = 0;

  const uint32_t begun = seqlock_write_begin(&seq);
  assert((begun & 1u) == 1u && "counter must be odd while writing");
  assert(seqlock_read_begin(&seq) == begun);

  seqlock_write_end(&seq, begun);
  const uint32_t after = seqlock_read_begin(&seq);
  assert((after & 1u) == 0u && "counter must be even at rest");
  assert(after == begun + 1u && "a completed write must advance the counter");

  printf("counter parity: ok\n");
}

/* A read that spans no write does not retry; one that spans a write does. */
static void test_retry_detection(void)
{
  _Atomic uint32_t seq = 0;

  uint32_t snapshot = seqlock_read_begin(&seq);
  assert(!seqlock_read_retry(&seq, snapshot) && "quiescent read must not retry");

  snapshot = seqlock_read_begin(&seq);
  const uint32_t begun = seqlock_write_begin(&seq);
  seqlock_write_end(&seq, begun);
  assert(seqlock_read_retry(&seq, snapshot) && "read spanning a write must retry");

  printf("retry detection: ok\n");
}

static void *reader_thread(void *arg)
{
  (void)arg;
  uint32_t copy[PAYLOAD_WORDS];

  while (!atomic_load_explicit(&g_writer_done, memory_order_relaxed)) {
    const uint32_t begun = seqlock_read_begin(&g_shared.seq);
    if (begun & 1u) /* write in progress: retry */
      continue;
    memcpy(copy, g_shared.payload, sizeof(copy));
    if (seqlock_read_retry(&g_shared.seq, begun)) /* changed under us: retry */
      continue;

    /* A consistent snapshot: every element was written by the same pass. */
    for (int i = 1; i < PAYLOAD_WORDS; i++) {
      if (copy[i] != copy[0]) {
        atomic_fetch_add_explicit(&g_torn_reads, 1, memory_order_relaxed);
        break;
      }
    }
    atomic_fetch_add_explicit(&g_completed_reads, 1, memory_order_relaxed);
  }
  return NULL;
}

/* One writer, several readers: no reader may ever see a torn payload. */
static void test_no_torn_reads(void)
{
  pthread_t readers[READER_THREADS];

  for (int i = 0; i < READER_THREADS; i++)
    assert(pthread_create(&readers[i], NULL, reader_thread, NULL) == 0);

  for (uint32_t pass = 1; pass <= CONCURRENT_WRITES; pass++) {
    const uint32_t begun = seqlock_write_begin(&g_shared.seq);
    for (int i = 0; i < PAYLOAD_WORDS; i++)
      g_shared.payload[i] = pass;
    seqlock_write_end(&g_shared.seq, begun);
  }

  atomic_store_explicit(&g_writer_done, 1, memory_order_relaxed);
  for (int i = 0; i < READER_THREADS; i++)
    pthread_join(readers[i], NULL);

  const long torn = atomic_load_explicit(&g_torn_reads, memory_order_relaxed);
  const long done = atomic_load_explicit(&g_completed_reads, memory_order_relaxed);
  printf("concurrent: %ld consistent reads, %ld torn\n", done, torn);
  assert(torn == 0 && "reader observed a partially written payload");
  assert(done > 0 && "no read completed -- the test proved nothing");
}

int main(void)
{
  test_counter_parity();
  test_retry_detection();
  test_no_torn_reads();
  printf("test_seqlock: PASS\n");
  return 0;
}
