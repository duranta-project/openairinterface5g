#include "du_tx_scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rte_ring.h>
#include <rte_byteorder.h>
#include "log.h"
#include "assertions.h"
#include "common/utils/nr/nr_common.h"

// Send queues, one per symbol, used round robin
#define WINDOW_DEPTH 256
// A queue is checked again this many symbols after it was sent: anything in it then was queued
// late. Work can be queued less than WINDOW_DEPTH - LATE_CHECK_LAG symbols ahead, so the checked
// queue is never being filled for a later symbol.
#define LATE_CHECK_LAG NR_SYMBOLS_PER_SLOT
// Packets one send symbol can hold
#define SEND_QUEUE_SIZE 64
#define MAX_SECTIONS_PER_CALL 64
#define MAX_TX_BURST 64
#define NUM_RU_PORT_IDS 16 // eAxC ruPortId values; PRACH ports sit above the PUSCH ones
#define NR_NUMBER_OF_SUBFRAMES_PER_FRAME 10

typedef struct {
  // Packets are built by the caller and queued for their send symbol, at index
  // send symbol % WINDOW_DEPTH. Any thread may enqueue; only the symbol tick dequeues.
  struct rte_ring *send_queue[WINDOW_DEPTH];

  uint64_t current_absolute_symbol; // written by the tick, read by the callers
  uint64_t window_tail_symbol;

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

  // eCPRI sequence numbers per eAxC, assigned when a packet is sent so they follow wire order
  uint8_t uplane_seq_id[NUM_RU_PORT_IDS];
  uint8_t cplane_seq_id[NUM_RU_PORT_IDS];

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

  for (int i = 0; i < WINDOW_DEPTH; i++) {
    char name[RTE_RING_NAMESIZE];
    snprintf(name, sizeof(name), "du_tx_send_%d", i);
    ctx->send_queue[i] = rte_ring_create(name, SEND_QUEUE_SIZE, rte_socket_id(), RING_F_SC_DEQ | RING_F_EXACT_SZ);
    AssertFatal(ctx->send_queue[i] != NULL, "Failed to create ring %s\n", name);
  }
  return ctx;
}

void cleanup_du_tx_scheduler(void *context)
{
  du_tx_scheduler_context_t *ctx = (du_tx_scheduler_context_t *)context;
  if (!ctx)
    return;
  for (int i = 0; i < WINDOW_DEPTH; i++) {
    struct rte_mbuf *m;
    while (rte_ring_dequeue(ctx->send_queue[i], (void **)&m) == 0)
      rte_pktmbuf_free(m);
    rte_ring_free(ctx->send_queue[i]);
  }
  free(ctx);
}

static uint64_t ota_absolute_symbol(const du_tx_scheduler_context_t *ctx, uint64_t hyper_frame, int frame, int slot, int symbol)
{
  int num_symbols_per_frame = NR_NUMBER_OF_SUBFRAMES_PER_FRAME * (1 << ctx->numerology) * NR_SYMBOLS_PER_SLOT;
  return (hyper_frame * 1024ULL + frame) * num_symbols_per_frame + (uint64_t)slot * NR_SYMBOLS_PER_SLOT + symbol;
}

// True if packets for send_at_symbol can still be queued. Counts work queued too far ahead;
// the caller counts work that is already late.
static bool can_queue(du_tx_scheduler_context_t *ctx, uint64_t send_at_symbol, bool *late)
{
  uint64_t now = __atomic_load_n(&ctx->current_absolute_symbol, __ATOMIC_ACQUIRE);
  *late = now == 0 || send_at_symbol <= now;
  if (*late)
    return false;
  if (send_at_symbol - now >= WINDOW_DEPTH - LATE_CHECK_LAG) {
    ctx->stats.too_far_ahead++;
    return false;
  }
  return true;
}

static void queue_packets(du_tx_scheduler_context_t *ctx, uint64_t send_at_symbol, struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  unsigned queued = rte_ring_enqueue_burst(ctx->send_queue[send_at_symbol % WINDOW_DEPTH], (void **)mbufs, num_mbufs, NULL);
  for (uint32_t i = queued; i < num_mbufs; i++)
    rte_pktmbuf_free(mbufs[i]);
  ctx->stats.send_queue_full += num_mbufs - queued;
}

