/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "debug.h"
#include "nfapi/oai_integration/vendor_ext.h"
#include "nfapi_nr_pnf_interface.h"
#include "nfapi_nr_interface.h"
#include "nfapi_nr_interface_scf.h"

#include "nfapi.h"
#include "common/ran_context.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <assert.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <pnf.h>

#include <vendor_ext.h>

#include "common/utils/LOG/log.h"
#include "common/utils/system.h"

#include "PHY/INIT/nr_phy_init.h"
#include "openair1/PHY/defs_gNB.h"
#include "executables/nr-softmodem.h"
#include "executables/softmodem-common.h"
#include "nfapi/open-nFAPI/pnf/inc/pnf_p7.h"

#include "openair2/NR_PHY_INTERFACE/NR_IF_Module.h"

#include "pnf/nfapi_common_pnf.h"
#include "pnf/nfapi_nr_pnf.h"

#ifdef ENABLE_WLS
#include <nr_fapi_p5.h>
#include <nr_fapi_p7.h>
#include "nfapi/oai_integration/wls_integration/include/wls_pnf.h"
#endif

#ifdef ENABLE_SOCKET
#include <socket/include/socket_pnf.h>
#endif

extern int config_sync_var;
extern RAN_CONTEXT_t RC;

extern pthread_cond_t nfapi_sync_cond;
extern pthread_mutex_t nfapi_sync_mutex;
extern int nfapi_sync_var;

extern int sync_var;

static nfapi_nr_pnf_p7_config_t *p7_config_g = NULL;
static pnf_info pnf;
static pthread_t pnf_p5_init_and_receive_pthread;
static bool has_sent_stop_ind = false;

static void *pnf_allocate(size_t size)
{
  return malloc(size);
}

static void pnf_deallocate(void *ptr)
{
  free(ptr);
}

static void pnf_nfapi_trace(nfapi_trace_level_t nfapi_level, const char *message, ...)
{
  va_list args;
  va_start(args, message);
  VLOG(NFAPI_PNF, nfapitooai_level(nfapi_level), message, args);
  va_end(args);
}

int pnf_nr_param_request(nfapi_nr_pnf_config_t *config, nfapi_nr_pnf_param_request_t *req)
{
  UNUSED(req)
  printf("[PNF] pnf param request\n");
  nfapi_nr_pnf_param_response_t resp;
  memset(&resp, 0, sizeof(resp));
  resp.header.message_id = NFAPI_PNF_PARAM_RESPONSE;
  resp.error_code = NFAPI_MSG_OK;
  pnf_info *pnf = (pnf_info *)(config->user_data);
  resp.pnf_param_general.tl.tag = NFAPI_PNF_PARAM_GENERAL_TAG;
  resp.pnf_param_general.nfapi_sync_mode = pnf->sync_mode;
  resp.pnf_param_general.location_mode = pnf->location_mode;
  resp.pnf_param_general.dl_config_timing = pnf->dl_config_timing;
  resp.pnf_param_general.tx_timing = pnf->tx_timing;
  resp.pnf_param_general.ul_config_timing = pnf->ul_config_timing;
  resp.pnf_param_general.hi_dci0_timing = pnf->hi_dci0_timing;
  resp.pnf_param_general.maximum_number_phys = pnf->max_phys;
  resp.pnf_param_general.maximum_total_bandwidth = pnf->max_total_bw;
  resp.pnf_param_general.maximum_total_number_dl_layers = pnf->max_total_dl_layers;
  resp.pnf_param_general.maximum_total_number_ul_layers = pnf->max_total_ul_layers;
  resp.pnf_param_general.shared_bands = pnf->shared_bands;
  resp.pnf_param_general.shared_pa = pnf->shared_pa;
  resp.pnf_param_general.maximum_total_power = pnf->max_total_power;
  resp.num_tlvs = 1;
  resp.pnf_phy.tl.tag = NFAPI_PNF_PHY_TAG;
  resp.pnf_phy.number_of_phys = 1;

  for (int i = 0; i < 1; ++i) {
    resp.pnf_phy.phy[i].phy_config_index = pnf->phys[i].index;
    resp.pnf_phy.phy[i].downlink_channel_bandwidth_supported = pnf->phys[i].dl_channel_bw_support;
    resp.pnf_phy.phy[i].uplink_channel_bandwidth_supported = pnf->phys[i].ul_channel_bw_support;
    resp.pnf_phy.phy[i].number_of_dl_layers_supported = pnf->phys[i].num_dl_layers_supported;
    resp.pnf_phy.phy[i].number_of_ul_layers_supported = pnf->phys[i].num_ul_layers_supported;
    resp.pnf_phy.phy[i].maximum_3gpp_release_supported = pnf->phys[i].release_supported;
    resp.pnf_phy.phy[i].nmm_modes_supported = pnf->phys[i].nmm_modes_supported;
    resp.pnf_phy.phy[i].number_of_rfs = 2;

    for (int j = 0; j < 1; ++j) {
      resp.pnf_phy.phy[i].rf_config[j].rf_config_index = pnf->phys[i].rfs[j];
    }

    resp.pnf_phy.phy[i].number_of_rf_exclusions = 0;

    for (int j = 0; j < 0; ++j) {
      resp.pnf_phy.phy[i].excluded_rf_config[j].rf_config_index = pnf->phys[i].excluded_rfs[j];
    }
    resp.num_tlvs++;
  }
  nfapi_nr_pnf_pnf_param_resp(config, &resp);
  return 0;
}

int pnf_nr_config_request(nfapi_nr_pnf_config_t *config, nfapi_nr_pnf_config_request_t *req)
{
  printf("[PNF] pnf config request\n");
  pnf_info *pnf = (pnf_info *)(config->user_data);
  phy_info *phy = pnf->phys;
  phy->id = req->pnf_phy_rf_config.phy_rf_config[0].phy_id;
  printf("[PNF] pnf config request assigned phy_id %d to phy_config_index %d\n",
         phy->id,
         req->pnf_phy_rf_config.phy_rf_config[0].phy_config_index);
  nfapi_nr_pnf_config_response_t resp;
  memset(&resp, 0, sizeof(resp));
  resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_PNF_CONFIG_RESPONSE;
  resp.error_code = NFAPI_MSG_OK;
  nfapi_nr_pnf_pnf_config_resp(config, &resp);
  printf("[PNF] Sent pnf_config_resp\n");
  return 0;
}

