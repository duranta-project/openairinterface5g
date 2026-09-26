/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**
 * @file spectrum_sensing_ring.h
 * @brief /e3_l2_sensing POSIX shm ring carrying the MAC sensing ranges.
 *
 * Rather than putting up to 128 ranges in every indication, the worker writes
 * them into this 256-slot ring and the indication carries only a reference
 * { write_idx, n_ranges } (+ sfn/slot). The dApp mmaps the region read-only and
 * reads the ranges directly — no JSON parse, far less wire traffic.
 *
 * Each slot self-tags its (sfn, slot); the dApp checks that against the
 * indication before trusting the ranges, so a wrapped-over (stale) slot is
 * dropped, never used. Single producer (the worker thread), so no lock is
 * needed: the slot is fully written before its indication is sent.
 */
#ifndef SPECTRUM_SENSING_RING_H
#define SPECTRUM_SENSING_RING_H

#include <stdint.h>

#include "LAYER2/NR_MAC_gNB/gNB_scheduler_ul_sensing_types.h" /* nr_mac_sensing_publish_meta_t, sensing_range_t, MAX_SENSING_RANGES */

#ifdef __cplusplus
extern "C" {
#endif

/* shm segment name. Separate from the L1 SM's /e3_ran_buffers. */
#define SPECTRUM_SENSING_RING_SHM_NAME "/e3_l2_sensing"

/* Ring depth: ~one write per UL slot (~0.5 ms), so 256 slots ≈ 128 ms of
 * history — far more than the dApp's read latency, so it never reads a slot
 * the producer is about to overwrite. Total region ≈ 530 KB. */
#define SPECTRUM_SENSING_RING_SLOTS 256u

/* Region header (64 bytes). The stride/size fields let the dApp check its
 * compiled layout against the live producer and bail on a mismatch. */
typedef struct {
  uint32_t version; /* = 1 */
  uint32_t slot_count; /* = SPECTRUM_SENSING_RING_SLOTS */
  uint32_t slot_stride; /* = sizeof(spectrum_sensing_ring_slot_t) */
  uint32_t max_ranges; /* = MAX_SENSING_RANGES */
  uint32_t range_size; /* = sizeof(sensing_range_t) */
  uint32_t reserved[11];
} spectrum_sensing_ring_header_t;

/* One ring slot. ranges[] is fixed-size so its offset matches on both sides;
 * n_ranges says how many entries are live. */
typedef struct {
  uint16_t sfn; /* SFN, 0..1023 (== meta->frame) */
  uint16_t slot; /* slot in frame */
  uint16_t beam; /* beam index */
  uint16_t n_ranges; /* live entries in ranges[], 0..MAX_SENSING_RANGES */
  uint64_t timestamp_ns; /* CLOCK_MONOTONIC at write time */
  uint32_t seq; /* monotonic write counter (freshness aid) */
  uint32_t _pad;
  sensing_range_t ranges[MAX_SENSING_RANGES];
} spectrum_sensing_ring_slot_t;

/* ---- Lifecycle ---- */

/* Create the shm region (idempotent). Made world-readable so a dApp under
 * another UID can mmap it; any stale segment is removed first.
 * Returns 0 on success, -1 on failure (logged once). */
int spectrum_sensing_ring_init(void);

/* Tear down the shm region. Idempotent. */
void spectrum_sensing_ring_destroy(void);

/* ---- Producer (SM worker thread only) ---- */

/* Write one slot's ranges into the ring; *out_write_idx returns the index the
 * indication should reference. Lazily creates the region if needed.
 * Returns 0 on success, -1 if the region is unavailable (then *out_write_idx
 * is untouched and the caller should skip this batch's indication). */
int spectrum_sensing_ring_write(const nr_mac_sensing_publish_meta_t *meta,
                                const sensing_range_t *ranges,
                                uint8_t n_ranges,
                                uint32_t *out_write_idx);

#ifdef __cplusplus
}
#endif

#endif /* SPECTRUM_SENSING_RING_H */