static void free_packets(struct rte_mbuf **mbufs, uint32_t num_mbufs)
{
  for (uint32_t i = 0; i < num_mbufs; i++)
    rte_pktmbuf_free(mbufs[i]);
}

// Builds one section type 1 C-Plane message per section; the sequence number is set when sent.
static uint32_t build_cplane_section1(du_tx_scheduler_context_t *ctx,
                                      struct rte_mbuf **mbufs,
                                      uint8_t direction,
                                      int frame,
                                      int slot,
                                      int start_symbol,
                                      int ant_id,
                                      const du_tx_dl_section_t *sections,
                                      int num_sections)
{
  // The O-RU compresses UL U-Plane as the C-Plane's udCompHdr says
  struct xran_radioapp_udComp_header udComp = {.udCompMeth = ctx->comp_method, .udIqWidth = XRAN_CONVERT_IQWIDTH(ctx->iq_width)};
  int slots_per_subframe = 1 << ctx->numerology;
  uint32_t num_mbufs = 0;
  for (int s = 0; s < num_sections; s++) {
    struct rte_mbuf *pkt = ctx->alloc_func(ctx->io_controller);
    if (pkt == NULL) {
      ctx->stats.out_of_mbufs++;
      free_packets(mbufs, num_mbufs);
      return 0;
    }
    fill_cplane_section1(pkt, &ctx->eaxcid_config,
                         direction, frame, slot / slots_per_subframe, slot % slots_per_subframe, start_symbol,
                         0, udComp,
                         0, ant_id, 0,
                         sections[s].section_id, sections[s].beam_id, 1,
                         sections[s].start_prb, sections[s].num_prb, 0xFFF, 0, 0);
    mbufs[num_mbufs++] = pkt;
  }
  return num_mbufs;
}