void nfapi_nr_send_pnf_start_resp(nfapi_nr_pnf_config_t *config, uint16_t phy_id)
{
  printf("Sending NFAPI_START_RESPONSE config:%p phy_id:%d\n", config, phy_id);
  nfapi_nr_start_response_scf_t start_resp;
  memset(&start_resp, 0, sizeof(start_resp));
  start_resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE;
  start_resp.header.phy_id = phy_id;
  start_resp.error_code = NFAPI_NR_START_MSG_OK;
  nfapi_nr_pnf_start_resp(config, &start_resp);
}

int pnf_nr_start_request(nfapi_nr_pnf_config_t *config, nfapi_nr_pnf_start_request_t *req)
{
  UNUSED(req)
  printf("Received NFAPI_PNF_START_REQUEST\n");
  nfapi_nr_pnf_start_response_t resp;
  memset(&resp, 0, sizeof(resp));
  resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_PNF_START_RESPONSE;
  resp.error_code = NFAPI_MSG_OK;
  nfapi_nr_pnf_pnf_start_resp(config, &resp);
  printf("[PNF] Sent NFAPI_PNF_START_RESP\n");

  return 0;
}

int nr_param_request(nfapi_nr_pnf_config_t *config, nfapi_pnf_phy_config_t *phy, nfapi_nr_param_request_scf_t *req)
{
  UNUSED(phy)
  printf("[PNF] Received NFAPI_PARAM_REQUEST phy_id:%d\n", req->header.phy_id);
  nfapi_nr_param_response_scf_t nfapi_resp;
  pnf_info *pnf = (pnf_info *)(config->user_data);
  memset(&nfapi_resp, 0, sizeof(nfapi_resp));
  nfapi_resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE;
  nfapi_resp.header.phy_id = req->header.phy_id;
  nfapi_resp.error_code = 0;
  struct sockaddr_in pnf_p7_sockaddr;

  // ASSIGN TAGS
  {
    nfapi_resp.cell_param.release_capability.tl.tag = NFAPI_NR_PARAM_TLV_RELEASE_CAPABILITY_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.cell_param.phy_state.tl.tag = NFAPI_NR_PARAM_TLV_PHY_STATE_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.cell_param.skip_blank_dl_config.tl.tag = NFAPI_NR_PARAM_TLV_SKIP_BLANK_DL_CONFIG_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.cell_param.skip_blank_ul_config.tl.tag = NFAPI_NR_PARAM_TLV_SKIP_BLANK_UL_CONFIG_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.cell_param.num_config_tlvs_to_report.tl.tag = NFAPI_NR_PARAM_TLV_NUM_CONFIG_TLVS_TO_REPORT_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.carrier_param.cyclic_prefix.tl.tag = NFAPI_NR_PARAM_TLV_CYCLIC_PREFIX_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.carrier_param.supported_subcarrier_spacings_dl.tl.tag = NFAPI_NR_PARAM_TLV_SUPPORTED_SUBCARRIER_SPACINGS_DL_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.carrier_param.supported_bandwidth_dl.tl.tag = NFAPI_NR_PARAM_TLV_SUPPORTED_BANDWIDTH_DL_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.carrier_param.supported_subcarrier_spacings_ul.tl.tag = NFAPI_NR_PARAM_TLV_SUPPORTED_SUBCARRIER_SPACINGS_UL_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.carrier_param.supported_bandwidth_ul.tl.tag = NFAPI_NR_PARAM_TLV_SUPPORTED_BANDWIDTH_UL_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pdcch_param.cce_mapping_type.tl.tag = NFAPI_NR_PARAM_TLV_CCE_MAPPING_TYPE_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pdcch_param.coreset_outside_first_3_of_ofdm_syms_of_slot.tl.tag =
        NFAPI_NR_PARAM_TLV_CORESET_OUTSIDE_FIRST_3_OFDM_SYMS_OF_SLOT_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pdcch_param.coreset_precoder_granularity_coreset.tl.tag = NFAPI_NR_PARAM_TLV_PRECODER_GRANULARITY_CORESET_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pdcch_param.pdcch_mu_mimo.tl.tag = NFAPI_NR_PARAM_TLV_PDCCH_MU_MIMO_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pdcch_param.pdcch_precoder_cycling.tl.tag = NFAPI_NR_PARAM_TLV_PDCCH_PRECODER_CYCLING_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pdcch_param.max_pdcch_per_slot.tl.tag = NFAPI_NR_PARAM_TLV_MAX_PDCCHS_PER_SLOT_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pucch_param.pucch_formats.tl.tag = NFAPI_NR_PARAM_TLV_PUCCH_FORMATS_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pucch_param.max_pucchs_per_slot.tl.tag = NFAPI_NR_PARAM_TLV_MAX_PUCCHS_PER_SLOT_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pdsch_param.pdsch_mapping_type.tl.tag = NFAPI_NR_PARAM_TLV_PDSCH_MAPPING_TYPE_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pdsch_param.pdsch_dmrs_additional_pos.tl.tag = NFAPI_NR_PARAM_TLV_PDSCH_DMRS_ADDITIONAL_POS_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pdsch_param.pdsch_allocation_types.tl.tag = NFAPI_NR_PARAM_TLV_PDSCH_ALLOCATION_TYPES_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pdsch_param.pdsch_vrb_to_prb_mapping.tl.tag = NFAPI_NR_PARAM_TLV_PDSCH_VRB_TO_PRB_MAPPING_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pdsch_param.pdsch_cbg.tl.tag = NFAPI_NR_PARAM_TLV_PDSCH_CBG_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pdsch_param.pdsch_dmrs_config_types.tl.tag = NFAPI_NR_PARAM_TLV_PDSCH_DMRS_CONFIG_TYPES_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pdsch_param.max_number_mimo_layers_pdsch.tl.tag = NFAPI_NR_PARAM_TLV_MAX_NUMBER_MIMO_LAYERS_PDSCH_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pdsch_param.max_mu_mimo_users_dl.tl.tag = NFAPI_NR_PARAM_TLV_MAX_MU_MIMO_USERS_DL_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pdsch_param.pdsch_data_in_dmrs_symbols.tl.tag = NFAPI_NR_PARAM_TLV_PDSCH_DATA_IN_DMRS_SYMBOLS_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pdsch_param.premption_support.tl.tag = NFAPI_NR_PARAM_TLV_PREMPTION_SUPPORT_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pdsch_param.pdsch_non_slot_support.tl.tag = NFAPI_NR_PARAM_TLV_PDSCH_NON_SLOT_SUPPORT_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pusch_param.uci_mux_ulsch_in_pusch.tl.tag = NFAPI_NR_PARAM_TLV_UCI_MUX_ULSCH_IN_PUSCH_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pusch_param.uci_only_pusch.tl.tag = NFAPI_NR_PARAM_TLV_UCI_ONLY_PUSCH_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pusch_param.pusch_frequency_hopping.tl.tag = NFAPI_NR_PARAM_TLV_PUSCH_FREQUENCY_HOPPING_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pusch_param.pusch_dmrs_config_types.tl.tag = NFAPI_NR_PARAM_TLV_PUSCH_DMRS_CONFIG_TYPES_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pusch_param.pusch_dmrs_max_len.tl.tag = NFAPI_NR_PARAM_TLV_PUSCH_DMRS_MAX_LEN_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pusch_param.pusch_dmrs_additional_pos.tl.tag = NFAPI_NR_PARAM_TLV_PUSCH_DMRS_ADDITIONAL_POS_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pusch_param.pusch_cbg.tl.tag = NFAPI_NR_PARAM_TLV_PUSCH_CBG_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pusch_param.pusch_mapping_type.tl.tag = NFAPI_NR_PARAM_TLV_PUSCH_MAPPING_TYPE_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pusch_param.pusch_allocation_types.tl.tag = NFAPI_NR_PARAM_TLV_PUSCH_ALLOCATION_TYPES_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pusch_param.pusch_vrb_to_prb_mapping.tl.tag = NFAPI_NR_PARAM_TLV_PUSCH_VRB_TO_PRB_MAPPING_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pusch_param.pusch_max_ptrs_ports.tl.tag = NFAPI_NR_PARAM_TLV_PUSCH_MAX_PTRS_PORTS_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pusch_param.max_pduschs_tbs_per_slot.tl.tag = NFAPI_NR_PARAM_TLV_MAX_PDUSCHS_TBS_PER_SLOT_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pusch_param.max_number_mimo_layers_non_cb_pusch.tl.tag = NFAPI_NR_PARAM_TLV_MAX_NUMBER_MIMO_LAYERS_NON_CB_PUSCH_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pusch_param.supported_modulation_order_ul.tl.tag = NFAPI_NR_PARAM_TLV_SUPPORTED_MODULATION_ORDER_UL_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pusch_param.max_mu_mimo_users_ul.tl.tag = NFAPI_NR_PARAM_TLV_MAX_MU_MIMO_USERS_UL_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pusch_param.dfts_ofdm_support.tl.tag = NFAPI_NR_PARAM_TLV_DFTS_OFDM_SUPPORT_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.pusch_param.pusch_aggregation_factor.tl.tag = NFAPI_NR_PARAM_TLV_PUSCH_AGGREGATION_FACTOR_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.prach_param.prach_long_formats.tl.tag = NFAPI_NR_PARAM_TLV_PRACH_LONG_FORMATS_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.prach_param.prach_short_formats.tl.tag = NFAPI_NR_PARAM_TLV_PRACH_SHORT_FORMATS_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.prach_param.prach_restricted_sets.tl.tag = NFAPI_NR_PARAM_TLV_PRACH_RESTRICTED_SETS_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.prach_param.max_prach_fd_occasions_in_a_slot.tl.tag = NFAPI_NR_PARAM_TLV_MAX_PRACH_FD_OCCASIONS_IN_A_SLOT_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.measurement_param.rssi_measurement_support.tl.tag = NFAPI_NR_PARAM_TLV_RSSI_MEASUREMENT_SUPPORT_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.nfapi_config.p7_vnf_address_ipv4.tl.tag = NFAPI_NR_NFAPI_P7_VNF_ADDRESS_IPV4_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.nfapi_config.p7_vnf_address_ipv6.tl.tag = NFAPI_NR_NFAPI_P7_VNF_ADDRESS_IPV6_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.nfapi_config.p7_vnf_port.tl.tag = NFAPI_NR_NFAPI_P7_VNF_PORT_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.nfapi_config.p7_pnf_address_ipv4.tl.tag = NFAPI_NR_NFAPI_P7_PNF_ADDRESS_IPV4_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.nfapi_config.p7_pnf_address_ipv6.tl.tag = NFAPI_NR_NFAPI_P7_PNF_ADDRESS_IPV6_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.nfapi_config.p7_pnf_port.tl.tag = NFAPI_NR_NFAPI_P7_PNF_PORT_TAG;
    nfapi_resp.num_tlv++;
    /*
      nfapi_resp.nfapi_config.dl_ue_per_sf.tl.tag =			 NFAPI_NR_NFAPI_DOWNLINK_UES_PER_SUBFRAME_TAG;
      nfapi_resp.num_tlv++;

      nfapi_resp.nfapi_config.ul_ue_per_sf.tl.tag =			 NFAPI_NR_NFAPI_UPLINK_UES_PER_SUBFRAME_TAG;
      nfapi_resp.num_tlv++;

      nfapi_resp.nfapi_config.rf_bands.tl.tag =			 NFAPI_NR_NFAPI_RF_BANDS_TAG;
      nfapi_resp.num_tlv++;
      nfapi_resp.nfapi_config.max_transmit_power.tl.tag =			 NFAPI_NR_NFAPI_MAXIMUM_TRANSMIT_POWER_TAG;
      nfapi_resp.num_tlv++;
    */
    nfapi_resp.nfapi_config.timing_window.tl.tag = NFAPI_NR_NFAPI_TIMING_WINDOW_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.nfapi_config.timing_info_mode.tl.tag = NFAPI_NR_NFAPI_TIMING_INFO_MODE_TAG;
    nfapi_resp.num_tlv++;

    nfapi_resp.nfapi_config.timing_info_period.tl.tag = NFAPI_NR_NFAPI_TIMING_INFO_PERIOD_TAG;
    nfapi_resp.num_tlv++;
  }

  nfapi_resp.nfapi_config.p7_pnf_port.value = htons(pnf->phys[0].udp.rx_port);
  nfapi_resp.num_tlv++;
  pnf_p7_sockaddr.sin_addr.s_addr = inet_addr(pnf->phys[0].local_addr);

  memcpy(nfapi_resp.nfapi_config.p7_pnf_address_ipv4.address, &pnf_p7_sockaddr.sin_addr.s_addr, 4);
  nfapi_resp.num_tlv++;
  // P7 PNF Port
  printf("TAG value :%d", nfapi_resp.cell_param.phy_state.tl.tag);
  nfapi_nr_pnf_param_resp(config, &nfapi_resp);

  printf("[PNF] Sent NFAPI_PNF_PARAM_RESPONSE phy_id:%d number_of_tlvs:%u\n", req->header.phy_id, nfapi_resp.num_tlv);
  return 0;
}

