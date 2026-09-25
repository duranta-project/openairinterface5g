/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**
 * @file e3_ran_buffers.c
 * @brief Writer for the cuBB-compatible /e3_ran_buffers shared memory.
 *
 * See e3_ran_buffers.h for the design overview.
 */

#include "e3_ran_buffers.h"
#include "../../e3_log.h"
#include "common/utils/LOG/log.h"
#include "openair2/E3AP/service_models/e3_shm_region.h"
#include "openair2/E3AP/service_models/pub_channel.h"
#include <inttypes.h> /* PRIu64 */

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(__AVX2__)
#include <immintrin.h> /* SIMD int16 -> FP16 spans in the rxdataF copy */
#endif

/* Buffer shape [ant][sym][prb][sc]. The writer checks
 * PHY's runtime PRB count fits within E3_RB_N_PRBS_LAYOUT and zero-fills the
 * rest, so a dApp always gets a buffer of this shape. */
#define E3_RB_N_ANTS 4
#define E3_RB_N_SYMBOLS 14
#define E3_RB_N_PRBS_LAYOUT 273
#define E3_RB_N_SC_PER_PRB 12
#define E3_RB_N_SC_PER_SLOT (E3_RB_N_PRBS_LAYOUT * E3_RB_N_SC_PER_PRB) /* 3276 */

/* Scale applied to PHY int16 IQ before the FP16 conversion below. It is a
 * per-front-end calibration constant, hence selected with the radio the build
 * targets:
 *
 *  - 1/2048 is cuPHY/cuBB's fixed-point baseline. dApps rescale by 1/beta, so
 *    keeping it makes an OAI capture magnitude-compatible with an Aerial one;
 *    it is the right value for Aerial and 7.2-split front ends.
 *  - USRP front ends deliver a much smaller rxdataF: at 1/2048 the thermal
 *    floor computes to ~-132 dBFS, 22 dB below the -110 dBFS display floor the
 *    dApp dashboard is calibrated for, so idle slots render as black rows.
 *    1/128 (+24 dB) puts it at ~-108 dBFS, just inside that window.
 *
 * Set from CMake (E3_FP16_BETA_USRP when OAI_USRP is on). A build carrying
 * several device libraries (e.g. -w USRP -w SIMU) takes the USRP value. */
#ifdef E3_FP16_BETA_USRP
#define E3_RB_FP16_BETA (1.0f / 128.0f)
#else
#define E3_RB_FP16_BETA (1.0f / 2048.0f)
#endif

/* Bytes per IQ pair on the wire: 2× FP16. Stored as a uint16 pair. */
#define E3_RB_BYTES_PER_SAMPLE (2u * sizeof(uint16_t))

/* Slot footprint = ANTS * SYMBOLS * SC_PER_SLOT * (FP16,FP16) */
#define E3_RB_BYTES_PER_SLOT ((size_t)E3_RB_N_ANTS * E3_RB_N_SYMBOLS * E3_RB_N_SC_PER_SLOT * E3_RB_BYTES_PER_SAMPLE)

/* Two buffers of 16 rows each. The reader gets (buffer, row) in an indication
 * and mmaps that row while the producer keeps writing newer slots; the depth
 * gives ~64 ms before the producer wraps onto a row a reader may still hold.
 */
#define E3_RB_NUM_FH_ROWS 16
#define E3_RB_NUM_BUFFERS 2

/* PUSCH and HEST regions exist for cuBB-header parity but the producer never
 * writes them (dApps subscribe only to IQ/sfn/slot/timestamp). Kept small. */
#define E3_RB_PUSCH_ROW_BYTES 80000u
#define E3_RB_NUM_PUSCH_ROWS 1u
#define E3_RB_HEST_SAMPLES 1024u
#define E3_RB_NUM_HEST_ROWS 1u
#define E3_RB_BYTES_PER_HEST 8u /* mirror cuFloatComplex on cuBB side */

