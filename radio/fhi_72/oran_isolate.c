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
  struct xran_fh_init fh_init;
  struct xran_fh_config fh_config[XRAN_PORTS_NUM];
} oran_eth_state_t;

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

  for (int32_t port_id = 0; port_id < s->fh_init.xran_ports; port_id++) {
    if (xran_start(((void **)s->oran_priv)[port_id]) != 0) {
      printf("%s:%d:%s: Start ORAN port ID %d failed ... Exit\n", __FILE__, __LINE__, __FUNCTION__, port_id);
      exit(1);
    }
  }

  printf("Start ORAN. Done\n");

  for (int32_t port_id = 0; port_id < s->fh_init.xran_ports; port_id++) {
    for (int32_t cc_id = 0; cc_id < s->fh_config[port_id].nCC; cc_id++) {
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
  for (int32_t port_id = 0; port_id < s->fh_init.xran_ports; port_id++) {
    xran_close(((void **)s->oran_priv)[port_id]);
  }
  xran_cleanup();
  xran_mem_mgr_leak_detector_destroy();
}

static int trx_oran_stop(oran_eth_state_t *s)
{
  printf("ORAN: %s\n", __FUNCTION__);

  for (int32_t port_id = 0; port_id < s->fh_init.xran_ports; port_id++) {
    for (int32_t cc_id = 0; cc_id < s->fh_config[port_id].nCC; cc_id++) {
      xran_deactivate_cc(port_id, cc_id);
    }
  }

  xran_timingsource_stop();

  for (int32_t port_id = 0; port_id < s->fh_init.xran_ports; port_id++) {
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

static void oran_fh_if4p5_south_ctrl(split7_config_t *s7cfg, uint16_t **beam_id, int frame, int slot)
{
  int ret;

  const struct xran_fh_init *fh_init = get_xran_fh_init();

  for (uint16_t cc_id = 0; cc_id < 1 /*nSectorNum*/; cc_id++) { // OAI does not support multiple CC yet.
    for (int xran_port = 0; xran_port < fh_init->xran_ports; xran_port++) {
      oran_buf_list_t *bufs = get_xran_buffers(xran_port);
      const struct xran_fh_config *fh_cfg = get_xran_fh_config(xran_port);
      const int tti = s7cfg->slots_per_frame * frame + slot;

      // UL slot
      if (s7cfg->slot_dir[slot] == NR_UPLINK_SLOT || s7cfg->slot_dir[slot] == NR_MIXED_SLOT) { // FDD falls under NR_MIXED_SLOT
        // Send CP UL
        ret = xran_send_cp_slot(tti, slot, xran_port, fh_cfg->neAxcUl, beam_id, bufs->dstcp);
        if (ret != 0) {
          LOG_W(HW, "[%d.%d] xran_send_cp_slot UL error for xran_port %d\n", frame, slot, xran_port);
        }
      }

      // DL slot
      if (s7cfg->slot_dir[slot] == NR_DOWNLINK_SLOT || s7cfg->slot_dir[slot] == NR_MIXED_SLOT) { // FDD falls under NR_MIXED_SLOT
        // Send CP DL
        ret = xran_send_cp_slot(tti, slot, xran_port, fh_cfg->neAxc, beam_id, bufs->srccp);
        if (ret != 0) {
          LOG_W(HW, "[%d.%d] xran_send_cp_slot DL error for xran_port %d\n", frame, slot, xran_port);
        }
      }
    }
  }
}

static void oran_fh_if4p5_south_out(split7_config_t *s7cfg, int frame, int slot)
{
  int ret;

  const struct xran_fh_init *fh_init = get_xran_fh_init();

  for (uint16_t cc_id = 0; cc_id < 1 /*nSectorNum*/; cc_id++) { // OAI does not support multiple CC yet.
    for (int xran_port = 0; xran_port < fh_init->xran_ports; xran_port++) {
      oran_buf_list_t *bufs = get_xran_buffers(xran_port);
      const struct xran_fh_config *fh_cfg = get_xran_fh_config(xran_port);
      const uint8_t mu_number = fh_cfg->mu_number[0];
      const int tti = s7cfg->slots_per_frame * frame + slot;

      const int fft_size = 1 << fh_cfg->perMu[mu_number].nDLFftSize;
      ret = xran_fh_tx_send_slot(tti, xran_port, fh_cfg->neAxc, fft_size, s7cfg->txdataF_BF, bufs);
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

  oai_xran_slot_sync_init();

  return eth;
}

typedef struct {
  fhi72_transport_t *transport;
  PHY_VARS_gNB *gNB; // for prach_ru_queue/prach_l1rx_queue/ru_push_tx_job
  oran_eth_state_t *eth;
} split7_thread_arg_t;

static void process_ul_data(int tti, int frame, int slot, PHY_VARS_gNB *gNB, struct xran_fh_init fh_init, struct xran_fh_config *fh_cfg, split7_config_t *s7cfg)
{
  int ret;

  // PUxCH/SRS
  for (int xran_port = 0; xran_port < fh_init.xran_ports; xran_port++) {
    oran_buf_list_t *bufs = get_xran_buffers(xran_port);
    const int fft_size = 1 << fh_cfg[xran_port].perMu[s7cfg->mu].nULFftSize;
    ret = xran_fh_rx_read_slot(tti, xran_port, fh_cfg[xran_port].neAxcUl, fft_size, bufs, s7cfg->rxdataF);
    if (ret != 0)
      LOG_W(HW, "[%d.%d] xran_fh_rx_read_slot error for xran_port %d\n", frame, slot, xran_port);
  }

  // PRACH
  prach_item_t p;
  fsn_t now = {.f = frame, .s = slot, .mu = s7cfg->mu}; // assuming all RUs have the same numerology
  while (get_next_nr_prach(&gNB->prach_ru_queue, &now, &p)) {
    for (int xran_port = 0; xran_port < fh_init.xran_ports; xran_port++) {
      oran_buf_list_t *bufs = get_xran_buffers(xran_port);
      // PRACH occasion in a frame if and only if SFN % x == y, TS 38.211 Table 6.3.3.2-2/3/4
      const nr_prach_info_t info = get_prach_info(xran_port);
      const int slots_per_sf = 1 << s7cfg->mu;
      const bool is_prach_slot = frame % info.x == info.y && xran_is_prach_slot(xran_port, slot / slots_per_sf, p.slot % slots_per_sf, s7cfg->mu);
      if (is_prach_slot) {
        ret = xran_fh_rx_prach_read_slot(tti, xran_port, fh_cfg[xran_port].neAxcUl, &fh_cfg[xran_port].ru_conf, &fh_cfg[xran_port].perMu[s7cfg->mu].prach_conf, bufs, p);
        if (ret != 0)
          LOG_W(HW, "[%d.%d] xran_fh_rx_prach_read_slot error for xran_port %d\n", frame, slot, xran_port);
      } else {
        LOG_W(HW, "[%d.%d] xran_port %d Expected PRACH reception of scheduled slot %d\n", xran_port, frame, slot, p.slot);
      }
    }
    // prach_l1rx_queue never full: emptied by rx_func() every slot
    bool success = spsc_q_put(&gNB->prach_l1rx_queue, &p, sizeof(p));
    DevAssert(success);
  }
}

/* @brief split 7.2 FH thread: starts xran, then per slot reads UL data and pushes the TX job.
 * Stops xran on exit. */
static void *split7_thread(void *param)
{
  split7_thread_arg_t *arg = param;
  fhi72_transport_t *transport = arg->transport;
  split7_config_t *s7cfg = &transport->s7cfg;
  PHY_VARS_gNB *gNB = arg->gNB;
  oran_eth_state_t *eth = arg->eth;
  free(arg);

  // fatal: the loop below blocks on slots signalled by xran
  AssertFatal(trx_oran_start(eth) == 0, "Could not start the O-RAN 7.2 fronthaul\n");

  wait_sync("split7_thread");

  // xran accepts U-plane and sends C-plane only from an SFN wrap on (tti_to_phy_cb()): drop slots until frame 0, slot 0
  oai_xran_slot_t w;
  do {
    oai_xran_wait_slot(&w);
  } while (!oai_exit && w.tti != 0);
  LOG_I(HW, "SFN wrap: starting UL/TX processing\n");

  while (!oai_exit) {
    oai_xran_slot_t s;
    oai_xran_wait_slot(&s);

    // convert received TTI [0-7999] to OAI (frame,slot) pair
    int frame = (s.tti / s7cfg->slots_per_frame) & 1023;
    int slot = s.tti % s7cfg->slots_per_frame;
    LOG_D(HW, "Processing tti[%d] = frame[%d].slot[%d]\n", s.tti, frame, slot);

    if (s7cfg->slot_dir[slot] == NR_UPLINK_SLOT || s7cfg->slot_dir[slot] == NR_MIXED_SLOT) { // FDD falls under NR_MIXED_SLOT
      process_ul_data(s.tti, frame, slot, gNB, eth->fh_init, eth->fh_config, s7cfg);
    }

    if ((frame & 0x7f) == 0 && slot == 0) {
      print_fhi_counters(eth->fh_init, eth->fh_config);
    }

    int frame_tx = (slot > s7cfg->slots_per_frame - 1 - s7cfg->sl_ahead) ? (frame + 1) & 1023 : frame;
    int slot_tx = (slot + s7cfg->sl_ahead) % s7cfg->slots_per_frame;

    // no timestamp as 7.2 is a frequency domain interface
    ru_push_tx_job(gNB, frame_tx, slot_tx, frame, slot, 0);
  }

  trx_oran_stop(eth);
  trx_oran_end(eth);
  trx_oran_get_stats(eth);

  LOG_I(HW, "O-RAN 7.2 fronthaul stopped\n");

  return NULL;
}

__attribute__((__visibility__("default"))) int oran_fhi72_init(struct PHY_VARS_gNB_s *gNB)
{
  oran_eth_state_t *eth = oran_fhi72_configure(&gNB->fhi72_transport->s7cfg);

  gNB->fhi72_transport->fh_south_out = oran_fh_if4p5_south_out;
  gNB->fhi72_transport->fh_south_ctrl = oran_fh_if4p5_south_ctrl;

  split7_thread_arg_t *arg = calloc_or_fail(1, sizeof(*arg));
  arg->transport = gNB->fhi72_transport;
  arg->gNB = gNB;
  arg->eth = eth;
  threadCreate(&gNB->fhi72_transport->pthread, split7_thread, arg, "split7_thread", eth->fh_init.io_cfg.system_core, OAI_PRIORITY_RT_MAX);

  return 0;
}