int nr_config_request(nfapi_nr_pnf_config_t *config, nfapi_pnf_phy_config_t *phy, nfapi_nr_config_request_scf_t *req)
{
  UNUSED(phy)
  printf("[PNF] Received NFAPI_CONFIG_REQ phy_id:%d\n", req->header.phy_id);
  pnf_info *pnf = (pnf_info *)(config->user_data);
  uint8_t num_tlv = 0;
  NR_DL_FRAME_PARMS *fp;

  if (NFAPI_MODE != NFAPI_UE_STUB_PNF) {
    struct PHY_VARS_gNB_s *gNB = RC.gNB[0];
    fp = &gNB->frame_parms;
  } else {
    fp = (NR_DL_FRAME_PARMS *)malloc(sizeof(NR_DL_FRAME_PARMS));
  }

  phy_info *phy_info = pnf->phys;

  printf("\nTiming window tag: %d\n", NFAPI_NR_NFAPI_TIMING_WINDOW_TAG);
  if (req->nfapi_config.timing_window.tl.tag == NFAPI_NR_NFAPI_TIMING_WINDOW_TAG) {
    phy_info->timing_window = req->nfapi_config.timing_window.value;
    printf("Phy_info:Timing window:%u NFAPI_CONFIG:timing_window:%u\n",
           phy_info->timing_window,
           req->nfapi_config.timing_window.value);
    num_tlv++;
  } else {
    // This is a nFAPI TLV, thus not sent when working with FAPI.
    // When not sent, we still need a default value, otherwise every message from the L2 is treated as late and not processed
    phy_info->timing_window = 30;
  }

  if (req->nfapi_config.timing_info_mode.tl.tag == NFAPI_NR_NFAPI_TIMING_INFO_MODE_TAG) {
    printf("timing info mode:%d\n", req->nfapi_config.timing_info_mode.value);
    phy_info->timing_info_mode = req->nfapi_config.timing_info_mode.value;
    num_tlv++;
  } else {
    phy_info->timing_info_mode = 0;
    printf("NO timing info mode provided\n");
  }
  // TODO: Read the P7 message offset values
  if (req->nfapi_config.timing_info_period.tl.tag == NFAPI_NR_NFAPI_TIMING_INFO_PERIOD_TAG) {
    printf("timing info period provided value:%d\n", req->nfapi_config.timing_info_period.value);
    phy_info->timing_info_period = req->nfapi_config.timing_info_period.value;
    num_tlv++;
  } else {
    phy_info->timing_info_period = 0;
  }

  if (req->carrier_config.dl_bandwidth.tl.tag == NFAPI_NR_CONFIG_DL_BANDWIDTH_TAG) {
    phy_info->dl_channel_bw_support = req->carrier_config.dl_bandwidth.value;
    fp->N_RB_DL = req->carrier_config.dl_bandwidth.value;
    num_tlv++;
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s() NFAPI_NR_CONFIG_DL_BANDWIDTH_TAG N_RB_DL:%u\n", __FUNCTION__, fp->N_RB_DL);
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s() Missing NFAPI_NR_CONFIG_DL_BANDWIDTH_TAG\n", __FUNCTION__);
  }

  if (req->carrier_config.uplink_bandwidth.tl.tag == NFAPI_NR_CONFIG_UPLINK_BANDWIDTH_TAG) {
    phy_info->ul_channel_bw_support = req->carrier_config.uplink_bandwidth.value;
    fp->N_RB_UL = req->carrier_config.uplink_bandwidth.value;
    num_tlv++;
  }

  if (req->cell_config.phy_cell_id.tl.tag == NFAPI_NR_CONFIG_PHY_CELL_ID_TAG) {
    fp->Nid_cell = req->cell_config.phy_cell_id.value;
    num_tlv++;
  }

  if (NFAPI_MODE != NFAPI_UE_STUB_PNF) {
    printf("[PNF] CONFIG_REQUEST[num_tlv:%d] TLVs processed:%d\n", req->num_tlv, num_tlv);
    printf("[PNF] Simulating PHY CONFIG\n");
    NR_PHY_Config_t nr_phy_config;
    nr_phy_config.phy_id = 0;
    nr_phy_config.cfg = req;
    nr_phy_config_request(&nr_phy_config);
    nr_dump_frame_parms(fp);
  }
  phy_info->remote_port = req->nfapi_config.p7_vnf_port.value;
  struct sockaddr_in vnf_p7_sockaddr;
  memcpy(&vnf_p7_sockaddr.sin_addr.s_addr, &(req->nfapi_config.p7_vnf_address_ipv4.address[0]), 4);
  phy_info->remote_addr = inet_ntoa(vnf_p7_sockaddr.sin_addr);
  printf("[PNF] %d vnf p7 %s:%d timing %d %d %d\n",
         phy_info->id,
         phy_info->remote_addr,
         phy_info->remote_port,
         phy_info->timing_window,
         phy_info->timing_info_mode,
         phy_info->timing_info_period);
  nfapi_nr_config_response_scf_t nfapi_resp;
  memset(&nfapi_resp, 0, sizeof(nfapi_resp));
  nfapi_resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_CONFIG_RESPONSE;
  nfapi_resp.header.phy_id = phy_info->id;
  nfapi_resp.error_code = 0;
  nfapi_resp.num_invalid_tlvs = 0;
  nfapi_resp.num_invalid_tlvs_configured_in_idle = 0;
  nfapi_resp.num_invalid_tlvs_configured_in_running = 0;
  nfapi_resp.num_missing_tlvs = 0;
  nfapi_nr_pnf_config_resp(config, &nfapi_resp);
  printf("[PNF] Sent NFAPI_PNF_CONFIG_RESPONSE phy_id:%d\n", phy_info->id);

  if (NFAPI_MODE == NFAPI_UE_STUB_PNF)
    free(fp);

  return 0;
}

