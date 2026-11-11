/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Exercises the seqlock through its public API: a single writer that never
 * blocks, and readers that must never be handed a half-written payload.
 *
 * The payload is deliberately wider than any atomic type -- an array whose
 * elements are all written to the same value -- so a torn read is detectable:
 * if a reader ever returns a snapshot whose elements disagree, the protocol is
 * broken. On x86 this cannot catch a missing release fence in the writer (the
 * hardware does not reorder the stores); that ordering is a property of the
 * generated code on weaker architectures and is not what this test proves. */

#include "common/utils/ds/seqlock.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>

#define PAYLOAD_WORDS 64
#define CONCURRENT_WRITES 200000
#define READER_THREADS 3
#define READ_TRIES 8

typedef struct {
  uint32_t word[PAYLOAD_WORDS];
} payload_t;

static seqlock_t g_seq;
static payload_t g_shared;
static _Atomic int g_writer_done;
static _Atomic long g_torn_reads;
static _Atomic long g_completed_reads;

static void fill(payload_t *p, uint32_t value)
{
  for (int i = 0; i < PAYLOAD_WORDS; i++)
    p->word[i] = value;
}

static bool consistent(const payload_t *p)
{
  for (int i = 1; i < PAYLOAD_WORDS; i++)
    if (p->word[i] != p->word[0])
      return false;
  return true;
}

/* What a writer stores is what a reader gets back, and every completed write
 * leaves the counter even (stable) and advanced. */
static void test_round_trip(void)
{
  seqlock_t seq = 0;
  payload_t shared, in, out;
  fill(&shared, 0);
  fill(&in, 7);
  fill(&out, 0);

  seqlock_write(&seq, &shared, &in, sizeof(in));
  assert((atomic_load(&seq) & 1u) == 0u && "counter must be even at rest");
  assert(atomic_load(&seq) == 2u && "a completed write must advance the counter");
  assert(seqlock_read(&seq, &out, &shared, sizeof(out), READ_TRIES));
  assert(out.word[0] == 7 && consistent(&out));

  fill(&in, 9);
  seqlock_write(&seq, &shared, &in, sizeof(in));
  assert(atomic_load(&seq) == 4u);
  assert(seqlock_read(&seq, &out, &shared, sizeof(out), READ_TRIES));
  assert(out.word[0] == 9 && consistent(&out));

  printf("round trip: ok\n");
}

/* A counter left odd is a write in progress: the read gives up after its tries
 * rather than returning the payload or spinning forever. */
static void test_read_gives_up_during_write(void)
{
  seqlock_t seq = 1;
  payload_t shared, out;
  fill(&shared, 3);
  fill(&out, 0);

  assert(!seqlock_read(&seq, &out, &shared, sizeof(out), READ_TRIES) && "read must not succeed mid-write");
  assert(!seqlock_read(&seq, &out, &shared, sizeof(out), 0) && "no tries, no snapshot");
  assert(out.word[0] == 0 && "no payload bytes may be copied while a write is in progress");

  atomic_store(&seq, 2u);
  assert(seqlock_read(&seq, &out, &shared, sizeof(out), READ_TRIES));
  assert(out.word[0] == 3);

  printf("read during write: ok\n");
}

static void *reader_thread(void *arg)
{
  (void)arg;
  payload_t copy;

  while (!atomic_load_explicit(&g_writer_done, memory_order_relaxed)) {
    if (!seqlock_read(&g_seq, &copy, &g_shared, sizeof(copy), READ_TRIES))
      continue; /* contended: the caller decides, here simply try again */
    if (!consistent(&copy))
      atomic_fetch_add_explicit(&g_torn_reads, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&g_completed_reads, 1, memory_order_relaxed);
  }
  return NULL;
}

/* One writer, several readers: no reader may ever see a torn payload. */
static void test_no_torn_reads(void)
{
  pthread_t readers[READER_THREADS];

  for (int i = 0; i < READER_THREADS; i++) {
    const int rc = pthread_create(&readers[i], NULL, reader_thread, NULL);
    assert(rc == 0);
    (void)rc;
  }

  for (uint32_t pass = 1; pass <= CONCURRENT_WRITES; pass++) {
    payload_t next;
    fill(&next, pass);
    seqlock_write(&g_seq, &g_shared, &next, sizeof(next));
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
  test_round_trip();
  test_read_gives_up_during_write();
  test_no_torn_reads();
  printf("test_seqlock: PASS\n");
  return 0;
}
