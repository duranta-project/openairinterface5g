/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "common/platform_types.h"
#include "xran_pkt_api.h"
#include "xran_pkt_bfw.h"
#include "oru_packet_processor.h"
#include "oru_pcap.h"
#include <rte_byteorder.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "assertions.h"
#include "log.h"
#include <rte_ring.h>
#include "common/utils/nr/nr_common.h"
#include <sys/types.h>
#include <stdatomic.h>

#define PRACH_ERR_LOG_RATELIMIT 10000

#define RATELIMIT(n, block)                                                               \
  do {                                                                                    \
    static _Atomic unsigned long counter = 0;                                             \
    unsigned long current = atomic_fetch_add_explicit(&counter, 1, memory_order_relaxed); \
    if (current % (n) == 0) {                                                             \
      block                                                                               \
    }                                                                                     \
  } while (0)

#define DL_JOB_RING_SIZE 1024
#define UL_JOB_RING_SIZE 1024
#define MAX_CONCURRENT_DL_JOBS (DL_JOB_RING_SIZE - 1)
#define NUM_CONCURRENT_DL_SYMBOL_WINDOWS MAX_CONCURRENT_DL_JOBS
#define NUM_CONCURRENT_UL_SYMBOL_WINDOWS 128
#define MAX_ANTENNAS 4
#define NUM_RU_PORT_IDS 16 // The eAxC layout allocates four bits to the RU port.
#define NR_NUMBER_OF_SUBFRAMES_PER_FRAME 10
#define MAX_TDD_PATTERN_LENGTH_MS 10
#define MAX_SLOTS_PER_MS 4
#define SYMBOL_BITMASK_SIZE ((NR_SYMBOLS_PER_SLOT * MAX_TDD_PATTERN_LENGTH_MS * MAX_SLOTS_PER_MS + 7) / 8)
#define MAX_SECTIONS_PER_DL_STREAM 4 // C-Plane sections (e.g. split PRB ranges) that can share one (eaxc, beam) stream in a DL symbol
#define MAX_MBUFS_PER_SYMBOL 64
#define MAX_SLOTS_PER_FRAME 160
#define MAX_PRACH_SECTIONS 4 // PRACH C-Plane sections (FDM occasions) per slot and PRACH stream
#define XRAN_IQ_BITS_UNCOMPRESSED 16 /* xRAN table 7.7.1.1-1: udIqWidth=0 means 16-bit samples */
#define NUM_SECTION_IDS 4096 // 12-bit sectionId
#define SECTION_ID_UNCOUPLED 4095 // O-RAN CUS 7.5.3.1: sent when C-Plane and U-Plane aren't coupled via sectionId
#define ORU_BFW_SLOTS 4096 // weight vectors held at once: the current ones of every beamId, plus superseded ones in flight
#define ORU_BFW_NUM_BEAM_IDS (1 << 15)
#define ORU_BFW_NO_SLOT 0xFFFF
#define ORU_BFW_RING_SIZE (1 << 18) // bundle refs: a few slots of SE11 with one-PRB bundles on 4 eAxCs

// One weight vector. gen changes on every rewrite and is 0 while one is in progress.
typedef struct {
  _Atomic(uint32_t) gen;
  c16_t w[ORU_MAX_BF_WEIGHTS];
} oru_bfw_slot_t;

// One PRB bundle's beam: its beamId and the weights it had when the section arrived. gen 0: no dynamic weights.
typedef struct {
  uint16_t beam_id;
  uint16_t slot;
  uint32_t gen;
} oru_bfw_ref_t;

// One (eaxc, beam_id) pair's bookkeeping for a DL symbol: completion tracking and declared
// section_ids, no IQ. Payload fragments live in dl_symbol_job_t.fragments[] instead.
typedef struct {
  uint8_t eaxc_id;
  uint16_t beam_id;
  uint16_t section_ids[MAX_SECTIONS_PER_DL_STREAM]; // every C-Plane section_id declared into this stream
  oru_bfw_set_t section_bfw[MAX_SECTIONS_PER_DL_STREAM]; // dynamic weights of each of those sections
  int num_section_ids;
  int expected_iq; // PRBs declared via C-Plane for this stream, summed across its section_ids
  int received_iq; // PRBs actually received via U-Plane for this stream
} dl_stream_slot_t;

// One joined C-Plane+U-Plane PRB run. comp_method/iq_width come from the U-Plane packet itself,
// not the C-Plane declaration - sections in the same symbol aren't guaranteed the same compression.
typedef struct {
  uint8_t eaxc_id;
  uint16_t beam_id;
  int start_prbc;
  int num_prbc;
  fh_comp_method_t comp_method;
  uint8_t iq_width;
  oru_bfw_set_t bfw;
  void *iq_data; // points into mbuf; valid until this fragment is unpacked and mbuf is freed
  void *mbuf;
} dl_fragment_t;

typedef struct {
  dl_stream_slot_t streams[MAX_DL_STREAMS_PER_SYMBOL];
  int num_streams;
  dl_fragment_t fragments[MAX_DL_FRAGMENTS_PER_SYMBOL];
  int num_fragments;
  int num_fragment_prbs; // PRBs across fragments[], bounded by DL_IQ_ARENA_PRBS() for read_dl_iq_streams()
  uint64_t absolute_symbol;
} dl_symbol_job_t;

typedef struct {
  bool active;
  uint64_t start_absolute_symbol;
  uint32_t num_symbols;
  int section_id;
  int num_prb;
  int start_prb;
  int filter_id;
  uint16_t beam_id; // section type 3 beamId: the PRACH beam for this stream (UL Rx beamforming)
  oru_bfw_set_t bfw; // dynamic weights of this section
  fh_comp_method_t comp_method;
  uint8_t iq_width;
  int occasion; // order among the sections starting at the same symbol
  bool served; // write_prach_iq() sent U-Plane for it
  _Atomic(uint32_t) seq; // odd while the C-Plane thread rewrites the job, for get_prach_bfw_set()
} prach_job_t;
typedef struct {
  _Atomic(uint64_t) dl_tdd_mismatch;
  _Atomic(uint64_t) ul_tdd_mismatch;
  _Atomic(uint64_t) ul_cplane_missing;
  _Atomic(uint64_t) prach_cplane_missing;
  _Atomic(uint64_t) prach_cplane_missing_ant;
  _Atomic(uint64_t) prach_cplane_missing_inactive;
  _Atomic(uint64_t) prach_cplane_missing_stale;
  _Atomic(uint64_t) prach_cplane_missing_early;
  _Atomic(uint64_t) prach_out_of_mbufs;
  _Atomic(uint64_t) prach_jobs_pool_exhausted;
  _Atomic(uint64_t) out_of_mbufs;
  _Atomic(uint64_t) total_uplane_sent;
  _Atomic(int64_t) ul_uplane_ota_delay_sum;
  _Atomic(uint64_t) ul_uplane_ota_delay_count;
  _Atomic(uint64_t) bfw_stale;
} thread_safe_stats_t;

typedef struct {
  dl_symbol_job_t dl_symbol_jobs[MAX_CONCURRENT_DL_JOBS];
  dl_symbol_job_t *dl_symbol_rx_window[NUM_CONCURRENT_DL_SYMBOL_WINDOWS];
  bool was_dl_symbol_completed[NUM_CONCURRENT_DL_SYMBOL_WINDOWS];
  prach_job_t prach_jobs[MAX_SLOTS_PER_FRAME][MAX_ANTENNAS][MAX_PRACH_SECTIONS];
  uint64_t current_absolute_symbol;
  uint64_t window_tail_symbol;
  uint64_t cplane_closed_tail_symbol;
  struct rte_ring *dl_free_jobs;
  struct rte_ring *dl_ready_jobs;
  struct rte_ring *ul_free_jobs;
  struct rte_ring *ul_ready_jobs;
  ul_job_t ul_jobs_pool[UL_JOB_RING_SIZE];
  uint32_t T2a_min_cp_sym_diff;
  uint32_t T2a_max_cp_sym_diff;
  uint32_t T2a_min_up_dl_sym_diff;
  uint32_t T2a_max_up_dl_sym_diff;
  struct xran_eaxcid_config eaxcid_config;
  int prach_eaxc_offset;
  int prach_kbar;
  int numerology;
  int num_prb;
  oru_packet_processor_stats_t stats;
  thread_safe_stats_t thread_safe_stats;
  uint8_t dl_symbol_bitmask[SYMBOL_BITMASK_SIZE];
  uint8_t ul_symbol_bitmask[SYMBOL_BITMASK_SIZE];
  uint16_t symbol_bitmask_length;
  alloc_func_t alloc_func;
  send_func_t send_func;
  void *io_controller;
  // O-RAN CUS, 5.1.3.2.8: sequence IDs advance per U-plane UL eAxC, not per channel type.
  _Atomic(uint8_t) ul_seq_id[NUM_RU_PORT_IDS];
  size_t mtu;
  fh_comp_method_t dl_comp_method;
  int num_bf_weights;
  bool bfw_decode_warned; // logged the first ext1/ext11 whose weights didn't decode
  // ext11 decode output, sized at init for ORU_MAX_BF_WEIGHTS
  int ext11_max_bundles;
  uint16_t *ext11_beam_ids;
  c16_t *ext11_weights;
  // Dynamic beamforming weights (CUS 12.4.3), written only by handle_cplane_packet().
  // A beamId keeps its weights until new ones arrive for it. Superseded slots are
  // queued FIFO for reuse, so a section that cited them can still read them
  // get_bf_weights() detects the reuse through the slot gen.
  oru_bfw_slot_t *bfw_slots;
  uint16_t *bfw_beam_slot;
  uint16_t *bfw_free; // FIFO of ORU_BFW_SLOTS
  uint32_t bfw_free_head;
  uint32_t bfw_free_count;
  uint32_t bfw_gen;
  oru_bfw_ref_t *bfw_ring;
  _Atomic(uint32_t) bfw_ring_reserve;
} oru_packet_processor_context_t;

static inline void set_bit(uint8_t *bits, uint64_t bit)
{
  bits[bit / 8] |= 1 << (bit % 8);
}

static inline int test_bit(uint8_t *bits, uint64_t bit)
{
  return bits[bit / 8] & (1 << (bit % 8));
}

// U-Plane packets carry (eaxc, section_id) - find which beam-stream declared that section_id.
static dl_stream_slot_t *find_dl_stream_by_section(dl_symbol_job_t *job, uint8_t eaxc_id, uint16_t section_id, oru_bfw_set_t *bfw)
{
  for (int s = 0; s < job->num_streams; s++) {
    dl_stream_slot_t *stream = &job->streams[s];
    if (stream->eaxc_id != eaxc_id) {
      continue;
    }
    for (int i = 0; i < stream->num_section_ids; i++) {
      if (stream->section_ids[i] == section_id) {
        if (bfw)
          *bfw = stream->section_bfw[i];
        return stream;
      }
    }
  }
  return NULL;
}

// Only C-Plane creates stream slots; a U-Plane packet with no match counts as
// uplane_missing_cplane rather than fabricating a zero-expected_iq slot.
static dl_stream_slot_t *find_or_add_dl_stream(dl_symbol_job_t *job, uint8_t eaxc_id, uint16_t beam_id)
{
  for (int s = 0; s < job->num_streams; s++) {
    if (job->streams[s].eaxc_id == eaxc_id && job->streams[s].beam_id == beam_id) {
      return &job->streams[s];
    }
  }
  if (job->num_streams >= MAX_DL_STREAMS_PER_SYMBOL) {
    return NULL;
  }
  dl_stream_slot_t *slot = &job->streams[job->num_streams++];
  memset(slot, 0, sizeof(*slot));
  slot->eaxc_id = eaxc_id;
  slot->beam_id = beam_id;
  return slot;
}

// Appends one joined PRB run; mbuf ownership passes to the job, freed later by read_dl_iq_streams().
// Refuses it when the symbol's fragments would no longer fit read_dl_iq_streams()'s packed arena.
static dl_fragment_t *add_dl_fragment(dl_symbol_job_t *job,
                                      uint8_t eaxc_id,
                                      uint16_t beam_id,
                                      const oru_bfw_set_t *bfw,
                                      int num_prb,
                                      int max_prbs)
{
  if (job->num_fragments >= MAX_DL_FRAGMENTS_PER_SYMBOL || job->num_fragment_prbs + num_prb > max_prbs) {
    return NULL;
  }
  dl_fragment_t *frag = &job->fragments[job->num_fragments++];
  job->num_fragment_prbs += num_prb;
  frag->eaxc_id = eaxc_id;
  frag->beam_id = beam_id;
  frag->bfw = *bfw;
  return frag;
}

void txrx_window_histogram_count(txrx_histogram_t *hist, int32_t diff)
{
  int bin = diff + HIST_SIZE / 2;
  bin = bin < 0 ? 0 : bin > HIST_SIZE - 1 ? HIST_SIZE - 1 : bin;
  hist->sum += diff;
  hist->count++;
  hist->hist[bin]++;
}