// Builds the DL U-Plane packets of one symbol, split to fit the MTU; the sequence numbers are set
// when sent.
static uint32_t build_dl_uplane(du_tx_scheduler_context_t *ctx,
                                struct rte_mbuf **mbufs,
                                uint32_t max_mbufs,
                                uint32_t **txdataF,
                                int nb_tx,
                                int frame,
                                int slot,
                                int symbol,
                                const du_tx_dl_section_t *sections,
                                int num_sections)
{
  bool use_comp = (ctx->comp_method != FH_COMP_NONE);
  size_t header_length = sizeof(struct xran_ecpri_hdr) + sizeof(struct radio_app_common_hdr) + sizeof(struct data_section_hdr);
  if (use_comp)
    header_length += sizeof(struct data_section_compression_hdr);
  const size_t prb_bytes = use_comp ? (size_t)FH_COMP_PRB_BYTES(ctx->iq_width) : (size_t)(NR_NB_SC_PER_RB * sizeof(int32_t));
  int max_prb_per_packet = (int)((ctx->mtu - sizeof(struct rte_ether_hdr) - header_length) / prb_bytes);

  uint32_t num_mbufs = 0;
  for (int aatx = 0; aatx < nb_tx; aatx++) {
    for (int s = 0; s < num_sections; s++) {
      for (int rbs_sent = 0; rbs_sent < sections[s].num_prb;) {
        int num_rbs = sections[s].num_prb - rbs_sent;
        if (num_rbs > max_prb_per_packet)
          num_rbs = max_prb_per_packet;
        int start_prb = sections[s].start_prb + rbs_sent;

        if (num_mbufs == max_mbufs) {
          ctx->stats.send_queue_full++;
          free_packets(mbufs, num_mbufs);
          return 0;
        }
        struct rte_mbuf *pkt = ctx->alloc_func(ctx->io_controller);
        size_t data_len = use_comp ? prb_bytes * num_rbs : (size_t)num_rbs * NR_NB_SC_PER_RB * sizeof(int32_t);
        char *buf = pkt ? rte_pktmbuf_append(pkt, (uint16_t)(header_length + data_len)) : NULL;
        if (buf == NULL) {
          if (pkt)
            rte_pktmbuf_free(pkt);
          ctx->stats.out_of_mbufs++;
          free_packets(mbufs, num_mbufs);
          return 0;
        }
        mbufs[num_mbufs++] = pkt;

        struct xran_ecpri_hdr *ecpri_header = (struct xran_ecpri_hdr *)buf;
        fill_ecpri_header(ecpri_header, &ctx->eaxcid_config, ECPRI_IQ_DATA, (uint16_t)(header_length - 4 + data_len), 0, aatx, 0, 0);
        struct radio_app_common_hdr *radio_app_header = (struct radio_app_common_hdr *)(ecpri_header + 1);
        fill_radio_app_header(radio_app_header, 0, XRAN_DIR_DL, frame, slot, symbol, ctx->numerology);
        struct data_section_hdr *data_section_header = (struct data_section_hdr *)(radio_app_header + 1);
        fill_data_section_header(data_section_header, num_rbs, start_prb, sections[s].section_id);

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

        // txdataF holds one packed I/Q pair per uint32_t
        const int16_t *src = (const int16_t *)&txdataF[aatx][start_prb * NR_NB_SC_PER_RB];
        if (use_comp) {
          fh_compress_prbs(ctx->comp_method, ctx->iq_width, num_rbs, src, (int8_t *)iq_data_start);
        } else {
          const uint16_t *raw = (const uint16_t *)src;
          uint16_t *dst = (uint16_t *)iq_data_start;
          for (int i = 0; i < num_rbs * NR_NB_SC_PER_RB * 2; i++)
            dst[i] = rte_cpu_to_be_16(raw[i]);
        }
        rbs_sent += num_rbs;
      }
    }
  }
  return num_mbufs;
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
  if (!ctx || num_sections <= 0 || num_sections > MAX_SECTIONS_PER_CALL)
    return;
  uint64_t ota = ota_absolute_symbol(ctx, hyper_frame, frame, slot, symbol);
  struct rte_mbuf *mbufs[SEND_QUEUE_SIZE];
  bool late;

  uint64_t send_at = ota - (ctx->T1a_up_max_sym_diff - 1);
  if (can_queue(ctx, send_at, &late)) {
    uint32_t n = build_dl_uplane(ctx, mbufs, SEND_QUEUE_SIZE, txdataF, nb_tx, frame, slot, symbol, sections, num_sections);
    if (n > 0) {
      queue_packets(ctx, send_at, mbufs, n);
      ctx->stats.total_dl_scheduled++;
    }
  } else if (late) {
    ctx->stats.app_too_late_tx++;
  }

  uint64_t cp_send_at = ota - (ctx->T1a_cp_dl_max_sym_diff - 1);
  if (can_queue(ctx, cp_send_at, &late)) {
    for (int ant = 0; ant < nb_tx; ant++) {
      uint32_t n = build_cplane_section1(ctx, mbufs, XRAN_DIR_DL, frame, slot, symbol, ant, sections, num_sections);
      queue_packets(ctx, cp_send_at, mbufs, n);
    }
  } else if (late) {
    ctx->stats.dl_cplane_too_late++;
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
  if (!ctx || num_sections <= 0 || num_sections > MAX_SECTIONS_PER_CALL)
    return;
  uint64_t cp_send_at = ota_absolute_symbol(ctx, hyper_frame, frame, slot, start_symbol) - (ctx->T1a_cp_ul_max_sym_diff - 1);
  bool late;
  if (!can_queue(ctx, cp_send_at, &late)) {
    if (late)
      ctx->stats.ul_grant_too_late++;
    return;
  }
  struct rte_mbuf *mbufs[MAX_SECTIONS_PER_CALL];
  uint32_t n = build_cplane_section1(ctx, mbufs, XRAN_DIR_UL, frame, slot, start_symbol, ant_id, sections, num_sections);
  queue_packets(ctx, cp_send_at, mbufs, n);
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
  if (!ctx)
    return;
  uint64_t cp_send_at = ota_absolute_symbol(ctx, hyper_frame, frame, slot, start_symbol) - (ctx->T1a_cp_ul_max_sym_diff - 1);
  bool late;
  if (!can_queue(ctx, cp_send_at, &late)) {
    if (late)
      ctx->stats.ul_grant_too_late++;
    return;
  }
  struct rte_mbuf *pkt = ctx->alloc_func(ctx->io_controller);
  if (pkt == NULL) {
    ctx->stats.out_of_mbufs++;
    return;
  }
  struct xran_radioapp_udComp_header udComp = {.udCompMeth = ctx->comp_method, .udIqWidth = XRAN_CONVERT_IQWIDTH(ctx->iq_width)};
  int slots_per_subframe = 1 << ctx->numerology;
  fill_cplane_section3(pkt, &ctx->eaxcid_config,
                       frame, slot / slots_per_subframe, slot % slots_per_subframe, start_symbol,
                       prach->filter_index, prach->time_offset, prach->scs, prach->fft_size,
                       0, udComp,
                       0, ant_id, 0,
                       prach->section_id, prach->beam_id, prach->num_symbol,
                       prach->start_prb, prach->num_prb, (uint32_t)prach->freq_offset);
  queue_packets(ctx, cp_send_at, &pkt, 1);
}

// Sets the eCPRI sequence number of a packet about to be sent and counts it
static void stamp_packet(du_tx_scheduler_context_t *ctx, struct rte_mbuf *m)
{
  struct xran_ecpri_hdr *ecpri = rte_pktmbuf_mtod(m, struct xran_ecpri_hdr *);
  uint8_t cu_port_id, band_sector_id, cc_id, ru_port_id;
  xran_decompose_cid(ecpri->ecpri_xtc_id, &ctx->eaxcid_config, &cu_port_id, &band_sector_id, &cc_id, &ru_port_id);
  ru_port_id &= NUM_RU_PORT_IDS - 1;
  if (ecpri->cmnhdr.bits.ecpri_mesg_type == ECPRI_IQ_DATA) {
    ecpri->ecpri_seq_id.bits.seq_id = ctx->uplane_seq_id[ru_port_id]++;
    ctx->stats.total_dl_sent++;
  } else {
    ecpri->ecpri_seq_id.bits.seq_id = ctx->cplane_seq_id[ru_port_id]++;
    struct radio_app_common_hdr *app = (struct radio_app_common_hdr *)(ecpri + 1);
    if (app->data_feature.data_direction == XRAN_DIR_DL)
      ctx->stats.total_cplane_dl_sent++;
    else
      ctx->stats.total_cplane_ul_sent++;
  }
}

static void send_queued(du_tx_scheduler_context_t *ctx, uint64_t symbol)
{
  struct rte_ring *queue = ctx->send_queue[symbol % WINDOW_DEPTH];
  struct rte_mbuf *pkts[MAX_TX_BURST];
  unsigned n;
  while ((n = rte_ring_dequeue_burst(queue, (void **)pkts, MAX_TX_BURST, NULL)) > 0) {
    for (unsigned i = 0; i < n; i++)
      stamp_packet(ctx, pkts[i]);
    ctx->send_func(ctx->io_controller, pkts, n);
  }
}

// Packets still in a queue LATE_CHECK_LAG symbols after it was sent were queued after their send symbol
static void drop_late(du_tx_scheduler_context_t *ctx, uint64_t symbol)
{
  struct rte_mbuf *m;
  while (rte_ring_dequeue(ctx->send_queue[symbol % WINDOW_DEPTH], (void **)&m) == 0) {
    rte_pktmbuf_free(m);
    ctx->stats.tx_late++;
  }
}

void du_tx_handle_absolute_symbol_tick(void *context, uint64_t absolute_symbol)
{
  du_tx_scheduler_context_t *ctx = (du_tx_scheduler_context_t *)context;
  if (ctx->current_absolute_symbol == 0)
    ctx->window_tail_symbol = absolute_symbol;
  __atomic_store_n(&ctx->current_absolute_symbol, absolute_symbol, __ATOMIC_RELEASE);
  for (; ctx->window_tail_symbol <= absolute_symbol; ctx->window_tail_symbol++) {
    send_queued(ctx, ctx->window_tail_symbol);
    drop_late(ctx, ctx->window_tail_symbol - LATE_CHECK_LAG);
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
  if (ctx->stats.too_far_ahead > 0)
    LOG_I(HW, "  Queued Too Far Ahead Drops: %lu\n", ctx->stats.too_far_ahead);
  if (ctx->stats.tx_late > 0)
    LOG_I(HW, "  Missed Send Symbol Drops: %lu\n", ctx->stats.tx_late);
  if (ctx->stats.send_queue_full > 0)
    LOG_I(HW, "  Send Queue Full Drops: %lu\n", ctx->stats.send_queue_full);
  if (ctx->stats.out_of_mbufs > 0)
    LOG_I(HW, "  Out Of Mbufs Errors: %lu\n", ctx->stats.out_of_mbufs);
}
