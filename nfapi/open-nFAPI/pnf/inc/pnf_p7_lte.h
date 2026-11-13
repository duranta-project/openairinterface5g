/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#ifndef _PNF_P7_LTE_H_
#define _PNF_P7_LTE_H_

#include <stddef.h>
#include "nfapi_lte_pnf_interface.h"
#include "pnf_p7_common.h"

typedef struct {
  uint16_t dl_conf_ontime;
  uint16_t dl_conf_late;
  uint16_t ul_conf_ontime;
  uint16_t ul_conf_late;
  uint16_t hi_dci0_ontime;
  uint16_t hi_dci0_late;
  uint16_t tx_ontime;
  uint16_t tx_late;
} pnf_p7_stats_t;

typedef struct pnf_p7_s {
  nfapi_pnf_p7_config_t _public; /* LTE P7 config — must be first */

  //private data
  int p7_sock;
  uint8_t terminate;
  uint8_t tx_message_buffer[NFAPI_MAX_PACKED_MESSAGE_SIZE];
  uint8_t* rx_message_buffer;
  uint16_t rx_message_buffer_size;

  pthread_mutex_t mutex; // should we allow the client to specifiy
  pthread_mutex_t pack_mutex; // should we allow the client to specifiy

  uint32_t sequence_number;
  uint16_t max_num_segments;

  pnf_p7_rx_reassembly_queue_t reassembly_queue;

  uint8_t* reassemby_buffer;
  uint32_t reassemby_buffer_size;

  uint8_t timing_info_period_counter;
  uint8_t timing_info_aperiodic_send; // 0:false 1:true

  uint32_t timing_info_ms_counter; // number of ms since last timing info

  uint32_t tick;

  /* LTE-specific */
  nfapi_pnf_p7_subframe_buffer_t subframe_buffer[30/*NFAPI_MAX_TIMING_WINDOW_SIZE*/];
  uint16_t sfn_sf;
  uint32_t sf_start_time_hr;
  int32_t sfn_sf_shift;

  uint32_t dl_config_jitter;
  uint32_t ul_config_jitter;
  uint32_t hi_dci0_jitter;
  uint32_t tx_jitter;

  pnf_p7_stats_t stats;
} pnf_p7_t;

int pnf_p7_message_pump(pnf_p7_t* pnf_p7);
int pnf_p7_pack_and_send_p7_message(pnf_p7_t* pnf_p7, nfapi_p7_message_header_t* msg, uint32_t msg_len);
int pnf_p7_send_message(pnf_p7_t* pnf_p7, uint8_t* msg, uint32_t msg_len);
int pnf_p7_subframe_ind(pnf_p7_t* config, uint16_t phy_id, uint16_t sfn_sf);
pnf_p7_rx_message_t* pnf_p7_rx_reassembly_queue_add_segment(pnf_p7_t* pnf_p7, pnf_p7_rx_reassembly_queue_t* queue, uint32_t rx_hr_time, uint16_t sequence_number, uint16_t segment_number, uint8_t m, uint8_t* data, uint16_t data_len);
void pnf_p7_rx_reassembly_queue_remove_msg(pnf_p7_t* pnf_p7, pnf_p7_rx_reassembly_queue_t* queue, pnf_p7_rx_message_t* msg);
void pnf_p7_rx_reassembly_queue_remove_old_msgs(pnf_p7_t* pnf_p7, pnf_p7_rx_reassembly_queue_t* queue, uint32_t rx_hr_time, uint32_t delta);
void pnf_handle_p7_vendor_extension(void* pRecvMsg, int recvMsgLen, pnf_p7_t* pnf_p7, uint16_t message_id);
void pnf_p7_free(pnf_p7_t* pnf_p7, void* ptr);
void* pnf_p7_malloc(pnf_p7_t* pnf_p7, size_t size);
struct timespec pnf_timespec_sub(struct timespec lhs, struct timespec rhs);
struct timespec pnf_timespec_add(struct timespec lhs, struct timespec rhs);
uint32_t pnf_get_current_time_hr(void);

#endif /* _PNF_P7_LTE_H_ */
