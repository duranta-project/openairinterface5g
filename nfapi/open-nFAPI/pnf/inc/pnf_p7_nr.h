/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#ifndef _PNF_P7_NR_H_
#define _PNF_P7_NR_H_

#include "pnf_p7_common.h"
#include "nfapi_nr_pnf_interface.h"

typedef struct pnf_p7_nr_s {
  nfapi_nr_pnf_p7_config_t _public; /* NR P7 config — must be first */

  int p7_sock;
  uint8_t terminate;
  uint8_t tx_message_buffer[NFAPI_MAX_PACKED_MESSAGE_SIZE];
  uint8_t* rx_message_buffer;
  uint16_t rx_message_buffer_size;

  pthread_mutex_t mutex;
  pthread_mutex_t pack_mutex;

  uint32_t sequence_number;
  uint16_t max_num_segments;

  pnf_p7_rx_reassembly_queue_t reassembly_queue;

  uint8_t* reassemby_buffer;
  uint32_t reassemby_buffer_size;

  uint8_t timing_info_period_counter;
  uint8_t timing_info_aperiodic_send;

  uint32_t timing_info_ms_counter;

  uint32_t tick;

  /* NR-specific */
  nfapi_pnf_p7_slot_buffer_t slot_buffer[30/*NFAPI_MAX_TIMING_WINDOW_SIZE*/];
  uint16_t sfn;
  uint16_t slot;
  int mu;
  uint16_t sfn_slot;
  uint32_t slot_start_time_hr;
  int32_t slot_shift;

  uint32_t dl_tti_jitter;
  uint32_t ul_tti_jitter;
  uint32_t ul_dci_jitter;
  uint32_t tx_data_jitter;

  pnf_p7_nr_stats_t nr_stats;
} pnf_p7_nr_t;

int pnf_nr_p7_message_pump(pnf_p7_nr_t* pnf_p7);
void pnf_nr_handle_p7_message(void* pRecvMsg, int recvMsgLen, pnf_p7_nr_t* pnf_p7, uint32_t rx_hr_time);
void pnf_nr_handle_p7_vendor_extension(void* pRecvMsg, int recvMsgLen, pnf_p7_nr_t* pnf_p7, uint16_t message_id);
int nr_pnf_p7_get_msgs(pnf_p7_nr_t* pnf_p7,
                       uint16_t phy_id,
                       uint16_t sfn,
                       uint16_t slot,
                       nfapi_nr_dl_tti_request_t* ret_dl_tti,
                       nfapi_nr_ul_tti_request_t* ret_ul_tti,
                       nfapi_nr_ul_dci_request_t* ret_ul_dci,
                       nfapi_nr_tx_data_request_t* ret_tx_data);
void pnf_handle_dl_tti_request(void* pRecvMsg, int recvMsgLen, pnf_p7_nr_t* pnf_p7);
void pnf_handle_ul_tti_request(void* pRecvMsg, int recvMsgLen, pnf_p7_nr_t* pnf_p7);
void pnf_handle_ul_dci_request(void* pRecvMsg, int recvMsgLen, pnf_p7_nr_t* pnf_p7);
void pnf_handle_tx_data_request(void* pRecvMsg, int recvMsgLen, pnf_p7_nr_t* pnf_p7);
void pnf_nr_handle_dl_node_sync(void* pRecvMsg, int recvMsgLen, pnf_p7_nr_t* pnf_p7, uint32_t rx_hr_time);
bool is_nr_p7_request_in_window(uint16_t sfn, uint16_t slot, const char* name, const pnf_p7_nr_t* phy);
void* pnf_p7_nr_malloc(pnf_p7_nr_t* pnf_p7, size_t size);
void pnf_p7_nr_free(pnf_p7_nr_t* pnf_p7, void* ptr);
pnf_p7_rx_message_t* pnf_p7_nr_rx_reassembly_queue_add_segment(pnf_p7_nr_t* pnf_p7,
                                                                pnf_p7_rx_reassembly_queue_t* queue,
                                                                uint32_t rx_hr_time,
                                                                uint16_t sequence_number,
                                                                uint16_t segment_number,
                                                                uint8_t m,
                                                                uint8_t* data,
                                                                uint16_t data_len);
void pnf_p7_nr_rx_reassembly_queue_remove_msg(pnf_p7_nr_t* pnf_p7,
                                              pnf_p7_rx_reassembly_queue_t* queue,
                                              pnf_p7_rx_message_t* msg);
void pnf_p7_nr_rx_reassembly_queue_remove_old_msgs(pnf_p7_nr_t* pnf_p7,
                                                   pnf_p7_rx_reassembly_queue_t* queue,
                                                   uint32_t rx_hr_time,
                                                   uint32_t delta);

#endif /* _PNF_P7_NR_H_ */
