/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "pnf_p7.h"
#include "nr_fapi_p7_utils.h"
#include "common/ran_context.h"
#include <SCHED_NR/phy_frame_config_nr.h>

static void add_slot(int mu, uint16_t* frameP, uint16_t* slotP, int offset)
{
  uint16_t num_slots = NFAPI_SLOTNUM(mu);

  *frameP = (*frameP + ((*slotP + offset) / num_slots)) % 1024;

  *slotP = ((*slotP + offset) % num_slots);
}

void sfnslot_add_slot(int mu, uint16_t* sfn, uint16_t* slot, int offset)
{
  add_slot(mu, sfn, slot, offset);
}

static uint32_t get_slot_time(uint32_t now_hr, uint32_t slot_start_hr)
{
  if (now_hr < slot_start_hr) {
    return 0;
  } else {
    uint32_t now_us = TIMEHR_USEC(now_hr);
    uint32_t slot_start_us = TIMEHR_USEC(slot_start_hr);

    // if the us have wrapped adjust for it
    if (now_hr < slot_start_us) {
      now_us += 500000;
    }

    return now_us - slot_start_us;
  }
}

void pnf_nr_pack_and_send_timing_info(pnf_p7_t* pnf_p7)
{
  nfapi_nr_timing_info_t timing_info;
  memset(&timing_info, 0, sizeof(timing_info));
  timing_info.header.message_id = NFAPI_TIMING_INFO;
  timing_info.header.phy_id = pnf_p7->_nr_public.phy_id;

  timing_info.last_sfn = pnf_p7->sfn;
  timing_info.last_slot = pnf_p7->slot;
  timing_info.time_since_last_timing_info = pnf_p7->timing_info_ms_counter;

  timing_info.dl_tti_jitter = pnf_p7->dl_tti_jitter;
  timing_info.tx_data_request_jitter = pnf_p7->tx_data_jitter;
  timing_info.ul_tti_jitter = pnf_p7->ul_tti_jitter;
  timing_info.ul_dci_jitter = pnf_p7->ul_dci_jitter;

  timing_info.dl_tti_latest_delay = 0;
  timing_info.tx_data_request_latest_delay = 0;
  timing_info.ul_tti_latest_delay = 0;
  timing_info.ul_dci_latest_delay = 0;

  timing_info.dl_tti_earliest_arrival = 0;
  timing_info.tx_data_request_earliest_arrival = 0;
  timing_info.ul_tti_earliest_arrival = 0;
  timing_info.ul_dci_earliest_arrival = 0;
  AssertFatal(pnf_p7->_nr_public.send_p7_msg, "The function pointer to pack and send P7 messages must be set");
  pnf_p7->_nr_public.send_p7_msg(pnf_p7, &(timing_info.header), sizeof(timing_info));

  pnf_p7->timing_info_ms_counter = 0;
}

