/*
* Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
* contributor license agreements.  See the NOTICE file distributed with
* this work for additional information regarding copyright ownership.
* The OpenAirInterface Software Alliance licenses this file to You under
* the OAI Public License, Version 1.1  (the "License"); you may not use this file
* except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.openairinterface.org/?page_id=698
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*-------------------------------------------------------------------------------
* For more information about the OpenAirInterface (OAI) Software Alliance:
*      contact@openairinterface.org
*/

/*! \file fapi/oai-integration/fapi_nvIPC.c
* \brief OAI MAC/Aerial L1 interface file using FAPI over shared memory
* \author Ruben S. Silva
* \date 2023
* \version 0.1
* \company OpenAirInterface Software Alliance
* \email: contact@openairinterface.org, rsilva@allbesmart.pt
* \note
* \warning
*/

#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <stdatomic.h>
#include <sys/queue.h>
#include <sys/epoll.h>
#include "fapi_nvIPC.h"
#include <nfapi_vnf.h>
#include <nr_fapi_p7_utils.h>
#include "nfapi_interface.h"
#include "nfapi.h"
#include "nfapi_nr_interface_scf.h"
#include "nfapi/open-nFAPI/vnf/inc/vnf_p7.h"
#include "system.h"
#include "fapi_vnf_p7.h"

#define MAX_EVENTS 10
#define RECV_BUF_LEN 8192
char cpu_buf_recv[RECV_BUF_LEN];

uint16_t sfn = 0, slot = 0;
nv_ipc_t *ipc;
void nvIPC_Stop()
{
  LOG_I(NR_MAC, "Received STOP.indication\n");
  ((vnf_t *)get_config())->terminate = true;
}

void nvIPC_send_stop_request()
{
  nfapi_nr_stop_request_scf_t req = {.header.message_id = NFAPI_NR_PHY_MSG_TYPE_STOP_REQUEST, .header.phy_id = 0};
  LOG_I(NR_MAC, "Sending NFAPI STOP.request\n");
  nfapi_nr_vnf_stop_req(get_config(), get_config()->pnf_list->p5_idx, &req);
}


