/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "pnf_lte.h"
#include "pnf_nr.h"

#define MAX_SCTP_STREAMS 16

void nfapi_pnf_phy_config_delete_all(nfapi_pnf_config_t* config)
{
  nfapi_pnf_phy_config_t* curr = config->phys;
  while (curr != 0) {
    nfapi_pnf_phy_config_t* to_delete = curr;
    curr = curr->next;
    free(to_delete);
  }

  config->phys = 0;
}

void nfapi_pnf_phy_config_add(nfapi_pnf_config_t* config, nfapi_pnf_phy_config_t* phy)
{
  phy->next = config->phys;
  config->phys = phy;
}

nfapi_pnf_phy_config_t* nfapi_pnf_phy_config_find(nfapi_pnf_config_t* config, uint16_t phy_id)
{
  nfapi_pnf_phy_config_t* curr = config->phys;
  while (curr != 0) {
    if (curr->phy_id == phy_id)
      return curr;

    curr = curr->next;
  }
  return 0;
}

void pnf_handle_vendor_extension(void* pRecvMsg, int recvMsgLen, pnf_t* pnf, uint16_t message_id)
{
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_pnf_config_t* config = &(pnf->_public);

    if (config->allocate_p4_p5_vendor_ext) {
      uint16_t msg_size;
      nfapi_p4_p5_message_header_t* msg = config->allocate_p4_p5_vendor_ext(message_id, &msg_size);

      if (msg == 0) {
        NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to allocate vendor extention structure\n");
        return;
      }

      int unpack_result = nfapi_p5_message_unpack(pRecvMsg, recvMsgLen, msg, msg_size, &config->codec_config);

      if (unpack_result == 0) {
        if (config->vendor_ext)
          config->vendor_ext(config, msg);
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
      }

      if (config->deallocate_p4_p5_vendor_ext)
        config->deallocate_p4_p5_vendor_ext(msg);
    }
  }
}

int pnf_pack_and_send_p5_message(pnf_t* pnf, nfapi_p4_p5_message_header_t* msg, uint32_t msg_len)
{
  int packed_len =
      nfapi_p5_message_pack(msg, msg_len, pnf->tx_message_buffer, sizeof(pnf->tx_message_buffer), &pnf->_public.codec_config);

  if (packed_len < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "nfapi_p5_message_pack failed (%d)\n", packed_len);
    return -1;
  }

  return pnf_send_message(pnf, pnf->tx_message_buffer, packed_len, 0 /*msg->stream_id*/);
}

int pnf_pack_and_send_p4_message(pnf_t* pnf, nfapi_p4_p5_message_header_t* msg, uint32_t msg_len)
{
  int packed_len =
      nfapi_p4_message_pack(msg, msg_len, pnf->tx_message_buffer, sizeof(pnf->tx_message_buffer), &pnf->_public.codec_config);

  if (packed_len < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "nfapi_p4_message_pack failed (%d)\n", packed_len);
    return -1;
  }

  return pnf_send_message(pnf, pnf->tx_message_buffer, packed_len, 0 /*msg->stream_id*/);
}