int nr_pnf_p7_get_msgs(pnf_p7_t* pnf_p7,
                       uint16_t phy_id,
                       uint16_t sfn,
                       uint16_t slot,
                       nfapi_nr_dl_tti_request_t* ret_dl_tti,
                       nfapi_nr_ul_tti_request_t* ret_ul_tti,
                       nfapi_nr_ul_dci_request_t* ret_ul_dci,
                       nfapi_nr_tx_data_request_t* ret_tx_data)
{
  UNUSED(phy_id)
  if (pthread_mutex_lock(&(pnf_p7->mutex)) != 0) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to lock mutex\n");
    return -1;
  }

  pnf_p7->slot_start_time_hr = pnf_get_current_time_hr();

  // We align the pnf_p7 sfn/slot with tx sfn/slot, and vnf is synced with pnf_p7 sfn/slot. This is so that the scheduler runs
  // slot_ahead from rx thread.
  pnf_p7->sfn = sfn;
  pnf_p7->slot = slot;

  uint32_t tx_slot_dec = NFAPI_SFNSLOT2DEC(pnf_p7->mu, sfn, slot);
  uint8_t buffer_index_tx = tx_slot_dec % NFAPI_SLOTNUM(pnf_p7->mu);

  if (pnf_p7->_nr_public.slot_buffer_size != 0) {
    if (pnf_p7->slot_shift != 0) {
      uint16_t shifted_slot = slot + pnf_p7->slot_shift;

      if (shifted_slot < 0)
        shifted_slot += NFAPI_MAX_SFNSLOTDEC(pnf_p7->mu);
      else if (shifted_slot > NFAPI_MAX_SFNSLOTDEC(pnf_p7->mu))
        shifted_slot -= NFAPI_MAX_SFNSLOTDEC(pnf_p7->mu);

      slot = shifted_slot;

      pnf_p7->slot_shift = 0;
    }

    nfapi_pnf_p7_slot_buffer_t* tx_slot_buffer = &(pnf_p7->slot_buffer[buffer_index_tx]);

    ret_dl_tti->SFN = sfn;
    ret_dl_tti->Slot = slot;
    ret_dl_tti->dl_tti_request_body.nPDUs = 0;
    nfapi_nr_dl_tti_request_t* dl_tti_req = &tx_slot_buffer->dl_tti_req;
    if (dl_tti_req->SFN == sfn && dl_tti_req->Slot == slot) {
      copy_dl_tti_request(dl_tti_req, ret_dl_tti);
      tx_slot_buffer->dl_tti_req.SFN = -1;
      tx_slot_buffer->dl_tti_req.Slot = -1;
    }

    ret_tx_data->SFN = sfn;
    ret_tx_data->Slot = slot;
    ret_tx_data->Number_of_PDUs = 0;
    nfapi_nr_tx_data_request_t* txd = &tx_slot_buffer->tx_data_req;
    if (txd->SFN == sfn && txd->Slot == slot) {
      copy_tx_data_request(txd, ret_tx_data);
      tx_slot_buffer->tx_data_req.SFN = -1;
      tx_slot_buffer->tx_data_req.Slot = -1;
    }

    ret_ul_tti->SFN = sfn;
    ret_ul_tti->Slot = slot;
    ret_ul_tti->n_pdus = 0;
    if (tx_slot_buffer->ul_tti_req.SFN == sfn && tx_slot_buffer->ul_tti_req.Slot == slot) {
      copy_ul_tti_request(&tx_slot_buffer->ul_tti_req, ret_ul_tti);
      tx_slot_buffer->ul_tti_req.SFN = -1;
      tx_slot_buffer->ul_tti_req.Slot = -1;
    }

    ret_ul_dci->SFN = sfn;
    ret_ul_dci->Slot = slot;
    ret_ul_dci->numPdus = 0;
    if (tx_slot_buffer->ul_dci_req.SFN == sfn && tx_slot_buffer->ul_dci_req.Slot == slot) {
      copy_ul_dci_request(&tx_slot_buffer->ul_dci_req, ret_ul_dci);
      tx_slot_buffer->ul_dci_req.SFN = -1;
      tx_slot_buffer->ul_dci_req.Slot = -1;
    }

    if (pnf_p7->_nr_public.timing_info_mode_periodic
        && (pnf_p7->timing_info_period_counter++) == pnf_p7->_nr_public.timing_info_period) {
      pnf_nr_pack_and_send_timing_info(pnf_p7);

      pnf_p7->timing_info_period_counter = 0;
    } else if (pnf_p7->_nr_public.timing_info_mode_aperiodic && pnf_p7->timing_info_aperiodic_send) {
      pnf_nr_pack_and_send_timing_info(pnf_p7);

      pnf_p7->timing_info_aperiodic_send = 0;
    } else {
      pnf_p7->timing_info_ms_counter++;
    }
  }

  if (sfn % 128 == 0 && slot == 0) {
    pnf_p7_nr_stats_t* s = &pnf_p7->nr_stats;
    int ontime = s->dl_tti.ontime + s->ul_tti.ontime + s->ul_dci.ontime + s->tx_data.ontime;
    int late = s->dl_tti.late + s->ul_tti.late + s->ul_dci.late + s->tx_data.late;

    float dl_thr = (float)s->dl.bytes * 8 / 1000 / 1000 / 1.28f;
    float ul_thr = (float)s->ul.bytes * 8 / 1000 / 1000 / 1.28f;
    NFAPI_TRACE(NFAPI_TRACE_INFO,
                "[P7:%d] msgs ontime %d thr DL %.2f UL %.2f msg late %d (vtime)\n",
                pnf_p7->_nr_public.phy_id,
                ontime,
                dl_thr,
                ul_thr,
                late);
    memset(s, 0, sizeof(*s));
  }

  if (pthread_mutex_unlock(&(pnf_p7->mutex)) != 0) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to unlock mutex\n");
    return -1;
  }

  return 0;
}

