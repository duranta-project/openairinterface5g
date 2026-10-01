/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#define _GNU_SOURCE
#include "du_fh.h"
#include "PHY/CODING/nrPolar_tools/nr_polar_defs.h"
#include "assertions.h"
#include "common/utils/utils.h"
#include "fh_timer.h"
#include "nr/nr_common.h"
#include "platform_types.h"
#include <rte_eal.h>
#include <rte_ethdev.h>
#include "log.h"
#include <sched.h>
#include <semaphore.h>
#include <rte_ring.h>

#define SLOT_EVENT_RING_SIZE 64

typedef struct {
  du_io_t io;
  du_io_config_t io_config;
  du_fh_config_t cfg;
  void *packet_processor;
  void *tx_scheduler;
  // One event per OTA slot, DL or UL, produced from the fh_timer tick once the slot's UL receive
  // window has closed (so every UL symbol job of the slot is already ready). This is what paces
  // the L1 RX loop, like xran's per-slot rx callback does for the vendor path.
  struct rte_ring *slot_events;
  sem_t slot_sem;
  uint32_t ul_window_symbols;
  uint64_t next_slot_event; // absolute slot of the next event; 0 until the first tick
} du_fh_t;

static void rx_cb(struct rte_mbuf **pkts, uint16_t n, void *user_data)
{
  du_fh_t *fh = (du_fh_t *)user_data;
  for (int i = 0; i < n; i++) {
    struct rte_mbuf *pkt = pkts[i];
    struct rte_ether_hdr *eth = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
    uint16_t eth_type = rte_be_to_cpu_16(eth->ether_type);
    size_t hdr_len = sizeof(struct rte_ether_hdr);

    if (eth_type == 0x8100) { // VLAN
      struct rte_vlan_hdr *vlan = (struct rte_vlan_hdr *)(eth + 1);
      eth_type = rte_be_to_cpu_16(vlan->eth_proto);
      hdr_len += sizeof(struct rte_vlan_hdr);
    }

    if (eth_type == ECPRI_ETHER_TYPE) {
      rte_pktmbuf_adj(pkt, hdr_len);
      struct xran_ecpri_hdr *ecpri_hdr = rte_pktmbuf_mtod(pkt, struct xran_ecpri_hdr *);
      if (ecpri_hdr->cmnhdr.bits.ecpri_mesg_type == ECPRI_IQ_DATA) {
        uint16_t pc_id = rte_be_to_cpu_16(ecpri_hdr->ecpri_xtc_id);
        // Route on the antenna (ruPortId) field alone, not the whole composite eAxC
        // ID -- matches the standard eaxcid_config (mask_ruPortId=0x000F, bit_ruPortId=0)
        // hardcoded throughout this codebase (init_du_packet_processor, oru_packet_processor.c).
        uint16_t ant_id = pc_id & 0x000F;
        if (fh->cfg.prach_eaxc_offset > 0 && ant_id >= fh->cfg.prach_eaxc_offset) {
          du_pp_handle_prach_uplane_packet(fh->packet_processor, pkt);
        } else {
          du_pp_handle_uplane_packet(fh->packet_processor, pkt);
        }
      } else {
        rte_pktmbuf_free(pkt);
      }
    } else {
      rte_pktmbuf_free(pkt);
    }
  }
}

static void timer_cb(uint64_t s_abs, void *user_data)
{
  du_fh_t *fh = (du_fh_t *)user_data;
  du_pp_handle_absolute_symbol_tick(fh->packet_processor, s_abs);
  du_tx_handle_absolute_symbol_tick(fh->tx_scheduler, s_abs);

  if (s_abs < fh->ul_window_symbols)
    return;
  uint64_t closed_symbol = s_abs - fh->ul_window_symbols; // last symbol whose UL window has closed
  if (fh->next_slot_event == 0)
    fh->next_slot_event = closed_symbol / NR_SYMBOLS_PER_SLOT + 1;
  while ((fh->next_slot_event + 1) * NR_SYMBOLS_PER_SLOT - 1 <= closed_symbol) {
    if (rte_ring_enqueue(fh->slot_events, (void *)(uintptr_t)fh->next_slot_event) == 0)
      sem_post(&fh->slot_sem);
    fh->next_slot_event++;
  }
}

