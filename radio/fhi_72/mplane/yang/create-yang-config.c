/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "create-yang-config.h"
#include "common/utils/nr/nr_common.h"

#define VERIFY_SUCCESS(var, message, args...) AssertError(var, return false, message, ##args)
const char *scs_name[] = {"KHZ_15", "KHZ_30", "KHZ_60", "KHZ_120", "KHZ_240", NULL};

#ifdef MPLANE_V2
static bool create_cu_interface_v2(const ru_session_t *ru_session, const size_t idx, const char *int_name, struct lyd_node **root)
{
  LY_ERR ret = LY_SUCCESS;

  ret = lyd_new_path(NULL, (struct ly_ctx *)ru_session->ctx, "/ietf-interfaces:interfaces", NULL, 0, root);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create root \"interfaces\" node.\n");

  struct lyd_node *interface_node = NULL;
  ret = lyd_new_list(*root, NULL, "interface", 0, &interface_node, int_name);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"interface\" node.\n");

  struct lyd_node *type_node = NULL;
  ret = lyd_new_term(interface_node, NULL, "type", "iana-if-type:l2vlan", 0, &type_node);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"type\" node.\n");

  ret = lyd_new_term(interface_node, NULL, "enabled", "true", 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"enabled\" node.\n");

  const struct lys_module *oran_int_mod = ly_ctx_get_module_implemented((struct ly_ctx *)ru_session->ctx, "o-ran-interfaces");
  VERIFY_SUCCESS(oran_int_mod != NULL, "[MPLANE] Failed to get \"o-ran-interfaces\" module.\n");

  struct lyd_node *base_int = NULL;
  ret = lyd_new_term(interface_node, oran_int_mod, "base-interface", ru_session->ru_mplane_config.interface_name, 0, &base_int);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"base-interface\" node with name \"%s\".\n", ru_session->ru_mplane_config.interface_name);

  struct lyd_node *vlan_id = NULL;
  char vlan_tag_str[8];
  snprintf(vlan_tag_str, sizeof(vlan_tag_str), "%d", ru_session->ru_mplane_config.vlan_tag[idx]);
  ret = lyd_new_term(interface_node, oran_int_mod, "vlan-id", vlan_tag_str, 0, &vlan_id);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"vlan-id\" node with \"%d\".\n", ru_session->ru_mplane_config.vlan_tag[idx]);

  struct lyd_node *ru_mac_addr = NULL;
  ret = lyd_new_term(interface_node, oran_int_mod, "mac-address", ru_session->xran_mplane.ru_mac_addr, 0, &ru_mac_addr);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"mac-address\" node with \"%s\".\n", ru_session->xran_mplane.ru_mac_addr);

  return true;
}

static bool create_proc_elem_v2(const ru_session_t *ru_session, const size_t idx, const char *int_name, struct lyd_node **root)
{
  LY_ERR ret = LY_SUCCESS;

  ret = lyd_new_path(NULL, (struct ly_ctx *)ru_session->ctx, "/o-ran-processing-element:processing-elements", NULL, 0, root);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create root \"processing-elements\" node.\n");

  ret = lyd_new_term(*root, NULL, "transport-session-type", "ETH-INTERFACE", 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"transport-session-type\" node.\n");

  struct lyd_node *ru_elem_node = NULL;
  char plane_name[8];
  snprintf(plane_name, sizeof(plane_name), "PLANE_%ld", idx);
  ret = lyd_new_list(*root, NULL, "ru-elements", 0, &ru_elem_node, plane_name);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"ru-elements\" node.\n");

  struct lyd_node *transp_flow = NULL;
  ret = lyd_new_inner(ru_elem_node, NULL, "transport-flow", 0, &transp_flow);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"transport-flow\" node.\n");

  ret = lyd_new_term(transp_flow, NULL, "interface-name", int_name, 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"interface-name\" node.\n");

  struct lyd_node *eth_flow = NULL;
  ret = lyd_new_inner(transp_flow, NULL, "eth-flow", 0, &eth_flow);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"eth-flow\" node.\n");

  char vlan_tag_str[8];
  snprintf(vlan_tag_str, sizeof(vlan_tag_str), "%d", ru_session->ru_mplane_config.vlan_tag[idx]);
  ret = lyd_new_term(eth_flow, NULL, "vlan-id", vlan_tag_str, 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"vlan-id\" node.\n");

  ret = lyd_new_term(eth_flow, NULL, "ru-mac-address", ru_session->xran_mplane.ru_mac_addr, 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"ru-mac-address\" node.\n");

  ret = lyd_new_term(eth_flow, NULL, "o-du-mac-address", ru_session->ru_mplane_config.du_mac_addr[idx], 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"o-du-mac-address\" node.\n");

  return true;
}