bool is_nr_p7_request_in_window(const uint16_t sfn, const uint16_t slot, const char* name, const pnf_p7_t* phy)
{
  const uint32_t recv = NFAPI_SFNSLOT2DEC(phy->mu, sfn, slot);
  const uint32_t curr = NFAPI_SFNSLOT2DEC(phy->mu, phy->sfn, phy->slot);
  const uint8_t timing_window = phy->_nr_public.slot_buffer_size;
  uint32_t diff = curr < recv ? recv - curr : curr - recv;
  if (diff > NFAPI_MAX_SFNSLOTDEC(phy->mu) / 2)
    diff = NFAPI_MAX_SFNSLOTDEC(phy->mu) - diff;
  if (diff > timing_window) {
    NFAPI_TRACE(NFAPI_TRACE_WARN, "[%d] %s is out of window %d (delta:%d) [max:%d]\n", curr, name, recv, diff, timing_window);
    return false;
  }
  return true;
}

bool check_nr_nfapi_p7_slot_type(const uint16_t sfn, const uint16_t slot, const char* name, int type)
{
  DevAssert(RC.gNB != NULL && RC.gNB[0] != NULL);
  PHY_VARS_gNB* gNB = RC.gNB[0];
  nfapi_nr_config_request_scf_t* cfg = &gNB->gNB_config;
  const int slot_type = nr_slot_select(cfg, sfn, slot);
  if (slot_type != type && slot_type != NR_MIXED_SLOT) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%4d.%2d Slot type is not appropriate for %s\n", sfn, slot, name);
    return false;
  }
  return true;
}

void pnf_handle_dl_tti_request(void* pRecvMsg, int recvMsgLen, pnf_p7_t* pnf_p7)
{
  uint16_t frame, slot;
  if (peek_nr_nfapi_p7_sfn_slot(pRecvMsg, recvMsgLen, &frame, &slot)) {
    if (pthread_mutex_lock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to lock mutex\n");
      return;
    }
    if (check_nr_nfapi_p7_slot_type(frame, slot, "DL_TTI.request", NR_DOWNLINK_SLOT)
        && is_nr_p7_request_in_window(frame, slot, "dl_tti_request", pnf_p7)) {
      uint32_t sfn_slot_dec = NFAPI_SFNSLOT2DEC(pnf_p7->mu, frame, slot);
      uint8_t buffer_index = sfn_slot_dec % NFAPI_SLOTNUM(pnf_p7->mu);
      pnf_p7->slot_buffer[buffer_index].sfn = frame;
      pnf_p7->slot_buffer[buffer_index].slot = slot;
      nfapi_nr_dl_tti_request_t* req = &pnf_p7->slot_buffer[buffer_index].dl_tti_req;
      pnf_p7->nr_stats.dl_tti.ontime++;

      NFAPI_TRACE(NFAPI_TRACE_DEBUG,
                  "POPULATE DL_TTI_REQ current tx sfn/slot:%d.%d p7 msg sfn/slot: %d.%d buffer_index:%d\n",
                  pnf_p7->sfn,
                  pnf_p7->slot,
                  frame,
                  slot,
                  buffer_index);
      const bool result =
          pnf_p7->_nr_public.unpack_func(pRecvMsg, recvMsgLen, req, sizeof(*req), &(pnf_p7->_nr_public.codec_config));
      if (!result)
        NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to unpack request\n");
    } else {
      if (pnf_p7->_nr_public.timing_info_mode_aperiodic)
        pnf_p7->timing_info_aperiodic_send = 1;

      pnf_p7->nr_stats.dl_tti.late++;
    }
    if (pthread_mutex_unlock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to unlock mutex\n");
      return;
    }
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Failed to unpack dl_tti_req");
  }
}

