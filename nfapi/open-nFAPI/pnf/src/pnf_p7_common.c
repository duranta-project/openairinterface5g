/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#include <sys/select.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include "pnf_p7.h"

uint32_t pnf_get_current_time_hr(void)
{
  struct timeval now;
  (void)gettimeofday(&now, NULL);
  uint32_t time_hr = TIME2TIMEHR(now);
  return time_hr;
}

void* pnf_p7_malloc(pnf_p7_t* pnf_p7, size_t size)
{
  if (pnf_p7->_public.malloc) {
    return (pnf_p7->_public.malloc)(size);
  } else {
    return calloc(1, size);
  }
}

void pnf_p7_free(pnf_p7_t* pnf_p7, void* ptr)
{
  if (pnf_p7->_public.free) {
    return (pnf_p7->_public.free)(ptr);
  } else {
    return free(ptr);
  }
}

pnf_p7_rx_message_t* pnf_p7_rx_reassembly_queue_add_segment(pnf_p7_t* pnf_p7,
                                                            pnf_p7_rx_reassembly_queue_t* queue,
                                                            uint32_t rx_hr_time,
                                                            uint16_t sequence_number,
                                                            uint16_t segment_number,
                                                            uint8_t m,
                                                            uint8_t* data,
                                                            uint16_t data_len)
{
  pnf_p7_rx_message_t* msg = 0;
  // attempt to find a entry for this segment
  pnf_p7_rx_message_t* iterator = queue->msg_queue;
  while (iterator != 0) {
    if (iterator->sequence_number == sequence_number) {
      msg = iterator;
      break;
    }

    iterator = iterator->next;
  }

  // if found then copy data to message
  if (msg != 0) {
    msg->segments[segment_number].buffer = (uint8_t*)pnf_p7_malloc(pnf_p7, data_len);
    memcpy(msg->segments[segment_number].buffer, data, data_len);
    msg->segments[segment_number].length = data_len;

    msg->num_segments_received++;

    // set the segement number if we have the last segment
    if (m == 0)
      msg->num_segments_expected = segment_number + 1;
  }
  // else add new rx message entry
  else {
    // create a new message
    msg = (pnf_p7_rx_message_t*)(pnf_p7_malloc(pnf_p7, sizeof(pnf_p7_rx_message_t)));
    memset(msg, 0, sizeof(pnf_p7_rx_message_t));

    msg->sequence_number = sequence_number;
    msg->num_segments_expected = m ? 255 : segment_number + 1;
    msg->num_segments_received = 1;
    msg->rx_hr_time = rx_hr_time;

    msg->segments[segment_number].buffer = (uint8_t*)pnf_p7_malloc(pnf_p7, data_len);
    memcpy(msg->segments[segment_number].buffer, data, data_len);
    msg->segments[segment_number].length = data_len;

    // place the message at the head of the queue
    msg->next = queue->msg_queue;
    queue->msg_queue = msg;
  }

  return msg;
}

void pnf_p7_rx_reassembly_queue_remove_msg(pnf_p7_t* pnf_p7, pnf_p7_rx_reassembly_queue_t* queue, pnf_p7_rx_message_t* msg)
{
  // remove message if it has the same sequence number
  pnf_p7_rx_message_t* iterator = queue->msg_queue;
  pnf_p7_rx_message_t* previous = 0;

  while (iterator != 0) {
    if (iterator->sequence_number == msg->sequence_number) {
      if (previous == 0) {
        queue->msg_queue = iterator->next;
      } else {
        previous->next = iterator->next;
      }

      uint16_t i;
      for (i = 0; i < 128; ++i) {
        if (iterator->segments[i].buffer)
          pnf_p7_free(pnf_p7, iterator->segments[i].buffer);
      }
      pnf_p7_free(pnf_p7, iterator);

      break;
    }

    previous = iterator;
    iterator = iterator->next;
  }
}

void pnf_p7_rx_reassembly_queue_remove_old_msgs(pnf_p7_t* pnf_p7,
                                                pnf_p7_rx_reassembly_queue_t* queue,
                                                uint32_t rx_hr_time,
                                                uint32_t delta)
{
  // remove all messages that are too old
  pnf_p7_rx_message_t* iterator = queue->msg_queue;
  pnf_p7_rx_message_t* previous = 0;

  while (iterator != 0) {
    if (rx_hr_time - iterator->rx_hr_time > delta) {
      if (previous == 0) {
        queue->msg_queue = iterator->next;
      } else {
        previous->next = iterator->next;
      }

      NFAPI_TRACE(NFAPI_TRACE_WARN,
                  "Deleting stale reassembly message (packet rx_hr_time %u current rx_hr_time %u delta %d us)\n",
                  iterator->rx_hr_time,
                  rx_hr_time,
                  delta);

      pnf_p7_rx_message_t* to_delete = iterator;
      iterator = iterator->next;

      uint16_t i;
      for (i = 0; i < 128; ++i) {
        if (to_delete->segments[i].buffer)
          pnf_p7_free(pnf_p7, to_delete->segments[i].buffer);
      }
      pnf_p7_free(pnf_p7, to_delete);

    } else {
      previous = iterator;
      iterator = iterator->next;
    }
  }
}