void *pnf_nr_phy_allocate_p7_vendor_ext(uint16_t message_id, uint16_t *msg_size)
{
  if (message_id == P7_VENDOR_EXT_REQ) {
    (*msg_size) = sizeof(vendor_ext_p7_req);
    return (nfapi_nr_p7_message_header_t *)malloc(sizeof(vendor_ext_p7_req));
  }

  return 0;
}

void pnf_nr_phy_deallocate_p7_vendor_ext(void *header)
{
  free(header);
}

int pnf_nr_phy_vendor_ext(nfapi_nr_pnf_p7_config_t *config, void *msg)
{
  UNUSED(config)
  if (((nfapi_nr_p7_message_header_t *)msg)->message_id == P7_VENDOR_EXT_REQ) {
    // vendor_ext_p7_req* req = (vendor_ext_p7_req*)msg;
    // printf("[PNF] vendor request (1:%d 2:%d)\n", req->dummy1, req->dummy2);
  } else {
    printf("[PNF] unknown vendor ext\n");
  }

  return 0;
}

int pnf_nr_phy_pack_p7_vendor_extension(void *header, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *codex)
{
  UNUSED(codex)
  if (((nfapi_nr_p7_message_header_t *)header)->message_id == P7_VENDOR_EXT_IND) {
    vendor_ext_p7_ind *ind = (vendor_ext_p7_ind *)(header);

    if (!push16(ind->error_code, ppWritePackedMsg, end))
      return 0;

    return 1;
  }

  return -1;
}

