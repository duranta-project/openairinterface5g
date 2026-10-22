/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include "common_lib.h"
#include "oran_isolate.h"
#include "oran-init.h"
#include "xran_fh_o_du.h"
#include "xran_sync_api.h"

#include "common/utils/LOG/log.h"
#include "openair1/PHY/defs_gNB.h"
#include "oaioran.h"
#include "oran-config.h"
#include "oran.h"

#include "common/ran_context.h"
#include "common/utils/fsn.h"
#include "common/utils/system.h"
#include "executables/softmodem-common.h"

// include the following file for VERSIONX, version of xran lib, to print it during
// startup. Only relevant for printing, if it ever makes problem, remove this
// line and the use of VERSIONX further below. It is relative to phy/fhi_lib/lib/api
#include "../../app/src/common.h"

#ifdef OAI_MPLANE
#include "mplane/init-mplane.h"
#include "mplane/connect-mplane.h"
#endif

/* fh_init/fh_config are kept until trx_oran_start() initializes xran from the FH thread */
typedef struct {
  void *oran_priv;
  void *mplane_priv;
  uint32_t nCC;
  uint32_t num_ports;
  int core; // xran io_cfg.system_core, used for split7_thread()
  struct xran_fh_init fh_init;
  struct xran_fh_config fh_config[XRAN_PORTS_NUM];
} oran_eth_state_t;

notifiedFIFO_t oran_sync_fifo;
notifiedFIFO_t oran_sync_fifo_prach;

/* Must be called from the FH thread, pinned to its final core */
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

static void oran_fh_if4p5_south_in(RU_t *ru, int frame, int slot)
{
  int ret = 0; // return code for PUSCH/PRACH processing

  const struct xran_fh_init *fh_init = get_xran_fh_init();

  for (uint16_t cc_id = 0; cc_id < 1 /*nSectorNum*/; cc_id++) { // OAI does not support multiple CC yet.
    for (int xran_port = 0; xran_port < fh_init->xran_ports; xran_port++) {
      oran_buf_list_t *bufs = get_xran_buffers(xran_port);
      const struct xran_fh_config *fh_cfg = get_xran_fh_config(xran_port);
      const uint8_t mu_number = fh_cfg->mu_number[0];
      const int slots_per_frame = 10 << mu_number;
      const int tti = slots_per_frame * frame + slot;
      const struct xran_frame_config *frame_conf = &fh_cfg->frame_conf;

      if (frame_conf->nFrameDuplexType == XRAN_FDD || is_tdd_ul_guard_slot(frame_conf, slot)) {
        const int fft_size = 1 << fh_cfg->perMu[mu_number].nULFftSize;
        ret = xran_fh_rx_read_slot(tti, xran_port, fh_cfg->neAxcUl, fft_size, bufs, ru->common.rxdataF);
        if (ret != 0) {
          LOG_W(HW, "[%d.%d] xran_fh_rx_read_slot error for xran_port %d\n", frame, slot, xran_port);
        }
        // check if it's a PRACH slot
        PHY_VARS_gNB *gNB = ru->gNB_list[0];
        prach_item_t p;
        fsn_t now = {.f = frame, .s = slot, .mu = gNB->frame_parms.numerology_index};
        if (get_next_nr_prach(&gNB->prach_ru_queue, &now, &p)) {
          int slots_per_subframe = 1 << mu_number;
          uint32_t subframe = slot / slots_per_subframe; // `slot` = slot in which PRACH is received
          // PRACH occasion in a frame if and only if SFN % x == y, TS 38.211 Table 6.3.3.2-2/3/4
          nr_prach_info_t prach_info = get_prach_info(xran_port);
          bool is_prach_frame = (frame % prach_info.x == prach_info.y);
          bool is_prach_slot = is_prach_frame && xran_is_prach_slot(0, subframe, (p.slot % slots_per_subframe), mu_number); // `p.slot` = slot in which PRACH is scheduled
          if (is_prach_slot) {
            ret = xran_fh_rx_prach_read_slot(tti, xran_port, fh_cfg->neAxcUl, &fh_cfg->ru_conf, &fh_cfg->perMu[mu_number].prach_conf, bufs, p);
            if (ret != 0) {
              LOG_W(HW, "[%d.%d] xran_fh_rx_prach_read_slot error for xran_port %d\n", frame, slot, xran_port);
            }
            // after reading PRACH, write back to queue
            bool success = spsc_q_put(&gNB->prach_l1rx_queue, &p, sizeof(p));
            // assume prach_l1rx_queue never full: prach_ru_queue filled at
            // constant pace, but prach_l1rx_queue emptied as fast as possible,
            // see rx_func()
            DevAssert(success);
          } else {
            LOG_W(HW, "[%d.%d] Expected PRACH reception of scheduled slot %d\n", frame, slot, p.slot);
          }
        }
      }
      print_fhi_counters(xran_port, fh_cfg->neAxcUl, frame, slot);
    }
  }

  RU_proc_t *proc = &ru->proc;
  int slots_per_frame = ru->nr_frame_parms->slots_per_frame;
  proc->tti_rx = slot;
  proc->frame_rx = frame;
  proc->tti_tx = (slot + ru->sl_ahead) % slots_per_frame;
  proc->frame_tx = (slot > (slots_per_frame - 1 - ru->sl_ahead)) ? (frame + 1) & 1023 : frame;

  if (proc->first_rx == 0) {
    if (proc->tti_rx != slot) {
      LOG_E(HW,
            "Received Time doesn't correspond to the time we think it is (slot mismatch, received %d.%d, expected %d.%d)\n",
            proc->frame_rx,
            proc->tti_rx,
            frame,
            slot);
      slot = proc->tti_rx;
    }

    if (proc->frame_rx != frame) {
      LOG_E(HW,
            "Received Time doesn't correspond to the time we think it is (frame mismatch, %d.%d , expected %d.%d)\n",
            proc->frame_rx,
            proc->tti_rx,
            frame,
            slot);
      frame = proc->frame_rx;
    }
  } else {
    proc->first_rx = 0;
    LOG_I(HW, "before adjusting, OAI: frame=%d slot=%d, XRAN: frame=%d slot=%d\n", frame, slot, proc->frame_rx, proc->tti_rx);
    frame = proc->frame_rx;
    slot = proc->tti_rx;
    LOG_I(HW, "After adjusting, OAI: frame=%d slot=%d, XRAN: frame=%d slot=%d\n", frame, slot, proc->frame_rx, proc->tti_rx);
  }
}

