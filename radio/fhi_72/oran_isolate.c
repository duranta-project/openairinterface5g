/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <string.h>
#include "common_lib.h"
#include "radio/ETHERNET/ethernet_lib.h"
#include "oran_isolate.h"
#include "oran-init.h"
#include "xran_fh_o_du.h"
#include "xran_sync_api.h"

#include "common/utils/LOG/log.h"
#include "openair1/PHY/defs_gNB.h"
#include "oaioran.h"
#include "oran-config.h"
#include "oran.h"

#include "common/ran_context.h" // RC, for split7_thread()
#include "common/utils/system.h" // threadCreate()/OAI_PRIORITY_RT_MAX, for split7_thread()
#include "executables/softmodem-common.h" // oai_exit, for split7_thread()

// include the following file for VERSIONX, version of xran lib, to print it during
// startup. Only relevant for printing, if it ever makes problem, remove this
// line and the use of VERSIONX further below. It is relative to phy/fhi_lib/lib/api
#include "../../app/src/common.h"

#ifdef OAI_MPLANE
#include "mplane/init-mplane.h"
#include "mplane/connect-mplane.h"
#endif

typedef struct {
  void *oran_priv;
  void *mplane_priv;
  uint32_t nCC;
  uint32_t num_ports;
  int core; // xran io_cfg.system_core - CPU core split7_thread() pins itself to
  struct xran_fh_init fh_init;
  struct xran_fh_config fh_config[XRAN_PORTS_NUM];
} oran_eth_state_t;

notifiedFIFO_t oran_sync_fifo;
notifiedFIFO_t oran_sync_fifo_prach;

/* Must be called from the thread that will actually poll xran (the FH thread), after it is
 * already pinned to its final core - not from whichever thread loaded the transport. Moved here
 * (out of transport_init()'s body) so split7_thread() below can defer it the same way: xran/DPDK
 * set up per-lcore state tied to the calling thread. */