static int pnf_connect_impl(const char *ip_addr, int port, uint8_t use_sctp, int *p5_sock, uint8_t *terminate)
{
  struct sockaddr_in servaddr;
  uint8_t socketConnected = 0;

  if (ip_addr == 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "vnfIpAddress is null\n");
    return -1;
  }

  (void)memset(&servaddr, 0, sizeof(struct sockaddr_in));

  NFAPI_TRACE(NFAPI_TRACE_INFO, "Starting P5 PNF connection to VNF at %s:%u\n", ip_addr, port);

  struct addrinfo hints, *servinfo;
  memset(&hints, 0, sizeof(hints));

  hints.ai_socktype = SOCK_STREAM;

  char port_str[8];
  snprintf(port_str, sizeof(port_str), "%d", port);
  if (getaddrinfo(ip_addr, port_str, &hints, &servinfo) != 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Failed to get host (%s) addr info h_errno:%d \n", ip_addr, h_errno);
    return -1;
  }

  struct addrinfo* p = servinfo;
  int connected = 0;

  while (p != NULL && connected == 0) {
#ifdef NFAPI_TRACE_ENABLED
    char* family = "Unknown";
    char* address = "Unknown";
    char _addr[128];

    if (p->ai_family == AF_INET6) {
      family = "IPV6";
      struct sockaddr_in6* addr = (struct sockaddr_in6*)p->ai_addr;
      inet_ntop(AF_INET6, &addr->sin6_addr, _addr, sizeof(_addr));
      address = &_addr[0];
    } else if (p->ai_family == AF_INET) {
      family = "IPV4";
      struct sockaddr_in* addr = (struct sockaddr_in*)p->ai_addr;
      address = inet_ntoa(addr->sin_addr);
    }

    NFAPI_TRACE(NFAPI_TRACE_NOTE, "Host address info  %d Family:%s Address:%s\n", i++, family, address);
#endif

    if (use_sctp) {
      if ((*p5_sock = socket(p->ai_family, SOCK_STREAM, IPPROTO_SCTP)) < 0) {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "After P5 socket errno: %d\n", errno);
        freeaddrinfo(servinfo);
        return -1;
      }
      int noDelay;
      struct sctp_initmsg initMsg;

      (void)memset(&initMsg, 0, sizeof(struct sctp_initmsg));

      NFAPI_TRACE(NFAPI_TRACE_NOTE, "PNF Setting the SCTP_INITMSG\n");
      initMsg.sinit_num_ostreams = 5;
      initMsg.sinit_max_instreams = 5;
      if (setsockopt(*p5_sock, IPPROTO_SCTP, SCTP_INITMSG, &initMsg, sizeof(initMsg)) < 0) {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "After setsockopt errno: %d\n", errno);
        freeaddrinfo(servinfo);
        return -1;
      }
      noDelay = 1;
      if (setsockopt(*p5_sock, IPPROTO_SCTP, SCTP_NODELAY, &noDelay, sizeof(noDelay)) < 0) {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "After setsockopt errno: %d\n", errno);
        freeaddrinfo(servinfo);
        return -1;
      }

      struct sctp_event_subscribe events;
      memset((void*)&events, 0, sizeof(events));
      events.sctp_data_io_event = 1;

      if (setsockopt(*p5_sock, SOL_SCTP, SCTP_EVENTS, (const void*)&events, sizeof(events)) < 0) {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "After setsockopt errno: %d\n", errno);
        freeaddrinfo(servinfo);
        return -1;
      }
    } else {
      if ((*p5_sock = socket(p->ai_family, SOCK_STREAM, IPPROTO_IP)) < 0) {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "After P5 socket errno: %d\n", errno);
        freeaddrinfo(servinfo);
        return -1;
      }
    }

    NFAPI_TRACE(NFAPI_TRACE_INFO, "P5 socket created...\n");

    if (connect(*p5_sock, p->ai_addr, p->ai_addrlen) < 0) {
      NFAPI_TRACE(NFAPI_TRACE_ERROR,
                  "After connect (address:%s port:%d) errno: %d\n",
                  ip_addr,
                  port,
                  errno);

      if (errno == EINVAL) {
        freeaddrinfo(servinfo);
        return -1;
      } else {
        if (*terminate != 0) {
          freeaddrinfo(servinfo);
          return 0;
        } else {
          close(*p5_sock);
          sleep(1);
        }
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "connect succeeded...\n");

      connected = 1;
    }

    p = p->ai_next;
  }

  freeaddrinfo(servinfo);

  if (connected == 0)
    return 0;

  NFAPI_TRACE(NFAPI_TRACE_NOTE, "After connect loop\n");
  if (use_sctp) {
    socklen_t optLen;
    struct sctp_status status;

    (void)memset(&status, 0, sizeof(struct sctp_status));

    optLen = (socklen_t)sizeof(struct sctp_status);
    if (getsockopt(*p5_sock, IPPROTO_SCTP, SCTP_STATUS, &status, &optLen) < 0) {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "After getsockopt errno: %d\n", errno);
      return -1;
    } else {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "Association ID = %d\n", status.sstat_assoc_id);
      NFAPI_TRACE(NFAPI_TRACE_INFO, "Receiver window size = %d\n", status.sstat_rwnd);
      NFAPI_TRACE(NFAPI_TRACE_INFO, "In Streams = %d\n", status.sstat_instrms);
      NFAPI_TRACE(NFAPI_TRACE_INFO, "Out Streams = %d\n", status.sstat_outstrms);

      socketConnected = 1;
    }
  } else {
    int error;
    socklen_t len = sizeof(error);

    if (getsockopt(*p5_sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "After getsockopt errno: %d\n", errno);
      return -1;
    } else {
      if (error == 0) {
        socketConnected = 1;
      }
    }
  }

  NFAPI_TRACE(NFAPI_TRACE_NOTE, "Socket %s\n", socketConnected ? "CONNECTED" : "NOT_CONNECTED");
  return socketConnected;
}

int pnf_connect(pnf_t* pnf)
{
  return pnf_connect_impl(pnf->_public.vnf_ip_addr, pnf->_public.vnf_p5_port, pnf->sctp, &pnf->p5_sock, &pnf->terminate);
}

int pnf_nr_connect(pnf_nr_t* pnf)
{
  return pnf_connect_impl(pnf->_public.vnf_ip_addr, pnf->_public.vnf_p5_port, pnf->sctp, &pnf->p5_sock, &pnf->terminate);
}

int pnf_send_message(pnf_t* pnf, uint8_t* msg, uint32_t len, uint16_t stream)
{
  if (pnf->sctp) {
    if (sctp_sendmsg(pnf->p5_sock, msg, len, NULL, 0, 42 /*config->sctp_stream_number*/, 0, stream /*P5_STREAM_ID*/, 0, 0) < 0) {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "sctp_sendmsg failed errno: %d\n", errno);
      return -1;
    }
  } else {
    if (write(pnf->p5_sock, msg, len) != len) {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "read failed errno: %d\n", errno);
      return -1;
    }
  }
  return 0;
}

int pnf_nr_send_message(pnf_nr_t* pnf, uint8_t* msg, uint32_t len, uint16_t stream)
{
  if (pnf->sctp) {
    if (sctp_sendmsg(pnf->p5_sock, msg, len, NULL, 0, 42 /*config->sctp_stream_number*/, 0, stream /*P5_STREAM_ID*/, 0, 0) < 0) {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "sctp_sendmsg failed errno: %d\n", errno);
      return -1;
    }
  } else {
    if (write(pnf->p5_sock, msg, len) != len) {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "read failed errno: %d\n", errno);
      return -1;
    }
  }
  return 0;
}