void pnf_handle_ul_tti_request(void* pRecvMsg, int recvMsgLen, pnf_p7_t* pnf_p7)
{
  uint16_t frame, slot;
  if (peek_nr_nfapi_p7_sfn_slot(pRecvMsg, recvMsgLen, &frame, &slot)) {
    if (pthread_mutex_lock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to lock mutex\n");
      return;
    }

    if (check_nr_nfapi_p7_slot_type(frame, slot, "UL_TTI.request", NR_UPLINK_SLOT)
        && is_nr_p7_request_in_window(frame, slot, "ul_tti_request", pnf_p7)) {
      uint32_t sfn_slot_dec = NFAPI_SFNSLOT2DEC(pnf_p7->mu, frame, slot);
      uint8_t buffer_index = sfn_slot_dec % NFAPI_SLOTNUM(pnf_p7->mu);
      pnf_p7->slot_buffer[buffer_index].sfn = frame;
      pnf_p7->slot_buffer[buffer_index].slot = slot;
      nfapi_nr_ul_tti_request_t* req = &pnf_p7->slot_buffer[buffer_index].ul_tti_req;
      pnf_p7->nr_stats.ul_tti.ontime++;

      NFAPI_TRACE(NFAPI_TRACE_DEBUG,
                  "POPULATE UL_TTI.request current tx sfn/slot:%d.%d p7 msg sfn/slot: %d.%d buffer_index:%d\n",
                  pnf_p7->sfn,
                  pnf_p7->slot,
                  frame,
                  slot,
                  buffer_index);
      const bool result =
          pnf_p7->_nr_public.unpack_func(pRecvMsg, recvMsgLen, req, sizeof(*req), &(pnf_p7->_nr_public.codec_config));
      if (!result)
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "failed to unpack UL_TTI.request\n");
    } else {
      NFAPI_TRACE(NFAPI_TRACE_NOTE,
                  "[%d.%d] NOT storing ul_tti_req OUTSIDE OF TRANSMIT BUFFER WINDOW SFN/SLOT %d.%d\n",
                  pnf_p7->sfn,
                  pnf_p7->slot,
                  frame,
                  slot);
      if (pnf_p7->_nr_public.timing_info_mode_aperiodic)
        pnf_p7->timing_info_aperiodic_send = 1;

      pnf_p7->nr_stats.ul_tti.late++;
    }

    if (pthread_mutex_unlock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to unlock mutex\n");
      return;
    }
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Failed to unpack ul_tti_req\n");
  }
}