static void oran_fh_if4p5_south_ctrl(uint16_t **beam_id, int frame, int slot)
{
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
        ret = xran_send_cp_slot(tti, slot, xran_port, fh_cfg->neAxcUl, beam_id, bufs->dstcp);
        if (ret != 0) {
          LOG_W(HW, "[%d.%d] xran_send_cp_slot UL error for xran_port %d\n", frame, slot, xran_port);
        }
      }

      // DL slot
      if (frame_conf->nFrameDuplexType == XRAN_FDD || is_tdd_dl_guard_slot(frame_conf, slot)) {
        // Send CP DL
        ret = xran_send_cp_slot(tti, slot, xran_port, fh_cfg->neAxc, beam_id, bufs->srccp);
        if (ret != 0) {
          LOG_W(HW, "[%d.%d] xran_send_cp_slot DL error for xran_port %d\n", frame, slot, xran_port);
        }
      }
    }
  }
}

static void oran_fh_if4p5_south_out(int32_t **txdataF_BF, int frame, int slot)
{
  int ret;

  const struct xran_fh_init *fh_init = get_xran_fh_init();

  for (uint16_t cc_id = 0; cc_id < 1 /*nSectorNum*/; cc_id++) { // OAI does not support multiple CC yet.
    for (int xran_port = 0; xran_port < fh_init->xran_ports; xran_port++) {
      oran_buf_list_t *bufs = get_xran_buffers(xran_port);
      const struct xran_fh_config *fh_cfg = get_xran_fh_config(xran_port);
      const uint8_t mu_number = fh_cfg->mu_number[0];
      const int slots_per_frame = 10 << mu_number;
      const int tti = slots_per_frame * frame + slot;

      const int fft_size = 1 << fh_cfg->perMu[mu_number].nDLFftSize;
      ret = xran_fh_tx_send_slot(tti, xran_port, fh_cfg->neAxc, fft_size, txdataF_BF, bufs);
      if (ret != 0) {
        LOG_W(HW, "[%d.%d] xran_fh_tx_send_slot error for xran_port %d\n", frame, slot, xran_port);
      }
    }
  }
}

