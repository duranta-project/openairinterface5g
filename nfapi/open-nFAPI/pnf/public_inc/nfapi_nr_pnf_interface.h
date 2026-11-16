/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#ifndef _NFAPI_NR_PNF_INTERFACE_H_
#define _NFAPI_NR_PNF_INTERFACE_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include "nfapi_pnf_interface_common.h"
#include "nfapi_nr_interface.h"
#include "nfapi_nr_interface_scf.h"
#include <openair2/PHY_INTERFACE/IF_Module.h>
#include "openair1/PHY/defs_gNB.h"

typedef struct nfapi_nr_pnf_config nfapi_nr_pnf_config_t;

/*
 * NR PNF P5 configuration.
 *
 * The first 9 fields (through user_data) form the common initial sequence
 * shared with nfapi_pnf_config_t, allowing safe casts between the two
 * for access to those shared fields (nfapi_pnf_phy_config_find et al.).
 */
typedef struct nfapi_nr_pnf_config {
  /* --- shared prefix with nfapi_pnf_config_t (must remain identical) --- */
  void* (*malloc)(size_t size);
  void (*free)(void* ptr);
  void (*trace)(nfapi_trace_level_t level, const char* message, ...);
  char* vnf_ip_addr;
  int vnf_p5_port;
  nfapi_pnf_state_e state;
  nfapi_pnf_phy_config_t* phys;
  nfapi_p4_p5_codec_config_t codec_config;
  void* user_data;
  /* --- NR-only P5 callbacks --- */
  int (*pnf_nr_param_req)(nfapi_nr_pnf_config_t* config, nfapi_nr_pnf_param_request_t* req);
  int (*pnf_nr_config_req)(nfapi_nr_pnf_config_t* config, nfapi_nr_pnf_config_request_t* req);
  int (*pnf_nr_start_req)(nfapi_nr_pnf_config_t* config, nfapi_nr_pnf_start_request_t* req);
  int (*nr_param_req)(nfapi_nr_pnf_config_t* config, nfapi_pnf_phy_config_t* phy, nfapi_nr_param_request_scf_t* req);
  int (*nr_config_req)(nfapi_nr_pnf_config_t* config, nfapi_pnf_phy_config_t* phy, nfapi_nr_config_request_scf_t* req);
  int (*nr_start_req)(nfapi_nr_pnf_config_t* config, nfapi_pnf_phy_config_t* phy, nfapi_nr_start_request_scf_t* req);
  int (*nr_stop_req)(nfapi_nr_pnf_config_t* config, nfapi_pnf_phy_config_t* phy, nfapi_nr_stop_request_scf_t* req);
  /* --- shared end fields --- */
  int (*vendor_ext)(nfapi_nr_pnf_config_t* config, void* msg);
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
  bool (*send_p5_msg)(pnf_t* pnf, nfapi_nr_p4_p5_message_header_t* msg, uint32_t msg_len);
} nfapi_nr_pnf_config_t;

nfapi_nr_pnf_config_t* nfapi_nr_pnf_config_create(void);
void nfapi_nr_pnf_config_destroy(nfapi_nr_pnf_config_t* config);
int nfapi_nr_pnf_start(nfapi_nr_pnf_config_t* config);
int nfapi_nr_pnf_stop(nfapi_nr_pnf_config_t* config);
int nfapi_nr_pnf_vendor_extension(nfapi_nr_pnf_config_t* config, nfapi_p4_p5_message_header_t* msg, uint32_t msg_len);

int nfapi_nr_pnf_pnf_param_resp(nfapi_nr_pnf_config_t* config, nfapi_nr_pnf_param_response_t* resp);
int nfapi_nr_pnf_pnf_config_resp(nfapi_nr_pnf_config_t* config, nfapi_nr_pnf_config_response_t* resp);
int nfapi_nr_pnf_pnf_start_resp(nfapi_nr_pnf_config_t* config, nfapi_nr_pnf_start_response_t* resp);
int nfapi_nr_pnf_param_resp(nfapi_nr_pnf_config_t* config, nfapi_nr_param_response_scf_t* resp);
int nfapi_nr_pnf_config_resp(nfapi_nr_pnf_config_t* config, nfapi_nr_config_response_scf_t* resp);
int nfapi_nr_pnf_start_resp(nfapi_nr_pnf_config_t* config, nfapi_nr_start_response_scf_t* resp);
int nfapi_nr_stop_indication(nfapi_nr_pnf_config_t* config, nfapi_nr_stop_indication_scf_t* resp);