void *du_fh_init(du_fh_config_t *cfg)
{
  AssertFatal(cfg->comp_type < FH_COMP_NUM_METHODS,
              "comp_type %d out of range [0..%d]\n", cfg->comp_type, FH_COMP_NUM_METHODS - 1);

  char *argv[64];
  int argc = 0;
  char vdev_args[MAX_RU_PORTS][64];
  int vdev_idx = 0;
  argv[argc++] = "du_fh";
  for (int i = 0; i < cfg->dpdk_conf.num_dpdk_devices; i++) {
    if (strchr(cfg->dpdk_conf.dpdk_devices[i], ':')) {
      argv[argc++] = "-a";
      argv[argc++] = cfg->dpdk_conf.dpdk_devices[i];
    } else {
      snprintf(vdev_args[vdev_idx], sizeof(vdev_args[vdev_idx]), "--vdev=%s", cfg->dpdk_conf.dpdk_devices[i]);
      argv[argc++] = vdev_args[vdev_idx++];
    }
  }
  char lcores[32];
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  if (sched_getaffinity(0, sizeof(cpu_set_t), &cpuset) == -1) {
    LOG_E(HW, "sched_getaffinity error\n");
    return NULL;
  }

  int sys_core = -1;
  long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
  for (int i = 0; i < num_cpus; i++) {
    if (CPU_ISSET(i, &cpuset)) {
      if (i != cfg->worker_core) {
        sys_core = i;
        break;
      }
    }
  }
  char main_lcore_arg[32];
  if (sys_core != -1) {
    snprintf(lcores, sizeof(lcores), "%d,%d", sys_core, cfg->worker_core);
    argv[argc++] = "-l";
    argv[argc++] = lcores;
    snprintf(main_lcore_arg, sizeof(main_lcore_arg), "--main-lcore=%d", sys_core);
    argv[argc++] = main_lcore_arg;
  }
  if (cfg->dpdk_conf.extra_eal_args) {
    for (int i = 0; i < cfg->dpdk_conf.num_extra_eal_args; i++) {
      argv[argc++] = cfg->dpdk_conf.extra_eal_args[i];
    }
  }
  LOG_I(HW, "EAL init: %d args\n", argc);
  for (int i = 0; i < argc; i++) {
    LOG_I(HW, "EAL arg %d: %s\n", i, argv[i]);
  }
  int ret = rte_eal_init(argc, argv);

  if (ret < 0) {
    if (rte_errno == EALREADY) {
      LOG_I(HW, "DPDK EAL already initialized\n");
    } else {
      if (sys_core == -1)
        LOG_E(HW, "Need to have at least 2 cores to start fronthaul library\n");
      else
        LOG_E(HW, "DPDK EAL initialization failed: %s\n", rte_strerror(rte_errno));
      return NULL;
    }
  } else if (sys_core == -1) {
    LOG_E(HW, "Need to have at least 2 cores to start fronthaul library\n");
    return NULL;
  }

  if (sys_core != -1) {
    CPU_CLR(cfg->worker_core, &cpuset);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) != 0) {
      LOG_E(HW, "pthread_setaffinity_np error\n");
      return NULL;
    }
  }

  du_fh_t *fh = (du_fh_t *)malloc_or_fail(sizeof(du_fh_t));
  memset(fh, 0, sizeof(du_fh_t));
  fh->cfg = *cfg;
  fh->io_config.numerology = cfg->numerology;
  fh->io_config.num_ports = cfg->dpdk_conf.num_dpdk_devices;
  for (int i = 0; i < cfg->dpdk_conf.num_dpdk_devices; i++) {
    if (rte_eth_dev_get_port_by_name(cfg->dpdk_conf.dpdk_devices[i], &fh->io_config.port_ids[i]) < 0) {
      fprintf(stderr, "DPDK device %s not found\n", cfg->dpdk_conf.dpdk_devices[i]);
      free(fh);
      return NULL;
    }
  }

  fh->io_config.num_macs = cfg->num_ru_mac_addrs;
  for (int i = 0; i < cfg->num_ru_mac_addrs; i++) {
    if (rte_ether_unformat_addr(cfg->ru_mac_addrs[i], &fh->io_config.ru_macs[i]) < 0) {
      fprintf(stderr, "Invalid MAC address: %s\n", cfg->ru_mac_addrs[i]);
      free(fh);
      return NULL;
    }
  }

  fh->io_config.rx_cb = rx_cb;
  fh->io_config.rx_user_data = fh;
  fh->io_config.timer_cb = timer_cb;
  fh->io_config.timer_user_data = fh;

  uint16_t header_size = sizeof(struct xran_up_pkt_hdr);
  uint16_t payload_size = cfg->num_prbs * 12 * 4;
  uint16_t required_mtu = header_size + payload_size;

  fh->io_config.mbuf_data_room = required_mtu + RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + RTE_PKTMBUF_HEADROOM;
  fh->io_config.mbuf_count = 8192;
  fh->io_config.mtu = cfg->mtu;

  if (du_io_init(&fh->io, &fh->io_config) < 0) {
    free(fh);
    return NULL;
  }

  fh->packet_processor = init_du_packet_processor(cfg->numerology,
                                                  cfg->num_prbs,
                                                  cfg->Ta3_min_uS,
                                                  cfg->Ta3_max_uS,
                                                  cfg->tdd_pattern.num_ul_slots,
                                                  cfg->tdd_pattern.num_ul_symbols,
                                                  cfg->tdd_pattern.tdd_pattern_length_slots,
                                                  cfg->fdd_mode,
                                                  cfg->mtu,
                                                  cfg->prach_eaxc_offset);

  struct xran_eaxcid_config eaxcid_config = {
      .mask_cuPortId = 0xf000,
      .mask_bandSectorId = 0x0f00,
      .mask_ccId = 0x00f0,
      .mask_ruPortId = 0x000f,
      .bit_cuPortId = 12,
      .bit_bandSectorId = 8,
      .bit_ccId = 4,
      .bit_ruPortId = 0};

  fh->tx_scheduler = init_du_tx_scheduler(cfg->numerology,
                                          cfg->num_prbs,
                                          cfg->T1a_up_min_uS,
                                          cfg->T1a_up_max_uS,
                                          cfg->T1a_cp_dl_min_uS,
                                          cfg->T1a_cp_dl_max_uS,
                                          cfg->T1a_cp_ul_min_uS,
                                          cfg->T1a_cp_ul_max_uS,
                                          cfg->mtu,
                                          cfg->comp_type,
                                          cfg->iq_width,
                                          eaxcid_config,
                                          (du_tx_alloc_func_t)du_io_get_sendbuf,
                                          (du_tx_send_func_t)du_io_send_uplane,
                                          &fh->io);

  fh->ul_window_symbols = du_pp_get_ul_window_symbols(fh->packet_processor);
  fh->slot_events = rte_ring_create("du_slot_events", SLOT_EVENT_RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
  AssertFatal(fh->slot_events != NULL, "could not create du_slot_events ring\n");
  sem_init(&fh->slot_sem, 0, 0);

  return fh;
}

