#include "du_tx_scheduler.h"
#include <stdlib.h>
#include <string.h>
#include <rte_ring.h>
#include <rte_byteorder.h>
#include "log.h"
#include "assertions.h"
#include "common/utils/nr/nr_common.h"

#define WINDOW_DEPTH 1024
#define MAX_SECTIONS_PER_JOB 64
#define MAX_MBUFS_PER_SYMBOL 64
#define MAX_ANTENNAS 4
#define NR_NUMBER_OF_SUBFRAMES_PER_FRAME 10
// 3GPP max carrier bandwidth (400MHz @ 120kHz SCS), matching du_fhi_isolate.c's
// DU_FHI_MAX_SCRATCH_PRB -- upper bound for the owned per-job IQ sample copy below.
#define MAX_PRB 275

typedef struct {
  uint64_t send_at_symbol;
  uint64_t ota_absolute_symbol;
  // Owned copy of this symbol's frequency-domain IQ, one buffer per antenna. The caller
  // (du_fhi_south_out) only guarantees its txdataF argument is valid for the duration of the
  // du_tx_schedule_dl_iq() call -- actual transmission happens later, once send_at_symbol's
  // T1a_up-windowed time arrives (see du_tx_handle_absolute_symbol_tick/dispatch_job), so the
  // data must be copied in at enqueue time rather than referenced by pointer.
  uint32_t txdataF_storage[MAX_ANTENNAS][MAX_PRB * NR_NB_SC_PER_RB];
  uint32_t *txdataF[MAX_ANTENNAS];
  int nb_tx;
  uint64_t hyper_frame;
  int frame;
  int slot;
  int symbol;
  du_tx_dl_section_t sections[MAX_SECTIONS_PER_JOB];
  int num_sections;
} du_tx_dl_job_t;

typedef struct du_tx_cplane_job_s {
  uint64_t send_at_symbol;
  uint8_t direction;
  int ant_id;
  int frame;
  int slot;
  int start_symbol;
  int num_symbol;
  du_tx_dl_section_t sections[MAX_SECTIONS_PER_JOB];
  int num_sections;
  bool is_prach; // section type 3 from `prach` instead of type 1 from `sections`
  du_tx_prach_section_t prach;
  struct du_tx_cplane_job_s *next;
} du_tx_cplane_job_t;

typedef struct {
  du_tx_dl_job_t jobs_pool[WINDOW_DEPTH];
  du_tx_dl_job_t *dl_symbol_tx_window[WINDOW_DEPTH];
  struct rte_ring *free_jobs;

  du_tx_cplane_job_t cplane_jobs_pool[WINDOW_DEPTH];
  du_tx_cplane_job_t *cplane_symbol_tx_window[WINDOW_DEPTH];
  struct rte_ring *free_cplane_jobs;

  uint64_t current_absolute_symbol;
  uint64_t window_tail_symbol;
  uint64_t cplane_window_tail_symbol;


  int numerology;
  int num_prb;
  uint32_t T1a_up_max_sym_diff;
  uint32_t T1a_cp_dl_max_sym_diff;
  uint32_t T1a_cp_ul_max_sym_diff;
  size_t mtu;
  fh_comp_method_t comp_method;
  uint8_t iq_width;
  struct xran_eaxcid_config eaxcid_config;
  
  du_tx_alloc_func_t alloc_func;
  du_tx_send_func_t send_func;
  void *io_controller;

  uint8_t seq_id[MAX_ANTENNAS];
  uint8_t cplane_seq_id[16]; // per eAxC RU port, PRACH ports sit above the PUSCH ones

  du_tx_scheduler_stats_t stats;
} du_tx_scheduler_context_t;

