/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#define _GNU_SOURCE
#include "oru_fh.h"
#include "PHY/CODING/nrPolar_tools/nr_polar_defs.h"
#include "assertions.h"
#include "common/utils/utils.h"
#include "fh_timer.h"
#include "nr/nr_common.h"
#include "oru_io.h"
#include "platform_types.h"
#include "xran_pkt_api.h"
#include <rte_eal.h>
#include <rte_ethdev.h>
#include "oru_packet_processor.h"
#include "oru_pcap.h"
#include "log.h"
#include <sched.h>

typedef struct {
  oru_io_t io;
  oru_io_config_t io_config;
  oru_fh_config_t cfg;
  void *packet_processor;
} oru_fh_t;

static void rx_cb(struct rte_mbuf **pkts, uint16_t n, void *user_data)
{
  oru_fh_t *fh = (oru_fh_t *)user_data;
  for (int i = 0; i < n; i++) {
    struct rte_mbuf *pkt = pkts[i];
    oru_pcap_write_rx_frame(pkt);
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
      switch (ecpri_hdr->cmnhdr.bits.ecpri_mesg_type) {
        case ECPRI_IQ_DATA:
          handle_uplane_packet(fh->packet_processor, pkt);
          break;
        case ECPRI_RT_CONTROL_DATA:
          handle_cplane_packet(fh->packet_processor, pkt);
          break;
        default:
          rte_pktmbuf_free(pkt);
          break;
      }
    } else {
      rte_pktmbuf_free(pkt);
    }
  }
}

static void timer_cb(uint64_t s_abs, void *user_data)
{
  oru_fh_t *fh = (oru_fh_t *)user_data;
  handle_absolute_symbol_tick(fh->packet_processor, s_abs);
}

void *oru_fh_init(oru_fh_config_t *cfg)
{
  AssertFatal(cfg->comp_type < FH_COMP_NUM_METHODS,
              "comp_type %d out of range [0..%d]\n", cfg->comp_type, FH_COMP_NUM_METHODS - 1);

  char *argv[64];
  int argc = 0;
  char vdev_args[MAX_RU_PORTS][64];
  int vdev_idx = 0;
  argv[argc++] = "oru_fh";
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

  // Select the system core. This is needed to corrently dispatch DPDK workers
  // The core will be returned to the applicaton after rte_eal_init
  int sys_core = -1;
  long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
  for (int i = 0; i < num_cpus; i++) {
    if (CPU_ISSET(i, &cpuset)) {
      if (i != cfg->rx_core) {
        sys_core = i;
        break;
      }
    }
  }
  char main_lcore_arg[32];
  if (sys_core != -1) {
    snprintf(lcores, sizeof(lcores), "%d,%d", sys_core, cfg->rx_core);
    argv[argc++] = "-l";
    argv[argc++] = lcores;
    // DPDK defaults to the lowest lcore ID as the main lcore. Explicitly set it so rx_core is always a worker lcore.
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
    // Restore affinity to all original cores except rx_core, which DPDK owns
    CPU_CLR(cfg->rx_core, &cpuset);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) != 0) {
      LOG_E(HW, "pthread_setaffinity_np error\n");
      return NULL;
    }
  }

  oru_fh_t *fh = (oru_fh_t *)malloc_or_fail(sizeof(oru_fh_t));
  memset(fh, 0, sizeof(oru_fh_t));
  fh->cfg = *cfg;
  fh->io_config.numerology = cfg->numerology;
  fh->io_config.clock_timebase = cfg->clock_timebase;
  fh->io_config.num_ports = cfg->dpdk_conf.num_dpdk_devices;
  for (int i = 0; i < cfg->dpdk_conf.num_dpdk_devices; i++) {
    if (rte_eth_dev_get_port_by_name(cfg->dpdk_conf.dpdk_devices[i], &fh->io_config.port_ids[i]) < 0) {
      fprintf(stderr, "DPDK device %s not found\n", cfg->dpdk_conf.dpdk_devices[i]);
      free(fh);
      return NULL;
    }
  }

  fh->io_config.num_macs = cfg->num_du_mac_addrs;
  for (int i = 0; i < cfg->num_du_mac_addrs; i++) {
    if (rte_ether_unformat_addr(cfg->du_mac_addrs[i], &fh->io_config.du_macs[i]) < 0) {
      fprintf(stderr, "Invalid MAC address: %s\n", cfg->du_mac_addrs[i]);
      free(fh);
      return NULL;
    }
  }

  fh->io_config.rx_cb = rx_cb;
  fh->io_config.rx_user_data = fh;
  fh->io_config.timer_cb = timer_cb;
  fh->io_config.timer_user_data = fh;

  // DPDK Port and Mbuf Pool configuration
  uint16_t header_size = sizeof(struct xran_up_pkt_hdr);
  uint16_t payload_size = cfg->num_prbs * 12 * 4; // 16-bit IQ (4 bytes per sample)
  uint16_t required_mtu = header_size + payload_size;

  printf("ORU_FH: PRBs %u -> Required MTU %u (Limit %u); clock_timebase=%s\n",
         cfg->num_prbs,
         required_mtu,
         cfg->mtu,
         cfg->clock_timebase == FH_CLOCK_TAI ? "tai" : "utc");

  fh->io_config.mbuf_data_room = required_mtu + RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + RTE_PKTMBUF_HEADROOM;
  fh->io_config.mbuf_count = 8192; // Default pool size
  fh->io_config.mtu = cfg->mtu;

  if (oru_io_init(&fh->io, &fh->io_config) < 0) {
    free(fh);
    return NULL;
  }

  oru_pcap_init_from_env(cfg->prach_eaxc_offset, cfg->mtu);

  fh->packet_processor = init_packet_processor(cfg->numerology,
                                               cfg->num_prbs,
                                               cfg->T2a_cp_min_uS,
                                               cfg->T2a_cp_max_uS,
                                               cfg->T2a_up_min_uS,
                                               cfg->T2a_up_max_uS,
                                               cfg->tdd_pattern.num_dl_slots,
                                               cfg->tdd_pattern.num_ul_slots,
                                               cfg->tdd_pattern.num_dl_symbols,
                                               cfg->tdd_pattern.num_ul_symbols,
                                               cfg->tdd_pattern.tdd_pattern_length_slots,
                                               (alloc_func_t)oru_io_get_sendbuf,
                                               (send_func_t)oru_io_send_uplane,
                                               &fh->io,
                                               cfg->mtu,
                                               cfg->prach_eaxc_offset,
                                               cfg->comp_type,
                                               cfg->prach_kbar);

  return fh;
}

