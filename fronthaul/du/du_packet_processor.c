#include "du_packet_processor.h"
#include "xran_pkt_api.h"
#include "fh_compression.h"
#include <rte_byteorder.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "assertions.h"
#include "log.h"
#include <rte_ring.h>
#include <rte_mbuf.h>
#include "common/utils/nr/nr_common.h"

#define MAX_ANTENNAS 4
#define MAX_RX_FRAGMENTS 4
#define NUM_CONCURRENT_UL_SYMBOL_WINDOWS 1024
#define UL_JOB_RING_SIZE NUM_CONCURRENT_UL_SYMBOL_WINDOWS

#define NR_NUMBER_OF_SUBFRAMES_PER_FRAME 10
#define MAX_TDD_PATTERN_LENGTH_MS 10
#define MAX_SLOTS_PER_MS 4
#define SYMBOL_BITMASK_SIZE ((NR_SYMBOLS_PER_SLOT * MAX_TDD_PATTERN_LENGTH_MS * MAX_SLOTS_PER_MS + 7) / 8)

#define XRAN_IQ_BITS_UNCOMPRESSED 16

#define MAX_SLOTS_PER_FRAME 160
#define PRACH_JOB_RING_SIZE 1024

typedef struct {
  bool expected;
  bool completed;
  uint64_t start_absolute_symbol;
  uint32_t num_symbols;
  int slot_in_frame;
  int antenna_id;
  int section_id;
  int start_prb;
  int num_prb;
  fh_comp_method_t comp_method;
  uint8_t iq_width;
  int kbar;

  int16_t accumulated_iq[FH_PRACH_NUM_PRBS * 24]; // 24 values per PRB
} prach_job_t;

typedef struct {
  struct {
    bool expected;
    int section_id;
    struct {
      int start_prbc;
      int num_prbc;
      void *iq_data;
      void *mbuf;
    } rx_fragments[MAX_RX_FRAGMENTS];
    int num_rx_fragments;
  } per_antenna[MAX_ANTENNAS];
  int expected_iq;
  int received_iq;
  uint64_t absolute_symbol;
  fh_comp_method_t comp_method;
  uint8_t iq_width;
} ul_symbol_job_t;

typedef struct {
  ul_symbol_job_t ul_symbol_jobs[UL_JOB_RING_SIZE - 1];
  ul_symbol_job_t *ul_symbol_rx_window[NUM_CONCURRENT_UL_SYMBOL_WINDOWS];
  bool was_ul_symbol_completed[NUM_CONCURRENT_UL_SYMBOL_WINDOWS];
  
  uint64_t current_absolute_symbol;
  uint64_t window_tail_symbol;
  
  struct rte_ring *ul_free_jobs;
  struct rte_ring *ul_ready_jobs;
  ul_symbol_job_t *ul_held_job; // dequeued by du_pp_read_ul_iq_upto() but past its bound
  
  prach_job_t prach_jobs[MAX_SLOTS_PER_FRAME][MAX_ANTENNAS];
  uint64_t prach_window_tail_symbol;
  struct rte_ring *prach_ready_jobs;
  
  uint32_t Ta3_min_sym_diff;
  uint32_t Ta3_max_sym_diff;
  
  struct xran_eaxcid_config eaxcid_config;
  int prach_eaxc_offset;
  
  int numerology;
  int num_prb;
  size_t mtu;
  bool fdd_mode;
  
  du_packet_processor_stats_t stats;
  
  uint8_t ul_symbol_bitmask[SYMBOL_BITMASK_SIZE];
  uint16_t symbol_bitmask_length;
} du_packet_processor_context_t;

static inline void set_bit(uint8_t *bits, uint64_t bit) {
  bits[bit / 8] |= 1 << (bit % 8);
}

static inline int test_bit(const uint8_t *bits, uint64_t bit) {
  return bits[bit / 8] & (1 << (bit % 8));
}

static void txrx_window_histogram_count(txrx_histogram_t *hist, int32_t diff) {
  int bin = diff + HIST_SIZE / 2;
  bin = bin < 0 ? 0 : bin > HIST_SIZE - 1 ? HIST_SIZE - 1 : bin;
  hist->sum += diff;
  hist->count++;
  hist->hist[bin]++;
}

