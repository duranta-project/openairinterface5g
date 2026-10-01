/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef WORK_Q_H_
#define WORK_Q_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(__cplusplus)
#include <atomic>
// use atomic types to help interworking with C++
using std::atomic_size_t;
using std::atomic_uchar;
#else
#include <stdatomic.h>
#endif

// Multi-producer, multi-consumer slot-handoff work queue. A producer copies an item into the
// next ring slot and hands that slot pointer to exactly one consumer (e.g. a thread-pool worker);
// the consumer owns the slot until it calls work_q_done(). Slot reuse is gated on ownership, not
// on a global in-flight count: push() refuses to wrap into a slot whose consumer has not finished,
// so a slow consumer can never have its item overwritten by a later push - the flaw of count-based
// admission, which only knows how many consumers are busy, not which slot is still owned.
//
// Producers may be any number of threads: each push reserves its slot with a fetch_add, so
// concurrent pushes always get distinct slots. (When the queue is nearly full, a push can fail -
// NULL - while another producer reserves a free slot: the drop is counted by the caller.)
// Consumers may be any number of threads. cnt must be a power of two (used as a mask); the
// usable depth is exactly cnt. The slot pitch is rounded up to 32 bytes and the slot array is
// 32-byte aligned, so any slot is a valid aligned scratch for SIMD payloads (e.g. FFT windows).
typedef struct work_q {
  uint8_t *slots;
  size_t elsiz;            // element size: how many bytes push copies from src
  size_t stride;           // slot pitch: elsiz rounded up to 32 bytes
  size_t mask;             // cnt - 1
  atomic_size_t prod_head; // producer write index (monotonic, masked on use; advanced by fetch_add)
  atomic_uchar *busy;      // per-slot ownership flag: 1 = handed out, 0 = free
} work_q_t;

bool work_q_alloc(work_q_t *q, size_t cnt, size_t elsiz);
void work_q_free(work_q_t *q);

// Copy src into the next free ring slot. Returns a pointer to the slot, to be handed to a single
// consumer and kept valid until work_q_done(), or NULL if the queue is full (the target slot's
// previous consumer has not called work_q_done() yet).
void *work_q_push(work_q_t *q, const void *src);

// Consumer-side: release a slot previously returned by work_q_push(). Must be called exactly once
// per slot, after the consumer has finished reading it. The release store makes everything the
// consumer read from the slot visible to the producer's acquire load before it reuses the slot.
void work_q_done(work_q_t *q, void *slot);

#endif /* WORK_Q_H_ */