int pnf_p7_send_message(pnf_p7_t* pnf_p7, uint8_t* msg, uint32_t len)
{
  struct sockaddr_in remote_addr = {.sin_family = AF_INET, .sin_port = htons(pnf_p7->_public.remote_p7_port)};

  if (inet_aton(pnf_p7->_public.remote_p7_addr, &remote_addr.sin_addr) == -1) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "inet_aton failed %d\n", errno);
    return -1;
  }

  socklen_t remote_addr_len = sizeof(struct sockaddr_in);

  int sendto_result;

  if ((sendto_result =
           sendto((int)pnf_p7->p7_sock, (const char*)msg, len, 0, (const struct sockaddr*)&remote_addr, remote_addr_len))
      < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR,
                "%s %s:%d sendto(%d, %p, %d) %d failed errno: %d\n",
                __FUNCTION__,
                pnf_p7->_public.remote_p7_addr,
                pnf_p7->_public.remote_p7_port,
                (int)pnf_p7->p7_sock,
                (const char*)msg,
                len,
                remote_addr_len,
                errno);
    return -1;
  }

  if (sendto_result != len) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s sendto failed to send the entire message %d %d\n", __FUNCTION__, sendto_result, len);
  }
  return 0;
}

int pnf_p7_pack_and_send_p7_message(pnf_p7_t* pnf_p7, nfapi_p7_message_header_t* header, uint32_t msg_len)
{
  UNUSED(msg_len)
  header->m_segment_sequence = NFAPI_P7_SET_MSS(0, 0, pnf_p7->sequence_number);

  // Need to guard against different threads calling the encode function at the same time
  if (pthread_mutex_lock(&(pnf_p7->pack_mutex)) != 0) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to lock mutex\n");
    return -1;
  }

  int len =
      nfapi_p7_message_pack(header, pnf_p7->tx_message_buffer, sizeof(pnf_p7->tx_message_buffer), &pnf_p7->_public.codec_config);

  if (len < 0) {
    if (pthread_mutex_unlock(&(pnf_p7->pack_mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to unlock mutex\n");
      return -1;
    }

    NFAPI_TRACE(NFAPI_TRACE_ERROR, "nfapi_p7_message_pack failed with return %d\n", len);
    return -1;
  }

  if (len > pnf_p7->_public.segment_size) {
    int msg_body_len = len - NFAPI_P7_HEADER_LENGTH;
    int seg_body_len = pnf_p7->_public.segment_size - NFAPI_P7_HEADER_LENGTH;
    int segment_count = (msg_body_len / (seg_body_len)) + ((msg_body_len % seg_body_len) ? 1 : 0);

    int segment = 0;
    int offset = NFAPI_P7_HEADER_LENGTH;
    uint8_t buffer[pnf_p7->_public.segment_size];
    for (segment = 0; segment < segment_count; ++segment) {
      uint8_t last = 0;
      uint16_t size = pnf_p7->_public.segment_size - NFAPI_P7_HEADER_LENGTH;
      if (segment + 1 == segment_count) {
        last = 1;
        size = (msg_body_len) - (seg_body_len * segment);
      }

      uint16_t segment_size = size + NFAPI_P7_HEADER_LENGTH;

      memcpy(&buffer[0], pnf_p7->tx_message_buffer, NFAPI_P7_HEADER_LENGTH);

      buffer[4] = (segment_size & 0xFF00) >> 8;
      buffer[5] = (segment_size & 0xFF);

      buffer[6] = ((!last) << 7) + segment;

      memcpy(&buffer[NFAPI_P7_HEADER_LENGTH], pnf_p7->tx_message_buffer + offset, size);
      offset += size;

      if (pnf_p7->_public.checksum_enabled) {
        nfapi_p7_update_checksum(buffer, segment_size);
      }

      pnf_p7_send_message(pnf_p7, &buffer[0], segment_size);
    }
  } else {
    if (pnf_p7->_public.checksum_enabled) {
      nfapi_p7_update_checksum(pnf_p7->tx_message_buffer, len);
    }

    pnf_p7_send_message(pnf_p7, pnf_p7->tx_message_buffer, len);
  }

  pnf_p7->sequence_number++;

  if (pthread_mutex_unlock(&(pnf_p7->pack_mutex)) != 0) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to unlock mutex\n");
    return -1;
  }

  return 0;
}

void pnf_handle_p7_vendor_extension(void* pRecvMsg, int recvMsgLen, pnf_p7_t* pnf_p7, uint16_t message_id)
{
  if (pnf_p7->_public.allocate_p7_vendor_ext) {
    uint16_t msg_size;
    nfapi_p7_message_header_t* msg = pnf_p7->_public.allocate_p7_vendor_ext(message_id, &msg_size);

    if (msg == 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to allocate vendor extention structure\n");
      return;
    }

    int unpack_result = nfapi_p7_message_unpack(pRecvMsg, recvMsgLen, msg, msg_size, &pnf_p7->_public.codec_config);

    if (unpack_result == 0) {
      if (pnf_p7->_public.vendor_ext)
        pnf_p7->_public.vendor_ext(&(pnf_p7->_public), msg);
    }

    if (pnf_p7->_public.deallocate_p7_vendor_ext)
      pnf_p7->_public.deallocate_p7_vendor_ext(msg);
  }
}

struct timespec pnf_timespec_add(struct timespec lhs, struct timespec rhs)
{
  struct timespec result;

  result.tv_sec = lhs.tv_sec + rhs.tv_sec;
  result.tv_nsec = lhs.tv_nsec + rhs.tv_nsec;

  if (result.tv_nsec > 1e9) {
    result.tv_sec++;
    result.tv_nsec -= 1e9;
  }

  return result;
}

struct timespec pnf_timespec_sub(struct timespec lhs, struct timespec rhs)
{
  struct timespec result;
  if ((lhs.tv_nsec - rhs.tv_nsec) < 0) {
    result.tv_sec = lhs.tv_sec - rhs.tv_sec - 1;
    result.tv_nsec = 1000000000 + lhs.tv_nsec - rhs.tv_nsec;
  } else {
    result.tv_sec = lhs.tv_sec - rhs.tv_sec;
    result.tv_nsec = lhs.tv_nsec - rhs.tv_nsec;
  }
  return result;
}