void *init_du_packet_processor(int numerology,
                               int num_prb,
                               uint32_t Ta3_min_uS,
                               uint32_t Ta3_max_uS,
                               int num_ul_slots,
                               int num_ul_symbols,
                               int tdd_pattern_length_slots,
                               bool fdd_mode,
                               size_t mtu,
                               int prach_eaxc_offset)
{
  du_packet_processor_context_t *ctx = calloc(1, sizeof(*ctx));
  ctx->current_absolute_symbol = 0;
  ctx->num_prb = num_prb;
  ctx->numerology = numerology;
  ctx->mtu = mtu;
  ctx->prach_eaxc_offset = prach_eaxc_offset;
  ctx->fdd_mode = fdd_mode;
  
  uint32_t slots_per_subframe = 1 << numerology;
  uint32_t symbol_duration_uS = 1000 / slots_per_subframe / NR_SYMBOLS_PER_SLOT;
  ctx->Ta3_min_sym_diff = Ta3_min_uS / symbol_duration_uS;
  ctx->Ta3_max_sym_diff = Ta3_max_uS / symbol_duration_uS;
  
  ctx->ul_ready_jobs = rte_ring_create("du_ul_ready_jobs", UL_JOB_RING_SIZE, rte_socket_id(), 0);
  AssertFatal(ctx->ul_ready_jobs != NULL, "Failed to create ring du_ul_ready_jobs\n");
  ctx->ul_free_jobs = rte_ring_create("du_ul_free_jobs", UL_JOB_RING_SIZE, rte_socket_id(), 0);
  AssertFatal(ctx->ul_free_jobs != NULL, "Failed to create ring du_ul_free_jobs\n");
  for (int i = 0; i < UL_JOB_RING_SIZE - 1; i++) {
    rte_ring_enqueue(ctx->ul_free_jobs, (void *)&ctx->ul_symbol_jobs[i]);
  }
  
  ctx->prach_ready_jobs = rte_ring_create("du_prach_ready_jobs", PRACH_JOB_RING_SIZE, rte_socket_id(), 0);
  AssertFatal(ctx->prach_ready_jobs != NULL, "Failed to create ring du_prach_ready_jobs\n");
  
  ctx->eaxcid_config = (struct xran_eaxcid_config){.mask_cuPortId = 0xF000,
                                                   .mask_bandSectorId = 0x0F00,
                                                   .mask_ccId = 0x00F0,
                                                   .mask_ruPortId = 0x000F,
                                                   .bit_cuPortId = 12,
                                                   .bit_bandSectorId = 8,
                                                   .bit_ccId = 4,
                                                   .bit_ruPortId = 0};

  ctx->symbol_bitmask_length = tdd_pattern_length_slots * NR_SYMBOLS_PER_SLOT;
  
  if (fdd_mode) {
    memset(ctx->ul_symbol_bitmask, 0xFF, SYMBOL_BITMASK_SIZE);
  } else {
    int last_bit = ctx->symbol_bitmask_length - 1;
    for (int i = 0; i < num_ul_slots * NR_SYMBOLS_PER_SLOT + num_ul_symbols; i++) {
      set_bit(ctx->ul_symbol_bitmask, last_bit - i);
    }
  }
  
  return ctx;
}

void cleanup_du_packet_processor(void *context)
{
  du_packet_processor_context_t *ctx = (du_packet_processor_context_t *)context;
  if (ctx) {
    du_pp_print_stats(ctx);
    if (ctx->ul_ready_jobs) {
      rte_ring_free(ctx->ul_ready_jobs);
    }
    if (ctx->ul_free_jobs) {
      rte_ring_free(ctx->ul_free_jobs);
    }
    if (ctx->prach_ready_jobs) {
      rte_ring_free(ctx->prach_ready_jobs);
    }
    free(ctx);
  }
}

static void release_completed_symbol_job(du_packet_processor_context_t *ctx, uint64_t absolute_symbol)
{
  uint32_t job_index = absolute_symbol % NUM_CONCURRENT_UL_SYMBOL_WINDOWS;
  ul_symbol_job_t *job = ctx->ul_symbol_rx_window[job_index];
  if (!job || job->absolute_symbol != absolute_symbol) {
    return;
  }
  ctx->ul_symbol_rx_window[job_index] = NULL;
  ctx->was_ul_symbol_completed[job_index] = true;
  int ret = rte_ring_enqueue(ctx->ul_ready_jobs, (void *)job);
  if (ret != 0) {
    ctx->stats.application_too_slow++;
  }
}