int pnf_nr_phy_unpack_p7_vendor_extension(void *header, uint8_t **ppReadPackedMessage, uint8_t *end, nfapi_p7_codec_config_t *codec)
{
  UNUSED(codec)
  if (((nfapi_nr_p7_message_header_t *)header)->message_id == P7_VENDOR_EXT_REQ) {
    vendor_ext_p7_req *req = (vendor_ext_p7_req *)(header);

    if (!(pull16(ppReadPackedMessage, &req->dummy1, end) && pull16(ppReadPackedMessage, &req->dummy2, end)))
      return 0;

    return 1;
  }

  return -1;
}

int pnf_nr_phy_unpack_vendor_extension_tlv(nfapi_tl_t *tl,
                                           uint8_t **ppReadPackedMessage,
                                           uint8_t *end,
                                           void **ve,
                                           nfapi_p7_codec_config_t *config)
{
  UNUSED(config)
  switch (tl->tag) {
    case VENDOR_EXT_TLV_1_TAG:
      *ve = malloc(sizeof(vendor_ext_tlv_1));

      if (!pull32(ppReadPackedMessage, &((vendor_ext_tlv_1 *)(*ve))->dummy, end))
        return 0;

      return 1;
      break;
  }

  return -1;
}

int pnf_nr_phy_pack_vendor_extention_tlv(void *ve, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config)
{
  UNUSED(ve)
  UNUSED(ppWritePackedMsg)
  UNUSED(end)
  UNUSED(config)
  return -1;
}

int pnf_nr_sim_unpack_vendor_extension_tlv(nfapi_tl_t *tl,
                                           uint8_t **ppReadPackedMessage,
                                           uint8_t *end,
                                           void **ve,
                                           nfapi_p4_p5_codec_config_t *config)
{
  UNUSED(config)
  switch (tl->tag) {
    case VENDOR_EXT_TLV_2_TAG:
      *ve = malloc(sizeof(vendor_ext_tlv_2));

      if (!pull32(ppReadPackedMessage, &((vendor_ext_tlv_2 *)(*ve))->dummy, end))
        return 0;

      return 1;
      break;
  }

  return -1;
}

int pnf_nr_sim_pack_vendor_extention_tlv(void *ve, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p4_p5_codec_config_t *config)
{
  UNUSED(ve)
  UNUSED(ppWritePackedMsg)
  UNUSED(end)
  UNUSED(config)

  return -1;
}

void *pnf_nr_sim_allocate_p4_p5_vendor_ext(uint16_t message_id, uint16_t *msg_size)
{
  if (message_id == P5_VENDOR_EXT_REQ) {
    (*msg_size) = sizeof(vendor_ext_p5_req);
    return (nfapi_p4_p5_message_header_t *)malloc(sizeof(vendor_ext_p5_req));
  }

  return 0;
}

void pnf_nr_sim_deallocate_p4_p5_vendor_ext(void *header)
{
  free(header);
}

int pnf_nr_sim_pack_p4_p5_vendor_extension(void *header,
                                           uint8_t **ppWritePackedMsg,
                                           uint8_t *end,
                                           nfapi_p4_p5_codec_config_t *config)
{
  UNUSED(config)
  if (((nfapi_nr_p4_p5_message_header_t *)header)->message_id == P5_VENDOR_EXT_RSP) {
    vendor_ext_p5_rsp *rsp = (vendor_ext_p5_rsp *)(header);
    return (!push16(rsp->error_code, ppWritePackedMsg, end));
  }

  return 0;
}