/* ---- IEEE 754 FP32->FP16 conversion (round-to-nearest-even) ----
 * Use the hardware cast where _Float16 is available, otherwise a pure-C
 * fallback so the build doesn't depend on a specific compiler. The FP16 bit
 * pattern is what crosses the wire; the dApp casts it back to FP32. */
#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
static inline uint16_t fp32_to_fp16_rne(float f)
{
  _Float16 h = (_Float16)f;
  uint16_t out;
  memcpy(&out, &h, sizeof(out));
  return out;
}
#else
static inline uint16_t fp32_to_fp16_rne(float f)
{
  union {
    float f;
    uint32_t u;
  } x = {f};
  const uint32_t b = x.u;
  const uint32_t sign = (b >> 16) & 0x8000u;
  const int32_t exp_f32 = (int32_t)((b >> 23) & 0xFFu) - 127;
  const uint32_t mant_f32 = b & 0x7FFFFFu;

  /* NaN / Inf */
  if (exp_f32 == 128) {
    return (uint16_t)(sign | 0x7C00u | (mant_f32 ? (mant_f32 >> 13) | 0x200u : 0u));
  }
  /* Overflow → ±Inf */
  if (exp_f32 >= 16) {
    return (uint16_t)(sign | 0x7C00u);
  }
  /* Subnormal / underflow → 0 (or denormal) */
  if (exp_f32 <= -15) {
    if (exp_f32 < -24)
      return (uint16_t)sign;
    uint32_t mant = (mant_f32 | 0x800000u) >> (-exp_f32 - 14);
    uint32_t round_bit = (mant_f32 | 0x800000u) >> (-exp_f32 - 15);
    mant += round_bit & 1u;
    return (uint16_t)(sign | (mant & 0x3FFu));
  }
  /* Normal */
  const uint16_t exp16 = (uint16_t)(exp_f32 + 15);
  const uint16_t mant16 = (uint16_t)(mant_f32 >> 13);
  const uint32_t low = mant_f32 & 0x1FFFu;
  uint16_t rounded = mant16;
  if (low > 0x1000u || (low == 0x1000u && (mant16 & 1u))) {
    rounded++;
  }
  uint16_t result = (uint16_t)(sign | (exp16 << 10) | (rounded & 0x3FFu));
  if (rounded == 0x400u) {
    result = (uint16_t)(sign | ((uint16_t)(exp16 + 1) << 10));
  }
  return result;
}
#endif

#if !defined(__F16C__) && !defined(__aarch64__)
/* No hardware fp32->fp16 on this target: use a full int16->fp16 lookup
 * table for the configured beta (128 KiB, built once at init). */
static uint16_t g_fp16_table[65536];
static float g_fp16_table_beta;
static bool g_fp16_table_ready;

static void fp16_table_build(float beta)
{
  for (uint32_t v = 0; v < 65536u; ++v) {
    g_fp16_table[v] = fp32_to_fp16_rne((float)(int16_t)v * beta);
  }
  g_fp16_table_beta = beta;
  g_fp16_table_ready = true;
}
#endif

/* dst[i] = fp16_rne((float)src[i] * beta). Each path hands its tail to the
 * next; all paths are bit-identical under the default round-to-nearest-even
 * FP mode. */