static void push_symbol_job(du_packet_processor_context_t *ctx, uint64_t absolute_symbol)
{
  while (ctx->window_tail_symbol <= absolute_symbol) {
    if (!test_bit(ctx->ul_symbol_bitmask, ctx->window_tail_symbol % ctx->symbol_bitmask_length)) {
      ctx->window_tail_symbol++;
      continue;
    }

    uint32_t job_index = ctx->window_tail_symbol % NUM_CONCURRENT_UL_SYMBOL_WINDOWS;
    ul_symbol_job_t *job = ctx->ul_symbol_rx_window[job_index];
    if (job) {
      ctx->ul_symbol_rx_window[job_index] = NULL;
      ctx->was_ul_symbol_completed[job_index] = false;
      int ret = rte_ring_enqueue(ctx->ul_ready_jobs, (void *)job);
      if (ret != 0) {
        ctx->stats.application_too_slow++;
      }
    } else if (ctx->was_ul_symbol_completed[job_index]) {
      ctx->was_ul_symbol_completed[job_index] = false;
    } else {
      int ret = rte_ring_dequeue(ctx->ul_free_jobs, (void **)&job);
      if (ret != 0) {
        ctx->stats.application_too_slow++;
        return;
      }
      memset(job, 0, sizeof(*job));
      job->absolute_symbol = ctx->window_tail_symbol;
      ret = rte_ring_enqueue(ctx->ul_ready_jobs, (void *)job);
      if (ret != 0) {
        ctx->stats.application_too_slow++;
      }
    }
    ctx->window_tail_symbol++;
  }
}

void du_pp_handle_absolute_symbol_tick(void *context, uint64_t absolute_symbol)
{
  du_packet_processor_context_t *ctx = (du_packet_processor_context_t *)context;
  if (ctx->current_absolute_symbol == 0) {
    ctx->current_absolute_symbol = absolute_symbol - 1;
    ctx->window_tail_symbol = absolute_symbol > ctx->Ta3_max_sym_diff ? absolute_symbol - ctx->Ta3_max_sym_diff : 0;
    ctx->prach_window_tail_symbol = ctx->window_tail_symbol;
  }
  ctx->current_absolute_symbol = absolute_symbol;
  
  if (ctx->current_absolute_symbol >= ctx->Ta3_max_sym_diff) {
    uint64_t window_expiry_symbol = ctx->current_absolute_symbol - ctx->Ta3_max_sym_diff;
    push_symbol_job(ctx, window_expiry_symbol);
    
    while (ctx->prach_window_tail_symbol <= window_expiry_symbol) {
      uint64_t sym = ctx->prach_window_tail_symbol;
      int slots_per_subframe = 1 << ctx->numerology;
      int num_slots_per_frame = NR_NUMBER_OF_SUBFRAMES_PER_FRAME * slots_per_subframe;
      int slot_idx = (sym / NR_SYMBOLS_PER_SLOT) % num_slots_per_frame;
      int prev_slot_idx = (slot_idx + num_slots_per_frame - 1) % num_slots_per_frame;
      
      for (int s = 0; s < 2; s++) {
        int check_slot = (s == 0) ? slot_idx : prev_slot_idx;
        for (int ant = 0; ant < MAX_ANTENNAS; ant++) {
          prach_job_t *job = &ctx->prach_jobs[check_slot][ant];
          if (job->expected && !job->completed) {
            if (job->start_absolute_symbol + job->num_symbols - 1 == sym) {
              job->completed = true;
              int ret = rte_ring_enqueue(ctx->prach_ready_jobs, (void *)job);
              if (ret != 0) {
                ctx->stats.application_too_slow++;
              }
            }
          }
        }
      }
      ctx->prach_window_tail_symbol++;
    }
  }
}