// Forgets the weights of every beamId. Once read, make them as stale
static void reset_bf_weights(oru_packet_processor_context_t *ctx)
{
  ctx->bfw_decode_warned = false;
  for (int b = 0; b < ORU_BFW_NUM_BEAM_IDS; b++)
    ctx->bfw_beam_slot[b] = ORU_BFW_NO_SLOT;
  for (int i = 0; i < ORU_BFW_SLOTS; i++) {
    atomic_store_explicit(&ctx->bfw_slots[i].gen, 0, memory_order_relaxed);
    ctx->bfw_free[i] = i;
  }
  ctx->bfw_free_head = 0;
  ctx->bfw_free_count = ORU_BFW_SLOTS;
}

void *init_packet_processor(int numerology,
                            int num_prb,
                            uint32_t T2a_cp_min_uS,
                            uint32_t T2a_cp_max_uS,
                            uint32_t T2a_up_min_uS,
                            uint32_t T2a_up_max_uS,
                            int num_dl_slots,
                            int num_ul_slots,
                            int num_dl_symbols,
                            int num_ul_symbols,
                            int tdd_pattern_length_slots,
                            alloc_func_t alloc_func,
                            send_func_t send_func,
                            void *io_controller,
                            size_t mtu,
                            int prach_eaxc_offset,
                            fh_comp_method_t dl_comp_method,
                            int prach_kbar)
{
  oru_packet_processor_context_t *ctx = calloc(1, sizeof(*ctx));
  ctx->alloc_func = alloc_func;
  ctx->send_func = send_func;
  ctx->io_controller = io_controller;
  ctx->current_absolute_symbol = 0;
  ctx->num_prb = num_prb;
  ctx->numerology = numerology;
  ctx->mtu = mtu;
  ctx->prach_eaxc_offset = prach_eaxc_offset;
  AssertFatal(prach_kbar >= 0 && prach_kbar + FH_PRACH_NUM_SUBCARRIERS * 2 <= FH_PRACH_NUM_PRBS * FH_VALS_PER_PRB,
              "PRACH kbar %d out of range\n",
              prach_kbar);
  ctx->prach_kbar = prach_kbar;
  ctx->dl_comp_method = dl_comp_method;
  // A section has at most num_prb PRBs (Type 3 PRBs are at the carrier SCS), and bundleOffset (6 bits) adds at most 63
  // bundles
  ctx->ext11_max_bundles = num_prb + 63;
  ctx->ext11_beam_ids = malloc(ctx->ext11_max_bundles * sizeof(*ctx->ext11_beam_ids));
  ctx->ext11_weights = malloc((size_t)ctx->ext11_max_bundles * ORU_MAX_BF_WEIGHTS * sizeof(*ctx->ext11_weights));
  AssertFatal(ctx->ext11_beam_ids != NULL && ctx->ext11_weights != NULL, "ext11 buffer allocation failed\n");
  ctx->bfw_slots = calloc(ORU_BFW_SLOTS, sizeof(*ctx->bfw_slots));
  ctx->bfw_beam_slot = malloc(ORU_BFW_NUM_BEAM_IDS * sizeof(*ctx->bfw_beam_slot));
  ctx->bfw_free = malloc(ORU_BFW_SLOTS * sizeof(*ctx->bfw_free));
  ctx->bfw_ring = malloc(ORU_BFW_RING_SIZE * sizeof(*ctx->bfw_ring));
  AssertFatal(ctx->bfw_slots != NULL && ctx->bfw_beam_slot != NULL && ctx->bfw_free != NULL && ctx->bfw_ring != NULL,
              "beamforming weight buffer allocation failed\n");
  reset_bf_weights(ctx);
  uint32_t slots_per_subframe = 1 << numerology;
  uint32_t symbol_duration_uS = 1000 / slots_per_subframe / NR_SYMBOLS_PER_SLOT;
  ctx->T2a_min_cp_sym_diff = T2a_cp_min_uS / symbol_duration_uS;
  ctx->T2a_max_cp_sym_diff = T2a_cp_max_uS / symbol_duration_uS;
  ctx->T2a_min_up_dl_sym_diff = T2a_up_min_uS / symbol_duration_uS;
  ctx->T2a_max_up_dl_sym_diff = T2a_up_max_uS / symbol_duration_uS;
  ctx->dl_ready_jobs = rte_ring_create("dl_ready_jobs", DL_JOB_RING_SIZE, rte_socket_id(), 0);
  AssertFatal(ctx->dl_ready_jobs != NULL, "Failed to create ring dl_ready_jobs\n");
  ctx->dl_free_jobs = rte_ring_create("dl_free_jobs", DL_JOB_RING_SIZE, rte_socket_id(), 0);
  AssertFatal(ctx->dl_free_jobs != NULL, "Failed to create ring dl_free_jobs\n");
  for (int i = 0; i < MAX_CONCURRENT_DL_JOBS; i++) {
    rte_ring_enqueue(ctx->dl_free_jobs, (void *)&ctx->dl_symbol_jobs[i]);
  }
  ctx->ul_ready_jobs = rte_ring_create("ul_ready_jobs", UL_JOB_RING_SIZE, rte_socket_id(), 0);
  AssertFatal(ctx->ul_ready_jobs != NULL, "Failed to create ring ul_ready_jobs\n");
  ctx->ul_free_jobs = rte_ring_create("ul_free_jobs", UL_JOB_RING_SIZE, rte_socket_id(), 0);
  AssertFatal(ctx->ul_free_jobs != NULL, "Failed to create ring ul_free_jobs\n");
  for (int i = 0; i < UL_JOB_RING_SIZE - 1; i++) {
    rte_ring_enqueue(ctx->ul_free_jobs, (void *)&ctx->ul_jobs_pool[i]);
  }

  ctx->eaxcid_config = (struct xran_eaxcid_config){.mask_cuPortId = 0xF000,
                                                   .mask_bandSectorId = 0x0F00,
                                                   .mask_ccId = 0x00F0,
                                                   .mask_ruPortId = 0x000F,
                                                   .bit_cuPortId = 12,
                                                   .bit_bandSectorId = 8,
                                                   .bit_ccId = 4,
                                                   .bit_ruPortId = 0};

  ctx->symbol_bitmask_length = tdd_pattern_length_slots * NR_SYMBOLS_PER_SLOT;
  for (int i = 0; i < num_dl_slots * NR_SYMBOLS_PER_SLOT + num_dl_symbols; i++) {
    set_bit(ctx->dl_symbol_bitmask, i);
  }
  int last_bit = ctx->symbol_bitmask_length - 1;
  for (int i = 0; i < num_ul_slots * NR_SYMBOLS_PER_SLOT + num_ul_symbols; i++) {
    set_bit(ctx->ul_symbol_bitmask, last_bit - i);
  }
  return ctx;
}

// Number of (bfwI, bfwQ) pairs per ext1/ext11 weight vector (M-plane array element count). The ext11 buffers are sized
// for ORU_MAX_BF_WEIGHTS, so changing it while C-plane traffic runs can't overflow them.
void set_num_bf_weights(void *context, int num_bf_weights)
{
  oru_packet_processor_context_t *ctx = (oru_packet_processor_context_t *)context;
  AssertFatal(num_bf_weights >= 0 && num_bf_weights <= ORU_MAX_BF_WEIGHTS,
              "num_bf_weights %d out of range [0..%d]\n",
              num_bf_weights,
              ORU_MAX_BF_WEIGHTS);
  if (num_bf_weights != ctx->num_bf_weights)
    reset_bf_weights(ctx);
  ctx->num_bf_weights = num_bf_weights;
}

void cleanup_packet_processor(void *context)
{
  oru_packet_processor_context_t *ctx = (oru_packet_processor_context_t *)context;
  if (ctx) {
    print_packet_processor_stats(ctx);
    if (ctx->dl_ready_jobs) {
      rte_ring_free(ctx->dl_ready_jobs);
    }
    if (ctx->dl_free_jobs) {
      rte_ring_free(ctx->dl_free_jobs);
    }
    if (ctx->ul_ready_jobs) {
      rte_ring_free(ctx->ul_ready_jobs);
    }
    if (ctx->ul_free_jobs) {
      rte_ring_free(ctx->ul_free_jobs);
    }
    free(ctx->ext11_beam_ids);
    free(ctx->ext11_weights);
    free(ctx->bfw_slots);
    free(ctx->bfw_beam_slot);
    free(ctx->bfw_free);
    free(ctx->bfw_ring);
    free(ctx);
  }
}

static bool dl_job_fully_delivered(const dl_symbol_job_t *job)
{
  for (int s = 0; s < job->num_streams; s++) {
    if (job->streams[s].received_iq != job->streams[s].expected_iq) {
      return false;
    }
  }
  return true;
}

// Releases a job early once its C-Plane window has closed and everything declared has already
// arrived, instead of waiting for push_symbol_job()'s mandatory backstop.
static void release_cplane_complete_jobs(oru_packet_processor_context_t *ctx, uint64_t absolute_symbol)
{
  while (ctx->cplane_closed_tail_symbol <= absolute_symbol) {
    if (!test_bit(ctx->dl_symbol_bitmask, ctx->cplane_closed_tail_symbol % ctx->symbol_bitmask_length)) {
      ctx->cplane_closed_tail_symbol++;
      continue;
    }
    uint32_t job_index = ctx->cplane_closed_tail_symbol % NUM_CONCURRENT_DL_SYMBOL_WINDOWS;
    dl_symbol_job_t *job = ctx->dl_symbol_rx_window[job_index];
    if (job && job->absolute_symbol == ctx->cplane_closed_tail_symbol && dl_job_fully_delivered(job)) {
      ctx->dl_symbol_rx_window[job_index] = NULL;
      ctx->was_dl_symbol_completed[job_index] = true;
      int ret = rte_ring_enqueue(ctx->dl_ready_jobs, (void *)job);
      if (ret != 0) {
        ctx->stats.application_too_slow++;
      }
    }
    ctx->cplane_closed_tail_symbol++;
  }
}

// Mandatory backstop, run on timer expiry: force-flushes every DL symbol job by its T2a deadline,
// complete or not. Symbols already released early by release_cplane_complete_jobs() are skipped
// via was_dl_symbol_completed.
void push_symbol_job(oru_packet_processor_context_t *ctx, uint64_t absolute_symbol)
{
  while (ctx->window_tail_symbol <= absolute_symbol) {
    if (!test_bit(ctx->dl_symbol_bitmask, ctx->window_tail_symbol % ctx->symbol_bitmask_length)) {
      // skip non-dl symbols
      ctx->window_tail_symbol++;
      continue;
    }

    uint32_t job_index = ctx->window_tail_symbol % NUM_CONCURRENT_DL_SYMBOL_WINDOWS;
    dl_symbol_job_t *job = ctx->dl_symbol_rx_window[job_index];
    if (job) {
      // Still sitting in the window (incomplete or never checked) - force flush whatever is there.
      ctx->dl_symbol_rx_window[job_index] = NULL;
      ctx->was_dl_symbol_completed[job_index] = false;
      int ret = rte_ring_enqueue(ctx->dl_ready_jobs, (void *)job);
      if (ret != 0) {
        ctx->stats.application_too_slow++;
      }
    } else if (ctx->was_dl_symbol_completed[job_index]) {
      // Already released early for this exact symbol by release_cplane_complete_jobs() - nothing to do.
      ctx->was_dl_symbol_completed[job_index] = false;
    } else {
      // Never got a single C-plane packet for this symbol - push an empty placeholder so the
      // consumer still sees every DL symbol.
      int ret = rte_ring_dequeue(ctx->dl_free_jobs, (void **)&job);
      if (ret != 0) {
        ctx->stats.application_too_slow++;
        return;
      }
      memset(job, 0, sizeof(*job));
      job->absolute_symbol = ctx->window_tail_symbol;
      ret = rte_ring_enqueue(ctx->dl_ready_jobs, (void *)job);
      if (ret != 0) {
        ctx->stats.application_too_slow++;
      }
    }
    ctx->window_tail_symbol++;
  }
}

void handle_absolute_symbol_tick(void *context, uint64_t absolute_symbol)
{
  oru_packet_processor_context_t *ctx = (oru_packet_processor_context_t *)context;
  if (ctx->current_absolute_symbol == 0) {
    ctx->current_absolute_symbol = absolute_symbol - 1;
    ctx->window_tail_symbol = absolute_symbol - 1;
    ctx->cplane_closed_tail_symbol = absolute_symbol - 1;
  }
  ctx->current_absolute_symbol = absolute_symbol;
  // Guard against underflow: T2a_min_*_sym_diff can legitimately be 0.
  if (ctx->T2a_min_cp_sym_diff > 0) {
    uint64_t cplane_closed_symbol = ctx->current_absolute_symbol + ctx->T2a_min_cp_sym_diff - 1;
    release_cplane_complete_jobs(ctx, cplane_closed_symbol);
  }
  uint32_t min_t2a_min_sym_diff =
      ctx->T2a_min_cp_sym_diff < ctx->T2a_min_up_dl_sym_diff ? ctx->T2a_min_cp_sym_diff : ctx->T2a_min_up_dl_sym_diff;
  uint64_t window_expiry_symbol = ctx->current_absolute_symbol + (min_t2a_min_sym_diff > 0 ? min_t2a_min_sym_diff - 1 : 0);
  push_symbol_job(ctx, window_expiry_symbol);
}

void get_dl_symbol_bitmask(void *context, const uint8_t **bitmask, uint16_t *bit_length)
{
  oru_packet_processor_context_t *ctx = (oru_packet_processor_context_t *)context;
  *bitmask = ctx->dl_symbol_bitmask;
  *bit_length = ctx->symbol_bitmask_length;
}