static uint16_t old_sfn = 0;
static uint16_t old_slot = 0;
////////////////////////////////////////////////////////////////////////
// Handle an RX message
static int ipc_handle_rx_msg(nv_ipc_t *ipc, nv_ipc_msg_t *msg)
{
  if (msg == NULL) {
    LOG_E(NFAPI_VNF, "%s: ERROR: buffer is empty\n", __func__);
    return -1;
  }

  uint8_t *pReadPackedMessage = msg->msg_buf;
  uint8_t *end = msg->msg_buf + msg->msg_len;

  // unpack FAPI messages and handle them
  nfapi_vnf_config_t * vnf_config = get_config();
  if (vnf_config != 0) {
    // first, unpack the header
    fapi_phy_api_msg fapi_msg;
    if (!(pull8(&pReadPackedMessage, &fapi_msg.num_msg, end) && pull8(&pReadPackedMessage, &fapi_msg.opaque_handle, end)
          && pull16(&pReadPackedMessage, &fapi_msg.message_id, end)
          && pull32(&pReadPackedMessage, &fapi_msg.message_length, end))) {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "FAPI message header unpack failed\n");
      return -1;
    }

   vnf_p7_t *vnf_p7_config = (vnf_p7_t *)((vnf_info *)vnf_config->user_data)->p7_vnfs->config;
    switch (fapi_msg.message_id) {
      case NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE ... NFAPI_NR_PHY_MSG_TYPE_ERROR_INDICATION:
        vnf_nr_handle_p4_p5_message(msg->msg_buf, msg->msg_len, 1, vnf_config);
        break;
      // P7 Messages
      case NFAPI_NR_PHY_MSG_TYPE_RX_DATA_INDICATION: {
        uint8_t *pReadData = msg->data_buf;
        int dataBufLen = msg->data_len;
        uint8_t *data_end = msg->data_buf + dataBufLen;
        nfapi_nr_rx_data_indication_t ind;
        ind.header.message_id = fapi_msg.message_id;
        ind.header.message_length = fapi_msg.message_length;
        aerial_unpack_nr_rx_data_indication(
            &pReadPackedMessage,
            end,
            &pReadData,
            data_end,
            &ind,
            &vnf_p7_config->_public.codec_config);
        NFAPI_TRACE(NFAPI_TRACE_INFO, "%s: Handling RX Indication\n", __FUNCTION__);
        if (vnf_p7_config->_public.nr_rx_data_indication) {
          (vnf_p7_config->_public.nr_rx_data_indication)(&ind);
          for (int i = 0; i < ind.number_of_pdus; ++i) {
            free(ind.pdu_list[i].pdu);
          }
          free(ind.pdu_list);
        }
        break;
      }

      case NFAPI_NR_PHY_MSG_TYPE_SRS_INDICATION: {
        uint8_t *pReadData = msg->data_buf;
        int dataBufLen = msg->data_len;
        uint8_t *data_end = msg->data_buf + dataBufLen;
        nfapi_nr_srs_indication_t ind;
        aerial_unpack_nr_srs_indication(&pReadPackedMessage,
                                        end,
                                        &pReadData,
                                        data_end,
                                        &ind,
                                        &vnf_p7_config->_public.codec_config);
        if (vnf_p7_config->_public.nr_srs_indication) {
          (vnf_p7_config->_public.nr_srs_indication)(&ind);
        }
        break;
      }

      case NFAPI_NR_PHY_MSG_TYPE_SLOT_INDICATION: {
        nfapi_nr_slot_indication_scf_t ind;
        if (!vnf_p7_config->_public.unpack_func(msg->msg_buf, msg->msg_len, &ind, sizeof(ind), &vnf_p7_config->_public.codec_config)) {
          NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Failed to unpack message\n", __FUNCTION__);
        } else {
          NFAPI_TRACE(NFAPI_TRACE_DEBUG, "%s: Handling NR SLOT Indication\n", __FUNCTION__);
          // check if the sfn/slot unpacked come wrong at any time, should be old + 1 (slot 0 -- 19, sfn 0 -- 1023)
          // add 1 to current sfn number
          uint16_t old_slot_plus = ((old_slot + 1) % 20);
          uint16_t old_sfn_plus = old_slot_plus == 0 ? ((old_sfn + 1) % 1024) : old_sfn;
          if (old_slot_plus != ind.slot || old_sfn_plus != ind.sfn) {
            LOG_E(NFAPI_VNF,
                  "\n============================================================================\n"
                  "sfn slot doesn't match unpacked one! L2->L1 %d.%d  vs L1->L2 %d.%d  \n"
                  "============================================================================\n",
                  old_sfn,
                  old_slot,
                  ind.sfn,
                  ind.slot);
          }
          old_sfn = ind.sfn;
          old_slot = ind.slot;
          if (vnf_p7_config->_public.nr_slot_indication) {
            (vnf_p7_config->_public.nr_slot_indication)(&ind);
          }
        }
        break;
      }
      case NFAPI_NR_PHY_MSG_TYPE_CRC_INDICATION:
      case NFAPI_NR_PHY_MSG_TYPE_UCI_INDICATION:
      case NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION: {
        vnf_nr_handle_p7_message(msg->msg_buf, msg->msg_len, vnf_p7_config);
        break;
      }
      default: {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s P5 Unknown message ID %d\n", __FUNCTION__, fapi_msg.message_id);

        break;
      }
    }
  }
  return 0;
}

int8_t buf[1024];

nv_ipc_config_t nv_ipc_config;

bool aerial_nr_send_p5_message(vnf_t *vnf, uint16_t p5_idx, nfapi_nr_p4_p5_message_header_t *msg, uint32_t msg_len)
{
  nfapi_vnf_pnf_info_t *pnf = nfapi_vnf_pnf_list_find(&(vnf->_public), p5_idx);

  if (pnf) {
    uint8_t tx_messagebufferFAPI[sizeof(vnf->tx_message_buffer)];
    int packedMessageLengthFAPI = -1;
    packedMessageLengthFAPI =
        vnf->_public.pack_func(msg, msg_len, tx_messagebufferFAPI, sizeof(tx_messagebufferFAPI), &vnf->_public.codec_config);
    return aerial_send_P5_msg(tx_messagebufferFAPI, packedMessageLengthFAPI, msg) == 0;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "%s() cannot find pnf info for p5_idx:%d\n", __FUNCTION__, p5_idx);
    return false;
  }
}