void *init_du_tx_scheduler(int numerology,
                           int num_prb,
                           uint32_t T1a_up_min_uS,
                           uint32_t T1a_up_max_uS,
                           uint32_t T1a_cp_dl_min_uS,
                           uint32_t T1a_cp_dl_max_uS,
                           uint32_t T1a_cp_ul_min_uS,
                           uint32_t T1a_cp_ul_max_uS,
                           size_t mtu,
                           fh_comp_method_t comp_method,
                           uint8_t iq_width,
                           struct xran_eaxcid_config eaxcid_config,
                           du_tx_alloc_func_t alloc_func,
                           du_tx_send_func_t send_func,
                           void *io_controller)
{
  du_tx_scheduler_context_t *ctx = calloc(1, sizeof(*ctx));
  ctx->numerology = numerology;
  ctx->num_prb = num_prb;
  ctx->mtu = mtu;
  ctx->comp_method = comp_method;
  ctx->iq_width = (iq_width == 0) ? 16 : iq_width;
  ctx->eaxcid_config = eaxcid_config;
  ctx->alloc_func = alloc_func;
  ctx->send_func = send_func;
  ctx->io_controller = io_controller;

  uint32_t slots_per_subframe = 1 << numerology;
  uint32_t symbol_duration_uS = 1000 / slots_per_subframe / NR_SYMBOLS_PER_SLOT;
  ctx->T1a_up_max_sym_diff = T1a_up_max_uS / symbol_duration_uS;
  ctx->T1a_cp_dl_max_sym_diff = T1a_cp_dl_max_uS / symbol_duration_uS;
  ctx->T1a_cp_ul_max_sym_diff = T1a_cp_ul_max_uS / symbol_duration_uS;
  
  ctx->free_jobs = rte_ring_create("du_tx_dl_free_jobs", WINDOW_DEPTH, rte_socket_id(), 0);
  AssertFatal(ctx->free_jobs != NULL, "Failed to create ring du_tx_dl_free_jobs\n");
  for (int i = 0; i < WINDOW_DEPTH - 1; i++) {
    rte_ring_enqueue(ctx->free_jobs, (void *)&ctx->jobs_pool[i]);
  }

  ctx->free_cplane_jobs = rte_ring_create("du_tx_cp_free_jobs", WINDOW_DEPTH, rte_socket_id(), 0);
  AssertFatal(ctx->free_cplane_jobs != NULL, "Failed to create ring du_tx_cp_free_jobs\n");
  for (int i = 0; i < WINDOW_DEPTH - 1; i++) {
    rte_ring_enqueue(ctx->free_cplane_jobs, (void *)&ctx->cplane_jobs_pool[i]);
  }
  
  return ctx;
}


void cleanup_du_tx_scheduler(void *context)
{
  du_tx_scheduler_context_t *ctx = (du_tx_scheduler_context_t *)context;
  if (ctx) {
    if (ctx->free_jobs) {
      rte_ring_free(ctx->free_jobs);
    }
    if (ctx->free_cplane_jobs) {
      rte_ring_free(ctx->free_cplane_jobs);
    }
    free(ctx);
  }
}

void du_tx_schedule_dl_iq(void *context,
                          uint32_t **txdataF,
                          int nb_tx,
                          uint64_t hyper_frame,
                          int frame,
                          int slot,
                          int symbol,
                          const du_tx_dl_section_t *sections,
                          int num_sections)
{
  du_tx_scheduler_context_t *ctx = (du_tx_scheduler_context_t *)context;
  if (!ctx || num_sections <= 0 || num_sections > MAX_SECTIONS_PER_JOB || nb_tx > MAX_ANTENNAS) return;

  int slots_per_subframe = 1 << ctx->numerology;
  int num_symbols_per_frame = NR_NUMBER_OF_SUBFRAMES_PER_FRAME * slots_per_subframe * NR_SYMBOLS_PER_SLOT;
  uint64_t ota_absolute_symbol = (hyper_frame * 1024ULL * num_symbols_per_frame) +
                                 ((uint64_t)frame * num_symbols_per_frame) +
                                 ((uint64_t)slot * NR_SYMBOLS_PER_SLOT) + symbol;

  uint64_t send_at_symbol = ota_absolute_symbol - (ctx->T1a_up_max_sym_diff - 1);

  if (ctx->current_absolute_symbol > 0 && send_at_symbol <= ctx->current_absolute_symbol) {
    ctx->stats.app_too_late_tx++;
  } else {
    uint32_t job_index = send_at_symbol % WINDOW_DEPTH;
    if (ctx->dl_symbol_tx_window[job_index] == NULL) {
      du_tx_dl_job_t *job;
      if (rte_ring_dequeue(ctx->free_jobs, (void **)&job) == 0) {
        job->send_at_symbol = send_at_symbol;
        job->ota_absolute_symbol = ota_absolute_symbol;
        job->nb_tx = nb_tx;
        size_t num_prb_bytes = (size_t)ctx->num_prb * NR_NB_SC_PER_RB * sizeof(uint32_t);
        for (int i = 0; i < nb_tx; i++) {
          memcpy(job->txdataF_storage[i], txdataF[i], num_prb_bytes);
          job->txdataF[i] = job->txdataF_storage[i];
        }
        job->hyper_frame = hyper_frame;
        job->frame = frame;
        job->slot = slot;
        job->symbol = symbol;
        job->num_sections = num_sections;
        for (int i = 0; i < num_sections; i++) {
          job->sections[i] = sections[i];
        }

        ctx->dl_symbol_tx_window[job_index] = job;
        ctx->stats.total_dl_scheduled++;
      }
    }
  }

  uint64_t cp_send_at_symbol = ota_absolute_symbol - (ctx->T1a_cp_dl_max_sym_diff - 1);
  if (ctx->current_absolute_symbol > 0 && cp_send_at_symbol <= ctx->current_absolute_symbol) {
    ctx->stats.dl_cplane_too_late++;
  } else {
    for (int ant = 0; ant < nb_tx; ant++) {
      du_tx_cplane_job_t *cp_job;
      if (rte_ring_dequeue(ctx->free_cplane_jobs, (void **)&cp_job) == 0) {
        cp_job->send_at_symbol = cp_send_at_symbol;
        cp_job->direction = XRAN_DIR_DL;
        cp_job->ant_id = ant;
        cp_job->frame = frame;
        cp_job->slot = slot;
        cp_job->start_symbol = symbol;
        cp_job->num_symbol = 1;
        cp_job->num_sections = num_sections;
        for (int i = 0; i < num_sections; i++) {
          cp_job->sections[i] = sections[i];
        }
        
        uint32_t cp_job_index = cp_send_at_symbol % WINDOW_DEPTH;
        cp_job->next = ctx->cplane_symbol_tx_window[cp_job_index];
        ctx->cplane_symbol_tx_window[cp_job_index] = cp_job;
      }
    }
  }
}

