/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "work_q.h"

bool work_q_alloc(work_q_t *q, size_t cnt, size_t elsiz)
{
  assert(cnt > 0 && (cnt & (cnt - 1)) == 0); // power of two, masked on use
  assert(elsiz > 0);
  const size_t stride = (elsiz + 31) & ~(size_t)31;
  void *slots = NULL;
  if (posix_memalign(&slots, 64, cnt * stride) != 0) {
    return false;
  }
  memset(slots, 0, cnt * stride);
  q->slots = slots;
  q->busy = calloc(cnt, sizeof(*q->busy));
  if (q->busy == NULL) {
    free(q->slots);
    q->slots = NULL;
    return false;
  }
  q->elsiz = elsiz;
  q->stride = stride;
  q->mask = cnt - 1;
  atomic_init(&q->prod_head, 0);
  return true;
}

void work_q_free(work_q_t *q)
{
  free(q->slots);
  free(q->busy);
  memset(q, 0, sizeof(*q));
}

void *work_q_push(work_q_t *q, const void *src)
{
  const size_t idx = atomic_fetch_add_explicit(&q->prod_head, 1, memory_order_relaxed) & q->mask;
  if (atomic_exchange_explicit(&q->busy[idx], 1, memory_order_acq_rel)) {
    return NULL;
  }

  uint8_t *slot = &q->slots[idx * q->stride];
  memcpy(slot, src, q->elsiz);
  return slot;
}

void work_q_done(work_q_t *q, void *slot)
{
  uint8_t *s = (uint8_t *)slot;
  assert(s >= q->slots && s < q->slots + (q->mask + 1) * q->stride);
  assert(((size_t)(s - q->slots) % q->stride) == 0);
  const size_t idx = (size_t)(s - q->slots) / q->stride;
  atomic_store_explicit(&q->busy[idx], 0, memory_order_release);
}