bool aerial_send_P5_msg(void *packedBuf, uint32_t packedMsgLength, nfapi_nr_p4_p5_message_header_t *header)
{
  if (ipc == NULL) {
    return false;
  }
  nv_ipc_msg_t send_msg = {0};
  // look for the specific message
  switch (header->message_id) {
    case NFAPI_NR_PHY_MSG_TYPE_PARAM_REQUEST:
    case NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE:
    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_REQUEST:
    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_RESPONSE:
    case NFAPI_NR_PHY_MSG_TYPE_START_REQUEST:
    case NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE:
    case NFAPI_NR_PHY_MSG_TYPE_STOP_REQUEST:
    case NFAPI_NR_PHY_MSG_TYPE_STOP_RESPONSE:
      break;
    default: {
      if (header->message_id >= NFAPI_VENDOR_EXT_MSG_MIN && header->message_id <= NFAPI_VENDOR_EXT_MSG_MAX) {
        // if(config && config->pack_p4_p5_vendor_extension) {
        //   result = (config->pack_p4_p5_vendor_extension)(header, ppWritePackedMsg, end, config);
        // } else {
        //   NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s VE NFAPI message ID %d. No ve ecoder provided\n", __FUNCTION__, header->message_id);
        // }
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s NFAPI Unknown message ID %d\n", __FUNCTION__, header->message_id);
      }
    } break;
  }
  send_msg.msg_id = header->message_id;
  send_msg.cell_id = 0;
  send_msg.msg_len = packedMsgLength + 8; // adding 8 to account for the size of the FAPI header
  send_msg.data_len = 0;
  send_msg.data_buf = NULL;
  send_msg.data_pool = NV_IPC_MEMPOOL_CPU_MSG;

  // procedure is  allocate->fill->send
  int alloc_retval = ipc->tx_allocate(ipc, &send_msg, 0);
  if (alloc_retval != 0) {
    LOG_E(NFAPI_VNF, "%s error: allocate TX buffer failed Error: %d\n", __FUNCTION__, alloc_retval);
    ipc->tx_release(ipc, &send_msg);
    return false;
  }

  memcpy(send_msg.msg_buf, packedBuf, send_msg.msg_len);
  LOG_D(NFAPI_VNF,
        "send: cell_id=%d msg_id=0x%02X msg_len=%d data_len=%d data_pool=%d\n",
        send_msg.cell_id,
        send_msg.msg_id,
        send_msg.msg_len,
        send_msg.data_len,
        send_msg.data_pool);
  // Send the message
  int send_retval = ipc->tx_send_msg(ipc, &send_msg);
  if (send_retval < 0) {
    LOG_E(NFAPI_VNF, "%s error: send TX message failed Error: %d\n", __FUNCTION__, send_retval);
    ipc->tx_release(ipc, &send_msg);
    return false;
  }

  ipc->notify(ipc, 1); // notify that there's 1 message in queue
  return true;
}