void du_pp_handle_uplane_packet(void *context, void *pkt)
{
  du_packet_processor_context_t *ctx = (du_packet_processor_context_t *)context;
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

  // The DU already knows what it scheduled (via du_pp_expect_ul_symbol), including
  // per-symbol comp_method/iq_width, so unlike the RU there's no single global
  // compression config. We need frame/slot/symbol to look the job up *before* we
  // can know which comp_method to pass to xran_extract_iq_samples() (expect_comp
  // only affects the IQ payload offset, not the frame/slot/symbol fields, so a
  // non-destructive manual read of the ecpri+radio_app headers first, then a single
  // call to xran_extract_iq_samples() once the job's comp_method is known, is safe).
  struct xran_ecpri_hdr *ecpri_hdr;
  struct xran_recv_packet_info pkt_info;
  int ret = xran_parse_ecpri_hdr(pkt, &ecpri_hdr, &pkt_info);
  if (ret != 0) {
    rte_pktmbuf_free(pkt);
    return;
  }
  
  uint8_t cu_port_id, band_sector_id, cc_id, ant_id;
  xran_decompose_cid(ecpri_hdr->ecpri_xtc_id, &ctx->eaxcid_config, &cu_port_id, &band_sector_id, &cc_id, &ant_id);
  
  struct radio_app_common_hdr *apphdr = (struct radio_app_common_hdr *)(ecpri_hdr + 1);
  frame_id = apphdr->frame_id;
  
  union {
    uint16_t value;
    struct {
      uint16_t symb_id: 6;
      uint16_t slot_id: 6;
      uint16_t subframe_id: 4;
    };
  } sf_slot_sym_local;
  sf_slot_sym_local.value = rte_be_to_cpu_16(apphdr->sf_slot_sym.value);
  
  subframe_id = sf_slot_sym_local.subframe_id;
  slot_id = sf_slot_sym_local.slot_id;
  symb_id = sf_slot_sym_local.symb_id;
  
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
  
  int32_t symbols_since_ota = -diff;
  txrx_window_histogram_count(&ctx->stats.ul_uplane_hist, symbols_since_ota);
  
  if (symbols_since_ota < (int32_t)ctx->Ta3_min_sym_diff) {
    ctx->stats.uplane_err_early++;
    rte_pktmbuf_free(pkt);
    return;
  }
  
  if (symbols_since_ota > (int32_t)ctx->Ta3_max_sym_diff) {
    ctx->stats.uplane_err_late++;
    rte_pktmbuf_free(pkt);
    return;
  }
  
  uint64_t target_absolute_symbol = ctx->current_absolute_symbol + diff;
  bool is_ul_symbol = test_bit(ctx->ul_symbol_bitmask, target_absolute_symbol % ctx->symbol_bitmask_length);
  if (!is_ul_symbol) {
    ctx->stats.ul_tdd_mismatch++;
    rte_pktmbuf_free(pkt);
    return;
  }
  
  uint32_t job_index = target_absolute_symbol % NUM_CONCURRENT_UL_SYMBOL_WINDOWS;
  ul_symbol_job_t *job = ctx->ul_symbol_rx_window[job_index];
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

  // Now we can use job->comp_method to extract IQ properly!
  bool has_comp_hdr = (job->comp_method != FH_COMP_NONE);
  uint8_t staticComp = 0;
  uint8_t compMeth = 0;
  uint8_t iqWidth = 0;
  ret = xran_extract_iq_samples(pkt,
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
    LOG_W(HW, "Error reading UL packet\n");
    rte_pktmbuf_free(pkt);
    return;
  }
  
  if (Ant_ID >= MAX_ANTENNAS) {
    LOG_W(HW, "Antenna id (%d) exceeds supported value %d\n", Ant_ID, MAX_ANTENNAS);
    rte_pktmbuf_free(pkt);
    return;
  }
  
  if (!job->per_antenna[Ant_ID].expected) {
    rte_pktmbuf_free(pkt);
    return;
  }
  
  if (job->per_antenna[Ant_ID].num_rx_fragments < MAX_RX_FRAGMENTS) {
    int frag_idx = job->per_antenna[Ant_ID].num_rx_fragments++;
    job->per_antenna[Ant_ID].rx_fragments[frag_idx].iq_data = iq_data_start;
    job->per_antenna[Ant_ID].rx_fragments[frag_idx].mbuf = pkt;
    job->per_antenna[Ant_ID].rx_fragments[frag_idx].start_prbc = start_prbu;
    job->per_antenna[Ant_ID].rx_fragments[frag_idx].num_prbc = num_prbu == 0 ? ctx->num_prb : num_prbu;
  } else {
    LOG_W(HW, "ODU: Dropping extra segment for Ant %d, sym %lu\n", Ant_ID, target_absolute_symbol);
    rte_pktmbuf_free(pkt);
  }
  
  job->received_iq += num_prbu == 0 ? ctx->num_prb : num_prbu;
  if (job->expected_iq == job->received_iq) {
    release_completed_symbol_job(ctx, target_absolute_symbol);
  }
}

