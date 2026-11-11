/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Single-writer / multi-reader seqlock: the writer never blocks, readers retry
 * while a write is in progress or the counter moved across their copy. See
 * https://docs.kernel.org/locking/seqlock.html (the protocol is the same).
 *
 * The protected payload lives in the caller's storage; the counter is odd while
 * a write is in progress and even at rest. Suits an RT writer that must not
 * block on readers (e.g. a SCHED_FIFO scheduler thread) and small payloads read
 * by a slower thread. Not for payloads holding pointers.
 *
 *   seqlock_t sl = 0;   shared_t shared;
 *   seqlock_write(&sl, &shared, &local, sizeof(shared));            writer only
 *   if (seqlock_read(&sl, &local, &shared, sizeof(shared), 8)) ...  any reader
 *
 * The payload is moved with memcpy, so a reader can copy bytes while the writer
 * stores them; such a copy is discarded by the counter check and never returned.
 */
#ifndef SEQLOCK_H_
#define SEQLOCK_H_

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef _Atomic uint32_t seqlock_t;

/* Copy n bytes from src into the shared storage dst. One writer at a time. */
static inline void seqlock_write(seqlock_t *sl, void *dst, const void *src, size_t n)
{
  const uint32_t odd = atomic_load_explicit(sl, memory_order_relaxed) | 1u;
  atomic_store_explicit(sl, odd, memory_order_relaxed);
  /* A release store only orders the accesses before it. The fence keeps the
   * payload stores below from being reordered above the odd counter. */
  atomic_thread_fence(memory_order_release);
  memcpy(dst, src, n);
  atomic_store_explicit(sl, odd + 1u, memory_order_release);
}

/* Copy n bytes of the shared storage src into dst. Returns true with a
 * consistent snapshot in dst, false if every one of max_tries attempts met a
 * write; dst is then unspecified. */
static inline bool seqlock_read(const seqlock_t *sl, void *dst, const void *src, size_t n, int max_tries)
{
  for (int i = 0; i < max_tries; i++) {
    const uint32_t begun = atomic_load_explicit(sl, memory_order_acquire);
    if (begun & 1u) /* write in progress */
      continue;
    memcpy(dst, src, n);
    /* An acquire fence, not an acquire load: it orders the payload reads above
     * before this re-check, which a load only does for what follows it. */
    atomic_thread_fence(memory_order_acquire);
    if (atomic_load_explicit(sl, memory_order_relaxed) == begun)
      return true;
  }
  return false;
}

#endif /* SEQLOCK_H_ */