/* Gather xran (and M-plane) configuration; xran is initialized later by trx_oran_start() */
static oran_eth_state_t *oran_fhi72_configure(split7_config_t *s7cfg)
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
    ru_configured[i] = manage_ru(ru_session, s7cfg, ru_session_list->num_rus);
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

  success = get_xran_config(ru_session_list, s7cfg, fh_init, fh_config);
  AssertFatal(success, "[MPLANE] Cannot configure xran with M-plane info.\n");
#else
  success = get_xran_config(NULL, s7cfg, fh_init, fh_config);
  AssertFatal(success, "cannot get configuration for xran\n");
#endif

  eth->nCC = fh_config->nCC;
  eth->num_ports = fh_init->xran_ports;
  eth->core = fh_init->io_cfg.system_core;

  initNotifiedFIFO(&oran_sync_fifo);
  initNotifiedFIFO(&oran_sync_fifo_prach);

  return eth;
}

typedef struct {
  RU_t *ru;
  oran_eth_state_t *eth;
} split7_thread_arg_t;

/* @brief split 7.2 FH thread: starts xran, then per slot reads UL data and pushes the TX job.
 * Stops xran on exit. */
static void *split7_thread(void *param)
{
  split7_thread_arg_t *arg = param;
  RU_t *ru = arg->ru;
  oran_eth_state_t *eth = arg->eth;
  free(arg);

  RU_proc_t *proc = &ru->proc;
  PHY_VARS_gNB *gNB = RC.gNB[0]; // this RU main loop handles only one RU
  //int frame = 1023;
  //int slot = ru->nr_frame_parms->slots_per_frame - 1;

  if (trx_oran_start(eth) != 0)
    LOG_E(HW, "Could not start the O-RAN 7.2 fronthaul\n");

  wait_sync("split7_thread");

  uint32_t xran_frame; // [0-99]
  uint32_t xran_subframe; // [0-10]
  uint32_t xran_slot; // [0-7]
  uint64_t xran_second; // current UTC second
  // assuming all RUs have the same numerology
  uint8_t mu = eth->fh_config[0].mu_number[0];
  // current TTI number [0-7999]
  int32_t tti = xran_timingsource_get_slotidx(&xran_frame, &xran_subframe, &xran_slot, &xran_second, mu);

  int frame = (tti / ru->nr_frame_parms->slots_per_frame) & 1023;
  int slot = tti % ru->nr_frame_parms->slots_per_frame;
  while (!oai_exit) {
    if (slot==(ru->nr_frame_parms->slots_per_frame-1)) {
      slot=0;
      frame++;
      frame&=1023;
    } else {
      slot++;
    }

    oran_fh_if4p5_south_in(ru, frame, slot); // sets proc->{frame,tti}_{rx,tx}

    if (ru->rx_fhaul.trials > 1000) {
      reset_meas(&ru->rx_fhaul);
      reset_meas(&ru->tx_fhaul);
    }

    // no TX timestamp: xran schedules by frame/slot
    ru_push_tx_job(gNB, proc->frame_tx, proc->tti_tx, proc->frame_rx, proc->tti_rx, 0);
  }

  trx_oran_get_stats(eth);
  trx_oran_stop(eth);
  trx_oran_end(eth);

  free(gNB->fhi72_transport);
  LOG_I(PHY, "RU %d O-RAN 7.2 fronthaul stopped\n", ru->idx);

  return NULL;
}

/* Library entry point (see fhi72_init_t): configures xran, fills transport, spawns split7_thread() */
__attribute__((__visibility__("default"))) int oran_fhi72_init(split7_config_t *s7cfg, RU_t *ru, fhi72_transport_t *transport)
{
  oran_eth_state_t *eth = oran_fhi72_configure(s7cfg);

  transport->priv = eth;
  transport->fh_south_out = oran_fh_if4p5_south_out;
  transport->fh_south_ctrl = oran_fh_if4p5_south_ctrl;

  split7_thread_arg_t *arg = calloc_or_fail(1, sizeof(*arg));
  arg->ru = ru;
  arg->eth = eth;
  threadCreate(&ru->proc.pthread_FH, split7_thread, arg, "split7_thread", eth->core, OAI_PRIORITY_RT_MAX);

  return 0;
}