bool aerial_send_P7_msg(void *packedBuf, uint32_t packedMsgLength, nfapi_nr_p7_message_header_t *header)
{
  if (ipc == NULL) {
    return false;
  }
  nv_ipc_msg_t send_msg;
  uint8_t *pPacketBodyField = &((uint8_t *)packedBuf)[8];
  uint8_t *pPackMessageEnd = packedBuf + packedMsgLength + 8;

  uint16_t present_sfn = 0;
  uint16_t present_slot = 0;
  pull16(&pPacketBodyField, &present_sfn, pPackMessageEnd);
  pull16(&pPacketBodyField, &present_slot, pPackMessageEnd);

  if (present_sfn != old_sfn || present_slot != old_slot) {
    LOG_E(NFAPI_VNF,
          "\n============================================================================\n"
          "sfn slot doesn't match unpacked one! L2->L1 %d.%d  vs L1->L2 %d.%d  \n"
          "============================================================================\n",
          present_sfn,
          present_slot,
          old_sfn,
          old_slot);
  }
  // look for the specific message
  switch (header->message_id) {
    case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST:
    case NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST:
    case NFAPI_NR_PHY_MSG_TYPE_TX_DATA_REQUEST:
    case NFAPI_NR_PHY_MSG_TYPE_UL_DCI_REQUEST:
    case NFAPI_UE_RELEASE_REQUEST:
    case NFAPI_UE_RELEASE_RESPONSE:
    case NFAPI_NR_PHY_MSG_TYPE_SLOT_INDICATION:
    case NFAPI_NR_PHY_MSG_TYPE_RX_DATA_INDICATION:
    case NFAPI_NR_PHY_MSG_TYPE_CRC_INDICATION:
    case NFAPI_NR_PHY_MSG_TYPE_UCI_INDICATION:
    case NFAPI_NR_PHY_MSG_TYPE_SRS_INDICATION:
    case NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION:
    case NFAPI_NR_PHY_MSG_TYPE_DL_NODE_SYNC:
    case NFAPI_NR_PHY_MSG_TYPE_UL_NODE_SYNC:
    case NFAPI_TIMING_INFO:
    case 0x8f:
      break;
    default: {
      if (header->message_id >= NFAPI_VENDOR_EXT_MSG_MIN && header->message_id <= NFAPI_VENDOR_EXT_MSG_MAX) {
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s NFAPI Unknown message ID %d\n", __FUNCTION__, header->message_id);
      }
    } break;
  }

  send_msg.msg_id = header->message_id;
  send_msg.cell_id = 0;
  send_msg.msg_len = packedMsgLength + 8; // adding 8 to account for the size of the FAPI header
  send_msg.data_len = 0;
  send_msg.data_buf = NULL;
  send_msg.data_pool = NV_IPC_MEMPOOL_CPU_MSG;
  // procedure is  allocate->fill->send

  // Allocate buffer for TX message
  int alloc_retval = ipc->tx_allocate(ipc, &send_msg, 0);
  if (alloc_retval != 0) {
    LOG_E(NFAPI_VNF, "%s error: allocate TX buffer failed Error: %d\n", __FUNCTION__, alloc_retval);
    ipc->tx_release(ipc, &send_msg);
    return false;
  }

  memcpy(send_msg.msg_buf, packedBuf, send_msg.msg_len);
  LOG_D(NFAPI_VNF,
        "send: cell_id=%d msg_id=0x%02X msg_len=%d data_len=%d data_pool=%d\n",
        send_msg.cell_id,
        send_msg.msg_id,
        send_msg.msg_len,
        send_msg.data_len,
        send_msg.data_pool);
  // Send the message
  int send_retval = ipc->tx_send_msg(ipc, &send_msg);
  if (send_retval < 0) {
    LOG_E(NFAPI_VNF, "%s error: send TX message failed Error: %d\n", __FUNCTION__, send_retval);
    ipc->tx_release(ipc, &send_msg);
    return false;
  }

  ipc->notify(ipc, 1); // notify that there's 1 message in queue
  return true;
}