void du_fh_cleanup(void *handle)
{
  if (!handle) return;
  du_fh_t *fh = (du_fh_t *)handle;
  if (fh->packet_processor) cleanup_du_packet_processor(fh->packet_processor);
  if (fh->tx_scheduler) cleanup_du_tx_scheduler(fh->tx_scheduler);
  if (fh->slot_events) {
    rte_ring_free(fh->slot_events);
    sem_destroy(&fh->slot_sem);
  }
  du_io_cleanup(&fh->io);
  free(fh);
}

int du_fh_start(void *handle)
{
  if (!handle) return -1;
  du_fh_t *fh = (du_fh_t *)handle;
  return du_io_run(&fh->io, fh->cfg.worker_core);
}

void du_fh_stop(void *handle)
{
  if (!handle) return;
  du_fh_t *fh = (du_fh_t *)handle;
  du_fh_print_stats(handle);
  du_io_stop(&fh->io);

  if (fh->cfg.worker_core != -1 && fh->cfg.worker_core != RTE_MAX_LCORE)
    rte_eal_wait_lcore(fh->cfg.worker_core);
}

void du_fh_tx_send_dl_iq(void *handle, uint32_t **txdataF, int nb_tx, uint64_t hyper_frame, int frame, int slot, int symbol, const du_tx_dl_section_t *sections, int num_sections)
{
  du_fh_t *fh = (du_fh_t *)handle;
  du_tx_schedule_dl_iq(fh->tx_scheduler, txdataF, nb_tx, hyper_frame, frame, slot, symbol, sections, num_sections);
}