int pnf_nr_sim_unpack_p4_p5_vendor_extension(void *header,
                                             uint8_t **ppReadPackedMessage,
                                             uint8_t *end,
                                             nfapi_p4_p5_codec_config_t *codec)
{
  UNUSED(codec)
  if (((nfapi_nr_p4_p5_message_header_t *)header)->message_id == P5_VENDOR_EXT_REQ) {
    vendor_ext_p5_req *req = (vendor_ext_p5_req *)(header);
    return (!(pull16(ppReadPackedMessage, &req->dummy1, end) && pull16(ppReadPackedMessage, &req->dummy2, end)));
  }

  return 0;
}

int nr_start_request(nfapi_nr_pnf_config_t *config, nfapi_pnf_phy_config_t *phy, nfapi_nr_start_request_scf_t *req)
{
  printf("[PNF] Received NFAPI_START_REQ phy_id:%d\n", req->header.phy_id);
  pnf_info *pnf = (pnf_info *)(config->user_data);
  phy_info *phy_info = pnf->phys;
  nfapi_nr_pnf_p7_config_t *p7_config = nfapi_nr_pnf_p7_config_create();
  p7_config->phy_id = phy->phy_id;
  p7_config->remote_p7_port = phy_info->remote_port;
  p7_config->remote_p7_addr = phy_info->remote_addr;

  p7_config->local_p7_port = phy_info->udp.rx_port;
  p7_config->local_p7_addr = phy_info->local_addr;
  printf("[PNF] P7 remote:%s:%d local:%s:%d\n",
         p7_config->remote_p7_addr,
         p7_config->remote_p7_port,
         p7_config->local_p7_addr,
         p7_config->local_p7_port);
  p7_config->user_data = phy_info;
  p7_config->malloc = &pnf_allocate;
  p7_config->free = &pnf_deallocate;
  p7_config->codec_config.allocate = &pnf_allocate;
  p7_config->codec_config.deallocate = &pnf_deallocate;
  p7_config->trace = &pnf_nfapi_trace;
  phy->user_data = p7_config;
  p7_config->slot_buffer_size = phy_info->timing_window;
  printf("slot_buffer_size configured using phy_info->timing_window:%d\n", phy_info->timing_window);

  if (phy_info->timing_info_mode & 0x1) {
    p7_config->timing_info_mode_periodic = 1;
    p7_config->timing_info_period = phy_info->timing_info_period;
  }

  if (phy_info->timing_info_mode & 0x2) {
    p7_config->timing_info_mode_aperiodic = 1;
  }

  p7_config->dl_tti_req_fn = NULL;
  p7_config->ul_tti_req_fn = NULL;
  p7_config->ul_dci_req_fn = NULL;
  p7_config->tx_data_req_fn = NULL;

  p7_config->vendor_ext = &pnf_nr_phy_vendor_ext;
  p7_config->allocate_p7_vendor_ext = &pnf_nr_phy_allocate_p7_vendor_ext;
  p7_config->deallocate_p7_vendor_ext = &pnf_nr_phy_deallocate_p7_vendor_ext;
  p7_config->codec_config.unpack_p7_vendor_extension = &pnf_nr_phy_unpack_p7_vendor_extension;
  p7_config->codec_config.pack_p7_vendor_extension = &pnf_nr_phy_pack_p7_vendor_extension;
  p7_config->codec_config.unpack_vendor_extension_tlv = &pnf_nr_phy_unpack_vendor_extension_tlv;
  p7_config->codec_config.pack_vendor_extension_tlv = &pnf_nr_phy_pack_vendor_extention_tlv;

#ifdef ENABLE_WLS
  p7_config->unpack_func = &fapi_nr_p7_message_unpack;
  p7_config->hdr_unpack_func = &fapi_nr_p7_message_header_unpack;
  p7_config->pack_func = &fapi_nr_p7_message_pack;
  p7_config->send_p7_msg = &wls_pnf_nr_send_p7_message;
  // pass p7_config to WLS handler
  wls_pnf_set_p7_config(p7_config);
#endif

#ifdef ENABLE_SOCKET
  p7_config->unpack_func = &nfapi_nr_p7_message_unpack;
  p7_config->hdr_unpack_func = &nfapi_nr_p7_message_header_unpack;
  p7_config->pack_func = &nfapi_nr_p7_message_pack;
  p7_config->send_p7_msg = &pnf_nr_send_p7_message;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[PNF] Creating P7 thread %s\n", __FUNCTION__);
  pthread_t p7_thread;
  threadCreate(&p7_thread, &pnf_nr_p7_thread_start, p7_config, "pnf_p7_thread", -1, OAI_PRIORITY_RT);
#endif

  NFAPI_TRACE(NFAPI_TRACE_INFO, "[PNF] Calling l1_north_init_gNB() %s\n", __FUNCTION__);
  l1_north_init_gNB();
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[PNF] HACK - Set p7_config global ready for subframe ind%s\n", __FUNCTION__);
  p7_config_g = p7_config;

  // Hack? in the config request (which comes before the start request here),
  // we receive the SCS/mu. The PNF needs that for calculating frame/slot
  // offsets. However, the memory for the structure which actually keeps track
  // of the time, is only allocated here, so "pick" the SCS from memory.
  PHY_VARS_gNB *gNB = RC.gNB[0];
  DevAssert(gNB != NULL);
  // check that SCS has actually been received
  const nfapi_uint8_tlv_t *scs = &gNB->gNB_config.ssb_config.scs_common;
  DevAssert(scs->tl.tag == NFAPI_NR_CONFIG_SCS_COMMON_TAG);
  pnf_p7_t *pnf_p7 = PNF_P7_FROM_NR_CONFIG(p7_config);
  pnf_p7->mu = scs->value;

  // Need to wait for main thread to create RU structures
  while (config_sync_var < 0) {
    usleep(5000000);
    printf("[PNF] waiting for OAI to be configured (gNB/RU)\n");
  }

  printf("[PNF] OAI gNB/RU configured\n");
  printf("[PNF] About to call init_eNB_afterRU()\n");

  if (NFAPI_MODE != NFAPI_UE_STUB_PNF) {
    init_eNB_afterRU();
  }

  // Signal to main thread that it can carry on - otherwise RU will startup too quickly and it is not initialised
  {
    pthread_mutex_lock(&nfapi_sync_mutex);
    nfapi_sync_var = 0;
    pthread_cond_broadcast(&nfapi_sync_cond);
    pthread_mutex_unlock(&nfapi_sync_mutex);
  }

  while (sync_var < 0) {
    usleep(50000);
    printf("[PNF] waiting for OAI to be started\n");
  }
#ifndef ENABLE_WLS
  printf("[PNF] Sending PNF_START_RESP\n");
  nfapi_nr_send_pnf_start_resp(config, p7_config->phy_id);
  printf("[PNF] Sending first P7 slot indication\n");
#endif

  return 0;
}