void du_pp_expect_ul_symbol(void *context, uint64_t absolute_symbol, int ant_id, int section_id, int start_prb, int num_prb, fh_comp_method_t comp_method, uint8_t iq_width)
{
  du_packet_processor_context_t *ctx = (du_packet_processor_context_t *)context;
  uint32_t job_index = absolute_symbol % NUM_CONCURRENT_UL_SYMBOL_WINDOWS;
  ul_symbol_job_t *job = ctx->ul_symbol_rx_window[job_index];
  
  if (!job) {
    int ret = rte_ring_dequeue(ctx->ul_free_jobs, (void **)&job);
    if (ret != 0) {
      ctx->stats.application_too_slow++;
      return;
    }
    job->absolute_symbol = absolute_symbol;
    job->expected_iq = 0;
    job->received_iq = 0;
    job->comp_method = comp_method;
    job->iq_width = iq_width;
    for (int j = 0; j < MAX_ANTENNAS; j++) {
      job->per_antenna[j].expected = false;
      job->per_antenna[j].num_rx_fragments = 0;
      for (int k = 0; k < MAX_RX_FRAGMENTS; k++) {
        job->per_antenna[j].rx_fragments[k].iq_data = NULL;
        job->per_antenna[j].rx_fragments[k].mbuf = NULL;
      }
    }
    ctx->ul_symbol_rx_window[job_index] = job;
    ctx->was_ul_symbol_completed[job_index] = false;
  } else if (job->absolute_symbol != absolute_symbol) {
    // Should not happen unless window is too small or ring wraps
    return;
  }
  
  job->per_antenna[ant_id].expected = true;
  job->per_antenna[ant_id].section_id = section_id;
  job->expected_iq += num_prb;
}

int du_pp_get_ready_ul_job_count(void *context)
{
  du_packet_processor_context_t *ctx = (du_packet_processor_context_t *)context;
  if (ctx == NULL)
    return 0;
  return rte_ring_count(ctx->ul_ready_jobs) + (ctx->ul_held_job != NULL);
}

static void unpack_iq(uint32_t *rxdataF, const uint8_t *iqdata, int start_prb, int num_prb,
                      fh_comp_method_t comp_method, uint8_t iq_width)
{
  if (comp_method != FH_COMP_NONE) {
    fh_decompress_prbs(comp_method, iq_width, num_prb,
                       (const int8_t *)iqdata,
                       (int16_t *)&rxdataF[start_prb * NR_NB_SC_PER_RB]);
  } else {
    const uint16_t *source = (const uint16_t *)iqdata;
    uint16_t *destination = (uint16_t *)&rxdataF[start_prb * NR_NB_SC_PER_RB];
    for (int j = 0; j < num_prb * NR_NB_SC_PER_RB * 2; j++)
      destination[j] = rte_bswap16(source[j]);
  }
}

static void unpack_ul_job(du_packet_processor_context_t *ctx, ul_symbol_job_t *job, uint32_t **rxdataF, int nb_rx)
{
  for (int aatx = 0; aatx < nb_rx; aatx++) {
    memset(rxdataF[aatx], 0, ctx->num_prb * NR_NB_SC_PER_RB * sizeof(uint32_t));
    if (job->per_antenna[aatx].num_rx_fragments == 0) {
      continue;
    }
    for (int k = 0; k < job->per_antenna[aatx].num_rx_fragments; k++) {
      unpack_iq(rxdataF[aatx],
                job->per_antenna[aatx].rx_fragments[k].iq_data,
                job->per_antenna[aatx].rx_fragments[k].start_prbc,
                job->per_antenna[aatx].rx_fragments[k].num_prbc,
                job->comp_method,
                job->iq_width);
      if (job->per_antenna[aatx].rx_fragments[k].mbuf) {
        rte_pktmbuf_free(job->per_antenna[aatx].rx_fragments[k].mbuf);
      }
    }
  }
  int ret = rte_ring_enqueue(ctx->ul_free_jobs, (void *)job);
  AssertFatal(ret == 0,
              "Failed to enqueue to ring du_ul_free_jobs. du_ul_free_jobs num_elements %d\n",
              rte_ring_count(ctx->ul_free_jobs));
}