typedef enum { UP_CH_RX, UP_CH_TX, UP_CH_PRACH } uplane_dir_t;

static LY_ERR fill_uplane_ch_common_v2(const uplane_dir_t dir, const xran_mplane_t *xran_mplane, const split7_config_t *cfg, const size_t idx, struct lyd_node **root)
{
  LY_ERR ret = LY_SUCCESS;

  char frame_str[8];
  snprintf(frame_str, sizeof(frame_str), "%d", (cfg->fftSize << 4) + cfg->mu); // 3GPP TS 38.211
  ret = lyd_new_term(*root, NULL, "frame-structure", frame_str, 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"frame-structure\" node.\n");

  // EXTENDED not supported
  // 3GPP TS 38.211
  const char cp_type[] = "NORMAL";
  ret = lyd_new_term(*root, NULL, "cp-type", cp_type, 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"cp-type\" node.\n");

  char cp_len[8];
  snprintf(cp_len, sizeof(cp_len), "%d", cfg->cp_prefix0);
  ret = lyd_new_term(*root, NULL, "cp-length", cp_len, 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"cp-length\" node.\n");

  char cp_len_other[8];
  snprintf(cp_len_other, sizeof(cp_len_other), "%d", cfg->cp_prefix_other);
  ret = lyd_new_term(*root, NULL, "cp-length-other", cp_len_other, 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"cp-length-other\" node.\n");

  char freq_offset[8];
  if (dir == UP_CH_RX || dir == UP_CH_PRACH) {
    snprintf(freq_offset, sizeof(freq_offset), "%d", cfg->ul_k0[cfg->mu]);
  } else {
    snprintf(freq_offset, sizeof(freq_offset), "%d", cfg->dl_k0[cfg->mu]);
  }
  ret = lyd_new_term(*root, NULL, "offset-to-absolute-frequency-center", freq_offset, 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"offset-to-absolute-frequency-center\" node.\n");

  struct lyd_node *compression_node = NULL;
  ret = lyd_new_inner(*root, NULL, "compression", 0, &compression_node);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"compression\" node.\n");

  char iq_width_str[8];
  snprintf(iq_width_str, sizeof(iq_width_str), "%d", xran_mplane->iq_width);
  ret = lyd_new_term(compression_node, NULL, "iq-bitwidth", iq_width_str, 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"iq-bitwidth\" node.\n");

  char *comp_type = (xran_mplane->comp_hdr_type == 0) ? "DYNAMIC" : "STATIC";
  ret = lyd_new_term(compression_node, NULL, "compression-type", comp_type, 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"compression-type\" node.\n");

  if (xran_mplane->iq_width < 16) {
    ret = lyd_new_term(compression_node, NULL, "compression-method", "BLOCK_FLOATING_POINT", 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"compression-method\" node.\n");
  } else {
    ret = lyd_new_term(compression_node, NULL, "compression-method", "NO_COMPRESSION", 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"compression-method\" node.\n");
  }

  struct lyd_node *eaxc_conf = NULL;
  ret = lyd_new_inner(*root, NULL, "e-axcid", 0, &eaxc_conf);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"e-axcid\" node.\n");

  char eaxc_id[8];
  snprintf(eaxc_id, sizeof(eaxc_id), "%ld", idx);
  ret = lyd_new_term(eaxc_conf, NULL, "eaxc-id", eaxc_id, 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"eaxc-id\" node.\n");

  char du_port_bitmask[16];
  snprintf(du_port_bitmask, sizeof(du_port_bitmask), "%d", xran_mplane->du_port_bitmask);
  ret = lyd_new_term(eaxc_conf, NULL, "o-du-port-bitmask", du_port_bitmask, 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"o-du-port-bitmask\" node.\n");

  char band_sector_bitmask[8];
  snprintf(band_sector_bitmask, sizeof(band_sector_bitmask), "%d", xran_mplane->band_sector_bitmask);
  ret = lyd_new_term(eaxc_conf, NULL, "band-sector-bitmask", band_sector_bitmask, 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"band-sector-bitmask\" node.\n");

  char ccid_bitmask[8];
  snprintf(ccid_bitmask, sizeof(ccid_bitmask), "%d", xran_mplane->ccid_bitmask);
  ret = lyd_new_term(eaxc_conf, NULL, "ccid-bitmask", ccid_bitmask, 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"ccid-bitmask\" node.\n");

  char ru_port_bitmask[8];
  snprintf(ru_port_bitmask, sizeof(ru_port_bitmask), "%d", xran_mplane->ru_port_bitmask);
  ret = lyd_new_term(eaxc_conf, NULL, "ru-port-bitmask", ru_port_bitmask, 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"ru-port-bitmask\" node.\n");

  return LY_SUCCESS;
}