void stop_nr_nfapi_pnf(void)
{
#ifdef ENABLE_WLS
  wls_fapi_nr_pnf_stop();
#endif
#ifdef ENABLE_SOCKET
  socket_nfapi_nr_pnf_stop();
#endif
}

int nr_stop_request(nfapi_nr_pnf_config_t *config, nfapi_pnf_phy_config_t *phy, nfapi_nr_stop_request_scf_t *req)
{
  UNUSED(phy)
  if (has_sent_stop_ind) {
    // STOP.indication already sent, nothing to do
    return 0;
  }
  NFAPI_TRACE(NFAPI_TRACE_INFO, "Send STOP.indication\n");
  nfapi_nr_stop_indication_scf_t resp = {.header.message_id = NFAPI_NR_PHY_MSG_TYPE_STOP_INDICATION,
                                         .header.phy_id = req->header.phy_id};
  nfapi_nr_stop_indication(config, &resp);
  has_sent_stop_ind = true;
  // Upon sending the STOP.indication mark P7 PNF to terminate, to not send any further messages to the VNF
  pnf_p7_t *pnf_p7 = PNF_P7_FROM_NR_CONFIG(p7_config_g);
  pnf_p7->terminate = 1;
#ifdef ENABLE_WLS
  wls_pnf_close(pnf_p5_init_and_receive_pthread);
#endif
  // raise the SIGTERM to simulate CTRL+C
  raise(SIGTERM);
  return 0;
}

int nr_vendor_ext(nfapi_nr_pnf_config_t *config, void *msg)
{
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[PNF] P5 %s %p\n", __FUNCTION__, msg);

  switch (((nfapi_nr_p4_p5_message_header_t *)msg)->message_id) {
    case P5_VENDOR_EXT_REQ: {
      vendor_ext_p5_req *req = (vendor_ext_p5_req *)msg;
      NFAPI_TRACE(NFAPI_TRACE_INFO, "[PNF] P5 Vendor Ext Req (%d %d)\n", req->dummy1, req->dummy2);
      vendor_ext_p5_rsp rsp;
      memset(&rsp, 0, sizeof(rsp));
      rsp.header.message_id = P5_VENDOR_EXT_RSP;
      rsp.error_code = NFAPI_MSG_OK;
      nfapi_nr_pnf_vendor_extension(config, &rsp.header, sizeof(vendor_ext_p5_rsp));
    } break;
  }

  return 0;
}

void configure_nr_nfapi_pnf(char *vnf_ip_addr, int vnf_p5_port, char *pnf_ip_addr, int pnf_p7_port, int vnf_p7_port)
{
  printf("%s() PNF\n\n\n\n\n\n", __FUNCTION__);
  nfapi_setmode(NFAPI_MODE_PNF);

  nfapi_nr_pnf_config_t *config = nfapi_nr_pnf_config_create();
  config->vnf_ip_addr = vnf_ip_addr;
  config->vnf_p5_port = vnf_p5_port;
  pnf.phys[0].udp.enabled = 1;
  pnf.phys[0].udp.rx_port = pnf_p7_port;
  pnf.phys[0].udp.tx_port = vnf_p7_port;
  strcpy(pnf.phys[0].udp.tx_addr, vnf_ip_addr);
  strcpy(pnf.phys[0].local_addr, pnf_ip_addr);
  printf("%s() VNF:%s:%d PNF_PHY[addr:%s UDP:tx_addr:%s:%u rx:%u]\n",
         __FUNCTION__,
         config->vnf_ip_addr,
         config->vnf_p5_port,
         pnf.phys[0].local_addr,
         pnf.phys[0].udp.tx_addr,
         pnf.phys[0].udp.tx_port,
         pnf.phys[0].udp.rx_port);

  config->pnf_nr_param_req = &pnf_nr_param_request;
  config->pnf_nr_config_req = &pnf_nr_config_request;
  config->pnf_nr_start_req = &pnf_nr_start_request;
  config->nr_param_req = &nr_param_request;
  config->nr_config_req = &nr_config_request;
  config->nr_start_req = &nr_start_request;
  config->nr_stop_req = &nr_stop_request;
  config->vendor_ext = &nr_vendor_ext;
  config->trace = &pnf_nfapi_trace;
  config->user_data = &pnf;
  config->codec_config.unpack_vendor_extension_tlv = &pnf_nr_sim_unpack_vendor_extension_tlv;
  config->codec_config.pack_vendor_extension_tlv = &pnf_nr_sim_pack_vendor_extention_tlv;
  config->allocate_p4_p5_vendor_ext = &pnf_nr_sim_allocate_p4_p5_vendor_ext;
  config->deallocate_p4_p5_vendor_ext = &pnf_nr_sim_deallocate_p4_p5_vendor_ext;
  config->codec_config.unpack_p4_p5_vendor_extension = &pnf_nr_sim_unpack_p4_p5_vendor_extension;
  config->codec_config.pack_p4_p5_vendor_extension = &pnf_nr_sim_pack_p4_p5_vendor_extension;

#ifdef ENABLE_WLS
  printf("WLS MODE PNF\n");
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[PNF] Creating WLS PNF NFAPI start thread %s\n", __FUNCTION__);
  config->unpack_func = &fapi_nr_p5_message_unpack;
  config->hdr_unpack_func = &fapi_nr_message_header_unpack;
  config->pack_func = &fapi_nr_p5_message_pack;
  config->send_p5_msg = &wls_pnf_nr_send_p5_message;
  threadCreate(&pnf_p5_init_and_receive_pthread, wls_fapi_pnf_nr_start_thread, config, "NFAPI_WLS_PNF", -1, OAI_PRIORITY_RT_MAX);
#endif

#ifdef ENABLE_SOCKET
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[PNF] Creating PNF NFAPI start thread %s\n", __FUNCTION__);
  config->unpack_func = &nfapi_nr_p5_message_unpack;
  config->hdr_unpack_func = &nfapi_nr_p5_message_header_unpack;
  config->pack_func = &nfapi_nr_p5_message_pack;
  config->send_p5_msg = &pnf_nr_send_p5_message;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[PNF] Creating PNF NFAPI start thread %s\n", __FUNCTION__);
  pthread_create(&pnf_p5_init_and_receive_pthread, NULL, &pnf_start_p5_thread, config);
  pthread_setname_np(pnf_p5_init_and_receive_pthread, "NFAPI_PNF");
#endif
}

