/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#ifndef _PNF_P7_COMMON_H_
#define _PNF_P7_COMMON_H_

#include <stdint.h>

#define TIMEHR_SEC(_time_hr) ((uint32_t)(_time_hr) >> 20)
#define TIMEHR_USEC(_time_hr) ((uint32_t)(_time_hr) & 0xFFFFF)
#define TIME2TIMEHR(_time) (((uint32_t)(_time.tv_sec) & 0xFFF) << 20 | ((uint32_t)(_time.tv_usec) & 0xFFFFF))

typedef struct {
  uint16_t ontime;
  uint16_t late;
} pnf_p7_arr_time_t;

typedef struct {
  uint32_t bytes;
} pnf_p7_dir_t;

typedef struct {
  pnf_p7_arr_time_t dl_tti;
  pnf_p7_arr_time_t ul_tti;
  pnf_p7_arr_time_t ul_dci;
  pnf_p7_arr_time_t tx_data;
  pnf_p7_dir_t dl;
  pnf_p7_dir_t ul;
} pnf_p7_nr_stats_t;

typedef struct {
  uint8_t *buffer;
  uint32_t length;
} pnf_p7_rx_message_segment_t;

typedef struct pnf_p7_rx_message pnf_p7_rx_message_t;

typedef struct pnf_p7_rx_message {
  uint8_t sequence_number;
  uint8_t num_segments_received;
  uint8_t num_segments_expected;

  // the spec allows of upto 128 segments, this does seem excessive
  pnf_p7_rx_message_segment_t segments[128];

  uint32_t rx_hr_time;

  pnf_p7_rx_message_t *next;
} pnf_p7_rx_message_t;

typedef struct {
  pnf_p7_rx_message_t *msg_queue;
} pnf_p7_rx_reassembly_queue_t;

uint32_t pnf_get_current_time_hr(void);
struct timespec pnf_timespec_sub(struct timespec lhs, struct timespec rhs);
struct timespec pnf_timespec_add(struct timespec lhs, struct timespec rhs);

#endif /* _PNF_P7_COMMON_H_ */