bool aerial_send_P7_msg_with_data(void *packedBuf,
                                 uint32_t packedMsgLength,
                                 void *dataBuf,
                                 uint32_t dataLength,
                                 nfapi_nr_p7_message_header_t *header)
{
  if (ipc == NULL) {
    return false;
  }
  nv_ipc_msg_t send_msg;
  uint8_t *pPacketBodyField = &((uint8_t *)packedBuf)[8];
  uint8_t *pPackMessageEnd = packedBuf + packedMsgLength + 8;
  uint16_t present_sfn = 0;
  uint16_t present_slot = 0;
  pull16(&pPacketBodyField, &present_sfn, pPackMessageEnd);
  pull16(&pPacketBodyField, &present_slot, pPackMessageEnd);

  if (present_sfn != old_sfn || present_slot != old_slot) {
    LOG_E(NFAPI_VNF,
          "\n============================================================================\n"
          "sfn slot doesn't match unpacked one! L2->L1 %d.%d  vs L1->L2 %d.%d  \n"
          "============================================================================\n",
          present_sfn,
          present_slot,
          old_sfn,
          old_slot);
  }
  // look for the specific message
  switch (header->message_id) {
    case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST:
    case NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST:
    case NFAPI_NR_PHY_MSG_TYPE_TX_DATA_REQUEST:
    case NFAPI_NR_PHY_MSG_TYPE_UL_DCI_REQUEST:
    case NFAPI_UE_RELEASE_REQUEST:
    case NFAPI_UE_RELEASE_RESPONSE:
    case NFAPI_NR_PHY_MSG_TYPE_SLOT_INDICATION:
    case NFAPI_NR_PHY_MSG_TYPE_RX_DATA_INDICATION:
    case NFAPI_NR_PHY_MSG_TYPE_CRC_INDICATION:
    case NFAPI_NR_PHY_MSG_TYPE_UCI_INDICATION:
    case NFAPI_NR_PHY_MSG_TYPE_SRS_INDICATION:
    case NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION:
    case NFAPI_NR_PHY_MSG_TYPE_DL_NODE_SYNC:
    case NFAPI_NR_PHY_MSG_TYPE_UL_NODE_SYNC:
    case NFAPI_TIMING_INFO:
      break;
    default: {
      if (header->message_id >= NFAPI_VENDOR_EXT_MSG_MIN && header->message_id <= NFAPI_VENDOR_EXT_MSG_MAX) {
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s NFAPI Unknown message ID %d\n", __FUNCTION__, header->message_id);
      }
    } break;
  }

  send_msg.msg_id = header->message_id;
  send_msg.cell_id = 0;
  send_msg.msg_len = packedMsgLength + 8; // adding 8 to account for the size of the FAPI header
  send_msg.data_len = dataLength;
  send_msg.data_pool = NV_IPC_MEMPOOL_CPU_DATA;

  // procedure is  allocate->fill->send

  // Allocate buffer for TX message
  int alloc_retval = ipc->tx_allocate(ipc, &send_msg, 0);
  if (alloc_retval != 0) {
    LOG_E(NFAPI_VNF, "%s error: allocate TX buffer failed Error: %d\n", __FUNCTION__, alloc_retval);
    ipc->tx_release(ipc, &send_msg);
    return false;
  }

  memcpy(send_msg.msg_buf, packedBuf, send_msg.msg_len);
  memcpy(send_msg.data_buf, dataBuf, send_msg.data_len);
  LOG_D(NFAPI_VNF,
        "send: cell_id=%d msg_id=0x%02X msg_len=%d data_len=%d data_pool=%d\n",
        send_msg.cell_id,
        send_msg.msg_id,
        send_msg.msg_len,
        send_msg.data_len,
        send_msg.data_pool);
  // Send the message
  int send_retval = ipc->tx_send_msg(ipc, &send_msg);
  if (send_retval != 0) {
    LOG_E(NFAPI_VNF, "%s error: send TX message failed Error: %d\n", __FUNCTION__, send_retval);
    ipc->tx_release(ipc, &send_msg);
    return false;
  }

  ipc->notify(ipc, 1); // notify that there's 1 message in queue
  return true;
}

// Always allocate message buffer, but allocate data buffer only when data_len > 0
static int aerial_recv_msg(nv_ipc_t *ipc, nv_ipc_msg_t *recv_msg)
{
  if (ipc == NULL) {
    return -1;
  }
  recv_msg->msg_buf = NULL;
  recv_msg->data_buf = NULL;

  // Allocate buffer for TX message
  if (ipc->rx_recv_msg(ipc, recv_msg) < 0) {
    LOG_D(NFAPI_VNF, "%s: no more message available\n", __func__);
    return -1;
  }
  LOG_D(NFAPI_VNF,
         "recv: cell_id=%d msg_id=0x%02X msg_len=%d data_len=%d data_pool=%d\n",
         recv_msg->cell_id,
         recv_msg->msg_id,
         recv_msg->msg_len,
         recv_msg->data_len,
         recv_msg->data_pool);
  ipc_handle_rx_msg(ipc, recv_msg);

  // Release buffer of RX message
  int release_retval = ipc->rx_release(ipc, recv_msg);
  if (release_retval != 0) {
    LOG_E(NFAPI_VNF, "%s error: release RX buffer failed Error: %d\n", __FUNCTION__, release_retval);
    return release_retval;
  }
  return 0;
}