void du_pp_read_ul_iq(void *context, uint32_t **rxdataF, int nb_rx, uint64_t *hyper_frame, int *frame, int *slot, int *symbol)
{
  du_packet_processor_context_t *ctx = (du_packet_processor_context_t *)context;
  if (ctx == NULL)
    return;
  ul_symbol_job_t *job = ctx->ul_held_job;
  ctx->ul_held_job = NULL;
  while (job == NULL) {
    if (rte_ring_dequeue(ctx->ul_ready_jobs, (void **)&job) != 0)
      job = NULL;
    rte_pause();
  }

  uint64_t absolute_gps_symbol = job->absolute_symbol;
  int numerology = ctx->numerology;
  int num_symbols_per_frame = NR_NUMBER_OF_SUBFRAMES_PER_FRAME * (1 << numerology) * NR_SYMBOLS_PER_SLOT;
  *hyper_frame = (absolute_gps_symbol / num_symbols_per_frame) / 1024;
  *frame = (absolute_gps_symbol / num_symbols_per_frame) % 1024;
  *slot = (absolute_gps_symbol % num_symbols_per_frame) / NR_SYMBOLS_PER_SLOT;
  *symbol = absolute_gps_symbol % NR_SYMBOLS_PER_SLOT;
  unpack_ul_job(ctx, job, rxdataF, nb_rx);
}

bool du_pp_read_ul_iq_upto(void *context, uint32_t **rxdataF, int nb_rx, uint64_t last_absolute_symbol, uint64_t *absolute_symbol)
{
  du_packet_processor_context_t *ctx = (du_packet_processor_context_t *)context;
  ul_symbol_job_t *job = ctx->ul_held_job;
  ctx->ul_held_job = NULL;
  if (job == NULL && rte_ring_dequeue(ctx->ul_ready_jobs, (void **)&job) != 0)
    return false;
  if (job->absolute_symbol > last_absolute_symbol) {
    ctx->ul_held_job = job;
    return false;
  }
  *absolute_symbol = job->absolute_symbol;
  unpack_ul_job(ctx, job, rxdataF, nb_rx);
  return true;
}

uint32_t du_pp_get_ul_window_symbols(void *context)
{
  du_packet_processor_context_t *ctx = (du_packet_processor_context_t *)context;
  return ctx->Ta3_max_sym_diff;
}

void du_pp_expect_prach_occasion(void *context, uint64_t start_absolute_symbol, int num_symbols, int slot_in_frame, int ant_id, int section_id, int start_prb, int num_prb, fh_comp_method_t comp_method, uint8_t iq_width, int kbar)
{
  du_packet_processor_context_t *ctx = (du_packet_processor_context_t *)context;
  if (!ctx || slot_in_frame < 0 || slot_in_frame >= MAX_SLOTS_PER_FRAME || ant_id < 0 || ant_id >= MAX_ANTENNAS) return;
  
  prach_job_t *job = &ctx->prach_jobs[slot_in_frame][ant_id];
  job->expected = true;
  job->completed = false;
  job->start_absolute_symbol = start_absolute_symbol;
  job->num_symbols = num_symbols;
  job->slot_in_frame = slot_in_frame;
  job->antenna_id = ant_id;
  job->section_id = section_id;
  job->start_prb = start_prb;
  job->num_prb = num_prb;
  job->comp_method = comp_method;
  job->iq_width = iq_width;
  job->kbar = kbar;
  memset(job->accumulated_iq, 0, sizeof(job->accumulated_iq));
}