void pnf_handle_ul_dci_request(void* pRecvMsg, int recvMsgLen, pnf_p7_t* pnf_p7)
{
  uint16_t frame, slot;
  if (peek_nr_nfapi_p7_sfn_slot(pRecvMsg, recvMsgLen, &frame, &slot)) {
    if (pthread_mutex_lock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to lock mutex\n");
      return;
    }
    if (check_nr_nfapi_p7_slot_type(frame, slot, "UL_DCI.request", NR_DOWNLINK_SLOT)
        && is_nr_p7_request_in_window(frame, slot, "ul_dci_request", pnf_p7)) {
      uint32_t sfn_slot_dec = NFAPI_SFNSLOT2DEC(pnf_p7->mu, frame, slot);
      uint8_t buffer_index = sfn_slot_dec % NFAPI_SLOTNUM(pnf_p7->mu);
      pnf_p7->slot_buffer[buffer_index].sfn = frame;
      pnf_p7->slot_buffer[buffer_index].slot = slot;
      nfapi_nr_ul_dci_request_t* req = &pnf_p7->slot_buffer[buffer_index].ul_dci_req;
      pnf_p7->nr_stats.ul_dci.ontime++;

      NFAPI_TRACE(NFAPI_TRACE_DEBUG,
                  "POPULATE UL_DCI.request current tx sfn/slot:%d.%d p7 msg sfn/slot: %d.%d buffer_index:%d\n",
                  pnf_p7->sfn,
                  pnf_p7->slot,
                  frame,
                  slot,
                  buffer_index);
      const bool result =
          pnf_p7->_nr_public.unpack_func(pRecvMsg, recvMsgLen, req, sizeof(*req), &(pnf_p7->_nr_public.codec_config));
      if (!result)
        NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to unpack request\n");
    } else {
      if (pnf_p7->_nr_public.timing_info_mode_aperiodic) {
        pnf_p7->timing_info_aperiodic_send = 1;
      }

      pnf_p7->nr_stats.ul_dci.late++;
    }

    if (pthread_mutex_unlock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to unlock mutex\n");
      return;
    }
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Failed to unpack UL DCI req\n");
  }
}

void pnf_handle_tx_data_request(void* pRecvMsg, int recvMsgLen, pnf_p7_t* pnf_p7)
{
  uint16_t frame, slot;
  if (peek_nr_nfapi_p7_sfn_slot(pRecvMsg, recvMsgLen, &frame, &slot)) {
    if (pthread_mutex_lock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to lock mutex\n");
      return;
    }
    if (check_nr_nfapi_p7_slot_type(frame, slot, "TX_DATA.REQUEST", NR_DOWNLINK_SLOT)
        && is_nr_p7_request_in_window(frame, slot, "tx_request", pnf_p7)) {
      uint32_t sfn_slot_dec = NFAPI_SFNSLOT2DEC(pnf_p7->mu, frame, slot);
      uint8_t buffer_index = sfn_slot_dec % NFAPI_SLOTNUM(pnf_p7->mu);
      pnf_p7->slot_buffer[buffer_index].sfn = frame;
      pnf_p7->slot_buffer[buffer_index].slot = slot;
      nfapi_nr_tx_data_request_t* req = &pnf_p7->slot_buffer[buffer_index].tx_data_req;
      pnf_p7->nr_stats.tx_data.ontime++;

      NFAPI_TRACE(NFAPI_TRACE_DEBUG,
                  "POPULATE TX_data.request current tx sfn/slot:%d.%d p7 msg sfn/slot: %d.%d buffer_index:%d\n",
                  pnf_p7->sfn,
                  pnf_p7->slot,
                  frame,
                  slot,
                  buffer_index);
      const bool result =
          pnf_p7->_nr_public.unpack_func(pRecvMsg, recvMsgLen, req, sizeof(*req), &(pnf_p7->_nr_public.codec_config));
      if (result) {
        for (int i = 0; i < req->Number_of_PDUs; ++i)
          pnf_p7->nr_stats.dl.bytes += req->pdu_list[i].PDU_length;
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "failed to unpack TX_data.request\n");
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_INFO,
                  "TX_DATA_REQUEST Request is outside of window REQ:SFN_SLOT:%d.%d CURR:SFN_SLOT:%d.%d\n",
                  frame,
                  slot,
                  pnf_p7->sfn,
                  pnf_p7->slot);

      if (pnf_p7->_nr_public.timing_info_mode_aperiodic) {
        pnf_p7->timing_info_aperiodic_send = 1;
      }

      pnf_p7->nr_stats.tx_data.late++;
    }

    if (pthread_mutex_unlock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to unlock mutex\n");
      return;
    }
  }
}

static uint32_t calculate_nr_t2(uint32_t now_time_hr, int mu, uint16_t sfn, uint16_t slot, uint32_t slot_start_time_hr)
{
  uint32_t slot_time_us = get_slot_time(now_time_hr, slot_start_time_hr);
  uint32_t t2 = NFAPI_SFNSLOT2DEC(mu, sfn, slot) * NFAPI_SLOTLEN(mu) + slot_time_us;
  return t2;
}