void du_tx_schedule_ul_grant(void *context,
                             uint64_t hyper_frame,
                             int frame,
                             int slot,
                             int start_symbol,
                             int ant_id,
                             const du_tx_dl_section_t *sections,
                             int num_sections)
{
  du_tx_scheduler_context_t *ctx = (du_tx_scheduler_context_t *)context;
  if (!ctx || num_sections <= 0 || num_sections > MAX_SECTIONS_PER_JOB) return;

  int slots_per_subframe = 1 << ctx->numerology;
  int num_symbols_per_frame = NR_NUMBER_OF_SUBFRAMES_PER_FRAME * slots_per_subframe * NR_SYMBOLS_PER_SLOT;
  uint64_t ota_absolute_symbol = (hyper_frame * 1024ULL * num_symbols_per_frame) +
                                 ((uint64_t)frame * num_symbols_per_frame) +
                                 ((uint64_t)slot * NR_SYMBOLS_PER_SLOT) + start_symbol;

  uint64_t cp_send_at_symbol = ota_absolute_symbol - (ctx->T1a_cp_ul_max_sym_diff - 1);

  if (ctx->current_absolute_symbol > 0 && cp_send_at_symbol <= ctx->current_absolute_symbol) {
    ctx->stats.ul_grant_too_late++;
    return;
  }
  
  du_tx_cplane_job_t *cp_job;
  if (rte_ring_dequeue(ctx->free_cplane_jobs, (void **)&cp_job) == 0) {
    cp_job->send_at_symbol = cp_send_at_symbol;
    cp_job->direction = XRAN_DIR_UL;
    cp_job->ant_id = ant_id;
    cp_job->frame = frame;
    cp_job->slot = slot;
    cp_job->start_symbol = start_symbol;
    cp_job->num_symbol = 1;
    cp_job->is_prach = false;
    cp_job->num_sections = num_sections;
    for (int i = 0; i < num_sections; i++) {
      cp_job->sections[i] = sections[i];
    }
    
    uint32_t cp_job_index = cp_send_at_symbol % WINDOW_DEPTH;
    cp_job->next = ctx->cplane_symbol_tx_window[cp_job_index];
    ctx->cplane_symbol_tx_window[cp_job_index] = cp_job;
  }
}

