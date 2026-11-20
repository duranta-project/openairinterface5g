/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#ifndef _NFAPI_LTE_PNF_INTERFACE_H_
#define _NFAPI_LTE_PNF_INTERFACE_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include "nfapi_pnf_interface_common.h"

typedef struct nfapi_pnf_config nfapi_pnf_config_t;

/*
 * LTE PNF P5 configuration.
 *
 * The first 9 fields (through user_data) form the common initial sequence
 * shared with nfapi_nr_pnf_config_t, allowing safe casts between the two
 * for access to those shared fields (nfapi_pnf_phy_config_find et al.).
 */
typedef struct nfapi_pnf_config {
  /* --- shared prefix with nfapi_nr_pnf_config_t --- */
  void* (*malloc)(size_t size);
  void (*free)(void* ptr);
  void (*trace)(nfapi_trace_level_t level, const char* message, ...);
  char* vnf_ip_addr;
  int vnf_p5_port;
  nfapi_pnf_state_e state;
  nfapi_pnf_phy_config_t* phys;
  nfapi_p4_p5_codec_config_t codec_config;
  void* user_data;
  /* --- LTE-only P5 callbacks --- */
  int (*pnf_param_req)(nfapi_pnf_config_t* config, nfapi_pnf_param_request_t* req);
  int (*pnf_config_req)(nfapi_pnf_config_t* config, nfapi_pnf_config_request_t* req);
  int (*pnf_start_req)(nfapi_pnf_config_t* config, nfapi_pnf_start_request_t* req);
  int (*pnf_stop_req)(nfapi_pnf_config_t* config, nfapi_pnf_stop_request_t* req);
  int (*param_req)(nfapi_pnf_config_t* config, nfapi_pnf_phy_config_t* phy, nfapi_param_request_t* req);
  int (*config_req)(nfapi_pnf_config_t* config, nfapi_pnf_phy_config_t* phy, nfapi_config_request_t* req);
  int (*start_req)(nfapi_pnf_config_t* config, nfapi_pnf_phy_config_t* phy, nfapi_start_request_t* req);
  int (*stop_req)(nfapi_pnf_config_t* config, nfapi_pnf_phy_config_t* phy, nfapi_stop_request_t* req);
  int (*measurement_req)(nfapi_pnf_config_t* config, nfapi_pnf_phy_config_t* phy, nfapi_measurement_request_t* req);
  int (*rssi_req)(nfapi_pnf_config_t* config, nfapi_pnf_phy_config_t* phy, nfapi_rssi_request_t* req);
  int (*cell_search_req)(nfapi_pnf_config_t* config, nfapi_pnf_phy_config_t* phy, nfapi_cell_search_request_t* req);
  int (*broadcast_detect_req)(nfapi_pnf_config_t* config, nfapi_pnf_phy_config_t* phy, nfapi_broadcast_detect_request_t* req);
  int (*system_information_schedule_req)(nfapi_pnf_config_t* config,
                                         nfapi_pnf_phy_config_t* phy,
                                         nfapi_system_information_schedule_request_t* req);
  int (*system_information_req)(nfapi_pnf_config_t* config, nfapi_pnf_phy_config_t* phy, nfapi_system_information_request_t* req);
  int (*nmm_stop_req)(nfapi_pnf_config_t* config, nfapi_pnf_phy_config_t* phy, nfapi_nmm_stop_request_t* req);
  /* --- shared end fields --- */
  int (*vendor_ext)(nfapi_pnf_config_t* config, void* msg);
  void* (*allocate_p4_p5_vendor_ext)(uint16_t message_id, uint16_t* msg_size);
  void (*deallocate_p4_p5_vendor_ext)(void* header);
  int (*pack_func)(void* pMessageBuf,
                   uint32_t messageBufLen,
                   void* pPackedBuf,
                   uint32_t packedBufLen,
                   nfapi_p4_p5_codec_config_t* config);
  bool (*unpack_func)(void* pMessageBuf,
                      uint32_t messageBufLen,
                      void* pUnpackedBuf,
                      uint32_t unpackedBufLen,
                      nfapi_p4_p5_codec_config_t* config);
  bool (*hdr_unpack_func)(void* pMessageBuf,
                          uint32_t messageBufLen,
                          void* pUnpackedBuf,
                          uint32_t unpackedBufLen,
                          nfapi_p4_p5_codec_config_t* config);
  bool (*send_p5_msg)(pnf_t* pnf, nfapi_p4_p5_message_header_t* msg, uint32_t msg_len);
} nfapi_pnf_config_t;