void handle_uplane_packet(void *context, void *pkt)
{
  oru_packet_processor_context_t *ctx = (oru_packet_processor_context_t *)context;
  ctx->stats.total_uplane_received++;
  void *iq_data_start = NULL;
  uint8_t CC_ID = 0xFF;
  uint8_t Ant_ID = 0xFF;
  uint8_t frame_id;
  uint8_t subframe_id;
  uint8_t slot_id;
  uint8_t symb_id;
  uint8_t filter_id;
  union ecpri_seq_id seq_id;
  uint16_t num_prbu;
  uint16_t start_prbu;
  uint16_t sym_inc;
  uint16_t rb;
  uint16_t sect_id;
  const bool has_comp_hdr = ctx->dl_comp_method != FH_COMP_NONE;
  uint8_t staticComp = 0;
  uint8_t compMeth = 0;
  uint8_t iqWidth = 0;
  int ret = xran_extract_iq_samples(pkt,
                                    &ctx->eaxcid_config,
                                    &iq_data_start,
                                    &CC_ID,
                                    &Ant_ID,
                                    &frame_id,
                                    &subframe_id,
                                    &slot_id,
                                    &symb_id,
                                    &filter_id,
                                    &seq_id,
                                    &num_prbu,
                                    &start_prbu,
                                    &sym_inc,
                                    &rb,
                                    &sect_id,
                                    has_comp_hdr,
                                    staticComp,
                                    &compMeth,
                                    &iqWidth);
  if (ret == 0) {
    LOG_W(HW, "Error reading packet\n");
    rte_pktmbuf_free(pkt);
    return;
  }
  if (has_comp_hdr && compMeth >= FH_COMP_NUM_METHODS) {
    LOG_W(HW, "U-plane packet: invalid compression method %u, dropping\n", compMeth);
    rte_pktmbuf_free(pkt);
    return;
  }
  if (Ant_ID >= MAX_ANTENNAS) {
    // Wire-controlled field - drop rather than let a malformed/out-of-range packet crash the process.
    ctx->stats.invalid_eaxc_id++;
    rte_pktmbuf_free(pkt);
    return;
  }
  uint16_t effective_num_prbu = num_prbu == 0 ? ctx->num_prb : num_prbu;
  uint8_t effective_iq_width = iqWidth == 0 ? XRAN_IQ_BITS_UNCOMPRESSED : iqWidth;
  // start_prbu/num_prbu are wire-controlled. combine_dl_streams() writes num_prb PRBs at start_prb
  // into a txDataF buffer sized for ctx->num_prb PRBs - reject anything that wouldn't fit.
  if (start_prbu > ctx->num_prb || effective_num_prbu > (uint16_t)(ctx->num_prb - start_prbu)) {
    LOG_W(HW,
          "U-plane packet: start_prbu %u + num_prbu %u exceeds configured num_prb %d, dropping\n",
          start_prbu,
          effective_num_prbu,
          ctx->num_prb);
    ctx->stats.uplane_err_prb_range++;
    rte_pktmbuf_free(pkt);
    return;
  }
  // ret is the packet length still remaining after xran_extract_iq_samples() stripped the headers -
  // verify it actually holds the IQ payload the header claims, before iq_data_start is handed off
  // to the deferred unpack_iq() in read_dl_iq_streams().
  size_t expected_bytes = has_comp_hdr ? (size_t)effective_num_prbu * FH_COMP_PRB_BYTES(effective_iq_width)
                                       : (size_t)effective_num_prbu * NR_NB_SC_PER_RB * 2 * sizeof(uint16_t);
  if ((size_t)ret < expected_bytes) {
    LOG_W(HW,
          "U-plane packet: payload too short (%d bytes, need %zu for %u PRBs), dropping\n",
          ret,
          expected_bytes,
          effective_num_prbu);
    ctx->stats.uplane_err_short_payload++;
    rte_pktmbuf_free(pkt);
    return;
  }
  LOG_D(HW,
        "ORAN: U-plane packet received. CC_ID %d, Ant_ID %d, frame_id %d, subframe_id %d, slot_id %d, symb_id %d, filter_id %d, "
        "num_prbu %d, start_prbu %d, sym_inc %d, rb %d, sect_id %d, compMeth %d, iqWidth %d\n",
        CC_ID,
        Ant_ID,
        frame_id,
        subframe_id,
        slot_id,
        symb_id,
        filter_id,
        num_prbu,
        start_prbu,
        sym_inc,
        rb,
        sect_id,
        compMeth,
        iqWidth);

  int mu = ctx->numerology;
  int slots_per_subframe = 1 << mu;
  int num_symbols_per_frame = NR_NUMBER_OF_SUBFRAMES_PER_FRAME * slots_per_subframe * NR_SYMBOLS_PER_SLOT;
  uint32_t current_symbol_in_frame = ctx->current_absolute_symbol % num_symbols_per_frame;
  int symbol_in_frame = NR_SYMBOLS_PER_SLOT * (slot_id + subframe_id * slots_per_subframe) + symb_id;
  uint8_t current_frame_id = (ctx->current_absolute_symbol / num_symbols_per_frame) % 256;
  int frame_diff = (int)frame_id - (int)current_frame_id;
  if (frame_diff < -128) {
    frame_diff += 256;
  } else if (frame_diff > 127) {
    frame_diff -= 256;
  }
  int32_t diff = frame_diff * num_symbols_per_frame + symbol_in_frame - (int32_t)current_symbol_in_frame;
  txrx_window_histogram_count(&ctx->stats.dl_uplane_hist, diff);
  if (diff > (int32_t)ctx->T2a_max_up_dl_sym_diff) {
    ctx->stats.uplane_err_early++;
    rte_pktmbuf_free(pkt);
    return;
  }
  if (diff < (int32_t)ctx->T2a_min_up_dl_sym_diff) {
    ctx->stats.uplane_err_late++;
    rte_pktmbuf_free(pkt);
    return;
  }
  uint64_t target_absolute_symbol = ctx->current_absolute_symbol + diff;
  bool is_dl_symbol = test_bit(ctx->dl_symbol_bitmask, target_absolute_symbol % ctx->symbol_bitmask_length);
  if (!is_dl_symbol) {
    ctx->stats.dl_tdd_mismatch++;
    rte_pktmbuf_free(pkt);
    return;
  }
  uint32_t job_index = target_absolute_symbol % NUM_CONCURRENT_DL_SYMBOL_WINDOWS;
  dl_symbol_job_t *job = ctx->dl_symbol_rx_window[job_index];
  if (!job) {
    ctx->stats.uplane_err_late++;
    rte_pktmbuf_free(pkt);
    return;
  }

  if (job->absolute_symbol != target_absolute_symbol) {
    ctx->stats.uplane_err_late++;
    rte_pktmbuf_free(pkt);
    return;
  }

  oru_bfw_set_t bfw;
  dl_stream_slot_t *stream = find_dl_stream_by_section(job, Ant_ID, sect_id, &bfw);
  if (!stream) {
    // C-Plane never declared this (eaxc, section) - drop rather than guess.
    ctx->stats.uplane_missing_cplane++;
    rte_pktmbuf_free(pkt);
    return;
  }

  dl_fragment_t *frag = add_dl_fragment(job, Ant_ID, stream->beam_id, &bfw, effective_num_prbu, DL_IQ_ARENA_PRBS(ctx->num_prb));
  if (!frag) {
    ctx->stats.dl_fragments_pool_exhausted++;
    LOG_W(HW, "ORU: Dropping extra segment for Ant %d, sym %lu\n", Ant_ID, target_absolute_symbol);
    rte_pktmbuf_free(pkt);
    return;
  }
  frag->start_prbc = start_prbu;
  frag->num_prbc = effective_num_prbu;
  frag->iq_data = iq_data_start;
  frag->mbuf = pkt;
  frag->comp_method = (fh_comp_method_t)compMeth;
  frag->iq_width = effective_iq_width;
  // Only counted once actually stored - crediting a dropped fragment would let
  // dl_job_fully_delivered() falsely report this symbol complete.
  stream->received_iq += frag->num_prbc;
  // Release happens in release_cplane_complete_jobs() or push_symbol_job(), not here.
  return;
}

static void handle_dl_cplane_packet(oru_packet_processor_context_t *ctx,
                                    struct xran_cp_radioapp_section1_header *hdr,
                                    struct xran_cp_radioapp_section1 *section,
                                    int ant_id,
                                    const oru_bfw_set_t *bfw)
{
  if (ant_id < 0 || ant_id >= MAX_ANTENNAS) {
    // Wire-controlled field - drop rather than let a malformed/out-of-range packet crash the process.
    ctx->stats.invalid_eaxc_id++;
    return;
  }
  int numerology = ctx->numerology;
  int slot_in_frame = hdr->cmnhdr.field.slotId + hdr->cmnhdr.field.subframeId * (1 << numerology);
  uint32_t start_symbol = hdr->cmnhdr.field.startSymbolId;
  int num_symbols = section->hdr.u.s1.numSymbol;
  int num_symbols_per_frame = NR_NUMBER_OF_SUBFRAMES_PER_FRAME * (1 << numerology) * NR_SYMBOLS_PER_SLOT;
  uint64_t symbol_in_frame = slot_in_frame * 14 + start_symbol;
  uint32_t current_symbol_in_frame = ctx->current_absolute_symbol % num_symbols_per_frame;
  uint8_t current_frame_id = (ctx->current_absolute_symbol / num_symbols_per_frame) % 256;
  int frame_diff = (int)hdr->cmnhdr.field.frameId - (int)current_frame_id;
  if (frame_diff < -128) {
    frame_diff += 256;
  } else if (frame_diff > 127) {
    frame_diff -= 256;
  }
  int32_t diff = frame_diff * num_symbols_per_frame + (int32_t)symbol_in_frame - (int32_t)current_symbol_in_frame;
  txrx_window_histogram_count(&ctx->stats.dl_cplane_hist, diff);
  if (diff > (int32_t)ctx->T2a_max_cp_sym_diff) {
    ctx->stats.cplane_err_early++;
    return;
  }
  if (diff < (int32_t)ctx->T2a_min_cp_sym_diff) {
    ctx->stats.cplane_err_late++;
    return;
  }
  uint64_t target_absolute_symbol = ctx->current_absolute_symbol + diff;
  bool is_dl_symbol = test_bit(ctx->dl_symbol_bitmask, target_absolute_symbol % ctx->symbol_bitmask_length);
  if (!is_dl_symbol) {
    ctx->stats.dl_tdd_mismatch++;
    return;
  }
  for (int i = 0; i < num_symbols; i++) {
    // Multi-symbol section: symbol i of the section sits at target_absolute_symbol + i, so its own
    // offset from "now" is diff + i (later symbols are further out, not closer) - check each one
    // individually against both bounds, not just the section's first symbol.
    int32_t sym_diff = diff + i;
    if (sym_diff > (int32_t)ctx->T2a_max_cp_sym_diff) {
      ctx->stats.cplane_err_early++;
      return;
    }
    if (sym_diff < (int32_t)ctx->T2a_min_cp_sym_diff) {
      ctx->stats.cplane_err_late++;
      return;
    }
    uint32_t job_index = (target_absolute_symbol + i) % NUM_CONCURRENT_DL_SYMBOL_WINDOWS;
    dl_symbol_job_t *job = ctx->dl_symbol_rx_window[job_index];
    if (!job) {
      // First C-Plane packet for this symbol.
      int ret = rte_ring_dequeue(ctx->dl_free_jobs, (void **)&job);
      if (ret != 0) {
        ctx->stats.application_too_slow++;
        return;
      }
      job->absolute_symbol = target_absolute_symbol + i;
      job->num_streams = 0;
      job->num_fragments = 0;
      job->num_fragment_prbs = 0;
      ctx->dl_symbol_rx_window[job_index] = job;
      ctx->was_dl_symbol_completed[job_index] = false;
    } else {
      if (job->absolute_symbol != target_absolute_symbol + i) {
        ctx->stats.cplane_err_late++;
        return;
      }
    }
    uint16_t section_id = section->hdr.u1.common.sectionId;
    if (find_dl_stream_by_section(job, (uint8_t)ant_id, section_id, NULL)) {
      // Duplicate C-Plane packet for this section_id.
      ctx->stats.cplane_err_dup++;
      ctx->stats.cplane_err_dup_dl++;
      return;
    }
    dl_stream_slot_t *stream = find_or_add_dl_stream(job, (uint8_t)ant_id, section->hdr.u.s1.beamId);
    if (!stream) {
      ctx->stats.dl_stream_pool_exhausted++;
      return;
    }
    if (stream->num_section_ids >= MAX_SECTIONS_PER_DL_STREAM) {
      ctx->stats.dl_stream_sections_exhausted++;
      return;
    }
    stream->section_bfw[stream->num_section_ids] = *bfw;
    stream->section_ids[stream->num_section_ids++] = section_id;
    stream->expected_iq += section->hdr.u1.common.numPrbc == 0 ? ctx->num_prb : section->hdr.u1.common.numPrbc;
    // comp_method/iq_width come from each U-Plane packet's own header, not the C-Plane
    // declaration - see dl_fragment_t.
  }
}

