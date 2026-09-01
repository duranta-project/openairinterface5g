/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Minimal single-writer / multi-reader seqlock over a 32-bit atomic counter.
 * The writer never blocks; readers retry while a write is in progress or the
 * counter moved across their read (odd counter = write in progress, even =
 * stable). The protected payload lives in the caller's own storage. Suits an
 * RT writer that must not block on readers (e.g. a SCHED_FIFO scheduler thread).
 */
#ifndef COMMON_UTILS_SEQLOCK_H
#define COMMON_UTILS_SEQLOCK_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

/* Writer: bracket the payload stores between begin and end.
 *     uint32_t s = seqlock_write_begin(&seq);
 *     <store the payload>
 *     seqlock_write_end(&seq, s); */
static inline uint32_t seqlock_write_begin(_Atomic uint32_t *seq)
{
  uint32_t odd = atomic_load_explicit(seq, memory_order_relaxed) | 1u;
  atomic_store_explicit(seq, odd, memory_order_release);
  return odd;
}
static inline void seqlock_write_end(_Atomic uint32_t *seq, uint32_t begun)
{
  atomic_store_explicit(seq, begun + 1u, memory_order_release);
}

/* Reader: snapshot the counter, copy the payload, then re-check. Retry the
 * whole read while the begin value is odd or seqlock_read_retry() is true.
 *     uint32_t s = seqlock_read_begin(&seq);
 *     if (s & 1u) continue;        // write in progress
 *     <copy the payload>
 *     if (seqlock_read_retry(&seq, s)) continue;   // changed under us */
static inline uint32_t seqlock_read_begin(const _Atomic uint32_t *seq)
{
  return atomic_load_explicit(seq, memory_order_acquire);
}
static inline bool seqlock_read_retry(const _Atomic uint32_t *seq, uint32_t begun)
{
  /* Acquire fence (not a bare acquire-load) so the plain payload reads are
   * ordered before this second load — an acquire-load only orders what follows. */
  atomic_thread_fence(memory_order_acquire);
  return atomic_load_explicit(seq, memory_order_relaxed) != begun;
}

#endif /* COMMON_UTILS_SEQLOCK_H */