#define SLOT_DURATION 800 // in microseconds
static void maybe_slow_down_pnf(int mu)
{
  static struct timespec last_execution = {0};
  struct timespec current_execution;
  clock_gettime(CLOCK_REALTIME, &current_execution);
  long elapsed_time = (current_execution.tv_sec - last_execution.tv_sec) * 1000000;
  elapsed_time += (current_execution.tv_nsec - last_execution.tv_nsec) / 1000;
  int duration = SLOT_DURATION >> mu;
  if (elapsed_time < duration)
    usleep(duration - elapsed_time);
  last_execution = current_execution;
}

void handle_nr_slot_ind(uint16_t sfn, uint16_t slot, NR_Sched_Rsp_t *sched_resp)
{
  nfapi_nr_pnf_p7_config_t *config = p7_config_g;
  pnf_p7_t *_this = PNF_P7_FROM_NR_CONFIG(config);
  int mu = _this->mu;

  if (IS_SOFTMODEM_RFSIM) {
    // RFsim can run faster than realtime. However, we need to give the VNF
    // some time to send an answer, so the PNF can run faster than realtime,
    // but it should not too much. This function will "maybe" slow down, up to
    // a slot length
    maybe_slow_down_pnf(mu);
  }

  //send VNF slot indication, which is aligned with TX thread, so that it can call the scheduler
  //we give four additional slots (2ms) which should be enough time for the VNF to
  //answer
#ifndef ENABLE_WLS
  int slot_ahead = 2 << mu;
#else
  int slot_ahead = 1 << mu;
#endif
  uint16_t sfn_tx = sfn;
  uint16_t slot_tx = slot;
  sfnslot_add_slot(mu, &sfn_tx, &slot_tx, slot_ahead);

  nfapi_nr_slot_indication_scf_t ind = {.sfn = sfn_tx, .slot = slot_tx};
  oai_nfapi_nr_slot_indication(&ind);

  // copy data from appropriate p7 slot buffers into channel structures for PHY processing
  nfapi_nr_pnf_p7_get_msgs(config,
                           config->phy_id,
                           sfn,
                           slot,
                           &sched_resp->DL_req,
                           &sched_resp->UL_tti_req,
                           &sched_resp->UL_dci_req,
                           &sched_resp->TX_req);
}

int oai_nfapi_nr_slot_indication(nfapi_nr_slot_indication_scf_t *ind)
{
  ind->header.phy_id = 0;
  ind->header.message_id = NFAPI_NR_PHY_MSG_TYPE_SLOT_INDICATION;
  return nfapi_pnf_p7_nr_slot_ind(p7_config_g, ind);
}

int oai_nfapi_nr_rx_data_indication(nfapi_nr_rx_data_indication_t *ind)
{
  ind->header.phy_id = 0; // HACK TODO FIXME - need to pass this around!!!!
  ind->header.message_id = NFAPI_NR_PHY_MSG_TYPE_RX_DATA_INDICATION;
  return nfapi_pnf_p7_nr_rx_data_ind(p7_config_g, ind);
}

int oai_nfapi_nr_crc_indication(nfapi_nr_crc_indication_t *ind)
{
  ind->header.phy_id = 0; // HACK TODO FIXME - need to pass this around!!!!
  ind->header.message_id = NFAPI_NR_PHY_MSG_TYPE_CRC_INDICATION;
  return nfapi_pnf_p7_nr_crc_ind(p7_config_g, ind);
}

int oai_nfapi_nr_srs_indication(nfapi_nr_srs_indication_t *ind)
{
  ind->header.phy_id = 0; // HACK TODO FIXME - need to pass this around!!!!
  ind->header.message_id = NFAPI_NR_PHY_MSG_TYPE_SRS_INDICATION;
  return nfapi_pnf_p7_nr_srs_ind(p7_config_g, ind);
}

int oai_nfapi_nr_srs_toa_vendor_ext_indication(nfapi_nr_srs_toa_vendor_ext_indication_t *ind)
{
  ind->header.phy_id = 0; // HACK TODO FIXME - need to pass this around!!!!
  ind->header.message_id = NFAPI_NR_PHY_MSG_TYPE_SRS_TOA_VENDOR_EXTENSION_INDICATION;
  return nfapi_pnf_p7_nr_srs_toa_vendor_ext_ind(p7_config_g, ind);
}

int oai_nfapi_nr_uci_indication(nfapi_nr_uci_indication_t *ind)
{
  ind->header.phy_id = 0; // HACK TODO FIXME - need to pass this around!!!!
  ind->header.message_id = NFAPI_NR_PHY_MSG_TYPE_UCI_INDICATION;
  return nfapi_pnf_p7_nr_uci_ind(p7_config_g, ind);
}

int oai_nfapi_nr_rach_indication(nfapi_nr_rach_indication_t *ind)
{
  ind->header.phy_id = 0; // HACK TODO FIXME - need to pass this around!!!!
  ind->header.message_id = NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION;
  return nfapi_pnf_p7_nr_rach_ind(p7_config_g, ind);
}