void *epoll_recv_task(void *arg)
{
  struct epoll_event ev, events[MAX_EVENTS];

  LOG_D(NFAPI_VNF,"Aerial recv task start \n");

  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    LOG_E(NFAPI_VNF, "%s epoll_create failed\n", __func__);
  }

  int ipc_rx_event_fd = ipc->get_fd(ipc);
  ev.events = EPOLLIN;
  ev.data.fd = ipc_rx_event_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev) == -1) {
    LOG_E(NFAPI_VNF, "%s epoll_ctl failed\n", __func__);
  }
  // From here on out the thread is ready to receive data, simulate the reception
  // of a PARAM.response to get the VNF to send a CONFIG.request
  nfapi_nr_param_response_scf_t resp_msg = {.header.message_id = NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE};
  nfapi_vnf_config_t * vnf_config = get_config();
  vnf_config->nr_param_resp(vnf_config, 1, &resp_msg);
  while (((vnf_t *)vnf_config)->terminate == false) {
    LOG_D(NFAPI_VNF, "%s: epoll_wait fd_rx=%d ...\n", __func__, ipc_rx_event_fd);

    int nfds;
    do {
      // epoll_wait() may return EINTR when get unexpected signal SIGSTOP from system
      nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    } while (nfds == -1 && errno == EINTR);

    if (nfds < 0) {
      LOG_E(NFAPI_VNF, "epoll_wait failed: epoll_fd=%d nfds=%d err=%d - %s\n", epoll_fd, nfds, errno, strerror(errno));
    }

    int n = 0;
    for (n = 0; n < nfds; ++n) {
      if (events[n].data.fd == ipc_rx_event_fd) {
        ipc->get_value(ipc);
        nv_ipc_msg_t recv_msg;
        while (aerial_recv_msg(ipc, &recv_msg) == 0);
      }
    }
  }
  close(epoll_fd);
  ipc->ipc_destroy(ipc);
  return NULL;
}

void create_recv_thread(int8_t affinity)
{
  pthread_t thread_id;
  threadCreate(&thread_id, epoll_recv_task, NULL, "vnf_nvipc_aerial", affinity, OAI_PRIORITY_RT_MAX);
}

int load_hard_code_config(nv_ipc_config_t *config, int module_type, nv_ipc_transport_t _transport)
{
  // Create configuration
  config->ipc_transport = _transport;
  if (set_nv_ipc_default_config(config, module_type) < 0) {
    LOG_E(NFAPI_VNF, "%s: set configuration failed\n", __func__);
    return -1;
  }

  int test_cuda_device_id = -1;
  LOG_D(NFAPI_VNF,"CUDA device ID configured : %d \n", test_cuda_device_id);
  config->transport_config.shm.cuda_device_id = test_cuda_device_id;
  if (test_cuda_device_id >= 0) {
    config->transport_config.shm.mempool_size[NV_IPC_MEMPOOL_CUDA_DATA].pool_len = 128;
    config->transport_config.shm.mempool_size[NV_IPC_MEMPOOL_CPU_DATA].pool_len = 1024;
    config->transport_config.shm.mempool_size[NV_IPC_MEMPOOL_CPU_MSG].pool_len = 4096;
  }

  return 0;
}

int nvIPC_Init(nvipc_params_t nvipc_params_s)
{
  // Want to use transport SHM, type epoll, module secondary (reads the created shm from cuphycontroller)
  load_hard_code_config(&nv_ipc_config, NV_IPC_MODULE_SECONDARY, NV_IPC_TRANSPORT_SHM);
  // Create nv_ipc_t instance
  LOG_I(NFAPI_VNF, "%s: creating IPC interface with prefix %s\n", __func__, nvipc_params_s.nvipc_shm_prefix);
  strcpy(nv_ipc_config.transport_config.shm.prefix, nvipc_params_s.nvipc_shm_prefix);
  if ((ipc = create_nv_ipc_interface(&nv_ipc_config)) == NULL) {
    LOG_E(NFAPI_VNF, "%s: create IPC interface failed\n", __func__);
    return -1;
  }
  LOG_I(NFAPI_VNF, "%s: create IPC interface successful\n", __func__);
  sleep(1);
  create_recv_thread(nvipc_params_s.nvipc_poll_core);
  return 0;
}