static bool fill_uplane_ch_rx_v2(const uplane_dir_t dir, const xran_mplane_t *xran_mplane, const split7_config_t *cfg, const size_t idx, struct lyd_node **root)
{
  LY_ERR ret = LY_SUCCESS;

  ret = fill_uplane_ch_common_v2(dir, xran_mplane, cfg, idx, root);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create common nodes.\n");

  const char *managed_delay = xran_mplane->managed_delay ? "true" : "false";
  ret = lyd_new_term(*root, NULL, "non-time-managed-delay-enabled", managed_delay, 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"non-time-managed-delay-enabled\" node.\n");

  struct lyd_node *fft_offset = NULL;
  ret = lyd_new_list(*root, NULL, "ul-fft-sampling-offsets", 0, &fft_offset, scs_name[cfg->mu]);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"ul-fft-sampling-offsets\" node.\n");

  // Note: set of allowed values is restricted by SCS derived from values in supported-frame-structures.
  char ul_fft_offset[8];
  int frame = 0;
  if (dir == UP_CH_RX) {
    frame = (cfg->fftSize << 4) + cfg->mu;
  } else if (dir == UP_CH_PRACH) {
    frame = (cfg->prach_fftSize << 4) + cfg->mu; // TODO: handle long PRACH
  }
  snprintf(ul_fft_offset, sizeof(ul_fft_offset), "%d", xran_mplane->frame_str - frame);
  ret = lyd_new_term(fft_offset, NULL, "ul-fft-sampling-offset", ul_fft_offset, 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"ul-fft-sampling-offset\" node.\n");

  return true;
}

static bool fill_uplane_ch_tx_v2(const xran_mplane_t *xran_mplane, const split7_config_t *cfg, const size_t idx, struct lyd_node **root)
{
  LY_ERR ret = LY_SUCCESS;

  ret = fill_uplane_ch_common_v2(UP_CH_TX, xran_mplane, cfg, idx, root);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create common nodes.\n");

  return true;
}