static void handle_ul_cplane_packet(oru_packet_processor_context_t *ctx,
                                    struct xran_cp_radioapp_section1_header *hdr,
                                    struct xran_cp_radioapp_section1 *section,
                                    int ant_id,
                                    const oru_bfw_set_t *bfw,
                                    oru_pcap_cplane_snap_t *snap)
{
  int numerology = ctx->numerology;
  int slot_in_frame = hdr->cmnhdr.field.slotId + hdr->cmnhdr.field.subframeId * (1 << numerology);
  uint32_t start_symbol = hdr->cmnhdr.field.startSymbolId;
  int num_symbols = section->hdr.u.s1.numSymbol;
  if (num_symbols <= 0) {
    ctx->stats.ul_cplane_err_invalid_num_symbols++;
    return;
  }
  int num_symbols_per_frame = NR_NUMBER_OF_SUBFRAMES_PER_FRAME * (1 << numerology) * NR_SYMBOLS_PER_SLOT;
  uint64_t symbol_in_frame = slot_in_frame * 14 + start_symbol;
  uint32_t current_symbol_in_frame = ctx->current_absolute_symbol % num_symbols_per_frame;
  uint8_t current_frame_id = (ctx->current_absolute_symbol / num_symbols_per_frame) % 256;
  int frame_diff = (int)hdr->cmnhdr.field.frameId - (int)current_frame_id;
  if (frame_diff < -128) {
    frame_diff += 256;
  } else if (frame_diff > 127) {
    frame_diff -= 256;
  }
  int32_t diff = frame_diff * num_symbols_per_frame + (int32_t)symbol_in_frame - (int32_t)current_symbol_in_frame;
  txrx_window_histogram_count(&ctx->stats.ul_cplane_hist, diff);
  if (diff > (int32_t)ctx->T2a_max_cp_sym_diff) {
    ctx->stats.cplane_err_early++;
    return;
  }
  if (diff < (int32_t)ctx->T2a_min_cp_sym_diff) {
    ctx->stats.cplane_err_late++;
    return;
  }
  uint64_t target_absolute_symbol = ctx->current_absolute_symbol + diff;
  bool is_ul_symbol = test_bit(ctx->ul_symbol_bitmask, target_absolute_symbol % ctx->symbol_bitmask_length);
  if (!is_ul_symbol) {
    ctx->stats.ul_tdd_mismatch++;
    return;
  }
  ul_job_t *ul_job = NULL;
  if (rte_ring_dequeue(ctx->ul_free_jobs, (void **)&ul_job) == 0) {
    memset(ul_job, 0, sizeof(*ul_job));
    ul_job->response_payload.section_id = section->hdr.u1.common.sectionId;
    ul_job->response_payload.comp_method = hdr->udComp.udCompMeth;
    ul_job->response_payload.iq_width = hdr->udComp.udIqWidth == 0 ? XRAN_IQ_BITS_UNCOMPRESSED : hdr->udComp.udIqWidth;
    uint64_t absolute_gps_symbol = target_absolute_symbol;
    ul_job->hyper_frame = absolute_gps_symbol / (1024 * (10 * (1 << ctx->numerology) * 14));
    ul_job->frame = (absolute_gps_symbol / (10 * (1 << ctx->numerology) * 14)) % 1024;
    ul_job->slot_in_frame = (absolute_gps_symbol % (10 * (1 << ctx->numerology) * 14)) / 14;
    ul_job->symbol = absolute_gps_symbol % 14;
    ul_job->num_symbols = num_symbols;
    ul_job->antenna_id = ant_id;
    ul_job->beam_id = section->hdr.u.s1.beamId;
    ul_job->num_prb = section->hdr.u1.common.numPrbc == 0 ? ctx->num_prb : section->hdr.u1.common.numPrbc;
    ul_job->start_prb = section->hdr.u1.common.startPrbc;
    ul_job->bfw = *bfw;
    int ret = rte_ring_enqueue(ctx->ul_ready_jobs, (void *)ul_job);
    AssertFatal(ret == 0, "Failed to enqueue ul_job to ul_ready_jobs ring\n");
    oru_pcap_cplane_commit_pusch(snap);
  } else {
    ctx->stats.application_too_slow++;
  }
}

// The C-Plane thread rewrites a PRACH job between these, so that get_prach_bfw_set() reads it whole from another thread
static void prach_job_write_begin(prach_job_t *job)
{
  atomic_store_explicit(&job->seq, atomic_load_explicit(&job->seq, memory_order_relaxed) + 1, memory_order_relaxed);
  atomic_thread_fence(memory_order_release);
}

static void prach_job_write_end(prach_job_t *job)
{
  atomic_store_explicit(&job->seq, atomic_load_explicit(&job->seq, memory_order_relaxed) + 1, memory_order_release);
}

// Registers PRACH section section_id of stream aarx starting at start_symbol. Jobs of an earlier slot are released
// first. Sections starting at the same symbol are FDM occasions, numbered in arrival order. Returns NULL for a
// duplicate section or a full pool.
static prach_job_t *add_prach_job(oru_packet_processor_context_t *ctx,
                                  int slot_in_frame,
                                  int aarx,
                                  uint64_t start_symbol,
                                  int section_id)
{
  prach_job_t *jobs = ctx->prach_jobs[slot_in_frame][aarx];
  prach_job_t *free_job = NULL;
  int occasion = 0;
  for (int i = 0; i < MAX_PRACH_SECTIONS; i++) {
    prach_job_t *job = &jobs[i];
    if (job->active && job->start_absolute_symbol / NR_SYMBOLS_PER_SLOT != start_symbol / NR_SYMBOLS_PER_SLOT) {
      if (!job->served)
        ctx->stats.prach_sections_unserved++;
      prach_job_write_begin(job);
      job->active = false;
      prach_job_write_end(job);
    }
    if (!job->active) {
      if (free_job == NULL)
        free_job = job;
      continue;
    }
    if (job->start_absolute_symbol != start_symbol)
      continue;
    if (job->section_id == section_id) {
      ctx->stats.cplane_err_dup++;
      ctx->stats.cplane_err_dup_prach++;
      RATELIMIT(PRACH_ERR_LOG_RATELIMIT, {
        LOG_W(HW,
              "PRACH CP: Duplicate section %d for slot %d, aarx %d, start_symbol %lu\n",
              section_id,
              slot_in_frame,
              aarx,
              start_symbol);
      });
      return NULL;
    }
    occasion++;
  }
  if (free_job == NULL) {
    ctx->stats.prach_jobs_pool_exhausted++;
    return NULL;
  }
  free_job->occasion = occasion;
  free_job->served = false;
  return free_job;
}

// The active job of PRACH occasion `occasion` of stream aarx whose symbols contain symbol, or another active job of that
// occasion when none does, or NULL.
static prach_job_t *find_prach_job(oru_packet_processor_context_t *ctx, int slot_in_frame, int aarx, int occasion, uint64_t symbol)
{
  prach_job_t *found = NULL;
  for (int i = 0; i < MAX_PRACH_SECTIONS; i++) {
    prach_job_t *job = &ctx->prach_jobs[slot_in_frame][aarx][i];
    if (!job->active || job->occasion != occasion)
      continue;
    if (symbol >= job->start_absolute_symbol && symbol < job->start_absolute_symbol + job->num_symbols)
      return job;
    found = job;
  }
  return found;
}

// Registers one Type 3 section of a PRACH C-Plane message for stream aarx, starting at start_symbol.
static void add_prach_section(oru_packet_processor_context_t *ctx,
                              const struct xran_cp_radioapp_section3_header *hdr,
                              const struct xran_cp_radioapp_section3 *section,
                              int slot_in_frame,
                              int aarx,
                              uint64_t start_symbol,
                              int num_prb,
                              const oru_bfw_set_t *bfw,
                              oru_pcap_cplane_snap_t *snap)
{
  prach_job_t *job = add_prach_job(ctx, slot_in_frame, aarx, start_symbol, section->hdr.u1.common.sectionId);
  if (job == NULL)
    return;
  prach_job_write_begin(job);
  job->active = true;
  job->start_absolute_symbol = start_symbol;
  job->num_symbols = section->hdr.u.s3.numSymbol;
  job->section_id = section->hdr.u1.common.sectionId;
  job->num_prb = num_prb;
  job->start_prb = section->hdr.u1.common.startPrbc;
  job->filter_id = hdr->cmnhdr.field.filterIndex;
  job->beam_id = section->hdr.u.s3.beamId;
  job->bfw = *bfw;
  job->comp_method = (fh_comp_method_t)hdr->udComp.udCompMeth;
  job->iq_width = hdr->udComp.udIqWidth == 0 ? XRAN_IQ_BITS_UNCOMPRESSED : hdr->udComp.udIqWidth;
  prach_job_write_end(job);
  RATELIMIT(PRACH_ERR_LOG_RATELIMIT, {
    LOG_A(HW,
          "PRACH JOB added slot_in_frame %d, aarx %d, occasion %d, target_absolute_symbol %lu\n",
          slot_in_frame,
          aarx,
          job->occasion,
          start_symbol);
  });
  oru_pcap_cplane_commit_prach(snap);
}

static const uint8_t *parse_section_extensions(oru_packet_processor_context_t *ctx,
                                               const uint8_t *ext,
                                               const uint8_t *end,
                                               bool ef,
                                               uint16_t beam_id,
                                               int start_prb,
                                               int num_prb,
                                               oru_bfw_set_t *bfw);

void handle_prach_cplane_packet(oru_packet_processor_context_t *ctx,
                                void *pkt,
                                struct xran_cp_radioapp_section3_header *hdr,
                                uint8_t ant_id,
                                oru_pcap_cplane_snap_t *snap)
{
  hdr->timeOffset = rte_be_to_cpu_16(hdr->timeOffset);
  hdr->cpLength = rte_be_to_cpu_16(hdr->cpLength);

  uint8_t *sect = (void *)rte_pktmbuf_adj(pkt, sizeof(struct xran_cp_radioapp_section3_header));
  if (sect == NULL) {
    ctx->stats.cplane_err_hdr++;
    RATELIMIT(PRACH_ERR_LOG_RATELIMIT, { LOG_W(HW, "PRACH CP: Failed to adjust mbuf for section3 header\n"); });
    return;
  }
  const uint8_t *end = sect + rte_pktmbuf_data_len((struct rte_mbuf *)pkt);
  int aarx = ant_id - ctx->prach_eaxc_offset;
  if (aarx < 0 || aarx >= MAX_ANTENNAS) {
    RATELIMIT(PRACH_ERR_LOG_RATELIMIT,
              { LOG_W(HW, "PRACH CP: Invalid aarx %d (ant_id %d, eaxc_offset %d)\n", aarx, ant_id, ctx->prach_eaxc_offset); });
    return;
  }

  int numerology = ctx->numerology;
  int slot_in_frame = hdr->cmnhdr.field.slotId + hdr->cmnhdr.field.subframeId * (1 << numerology);
  uint32_t start_symbol = hdr->cmnhdr.field.startSymbolId;
  int num_symbols_per_frame = NR_NUMBER_OF_SUBFRAMES_PER_FRAME * (1 << numerology) * NR_SYMBOLS_PER_SLOT;
  uint64_t symbol_in_frame = slot_in_frame * NR_SYMBOLS_PER_SLOT + start_symbol;
  uint32_t current_symbol_in_frame = ctx->current_absolute_symbol % num_symbols_per_frame;
  uint8_t current_frame_id = (ctx->current_absolute_symbol / num_symbols_per_frame) % 256;
  int frame_diff = (int)hdr->cmnhdr.field.frameId - (int)current_frame_id;
  if (frame_diff < -128) {
    frame_diff += 256;
  } else if (frame_diff > 127) {
    frame_diff -= 256;
  }
  int32_t diff = frame_diff * num_symbols_per_frame + (int32_t)symbol_in_frame - (int32_t)current_symbol_in_frame;
  txrx_window_histogram_count(&ctx->stats.prach_cplane_hist, diff);
  uint64_t target_absolute_symbol = ctx->current_absolute_symbol + diff;

  if (slot_in_frame < 0 || slot_in_frame >= MAX_SLOTS_PER_FRAME) {
    RATELIMIT(PRACH_ERR_LOG_RATELIMIT, { LOG_W(HW, "PRACH CP: Invalid slot_in_frame %d\n", slot_in_frame); });
    return;
  }

  // As for Type 1: the next section starts where the previous one's extension chain ends, and only the first citation
  // of a sectionId is used
  uint8_t cited[NUM_SECTION_IDS / 8] = {0};
  for (int i = 0; i < hdr->cmnhdr.numOfSections; i++) {
    if ((size_t)(end - sect) < sizeof(struct xran_cp_radioapp_section3)) {
      ctx->stats.cplane_err_hdr++;
      RATELIMIT(PRACH_ERR_LOG_RATELIMIT,
                { LOG_W(HW, "PRACH CP: Section %d of %d past the end of the message\n", i, hdr->cmnhdr.numOfSections); });
      break;
    }
    struct xran_cp_radioapp_section3 *section = (struct xran_cp_radioapp_section3 *)sect;
    *((uint64_t *)section) = rte_be_to_cpu_64(*((uint64_t *)section));
    // Type 3 PRBs are in the SCS of frameStructure (O-RAN CUS 7.5.2.13), taken to be the carrier's: PRACH formats with
    // another SCS aren't supported. numPrbc 0 is then all carrier PRBs (7.5.3.6).
    const int num_prb = section->hdr.u1.common.numPrbc == 0 ? ctx->num_prb : section->hdr.u1.common.numPrbc;
    // ext1/ext11 weights are stored as for Type 1
    oru_bfw_set_t bfw;
    const uint8_t *next = parse_section_extensions(ctx,
                                                   sect + sizeof(*section),
                                                   end,
                                                   section->hdr.u.s3.ef,
                                                   section->hdr.u.s3.beamId,
                                                   section->hdr.u1.common.startPrbc,
                                                   num_prb,
                                                   &bfw);
    const uint16_t section_id = section->hdr.u1.common.sectionId;
    if (section_id != SECTION_ID_UNCOUPLED && test_bit(cited, section_id)) {
      ctx->stats.cplane_section_repeated++;
    } else {
      set_bit(cited, section_id);
      add_prach_section(ctx, hdr, section, slot_in_frame, aarx, target_absolute_symbol, num_prb, &bfw, snap);
    }
    // A malformed chain hides where the next section starts
    if (next == NULL)
      break;
    sect = (uint8_t *)next;
  }
}

