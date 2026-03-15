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

// include the following file for VERSIONX, version of xran lib, to print it during
// startup. Only relevant for printing, if it ever makes problem, remove this
// line and the use of VERSIONX further below. It is relative to phy/fhi_lib/lib/api
#include "../../app/src/common.h"

#ifdef OAI_MPLANE
#include "mplane/init-mplane.h"
#include "mplane/connect-mplane.h"
#endif

typedef struct {
  eth_state_t e;
  rru_config_msg_type_t last_msg;
  int capabilities_sent;
  void *oran_priv;
  void *mplane_priv;
  uint32_t nCC;
  uint32_t num_ports;
} oran_eth_state_t;

notifiedFIFO_t oran_sync_fifo;
#if defined K_RELEASE
notifiedFIFO_t oran_sync_fifo_prach;
#endif

int trx_oran_start(openair0_device_t *device)
{
  printf("ORAN: %s\n", __FUNCTION__);

  oran_eth_state_t *s = device->priv;

  // Start ORAN
#if defined K_RELEASE
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
#endif

#if defined F_RELEASE
  if (xran_start(s->oran_priv) != 0) {
    printf("%s:%d:%s: Start ORAN failed ... Exit\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
#elif defined K_RELEASE
  for (int32_t port_id = 0; port_id < s->num_ports; port_id++) {
    if (xran_start(((void **)s->oran_priv)[port_id]) != 0) {
      printf("%s:%d:%s: Start ORAN port ID %d failed ... Exit\n", __FILE__, __LINE__, __FUNCTION__, port_id);
      exit(1);
    }
  }
#endif

  printf("Start ORAN. Done\n");

#if defined K_RELEASE
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
#endif

  return 0;
}

void trx_oran_end(openair0_device_t *device)
{
  printf("ORAN: %s\n", __FUNCTION__);
  oran_eth_state_t *s = device->priv;
#if defined K_RELEASE
  xran_shutdown(s->oran_priv);
#endif
  xran_close(s->oran_priv);
#if defined K_RELEASE
  xran_cleanup();
  xran_mem_mgr_leak_detector_destroy();
#endif
}

int trx_oran_stop(openair0_device_t *device)
{
  printf("ORAN: %s\n", __FUNCTION__);
  oran_eth_state_t *s = device->priv;

#if defined K_RELEASE
  for (int32_t cc_id = 0; cc_id < s->nCC; cc_id++) {
    for (int32_t port_id = 0; port_id < s->num_ports; port_id++) {
      xran_deactivate_cc(port_id, cc_id);
    }
  }

  xran_timingsource_stop();
#endif

#if defined F_RELEASE
  xran_stop(s->oran_priv);
#elif defined K_RELEASE
  for (int32_t port_id = 0; port_id < s->num_ports; port_id++) {
    xran_stop(((void **)s->oran_priv)[port_id]);
  }
#endif

#ifdef OAI_MPLANE
  printf("[MPLANE] Stopping M-plane.\n");
  disconnect_mplane(s->mplane_priv);
  free(s->mplane_priv);
#endif
  return (0);
}

int trx_oran_set_freq(openair0_device_t *device, openair0_config_t *openair0_cfg)
{
  printf("ORAN: %s\n", __FUNCTION__);
  return (0);
}

int trx_oran_set_gains(openair0_device_t *device, openair0_config_t *openair0_cfg)
{
  printf("ORAN: %s\n", __FUNCTION__);
  return (0);
}

int trx_oran_get_stats(openair0_device_t *device)
{
  uint64_t total_time, used_time;
  uint32_t num_core_used, core_used[64];
  uint32_t ret = xran_get_time_stats(&total_time, &used_time, &num_core_used, &core_used[0], 0);
  if (ret == 0)
    LOG_I(HW, "xran_get_time_stats(): total thread time %ld, total time essential tasks %ld, num cores used %d\n", total_time, used_time, num_core_used);
  printf("ORAN: %s\n", __FUNCTION__);
  return (0);
}

int trx_oran_reset_stats(openair0_device_t *device)
{
  printf("ORAN: %s\n", __FUNCTION__);
  return (0);
}

int ethernet_tune(openair0_device_t *device, unsigned int option, int value)
{
  printf("ORAN: %s\n", __FUNCTION__);
  return 0;
}

int trx_oran_write_raw(openair0_device_t *device, openair0_timestamp_t timestamp, void **buff, int nsamps, int cc, int flags)
{
  printf("ORAN: %s\n", __FUNCTION__);
  return 0;
}

int trx_oran_read_raw(openair0_device_t *device, openair0_timestamp_t *timestamp, void **buff, int nsamps, int cc)
{
  printf("ORAN: %s\n", __FUNCTION__);
  return 0;
}

char *msg_type(int t)
{
  static char *s[12] = {
      "RAU_tick",
      "RRU_capabilities",
      "RRU_config",
      "RRU_config_ok",
      "RRU_start",
      "RRU_stop",
      "RRU_sync_ok",
      "RRU_frame_resynch",
      "RRU_MSG_max_num",
      "RRU_check_sync",
      "RRU_config_update",
      "RRU_config_update_ok",
  };

  if (t < 0 || t > 11)
    return "UNKNOWN";
  return s[t];
}

int trx_oran_ctlsend(openair0_device_t *device, void *msg, ssize_t msg_len)
{
  RRU_CONFIG_msg_t *rru_config_msg = msg;
  oran_eth_state_t *s = device->priv;

  printf("ORAN: %s\n", __FUNCTION__);

  printf("    rru_config_msg->type %d [%s]\n", rru_config_msg->type, msg_type(rru_config_msg->type));

  s->last_msg = rru_config_msg->type;

  return msg_len;
}

int trx_oran_ctlrecv(openair0_device_t *device, void *msg, ssize_t msg_len)
{
  RRU_CONFIG_msg_t *rru_config_msg = msg;
  oran_eth_state_t *s = device->priv;

  printf("ORAN: %s\n", __FUNCTION__);

  if (s->last_msg == RAU_tick && s->capabilities_sent == 0) {
    printf("ORAN ctrlrcv RRU_tick received and send capabilities hard coded\n");
    RRU_capabilities_t *cap;
    rru_config_msg->type = RRU_capabilities;
    rru_config_msg->len = sizeof(RRU_CONFIG_msg_t) - MAX_RRU_CONFIG_SIZE + sizeof(RRU_capabilities_t);
    // Fill RRU capabilities (see openair1/PHY/defs_RU.h)
    // For now they are hard coded - try to retreive the params from openari device

    cap = (RRU_capabilities_t *)&rru_config_msg->msg[0];
    cap->FH_fmt = OAI_IF4p5_only;
    cap->num_bands = 1;
    cap->band_list[0] = 78;
    // cap->num_concurrent_bands             = 1; component carriers
    cap->nb_rx[0] = 1; // device->openair0_cfg->rx_num_channels;
    cap->nb_tx[0] = 1; // device->openair0_cfg->tx_num_channels;
    cap->max_pdschReferenceSignalPower[0] = -27;
    cap->max_rxgain[0] = 90;
    cap->N_RB_DL[0] = 106;
    cap->N_RB_UL[0] = 106;

    s->capabilities_sent = 1;

    return rru_config_msg->len;
  }
  if (s->last_msg == RRU_config) {
    printf("Oran RRU_config\n");
    rru_config_msg->type = RRU_config_ok;
  }
  return 0;
}

void oran_fh_if4p5_south_in(RU_t *ru, int *frame, int *slot)
{
  int ret = 0; // return code for PUSCH/PRACH processing

  ru_info_t ru_info = {
      .nb_rx = ru->nb_rx,
      .nb_tx = ru->nb_tx,
      .rxdataF = ru->common.rxdataF,
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
#if defined F_RELEASE
  // no PRACH callback (no queue) in F release so use the expected combination
  f_prach = *frame;
  sl_prach = *slot;
#endif
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

void oran_fh_if4p5_south_out(RU_t *ru, int frame, int slot, uint64_t timestamp)
{
  start_meas(&ru->tx_fhaul);
  ru_info_t ru_info = {
      .nb_rx = ru->nb_rx,
      .nb_tx = ru->nb_tx,
      .txdataF_BF = ru->common.txdataF_BF,
  };

  // printf("south_out:\tframe=%d\tslot=%d\ttimestamp=%ld\n",frame,slot,timestamp);

  int ret = xran_fh_tx_send_slot(&ru_info, frame, slot, timestamp);
  if (ret != 0) {
    printf("ORAN: ORAN_fh_if4p5_south_out ERROR in TX function \n");
  }
  stop_meas(&ru->tx_fhaul);
}

void *get_internal_parameter(char *name)
{
  printf("ORAN: %s\n", __FUNCTION__);

  if (!strcmp(name, "fh_if4p5_south_in"))
    return (void *)oran_fh_if4p5_south_in;
  if (!strcmp(name, "fh_if4p5_south_out"))
    return (void *)oran_fh_if4p5_south_out;

  return NULL;
}

__attribute__((__visibility__("default"))) int transport_init(openair0_device_t *device,
                                                              openair0_config_t *openair0_cfg,
                                                              eth_params_t *eth_params)
{
  oran_eth_state_t *eth = calloc_or_fail(1, sizeof(*eth));

  struct xran_fh_init fh_init = {0};
  struct xran_fh_config fh_config[XRAN_PORTS_NUM] = {0};

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

  success = get_xran_config(ru_session_list, openair0_cfg, &fh_init, fh_config);
  AssertFatal(success, "[MPLANE] Cannot configure xran with M-plane info.\n");
#else
  success = get_xran_config(NULL, openair0_cfg, &fh_init, fh_config);
  AssertFatal(success, "cannot get configuration for xran\n");
#endif

  LOG_I(HW, "Initializing O-RAN 7.2 FH interface through xran library (compiled against headers of %s)\n", VERSIONX);
  eth->oran_priv = oai_oran_initialize(&fh_init, fh_config);
  AssertFatal(eth->oran_priv != NULL, "can not initialize fronthaul");
  // create message queues for ORAN sync

  initNotifiedFIFO(&oran_sync_fifo);
#if defined K_RELEASE
  initNotifiedFIFO(&oran_sync_fifo_prach);
#endif

  eth->e.flags = ETH_RAW_IF4p5_MODE;
  eth->e.compression = NO_COMPRESS;
  eth->e.if_name = eth_params->local_if_name;
  eth->last_msg = (rru_config_msg_type_t)-1;
  eth->nCC = fh_config->nCC;
  eth->num_ports = fh_init.xran_ports;

  device->transp_type = ETHERNET_TP;
  device->trx_start_func = trx_oran_start;
  device->trx_get_stats_func = trx_oran_get_stats;
  device->trx_reset_stats_func = trx_oran_reset_stats;
  device->trx_end_func = trx_oran_end;
  device->trx_stop_func = trx_oran_stop;
  device->trx_set_freq_func = trx_oran_set_freq;
  device->trx_set_gains_func = trx_oran_set_gains;
  device->trx_write_func = trx_oran_write_raw;
  device->trx_read_func = trx_oran_read_raw;
  device->trx_ctlsend_func = trx_oran_ctlsend;
  device->trx_ctlrecv_func = trx_oran_ctlrecv;
  device->get_internal_parameter = get_internal_parameter;
  device->priv = eth;
  device->openair0_cfg = &openair0_cfg[0];

  return 0;
}