void du_tx_schedule_prach(void *context,
                          uint64_t hyper_frame,
                          int frame,
                          int slot,
                          int start_symbol,
                          int ant_id,
                          const du_tx_prach_section_t *prach)
{
  du_tx_scheduler_context_t *ctx = (du_tx_scheduler_context_t *)context;
  int slots_per_subframe = 1 << ctx->numerology;
  int num_symbols_per_frame = NR_NUMBER_OF_SUBFRAMES_PER_FRAME * slots_per_subframe * NR_SYMBOLS_PER_SLOT;
  uint64_t ota_absolute_symbol = (hyper_frame * 1024ULL * num_symbols_per_frame) +
                                 ((uint64_t)frame * num_symbols_per_frame) +
                                 ((uint64_t)slot * NR_SYMBOLS_PER_SLOT) + start_symbol;
  uint64_t cp_send_at_symbol = ota_absolute_symbol - (ctx->T1a_cp_ul_max_sym_diff - 1);
  if (ctx->current_absolute_symbol > 0 && cp_send_at_symbol <= ctx->current_absolute_symbol) {
    ctx->stats.ul_grant_too_late++;
    return;
  }

  du_tx_cplane_job_t *cp_job;
  if (rte_ring_dequeue(ctx->free_cplane_jobs, (void **)&cp_job) != 0) {
    ctx->stats.out_of_mbufs++;
    return;
  }
  cp_job->send_at_symbol = cp_send_at_symbol;
  cp_job->direction = XRAN_DIR_UL;
  cp_job->ant_id = ant_id;
  cp_job->frame = frame;
  cp_job->slot = slot;
  cp_job->start_symbol = start_symbol;
  cp_job->num_symbol = prach->num_symbol;
  cp_job->num_sections = 1;
  cp_job->is_prach = true;
  cp_job->prach = *prach;
  uint32_t cp_job_index = cp_send_at_symbol % WINDOW_DEPTH;
  cp_job->next = ctx->cplane_symbol_tx_window[cp_job_index];
  ctx->cplane_symbol_tx_window[cp_job_index] = cp_job;
}

static void dispatch_cplane_job(du_tx_scheduler_context_t *ctx, du_tx_cplane_job_t *job)
{
  struct rte_mbuf *mbufs[MAX_MBUFS_PER_SYMBOL];
  uint32_t num_mbufs = 0;

  for (int s = 0; s < job->num_sections; s++) {
    struct rte_mbuf *pkt = ctx->alloc_func(ctx->io_controller);
    if (pkt == NULL) {
      ctx->stats.out_of_mbufs++;
      break;
    }

    struct xran_radioapp_udComp_header udComp = {0};
    uint8_t seq = ctx->cplane_seq_id[job->ant_id]++;

    // job->slot is a full slot-in-frame index; decompose into (subframe, slotId)
    // the same way fill_radio_app_header does internally for U-Plane, since
    // fill_cplane_section1 takes the two fields pre-split and assigns them as-is.
    int slots_per_subframe = 1 << ctx->numerology;
    uint8_t subframe = job->slot / slots_per_subframe;
    uint8_t slot_id = job->slot % slots_per_subframe;

    if (job->is_prach) {
      const du_tx_prach_section_t *p = &job->prach;
      fill_cplane_section3(pkt, &ctx->eaxcid_config,
                           job->frame, subframe, slot_id, job->start_symbol,
                           p->filter_index, p->time_offset, p->scs, p->fft_size,
                           0, udComp,
                           0, job->ant_id, seq,
                           p->section_id, p->beam_id, p->num_symbol,
                           p->start_prb, p->num_prb, (uint32_t)p->freq_offset);
    } else {
      fill_cplane_section1(pkt, &ctx->eaxcid_config,
                           job->direction, job->frame, subframe, slot_id, job->start_symbol,
                           0, udComp,
                           0, job->ant_id, seq,
                           job->sections[s].section_id, job->sections[s].beam_id, job->num_symbol,
                           job->sections[s].start_prb, job->sections[s].num_prb, 0xFFF, 0, 0);
    }

    if (num_mbufs == (MAX_MBUFS_PER_SYMBOL - 1)) {
      ctx->send_func(ctx->io_controller, mbufs, num_mbufs);
      if (job->direction == XRAN_DIR_DL) ctx->stats.total_cplane_dl_sent += num_mbufs;
      else ctx->stats.total_cplane_ul_sent += num_mbufs;
      num_mbufs = 0;
    }
    mbufs[num_mbufs++] = pkt;
  }

  if (num_mbufs > 0) {
    ctx->send_func(ctx->io_controller, mbufs, num_mbufs);
    if (job->direction == XRAN_DIR_DL) ctx->stats.total_cplane_dl_sent += num_mbufs;
    else ctx->stats.total_cplane_ul_sent += num_mbufs;
  }
}