// Counts a malformed section extension against its type
static void count_section_ext_error(oru_packet_processor_context_t *ctx, uint8_t ext_type)
{
  switch (ext_type) {
    case XRAN_CP_SECTIONEXTCMD_1:
      ctx->stats.cplane_err_sect_ext1++;
      break;
    case XRAN_CP_SECTIONEXTCMD_11:
      ctx->stats.cplane_err_sect_ext11++;
      break;
    default:
      ctx->stats.cplane_err_sect_ext_other++;
      break;
  }
}

// Current weights of beam_id
static oru_bfw_ref_t bfw_lookup(const oru_packet_processor_context_t *ctx, uint16_t beam_id)
{
  oru_bfw_ref_t ref = {.beam_id = beam_id};
  uint16_t slot = ctx->bfw_beam_slot[beam_id];
  if (slot != ORU_BFW_NO_SLOT) {
    ref.slot = slot;
    ref.gen = atomic_load_explicit(&ctx->bfw_slots[slot].gen, memory_order_relaxed);
  }
  return ref;
}

// Create weights of beam_id for num_bf_weights. beamId 0 is no beamforming (CUS 7.5.3.9).
static oru_bfw_ref_t bfw_store(oru_packet_processor_context_t *ctx, uint16_t beam_id, const c16_t *w)
{
  oru_bfw_ref_t ref = {.beam_id = beam_id};
  if (beam_id == 0)
    return ref;
  size_t size = ctx->num_bf_weights * sizeof(*w);
  uint16_t cur = ctx->bfw_beam_slot[beam_id];
  // Resending the same weights is not an update (CUS 12.4.3.1)
  if (cur != ORU_BFW_NO_SLOT && memcmp(ctx->bfw_slots[cur].w, w, size) == 0)
    return bfw_lookup(ctx, beam_id);
  if (ctx->bfw_free_count == 0) {
    if (cur == ORU_BFW_NO_SLOT) {
      ctx->stats.bfw_pool_exhausted++;
      return ref;
    }
    // Every slot is some beamId's current weights: rewrite this beamId's own
    ctx->bfw_free[ctx->bfw_free_head] = cur;
    ctx->bfw_free_count = 1;
    cur = ORU_BFW_NO_SLOT;
  }
  uint16_t slot = ctx->bfw_free[ctx->bfw_free_head];
  ctx->bfw_free_head = (ctx->bfw_free_head + 1) % ORU_BFW_SLOTS;
  ctx->bfw_free_count--;
  if (cur != ORU_BFW_NO_SLOT) {
    ctx->bfw_free[(ctx->bfw_free_head + ctx->bfw_free_count) % ORU_BFW_SLOTS] = cur;
    ctx->bfw_free_count++;
  }
  if (++ctx->bfw_gen == 0)
    ctx->bfw_gen = 1;
  oru_bfw_slot_t *s = &ctx->bfw_slots[slot];
  atomic_store_explicit(&s->gen, 0, memory_order_relaxed);
  atomic_thread_fence(memory_order_release);
  memcpy(s->w, w, size);
  atomic_store_explicit(&s->gen, ctx->bfw_gen, memory_order_release);
  ctx->bfw_beam_slot[beam_id] = slot;
  ctx->stats.bfw_updates++;
  ref.slot = slot;
  ref.gen = ctx->bfw_gen;
  return ref;
}

// Reserves n bundle ring entries and returns the position of the first
static uint32_t bfw_ring_reserve(oru_packet_processor_context_t *ctx, int n)
{
  uint32_t first = atomic_load_explicit(&ctx->bfw_ring_reserve, memory_order_relaxed);
  atomic_store_explicit(&ctx->bfw_ring_reserve, first + n, memory_order_relaxed);
  atomic_thread_fence(memory_order_release);
  return first;
}

static inline oru_bfw_ref_t *bfw_ring_entry(oru_packet_processor_context_t *ctx, uint32_t pos)
{
  return &ctx->bfw_ring[pos % ORU_BFW_RING_SIZE];
}

// One bundle covering the section, for the weights ref
static oru_bfw_set_t bfw_section_set(oru_packet_processor_context_t *ctx, int start_prb, int num_prb, oru_bfw_ref_t ref)
{
  oru_bfw_set_t set = {0};
  if (ref.gen == 0)
    return set;
  set.first = bfw_ring_reserve(ctx, 1);
  *bfw_ring_entry(ctx, set.first) = ref;
  set.num_bundles = 1;
  set.bundle_prbs = num_prb;
  set.first_prb = start_prb;
  return set;
}

// SE11 bundles: their weights are stored for their beamIds (CUS 7.9.4.1)
static oru_bfw_set_t bfw_ext11_set(oru_packet_processor_context_t *ctx,
                                   int start_prb,
                                   const xran_bfw_ext11_hdr_t *hdr,
                                   int num_bundles)
{
  oru_bfw_set_t set = {.num_bundles = num_bundles, .bundle_prbs = hdr->numBundPrb, .first_prb = start_prb - hdr->bundleOffset};
  set.first = bfw_ring_reserve(ctx, num_bundles);
  for (int b = 0; b < num_bundles; b++) {
    uint16_t beam_id = ctx->ext11_beam_ids[b];
    *bfw_ring_entry(ctx, set.first + b) =
        hdr->disableBFWs ? bfw_lookup(ctx, beam_id) : bfw_store(ctx, beam_id, &ctx->ext11_weights[b * ctx->num_bf_weights]);
  }
  return set;
}

// Decodes the weights of an ext1 of ext_len bytes and stores them for beam_id; *bfw gets them and *have_bfw is set.
// Without configured weights it only counts the ext1. Returns false if the weights don't decode.
static bool parse_section_ext1(oru_packet_processor_context_t *ctx,
                               uint16_t beam_id,
                               int start_prb,
                               const uint8_t *ext,
                               size_t ext_len,
                               int num_prb,
                               oru_bfw_set_t *bfw,
                               bool *have_bfw)
{
  ctx->stats.cplane_ext1_received++;
  if (ctx->num_bf_weights == 0)
    return true;
  c16_t weights[ORU_MAX_BF_WEIGHTS];
  if (xran_decode_bfw_ext1(ext, ext_len, ctx->num_bf_weights, weights) != XRAN_BFW_OK)
    return false;
  *bfw = bfw_section_set(ctx, start_prb, num_prb, bfw_store(ctx, beam_id, weights));
  *have_bfw = true;
  return true;
}

// Decodes the beamIds and weights of an ext11 of ext_len bytes for a section of num_prb PRBs from start_prb and stores each
// bundle's weights for its beamId; *bfw gets the bundles and *have_bfw is set. Without configured weights it only counts
// the ext11. Returns false if the extension doesn't decode.
static bool parse_section_ext11(oru_packet_processor_context_t *ctx,
                                int start_prb,
                                const uint8_t *ext,
                                size_t ext_len,
                                int num_prb,
                                oru_bfw_set_t *bfw,
                                bool *have_bfw)
{
  ctx->stats.cplane_ext11_received++;
  if (ctx->num_bf_weights == 0)
    return true;
  xran_bfw_ext11_hdr_t hdr;
  int num_bundles;
  if (xran_decode_bfw_ext11(ext,
                            ext_len,
                            num_prb,
                            ctx->num_bf_weights,
                            ctx->ext11_max_bundles,
                            &hdr,
                            &num_bundles,
                            ctx->ext11_beam_ids,
                            ctx->ext11_weights)
      != XRAN_BFW_OK)
    return false;
  *bfw = bfw_ext11_set(ctx, start_prb, &hdr, num_bundles);
  *have_bfw = true;
  return true;
}

// Counts an ext1/ext11 whose weights don't decode and logs the first: a DU sending another weight count fails every one
static void bfw_decode_failed(oru_packet_processor_context_t *ctx, uint8_t ext_type)
{
  count_section_ext_error(ctx, ext_type);
  if (!ctx->bfw_decode_warned)
    LOG_W(HW,
          "ext%d weights don't decode for %d weights per vector (malformed, or the DU sends another count), using the "
          "section beamId\n",
          ext_type,
          ctx->num_bf_weights);
  ctx->bfw_decode_warned = true;
}

// Walks the extension chain in [ext, end) of a section of num_prb PRBs from start_prb, with the given beamId and ef,
// counting malformed ext1, ext11 and other extensions, and a chain that ends before an extension its ef promised. Returns
// the end of the chain, or NULL at the first error, since the rest of the chain can't be located. A decoded ext1 or ext11
// sets *bfw (the last one wins; CUS doesn't allow both in one section). Without one, *bfw gets the weights an earlier
// extension set for beam_id, if any.
static const uint8_t *parse_section_extensions(oru_packet_processor_context_t *ctx,
                                               const uint8_t *ext,
                                               const uint8_t *end,
                                               bool ef,
                                               uint16_t beam_id,
                                               int start_prb,
                                               int num_prb,
                                               oru_bfw_set_t *bfw)
{
  *bfw = (oru_bfw_set_t){0};
  bool have_bfw = false;

  // Walk the extension chain until ef is 0
  while (ef) {
    if (ext >= end) {
      ctx->stats.cplane_err_sect_ext_chain++;
      ext = NULL;
      break;
    }
    // extType/ef are in the first byte, followed by an 8- or 16-bit extLen (4-byte words)
    uint8_t ext_type = ext[0] & 0x7F;
    ef = ext[0] & 0x80;
    size_t ext_len;
    if (xran_section_ext_len(ext_type, ext, end - ext, &ext_len) != XRAN_BFW_OK) {
      count_section_ext_error(ctx, ext_type);
      ext = NULL;
      break;
    }
    bool valid = true;
    switch (ext_type) {
      case XRAN_CP_SECTIONEXTCMD_1:
        valid = parse_section_ext1(ctx, beam_id, start_prb, ext, ext_len, num_prb, bfw, &have_bfw);
        break;
      case XRAN_CP_SECTIONEXTCMD_11:
        valid = parse_section_ext11(ctx, start_prb, ext, ext_len, num_prb, bfw, &have_bfw);
        break;
      default: // skipped
        break;
    }
    if (!valid) {
      bfw_decode_failed(ctx, ext_type);
      ext = NULL;
      break;
    }
    ext += ext_len;
  }
  if (!have_bfw && ctx->num_bf_weights > 0)
    *bfw = bfw_section_set(ctx, start_prb, num_prb, bfw_lookup(ctx, beam_id));
  return ext;
}

