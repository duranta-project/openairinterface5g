/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**
 * @file spectrum_sensing_ring.c
 * @brief SM-owned /e3_l2_sensing POSIX shm ring for MAC sensing ranges.
 *
 * See spectrum_sensing_ring.h for the design overview.
 */
#include "spectrum_sensing_ring.h"
#include "../../e3_log.h"
#include "common/utils/LOG/log.h"
#include "openair2/E3AP/service_models/e3_shm_region.h"
#include "openair2/E3AP/service_models/pub_channel.h" /* pub_channel_now_ns */

#include <stdatomic.h>
#include <string.h>

/* ---- Module state (single producer) ---------------------------------- */

static const e3_shm_region_desc_t g_ring_desc = {
    .name = SPECTRUM_SENSING_RING_SHM_NAME,
    .log_tag = "SPECTRUM-SM",
    .size = sizeof(spectrum_sensing_ring_header_t) + (size_t)SPECTRUM_SENSING_RING_SLOTS * sizeof(spectrum_sensing_ring_slot_t),
};

static struct {
  e3_shm_region_t region;
  spectrum_sensing_ring_header_t *hdr;
  spectrum_sensing_ring_slot_t *ring; /* base[sizeof(header) ..] */
  e3_shm_cursor_t cursor; /* next slot to write (worker-thread-only) */
  uint32_t seq; /* monotonic write counter */
} g_st = {
    .region = {.fd = -1},
};

static atomic_bool g_ready = ATOMIC_VAR_INIT(false);

/* ---- Lifecycle ---- */

int spectrum_sensing_ring_init(void)
{
  if (atomic_load(&g_ready)) {
    return 0; /* already up */
  }

  if (e3_shm_region_create(&g_st.region, &g_ring_desc) != 0)
    return -1;

  spectrum_sensing_ring_header_t *hdr = (spectrum_sensing_ring_header_t *)g_st.region.base;
  memset(g_st.region.base, 0, g_st.region.size);
  hdr->version = 1;
  hdr->slot_count = SPECTRUM_SENSING_RING_SLOTS;
  hdr->slot_stride = (uint32_t)sizeof(spectrum_sensing_ring_slot_t);
  hdr->max_ranges = MAX_SENSING_RANGES;
  hdr->range_size = (uint32_t)sizeof(sensing_range_t);

  g_st.hdr = hdr;
  g_st.ring = (spectrum_sensing_ring_slot_t *)(hdr + 1);
  g_st.cursor = (e3_shm_cursor_t)E3_SHM_CURSOR_INIT(SPECTRUM_SENSING_RING_SLOTS, 1);
  g_st.seq = 0;

  atomic_store(&g_ready, true);

  SPEC_LOG_I("shm '%s' ready: total=%zu B (%u slots × %zu B), max_ranges=%d\n",
             SPECTRUM_SENSING_RING_SHM_NAME,
             g_st.region.size,
             SPECTRUM_SENSING_RING_SLOTS,
             sizeof(spectrum_sensing_ring_slot_t),
             MAX_SENSING_RANGES);
  return 0;
}

void spectrum_sensing_ring_destroy(void)
{
  if (!atomic_load(&g_ready))
    return;
  atomic_store(&g_ready, false);

  e3_shm_region_destroy(&g_st.region);
  g_st.hdr = NULL;
  g_st.ring = NULL;
  SPEC_LOG_I("shm '%s' destroyed\n", SPECTRUM_SENSING_RING_SHM_NAME);
}

/* ---- Producer ---- */

int spectrum_sensing_ring_write(const nr_mac_sensing_publish_meta_t *meta,
                                const sensing_range_t *ranges,
                                uint8_t n_ranges,
                                uint32_t *out_write_idx)
{
  if (!meta || !out_write_idx)
    return -1;
  if (!atomic_load(&g_ready)) {
    if (spectrum_sensing_ring_init() != 0)
      return -1; /* lazy init */
  }

  uint8_t n = n_ranges;
  if (n > MAX_SENSING_RANGES)
    n = MAX_SENSING_RANGES;

  const uint32_t idx = g_st.cursor.row;
  spectrum_sensing_ring_slot_t *s = &g_st.ring[idx];

  s->beam = meta->beam;
  s->timestamp_ns = (meta->timestamp_ns != 0) ? meta->timestamp_ns : pub_channel_now_ns();
  s->seq = ++g_st.seq;
  s->_pad = 0;
  s->n_ranges = n;
  if (n > 0 && ranges) {
    memcpy(s->ranges, ranges, (size_t)n * sizeof(sensing_range_t));
  }
  /* Stamp the self-describing (sfn, slot) last: it is the field the dApp
   * matches against the indication before trusting the ranges, so writing
   * it after the payload narrows the wrap-overwrite torn-read window. */
  s->sfn = meta->frame;
  s->slot = meta->slot;

  e3_shm_cursor_advance(&g_st.cursor);

  *out_write_idx = idx;
  return 0;
}