static bool create_uplane_conf_v2(const ru_session_t *ru_session, const split7_config_t *s7cfg, const size_t num_rus, const char *u_proc_elem, struct lyd_node **root)
{
  LY_ERR ret = LY_SUCCESS;
  bool success = true;

  ret = lyd_new_path(NULL, (struct ly_ctx *)ru_session->ctx, "/o-ran-uplane-conf:user-plane-configuration", NULL, 0, root);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create root \"user-plane-configuration\" node.\n");

  const size_t num_rx_ch = s7cfg->rx_num_channels / num_rus;

  // RX carriers
  const size_t num_rx_carriers = (ru_session->ru_mplane_config.rx_carriers.num == 1) ? 1 : num_rx_ch;
  for (size_t i = 0; i < num_rx_carriers; i++) {
    struct lyd_node *rx_carrier_node = NULL;
    ret = lyd_new_list(*root, NULL, "rx-array-carriers", 0, &rx_carrier_node, ru_session->ru_mplane_config.rx_carriers.name[i]);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"rx-array-carriers\" node.\n");

    char rx_freq[16];
    snprintf(rx_freq, sizeof(rx_freq), "%.0f", s7cfg->rx_freq[0]);
    ret = lyd_new_term(rx_carrier_node, NULL, "center-of-channel-bandwidth", rx_freq, 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"center-of-channel-bandwidth\" node.\n");

    char rx_arfcn[16];
    snprintf(rx_arfcn, sizeof(rx_arfcn), "%d", to_nrarfcn(s7cfg->rx_freq[0]));
    ret = lyd_new_term(rx_carrier_node, NULL, "absolute-frequency-center", rx_arfcn, 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"absolute-frequency-center\" node.\n");

    char rx_bw[16];
    snprintf(rx_bw, sizeof(rx_bw), "%.0f", s7cfg->rx_bw);
    ret = lyd_new_term(rx_carrier_node, NULL, "channel-bandwidth", rx_bw, 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"channel-bandwidth\" node.\n");

    ret = lyd_new_term(rx_carrier_node, NULL, "downlink-radio-frame-offset", "0", 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"downlink-radio-frame-offset\" node.\n");

    ret = lyd_new_term(rx_carrier_node, NULL, "downlink-sfn-offset", "0", 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"downlink-sfn-offset\" node.\n");

    // gain offset applied uniformly across all array elements or layers
    // needs to be within <gain-correction-range>
    ret = lyd_new_term(rx_carrier_node, NULL, "gain-correction", "0", 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"gain-correction\" node.\n");

    ret = lyd_new_term(rx_carrier_node, NULL, "n-ta-offset", "0", 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"n-ta-offset\" node.\n");

    ret = lyd_new_term(rx_carrier_node, NULL, "active", "ACTIVE", 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"active\" node.\n");
  }

  const size_t num_tx_ch = s7cfg->tx_num_channels / num_rus;

  // TX carriers
  const size_t num_tx_carriers = (ru_session->ru_mplane_config.tx_carriers.num == 1) ? 1 : num_tx_ch;
  for (size_t i = 0; i < num_tx_carriers; i++) {
    struct lyd_node *tx_carrier_node = NULL;
    ret = lyd_new_list(*root, NULL, "tx-array-carriers", 0, &tx_carrier_node, ru_session->ru_mplane_config.tx_carriers.name[i]);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"tx-array-carriers\" node.\n");

    char tx_freq[16];
    snprintf(tx_freq, sizeof(tx_freq), "%.0f", s7cfg->tx_freq[0]);
    ret = lyd_new_term(tx_carrier_node, NULL, "center-of-channel-bandwidth", tx_freq, 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"center-of-channel-bandwidth\" node.\n");

    char tx_arfcn[16];
    snprintf(tx_arfcn, sizeof(tx_arfcn), "%d", to_nrarfcn(s7cfg->tx_freq[0]));
    ret = lyd_new_term(tx_carrier_node, NULL, "absolute-frequency-center", tx_arfcn, 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"absolute-frequency-center\" node.\n");

    char tx_bw[16];
    snprintf(tx_bw, sizeof(tx_bw), "%.0f", s7cfg->tx_bw);
    ret = lyd_new_term(tx_carrier_node, NULL, "channel-bandwidth", tx_bw, 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"channel-bandwidth\" node.\n");

    // s7cfg->tx_gain takes the value of att_tx from the RU section of gNB config file
    // we assume the same gain is applied to all channels; same as above for the frequency
    const double tx_gain_value = ((s7cfg->tx_gain[0] * 10000) < ru_session->xran_mplane.max_tx_gain) ? (s7cfg->tx_gain[0] * 10000) : ru_session->xran_mplane.max_tx_gain;
    char tx_gain_str[8];
    snprintf(tx_gain_str, sizeof(tx_gain_str), "%.f", tx_gain_value);
    ret = lyd_new_term(tx_carrier_node, NULL, "gain", tx_gain_str, 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"gain\" node.\n");
  
    ret = lyd_new_term(tx_carrier_node, NULL, "downlink-radio-frame-offset", "0", 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"downlink-radio-frame-offset\" node.\n");

    ret = lyd_new_term(tx_carrier_node, NULL, "downlink-sfn-offset", "0", 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"downlink-sfn-offset\" node.\n");

    ret = lyd_new_term(tx_carrier_node, NULL, "active", "ACTIVE", 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"active\" node.\n");
  }

  // this is a hack due to Benetel limitation
  // 2x2 mode requires 2 odd (1+3) or 2 even (0+2) antennas
  int i_rx = (num_rx_ch == 2) ? 2 : 1;
  int i_tx = (num_tx_ch == 2) ? 2 : 1;

  // PUSCH & PRACH endpoints
  for (size_t i = 0; i < num_rx_ch; i++) {
    struct lyd_node *pusch_node = NULL;
    ret = lyd_new_list(*root, NULL, "low-level-rx-endpoints", 0, &pusch_node, ru_session->ru_mplane_config.rx_endpoints.name[i * i_rx]);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"low-level-rx-endpoints\" node.\n");
    
    success = fill_uplane_ch_rx_v2(UP_CH_RX, &ru_session->xran_mplane, s7cfg, i, &pusch_node);
    VERIFY_SUCCESS(success, "[MPLANE] Failed to fill \"low-level-rx-endpoints\" node for %s.\n", ru_session->ru_mplane_config.rx_endpoints.name[i * i_rx]);

    const size_t prach_endpoint_name_offset = i * i_rx + (ru_session->ru_mplane_config.rx_endpoints.num / 2);
    struct lyd_node *prach_node = NULL;
    ret = lyd_new_list(*root, NULL, "low-level-rx-endpoints", 0, &prach_node, ru_session->ru_mplane_config.rx_endpoints.name[prach_endpoint_name_offset]);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"low-level-rx-endpoints\" node.\n");

    success = fill_uplane_ch_rx_v2(UP_CH_PRACH, &ru_session->xran_mplane, s7cfg, i + ru_session->xran_mplane.prach_offset, &prach_node);
    VERIFY_SUCCESS(success, "[MPLANE] Failed to fill \"low-level-rx-endpoints\" node for %s.\n", ru_session->ru_mplane_config.rx_endpoints.name[prach_endpoint_name_offset]);
  }

  // PDSCH endpoints
  for (size_t i = 0; i < num_tx_ch; i++) {
    struct lyd_node *pdsch_node = NULL;
    ret = lyd_new_list(*root, NULL, "low-level-tx-endpoints", 0, &pdsch_node, ru_session->ru_mplane_config.tx_endpoints.name[i * i_tx]);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"low-level-tx-endpoints\" node.\n");

    success = fill_uplane_ch_tx_v2(&ru_session->xran_mplane, s7cfg, i, &pdsch_node);
    VERIFY_SUCCESS(success, "[MPLANE] Failed to fill \"low-level-tx-endpoints\" node for %s.\n", ru_session->ru_mplane_config.tx_endpoints.name[i * i_tx]);
  }

  // PUSCH and PRACH links
  for (size_t i = 0; i < num_rx_ch; i++) {
    const size_t rx_carrier_id = (num_rx_carriers == 1) ? 0 : i;
    // PUSCH
    struct lyd_node *pusch_link_node = NULL;
    char pusch_link[16];
    snprintf(pusch_link, sizeof(pusch_link), "%s%ld", "PuschLink", i);
    ret = lyd_new_list(*root, NULL, "low-level-rx-links", 0, &pusch_link_node, pusch_link);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"low-level-rx-links\" node.\n");

    ret = lyd_new_term(pusch_link_node, NULL, "processing-element", u_proc_elem, 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"processing-element\" node.\n");

    ret = lyd_new_term(pusch_link_node, NULL, "rx-array-carrier", ru_session->ru_mplane_config.rx_carriers.name[rx_carrier_id], 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"rx-array-carrier\" node.\n");

    ret = lyd_new_term(pusch_link_node, NULL, "low-level-rx-endpoint", ru_session->ru_mplane_config.rx_endpoints.name[i * i_rx], 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"low-level-rx-endpoint\" node.\n");

    // PRACH
    struct lyd_node *prach_link_node = NULL;
    char prach_link[16];
    snprintf(prach_link, sizeof(prach_link), "%s%ld", "PrachLink", i);
    ret = lyd_new_list(*root, NULL, "low-level-rx-links", 0, &prach_link_node, prach_link);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"low-level-rx-links\" node.\n");

    ret = lyd_new_term(prach_link_node, NULL, "processing-element", u_proc_elem, 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"processing-element\" node.\n");

    ret = lyd_new_term(prach_link_node, NULL, "rx-array-carrier", ru_session->ru_mplane_config.rx_carriers.name[rx_carrier_id], 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"rx-array-carrier\" node.\n");

    const size_t prach_endpoint_name_offset = i * i_rx + (ru_session->ru_mplane_config.rx_endpoints.num / 2);
    ret = lyd_new_term(prach_link_node, NULL, "low-level-rx-endpoint", ru_session->ru_mplane_config.rx_endpoints.name[prach_endpoint_name_offset], 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"low-level-rx-endpoint\" node.\n");
  }

  // PDSCH links
  for (size_t i = 0; i < num_tx_ch; i++) {
    const size_t tx_carrier_id = (num_tx_carriers == 1) ? 0 : i;
    struct lyd_node *pdsch_link_node = NULL;
    char pdsch_link[16];
    snprintf(pdsch_link, sizeof(pdsch_link), "%s%ld", "PdschLink", i);
    ret = lyd_new_list(*root, NULL, "low-level-tx-links", 0, &pdsch_link_node, pdsch_link);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"low-level-tx-links\" node.\n");

    ret = lyd_new_term(pdsch_link_node, NULL, "processing-element", u_proc_elem, 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"processing-element\" node.\n");

    ret = lyd_new_term(pdsch_link_node, NULL, "tx-array-carrier", ru_session->ru_mplane_config.tx_carriers.name[tx_carrier_id], 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"tx-array-carrier\" node.\n");

    ret = lyd_new_term(pdsch_link_node, NULL, "low-level-tx-endpoint", ru_session->ru_mplane_config.tx_endpoints.name[i * i_tx], 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"low-level-tx-endpoint\" node.\n");
  }

  return true;
}