void handle_cplane_packet(void *context, void *pkt)
{
  oru_packet_processor_context_t *ctx = (oru_packet_processor_context_t *)context;
  oru_pcap_cplane_snap_t snap = {0};
  oru_pcap_cplane_begin(pkt, &snap);
  struct xran_ecpri_hdr *ecpri_hdr;
  struct xran_recv_packet_info xran_recv_packet_info;
  int ret = xran_parse_ecpri_hdr(pkt, &ecpri_hdr, &xran_recv_packet_info);
  if (ret != 0) {
    rte_pktmbuf_free(pkt);
    return;
  }
  uint8_t cu_port_id, band_sector_id, cc_id, ant_id;
  xran_decompose_cid(ecpri_hdr->ecpri_xtc_id, &ctx->eaxcid_config, &cu_port_id, &band_sector_id, &cc_id, &ant_id);
  ctx->stats.total_cplane++;
  struct xran_cp_radioapp_common_header *apphdr = (void *)rte_pktmbuf_adj(pkt, sizeof(struct xran_ecpri_hdr));
  if (apphdr == NULL) {
    ctx->stats.cplane_err_hdr++;
    rte_pktmbuf_free(pkt);
    return;
  }
  apphdr->field.all_bits = rte_be_to_cpu_32(apphdr->field.all_bits);
  if (apphdr->field.payloadVer != XRAN_PAYLOAD_VER) {
    ctx->stats.cplane_err_ver++;
    rte_pktmbuf_free(pkt);
    return;
  }

  switch (apphdr->sectionType) {
    case XRAN_CP_SECTIONTYPE_1: {
      struct xran_cp_radioapp_section1_header *hdr = (struct xran_cp_radioapp_section1_header *)apphdr;
      uint8_t *sect = (void *)rte_pktmbuf_adj(pkt, sizeof(struct xran_cp_radioapp_section1_header));
      if (sect == NULL) {
        ctx->stats.cplane_err_hdr++;
        rte_pktmbuf_free(pkt);
        return;
      }
      const uint8_t *end = sect + rte_pktmbuf_data_len((struct rte_mbuf *)pkt);
      const bool is_dl = hdr->cmnhdr.field.dataDirection == XRAN_DIR_DL;
      if (is_dl)
        ctx->stats.cplane_received_dl++;
      else
        ctx->stats.cplane_received_ul++;
      // O-RAN CUS 7.5.3.1: a sectionId may be cited more than once in a message, with complementary reMasks and
      // different beams. Per-RE beams aren't supported, so only the first citation is used.
      uint8_t cited[NUM_SECTION_IDS / 8] = {0};
      // Each section is followed by its own extension chain, so the next section starts where that chain ends
      for (int i = 0; i < hdr->cmnhdr.numOfSections; i++) {
        if ((size_t)(end - sect) < sizeof(struct xran_cp_radioapp_section1)) {
          ctx->stats.cplane_err_hdr++;
          break;
        }
        struct xran_cp_radioapp_section1 *section = (struct xran_cp_radioapp_section1 *)sect;
        *((uint64_t *)section) = rte_be_to_cpu_64(*((uint64_t *)section));
        int num_prb = section->hdr.u1.common.numPrbc == 0 ? ctx->num_prb : section->hdr.u1.common.numPrbc;
        oru_bfw_set_t bfw;
        const uint8_t *next = parse_section_extensions(ctx,
                                                       sect + sizeof(*section),
                                                       end,
                                                       section->hdr.u.s1.ef,
                                                       section->hdr.u.s1.beamId,
                                                       section->hdr.u1.common.startPrbc,
                                                       num_prb,
                                                       &bfw);
        const uint16_t section_id = section->hdr.u1.common.sectionId;
        if (section_id != SECTION_ID_UNCOUPLED && test_bit(cited, section_id)) {
          ctx->stats.cplane_section_repeated++;
        } else {
          set_bit(cited, section_id);
          if (is_dl)
            handle_dl_cplane_packet(ctx, hdr, section, ant_id, &bfw);
          else
            handle_ul_cplane_packet(ctx, hdr, section, ant_id, &bfw, &snap);
        }
        // A malformed chain hides where the next section starts
        if (next == NULL)
          break;
        sect = (uint8_t *)next;
      }
      rte_pktmbuf_free(pkt);
      return;
    }
    case XRAN_CP_SECTIONTYPE_3: {
      ctx->stats.cplane_received_prach++;
      struct xran_cp_radioapp_section3_header *hdr = (struct xran_cp_radioapp_section3_header *)apphdr;
      handle_prach_cplane_packet(ctx, pkt, hdr, ant_id, &snap);
      rte_pktmbuf_free(pkt);
      return;
    }
    default:
      ctx->stats.cplane_received_other++;
      rte_pktmbuf_free(pkt);
      return;
  }
}

static void print_histogram(const char *name, txrx_histogram_t *hist, uint32_t window_start, uint32_t window_end)
{
  if (hist->count == 0)
    return;
  char buf[4096];
  int len = snprintf(buf,
                     sizeof(buf),
                     "  %s (mean: %.2f symbols) window [%u, %u]:",
                     name,
                     (double)hist->sum / hist->count,
                     window_start,
                     window_end);
  bool first = true;
  for (int i = 0; i < HIST_SIZE; i++) {
    if (hist->hist[i] > 0) {
      int bucket = i - HIST_SIZE / 2;
      char bin_str[64];
      int bin_len = 0;
      if (i == 0) {
        bin_len = snprintf(bin_str, sizeof(bin_str), "%s<=%+d:%lu", first ? " " : ", ", bucket, hist->hist[i]);
      } else if (i == HIST_SIZE - 1) {
        bin_len = snprintf(bin_str, sizeof(bin_str), "%s>=%+d:%lu", first ? " " : ", ", bucket, hist->hist[i]);
      } else {
        bin_len = snprintf(bin_str, sizeof(bin_str), "%s%+d:%lu", first ? " " : ", ", bucket, hist->hist[i]);
      }
      first = false;
      if (len + bin_len < sizeof(buf)) {
        strcpy(buf + len, bin_str);
        len += bin_len;
      } else {
        break; // buffer full
      }
    }
  }
  LOG_I(HW, "%s\n", buf);
  memset(hist, 0, sizeof(*hist));
}

void print_packet_processor_stats(void *context)
{
  oru_packet_processor_context_t *ctx = (oru_packet_processor_context_t *)context;
  if (ctx == NULL)
    return;

  LOG_I(HW, "ORU Packet Processor Stats:\n");
  LOG_I(HW,
        "  Total C-Plane Packets received: %lu (DL: %lu, UL: %lu, PRACH: %lu, Other: %lu)\n",
        ctx->stats.total_cplane,
        ctx->stats.cplane_received_dl,
        ctx->stats.cplane_received_ul,
        ctx->stats.cplane_received_prach,
        ctx->stats.cplane_received_other);
  LOG_I(HW, "  Total U-Plane Packets received: %lu\n", ctx->stats.total_uplane_received);
  LOG_I(HW, "  Total U-Plane Packets sent: %lu\n", ctx->thread_safe_stats.total_uplane_sent);
  if (ctx->thread_safe_stats.ul_uplane_ota_delay_count > 0)
    LOG_I(HW,
          "  UL U-Plane OTA delay (mean symbols): %.2f (%lu packets)\n",
          (double)(int64_t)ctx->thread_safe_stats.ul_uplane_ota_delay_sum
              / (double)ctx->thread_safe_stats.ul_uplane_ota_delay_count,
          (uint64_t)ctx->thread_safe_stats.ul_uplane_ota_delay_count);

  if (ctx->stats.cplane_err_hdr > 0)
    LOG_I(HW, "  C-Plane Header Errors: %lu\n", ctx->stats.cplane_err_hdr);
  if (ctx->stats.cplane_err_ver > 0)
    LOG_I(HW, "  C-Plane Protocol Version Errors: %lu\n", ctx->stats.cplane_err_ver);
  if (ctx->stats.cplane_err_early > 0)
    LOG_I(HW, "  C-Plane Timing Early Errors: %lu\n", ctx->stats.cplane_err_early);
  if (ctx->stats.cplane_err_late > 0)
    LOG_I(HW, "  C-Plane Timing Late Errors: %lu\n", ctx->stats.cplane_err_late);
  if (ctx->stats.cplane_err_dup > 0)
    LOG_I(HW,
          "  C-Plane Duplicate Packet Errors: %lu (DL: %lu, UL: %lu, PRACH: %lu)\n",
          ctx->stats.cplane_err_dup,
          ctx->stats.cplane_err_dup_dl,
          ctx->stats.cplane_err_dup_ul,
          ctx->stats.cplane_err_dup_prach);
  if (ctx->stats.cplane_section_repeated > 0)
    LOG_I(HW, "  C-Plane Repeated sectionIds (first citation used): %lu\n", ctx->stats.cplane_section_repeated);
  if (ctx->stats.uplane_err_early > 0)
    LOG_I(HW, "  U-Plane Timing Early Errors: %lu\n", ctx->stats.uplane_err_early);
  if (ctx->stats.uplane_err_late > 0)
    LOG_I(HW, "  U-Plane Timing Late Errors: %lu\n", ctx->stats.uplane_err_late);
  if (ctx->stats.uplane_err_dup > 0)
    LOG_I(HW, "  U-Plane Duplicate Packet Errors: %lu\n", ctx->stats.uplane_err_dup);
  if (ctx->stats.uplane_missing_cplane > 0)
    LOG_I(HW, "  U-Plane Missing C-Plane Errors: %lu\n", ctx->stats.uplane_missing_cplane);
  if (ctx->stats.dl_stream_pool_exhausted > 0)
    LOG_I(HW, "  DL Stream Pool Exhausted Errors: %lu\n", ctx->stats.dl_stream_pool_exhausted);
  if (ctx->stats.dl_stream_sections_exhausted > 0)
    LOG_I(HW, "  DL Stream Sections Exhausted Errors: %lu\n", ctx->stats.dl_stream_sections_exhausted);
  if (ctx->stats.dl_fragments_pool_exhausted > 0)
    LOG_I(HW, "  DL Fragment Pool Exhausted Errors: %lu\n", ctx->stats.dl_fragments_pool_exhausted);
  if (ctx->stats.invalid_eaxc_id > 0)
    LOG_I(HW, "  Invalid Eaxc/Antenna ID Errors: %lu\n", ctx->stats.invalid_eaxc_id);
  if (ctx->stats.dl_tdd_mismatch + ctx->thread_safe_stats.dl_tdd_mismatch > 0)
    LOG_I(HW, "  DL TDD Mismatch Errors: %lu\n", ctx->stats.dl_tdd_mismatch + ctx->thread_safe_stats.dl_tdd_mismatch);
  if (ctx->stats.ul_tdd_mismatch + ctx->thread_safe_stats.ul_tdd_mismatch > 0)
    LOG_I(HW, "  UL TDD Mismatch Errors: %lu\n", ctx->stats.ul_tdd_mismatch + ctx->thread_safe_stats.ul_tdd_mismatch);
  if (ctx->stats.ul_cplane_missing + ctx->thread_safe_stats.ul_cplane_missing > 0)
    LOG_I(HW, "  UL C-Plane Missing Errors: %lu\n", ctx->stats.ul_cplane_missing + ctx->thread_safe_stats.ul_cplane_missing);
  if (ctx->stats.ul_cplane_err_invalid_num_symbols > 0)
    LOG_I(HW, "  UL C-Plane Invalid numSymbol Errors: %lu\n", ctx->stats.ul_cplane_err_invalid_num_symbols);
  if (ctx->stats.prach_cplane_missing + ctx->thread_safe_stats.prach_cplane_missing > 0)
    LOG_I(HW,
          "  PRACH C-Plane Missing Errors: %lu (Never Received: %lu, Stale: %lu, Early: %lu)\n",
          ctx->stats.prach_cplane_missing + ctx->thread_safe_stats.prach_cplane_missing,
          ctx->stats.prach_cplane_missing_inactive + ctx->thread_safe_stats.prach_cplane_missing_inactive,
          ctx->stats.prach_cplane_missing_stale + ctx->thread_safe_stats.prach_cplane_missing_stale,
          ctx->stats.prach_cplane_missing_early + ctx->thread_safe_stats.prach_cplane_missing_early);
  if (ctx->stats.prach_cplane_missing_ant + ctx->thread_safe_stats.prach_cplane_missing_ant > 0)
    LOG_I(HW,
          "  PRACH Ant C-Plane Missing Errors: %lu\n",
          ctx->stats.prach_cplane_missing_ant + ctx->thread_safe_stats.prach_cplane_missing_ant);
  if (ctx->stats.prach_out_of_mbufs + ctx->thread_safe_stats.prach_out_of_mbufs > 0)
    LOG_I(HW, "  PRACH Out Of Mbufs Errors: %lu\n", ctx->stats.prach_out_of_mbufs + ctx->thread_safe_stats.prach_out_of_mbufs);
  if (ctx->stats.prach_jobs_pool_exhausted + ctx->thread_safe_stats.prach_jobs_pool_exhausted > 0)
    LOG_I(HW,
          "  PRACH Jobs Pool Exhausted Errors: %lu\n",
          ctx->stats.prach_jobs_pool_exhausted + ctx->thread_safe_stats.prach_jobs_pool_exhausted);
  if (ctx->stats.prach_sections_unserved > 0)
    LOG_I(HW, "  PRACH Sections Never Served: %lu\n", ctx->stats.prach_sections_unserved);
  if (ctx->stats.out_of_mbufs + ctx->thread_safe_stats.out_of_mbufs > 0)
    LOG_I(HW, "  Out Of Mbufs Errors: %lu\n", ctx->stats.out_of_mbufs + ctx->thread_safe_stats.out_of_mbufs);
  if (ctx->stats.application_too_slow > 0)
    LOG_I(HW, "  Application Too Slow Errors: %lu\n", ctx->stats.application_too_slow);
  if (ctx->stats.bfw_updates > 0)
    LOG_I(HW, "  Beamforming Weight Updates: %lu\n", ctx->stats.bfw_updates);
  if (ctx->stats.bfw_pool_exhausted > 0)
    LOG_I(HW, "  Beamforming Weight Pool Exhausted Errors: %lu\n", ctx->stats.bfw_pool_exhausted);
  if (ctx->thread_safe_stats.bfw_stale > 0)
    LOG_I(HW, "  Beamforming Weights Stale Errors: %lu\n", (uint64_t)ctx->thread_safe_stats.bfw_stale);

  print_histogram("DL C-Plane", &ctx->stats.dl_cplane_hist, ctx->T2a_max_cp_sym_diff, ctx->T2a_min_cp_sym_diff);
  print_histogram("DL U-Plane", &ctx->stats.dl_uplane_hist, ctx->T2a_max_up_dl_sym_diff, ctx->T2a_min_up_dl_sym_diff);
  print_histogram("UL C-Plane", &ctx->stats.ul_cplane_hist, ctx->T2a_max_cp_sym_diff, ctx->T2a_min_cp_sym_diff);
  print_histogram("PRACH C-Plane", &ctx->stats.prach_cplane_hist, ctx->T2a_max_cp_sym_diff, ctx->T2a_min_cp_sym_diff);
}

