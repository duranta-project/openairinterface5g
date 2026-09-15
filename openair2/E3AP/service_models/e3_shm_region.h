/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**
 * @file e3_shm_region.h
 * @brief Shared POSIX-shm lifecycle for the E3 service models.
 *
 * One utility owns the segment lifecycle every SM shm region repeats:
 * drop-stale + shm_open + fchmod + ftruncate + mmap on create, and
 * munmap + close + shm_unlink on destroy. Each SM keeps its own header for
 * the layout specifics (header struct, region carving, payload format) and
 * parameterizes this utility with a descriptor.
 *
 * The row cursor below is the shared write-index handling: a region is
 * n_buffers x rows_per_buffer rows; the producer takes the current (buffer,
 * row), writes, and advances, flipping buffers on wrap. A plain ring is the
 * n_buffers == 1 case.
 */
#ifndef OPENAIR2_E3AP_SERVICE_MODELS_E3_SHM_REGION_H
#define OPENAIR2_E3AP_SERVICE_MODELS_E3_SHM_REGION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  const char *name; /* segment name, e.g. "/e3_ran_buffers" */
  const char *log_tag; /* short tag for log lines, e.g. "E3RB" */
  size_t size; /* total mapping size in bytes */
} e3_shm_region_desc_t;

typedef struct {
  int fd;
  void *base; /* NULL when not mapped */
  size_t size;
  const e3_shm_region_desc_t *desc;
} e3_shm_region_t;

/* Create (or recreate) the segment: unlink any stale copy from a previous
 * crash, open O_CREAT|O_RDWR 0644 (fchmod defeats umask so a dApp in another
 * container can mmap O_RDONLY), size it and map it read-write. Returns 0 and
 * fills *region on success, -1 on failure (logged; nothing left behind).
 * Content is NOT zeroed; the caller initializes its own header/layout. */
int e3_shm_region_create(e3_shm_region_t *region, const e3_shm_region_desc_t *desc);

/* Unmap, close and unlink. Safe on a never-created/already-destroyed region. */
void e3_shm_region_destroy(e3_shm_region_t *region);

/* ---- Shared write-index handling ---- */

typedef struct {
  uint32_t row; /* next row to write within the current buffer */
  uint8_t buffer; /* current buffer index */
  uint32_t rows_per_buffer;
  uint8_t n_buffers; /* 1 = plain ring */
} e3_shm_cursor_t;

#define E3_SHM_CURSOR_INIT(rows, bufs)                                    \
  {                                                                       \
    .row = 0, .buffer = 0, .rows_per_buffer = (rows), .n_buffers = (bufs) \
  }

/* Move past the row just written; flip to the next buffer on wrap. */
static inline void e3_shm_cursor_advance(e3_shm_cursor_t *c)
{
  if (++c->row >= c->rows_per_buffer) {
    c->row = 0;
    c->buffer = (uint8_t)((c->buffer + 1u) % c->n_buffers);
  }
}

#endif /* OPENAIR2_E3AP_SERVICE_MODELS_E3_SHM_REGION_H */