static void dispatch_job(du_tx_scheduler_context_t *ctx, du_tx_dl_job_t *job)
{
  bool use_comp = (ctx->comp_method != FH_COMP_NONE);
  size_t overhead = sizeof(struct rte_ether_hdr) + sizeof(struct xran_ecpri_hdr) + sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr);
  if (use_comp) {
    overhead += sizeof(struct data_section_compression_hdr);
  }
  
  const size_t prb_bytes = use_comp ? (size_t)FH_COMP_PRB_BYTES(ctx->iq_width) : (size_t)(NR_NB_SC_PER_RB * sizeof(int32_t));
  int max_prb_per_packet = (int)((ctx->mtu - overhead) / prb_bytes);

  struct rte_mbuf *mbufs[MAX_MBUFS_PER_SYMBOL];
  uint32_t num_mbufs = 0;

  for (int aatx = 0; aatx < job->nb_tx; aatx++) {
    for (int s = 0; s < job->num_sections; s++) {
      int total_section_rbs = job->sections[s].num_prb;
      int start_prb_base = job->sections[s].start_prb;
      int section_id = job->sections[s].section_id;
      int rbs_sent = 0;

      while (rbs_sent < total_section_rbs) {
        int num_rbs = total_section_rbs - rbs_sent;
        if (num_rbs > max_prb_per_packet) {
          num_rbs = max_prb_per_packet;
        }

        struct rte_mbuf *pkt = ctx->alloc_func(ctx->io_controller);
        if (pkt == NULL) {
          ctx->stats.out_of_mbufs++;
          break;
        }

        size_t header_length = sizeof(struct xran_ecpri_hdr) + sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr);
        if (use_comp) {
          header_length += sizeof(struct data_section_compression_hdr);
        }
        
        const uint num_sc = num_rbs * NR_NB_SC_PER_RB;
        size_t data_len = use_comp ? (size_t)FH_COMP_PRB_BYTES(ctx->iq_width) * num_rbs : (size_t)sizeof(int32_t) * num_sc;

        char *buf = rte_pktmbuf_append(pkt, (uint16_t)(header_length + data_len));
        if (buf == NULL) {
          rte_pktmbuf_free(pkt);
          break;
        }

        if (num_mbufs == (MAX_MBUFS_PER_SYMBOL - 1)) {
          ctx->send_func(ctx->io_controller, mbufs, num_mbufs);
          ctx->stats.total_dl_sent += num_mbufs;
          num_mbufs = 0;
        }
        mbufs[num_mbufs++] = pkt;

        struct xran_ecpri_hdr *ecpri_header = (struct xran_ecpri_hdr *)buf;
        uint16_t ecpri_payload_size = (uint16_t)(header_length - 4 + data_len);
        
        fill_ecpri_header(ecpri_header, &ctx->eaxcid_config, ECPRI_IQ_DATA, ecpri_payload_size, 0, aatx, ctx->seq_id[aatx]++, 0);

        struct radio_app_common_hdr *radio_app_header = (struct radio_app_common_hdr *)(ecpri_header + 1);
        fill_radio_app_header(radio_app_header, 0, XRAN_DIR_DL, job->frame, job->slot, job->symbol, ctx->numerology);

        struct data_section_hdr *data_section_header = (struct data_section_hdr *)(radio_app_header + 1);
        fill_data_section_header(data_section_header, num_rbs, start_prb_base + rbs_sent, section_id);

        uint8_t *iq_data_start;
        if (use_comp) {
          struct data_section_compression_hdr *compression_header = (struct data_section_compression_hdr *)(data_section_header + 1);
          compression_header->ud_comp_hdr.ud_comp_meth = ctx->comp_method;
          compression_header->ud_comp_hdr.ud_iq_width = XRAN_CONVERT_IQWIDTH(ctx->iq_width);
          compression_header->rsrvd = 0;
          iq_data_start = (uint8_t *)(compression_header + 1);
        } else {
          iq_data_start = (uint8_t *)(data_section_header + 1);
        }

        // txdataF is uint32_t* (one packed I/Q pair per element), so indexing it directly
        // already lands on the right byte offset once reinterpreted as int16_t* -- no extra
        // factor needed (matches write_ul_iq's rxdataF indexing in oru_packet_processor.c).
        const int16_t *src = (const int16_t *)&job->txdataF[aatx][(start_prb_base + rbs_sent) * NR_NB_SC_PER_RB];
        if (use_comp) {
          fh_compress_prbs(ctx->comp_method,
                           ctx->iq_width,
                           num_rbs,
                           src,
                           (int8_t *)iq_data_start);
        } else {
          const uint16_t *raw = (const uint16_t *)src;
          uint16_t *dst = (uint16_t *)iq_data_start;
          for (int i = 0; i < num_sc * 2; i++)
            dst[i] = rte_cpu_to_be_16(raw[i]);
        }
        
        rbs_sent += num_rbs;
      }
    }
  }

  if (num_mbufs > 0) {
    ctx->send_func(ctx->io_controller, mbufs, num_mbufs);
    ctx->stats.total_dl_sent += num_mbufs;
  }
}