int oai_fapi_ul_tti_req(nfapi_nr_ul_tti_request_t *ul_tti_req)
{
  nfapi_vnf_p7_config_t *p7_config = get_p7_vnf_config();

  ul_tti_req->header.phy_id = 1; // DJP HACK TODO FIXME - need to pass this around!!!!
  ul_tti_req->header.message_id = NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST;

  // int retval = nfapi_vnf_p7_ul_tti_req(p7_config, ul_tti_req);
  int retval = p7_config->send_p7_msg(get_p7_vnf(), &ul_tti_req->header);

  if (retval != 0) {
    LOG_E(PHY, "%s() Problem sending retval:%d\n", __FUNCTION__, retval);
  } else {
    // Reset number of PDUs so that it is not resent
    ul_tti_req->n_pdus = 0;
    ul_tti_req->n_group = 0;
    ul_tti_req->n_ulcch = 0;
    ul_tti_req->n_ulsch = 0;
  }
  return retval;
}

int oai_fapi_ul_dci_req(nfapi_nr_ul_dci_request_t *ul_dci_req)
{
  nfapi_vnf_p7_config_t *p7_config = get_p7_vnf_config();
  ul_dci_req->header.phy_id = 1; // DJP HACK TODO FIXME - need to pass this around!!!!
  ul_dci_req->header.message_id = NFAPI_NR_PHY_MSG_TYPE_UL_DCI_REQUEST;
  // int retval = nfapi_vnf_p7_ul_dci_req(p7_config, ul_dci_req);
  int retval = p7_config->send_p7_msg(get_p7_vnf(), &ul_dci_req->header);
  if (retval != 0) {
    LOG_E(PHY, "%s() Problem sending retval:%d\n", __FUNCTION__, retval);
  } else {
    ul_dci_req->numPdus = 0;
  }
  return retval;
}

int oai_fapi_tx_data_req(nfapi_nr_tx_data_request_t *tx_data_req)
{
  nfapi_vnf_p7_config_t *p7_config = get_p7_vnf_config();
  tx_data_req->header.phy_id = 1; // DJP HACK TODO FIXME - need to pass this around!!!!
  tx_data_req->header.message_id = NFAPI_NR_PHY_MSG_TYPE_TX_DATA_REQUEST;
  // LOG_D(PHY, "[VNF] %s() TX_REQ sfn_sf:%d number_of_pdus:%d\n", __FUNCTION__, NFAPI_SFNSF2DEC(tx_data_req->SFN),
  // tx_data_req->Number_of_PDUs); int retval = nfapi_vnf_p7_tx_data_req(p7_config, tx_data_req);
  int retval = p7_config->send_p7_msg(get_p7_vnf(), &tx_data_req->header);
  if (retval != 0) {
    LOG_E(PHY, "%s() Problem sending retval:%d\n", __FUNCTION__, retval);
  } else {
    tx_data_req->Number_of_PDUs = 0;
  }

  return retval;
}

int oai_fapi_dl_tti_req(nfapi_nr_dl_tti_request_t *dl_config_req)
{
  nfapi_vnf_p7_config_t *p7_config = get_p7_vnf_config();
  dl_config_req->header.message_id = NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST;
  dl_config_req->header.phy_id = 1; // DJP HACK TODO FIXME - need to pass this around!!!!
  int retval = p7_config->send_p7_msg(get_p7_vnf(), &dl_config_req->header);
  dl_config_req->dl_tti_request_body.nPDUs = 0;
  dl_config_req->dl_tti_request_body.nGroup = 0;

  if (retval != 0) {
    LOG_E(PHY, "%s() Problem sending retval:%d\n", __FUNCTION__, retval);
  }
  return retval;
}

int oai_fapi_send_end_request(int cell, uint32_t frame, uint32_t slot){
  nfapi_vnf_p7_config_t *p7_config = get_p7_vnf_config();
  nfapi_nr_slot_indication_scf_t nr_slot_resp = {.header.message_id = 0x8F, .sfn = frame, .slot = slot};
  int retval = p7_config->send_p7_msg(get_p7_vnf(), &nr_slot_resp.header);
  if (retval != 0) {
    LOG_E(PHY, "%s() Problem sending retval:%d\n", __FUNCTION__, retval);
  }
  return retval;
}