static void convert_span_fp16(uint16_t *dst, const int16_t *src, size_t n, float beta)
{
  size_t i = 0;
#if defined(__AVX2__) && defined(__F16C__)
  const __m256 beta_vec = _mm256_set1_ps(beta);
  for (; i + 8 <= n; i += 8) {
    const __m256i widened = _mm256_cvtepi16_epi32(_mm_loadu_si128((const __m128i *)(src + i)));
    const __m256 scaled = _mm256_mul_ps(_mm256_cvtepi32_ps(widened), beta_vec);
    _mm_storeu_si128((__m128i *)(dst + i), _mm256_cvtps_ph(scaled, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
  }
#endif
#if !defined(__F16C__) && !defined(__aarch64__)
  if (g_fp16_table_ready && beta == g_fp16_table_beta) {
    for (; i < n; ++i) {
      dst[i] = g_fp16_table[(uint16_t)src[i]];
    }
    return;
  }
#endif
  for (; i < n; ++i) {
    dst[i] = fp32_to_fp16_rne((float)src[i] * beta);
  }
}

/* ---- Module state ---- */

static const e3_shm_region_desc_t g_rb_desc = {
    .name = E3_RB_SHM_NAME,
    .log_tag = "E3RB",
    .size = sizeof(e3_ran_buffers_header_t) + 2 * (E3_RB_BYTES_PER_SLOT * E3_RB_NUM_FH_ROWS)
            + 2 * ((size_t)E3_RB_PUSCH_ROW_BYTES * E3_RB_NUM_PUSCH_ROWS)
            + 2 * ((size_t)E3_RB_HEST_SAMPLES * E3_RB_NUM_HEST_ROWS * E3_RB_BYTES_PER_HEST),
};

static struct {
  e3_shm_region_t region;
  uint8_t *fh_buf[E3_RB_NUM_BUFFERS]; /* pointer to each FH buffer's row 0 */
  e3_ran_buffers_header_t *header;

  /* Cursor of the slot currently being written; the publish channel's
   * sequence is bumped after each full slot write so readers see a
   * monotonic publish marker. */
  e3_shm_cursor_t fh_cursor; /* .buffer = FH buffer (0/1), .row = row within it */

  /* Latest published slot info — copied under the publish channel lock. */
  e3_ran_buffers_slot_info_t latest;
} g_state = {
    .region = {.fd = -1},
};

static atomic_bool g_ready = ATOMIC_VAR_INIT(false);

/* Publishes each completed FH slot write to the SM worker thread.
 * See openair2/E3AP/service_models/pub_channel.h. */
static pub_channel_t g_fh_chan = PUB_CHANNEL_INIT;

/* ---- Lifecycle ---- */

int e3_ran_buffers_init_from_phy(const PHY_VARS_gNB *gNB)
{
  if (atomic_load(&g_ready)) {
    return 0; /* already initialized */
  }
  if (!gNB) {
    E3RB_LOG_E("init: gNB is NULL\n");
    return -1;
  }

  const NR_DL_FRAME_PARMS *frame_parms = &gNB->frame_parms;
  if (frame_parms->N_RB_UL > E3_RB_N_PRBS_LAYOUT) {
    E3RB_LOG_E("init: PHY has %u PRBs which exceeds aerial layout cap %u\n",
               (unsigned)frame_parms->N_RB_UL,
               (unsigned)E3_RB_N_PRBS_LAYOUT);
    return -1;
  }

  if (e3_shm_region_create(&g_state.region, &g_rb_desc) != 0)
    return -1;

  const size_t fh_buffer_size = E3_RB_BYTES_PER_SLOT * E3_RB_NUM_FH_ROWS;
  const size_t pusch_buffer_sz = (size_t)E3_RB_PUSCH_ROW_BYTES * E3_RB_NUM_PUSCH_ROWS;
  const size_t hest_buffer_sz = (size_t)E3_RB_HEST_SAMPLES * E3_RB_NUM_HEST_ROWS * E3_RB_BYTES_PER_HEST;

  /* Populate header. The wire-visible struct must match cuBB byte-for-byte. */
  e3_ran_buffers_header_t *header = (e3_ran_buffers_header_t *)g_state.region.base;
  memset(header, 0, sizeof(*header));
  header->version = 1;
  header->fh_buffer_size = (uint32_t)fh_buffer_size;
  header->pusch_buffer_size = (uint32_t)pusch_buffer_sz;
  header->hest_buffer_size = (uint32_t)hest_buffer_sz;
  header->num_fh_samples = (uint32_t)(E3_RB_BYTES_PER_SLOT / sizeof(uint16_t));
  header->num_fh_rows = E3_RB_NUM_FH_ROWS;
  header->num_pusch_rows = E3_RB_NUM_PUSCH_ROWS;
  header->num_hest_rows = E3_RB_NUM_HEST_ROWS;
  header->max_hest_samples_per_row = E3_RB_HEST_SAMPLES;

  uint8_t *cursor = (uint8_t *)(header + 1);
  g_state.fh_buf[0] = cursor;
  cursor += fh_buffer_size;
  g_state.fh_buf[1] = cursor;
  cursor += fh_buffer_size;
  (void)cursor;

  g_state.header = header;
  g_state.fh_cursor = (e3_shm_cursor_t)E3_SHM_CURSOR_INIT(E3_RB_NUM_FH_ROWS, E3_RB_NUM_BUFFERS);
  g_fh_chan.publish_sequence = 0;
  memset(&g_state.latest, 0, sizeof(g_state.latest));

#if !defined(__F16C__) && !defined(__aarch64__)
  fp16_table_build(E3_RB_FP16_BETA);
#endif

  atomic_store(&g_ready, true);

  E3RB_LOG_I(
      "shm '%s' ready: total=%zu B (FH %zu×2, PUSCH %zu×2, HEST %zu×2), "
      "layout [ant=%d][sym=%d][prb=%d][sc=%d] FP16, %d rows/buf\n",
      E3_RB_SHM_NAME,
      g_state.region.size,
      fh_buffer_size,
      pusch_buffer_sz,
      hest_buffer_sz,
      E3_RB_N_ANTS,
      E3_RB_N_SYMBOLS,
      E3_RB_N_PRBS_LAYOUT,
      E3_RB_N_SC_PER_PRB,
      E3_RB_NUM_FH_ROWS);
  return 0;
}

void e3_ran_buffers_destroy(void)
{
  if (!atomic_load(&g_ready))
    return;
  atomic_store(&g_ready, false);

  e3_ran_buffers_signal_shutdown();

  e3_shm_region_destroy(&g_state.region);
  g_state.header = NULL;
  E3RB_LOG_I("shm '%s' destroyed\n", E3_RB_SHM_NAME);
}

/* ---- Producer ---- */

void e3_ran_buffers_push_rxdataF(const PHY_VARS_gNB *gNB, int frame_rx, int slot_rx)
{
  if (!atomic_load(&g_ready)) {
    /* Lazy init on first call. */
    if (e3_ran_buffers_init_from_phy(gNB) != 0)
      return;
  }
  if (!gNB)
    return;

  const NR_DL_FRAME_PARMS *frame_parms = &gNB->frame_parms;
  const NR_gNB_COMMON *common_vars = &gNB->common_vars;

  const uint32_t n_rx_ant = frame_parms->nb_antennas_rx;
  const uint32_t n_prbs = frame_parms->N_RB_UL;
  const uint32_t fft_size = frame_parms->ofdm_symbol_size;
  const uint32_t k0 = frame_parms->first_carrier_offset;

  uint32_t ants = n_rx_ant;
  if (ants > (uint32_t)E3_RB_N_ANTS) {
    static int warned = 0;
    if (!warned) {
      E3RB_LOG_W("PHY has %u rx antennas; layout caps at %d (truncating)\n", (unsigned)ants, E3_RB_N_ANTS);
      warned = 1;
    }
    ants = E3_RB_N_ANTS;
  }

  const float beta = E3_RB_FP16_BETA;
  const uint8_t buffer_index = g_state.fh_cursor.buffer;
  const uint32_t row_index = g_state.fh_cursor.row;
  uint8_t *const row_base = g_state.fh_buf[buffer_index] + (size_t)row_index * E3_RB_BYTES_PER_SLOT;

  /* rxdataF is a ring of RU_RX_SLOT_DEPTH slot buckets, allocated in
   * nr_init_ru.c and aliased into gNB->common_vars by init_eNB_afterRU(). Both
   * producers bucket into it identically -- the RU FFT with
   * slot % RU_RX_SLOT_DEPTH, the split-7.2 fronthaul with an open-coded
   * 3 & slot -- and rx_func() derives the same offset for its own consumers.
   * Without it we would always read bucket 0. */
  const uint32_t slot_offset = (uint32_t)(slot_rx % RU_RX_SLOT_DEPTH) * frame_parms->symbols_per_slot * fft_size;

  /* Write [ant][sym][prb][sc][I,Q] FP16. */
  const uint32_t total_sc = n_prbs * E3_RB_N_SC_PER_PRB;
  const uint32_t span1_sc = (k0 + total_sc <= fft_size) ? total_sc : fft_size - k0;
  const uint32_t span2_sc = total_sc - span1_sc;
  for (uint32_t antenna = 0; antenna < ants; ++antenna) {
    const c16_t *ant_data = common_vars->rxdataF[antenna] + slot_offset;
    for (uint32_t symbol = 0; symbol < (uint32_t)E3_RB_N_SYMBOLS; ++symbol) {
      const c16_t *sym_data = ant_data + (size_t)symbol * fft_size;
      uint16_t *out_iq =
          (uint16_t *)(row_base + (((size_t)antenna * E3_RB_N_SYMBOLS + symbol) * E3_RB_N_SC_PER_SLOT) * E3_RB_BYTES_PER_SAMPLE);
      convert_span_fp16(out_iq, (const int16_t *)(sym_data + k0), 2u * span1_sc, beta);
      convert_span_fp16(out_iq + 2u * span1_sc, (const int16_t *)sym_data, 2u * span2_sc, beta);
      /* Zero-pad remaining PRB columns if PHY has fewer than the layout. */
      memset(out_iq + 2u * total_sc, 0, ((size_t)E3_RB_N_SC_PER_SLOT - total_sc) * E3_RB_BYTES_PER_SAMPLE);
    }
  }

  /* Per-symbol UL validity from the TDD config (same indexing as
   * nr_slot_select); FDD = whole slot. Rides in the indication only, the
   * shm layout is untouched. */
  uint16_t valid_symbol_mask = 0x3FFF;
  if (frame_parms->frame_type == TDD) {
    const nfapi_nr_config_request_scf_t *cfg = &gNB->gNB_config;
    valid_symbol_mask = 0;
    for (int s = 0; s < E3_RB_N_SYMBOLS; s++) {
      if (cfg->tdd_table.max_tdd_periodicity_list[slot_rx].max_num_of_symbol_per_slot_list[s].slot_config.value == 1)
        valid_symbol_mask |= (uint16_t)(1u << s);
    }
  }

  /* Publish timestamp, taken just before flipping the sequence so it marks
   * when a reader could first see the slot. */
  const uint64_t producer_ts_ns = pub_channel_now_ns();

  /* Build the slot-info snapshot the reader consumes. */
  e3_ran_buffers_slot_info_t info = {
      .fh_buffer_index = buffer_index,
      .fh_write_index = row_index,
      .pusch_buffer_index = 0,
      .pusch_write_index = 0,
      .hest_buffer_index = 0,
      .hest_write_index = 0,
      .hest_data_size = 0,
      .timestamp_ns = producer_ts_ns,
      .sfn = (uint16_t)frame_rx,
      .slot = (uint16_t)slot_rx,
      .cell_id = 0,
      .n_rx_ant = (uint16_t)ants,
      .valid_symbol_mask = valid_symbol_mask,
      .valid = true,
  };

  pub_channel_lock(&g_fh_chan);
  g_state.latest = info;
  e3_shm_cursor_advance(&g_state.fh_cursor);
  pub_channel_publish_and_wake(&g_fh_chan);
  pub_channel_unlock(&g_fh_chan);
}

bool e3_ran_buffers_wait_for_publish(uint64_t timeout_ns, uint64_t *caller_sequence, e3_ran_buffers_slot_info_t *out_snapshot)
{
  return pub_channel_wait(&g_fh_chan, &g_state.latest, out_snapshot, sizeof(*out_snapshot), timeout_ns, caller_sequence);
}

void e3_ran_buffers_signal_shutdown(void)
{
  pub_channel_signal_shutdown(&g_fh_chan);
}