void du_pp_handle_prach_uplane_packet(void *context, void *pkt)
{
  du_packet_processor_context_t *ctx = (du_packet_processor_context_t *)context;
  ctx->stats.total_prach_uplane_received++;
  
  void *iq_data_start = NULL;
  uint8_t CC_ID = 0xFF, Ant_ID = 0xFF, frame_id, subframe_id, slot_id, symb_id, filter_id;
  union ecpri_seq_id seq_id;
  uint16_t num_prbu, start_prbu, sym_inc, rb, sect_id;
  
  struct xran_ecpri_hdr *ecpri_hdr;
  struct xran_recv_packet_info pkt_info;
  int ret = xran_parse_ecpri_hdr(pkt, &ecpri_hdr, &pkt_info);
  if (ret != 0) {
    rte_pktmbuf_free(pkt);
    return;
  }
  
  uint8_t cu_port_id, band_sector_id, cc_id, ant_id_raw;
  xran_decompose_cid(ecpri_hdr->ecpri_xtc_id, &ctx->eaxcid_config, &cu_port_id, &band_sector_id, &cc_id, &ant_id_raw);

  struct radio_app_common_hdr *apphdr = (struct radio_app_common_hdr *)(ecpri_hdr + 1);
  frame_id = apphdr->frame_id;
  union {
    uint16_t value;
    struct {
      uint16_t symb_id: 6;
      uint16_t slot_id: 6;
      uint16_t subframe_id: 4;
    };
  } sf_slot_sym_local;
  sf_slot_sym_local.value = rte_be_to_cpu_16(apphdr->sf_slot_sym.value);
  
  subframe_id = sf_slot_sym_local.subframe_id;
  slot_id = sf_slot_sym_local.slot_id;
  symb_id = sf_slot_sym_local.symb_id;
  
  int mu = ctx->numerology;
  int slots_per_subframe = 1 << mu;
  int num_symbols_per_frame = NR_NUMBER_OF_SUBFRAMES_PER_FRAME * slots_per_subframe * NR_SYMBOLS_PER_SLOT;
  uint32_t current_symbol_in_frame = ctx->current_absolute_symbol % num_symbols_per_frame;
  int symbol_in_frame = NR_SYMBOLS_PER_SLOT * (slot_id + subframe_id * slots_per_subframe) + symb_id;
  int slot_in_frame = slot_id + subframe_id * slots_per_subframe;
  uint8_t current_frame_id = (ctx->current_absolute_symbol / num_symbols_per_frame) % 256;
  
  int frame_diff = (int)frame_id - (int)current_frame_id;
  if (frame_diff < -128) frame_diff += 256;
  else if (frame_diff > 127) frame_diff -= 256;
  
  int32_t diff = frame_diff * num_symbols_per_frame + symbol_in_frame - (int32_t)current_symbol_in_frame;
  int32_t symbols_since_ota = -diff;
  txrx_window_histogram_count(&ctx->stats.prach_uplane_hist, symbols_since_ota);
  
  if (symbols_since_ota < (int32_t)ctx->Ta3_min_sym_diff) {
    ctx->stats.prach_uplane_err_early++;
    rte_pktmbuf_free(pkt);
    return;
  }
  if (symbols_since_ota > (int32_t)ctx->Ta3_max_sym_diff) {
    ctx->stats.prach_uplane_err_late++;
    rte_pktmbuf_free(pkt);
    return;
  }
  
  uint64_t target_absolute_symbol = ctx->current_absolute_symbol + diff;
  int aarx = ant_id_raw - ctx->prach_eaxc_offset;
  if (aarx < 0 || aarx >= MAX_ANTENNAS || slot_in_frame < 0 || slot_in_frame >= MAX_SLOTS_PER_FRAME) {
    rte_pktmbuf_free(pkt);
    return;
  }
  
  prach_job_t *job = &ctx->prach_jobs[slot_in_frame][aarx];
  if (!job->expected || job->completed || target_absolute_symbol < job->start_absolute_symbol || target_absolute_symbol >= job->start_absolute_symbol + job->num_symbols) {
    ctx->stats.prach_missing_expect_job++;
    rte_pktmbuf_free(pkt);
    return;
  }
  
  bool has_comp_hdr = (job->comp_method != FH_COMP_NONE);
  uint8_t staticComp = 0, compMeth = 0, iqWidth = 0;
  ret = xran_extract_iq_samples(pkt, &ctx->eaxcid_config, &iq_data_start, &CC_ID, &Ant_ID, &frame_id, &subframe_id, &slot_id, &symb_id, &filter_id, &seq_id, &num_prbu, &start_prbu, &sym_inc, &rb, &sect_id, has_comp_hdr, staticComp, &compMeth, &iqWidth);
  if (ret == 0) {
    rte_pktmbuf_free(pkt);
    return;
  }
  
  int16_t tmp_buf[FH_PRACH_NUM_PRBS * FH_VALS_PER_PRB] = {0};
  if (job->comp_method != FH_COMP_NONE) {
    fh_decompress_prbs(job->comp_method, job->iq_width, FH_PRACH_NUM_PRBS, (const int8_t *)iq_data_start, tmp_buf);
  } else {
    const uint16_t *source = (const uint16_t *)iq_data_start;
    uint16_t *destination = (uint16_t *)tmp_buf;
    int payload_len_shorts = (139 + job->kbar) * 2; 
    if (payload_len_shorts > FH_PRACH_NUM_PRBS * FH_VALS_PER_PRB) {
      payload_len_shorts = FH_PRACH_NUM_PRBS * FH_VALS_PER_PRB;
    }
    for (int j = 0; j < payload_len_shorts; j++) {
      destination[j] = rte_bswap16(source[j]);
    }
  }
  
  for (int j = 0; j < FH_PRACH_NUM_PRBS * FH_VALS_PER_PRB; j++) {
    job->accumulated_iq[j] += tmp_buf[j];
  }
  
  rte_pktmbuf_free(pkt);
}

