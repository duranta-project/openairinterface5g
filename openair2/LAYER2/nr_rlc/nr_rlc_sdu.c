/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_rlc_sdu.h"

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "LOG/log.h"
#include "common/utils/assertions.h"
#include "common/utils/system.h"

nr_rlc_sdu_segment_t *nr_rlc_new_sdu(
    char *buffer, int size,
    int upper_layer_id)
{
  /* allocate sdu header and data together */
  nr_rlc_sdu_t *sdu = malloc(sizeof(nr_rlc_sdu_t) + size);
  nr_rlc_sdu_segment_t *ret = calloc(1, sizeof(nr_rlc_sdu_segment_t));

  DevAssert(sdu != NULL);
  DevAssert(ret != NULL);

  /* only memset the header */
  memset(sdu, 0 , sizeof(*sdu));

  sdu->ref_count      = 1;
  sdu->sn             = -1;                 /* set later */
  sdu->upper_layer_id = upper_layer_id;
  sdu->data           = (char*)(sdu + 1);
  memcpy(sdu->data, buffer, size);
  sdu->size           = size;
  sdu->retx_count     = -1;

  ret->sdu      = sdu;
  ret->size     = size;
  ret->so       = 0;
  ret->is_first = 1;
  ret->is_last  = 1;

  return ret;
}

/* nr_rlc_defer_free_begin()/end(): segments queued for free(), chained through their
 * now unused next field; seg_and_sdu ones also take their SDU, whose last segment they
 * were.  Thread-local, so no locking; the tails let end() hand a whole chain over at once. */
static __thread bool defer_free;
static __thread nr_rlc_sdu_segment_t *deferred_seg, *deferred_seg_tail;
static __thread nr_rlc_sdu_segment_t *deferred_seg_and_sdu, *deferred_seg_and_sdu_tail;

/* The rlc_free thread frees what end() hands it, so the free() calls -- thousands per
 * STATUS PDU at high DL rate -- leave the caller's thread too, not only the lock.  The
 * caller is L1_rx_thread (via NR_UL_indication), where they cost up to ~2 ms per UL slot.
 * Many producers push whole chains with one CAS; the one consumer takes a whole stack with
 * an exchange, so there is no ABA. */
static _Atomic(nr_rlc_sdu_segment_t *) bg_seg, bg_seg_and_sdu;
static sem_t bg_sem;
static pthread_once_t bg_once = PTHREAD_ONCE_INIT;

static void free_chains(nr_rlc_sdu_segment_t *seg_and_sdu, nr_rlc_sdu_segment_t *seg)
{
  while (seg_and_sdu != NULL) {
    nr_rlc_sdu_segment_t *s = seg_and_sdu;
    seg_and_sdu = s->next;
    free(s->sdu);
    free(s);
  }
  while (seg != NULL) {
    nr_rlc_sdu_segment_t *s = seg;
    seg = s->next;
    free(s);
  }
}

static void push_chain(_Atomic(nr_rlc_sdu_segment_t *) *head, nr_rlc_sdu_segment_t *first, nr_rlc_sdu_segment_t *last)
{
  nr_rlc_sdu_segment_t *old = atomic_load(head);
  do {
    last->next = old;
  } while (!atomic_compare_exchange_weak(head, &old, first));
}

static void *rlc_free_thread(void *arg)
{
  (void)arg;
  while (true) {
    if (sem_wait(&bg_sem) != 0) {
      AssertFatal(errno == EINTR, "sem_wait: %s\n", strerror(errno));
      continue;
    }
    free_chains(atomic_exchange(&bg_seg_and_sdu, NULL), atomic_exchange(&bg_seg, NULL));
  }
  return NULL;
}

static void bg_start(void)
{
  AssertFatal(sem_init(&bg_sem, 0, 0) == 0, "sem_init: %s\n", strerror(errno));
  pthread_t t;
  threadCreate(&t, rlc_free_thread, NULL, "rlc_free", -1, OAI_PRIORITY_RT_LOW);
}

/* Called from nr_rlc_module_init(), so that the thread is created from the init path and not
 * from a real-time caller; end() calls it too, for users that never init the module. */
void nr_rlc_free_thread_start(void)
{
  pthread_once(&bg_once, bg_start);
}

void nr_rlc_defer_free_begin(void)
{
  defer_free = true;
}

void nr_rlc_defer_free_end(void)
{
  defer_free = false;
  if (deferred_seg == NULL && deferred_seg_and_sdu == NULL)
    return;
  nr_rlc_free_thread_start();
  if (deferred_seg_and_sdu != NULL)
    push_chain(&bg_seg_and_sdu, deferred_seg_and_sdu, deferred_seg_and_sdu_tail);
  if (deferred_seg != NULL)
    push_chain(&bg_seg, deferred_seg, deferred_seg_tail);
  sem_post(&bg_sem);
  deferred_seg = deferred_seg_and_sdu = NULL;
  deferred_seg_tail = deferred_seg_and_sdu_tail = NULL;
}

int nr_rlc_free_sdu_segment(nr_rlc_sdu_segment_t *sdu)
{
  int ret = 0;

  sdu->sdu->free_count++;
  if (sdu->sdu->free_count == sdu->sdu->ref_count)
    ret = 1;

  if (defer_free) {
    nr_rlc_sdu_segment_t **l = ret ? &deferred_seg_and_sdu : &deferred_seg;
    nr_rlc_sdu_segment_t **tail = ret ? &deferred_seg_and_sdu_tail : &deferred_seg_tail;
    if (*l == NULL)
      *tail = sdu;
    sdu->next = *l;
    *l = sdu;
    return ret;
  }

  if (ret)
    free(sdu->sdu);
  free(sdu);

  return ret;
}

void nr_rlc_sdu_segment_list_append(nr_rlc_sdu_segment_t **list,
                                    nr_rlc_sdu_segment_t **end,
                                    nr_rlc_sdu_segment_t *sdu)
{
  if (*list == NULL) {
    *list = sdu;
    *end = sdu;
    return;
  }

  (*end)->next = sdu;
  *end = sdu;
}

void nr_rlc_free_sdu_segment_list(nr_rlc_sdu_segment_t *l)
{
  nr_rlc_sdu_segment_t *cur;

  while (l != NULL) {
    cur = l;
    l = l->next;
    nr_rlc_free_sdu_segment(cur);
  }
}