void du_fh_schedule_ul_grant(void *handle, uint64_t hyper_frame, int frame, int slot, int start_symbol, int ant_id, const du_tx_dl_section_t *sections, int num_sections)
{
  du_fh_t *fh = (du_fh_t *)handle;
  du_tx_schedule_ul_grant(fh->tx_scheduler, hyper_frame, frame, slot, start_symbol, ant_id, sections, num_sections);
}

void du_fh_schedule_prach(void *handle, uint64_t hyper_frame, int frame, int slot, int start_symbol, int ant_id, const du_tx_prach_section_t *prach)
{
  du_fh_t *fh = (du_fh_t *)handle;
  du_tx_schedule_prach(fh->tx_scheduler, hyper_frame, frame, slot, start_symbol, ant_id, prach);
}

void du_fh_expect_ul_symbol(void *handle, uint64_t absolute_symbol, int ant_id, int section_id, int start_prb, int num_prb, fh_comp_method_t comp_method, uint8_t iq_width)
{
  du_fh_t *fh = (du_fh_t *)handle;
  du_pp_expect_ul_symbol(fh->packet_processor, absolute_symbol, ant_id, section_id, start_prb, num_prb, comp_method, iq_width);
}

void du_fh_expect_prach_occasion(void *handle, uint64_t start_absolute_symbol, int num_symbols, int slot_in_frame, int ant_id, int section_id, int start_prb, int num_prb, fh_comp_method_t comp_method, uint8_t iq_width, int kbar)
{
  du_fh_t *fh = (du_fh_t *)handle;
  du_pp_expect_prach_occasion(fh->packet_processor, start_absolute_symbol, num_symbols, slot_in_frame, ant_id, section_id, start_prb, num_prb, comp_method, iq_width, kbar);
}

int du_fh_get_ready_ul_job_count(void *handle)
{
  du_fh_t *fh = (du_fh_t *)handle;
  return du_pp_get_ready_ul_job_count(fh->packet_processor);
}

void du_fh_read_ul_iq(void *handle, uint32_t **rxdataF, int nb_rx, uint64_t *hyper_frame, int *frame, int *slot, int *symbol)
{
  du_fh_t *fh = (du_fh_t *)handle;
  du_pp_read_ul_iq(fh->packet_processor, rxdataF, nb_rx, hyper_frame, frame, slot, symbol);
}