void oru_fh_cleanup(void *handle)
{
  if (!handle)
    return;
  oru_fh_t *fh = (oru_fh_t *)handle;
  if (fh->packet_processor) {
    cleanup_packet_processor(fh->packet_processor);
  }
  oru_io_cleanup(&fh->io);
  free(fh);
}

int oru_fh_tx_read_symbol(void *handle, uint32_t **txdataF, int nb_tx, uint64_t *hyper_frame, int *frame, int *slot, int *symbol)
{
  if (!handle)
    return -1;
  oru_fh_t *fh = (oru_fh_t *)handle;
  read_dl_iq(fh->packet_processor, txdataF, nb_tx, hyper_frame, frame, slot, symbol);
  return 0;
}

int oru_fh_get_ready_jobs(void *handle)
{
  if (!handle)
    return 0;
  oru_fh_t *fh = (oru_fh_t *)handle;
  return get_ready_job_count(fh->packet_processor);
}

int oru_fh_get_utc_anchor_point(void *handle, uint64_t *hyper_frame, uint32_t *frame, uint32_t *slot, struct timespec *ts)
{
  if (!handle || !frame || !slot || !ts)
    return -1;
  oru_fh_t *fh = (oru_fh_t *)handle;
  uint64_t absolute_gps_symbol = fh_timer_get_current_symbol(&fh->io.timer);
  absolute_gps_symbol -= absolute_gps_symbol % NR_SYMBOLS_PER_SLOT; // Round down to start of current slot
  uint64_t absolute_slot = absolute_gps_symbol / NR_SYMBOLS_PER_SLOT;
  uint32_t slots_per_frame = 10 << fh->cfg.numerology;
  *hyper_frame = (absolute_slot / slots_per_frame) / 1024;
  *frame = (absolute_slot / slots_per_frame) % 1024;
  *slot = absolute_slot % slots_per_frame;

  uint64_t total_syms_per_sec = (NR_SYMBOLS_PER_SLOT * 1000) << fh->cfg.numerology;
  uint64_t ns_per_symbol = 1000000000 / total_syms_per_sec;
  uint64_t leftover_syms = absolute_gps_symbol % total_syms_per_sec;
  /* Invert get_gps_ns(): system_sec = gps_sec + epoch - clock_to_gps */
  int64_t clock_to_gps = (fh->io.timer.timebase == FH_CLOCK_TAI)
                             ? -(int64_t)GPS_TAI_LAG_SECONDS
                             : (int64_t)GPS_UTC_LEAP_SECONDS;
  ts->tv_sec = (absolute_gps_symbol / total_syms_per_sec) + GPS_EPOCH_OFFSET_UNIX - clock_to_gps;
  ts->tv_nsec = leftover_syms * ns_per_symbol;
  return 0;
}

void oru_fh_rx_send_prach(void *handle, uint32_t **prachF, int nb_rx, int frame, int slot, int symbol)
{
  oru_fh_t *fh = (oru_fh_t *)handle;
  AssertFatal(fh, "Invalid handle\n");
  write_prach_iq(fh->packet_processor, prachF, nb_rx, frame, slot, symbol);
}

int oru_fh_poll_ul_job(void *handle, ul_job_t *job) {
  oru_fh_t *fh = (oru_fh_t *)handle;
  AssertFatal(fh, "Invalid handle\n");
  return poll_ul_job(fh->packet_processor, job);
}

void oru_fh_get_dl_symbol_bitmask(void *handle, const uint8_t **bitmask, uint16_t *bit_length)
{
  oru_fh_t *fh = (oru_fh_t *)handle;
  AssertFatal(fh, "Invalid handle\n");
  get_dl_symbol_bitmask(fh->packet_processor, bitmask, bit_length);
}

void oru_fh_rx_send_pusch(void *handle, uint32_t *puschF, int symbol_index, const ul_job_t *job)
{
  oru_fh_t *fh = (oru_fh_t *)handle;
  AssertFatal(fh, "Invalid handle\n");
  write_ul_iq(fh->packet_processor, puschF, symbol_index, job);
}

int oru_fh_start(void *handle)
{
  if (!handle)
    return -1;
  oru_fh_t *fh = (oru_fh_t *)handle;

  return oru_io_run(&fh->io, fh->cfg.rx_core);
}

void oru_fh_stop(void *handle)
{
  if (!handle)
    return;
  oru_fh_t *fh = (oru_fh_t *)handle;
  print_packet_processor_stats(fh->packet_processor);
  oru_io_stop(&fh->io);

  // Wait for worker thread to finish
  if (fh->cfg.rx_core != -1 && fh->cfg.rx_core != RTE_MAX_LCORE)
    rte_eal_wait_lcore(fh->cfg.rx_core);
}

void oru_fh_print_stats(void *handle)
{
  if (!handle)
    return;
  oru_fh_t *fh = (oru_fh_t *)handle;
  print_packet_processor_stats(fh->packet_processor);
}