int du_pp_get_ready_prach_job_count(void *context)
{
  du_packet_processor_context_t *ctx = (du_packet_processor_context_t *)context;
  if (ctx == NULL) return 0;
  return rte_ring_count(ctx->prach_ready_jobs);
}

void du_pp_read_prach_iq(void *context, int16_t *rxdata, uint64_t *hyper_frame, int *frame, int *slot, int *antenna, int *section_id)
{
  du_packet_processor_context_t *ctx = (du_packet_processor_context_t *)context;
  if (ctx == NULL) return;
  prach_job_t *job;
  int ret = -1;
  while (ret != 0) {
    ret = rte_ring_dequeue(ctx->prach_ready_jobs, (void **)&job);
    rte_pause();
  }

  uint64_t absolute_gps_symbol = job->start_absolute_symbol;
  int numerology = ctx->numerology;
  int num_symbols_per_frame = NR_NUMBER_OF_SUBFRAMES_PER_FRAME * (1 << numerology) * NR_SYMBOLS_PER_SLOT;
  if (hyper_frame) *hyper_frame = (absolute_gps_symbol / num_symbols_per_frame) / 1024;
  if (frame) *frame = (absolute_gps_symbol / num_symbols_per_frame) % 1024;
  if (slot) *slot = (absolute_gps_symbol % num_symbols_per_frame) / NR_SYMBOLS_PER_SLOT;
  if (antenna) *antenna = job->antenna_id;
  if (section_id) *section_id = job->section_id;

  for (int i = 0; i < 139 * 2; i++) {
    rxdata[i] = job->accumulated_iq[job->kbar + i];
  }
}

void du_pp_get_stats(void *context, du_packet_processor_stats_t *out)
{
  du_packet_processor_context_t *ctx = (du_packet_processor_context_t *)context;
  if (ctx && out) {
    *out = ctx->stats;
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
        break;
      }
    }
  }
  LOG_I(HW, "%s\n", buf);
  memset(hist, 0, sizeof(*hist));
}

void du_pp_print_stats(void *context)
{
  du_packet_processor_context_t *ctx = (du_packet_processor_context_t *)context;
  if (ctx == NULL)
    return;

  LOG_I(HW, "ODU Packet Processor Stats:\n");
  LOG_I(HW, "  Total U-Plane Packets received: %lu\n", ctx->stats.total_uplane_received);
  if (ctx->stats.uplane_err_early > 0)
    LOG_I(HW, "  U-Plane Timing Early Errors: %lu\n", ctx->stats.uplane_err_early);
  if (ctx->stats.uplane_err_late > 0)
    LOG_I(HW, "  U-Plane Timing Late Errors: %lu\n", ctx->stats.uplane_err_late);
  if (ctx->stats.ul_tdd_mismatch > 0)
    LOG_I(HW, "  UL TDD Mismatch Errors: %lu\n", ctx->stats.ul_tdd_mismatch);
  if (ctx->stats.application_too_slow > 0)
    LOG_I(HW, "  Application Too Slow Errors: %lu\n", ctx->stats.application_too_slow);
  if (ctx->stats.out_of_mbufs > 0)
    LOG_I(HW, "  Out Of Mbufs Errors: %lu\n", ctx->stats.out_of_mbufs);

  LOG_I(HW, "  Total PRACH U-Plane Packets received: %lu\n", ctx->stats.total_prach_uplane_received);
  if (ctx->stats.prach_uplane_err_early > 0)
    LOG_I(HW, "  PRACH Timing Early Errors: %lu\n", ctx->stats.prach_uplane_err_early);
  if (ctx->stats.prach_uplane_err_late > 0)
    LOG_I(HW, "  PRACH Timing Late Errors: %lu\n", ctx->stats.prach_uplane_err_late);
  if (ctx->stats.prach_missing_expect_job > 0)
    LOG_I(HW, "  PRACH Missing Expect Job Errors: %lu\n", ctx->stats.prach_missing_expect_job);

  print_histogram("UL U-Plane", &ctx->stats.ul_uplane_hist, ctx->Ta3_min_sym_diff, ctx->Ta3_max_sym_diff);
  print_histogram("PRACH U-Plane", &ctx->stats.prach_uplane_hist, ctx->Ta3_min_sym_diff, ctx->Ta3_max_sym_diff);
}