static uint32_t calculate_nr_t3(int mu, uint16_t sfn, uint16_t slot, uint32_t slot_start_time_hr)
{
  uint32_t now_time_hr = pnf_get_current_time_hr();

  uint32_t slot_time_us = get_slot_time(now_time_hr, slot_start_time_hr);

  uint32_t t3 = NFAPI_SFNSLOT2DEC(mu, sfn, slot) * NFAPI_SLOTLEN(mu) + slot_time_us;

  return t3;
}

void pnf_nr_handle_dl_node_sync(void* pRecvMsg, int recvMsgLen, pnf_p7_t* pnf_p7, uint32_t rx_hr_time)
{
  nfapi_nr_dl_node_sync_t dl_node_sync;

  if (pRecvMsg == NULL || pnf_p7 == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return;
  }

  const bool result =
      pnf_p7->_nr_public.unpack_func(pRecvMsg, recvMsgLen, &dl_node_sync, sizeof(dl_node_sync), &pnf_p7->_nr_public.codec_config);
  if (!result) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    return;
  }

  if (pthread_mutex_lock(&(pnf_p7->mutex)) != 0) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to lock mutex\n");
    return;
  }

  if (dl_node_sync.delta_sfn_slot != 0) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "Will shift Slot timing by %d on next slot\n", dl_node_sync.delta_sfn_slot);

    pnf_p7->slot_shift = dl_node_sync.delta_sfn_slot;
  }

  nfapi_nr_ul_node_sync_t ul_node_sync;
  memset(&ul_node_sync, 0, sizeof(ul_node_sync));
  ul_node_sync.header.message_id = NFAPI_NR_PHY_MSG_TYPE_UL_NODE_SYNC;
  ul_node_sync.header.phy_id = dl_node_sync.header.phy_id;
  ul_node_sync.t1 = dl_node_sync.t1;
  ul_node_sync.t2 = calculate_nr_t2(rx_hr_time, pnf_p7->mu, pnf_p7->sfn, pnf_p7->slot, pnf_p7->slot_start_time_hr);
  ul_node_sync.t3 = calculate_nr_t3(pnf_p7->mu, pnf_p7->sfn, pnf_p7->slot, pnf_p7->slot_start_time_hr);

  if (pthread_mutex_unlock(&(pnf_p7->mutex)) != 0) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to unlock mutex\n");
    return;
  }
  AssertFatal(pnf_p7->_nr_public.send_p7_msg, "Function pointer must be configured|");
  pnf_p7->_nr_public.send_p7_msg(pnf_p7, &(ul_node_sync.header), sizeof(ul_node_sync));
}

void pnf_nr_handle_p7_message(void* pRecvMsg, int recvMsgLen, pnf_p7_t* pnf_p7, uint32_t rx_hr_time)
{
  nfapi_nr_p7_message_header_t header;

  if (pRecvMsg == NULL || recvMsgLen < 4 || pnf_p7 == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return;
  }

  const bool result =
      pnf_p7->_nr_public.hdr_unpack_func(pRecvMsg, recvMsgLen, &header, sizeof(header), &pnf_p7->_nr_public.codec_config);
  if (!result) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Unpack message header failed, ignoring\n");
    return;
  }

  if (recvMsgLen < 8 || pRecvMsg == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_WARN, "Invalid message size: %d, ignoring\n", recvMsgLen);
    return;
  }

  switch (header.message_id) {
    case NFAPI_NR_PHY_MSG_TYPE_DL_NODE_SYNC:
      pnf_nr_handle_dl_node_sync(pRecvMsg, recvMsgLen, pnf_p7, rx_hr_time);
      break;
    case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST:
      pnf_handle_dl_tti_request(pRecvMsg, recvMsgLen, pnf_p7);
      break;
    case NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST:
      pnf_handle_ul_tti_request(pRecvMsg, recvMsgLen, pnf_p7);
      break;
    case NFAPI_NR_PHY_MSG_TYPE_UL_DCI_REQUEST:
      pnf_handle_ul_dci_request(pRecvMsg, recvMsgLen, pnf_p7);
      break;
    case NFAPI_NR_PHY_MSG_TYPE_TX_DATA_REQUEST:
      pnf_handle_tx_data_request(pRecvMsg, recvMsgLen, pnf_p7);
      break;
    default: {
      if (header.message_id >= NFAPI_VENDOR_EXT_MSG_MIN && header.message_id <= NFAPI_VENDOR_EXT_MSG_MAX) {
        pnf_nr_handle_p7_vendor_extension(pRecvMsg, recvMsgLen, pnf_p7, header.message_id);
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s P7 Unknown message ID %d\n", __FUNCTION__, header.message_id);
      }
    } break;
  }
}