void du_tx_handle_absolute_symbol_tick(void *context, uint64_t absolute_symbol)
{
  du_tx_scheduler_context_t *ctx = (du_tx_scheduler_context_t *)context;
  if (ctx->current_absolute_symbol == 0) {
    ctx->current_absolute_symbol = absolute_symbol - 1;
    ctx->window_tail_symbol = absolute_symbol - 1;
    ctx->cplane_window_tail_symbol = absolute_symbol - 1;
  }
  ctx->current_absolute_symbol = absolute_symbol;

  while (ctx->window_tail_symbol <= absolute_symbol) {
    uint32_t job_index = ctx->window_tail_symbol % WINDOW_DEPTH;
    du_tx_dl_job_t *job = ctx->dl_symbol_tx_window[job_index];
    
    if (job && job->send_at_symbol <= ctx->window_tail_symbol) {
      if (job->send_at_symbol == ctx->window_tail_symbol) {
        dispatch_job(ctx, job);
      }
      ctx->dl_symbol_tx_window[job_index] = NULL;
      rte_ring_enqueue(ctx->free_jobs, job);
    }
    ctx->window_tail_symbol++;
  }

  while (ctx->cplane_window_tail_symbol <= absolute_symbol) {
    uint32_t job_index = ctx->cplane_window_tail_symbol % WINDOW_DEPTH;
    du_tx_cplane_job_t *job = ctx->cplane_symbol_tx_window[job_index];
    // A ring slot can chain jobs from a later window generation (same modular
    // index, send_at_symbol far ahead) -- preserve those instead of dropping
    // the whole list once this generation's due jobs are processed.
    du_tx_cplane_job_t *remaining_head = NULL;

    while (job) {
      du_tx_cplane_job_t *next = job->next;
      if (job->send_at_symbol <= ctx->cplane_window_tail_symbol) {
        if (job->send_at_symbol == ctx->cplane_window_tail_symbol) {
          dispatch_cplane_job(ctx, job);
        }
        rte_ring_enqueue(ctx->free_cplane_jobs, job);
      } else {
        job->next = remaining_head;
        remaining_head = job;
      }
      job = next;
    }
    ctx->cplane_symbol_tx_window[job_index] = remaining_head;
    ctx->cplane_window_tail_symbol++;
  }
}

void du_tx_get_stats(void *context, du_tx_scheduler_stats_t *out)
{
  du_tx_scheduler_context_t *ctx = (du_tx_scheduler_context_t *)context;
  if (ctx && out) {
    *out = ctx->stats;
  }
}

void du_tx_print_stats(void *context)
{
  du_tx_scheduler_context_t *ctx = (du_tx_scheduler_context_t *)context;
  if (ctx == NULL) return;
  LOG_I(HW, "ODU TX Scheduler Stats:\n");
  LOG_I(HW, "  Total DL U-Plane Symbols Scheduled: %lu\n", ctx->stats.total_dl_scheduled);
  LOG_I(HW, "  Total DL U-Plane Packets Sent: %lu\n", ctx->stats.total_dl_sent);
  LOG_I(HW, "  Total C-Plane DL Packets Sent: %lu\n", ctx->stats.total_cplane_dl_sent);
  LOG_I(HW, "  Total C-Plane UL Packets Sent: %lu\n", ctx->stats.total_cplane_ul_sent);
  if (ctx->stats.app_too_late_tx > 0)
    LOG_I(HW, "  App Too Late TX Drops: %lu\n", ctx->stats.app_too_late_tx);
  if (ctx->stats.dl_cplane_too_late > 0)
    LOG_I(HW, "  DL C-Plane Too Late Drops: %lu\n", ctx->stats.dl_cplane_too_late);
  if (ctx->stats.ul_grant_too_late > 0)
    LOG_I(HW, "  UL Grant Too Late Drops: %lu\n", ctx->stats.ul_grant_too_late);
  if (ctx->stats.out_of_mbufs > 0)
    LOG_I(HW, "  Out Of Mbufs Errors: %lu\n", ctx->stats.out_of_mbufs);
}