int du_fh_wait_slot(void *handle, uint64_t *absolute_slot)
{
  du_fh_t *fh = (du_fh_t *)handle;
  void *ev;
  sem_wait(&fh->slot_sem);
  AssertFatal(rte_ring_dequeue(fh->slot_events, &ev) == 0, "slot event semaphore/ring mismatch\n");
  int skipped = 0;
  // Behind by more than a slot or two: jump to the newest slot, as the vendor path does.
  while (rte_ring_count(fh->slot_events) >= DU_FH_MAX_SLOT_BACKLOG) {
    sem_wait(&fh->slot_sem);
    rte_ring_dequeue(fh->slot_events, &ev);
    skipped++;
  }
  *absolute_slot = (uint64_t)(uintptr_t)ev;
  return skipped;
}

bool du_fh_read_ul_iq_upto(void *handle, uint32_t **rxdataF, int nb_rx, uint64_t last_absolute_symbol, uint64_t *absolute_symbol)
{
  du_fh_t *fh = (du_fh_t *)handle;
  return du_pp_read_ul_iq_upto(fh->packet_processor, rxdataF, nb_rx, last_absolute_symbol, absolute_symbol);
}

int du_fh_get_ready_prach_job_count(void *handle)
{
  du_fh_t *fh = (du_fh_t *)handle;
  return du_pp_get_ready_prach_job_count(fh->packet_processor);
}

void du_fh_read_prach_iq(void *handle, int16_t *rxdata, uint64_t *hyper_frame, int *frame, int *slot, int *antenna, int *section_id)
{
  du_fh_t *fh = (du_fh_t *)handle;
  du_pp_read_prach_iq(fh->packet_processor, rxdata, hyper_frame, frame, slot, antenna, section_id);
}

uint64_t du_fh_get_current_absolute_symbol(void *handle)
{
  du_fh_t *fh = (du_fh_t *)handle;
  if (!fh) return 0;
  return fh_timer_get_current_symbol(&fh->io.timer);
}

int du_fh_get_utc_anchor_point(void *handle, uint64_t *hyper_frame, uint32_t *frame, uint32_t *slot, struct timespec *ts)
{
  if (!handle || !frame || !slot || !ts) return -1;
  du_fh_t *fh = (du_fh_t *)handle;
  uint64_t absolute_gps_symbol = fh_timer_get_current_symbol(&fh->io.timer);
  absolute_gps_symbol -= absolute_gps_symbol % NR_SYMBOLS_PER_SLOT;
  uint64_t absolute_slot = absolute_gps_symbol / NR_SYMBOLS_PER_SLOT;
  uint32_t slots_per_frame = 10 << fh->cfg.numerology;
  *hyper_frame = (absolute_slot / slots_per_frame) / 1024;
  *frame = (absolute_slot / slots_per_frame) % 1024;
  *slot = absolute_slot % slots_per_frame;

  uint64_t total_syms_per_sec = (NR_SYMBOLS_PER_SLOT * 1000) << fh->cfg.numerology;
  uint64_t ns_per_symbol = 1000000000 / total_syms_per_sec;
  uint64_t leftover_syms = absolute_gps_symbol % total_syms_per_sec;
  ts->tv_sec = (absolute_gps_symbol / total_syms_per_sec) + GPS_EPOCH_OFFSET_UNIX - GPS_LEAP_SECONDS;
  ts->tv_nsec = leftover_syms * ns_per_symbol;
  return 0;
}

void du_fh_get_stats(void *handle, du_packet_processor_stats_t *rx_stats, du_tx_scheduler_stats_t *tx_stats)
{
  du_fh_t *fh = (du_fh_t *)handle;
  if (rx_stats) du_pp_get_stats(fh->packet_processor, rx_stats);
  if (tx_stats) du_tx_get_stats(fh->tx_scheduler, tx_stats);
}

void du_fh_print_stats(void *handle)
{
  if (!handle) return;
  du_fh_t *fh = (du_fh_t *)handle;
  du_pp_print_stats(fh->packet_processor);
  du_tx_print_stats(fh->tx_scheduler);
}