/* NR P7 slot buffer */
typedef struct {
  uint16_t sfn;
  uint16_t slot;
  nfapi_nr_dl_tti_request_t dl_tti_req;
  nfapi_nr_ul_tti_request_t ul_tti_req;
  nfapi_nr_ul_dci_request_t ul_dci_req;
  nfapi_nr_tx_data_request_t tx_data_req;
} nfapi_pnf_p7_slot_buffer_t;

typedef struct nfapi_nr_pnf_p7_config nfapi_nr_pnf_p7_config_t;

/*
 * NR PNF P7 configuration.
 *
 * The first 10 fields (through segment_size) form the common initial sequence
 * shared with nfapi_pnf_p7_config_t.
 */
typedef struct nfapi_nr_pnf_p7_config {
  /* --- shared P7 prefix with nfapi_pnf_p7_config_t (must remain identical) --- */
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
  /* --- NR-specific --- */
  nfapi_pnf_p7_slot_buffer_t dummy_slot;
  nfapi_p7_codec_config_t codec_config;
  void* user_data;
  uint16_t slot_buffer_size;
  uint8_t timing_info_mode_periodic;
  uint8_t timing_info_mode_aperiodic;
  uint8_t timing_info_period;
  /* --- NR P7 callbacks --- */
  int (*dl_tti_req_fn)(gNB_L1_rxtx_proc_t* proc, nfapi_nr_pnf_p7_config_t* config, nfapi_nr_dl_tti_request_t* req);
  int (*ul_tti_req_fn)(gNB_L1_rxtx_proc_t* proc, nfapi_nr_pnf_p7_config_t* config, nfapi_nr_ul_tti_request_t* req);
  int (*ul_dci_req_fn)(gNB_L1_rxtx_proc_t* proc, nfapi_nr_pnf_p7_config_t* config, nfapi_nr_ul_dci_request_t* req);
  int (*tx_data_req_fn)(nfapi_nr_pnf_p7_config_t* config, nfapi_nr_tx_data_request_t* req);
  /* --- shared P7 end fields --- */
  int (*vendor_ext)(nfapi_nr_pnf_p7_config_t* config, void* msg);
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
  bool (*send_p7_msg)(pnf_p7_t* pnf_p7, nfapi_nr_p7_message_header_t* header, uint32_t msg_len);
} nfapi_nr_pnf_p7_config_t;

nfapi_nr_pnf_p7_config_t* nfapi_nr_pnf_p7_config_create(void);
void nfapi_nr_pnf_p7_config_destroy(nfapi_nr_pnf_p7_config_t* config);
int nfapi_nr_pnf_p7_start(nfapi_nr_pnf_p7_config_t* config);
int nfapi_nr_pnf_p7_stop(nfapi_nr_pnf_p7_config_t* config);

int nfapi_nr_pnf_p7_get_msgs(nfapi_nr_pnf_p7_config_t* config,
                             uint16_t phy_id,
                             uint16_t sfn,
                             uint16_t slot,
                             nfapi_nr_dl_tti_request_t* ret_dl_tti,
                             nfapi_nr_ul_tti_request_t* ret_ul_tti,
                             nfapi_nr_ul_dci_request_t* ret_ul_dci,
                             nfapi_nr_tx_data_request_t* ret_tx_data);

int nfapi_pnf_p7_nr_slot_ind(nfapi_nr_pnf_p7_config_t* config, nfapi_nr_slot_indication_scf_t* ind);
int nfapi_pnf_p7_nr_rx_data_ind(nfapi_nr_pnf_p7_config_t* config, nfapi_nr_rx_data_indication_t* ind);
int nfapi_pnf_p7_nr_crc_ind(nfapi_nr_pnf_p7_config_t* config, nfapi_nr_crc_indication_t* ind);
int nfapi_pnf_p7_nr_srs_ind(nfapi_nr_pnf_p7_config_t* config, nfapi_nr_srs_indication_t* ind);
int nfapi_pnf_p7_nr_srs_toa_vendor_ext_ind(nfapi_nr_pnf_p7_config_t* config, nfapi_nr_srs_toa_vendor_ext_indication_t* ind);
int nfapi_pnf_p7_nr_uci_ind(nfapi_nr_pnf_p7_config_t* config, nfapi_nr_uci_indication_t* ind);
int nfapi_pnf_p7_nr_rach_ind(nfapi_nr_pnf_p7_config_t* config, nfapi_nr_rach_indication_t* ind);

#if defined(__cplusplus)
}
#endif
#endif /* _NFAPI_NR_PNF_INTERFACE_H_ */