static int trx_oran_start(oran_eth_state_t *s)
{
  LOG_I(HW, "Initializing O-RAN 7.2 FH interface through xran library (compiled against headers of %s)\n", VERSIONX);
  s->oran_priv = oai_oran_initialize(&s->fh_init, s->fh_config);
  AssertFatal(s->oran_priv != NULL, "can not initialize fronthaul");

  printf("ORAN: %s\n", __FUNCTION__);

  // Start ORAN
  if (xran_timingsource_start() != 0) {
    printf("%s:%d:%s: Start timing source failed ... Exit\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  } else {
    printf("Start timing source. Done\n");
  }

  if (xran_start_worker_threads() != 0) {
    printf("%s:%d:%s: Start worker thread failed ... Exit\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  } else {
    printf("Start worker thread. Done\n");
  }

  xran_mem_mgr_leak_detector_display(0);

  for (int32_t port_id = 0; port_id < s->num_ports; port_id++) {
    if (xran_start(((void **)s->oran_priv)[port_id]) != 0) {
      printf("%s:%d:%s: Start ORAN port ID %d failed ... Exit\n", __FILE__, __LINE__, __FUNCTION__, port_id);
      exit(1);
    }
  }

  printf("Start ORAN. Done\n");

  for (int32_t cc_id = 0; cc_id < s->nCC; cc_id++) {
    for (int32_t port_id = 0; port_id < s->num_ports; port_id++) {
      if (xran_activate_cc(port_id, cc_id) != 0) {
        printf("%s:%d:%s: Activate CC failed ... Exit\n", __FILE__, __LINE__, __FUNCTION__);
        exit(1);
      } else {
        printf("Activate CC. Done\n");
      }
    }
  }

  return 0;
}

static void trx_oran_end(oran_eth_state_t *s)
{
  printf("ORAN: %s\n", __FUNCTION__);
  xran_shutdown(s->oran_priv);
  for (int32_t port_id = 0; port_id < s->num_ports; port_id++) {
    xran_close(((void **)s->oran_priv)[port_id]);
  }
  xran_cleanup();
  xran_mem_mgr_leak_detector_destroy();
}

static int trx_oran_stop(oran_eth_state_t *s)
{
  printf("ORAN: %s\n", __FUNCTION__);

  for (int32_t cc_id = 0; cc_id < s->nCC; cc_id++) {
    for (int32_t port_id = 0; port_id < s->num_ports; port_id++) {
      xran_deactivate_cc(port_id, cc_id);
    }
  }

  xran_timingsource_stop();

  for (int32_t port_id = 0; port_id < s->num_ports; port_id++) {
    xran_stop(((void **)s->oran_priv)[port_id]);
  }

#ifdef OAI_MPLANE
  printf("[MPLANE] Stopping M-plane.\n");
  disconnect_mplane(s->mplane_priv);
  free(s->mplane_priv);
#endif
  return (0);
}

static int trx_oran_get_stats(oran_eth_state_t *s)
{
  (void)s;
  uint64_t total_time, used_time;
  uint32_t num_core_used, core_used[64];
  uint32_t ret = xran_get_time_stats(&total_time, &used_time, &num_core_used, &core_used[0], 0);
  if (ret == 0)
    LOG_I(HW, "xran_get_time_stats(): total thread time %ld, total time essential tasks %ld, num cores used %d\n", total_time, used_time, num_core_used);
  printf("ORAN: %s\n", __FUNCTION__);
  return (0);
}

/* Adapters binding the shared trx_oran_* core above to the legacy openair0_device_t contract
 * (transport_init(), still used by executables/lte-ru.c). */
static int trx_oran_start_dev(openair0_device_t *device)
{
  return trx_oran_start(device->priv);
}
static int trx_oran_stop_dev(openair0_device_t *device)
{
  return trx_oran_stop(device->priv);
}
static void trx_oran_end_dev(openair0_device_t *device)
{
  trx_oran_end(device->priv);
}
static int trx_oran_get_stats_dev(openair0_device_t *device)
{
  return trx_oran_get_stats(device->priv);
}

/* Adapters binding the shared trx_oran_* core above to the fhi72_transport_t contract
 * (oran_fhi72_init(), used by executables/nr-ru.c). trx_oran_start() itself has no _fhi adapter -
 * it's only ever called from split7_thread(), in this same file, below. */
static int trx_oran_stop_fhi(fhi72_transport_t *t)
{
  return trx_oran_stop(t->priv);
}
static void trx_oran_end_fhi(fhi72_transport_t *t)
{
  trx_oran_end(t->priv);
}
static int trx_oran_get_stats_fhi(fhi72_transport_t *t)
{
  return trx_oran_get_stats(t->priv);
}

static void oran_fh_if4p5_south_in(RU_t *ru, int *frame, int *slot)
{
  int ret = 0; // return code for PUSCH/PRACH processing

  ru_info_t ru_info = {
      .nb_rx = ru->nb_rx,
      .nb_tx = ru->nb_tx,
      .rxdataF = ru->common.rxdataF,
      .beam_id = ru->common.beam_id,
      .prach_buf = NULL,
  };

  /* Firstly, process PUSCH packets */
  RU_proc_t *proc = &ru->proc; // to check if (frame,slot) combination corresponds to the expected PUSCH one
  int f, sl;
  LOG_D(HW, "Read rxdataF %p,%p\n", ru_info.rxdataF[0], ru_info.rxdataF[1]);
  start_meas(&ru->rx_fhaul);
  ret = xran_fh_rx_read_slot(&ru_info, &f, &sl);
  stop_meas(&ru->rx_fhaul);
  LOG_D(HW, "Read %d.%d rxdataF %p,%p\n", f, sl, ru_info.rxdataF[0], ru_info.rxdataF[1]);
  if (ret != 0) {
    printf("ORAN: %d.%d ORAN_fh_if4p5_south_in ERROR in RX function \n", f, sl);
  }

  /* Secondly, process PRACH packets */
  int f_prach, sl_prach;
  ret = xran_fh_rx_prach_read_slot(ru->gNB_list[0], &ru_info, &f_prach, &sl_prach);
  if (ret != 0) {
    printf("ORAN: %d.%d ORAN_fh_if4p5_south_in ERROR in RX PRACH function \n", f_prach, sl_prach);
  }

  int slots_per_frame = 10 << (ru->openair0_cfg.nr_scs_for_raster);
  proc->tti_rx = sl;
  proc->frame_rx = f;
  proc->tti_tx = (sl + ru->sl_ahead) % slots_per_frame;
  proc->frame_tx = (sl > (slots_per_frame - 1 - ru->sl_ahead)) ? (f + 1) & 1023 : f;

  if (proc->first_rx == 0) {
    print_fhi_counters(&ru_info, proc->frame_rx, proc->tti_rx);
    if (proc->tti_rx != *slot) {
      LOG_E(HW,
            "Received Time doesn't correspond to the time we think it is (slot mismatch, received %d.%d, expected %d.%d)\n",
            proc->frame_rx,
            proc->tti_rx,
            *frame,
            *slot);
      *slot = proc->tti_rx;
    }

    if (proc->frame_rx != *frame) {
      LOG_E(HW,
            "Received Time doesn't correspond to the time we think it is (frame mismatch, %d.%d , expected %d.%d)\n",
            proc->frame_rx,
            proc->tti_rx,
            *frame,
            *slot);
      *frame = proc->frame_rx;
    }
  } else {
    proc->first_rx = 0;
    LOG_I(HW, "before adjusting, OAI: frame=%d slot=%d, XRAN: frame=%d slot=%d\n", *frame, *slot, proc->frame_rx, proc->tti_rx);
    *frame = proc->frame_rx;
    *slot = proc->tti_rx;
    LOG_I(HW, "After adjusting, OAI: frame=%d slot=%d, XRAN: frame=%d slot=%d\n", *frame, *slot, proc->frame_rx, proc->tti_rx);
  }
}

static void oran_fh_if4p5_south_out(RU_t *ru, int frame, int slot, uint64_t timestamp)
{
  start_meas(&ru->tx_fhaul);

  int ret;
  const struct xran_fh_init *fh_init = get_xran_fh_init();

  for (uint16_t cc_id = 0; cc_id < 1 /*nSectorNum*/; cc_id++) { // OAI does not support multiple CC yet.
    for (int xran_port = 0; xran_port < fh_init->xran_ports; xran_port++) {
      oran_buf_list_t *bufs = get_xran_buffers(xran_port);
      const struct xran_fh_config *fh_cfg = get_xran_fh_config(xran_port);
      const uint8_t mu_number = fh_cfg->mu_number[0];
      const int slots_per_frame = 10 << mu_number;
      const int tti = slots_per_frame * frame + slot;
      const struct xran_frame_config *frame_conf = &fh_cfg->frame_conf;

      // UL slot
      if (frame_conf->nFrameDuplexType == XRAN_FDD || is_tdd_ul_guard_slot(frame_conf, slot)) {
        // Send CP UL
        ret = xran_send_cp_slot(fh_cfg->neAxcUl, ru->common.beam_id, tti, slot, bufs->dstcp);
        if (ret != 0) {
          LOG_W(HW, "[%d.%d] xran_send_cp_slot UL error for xran_port %d\n", frame, slot, xran_port);
        }
      }

      // DL slot
      if (frame_conf->nFrameDuplexType == XRAN_FDD || is_tdd_dl_guard_slot(frame_conf, slot)) {
        // Send CP DL
        ret = xran_send_cp_slot(fh_cfg->neAxc, ru->common.beam_id, tti, slot, bufs->srccp);
        if (ret != 0) {
          LOG_W(HW, "[%d.%d] xran_send_cp_slot DL error for xran_port %d\n", frame, slot, xran_port);
        }
        const int fft_size = 1 << fh_cfg->perMu[mu_number].nDLFftSize;
        ret = xran_fh_tx_send_slot(ru->common.txdataF_BF, fft_size, fh_cfg->neAxc, tti, bufs);
        if (ret != 0) {
          LOG_W(HW, "[%d.%d] xran_fh_tx_send_slot error for xran_port %d\n", frame, slot, xran_port);
        }
      }
    }
  }

  stop_meas(&ru->tx_fhaul);
}

static void *get_internal_parameter(char *name)
{
  printf("ORAN: %s\n", __FUNCTION__);

  if (!strcmp(name, "fh_if4p5_south_in"))
    return (void *)oran_fh_if4p5_south_in;
  if (!strcmp(name, "fh_if4p5_south_out"))
    return (void *)oran_fh_if4p5_south_out;

  return NULL;
}

/* Shared xran/M-plane configuration for both entry points below. Only gathers configuration -
 * does NOT call oai_oran_initialize()/start xran itself, see trx_oran_start() above. */
static oran_eth_state_t *oran_fhi72_configure(openair0_config_t *openair0_cfg)
{
  oran_eth_state_t *eth = calloc_or_fail(1, sizeof(*eth));
  struct xran_fh_init *fh_init = &eth->fh_init;
  struct xran_fh_config *fh_config = eth->fh_config;

  bool success = false;
#ifdef OAI_MPLANE
  ru_session_list_t *ru_session_list = calloc(1, sizeof(*ru_session_list));
  assert(ru_session_list != NULL && "Memory exhausted");
  success = init_mplane(ru_session_list);
  AssertFatal(success, "[MPLANE] Cannot initialize M-plane.\n");

  bool ru_configured[ru_session_list->num_rus];
  for (size_t i = 0; i < ru_session_list->num_rus; i++) {
    ru_session_t *ru_session = &ru_session_list->ru_session[i];
    ru_configured[i] = connect_mplane(ru_session);
    if (!ru_configured[i]) {
      continue;
    }
    ru_configured[i] = manage_ru(ru_session, openair0_cfg, ru_session_list->num_rus);
  }

  bool all_ok = true;
  bool ru_ready[ru_session_list->num_rus];
  for (size_t i = 0; i < ru_session_list->num_rus; i++) {
    if (!ru_configured[i]) {
      MP_LOG_I("RU with IP %s couldn't be configured.\n", ru_session_list->ru_session[i].ru_ip_add);
      all_ok = false;
    }
    ru_ready[i] = false;
  }

  if (!all_ok) {
    disconnect_mplane(ru_session_list);
    AssertFatal(false, "[MPLANE] Stopping M-plane.\n");
  }

  while (true) {
    sleep(1);
    bool all_rus_ready = true;
    for (int i = 0; i < ru_session_list->num_rus; i++) {
      ru_session_t *ru_session = &ru_session_list->ru_session[i];
      if (!ru_ready[i] && ru_session->ru_notif.config_change && !ru_session->ru_notif.rx_carrier_state && !ru_session->ru_notif.tx_carrier_state) {
        MP_LOG_I("RU \"%s\" is now ready.\n", ru_session->ru_ip_add);
        ru_ready[i] = true;
        if (!ru_session->pm_stats.start_up_timing) {
          success = pm_conf(ru_session, "true");
          if (success)
            MP_LOG_I("Sucessfully activated PM after start-up procedure for RU \"%s\".\n", ru_session->ru_ip_add);
        }
      } else {
        all_rus_ready = false;
        break;
      }
    }
    if (all_rus_ready) {
      break;
    }
  }

  eth->mplane_priv = ru_session_list;

  success = get_xran_config(ru_session_list, openair0_cfg, fh_init, fh_config);
  AssertFatal(success, "[MPLANE] Cannot configure xran with M-plane info.\n");
#else
  success = get_xran_config(NULL, openair0_cfg, fh_init, fh_config);
  AssertFatal(success, "cannot get configuration for xran\n");
#endif

  eth->nCC = fh_config->nCC;
  eth->num_ports = fh_init->xran_ports;
  eth->core = fh_init->io_cfg.system_core;

  initNotifiedFIFO(&oran_sync_fifo);
  initNotifiedFIFO(&oran_sync_fifo_prach);

  return eth;
}

/* Legacy entry point, dlsym'd by name ("transport_init") via openair0_transport_load() ->
 * load_lib() in radio/COMMON/common_lib.c. Still used by executables/lte-ru.c. */
__attribute__((__visibility__("default"))) int transport_init(openair0_device_t *device, openair0_config_t *openair0_cfg)
{
  oran_eth_state_t *eth = oran_fhi72_configure(openair0_cfg);

  device->host_type = RAU_HOST;
  device->transp_type = ETHERNET_TP;
  device->trx_start_func = trx_oran_start_dev;
  device->trx_get_stats_func = trx_oran_get_stats_dev;
  device->trx_end_func = trx_oran_end_dev;
  device->trx_stop_func = trx_oran_stop_dev;
  device->get_internal_parameter = get_internal_parameter;
  device->priv = eth;
  device->openair0_cfg = &openair0_cfg[0];

  return 0;
}

typedef struct {
  RU_t *ru;
  oran_eth_state_t *eth;
} split7_thread_arg_t;

/* @brief O-RAN 7.2 (REMOTE_IF4p5) FH thread. Lives here, not in executables/nr-ru.c, so it can
 * call oran_fh_if4p5_south_in()/trx_oran_start() directly - no function-pointer contract needed
 * for either, since caller and callee are in the same file.
 *
 * Deliberately a separate thread/function from ru_thread() (split 8, in nr-ru.c), not a reuse of
 * it: it is pinned to the CPU core xran itself wants (oran_eth_state_t::core, i.e. xran's
 * io_cfg.system_core), independent of the ru_thread_core config knob, and it has no IQ sample
 * clock to maintain - the frame/slot wraparound below just keeps a running count the same way
 * ru_thread() does, but there's no get_samples_per_slot()/timestamp accumulation on top of it,
 * since xran schedules TX by frame/slot, not by sample clock (oran_fh_if4p5_south_in() still
 * corrects frame/slot from xran's own timing, logging a mismatch if the two disagree).
 *
 * Unlike ru_thread(), this never calls ru_rx_slot(): split 7.2 has no time-domain front-end
 * processing, scope-copy, or (yet) PRACH extraction to do - the O-RU/xran side owns that. The only
 * part shared with ru_thread() is ru_push_tx_job() (in nr-ru.c, exposed via
 * openair1/PHY/defs_RU.h so this file can call it without duplicating that logic).
 *
 * trx_oran_start() (which runs oai_oran_initialize() and the xran/DPDK startup sequence) is
 * called here, first thing, rather than in oran_fhi72_init() before this thread is created:
 * xran/DPDK set up per-lcore state tied to the calling thread, so it has to run from this thread
 * once threadCreate() has already pinned it to its final core - not from whichever thread loaded
 * the transport. */
static void *split7_thread(void *param)
{
  split7_thread_arg_t *arg = param;
  RU_t *ru = arg->ru;
  oran_eth_state_t *eth = arg->eth;
  free(arg);

  RU_proc_t *proc = &ru->proc;
  PHY_VARS_gNB *gNB = RC.gNB[0]; // this RU main loop handles only one RU
  int frame = 1023;
  int slot = ru->nr_frame_parms->slots_per_frame - 1;

  if (trx_oran_start(eth) != 0)
    LOG_E(HW, "Could not start the O-RAN 7.2 fronthaul\n");

  LOG_I(PHY, "Signaling main thread that RU %d is ready, sl_ahead %d\n", ru->idx, ru->sl_ahead);
  pthread_mutex_lock(&RC.ru_mutex);
  RC.ru_mask &= ~(1 << ru->idx);
  pthread_cond_signal(&RC.ru_cond);
  pthread_mutex_unlock(&RC.ru_mutex);
  wait_sync("split7_thread");

  while (!oai_exit) {
    if (slot==(ru->nr_frame_parms->slots_per_frame-1)) {
      slot=0;
      frame++;
      frame&=1023;
    } else {
      slot++;
    }

    oran_fh_if4p5_south_in(ru, &frame, &slot); // also sets proc->{frame,tti}_{rx,tx} from xran's own timing

    if (ru->rx_fhaul.trials > 1000) {
      reset_meas(&ru->rx_fhaul);
      reset_meas(&ru->tx_fhaul);
    }

    // no IQ sample clock for split 7.2: xran schedules TX by frame/slot, not by timestamp
    ru_push_tx_job(gNB, proc->frame_tx, proc->tti_tx, proc->frame_rx, proc->tti_rx, 0);
  }

  return NULL;
}

/* NR entry point, dlsym'd by name ("oran_fhi72_init") via load_transport_shlib() in
 * radio/COMMON/common_lib.c, called from executables/nr-ru.c. Deliberately not shaped like
 * openair0_device_t - split 7.2 never does IQ-sample read/write, so that abstraction doesn't fit
 * here; see fhi72_transport_t in oran.h.
 *
 * Spawns and owns the FH thread itself (split7_thread() above) instead of handing a start
 * function back to nr-ru.c: a dlopen()'d module can't hand a function pointer back across the
 * boundary for the caller to threadCreate() with, since nr-ru.c has no link-time symbol for it. */
__attribute__((__visibility__("default"))) int oran_fhi72_init(openair0_config_t *openair0_cfg, RU_t *ru, fhi72_transport_t *transport)
{
  oran_eth_state_t *eth = oran_fhi72_configure(openair0_cfg);

  transport->priv = eth;
  transport->stop = trx_oran_stop_fhi;
  transport->end = trx_oran_end_fhi;
  transport->get_stats = trx_oran_get_stats_fhi;
  transport->fh_south_out = oran_fh_if4p5_south_out;

  split7_thread_arg_t *arg = calloc_or_fail(1, sizeof(*arg));
  arg->ru = ru;
  arg->eth = eth;
  threadCreate(&ru->proc.pthread_FH, split7_thread, arg, "split7_thread", eth->core, OAI_PRIORITY_RT_MAX);

  return 0;
}