void pnf_nr_handle_p7_vendor_extension(void* pRecvMsg, int recvMsgLen, pnf_p7_t* pnf_p7, uint16_t message_id)
{
  if (!pnf_p7->_nr_public.allocate_p7_vendor_ext)
    return;

  uint16_t msg_size;
  nfapi_p7_message_header_t* msg = pnf_p7->_nr_public.allocate_p7_vendor_ext(message_id, &msg_size);
  if (!msg) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to allocate vendor extension structure\n");
    return;
  }

  int unpack_result = nfapi_p7_message_unpack(pRecvMsg, recvMsgLen, msg, msg_size, &pnf_p7->_nr_public.codec_config);
  if (unpack_result == 0) {
    if (pnf_p7->_nr_public.vendor_ext)
      pnf_p7->_nr_public.vendor_ext(&pnf_p7->_nr_public, msg);
  }

  if (pnf_p7->_nr_public.deallocate_p7_vendor_ext)
    pnf_p7->_nr_public.deallocate_p7_vendor_ext(msg);
}

static void pnf_nr_nfapi_p7_read_dispatch_message(pnf_p7_t *pnf_p7, uint32_t now_hr_time)
{
  int recvfrom_result = 0;
  struct sockaddr_in remote_addr;
  socklen_t remote_addr_size = sizeof(remote_addr);

  do {
    uint8_t header_buffer[NFAPI_P7_HEADER_LENGTH];
    recvfrom_result = recvfrom(pnf_p7->p7_sock,
                               header_buffer,
                               NFAPI_P7_HEADER_LENGTH,
                               MSG_DONTWAIT | MSG_PEEK,
                               (struct sockaddr *)&remote_addr,
                               &remote_addr_size);

    if (recvfrom_result > 0) {
      nfapi_p7_message_header_t header;
      nfapi_p7_message_header_unpack(header_buffer, NFAPI_P7_HEADER_LENGTH, &header, 34, 0);

      if (header.message_length > pnf_p7->rx_message_buffer_size) {
        pnf_p7->rx_message_buffer = realloc(pnf_p7->rx_message_buffer, header.message_length);
        pnf_p7->rx_message_buffer_size = header.message_length;
      }

      recvfrom_result = recvfrom(pnf_p7->p7_sock,
                                 pnf_p7->rx_message_buffer,
                                 pnf_p7->rx_message_buffer_size,
                                 MSG_DONTWAIT | MSG_TRUNC,
                                 (struct sockaddr *)&remote_addr,
                                 &remote_addr_size);

      now_hr_time = pnf_get_current_time_hr();

      if (recvfrom_result > 0) {
        if (recvfrom_result != header.message_length) {
          NFAPI_TRACE(NFAPI_TRACE_ERROR,
                      "(%d) Received unexpected number of bytes. %d != %d",
                      __LINE__,
                      recvfrom_result,
                      header.message_length);
          break;
        }
        pnf_nr_handle_p7_message(pnf_p7->rx_message_buffer, recvfrom_result, pnf_p7, now_hr_time);
      }
    } else if (recvfrom_result == 0) {
      recvfrom_result =
          recvfrom(pnf_p7->p7_sock, header_buffer, 0, MSG_DONTWAIT, (struct sockaddr *)&remote_addr, &remote_addr_size);
    }

    if (recvfrom_result == -1) {
      if (errno != EAGAIN && errno != EWOULDBLOCK)
        NFAPI_TRACE(NFAPI_TRACE_WARN, "%s recvfrom failed errno:%d\n", __FUNCTION__, errno);
    }
  } while (recvfrom_result > 0);
}