nfapi_pnf_config_t* nfapi_pnf_config_create(void);
int nfapi_pnf_config_destroy(nfapi_pnf_config_t* config);
int nfapi_pnf_start(nfapi_pnf_config_t* config);
int nfapi_pnf_stop(nfapi_pnf_config_t* config);
int nfapi_pnf_vendor_extension(nfapi_pnf_config_t* config, nfapi_p4_p5_message_header_t* msg, uint32_t msg_len);

int nfapi_pnf_pnf_param_resp(nfapi_pnf_config_t* config, nfapi_pnf_param_response_t* resp);
int nfapi_pnf_pnf_config_resp(nfapi_pnf_config_t* config, nfapi_pnf_config_response_t* resp);
int nfapi_pnf_pnf_start_resp(nfapi_pnf_config_t* config, nfapi_pnf_start_response_t* resp);
int nfapi_pnf_pnf_stop_resp(nfapi_pnf_config_t* config, nfapi_pnf_stop_response_t* resp);
int nfapi_pnf_param_resp(nfapi_pnf_config_t* config, nfapi_param_response_t* resp);
int nfapi_pnf_config_resp(nfapi_pnf_config_t* config, nfapi_config_response_t* resp);
int nfapi_pnf_start_resp(nfapi_pnf_config_t* config, nfapi_start_response_t* resp);
int nfapi_pnf_stop_resp(nfapi_pnf_config_t* config, nfapi_stop_response_t* resp);
int nfapi_pnf_measurement_resp(nfapi_pnf_config_t* config, nfapi_measurement_response_t* resp);
int nfapi_pnf_rssi_resp(nfapi_pnf_config_t* config, nfapi_rssi_response_t* resp);
int nfapi_pnf_rssi_ind(nfapi_pnf_config_t* config, nfapi_rssi_indication_t* ind);
int nfapi_pnf_cell_search_resp(nfapi_pnf_config_t* config, nfapi_cell_search_response_t* resp);
int nfapi_pnf_cell_search_ind(nfapi_pnf_config_t* config, nfapi_cell_search_indication_t* ind);
int nfapi_pnf_broadcast_detect_resp(nfapi_pnf_config_t* config, nfapi_broadcast_detect_response_t* resp);
int nfapi_pnf_broadcast_detect_ind(nfapi_pnf_config_t* config, nfapi_broadcast_detect_indication_t* ind);
int nfapi_pnf_system_information_schedule_resp(nfapi_pnf_config_t* config, nfapi_system_information_schedule_response_t* resp);
int nfapi_pnf_system_information_schedule_ind(nfapi_pnf_config_t* config, nfapi_system_information_schedule_indication_t* ind);
int nfapi_pnf_system_information_resp(nfapi_pnf_config_t* config, nfapi_system_information_response_t* resp);
int nfapi_pnf_system_information_ind(nfapi_pnf_config_t* config, nfapi_system_information_indication_t* ind);
int nfapi_pnf_nmm_stop_resp(nfapi_pnf_config_t* config, nfapi_nmm_stop_response_t* resp);

/* LTE P7 subframe buffer */
typedef struct {
  uint16_t sfn_sf;
  nfapi_dl_config_request_t* dl_config_req;
  nfapi_ul_config_request_t* ul_config_req;
  nfapi_hi_dci0_request_t* hi_dci0_req;
  nfapi_tx_request_t* tx_req;
  nfapi_lbt_dl_config_request_t* lbt_dl_config_req;
  nfapi_ue_release_request_t* ue_release_req;
} nfapi_pnf_p7_subframe_buffer_t;

typedef struct nfapi_pnf_p7_config nfapi_pnf_p7_config_t;

/*
 * LTE PNF P7 configuration.
 *
 * The first 10 fields (through segment_size) form the common initial sequence
 * shared with nfapi_nr_pnf_p7_config_t.
 */