void get_packet_processor_stats(void *context, oru_packet_processor_stats_t *out_stats)
{
  oru_packet_processor_context_t *ctx = (oru_packet_processor_context_t *)context;
  if (ctx && out_stats) {
    *out_stats = ctx->stats;
    out_stats->dl_tdd_mismatch += ctx->thread_safe_stats.dl_tdd_mismatch;
    out_stats->ul_tdd_mismatch += ctx->thread_safe_stats.ul_tdd_mismatch;
    out_stats->ul_cplane_missing += ctx->thread_safe_stats.ul_cplane_missing;
    out_stats->prach_cplane_missing += ctx->thread_safe_stats.prach_cplane_missing;
    out_stats->prach_cplane_missing_ant += ctx->thread_safe_stats.prach_cplane_missing_ant;
    out_stats->prach_cplane_missing_inactive += ctx->thread_safe_stats.prach_cplane_missing_inactive;
    out_stats->prach_cplane_missing_stale += ctx->thread_safe_stats.prach_cplane_missing_stale;
    out_stats->prach_cplane_missing_early += ctx->thread_safe_stats.prach_cplane_missing_early;
    out_stats->prach_out_of_mbufs += ctx->thread_safe_stats.prach_out_of_mbufs;
    out_stats->prach_jobs_pool_exhausted += ctx->thread_safe_stats.prach_jobs_pool_exhausted;
    out_stats->out_of_mbufs += ctx->thread_safe_stats.out_of_mbufs;
    out_stats->total_uplane_sent = ctx->thread_safe_stats.total_uplane_sent;
    out_stats->ul_uplane_ota_delay_sum += ctx->thread_safe_stats.ul_uplane_ota_delay_sum;
    out_stats->ul_uplane_ota_delay_count += ctx->thread_safe_stats.ul_uplane_ota_delay_count;
    out_stats->bfw_stale = ctx->thread_safe_stats.bfw_stale;
  }
}

static void unpack_iq(c16_t *txdataF, const uint8_t *iqdata, int start_prb, int num_prb,
                      fh_comp_method_t comp_method, uint8_t iq_width)
{
  if (comp_method != FH_COMP_NONE) {
    fh_decompress_prbs(comp_method, iq_width, num_prb,
                       (const int8_t *)iqdata,
                       (int16_t *)&txdataF[start_prb * NR_NB_SC_PER_RB]);
  } else {
    const uint16_t *source = (const uint16_t *)iqdata;
    uint16_t *destination = (uint16_t *)&txdataF[start_prb * NR_NB_SC_PER_RB];
    for (int j = 0; j < num_prb * NR_NB_SC_PER_RB * 2; j++)
      destination[j] = rte_bswap16(source[j]);
  }
}

int read_dl_iq_streams(void *context,
                       dl_iq_stream_t *streams,
                       uint32_t *iq_arena,
                       int max_streams,
                       uint64_t *hyper_frame,
                       int *frame,
                       int *slot,
                       int *symbol)
{
  oru_packet_processor_context_t *ctx = (oru_packet_processor_context_t *)context;
  if (ctx == NULL)
    return -1;
  dl_symbol_job_t *job;
  int ret = -1;
  while (ret != 0) {
    ret = rte_ring_dequeue(ctx->dl_ready_jobs, (void **)&job);
    rte_pause();
  }

  uint64_t absolute_gps_symbol = job->absolute_symbol;
  int numerology = ctx->numerology;
  int num_symbols_per_frame = NR_NUMBER_OF_SUBFRAMES_PER_FRAME * (1 << numerology) * NR_SYMBOLS_PER_SLOT;
  *hyper_frame = (absolute_gps_symbol / num_symbols_per_frame) / 1024;
  *frame = (absolute_gps_symbol / num_symbols_per_frame) % 1024;
  *slot = (absolute_gps_symbol % num_symbols_per_frame) / NR_SYMBOLS_PER_SLOT;
  *symbol = absolute_gps_symbol % NR_SYMBOLS_PER_SLOT;

  // Each fragment becomes one output stream, packed back to back in iq_arena (start_prb/num_prb are
  // metadata, not an offset into a full-band buffer). add_dl_fragment() kept the total within
  // DL_IQ_ARENA_PRBS(), so the arena cannot overflow.
  int out_count = 0;
  size_t arena_off = 0;
  for (int f = 0; f < job->num_fragments; f++) {
    dl_fragment_t *frag = &job->fragments[f];
    if (out_count >= max_streams) {
      // Caller-sized array too small for what this symbol actually carried - drop the rest rather
      // than overflow. Sizing streams/iq_arena for MAX_DL_FRAGMENTS_PER_SYMBOL avoids this.
      LOG_W(HW, "ORU: read_dl_iq_streams() output too small (max_streams=%d) - dropping remaining fragments\n", max_streams);
      if (frag->mbuf) {
        rte_pktmbuf_free(frag->mbuf);
      }
      continue;
    }
    dl_iq_stream_t *out = &streams[out_count];
    out->ant_id = frag->eaxc_id;
    out->beam_id = frag->beam_id;
    out->start_prb = frag->start_prbc;
    out->num_prb = frag->num_prbc;
    out->bfw = frag->bfw;
    out->iq = iq_arena + arena_off;
    arena_off += (size_t)frag->num_prbc * NR_NB_SC_PER_RB;
    unpack_iq((c16_t *)out->iq, frag->iq_data, 0, frag->num_prbc, frag->comp_method, frag->iq_width);
    if (frag->mbuf) {
      rte_pktmbuf_free(frag->mbuf);
    }
    out_count++;
  }

  ret = rte_ring_enqueue(ctx->dl_free_jobs, (void *)job);
  AssertFatal(ret == 0,
              "Failed to enqueue to ring dl_free_jobs. dl_free_jobs num_elements %d\n",
              rte_ring_count(ctx->dl_free_jobs));
  return out_count;
}

int get_prach_beam_id(void *context, int slot_in_frame, int aarx, int occasion)
{
  oru_bfw_set_t bfw;
  int start_prb;
  return get_prach_bfw_set(context, slot_in_frame, aarx, occasion, &bfw, &start_prb);
}

int get_prach_bfw_set(void *context, int slot_in_frame, int aarx, int occasion, oru_bfw_set_t *bfw, int *start_prb)
{
  oru_packet_processor_context_t *ctx = (oru_packet_processor_context_t *)context;
  if (ctx == NULL || slot_in_frame < 0 || slot_in_frame >= MAX_SLOTS_PER_FRAME || aarx < 0 || aarx >= MAX_ANTENNAS)
    return -1;
  for (int i = 0; i < MAX_PRACH_SECTIONS; i++) {
    prach_job_t *job = &ctx->prach_jobs[slot_in_frame][aarx][i];
    uint32_t seq;
    bool match;
    uint16_t beam_id;
    oru_bfw_set_t set;
    int prb;
    // Retry while the C-Plane thread rewrites the job
    do {
      seq = atomic_load_explicit(&job->seq, memory_order_acquire);
      match = job->active && job->occasion == occasion;
      beam_id = job->beam_id;
      set = job->bfw;
      prb = job->start_prb;
      atomic_thread_fence(memory_order_acquire);
    } while ((seq & 1) || atomic_load_explicit(&job->seq, memory_order_relaxed) != seq);
    if (match) {
      *bfw = set;
      *start_prb = prb;
      return beam_id;
    }
  }
  return -1;
}

int get_bf_weights(void *context, const oru_bfw_set_t *set, int bundle, uint16_t *beam_id, c16_t *weights)
{
  oru_packet_processor_context_t *ctx = (oru_packet_processor_context_t *)context;
  DevAssert(bundle >= 0 && bundle < set->num_bundles);
  uint32_t pos = set->first + bundle;
  oru_bfw_ref_t ref = *bfw_ring_entry(ctx, pos);
  atomic_thread_fence(memory_order_acquire);
  // The entry was overwritten once the ring moved a full turn past it
  if (atomic_load_explicit(&ctx->bfw_ring_reserve, memory_order_relaxed) - pos > ORU_BFW_RING_SIZE)
    goto stale;
  if (ref.gen == 0) {
    *beam_id = ref.beam_id;
    return 0;
  }
  oru_bfw_slot_t *s = &ctx->bfw_slots[ref.slot];
  if (atomic_load_explicit(&s->gen, memory_order_acquire) != ref.gen)
    goto stale;
  int n = ctx->num_bf_weights;
  memcpy(weights, s->w, n * sizeof(*weights));
  atomic_thread_fence(memory_order_acquire);
  if (atomic_load_explicit(&s->gen, memory_order_relaxed) != ref.gen)
    goto stale;
  *beam_id = ref.beam_id;
  return n;
stale:
  atomic_fetch_add_explicit(&ctx->thread_safe_stats.bfw_stale, 1, memory_order_relaxed);
  return -1;
}

int get_ready_job_count(void *context)
{
  oru_packet_processor_context_t *ctx = (oru_packet_processor_context_t *)context;
  if (ctx == NULL)
    return 0;
  return rte_ring_count(ctx->dl_ready_jobs);
}

void fill_ecpri_header(struct xran_ecpri_hdr *ecpri_header,
                       struct xran_eaxcid_config *eaxcid_config,
                       uint8_t ecpri_mesg_type,
                       size_t ecpri_payload_size,
                       uint8_t CC_ID,
                       uint8_t Ant_ID,
                       uint8_t seq_id,
                       uint8_t oxu_port_id)
{
  ecpri_header->cmnhdr.data.data_num_1 = 0x0;
  ecpri_header->cmnhdr.bits.ecpri_ver = XRAN_ECPRI_VER;
  ecpri_header->cmnhdr.bits.ecpri_mesg_type = ecpri_mesg_type;
  ecpri_header->cmnhdr.bits.ecpri_payl_size = rte_cpu_to_be_16(ecpri_payload_size);
  ecpri_header->ecpri_xtc_id = xran_compose_cid(eaxcid_config, 0, 0, CC_ID, Ant_ID);
  ecpri_header->ecpri_seq_id.bits.seq_id = seq_id;
  ecpri_header->ecpri_seq_id.bits.e_bit = 1;
  ecpri_header->ecpri_seq_id.bits.sub_seq_id = 0;
  /// No byteswap for ecpri_seq_id. Possibly because of inverse definition in xran
}

void fill_radio_app_header(struct radio_app_common_hdr *radio_app_header,
                           int filter_id,
                           int direction,
                           int frame,
                           int slot,
                           int symbol,
                           int mu)
{
  radio_app_header->frame_id = frame & 0xff;
  radio_app_header->sf_slot_sym.slot_id = slot % (1 << mu);
  radio_app_header->sf_slot_sym.subframe_id = slot / (1 << mu);
  radio_app_header->sf_slot_sym.symb_id = symbol;
  radio_app_header->sf_slot_sym.value = rte_cpu_to_be_16(radio_app_header->sf_slot_sym.value);
  radio_app_header->data_feature.data_direction = direction;
  radio_app_header->data_feature.payl_ver = 1;
  radio_app_header->data_feature.filter_id = filter_id;
}

void fill_data_section_header(struct data_section_hdr *data_section_hdr, int num_prb, int start_prb, int section_id)
{
  data_section_hdr->fields.all_bits = 0;
  data_section_hdr->fields.num_prbu = (uint8_t)XRAN_CONVERT_NUMPRBC(num_prb);
  data_section_hdr->fields.start_prbu = (start_prb & 0x03ff);
  data_section_hdr->fields.sect_id = section_id;
  data_section_hdr->fields.all_bits = rte_cpu_to_be_32(data_section_hdr->fields.all_bits);
}

int poll_ul_job(void *context, ul_job_t *job)
{
  oru_packet_processor_context_t *ctx = (oru_packet_processor_context_t *)context;
  if (ctx == NULL || job == NULL) {
    return -1;
  }
  ul_job_t *dequeued_job = NULL;
  if (rte_ring_dequeue(ctx->ul_ready_jobs, (void **)&dequeued_job) == 0) {
    *job = *dequeued_job;
    rte_ring_enqueue(ctx->ul_free_jobs, (void *)dequeued_job);
    return 0;
  }
  return -1;
}