static
bool create_pm_conf_v2(const ru_session_t *ru_session, const char *active, struct lyd_node **root)
{
  LY_ERR ret = LY_SUCCESS;

  ret = lyd_new_path(NULL, (struct ly_ctx *)ru_session->ctx, "/o-ran-performance-management:performance-measurement-objects", NULL, 0, root);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create root \"performance-measurement-objects\" node.\n");

  ret = lyd_new_term(*root, NULL, "notification-interval", "10", 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"notification-interval\" node.\n");

  ret = lyd_new_term(*root, NULL, "rx-window-measurement-interval", "10", 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"rx-window-measurement-interval\" node.\n");

  ret = lyd_new_term(*root, NULL, "tx-measurement-interval", "10", 0, NULL);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"tx-measurement-interval\" node.\n");

  for (size_t i = 0; i < ru_session->pm_stats.rx_num; i++) {
    struct lyd_node *rx_window_meas_objects = NULL;
    ret = lyd_new_list(*root, NULL, "rx-window-measurement-objects", 0, &rx_window_meas_objects, ru_session->pm_stats.rx_window_meas[i]);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"rx-window-measurement-objects\" node.\n");

    ret = lyd_new_term(rx_window_meas_objects, NULL, "active", active, 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"active\" node.\n");

    ret = lyd_new_term(rx_window_meas_objects, NULL, "object-unit", "RU", 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"object-unit\" node.\n");

    ret = lyd_new_term(rx_window_meas_objects, NULL, "report-info", "COUNT", 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"report-info\" node.\n");
  }

  for (size_t i = 0; i < ru_session->pm_stats.tx_num; i++) {
    struct lyd_node *tx_meas_objects = NULL;
    ret = lyd_new_list(*root, NULL, "tx-measurement-objects", 0, &tx_meas_objects, ru_session->pm_stats.tx_meas[i]);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"tx-measurement-objects\" node.\n");

    ret = lyd_new_term(tx_meas_objects, NULL, "active", active, 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"active\" node.\n");

    ret = lyd_new_term(tx_meas_objects, NULL, "object-unit", "RU", 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"object-unit\" node.\n");

    ret = lyd_new_term(tx_meas_objects, NULL, "report-info", "COUNT", 0, NULL);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Failed to create \"report-info\" node.\n");
  }

  return true;
}
#endif