typedef struct nfapi_pnf_p7_config {
  /* --- shared P7 prefix with nfapi_nr_pnf_p7_config_t --- */
  void* (*malloc)(size_t size);
  void (*free)(void* ptr);
  void (*trace)(nfapi_trace_level_t level, const char* message, ...);
  uint16_t phy_id;
  int remote_p7_port;
  char* remote_p7_addr;
  int local_p7_port;
  char* local_p7_addr;
  uint8_t checksum_enabled;
  uint32_t segment_size;
  /* --- LTE-specific --- */
  nfapi_pnf_p7_subframe_buffer_t dummy_subframe;
  nfapi_p7_codec_config_t codec_config;
  void* user_data;
  uint16_t subframe_buffer_size;
  uint8_t timing_info_mode_periodic;
  uint8_t timing_info_mode_aperiodic;
  uint8_t timing_info_period;
  /* --- LTE P7 callbacks --- */
  int (*dl_config_req)(nfapi_pnf_p7_config_t* config, nfapi_dl_config_request_t* req);
  int (*ul_config_req)(nfapi_pnf_p7_config_t* config, nfapi_ul_config_request_t* req);
  int (*hi_dci0_req)(nfapi_pnf_p7_config_t* config, nfapi_hi_dci0_request_t* req);
  int (*tx_req)(nfapi_pnf_p7_config_t* config, nfapi_tx_request_t* req);
  int (*lbt_dl_config_req)(nfapi_pnf_p7_config_t* config, nfapi_lbt_dl_config_request_t* req);
  int (*ue_release_req)(nfapi_pnf_p7_config_t* config, nfapi_ue_release_request_t* req);
  /* --- shared P7 end fields --- */
  int (*vendor_ext)(nfapi_pnf_p7_config_t* config, void* msg);
  void* (*allocate_p7_vendor_ext)(uint16_t message_id, uint16_t* msg_size);
  void (*deallocate_p7_vendor_ext)(void* header);
  int (*pack_func)(void* pMessageBuf, void* pPackedBuf, uint32_t packedBufLen, nfapi_p7_codec_config_t* config);
  bool (*unpack_func)(void* pMessageBuf,
                      uint32_t messageBufLen,
                      void* pUnpackedBuf,
                      uint32_t unpackedBufLen,
                      nfapi_p7_codec_config_t* config);
  bool (*hdr_unpack_func)(void* pMessageBuf,
                          uint32_t messageBufLen,
                          void* pUnpackedBuf,
                          uint32_t unpackedBufLen,
                          nfapi_p7_codec_config_t* config);
  bool (*send_p7_msg)(pnf_p7_t* pnf_p7, nfapi_p7_message_header_t* header, uint32_t msg_len);
} nfapi_pnf_p7_config_t;

nfapi_pnf_p7_config_t* nfapi_pnf_p7_config_create(void);
int nfapi_pnf_p7_config_destroy(nfapi_pnf_p7_config_t* config);
int nfapi_pnf_p7_start(nfapi_pnf_p7_config_t* config);
int nfapi_pnf_p7_stop(nfapi_pnf_p7_config_t* config);

int nfapi_pnf_p7_subframe_ind(nfapi_pnf_p7_config_t* config, uint16_t phy_id, uint16_t sfn_sf);
int nfapi_pnf_p7_harq_ind(nfapi_pnf_p7_config_t* config, nfapi_harq_indication_t* ind);
int nfapi_pnf_p7_crc_ind(nfapi_pnf_p7_config_t* config, nfapi_crc_indication_t* ind);
int nfapi_pnf_p7_rx_ind(nfapi_pnf_p7_config_t* config, nfapi_rx_indication_t* ind);
int nfapi_pnf_p7_rach_ind(nfapi_pnf_p7_config_t* config, nfapi_rach_indication_t* ind);
int nfapi_pnf_p7_srs_ind(nfapi_pnf_p7_config_t* config, nfapi_srs_indication_t* ind);
int nfapi_pnf_p7_sr_ind(nfapi_pnf_p7_config_t* config, nfapi_sr_indication_t* ind);
int nfapi_pnf_p7_cqi_ind(nfapi_pnf_p7_config_t* config, nfapi_cqi_indication_t* ind);
int nfapi_pnf_p7_lbt_dl_ind(nfapi_pnf_p7_config_t* config, nfapi_lbt_dl_indication_t* ind);
int nfapi_pnf_p7_nb_harq_ind(nfapi_pnf_p7_config_t* config, nfapi_nb_harq_indication_t* ind);
int nfapi_pnf_p7_nrach_ind(nfapi_pnf_p7_config_t* config, nfapi_nrach_indication_t* ind);
int nfapi_pnf_p7_vendor_extension(nfapi_pnf_p7_config_t* config, nfapi_p7_message_header_t* msg);
int nfapi_pnf_ue_release_resp(nfapi_pnf_p7_config_t* config, nfapi_ue_release_response_t* resp);

#if defined(__cplusplus)
}
#endif
#endif /* _NFAPI_LTE_PNF_INTERFACE_H_ */