void write_ul_iq(void *context, uint32_t *rxdataF, int symbol, const ul_job_t *job)
{
  oru_packet_processor_context_t *ctx = (oru_packet_processor_context_t *)context;
  if (ctx == NULL || job == NULL)
    return;
  AssertFatal(symbol >= job->symbol && symbol < job->symbol + job->num_symbols && symbol < NR_SYMBOLS_PER_SLOT,
              "Symbol %d outside of job range [%d, %d)\n",
              symbol,
              job->symbol,
              job->symbol + job->num_symbols);

  // Delay from OTA symbol to packet send: current timer symbol minus the absolute symbol of this UL symbol.
  const int slots_per_frame = NR_NUMBER_OF_SUBFRAMES_PER_FRAME * (1 << ctx->numerology);
  const uint64_t ota_absolute_symbol = (uint64_t)job->hyper_frame * 1024ULL * slots_per_frame * NR_SYMBOLS_PER_SLOT
                                       + (uint64_t)job->frame * slots_per_frame * NR_SYMBOLS_PER_SLOT
                                       + (uint64_t)job->slot_in_frame * NR_SYMBOLS_PER_SLOT
                                       + (uint64_t)symbol;
  int64_t delay = (int64_t)ctx->current_absolute_symbol - (int64_t)ota_absolute_symbol;
  atomic_fetch_add_explicit(&ctx->thread_safe_stats.ul_uplane_ota_delay_sum, delay, memory_order_relaxed);
  atomic_fetch_add_explicit(&ctx->thread_safe_stats.ul_uplane_ota_delay_count, 1, memory_order_relaxed);

  int aarx = job->antenna_id;
  if (aarx < 0 || aarx >= MAX_ANTENNAS) {
    LOG_W(HW, "ORU: Invalid antenna index %d\n", aarx);
    return;
  }

  const bool use_comp = (job->response_payload.comp_method != 0);

  int section_id = job->response_payload.section_id;
  int total_ul_rbs = job->num_prb;
  int start_prb_base = job->start_prb;
  int frame = job->frame & 0xff;
  int slot_in_frame = job->slot_in_frame;
  int mu = ctx->numerology;

  struct rte_mbuf *mbufs[MAX_MBUFS_PER_SYMBOL];
  uint32_t num_mbufs = 0;

  int rbs_sent = 0;
  size_t overhead = sizeof(struct rte_ether_hdr) + sizeof(struct xran_ecpri_hdr) + sizeof(struct radio_app_common_hdr)
                    + sizeof(struct data_section_hdr);
  if (use_comp) {
    overhead += sizeof(struct data_section_compression_hdr);
  }
  const size_t prb_bytes = use_comp ? (size_t)FH_COMP_PRB_BYTES(job->response_payload.iq_width)
                                    : (size_t)(NR_NB_SC_PER_RB * sizeof(int32_t));
  int max_prb_per_packet = (int)((ctx->mtu - overhead) / prb_bytes);

  while (rbs_sent < total_ul_rbs) {
    int num_ul_rbs = total_ul_rbs - rbs_sent;
    if (num_ul_rbs > max_prb_per_packet) {
      num_ul_rbs = max_prb_per_packet;
    }

    struct rte_mbuf *pkt = ctx->alloc_func(ctx->io_controller);
    if (pkt == NULL) {
      ctx->thread_safe_stats.out_of_mbufs++;
      break;
    }

    size_t header_length = sizeof(struct xran_ecpri_hdr) + sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr);
    if (use_comp) {
      header_length += sizeof(struct data_section_compression_hdr);
    }
    const uint num_sc = num_ul_rbs * NR_NB_SC_PER_RB;
    size_t data_len = use_comp ? (size_t)FH_COMP_PRB_BYTES(job->response_payload.iq_width) * num_ul_rbs
                               : (size_t)sizeof(int32_t) * num_sc;

    char *buf = rte_pktmbuf_append(pkt, (uint16_t)(header_length + data_len));
    if (buf == NULL) {
      LOG_W(HW, "ORU: Failed to append data to mbuf (insufficient space)\n");
      rte_pktmbuf_free(pkt);
      break;
    }

    if (num_mbufs == (MAX_MBUFS_PER_SYMBOL - 1)) {
      ctx->send_func(ctx->io_controller, mbufs, num_mbufs);
      ctx->thread_safe_stats.total_uplane_sent += num_mbufs;
      num_mbufs = 0;
    }
    mbufs[num_mbufs++] = pkt;

    struct xran_ecpri_hdr *ecpri_header = (struct xran_ecpri_hdr *)buf;
    uint16_t ecpri_payload_size = (uint16_t)(header_length - 4 + data_len);
    fill_ecpri_header(ecpri_header, &ctx->eaxcid_config, ECPRI_IQ_DATA, ecpri_payload_size, 0, aarx, ctx->ul_seq_id[aarx]++, 0);

    struct radio_app_common_hdr *radio_app_header = (struct radio_app_common_hdr *)(ecpri_header + 1);
    fill_radio_app_header(radio_app_header, 0, XRAN_DIR_UL, frame, slot_in_frame, symbol, mu);

    struct data_section_hdr *data_section_header = (struct data_section_hdr *)(radio_app_header + 1);
    fill_data_section_header(data_section_header, num_ul_rbs, start_prb_base + rbs_sent, section_id);

    uint8_t *iq_data_start;
    if (use_comp) {
      struct data_section_compression_hdr *compression_header = (struct data_section_compression_hdr *)(data_section_header + 1);
      compression_header->ud_comp_hdr.ud_comp_meth = job->response_payload.comp_method;
      compression_header->ud_comp_hdr.ud_iq_width = XRAN_CONVERT_IQWIDTH(job->response_payload.iq_width);
      compression_header->rsrvd = 0;
      iq_data_start = (uint8_t *)(compression_header + 1);
    } else {
      iq_data_start = (uint8_t *)(data_section_header + 1);
    }

    const int16_t *src = (const int16_t *)&rxdataF[(start_prb_base + rbs_sent) * NR_NB_SC_PER_RB];
    if (use_comp) {
      fh_compress_prbs((fh_comp_method_t)job->response_payload.comp_method,
                       job->response_payload.iq_width,
                       num_ul_rbs,
                       src,
                       (int8_t *)iq_data_start);
    } else {
      const uint16_t *raw = (const uint16_t *)src;
      uint16_t *dst = (uint16_t *)iq_data_start;
      for (int i = 0; i < num_sc * 2; i++)
        dst[i] = rte_cpu_to_be_16(raw[i]);
    }
    rbs_sent += num_ul_rbs;
  }

  if (num_mbufs > 0) {
    ctx->send_func(ctx->io_controller, mbufs, num_mbufs);
    ctx->thread_safe_stats.total_uplane_sent += num_mbufs;
  }
}

void write_prach_iq(void *context, uint32_t **txdataF, int nb_rx, int frame, int slot_in_frame, int symbol, int occasion)
{
  oru_packet_processor_context_t *ctx = (oru_packet_processor_context_t *)context;
  if (ctx == NULL)
    return;

  int numerology = ctx->numerology;
  int num_symbols_per_frame = NR_NUMBER_OF_SUBFRAMES_PER_FRAME * (1 << numerology) * NR_SYMBOLS_PER_SLOT;
  uint32_t current_symbol_in_frame = ctx->current_absolute_symbol % num_symbols_per_frame;
  int symbol_in_frame = NR_SYMBOLS_PER_SLOT * slot_in_frame + symbol;
  int32_t diff = symbol_in_frame - current_symbol_in_frame;
  if (diff < -num_symbols_per_frame / 2) {
    diff += num_symbols_per_frame;
  } else if (diff > num_symbols_per_frame / 2) {
    diff -= num_symbols_per_frame;
  }
  uint64_t target_absolute_symbol = ctx->current_absolute_symbol + diff;
  struct rte_mbuf *mbufs[MAX_MBUFS_PER_SYMBOL];
  uint32_t num_mbufs = 0;

  for (int aarx = 0; aarx < nb_rx; aarx++) {
    if (slot_in_frame < 0 || slot_in_frame >= MAX_SLOTS_PER_FRAME) {
      RATELIMIT(PRACH_ERR_LOG_RATELIMIT, { LOG_W(HW, "PRACH UP: Invalid slot_in_frame %d\n", slot_in_frame); });
      continue;
    }
    prach_job_t *job = find_prach_job(ctx, slot_in_frame, aarx, occasion, target_absolute_symbol);
    if (job == NULL || target_absolute_symbol < job->start_absolute_symbol
        || target_absolute_symbol >= job->start_absolute_symbol + job->num_symbols) {
      ctx->thread_safe_stats.prach_cplane_missing++;
      if (job == NULL) {
        ctx->thread_safe_stats.prach_cplane_missing_inactive++;
        RATELIMIT(PRACH_ERR_LOG_RATELIMIT,
                  { LOG_W(HW, "PRACH UP: Missing C-Plane - Inactive job for slot %d, aarx %d\n", slot_in_frame, aarx); });
      } else if (target_absolute_symbol < job->start_absolute_symbol) {
        ctx->thread_safe_stats.prach_cplane_missing_early++;
        RATELIMIT(PRACH_ERR_LOG_RATELIMIT, {
          LOG_W(HW,
                "PRACH UP: Missing C-Plane - Early symbol %lu (job start %lu) for slot %d, aarx %d\n",
                target_absolute_symbol,
                job->start_absolute_symbol,
                slot_in_frame,
                aarx);
        });
      } else {
        ctx->thread_safe_stats.prach_cplane_missing_stale++;
        RATELIMIT(PRACH_ERR_LOG_RATELIMIT, {
          LOG_W(HW,
                "PRACH UP: Missing C-Plane - Stale symbol %lu (job end %lu) for slot %d, aarx %d\n",
                target_absolute_symbol,
                job->start_absolute_symbol + job->num_symbols,
                slot_in_frame,
                aarx);
        });
      }
      continue;
    }

    int section_id = job->section_id;
    int num_ul_rbs = job->num_prb;
    int start_prb = job->start_prb;
    int filter_id = job->filter_id;

    struct rte_mbuf *pkt = ctx->alloc_func(ctx->io_controller);
    if (pkt == NULL) {
      ctx->thread_safe_stats.prach_out_of_mbufs++;
      ctx->thread_safe_stats.out_of_mbufs++;
      RATELIMIT(PRACH_ERR_LOG_RATELIMIT, { LOG_W(HW, "PRACH UP: Failed to allocate mbuf\n"); });
      continue;
    }

    const bool prach_compressed = job->comp_method != FH_COMP_NONE;
    size_t header_length = sizeof(struct xran_ecpri_hdr) + sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr);
    if (prach_compressed)
      header_length += sizeof(struct data_section_compression_hdr);
    const uint prach_length = 139;
    // O-RAN CUS, 8.3.2: uncompressed U-plane sections carry all 12 complex REs of each advertised PRB.
    size_t data_len = prach_compressed ? (size_t)FH_COMP_PRB_BYTES(job->iq_width) * FH_PRACH_NUM_PRBS
                                       : (size_t)num_ul_rbs * NR_NB_SC_PER_RB * sizeof(c16_t);
    if (!prach_compressed && data_len < (ctx->prach_kbar + prach_length * 2) * sizeof(uint16_t)) {
      rte_pktmbuf_free(pkt);
      continue;
    }

    char *buf = rte_pktmbuf_append(pkt, (uint16_t)(header_length + data_len));
    if (buf == NULL) {
      ctx->thread_safe_stats.prach_out_of_mbufs++;
      ctx->thread_safe_stats.out_of_mbufs++;
      rte_pktmbuf_free(pkt);
      RATELIMIT(PRACH_ERR_LOG_RATELIMIT, { LOG_W(HW, "PRACH UP: Failed to append data to mbuf\n"); });
      continue;
    }

    mbufs[num_mbufs++] = pkt;
    job->served = true;

    struct xran_ecpri_hdr *ecpri_header = (struct xran_ecpri_hdr *)buf;
    uint16_t ecpri_payload_size = (uint16_t)(header_length - 4 + data_len);

    fill_ecpri_header(ecpri_header,
                      &ctx->eaxcid_config,
                      ECPRI_IQ_DATA,
                      ecpri_payload_size,
                      0,
                      aarx + ctx->prach_eaxc_offset,
                      ctx->ul_seq_id[(aarx + ctx->prach_eaxc_offset) & (NUM_RU_PORT_IDS - 1)]++,
                      0);

    struct radio_app_common_hdr *radio_app_header = (struct radio_app_common_hdr *)(ecpri_header + 1);
    fill_radio_app_header(radio_app_header,
                          filter_id,
                          XRAN_DIR_UL,
                          frame & 0xff,
                          slot_in_frame,
                          symbol,
                          numerology);

    struct data_section_hdr *data_section_header = (struct data_section_hdr *)(radio_app_header + 1);
    fill_data_section_header(data_section_header, num_ul_rbs, start_prb, section_id);

    uint8_t *iq_data_start;
    if (prach_compressed) {
      struct data_section_compression_hdr *comp_hdr =
          (struct data_section_compression_hdr *)(data_section_header + 1);
      comp_hdr->ud_comp_hdr.ud_comp_meth = job->comp_method;
      comp_hdr->ud_comp_hdr.ud_iq_width = XRAN_CONVERT_IQWIDTH(job->iq_width);
      comp_hdr->rsrvd = 0;
      iq_data_start = (uint8_t *)(comp_hdr + 1);
      fh_compress_prach(job->comp_method, job->iq_width, ctx->prach_kbar, (const int16_t *)txdataF[aarx], (int8_t *)iq_data_start);
    } else {
      iq_data_start = (uint8_t *)(data_section_header + 1);
      const uint16_t *raw = (const uint16_t *)txdataF[aarx];
      uint16_t *dst = (uint16_t *)iq_data_start;
      memset(dst, 0, data_len);
      for (int i = 0; i < prach_length * 2; i++)
        dst[ctx->prach_kbar + i] = rte_cpu_to_be_16(raw[i]);
    }
  }

  if (num_mbufs > 0) {
    ctx->send_func(ctx->io_controller, mbufs, num_mbufs);
    ctx->thread_safe_stats.total_uplane_sent += num_mbufs;
  }
}