int pnf_nr_p7_message_pump(pnf_p7_t *pnf_p7)
{
  if (pthread_mutex_init(&(pnf_p7->mutex), NULL) != 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "After P7 mutex init: %d\n", errno);
    return -1;
  }

  if (pthread_mutex_init(&(pnf_p7->pack_mutex), NULL) != 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "After P7 mutex init: %d\n", errno);
    pthread_mutex_destroy(&(pnf_p7->mutex));
    return -1;
  }

  if ((pnf_p7->p7_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "After P7 socket errno: %d\n", errno);
    return -1;
  }
  NFAPI_TRACE(NFAPI_TRACE_INFO, "PNF P7 socket created (%d)...\n", pnf_p7->p7_sock);

  int reuseaddr_enable = 1;
  if (setsockopt(pnf_p7->p7_sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_enable, sizeof(int)) < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "PNF P7 setsockopt (SOL_SOCKET, SO_REUSEADDR) failed errno: %d\n", errno);
    return -1;
  }

  int iptos_value = 0;
  if (setsockopt(pnf_p7->p7_sock, IPPROTO_IP, IP_TOS, &iptos_value, sizeof(iptos_value)) < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "PNF P7 setsockopt (IPPROTO_IP, IP_TOS) failed errno: %d\n", errno);
    return -1;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(pnf_p7->_nr_public.local_p7_port);

  if (pnf_p7->_nr_public.local_p7_addr == 0) {
    addr.sin_addr.s_addr = INADDR_ANY;
  } else {
    if (inet_aton(pnf_p7->_nr_public.local_p7_addr, &addr.sin_addr) == -1)
      NFAPI_TRACE(NFAPI_TRACE_INFO, "inet_aton failed\n");
  }

  NFAPI_TRACE(NFAPI_TRACE_INFO,
              "PNF P7 binding %d to %s:%d\n",
              pnf_p7->p7_sock,
              inet_ntoa(addr.sin_addr),
              ntohs(addr.sin_port));
  if (bind(pnf_p7->p7_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "PNF_P7 bind error fd:%d errno: %d\n", pnf_p7->p7_sock, errno);
    return -1;
  }
  NFAPI_TRACE(NFAPI_TRACE_INFO, "PNF P7 bind succeeded...\n");

  while (pnf_p7->terminate == 0) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(pnf_p7->p7_sock, &rfds);

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    int selectRetval = select(pnf_p7->p7_sock + 1, &rfds, NULL, NULL, &timeout);

    uint32_t now_hr_time = pnf_get_current_time_hr();

    if (selectRetval == 0) {
      continue;
    } else if (selectRetval == -1 && (errno == EINTR)) {
      NFAPI_TRACE(NFAPI_TRACE_WARN, "PNF P7 Signal Interrupt %d\n", errno);
      continue;
    } else if (selectRetval == -1) {
      NFAPI_TRACE(NFAPI_TRACE_WARN, "PNF P7 select() failed\n");
      sleep(1);
      continue;
    }

    if (FD_ISSET(pnf_p7->p7_sock, &rfds))
      pnf_nr_nfapi_p7_read_dispatch_message(pnf_p7, now_hr_time);
  }
  NFAPI_TRACE(NFAPI_TRACE_ERROR, "PNF_P7 Terminating..\n");

  if (close(pnf_p7->p7_sock) < 0)
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "close failed errno: %d\n", errno);

  if (pthread_mutex_destroy(&(pnf_p7->pack_mutex)) != 0)
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "mutex destroy failed errno: %d\n", errno);

  if (pthread_mutex_destroy(&(pnf_p7->mutex)) != 0)
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "mutex destroy failed errno: %d\n", errno);

  return 0;
}
