/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**
 * @file e3_ran_buffers.h
 * @brief Writes post-FFT IQ into the cuBB-compatible /e3_ran_buffers shm.
 *
 * Owns the POSIX shared-memory region /e3_ran_buffers and its header layout.
 * The PHY pushes each UL slot's IQ here; the SM worker reads the latest
 * published slot and advertises it. Producer/consumer hand-off uses a small
 * mutex + condition variable (the snapshot is a plain struct, so the copy
 * happens under the lock).
 */
#ifndef E3_RAN_BUFFERS_H
#define E3_RAN_BUFFERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "PHY/defs_gNB.h"

#define E3_RB_SHM_NAME "/e3_ran_buffers"

/* ---- shm header layout ---- */
typedef struct {
  uint32_t version;
  uint32_t fh_buffer_size;
  uint32_t pusch_buffer_size;
  uint32_t hest_buffer_size;
  uint32_t num_fh_samples;
  uint32_t num_fh_rows;
  uint32_t num_pusch_rows;
  uint32_t num_hest_rows;
  uint32_t max_hest_samples_per_row;
  uint32_t reserved[7];
} e3_ran_buffers_header_t;

/* Slot indices + metadata the SM embeds in each outbound indication. */
typedef struct {
  uint8_t fh_buffer_index;
  uint32_t fh_write_index;
  uint8_t pusch_buffer_index;
  uint32_t pusch_write_index;
  uint8_t hest_buffer_index;
  uint32_t hest_write_index;
  uint32_t hest_data_size;
  uint64_t timestamp_ns;
  uint16_t sfn;
  uint16_t slot;
  uint16_t cell_id;
  uint16_t n_rx_ant;
  /* Bit s set = symbol s carries genuine off-air UL per the TDD config
   * (0x3FFF on FDD and pure UL slots). Mixed slots are FFT'd whole, so their
   * leading DL/guard symbols hold the gNB's own TX leakage; the indication
   * forwards this mask so consumers can discard them. Process-local only —
   * NOT part of the shm layout. */
  uint16_t valid_symbol_mask;
  bool valid; /* false until the first push completes */
} e3_ran_buffers_slot_info_t;

/* ---- Lifecycle ---- */

/**
 * Create the /e3_ran_buffers shm region sized from the PHY dimensions.
 * Idempotent. Permissions are 0644 so a dApp in another container can mmap it
 * read-only. Returns 0 on success, -1 on failure (errno set).
 */
int e3_ran_buffers_init_from_phy(const PHY_VARS_gNB *gNB);

/**
 * Destroy the shm region (munmap, close, unlink). Idempotent. Also wakes any
 * worker blocked in e3_ran_buffers_wait_for_publish().
 */
void e3_ran_buffers_destroy(void);

/* ---- Producer ---- */

/**
 * Convert this slot's PHY rxdataF into the correct FH layout, store it at the next
 * buffer slot, and publish it. Called once per UL slot from the PHY RX thread,
 * only when a dApp is subscribed.
 */
void e3_ran_buffers_push_rxdataF(const PHY_VARS_gNB *gNB, int frame_rx, int slot_rx);

/* ---- Consumer-side accessors ---- */

/**
 * Block until a newer slot than caller_sequence is published, or the timeout
 * fires, or shutdown wakes us. Updates caller_sequence to the current value and
 * fills out_snapshot (if given) with the latest slot. timeout_ns: 0 = poll,
 * UINT64_MAX = wait forever. Returns true if a publish (or shutdown) happened.
 */
bool e3_ran_buffers_wait_for_publish(uint64_t timeout_ns, uint64_t *caller_sequence, e3_ran_buffers_slot_info_t *out_snapshot);

/**
 * Wake any thread blocked in e3_ran_buffers_wait_for_publish() (e.g. at
 * shutdown). The waiter returns once and must re-check its own running flag.
 */
void e3_ran_buffers_signal_shutdown(void);

#endif /* E3_RAN_BUFFERS_H */