bool configure_ru_from_yang(const ru_session_t *ru_session, const split7_config_t *s7cfg, const size_t num_rus, char **result)
{
  bool success = false;

#ifdef MPLANE_V1
  VERIFY_SUCCESS(success, "[MPLANE] To be implemented and tested.\n");
#elif defined MPLANE_V2
  const ru_mplane_config_t *ru_config = &ru_session->ru_mplane_config;
  size_t num_cu_planes = ru_config->num_cu_planes;
  if (num_cu_planes == 2 && ru_config->vlan_tag[0] == ru_config->vlan_tag[1]) {
    MP_LOG_I("The VLAN tags for C and U plane for the RU \"%s\" are the same. Therefore, configuring one common interface and one processing element.\n", ru_session->ru_ip_add);
    num_cu_planes = 1;
  }

  struct lyd_node *all_merge = NULL;
  LY_ERR ret = LY_SUCCESS;

  for (size_t i = 0; i < num_cu_planes; i++) {
    // <ietf-interfaces>
    struct lyd_node *cu_interface = NULL;
    char int_name[12];
    snprintf(int_name, sizeof(int_name), "INTERFACE_%ld", i);
    success = create_cu_interface_v2(ru_session, i, int_name, &cu_interface);
    VERIFY_SUCCESS(success, "[MPLANE] Cannot create CU-plane interface.\n");

    // <o-ran-processing-element>
    struct lyd_node *proc_elem = NULL;
    success = create_proc_elem_v2(ru_session, i, int_name, &proc_elem);
    VERIFY_SUCCESS(success, "[MPLANE] Cannot create CU-plane processing element.\n");

    ret = lyd_merge_siblings(&all_merge, cu_interface, 0);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Cannot merge CU-plane interface.\n");
    ret = lyd_merge_siblings(&all_merge, proc_elem, 0);
    VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Cannot merge processing element.\n");

    lyd_free_tree(cu_interface);
    lyd_free_tree(proc_elem);
  }

  // <o-ran-uplane-conf>
  struct lyd_node *uplane_conf = NULL;
  success = create_uplane_conf_v2(ru_session, s7cfg, num_rus, "PLANE_0", &uplane_conf);
  VERIFY_SUCCESS(success, "[MPLANE] Cannot create U-plane configuration.\n");
 
  ret = lyd_merge_siblings(&all_merge, uplane_conf, 0);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Cannot merge CU-plane interface, processing element and U-plane configuration.\n");

  // <o-ran-pm>
  struct lyd_node *pm_conf = NULL;
  if (ru_session->pm_stats.start_up_timing) {
    success = create_pm_conf_v2(ru_session, "true", &pm_conf);
    VERIFY_SUCCESS(success, "[MPLANE] Cannot create PM activation.\n");
  } else {
    success = create_pm_conf_v2(ru_session, "false", &pm_conf);
    VERIFY_SUCCESS(success, "[MPLANE] Cannot create PM deactivation.\n");
  }

  ret = lyd_merge_siblings(&all_merge, pm_conf, 0);
  VERIFY_SUCCESS(ret == LY_SUCCESS, "[MPLANE] Cannot merge CU-plane interface, processing element, U-plane configuration and PM configuration.\n");

  lyd_print_mem(result, all_merge, LYD_XML, LYD_PRINT_WITHSIBLINGS);

  lyd_free_siblings(all_merge);
  lyd_free_siblings(uplane_conf);
  lyd_free_siblings(pm_conf);
#endif

  return true;
}

char *get_pm_content(const ru_session_t *ru_session, const char *active)
{
  bool success = false;
  char *content = NULL;

#ifdef MPLANE_V1
  VERIFY_SUCCESS(success, "[MPLANE] To be implemented and tested.\n");
#elif defined MPLANE_V2
  struct lyd_node *pm_conf = NULL;
  success = create_pm_conf_v2(ru_session, active, &pm_conf);
  AssertError(success, return false, "[MPLANE] Cannot create PM configuration.\n");
  lyd_print_mem(&content, pm_conf, LYD_XML, LYD_PRINT_WITHSIBLINGS);
  lyd_free_siblings(pm_conf);
#endif

  return content;
}
