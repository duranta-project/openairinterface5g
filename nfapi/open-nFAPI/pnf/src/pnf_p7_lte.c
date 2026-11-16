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

extern int sf_ahead;

void add_sf(uint16_t* frameP, uint16_t* subframeP, int offset)
{
  *frameP = (*frameP + ((*subframeP + offset) / 10)) % 1024;

  *subframeP = ((*subframeP + offset) % 10);
}

void subtract_sf(uint16_t* frameP, uint16_t* subframeP, int offset)
{
  if (*subframeP < offset) {
    *frameP = (*frameP + 1024 - 1) % 1024;
  }
  *subframeP = (*subframeP + 10 - offset) % 10;
}

uint16_t sfnsf_add_sf(uint16_t sfnsf, int offset)
{
  uint16_t new_sfnsf;
  uint16_t sfn = NFAPI_SFNSF2SFN(sfnsf);
  uint16_t sf = NFAPI_SFNSF2SF(sfnsf);

  add_sf(&sfn, &sf, offset);

  new_sfnsf = sfn << 4 | sf;

  return new_sfnsf;
}

uint16_t sfnsf_subtract_sf(uint16_t sfnsf, int offset)
{
  uint16_t new_sfnsf;
  uint16_t sfn = NFAPI_SFNSF2SFN(sfnsf);
  uint16_t sf = NFAPI_SFNSF2SF(sfnsf);

  subtract_sf(&sfn, &sf, offset);

  new_sfnsf = sfn << 4 | sf;

  return new_sfnsf;
}

static uint32_t get_sf_time(uint32_t now_hr, uint32_t sf_start_hr)
{
  if (now_hr < sf_start_hr) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "now is earlier than start of subframe\n");
    return 0;
  } else {
    uint32_t now_us = TIMEHR_USEC(now_hr);
    uint32_t sf_start_us = TIMEHR_USEC(sf_start_hr);

    // if the us have wrapped adjust for it
    if (now_hr < sf_start_us) {
      now_us += 1000000;
    }

    return now_us - sf_start_us;
  }
}

nfapi_dl_config_request_t* allocate_nfapi_dl_config_request(pnf_p7_t* pnf_p7)
{
  void* ptr = pnf_p7_malloc(pnf_p7, sizeof(nfapi_dl_config_request_t));
  return ptr;
}

void deallocate_nfapi_dl_config_request(nfapi_dl_config_request_t* req, pnf_p7_t* pnf_p7)
{
  if (pnf_p7->_public.codec_config.deallocate) {
    (pnf_p7->_public.codec_config.deallocate)(req->dl_config_request_body.dl_config_pdu_list);
  } else {
    free(req->dl_config_request_body.dl_config_pdu_list);
  }
  req->dl_config_request_body.dl_config_pdu_list = 0;

  pnf_p7_free(pnf_p7, req);
}

nfapi_ul_config_request_t* allocate_nfapi_ul_config_request(pnf_p7_t* pnf_p7)
{
  void* ptr = pnf_p7_malloc(pnf_p7, sizeof(nfapi_ul_config_request_t));
  return ptr;
}

void deallocate_nfapi_ul_config_request(nfapi_ul_config_request_t* req, pnf_p7_t* pnf_p7)
{
  if (pnf_p7->_public.codec_config.deallocate) {
    (pnf_p7->_public.codec_config.deallocate)(req->ul_config_request_body.ul_config_pdu_list);
  } else {
    free(req->ul_config_request_body.ul_config_pdu_list);
  }
  req->ul_config_request_body.ul_config_pdu_list = 0;

  pnf_p7_free(pnf_p7, req);
}

nfapi_hi_dci0_request_t* allocate_nfapi_hi_dci0_request(pnf_p7_t* pnf_p7)
{
  return pnf_p7_malloc(pnf_p7, sizeof(nfapi_hi_dci0_request_t));
}

void deallocate_nfapi_hi_dci0_request(nfapi_hi_dci0_request_t* req, pnf_p7_t* pnf_p7)
{
  if (pnf_p7->_public.codec_config.deallocate) {
    (pnf_p7->_public.codec_config.deallocate)(req->hi_dci0_request_body.hi_dci0_pdu_list);
  } else {
    free(req->hi_dci0_request_body.hi_dci0_pdu_list);
  }
  req->hi_dci0_request_body.hi_dci0_pdu_list = 0;

  pnf_p7_free(pnf_p7, req);
}

nfapi_tx_request_t* allocate_nfapi_tx_request(pnf_p7_t* pnf_p7)
{
  return pnf_p7_malloc(pnf_p7, sizeof(nfapi_tx_request_t));
}

void deallocate_nfapi_tx_request(nfapi_tx_request_t* req, pnf_p7_t* pnf_p7)
{
  int i = 0;

  for (i = 0; i < req->tx_request_body.number_of_pdus; ++i) {
    void* data = req->tx_request_body.tx_pdu_list[i].segments[0].segment_data;

    if (pnf_p7->_public.codec_config.deallocate) {
      (pnf_p7->_public.codec_config.deallocate)(data);
    } else {
      free(data);
    }
    data = 0;
  }

  if (pnf_p7->_public.codec_config.deallocate) {
    (pnf_p7->_public.codec_config.deallocate)(req->tx_request_body.tx_pdu_list);
  } else {
    free(req->tx_request_body.tx_pdu_list);
  }
  req->tx_request_body.tx_pdu_list = 0;

  pnf_p7_free(pnf_p7, req);
}

nfapi_lbt_dl_config_request_t* allocate_nfapi_lbt_dl_config_request(pnf_p7_t* pnf_p7)
{
  return pnf_p7_malloc(pnf_p7, sizeof(nfapi_lbt_dl_config_request_t));
}

void deallocate_nfapi_lbt_dl_config_request(nfapi_lbt_dl_config_request_t* req, pnf_p7_t* pnf_p7)
{
  if (pnf_p7->_public.codec_config.deallocate) {
    (pnf_p7->_public.codec_config.deallocate)(req->lbt_dl_config_request_body.lbt_dl_config_req_pdu_list);
  } else {
    free(req->lbt_dl_config_request_body.lbt_dl_config_req_pdu_list);
  }
  req->lbt_dl_config_request_body.lbt_dl_config_req_pdu_list = 0;

  pnf_p7_free(pnf_p7, req);
}

nfapi_ue_release_request_t* allocate_nfapi_ue_release_request(pnf_p7_t* pnf_p7)
{
  return pnf_p7_malloc(pnf_p7, sizeof(nfapi_ue_release_request_t));
}

void deallocate_nfapi_ue_release_request(nfapi_ue_release_request_t* req, pnf_p7_t* pnf_p7)
{
  pnf_p7_free(pnf_p7, req);
}

void pnf_pack_and_send_timing_info(pnf_p7_t* pnf_p7)
{
  nfapi_timing_info_t timing_info;
  memset(&timing_info, 0, sizeof(timing_info));
  timing_info.header.message_id = NFAPI_TIMING_INFO;
  timing_info.header.phy_id = pnf_p7->_public.phy_id;

  timing_info.last_sfn_sf = pnf_p7->sfn_sf;
  timing_info.time_since_last_timing_info = pnf_p7->timing_info_ms_counter;

  timing_info.dl_config_jitter = pnf_p7->dl_config_jitter;
  timing_info.tx_request_jitter = pnf_p7->tx_jitter;
  timing_info.ul_config_jitter = pnf_p7->ul_config_jitter;
  timing_info.hi_dci0_jitter = pnf_p7->hi_dci0_jitter;

  timing_info.dl_config_latest_delay = 0;
  timing_info.tx_request_latest_delay = 0;
  timing_info.ul_config_latest_delay = 0;
  timing_info.hi_dci0_latest_delay = 0;

  timing_info.dl_config_earliest_arrival = 0;
  timing_info.tx_request_earliest_arrival = 0;
  timing_info.ul_config_earliest_arrival = 0;
  timing_info.hi_dci0_earliest_arrival = 0;

  pnf_p7_pack_and_send_p7_message(pnf_p7, &(timing_info.header), sizeof(timing_info));

  pnf_p7->timing_info_ms_counter = 0;
}

void send_dummy_subframe(pnf_p7_t* pnf_p7, uint16_t sfn_sf)
{
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);

  if (pnf_p7->_public.tx_req && pnf_p7->_public.dummy_subframe.tx_req) {
    pnf_p7->_public.dummy_subframe.tx_req->sfn_sf = sfn_sf;
    (pnf_p7->_public.tx_req)(&pnf_p7->_public, pnf_p7->_public.dummy_subframe.tx_req);
  }
  if (pnf_p7->_public.dl_config_req && pnf_p7->_public.dummy_subframe.dl_config_req) {
    pnf_p7->_public.dummy_subframe.dl_config_req->sfn_sf = sfn_sf;
    (pnf_p7->_public.dl_config_req)(&(pnf_p7->_public), pnf_p7->_public.dummy_subframe.dl_config_req);
  }
  if (pnf_p7->_public.ul_config_req && pnf_p7->_public.dummy_subframe.ul_config_req) {
    pnf_p7->_public.dummy_subframe.ul_config_req->sfn_sf = sfn_sf;
    NFAPI_TRACE(NFAPI_TRACE_INFO, "Dummy ul_config_req - enter\n");
    (pnf_p7->_public.ul_config_req)(&pnf_p7->_public, pnf_p7->_public.dummy_subframe.ul_config_req);
  }
  if (pnf_p7->_public.hi_dci0_req && pnf_p7->_public.dummy_subframe.hi_dci0_req) {
    pnf_p7->_public.dummy_subframe.hi_dci0_req->sfn_sf = sfn_sf;
    NFAPI_TRACE(NFAPI_TRACE_INFO, "Dummy hi_dci0 - enter\n");
    (pnf_p7->_public.hi_dci0_req)(&pnf_p7->_public, pnf_p7->_public.dummy_subframe.hi_dci0_req);
  }
  if (pnf_p7->_public.lbt_dl_config_req && pnf_p7->_public.dummy_subframe.lbt_dl_config_req) {
    pnf_p7->_public.dummy_subframe.lbt_dl_config_req->sfn_sf = sfn_sf;
    NFAPI_TRACE(NFAPI_TRACE_INFO, "Dummy lbt - enter\n");
    (pnf_p7->_public.lbt_dl_config_req)(&pnf_p7->_public, pnf_p7->_public.dummy_subframe.lbt_dl_config_req);
  }
}

int pnf_p7_subframe_ind(pnf_p7_t* pnf_p7, uint16_t phy_id, uint16_t sfn_sf)
{
  UNUSED(phy_id)
  // We could either send an event to the p7 thread have have it run the
  // subframe or we could handle it here and lock access to the subframe
  // buffers. If we do it on the p7 thread then we run the risk of blocking
  // on the udp send.
  //
  // todo : start a timer to give us more of the 1 ms tick before send back
  // the frame

  // todo : consider a more efficent lock mechasium
  if (pthread_mutex_lock(&(pnf_p7->mutex)) != 0) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to lock mutex\n");
    return -1;
  }

#if 1
  pnf_p7->sf_start_time_hr = pnf_get_current_time_hr();
  pnf_p7->sfn_sf = sfn_sf;

  uint32_t sfn_sf_tx = sfnsf_add_sf(sfn_sf, sf_ahead);
  uint32_t tx_sfn_sf_dec = NFAPI_SFNSF2DEC(sfn_sf_tx);

  if (pnf_p7->_public.subframe_buffer_size != 0) {
    if (pnf_p7->sfn_sf_shift != 0) {
      int32_t sfn_sf_dec = NFAPI_SFNSF2DEC(sfn_sf);

      int32_t shifted_sfn_sf = sfn_sf_dec += pnf_p7->sfn_sf_shift;

      if (shifted_sfn_sf < 0)
        shifted_sfn_sf += NFAPI_MAX_SFNSFDEC;
      else if (shifted_sfn_sf > NFAPI_MAX_SFNSFDEC)
        shifted_sfn_sf -= NFAPI_MAX_SFNSFDEC;

      NFAPI_TRACE(NFAPI_TRACE_INFO,
                  "Applying shift %d to sfn/sf (%d -> %d)\n",
                  pnf_p7->sfn_sf_shift,
                  NFAPI_SFNSF2DEC(sfn_sf),
                  shifted_sfn_sf);
      sfn_sf = shifted_sfn_sf;

      pnf_p7->sfn_sf_shift = 0;
    }

    uint32_t sfn_sf_dec = NFAPI_SFNSF2DEC(sfn_sf);
    uint8_t buffer_index = sfn_sf_dec % pnf_p7->_public.subframe_buffer_size;

    nfapi_pnf_p7_subframe_buffer_t* subframe_buffer = &(pnf_p7->subframe_buffer[buffer_index]);

    uint8_t tx_buffer_index = tx_sfn_sf_dec % pnf_p7->_public.subframe_buffer_size;
    nfapi_pnf_p7_subframe_buffer_t* tx_subframe_buffer = &(pnf_p7->subframe_buffer[tx_buffer_index]);

    if (tx_subframe_buffer->sfn_sf == sfn_sf_tx) {
      if (tx_subframe_buffer->tx_req != 0) {
        if (pnf_p7->_public.tx_req)
          (pnf_p7->_public.tx_req)(&(pnf_p7->_public), tx_subframe_buffer->tx_req);
      } else {
        if (pnf_p7->_public.tx_req && pnf_p7->_public.dummy_subframe.tx_req) {
          pnf_p7->_public.dummy_subframe.tx_req->sfn_sf = sfn_sf_tx;
          (pnf_p7->_public.tx_req)(&(pnf_p7->_public), pnf_p7->_public.dummy_subframe.tx_req);
        }
      }

      if (tx_subframe_buffer->dl_config_req != 0) {
        if (pnf_p7->_public.dl_config_req)
          (pnf_p7->_public.dl_config_req)(&(pnf_p7->_public), tx_subframe_buffer->dl_config_req);
      } else {
        if (pnf_p7->_public.dl_config_req && pnf_p7->_public.dummy_subframe.dl_config_req) {
          pnf_p7->_public.dummy_subframe.dl_config_req->sfn_sf = sfn_sf_tx;
          (pnf_p7->_public.dl_config_req)(&(pnf_p7->_public), pnf_p7->_public.dummy_subframe.dl_config_req);
        }
      }

      if (tx_subframe_buffer->hi_dci0_req != 0) {
        if (pnf_p7->_public.hi_dci0_req)
          (pnf_p7->_public.hi_dci0_req)(&(pnf_p7->_public), tx_subframe_buffer->hi_dci0_req);
      } else {
        if (pnf_p7->_public.hi_dci0_req && pnf_p7->_public.dummy_subframe.hi_dci0_req) {
          pnf_p7->_public.dummy_subframe.hi_dci0_req->sfn_sf = sfn_sf_tx;
          (pnf_p7->_public.hi_dci0_req)(&(pnf_p7->_public), pnf_p7->_public.dummy_subframe.hi_dci0_req);
        }
      }

      if (tx_subframe_buffer->ue_release_req != 0) {
        if (pnf_p7->_public.ue_release_req)
          (pnf_p7->_public.ue_release_req)(&(pnf_p7->_public), tx_subframe_buffer->ue_release_req);
      } else {
        if (pnf_p7->_public.ue_release_req && pnf_p7->_public.dummy_subframe.ue_release_req) {
          pnf_p7->_public.dummy_subframe.ue_release_req->sfn_sf = sfn_sf_tx;
          (pnf_p7->_public.ue_release_req)(&(pnf_p7->_public), pnf_p7->_public.dummy_subframe.ue_release_req);
        }
      }

      if (tx_subframe_buffer->dl_config_req != 0) {
        deallocate_nfapi_dl_config_request(tx_subframe_buffer->dl_config_req, pnf_p7);
        tx_subframe_buffer->dl_config_req = 0;
      }
      if (tx_subframe_buffer->tx_req != 0) {
        deallocate_nfapi_tx_request(tx_subframe_buffer->tx_req, pnf_p7);
        tx_subframe_buffer->tx_req = 0;
      }
      if (tx_subframe_buffer->hi_dci0_req != 0) {
        deallocate_nfapi_hi_dci0_request(tx_subframe_buffer->hi_dci0_req, pnf_p7);
        tx_subframe_buffer->hi_dci0_req = 0;
      }
      if (tx_subframe_buffer->ue_release_req != 0) {
        deallocate_nfapi_ue_release_request(tx_subframe_buffer->ue_release_req, pnf_p7);
        tx_subframe_buffer->ue_release_req = 0;
      }
    } else {
      send_dummy_subframe(pnf_p7, sfn_sf_tx);
    }

    if (subframe_buffer->sfn_sf == sfn_sf) {
      if (subframe_buffer->ul_config_req != 0) {
        if (pnf_p7->_public.ul_config_req)
          (pnf_p7->_public.ul_config_req)(&(pnf_p7->_public), subframe_buffer->ul_config_req);
      } else {
        if (pnf_p7->_public.ul_config_req && pnf_p7->_public.dummy_subframe.ul_config_req) {
          pnf_p7->_public.dummy_subframe.ul_config_req->sfn_sf = sfn_sf;
          (pnf_p7->_public.ul_config_req)(&(pnf_p7->_public), pnf_p7->_public.dummy_subframe.ul_config_req);
        }
      }

      if (subframe_buffer->lbt_dl_config_req != 0) {
        if (pnf_p7->_public.lbt_dl_config_req)
          (pnf_p7->_public.lbt_dl_config_req)(&(pnf_p7->_public), subframe_buffer->lbt_dl_config_req);
      } else {
        if (pnf_p7->_public.lbt_dl_config_req && pnf_p7->_public.dummy_subframe.lbt_dl_config_req) {
          pnf_p7->_public.dummy_subframe.lbt_dl_config_req->sfn_sf = sfn_sf;
          (pnf_p7->_public.lbt_dl_config_req)(&(pnf_p7->_public), pnf_p7->_public.dummy_subframe.lbt_dl_config_req);
        }
      }

      if (subframe_buffer->ul_config_req != 0) {
        deallocate_nfapi_ul_config_request(subframe_buffer->ul_config_req, pnf_p7);
        subframe_buffer->ul_config_req = 0;
      }
      if (subframe_buffer->lbt_dl_config_req != 0) {
        deallocate_nfapi_lbt_dl_config_request(subframe_buffer->lbt_dl_config_req, pnf_p7);
        subframe_buffer->lbt_dl_config_req = 0;
      }
    } // sfn_sf match

    if (subframe_buffer->dl_config_req == 0 && subframe_buffer->tx_req == 0 && subframe_buffer->ul_config_req == 0
        && subframe_buffer->lbt_dl_config_req == 0 && subframe_buffer->ue_release_req == 0) {
      memset(&(pnf_p7->subframe_buffer[buffer_index]), 0, sizeof(nfapi_pnf_p7_subframe_buffer_t));
      pnf_p7->subframe_buffer[buffer_index].sfn_sf = -1;
    }

    if (pnf_p7->_public.timing_info_mode_periodic && (pnf_p7->timing_info_period_counter++) == pnf_p7->_public.timing_info_period) {
      pnf_pack_and_send_timing_info(pnf_p7);

      pnf_p7->timing_info_period_counter = 0;
    } else if (pnf_p7->_public.timing_info_mode_aperiodic && pnf_p7->timing_info_aperiodic_send) {
      pnf_pack_and_send_timing_info(pnf_p7);

      pnf_p7->timing_info_aperiodic_send = 0;
    } else {
      pnf_p7->timing_info_ms_counter++;
    }
  } else {
    // subframe_buffer_size == 0
  }

  if (pnf_p7->tick == 1000) {
    NFAPI_TRACE(NFAPI_TRACE_INFO,
                "[PNF P7:%d] (ONTIME/LATE) DL:(%d/%d) UL:(%d/%d) HI:(%d/%d) TX:(%d/%d)\n",
                pnf_p7->_public.phy_id,
                pnf_p7->stats.dl_conf_ontime,
                pnf_p7->stats.dl_conf_late,
                pnf_p7->stats.ul_conf_ontime,
                pnf_p7->stats.ul_conf_late,
                pnf_p7->stats.hi_dci0_ontime,
                pnf_p7->stats.hi_dci0_late,
                pnf_p7->stats.tx_ontime,
                pnf_p7->stats.tx_late);
    pnf_p7->tick = 0;
    memset(&pnf_p7->stats, 0, sizeof(pnf_p7->stats));
  }
  pnf_p7->tick++;
#endif

  if (pthread_mutex_unlock(&(pnf_p7->mutex)) != 0) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to unlock mutex\n");
    return -1;
  }

  return 0;
}

uint8_t is_p7_request_in_window(uint16_t sfnsf, const char* name, pnf_p7_t* phy)
{
  uint32_t recv_sfn_sf_dec = NFAPI_SFNSF2DEC(sfnsf);
  uint32_t current_sfn_sf_dec = NFAPI_SFNSF2DEC(phy->sfn_sf);

  uint8_t in_window = 0;
  uint8_t timing_window = phy->_public.subframe_buffer_size;

  if (recv_sfn_sf_dec <= current_sfn_sf_dec) {
    if (((current_sfn_sf_dec + timing_window) % NFAPI_MAX_SFNSFDEC) < current_sfn_sf_dec) {
      if (recv_sfn_sf_dec > ((current_sfn_sf_dec + timing_window) % NFAPI_MAX_SFNSFDEC)) {
        NFAPI_TRACE(NFAPI_TRACE_NOTE, "[%d] %s is late %d (with wrap)\n", current_sfn_sf_dec, name, recv_sfn_sf_dec);
      } else {
        in_window = 1;
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_NOTE,
                  "[%d] %s is in late %d (delta:%d)\n",
                  current_sfn_sf_dec,
                  name,
                  recv_sfn_sf_dec,
                  (current_sfn_sf_dec - recv_sfn_sf_dec));
    }

  } else {
    if ((recv_sfn_sf_dec - current_sfn_sf_dec) <= timing_window) {
      in_window = 1;
    } else {
      NFAPI_TRACE(NFAPI_TRACE_NOTE,
                  "[%d] %s is out of window %d (delta:%d) [max:%d]\n",
                  current_sfn_sf_dec,
                  name,
                  recv_sfn_sf_dec,
                  (recv_sfn_sf_dec - current_sfn_sf_dec),
                  timing_window);
    }
  }

  return in_window;
}

void pnf_handle_dl_config_request(void* pRecvMsg, int recvMsgLen, pnf_p7_t* pnf_p7)
{
  nfapi_dl_config_request_t* req = allocate_nfapi_dl_config_request(pnf_p7);

  if (req == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to allocate nfapi_dl_config_request structure\n");
    return;
  }

  int unpack_result =
      nfapi_p7_message_unpack(pRecvMsg, recvMsgLen, req, sizeof(nfapi_dl_config_request_t), &(pnf_p7->_public.codec_config));

  if (unpack_result == 0) {
    if (pthread_mutex_lock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to lock mutex\n");
      return;
    }

    if (is_p7_request_in_window(req->sfn_sf, "dl_config_request", pnf_p7)) {
      uint32_t sfn_sf_dec = NFAPI_SFNSF2DEC(req->sfn_sf);
      uint8_t buffer_index = sfn_sf_dec % pnf_p7->_public.subframe_buffer_size;

      struct timespec t;
      clock_gettime(CLOCK_MONOTONIC, &t);

      NFAPI_TRACE(NFAPI_TRACE_INFO,
                  "%s() %ld.%09ld POPULATE DL_CONFIG_REQ sfn_sf:%d buffer_index:%d\n",
                  __FUNCTION__,
                  t.tv_sec,
                  t.tv_nsec,
                  sfn_sf_dec,
                  buffer_index);

      if (pnf_p7->subframe_buffer[buffer_index].dl_config_req != 0) {
        NFAPI_TRACE(NFAPI_TRACE_NOTE,
                    "%s() is_p7_request_in_window()=TRUE buffer_index occupied - free it first sfn_sf:%d buffer_index:%d\n",
                    __FUNCTION__,
                    NFAPI_SFNSF2DEC(req->sfn_sf),
                    buffer_index);
        deallocate_nfapi_dl_config_request(pnf_p7->subframe_buffer[buffer_index].dl_config_req, pnf_p7);
      }

      pnf_p7->subframe_buffer[buffer_index].sfn_sf = req->sfn_sf;
      pnf_p7->subframe_buffer[buffer_index].dl_config_req = req;

      pnf_p7->stats.dl_conf_ontime++;

    } else {
      deallocate_nfapi_dl_config_request(req, pnf_p7);

      if (pnf_p7->_public.timing_info_mode_aperiodic) {
        pnf_p7->timing_info_aperiodic_send = 1;
      }

      pnf_p7->stats.dl_conf_late++;
    }

    if (pthread_mutex_unlock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to unlock mutex\n");
      return;
    }
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Failed to unpack dl_config_req");
    deallocate_nfapi_dl_config_request(req, pnf_p7);
  }
}

void pnf_handle_ul_config_request(void* pRecvMsg, int recvMsgLen, pnf_p7_t* pnf_p7)
{
  nfapi_ul_config_request_t* req = allocate_nfapi_ul_config_request(pnf_p7);

  if (req == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to allocate nfapi_ul_config_request structure\n");
    return;
  }

  int unpack_result =
      nfapi_p7_message_unpack(pRecvMsg, recvMsgLen, req, sizeof(nfapi_ul_config_request_t), &(pnf_p7->_public.codec_config));

  if (unpack_result == 0) {
    if (pthread_mutex_lock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to lock mutex\n");
      return;
    }

    if (is_p7_request_in_window(req->sfn_sf, "ul_config_request", pnf_p7)) {
      uint32_t sfn_sf_dec = NFAPI_SFNSF2DEC(req->sfn_sf);
      uint8_t buffer_index = sfn_sf_dec % pnf_p7->_public.subframe_buffer_size;

      struct timespec t;
      clock_gettime(CLOCK_MONOTONIC, &t);

      NFAPI_TRACE(NFAPI_TRACE_INFO,
                  "%s() %ld.%09ld POPULATE UL_CONFIG_REQ sfn_sf:%d buffer_index:%d\n",
                  __FUNCTION__,
                  t.tv_sec,
                  t.tv_nsec,
                  sfn_sf_dec,
                  buffer_index);

      if (pnf_p7->subframe_buffer[buffer_index].ul_config_req != 0) {
        deallocate_nfapi_ul_config_request(pnf_p7->subframe_buffer[buffer_index].ul_config_req, pnf_p7);
      }

      pnf_p7->subframe_buffer[buffer_index].sfn_sf = req->sfn_sf;
      pnf_p7->subframe_buffer[buffer_index].ul_config_req = req;

      pnf_p7->stats.ul_conf_ontime++;
    } else {
      NFAPI_TRACE(NFAPI_TRACE_NOTE,
                  "[%d] NOT storing ul_config_req OUTSIDE OF TRANSMIT BUFFER WINDOW SFN/SF %d\n",
                  NFAPI_SFNSF2DEC(pnf_p7->sfn_sf),
                  NFAPI_SFNSF2DEC(req->sfn_sf));
      deallocate_nfapi_ul_config_request(req, pnf_p7);

      if (pnf_p7->_public.timing_info_mode_aperiodic) {
        pnf_p7->timing_info_aperiodic_send = 1;
      }

      pnf_p7->stats.ul_conf_late++;
    }

    if (pthread_mutex_unlock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to unlock mutex\n");
      return;
    }
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Failed to unpack ul_config_req\n");
    deallocate_nfapi_ul_config_request(req, pnf_p7);
  }
}

void pnf_handle_hi_dci0_request(void* pRecvMsg, int recvMsgLen, pnf_p7_t* pnf_p7)
{
  nfapi_hi_dci0_request_t* req = allocate_nfapi_hi_dci0_request(pnf_p7);

  if (req == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to allocate nfapi_hi_dci0_request structure\n");
    return;
  }

  int unpack_result =
      nfapi_p7_message_unpack(pRecvMsg, recvMsgLen, req, sizeof(nfapi_hi_dci0_request_t), &pnf_p7->_public.codec_config);

  if (unpack_result == 0) {
    if (pthread_mutex_lock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to lock mutex\n");
      return;
    }

    if (is_p7_request_in_window(req->sfn_sf, "hi_dci0_request", pnf_p7)) {
      uint32_t sfn_sf_dec = NFAPI_SFNSF2DEC(req->sfn_sf);
      uint8_t buffer_index = sfn_sf_dec % pnf_p7->_public.subframe_buffer_size;

      if (pnf_p7->subframe_buffer[buffer_index].hi_dci0_req != 0) {
        deallocate_nfapi_hi_dci0_request(pnf_p7->subframe_buffer[buffer_index].hi_dci0_req, pnf_p7);
      }

      pnf_p7->subframe_buffer[buffer_index].sfn_sf = req->sfn_sf;
      pnf_p7->subframe_buffer[buffer_index].hi_dci0_req = req;

      pnf_p7->stats.hi_dci0_ontime++;

    } else {
      deallocate_nfapi_hi_dci0_request(req, pnf_p7);

      if (pnf_p7->_public.timing_info_mode_aperiodic) {
        pnf_p7->timing_info_aperiodic_send = 1;
      }

      pnf_p7->stats.hi_dci0_late++;
    }

    if (pthread_mutex_unlock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to unlock mutex\n");
      return;
    }
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Failed to unpack hi_dci0_req\n");
    deallocate_nfapi_hi_dci0_request(req, pnf_p7);
  }
}

void pnf_handle_tx_request(void* pRecvMsg, int recvMsgLen, pnf_p7_t* pnf_p7)
{
  nfapi_tx_request_t* req = allocate_nfapi_tx_request(pnf_p7);

  if (req == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to allocate nfapi_tx_request structure\n");
    return;
  }

  int unpack_result = nfapi_p7_message_unpack(pRecvMsg, recvMsgLen, req, sizeof(nfapi_tx_request_t), &pnf_p7->_public.codec_config);
  if (unpack_result == 0) {
    if (pthread_mutex_lock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to lock mutex\n");
      return;
    }

    if (is_p7_request_in_window(req->sfn_sf, "tx_request", pnf_p7)) {
      uint32_t sfn_sf_dec = NFAPI_SFNSF2DEC(req->sfn_sf);
      uint8_t buffer_index = sfn_sf_dec % pnf_p7->_public.subframe_buffer_size;

      struct timespec t;
      clock_gettime(CLOCK_MONOTONIC, &t);

      NFAPI_TRACE(NFAPI_TRACE_INFO,
                  "%s() %ld.%09ld POPULATE TX_REQ sfn_sf:%d buffer_index:%d\n",
                  __FUNCTION__,
                  t.tv_sec,
                  t.tv_nsec,
                  sfn_sf_dec,
                  buffer_index);

      if (pnf_p7->subframe_buffer[buffer_index].tx_req != 0) {
        deallocate_nfapi_tx_request(pnf_p7->subframe_buffer[buffer_index].tx_req, pnf_p7);
      }

      pnf_p7->subframe_buffer[buffer_index].sfn_sf = req->sfn_sf;
      pnf_p7->subframe_buffer[buffer_index].tx_req = req;

      pnf_p7->stats.tx_ontime++;
    } else {
      NFAPI_TRACE(NFAPI_TRACE_INFO,
                  "%s() TX_REQUEST Request is outside of window REQ:SFN_SF:%d CURR:SFN_SF:%d\n",
                  __FUNCTION__,
                  NFAPI_SFNSF2DEC(req->sfn_sf),
                  NFAPI_SFNSF2DEC(pnf_p7->sfn_sf));

      deallocate_nfapi_tx_request(req, pnf_p7);

      if (pnf_p7->_public.timing_info_mode_aperiodic) {
        pnf_p7->timing_info_aperiodic_send = 1;
      }

      pnf_p7->stats.tx_late++;
    }

    if (pthread_mutex_unlock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to unlock mutex\n");
      return;
    }
  } else {
    deallocate_nfapi_tx_request(req, pnf_p7);
  }
}

void pnf_handle_lbt_dl_config_request(void* pRecvMsg, int recvMsgLen, pnf_p7_t* pnf_p7)
{
  nfapi_lbt_dl_config_request_t* req = allocate_nfapi_lbt_dl_config_request(pnf_p7);

  if (req == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to allocate nfapi_lbt_dl_config_request structure\n");
    return;
  }

  int unpack_result =
      nfapi_p7_message_unpack(pRecvMsg, recvMsgLen, req, sizeof(nfapi_lbt_dl_config_request_t), &pnf_p7->_public.codec_config);

  if (unpack_result == 0) {
    if (pthread_mutex_lock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to lock mutex\n");
      return;
    }

    if (is_p7_request_in_window(req->sfn_sf, "lbt_dl_request", pnf_p7)) {
      uint32_t sfn_sf_dec = NFAPI_SFNSF2DEC(req->sfn_sf);
      uint8_t buffer_index = sfn_sf_dec % pnf_p7->_public.subframe_buffer_size;

      if (pnf_p7->subframe_buffer[buffer_index].lbt_dl_config_req != 0) {
        deallocate_nfapi_lbt_dl_config_request(pnf_p7->subframe_buffer[buffer_index].lbt_dl_config_req, pnf_p7);
      }

      pnf_p7->subframe_buffer[buffer_index].sfn_sf = req->sfn_sf;
      pnf_p7->subframe_buffer[buffer_index].lbt_dl_config_req = req;
    } else {
      deallocate_nfapi_lbt_dl_config_request(req, pnf_p7);

      if (pnf_p7->_public.timing_info_mode_aperiodic) {
        pnf_p7->timing_info_aperiodic_send = 1;
      }
    }

    if (pthread_mutex_unlock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to unlock mutex\n");
      return;
    }
  } else {
    deallocate_nfapi_lbt_dl_config_request(req, pnf_p7);
  }
}

void pnf_handle_ue_release_request(void* pRecvMsg, int recvMsgLen, pnf_p7_t* pnf_p7)
{
  nfapi_ue_release_request_t* req = allocate_nfapi_ue_release_request(pnf_p7);
  if (req == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "failed to allocate nfapi_ue_release_request structure\n");
    return;
  }

  int unpack_result =
      nfapi_p7_message_unpack(pRecvMsg, recvMsgLen, req, sizeof(nfapi_ue_release_request_t), &pnf_p7->_public.codec_config);
  if (unpack_result == 0) {
    if (pthread_mutex_lock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "failed to lock mutex\n");
      return;
    }

    if (is_p7_request_in_window(req->sfn_sf, "ue_release_request", pnf_p7)) {
      uint32_t sfn_sf_dec = NFAPI_SFNSF2DEC(req->sfn_sf);
      uint8_t buffer_index = sfn_sf_dec % pnf_p7->_public.subframe_buffer_size;

      struct timespec t;
      clock_gettime(CLOCK_MONOTONIC, &t);

      NFAPI_TRACE(NFAPI_TRACE_INFO,
                  "%s() %ld.%09ld POPULATE UE_RELEASE_REQ sfn_sf:%d buffer_index:%d\n",
                  __FUNCTION__,
                  t.tv_sec,
                  t.tv_nsec,
                  sfn_sf_dec,
                  buffer_index);

      if (pnf_p7->subframe_buffer[buffer_index].ue_release_req != 0) {
        deallocate_nfapi_ue_release_request(pnf_p7->subframe_buffer[buffer_index].ue_release_req, pnf_p7);
      }

      pnf_p7->subframe_buffer[buffer_index].sfn_sf = req->sfn_sf;
      pnf_p7->subframe_buffer[buffer_index].ue_release_req = req;

      pnf_p7->stats.tx_ontime++;
    } else {
      NFAPI_TRACE(NFAPI_TRACE_INFO,
                  "%s() UE_RELEASE_REQUEST Request is outside of window REQ:SFN_SF:%d CURR:SFN_SF:%d\n",
                  __FUNCTION__,
                  NFAPI_SFNSF2DEC(req->sfn_sf),
                  NFAPI_SFNSF2DEC(pnf_p7->sfn_sf));

      deallocate_nfapi_ue_release_request(req, pnf_p7);
      if (pnf_p7->_public.timing_info_mode_aperiodic) {
        pnf_p7->timing_info_aperiodic_send = 1;
      }

      pnf_p7->stats.tx_late++;
    }
    nfapi_ue_release_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.header.message_id = NFAPI_UE_RELEASE_RESPONSE;
    resp.header.phy_id = req->header.phy_id;
    resp.error_code = NFAPI_MSG_OK;
    nfapi_pnf_ue_release_resp(&(pnf_p7->_public), &resp);
    NFAPI_TRACE(NFAPI_TRACE_INFO, "do ue_release_response\n");

    if (pthread_mutex_unlock(&(pnf_p7->mutex)) != 0) {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "failed to unlock mutex\n");
      return;
    }
  } else {
    deallocate_nfapi_ue_release_request(req, pnf_p7);
  }
}

uint32_t calculate_t2(uint32_t now_time_hr, uint16_t sfn_sf, uint32_t sf_start_time_hr)
{
  uint32_t sf_time_us = get_sf_time(now_time_hr, sf_start_time_hr);
  uint32_t t2 = (NFAPI_SFNSF2DEC(sfn_sf) * 1000) + sf_time_us;
  return t2;
}

uint32_t calculate_t3(uint16_t sfn_sf, uint32_t sf_start_time_hr)
{
  uint32_t now_time_hr = pnf_get_current_time_hr();

  uint32_t sf_time_us = get_sf_time(now_time_hr, sf_start_time_hr);

  uint32_t t3 = (NFAPI_SFNSF2DEC(sfn_sf) * 1000) + sf_time_us;

  return t3;
}

void pnf_handle_dl_node_sync(void* pRecvMsg, int recvMsgLen, pnf_p7_t* pnf_p7, uint32_t rx_hr_time)
{
  nfapi_dl_node_sync_t dl_node_sync;

  if (pRecvMsg == NULL || pnf_p7 == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return;
  }

  if (nfapi_p7_message_unpack(pRecvMsg, recvMsgLen, &dl_node_sync, sizeof(dl_node_sync), &pnf_p7->_public.codec_config) < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    return;
  }

  if (pthread_mutex_lock(&(pnf_p7->mutex)) != 0) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to lock mutex\n");
    return;
  }

  if (dl_node_sync.delta_sfn_sf != 0) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "Will shift SF timing by %d on next subframe\n", dl_node_sync.delta_sfn_sf);

    pnf_p7->sfn_sf_shift = dl_node_sync.delta_sfn_sf;
  }

  nfapi_ul_node_sync_t ul_node_sync;
  memset(&ul_node_sync, 0, sizeof(ul_node_sync));
  ul_node_sync.header.message_id = NFAPI_UL_NODE_SYNC;
  ul_node_sync.header.phy_id = dl_node_sync.header.phy_id;
  ul_node_sync.t1 = dl_node_sync.t1;
  ul_node_sync.t2 = calculate_t2(rx_hr_time, pnf_p7->sfn_sf, pnf_p7->sf_start_time_hr);
  ul_node_sync.t3 = calculate_t3(pnf_p7->sfn_sf, pnf_p7->sf_start_time_hr);

  if (pthread_mutex_unlock(&(pnf_p7->mutex)) != 0) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to unlock mutex\n");
    return;
  }

  pnf_p7_pack_and_send_p7_message(pnf_p7, &(ul_node_sync.header), sizeof(ul_node_sync));
}

void pnf_dispatch_p7_message(void* pRecvMsg, int recvMsgLen, pnf_p7_t* pnf_p7, uint32_t rx_hr_time)
{
  nfapi_p7_message_header_t header;

  if (pRecvMsg == NULL || recvMsgLen < 4 || pnf_p7 == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return;
  }

  if (nfapi_p7_message_header_unpack(pRecvMsg, recvMsgLen, &header, sizeof(header), &pnf_p7->_public.codec_config) < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Unpack message header failed, ignoring\n");
    return;
  }

  if (recvMsgLen < 8 || pRecvMsg == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_WARN, "Invalid message size: %d, ignoring\n", recvMsgLen);
    return;
  }

  switch (header.message_id) {
    case NFAPI_DL_NODE_SYNC:
      pnf_handle_dl_node_sync(pRecvMsg, recvMsgLen, pnf_p7, rx_hr_time);
      break;
    case NFAPI_DL_CONFIG_REQUEST:
      pnf_handle_dl_config_request(pRecvMsg, recvMsgLen, pnf_p7);
      break;

    case NFAPI_UL_CONFIG_REQUEST:
      pnf_handle_ul_config_request(pRecvMsg, recvMsgLen, pnf_p7);
      break;

    case NFAPI_HI_DCI0_REQUEST:
      pnf_handle_hi_dci0_request(pRecvMsg, recvMsgLen, pnf_p7);
      break;

    case NFAPI_TX_REQUEST:
      pnf_handle_tx_request(pRecvMsg, recvMsgLen, pnf_p7);
      break;

    case NFAPI_LBT_DL_CONFIG_REQUEST:
      pnf_handle_lbt_dl_config_request(pRecvMsg, recvMsgLen, pnf_p7);
      break;

    case NFAPI_UE_RELEASE_REQUEST:
      pnf_handle_ue_release_request(pRecvMsg, recvMsgLen, pnf_p7);
      break;

    default: {
      if (header.message_id >= NFAPI_VENDOR_EXT_MSG_MIN && header.message_id <= NFAPI_VENDOR_EXT_MSG_MAX) {
        pnf_handle_p7_vendor_extension(pRecvMsg, recvMsgLen, pnf_p7, header.message_id);
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s P7 Unknown message ID %d\n", __FUNCTION__, header.message_id);
      }
    } break;
  }
}

void pnf_handle_p7_message(void* pRecvMsg, int recvMsgLen, pnf_p7_t* pnf_p7, uint32_t rx_hr_time)
{
  nfapi_p7_message_header_t messageHeader;

  if (pRecvMsg == NULL || recvMsgLen < 4 || pnf_p7 == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "pnf_handle_p7_message: invalid input params (%p %d %p)\n", pRecvMsg, recvMsgLen, pnf_p7);
    return;
  }

  if (nfapi_p7_message_header_unpack(pRecvMsg,
                                     recvMsgLen,
                                     &messageHeader,
                                     sizeof(nfapi_p7_message_header_t),
                                     &pnf_p7->_public.codec_config)
      < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Unpack message header failed, ignoring\n");
    return;
  }

  uint8_t m = NFAPI_P7_GET_MORE(messageHeader.m_segment_sequence);
  uint8_t sequence_num = NFAPI_P7_GET_SEQUENCE(messageHeader.m_segment_sequence);
  uint8_t segment_num = NFAPI_P7_GET_SEGMENT(messageHeader.m_segment_sequence);

  if (pnf_p7->_public.checksum_enabled) {
    uint32_t checksum = nfapi_p7_calculate_checksum(pRecvMsg, recvMsgLen);
    if (checksum != messageHeader.checksum) {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "Checksum verification failed %d %d\n", checksum, messageHeader.checksum);
      return;
    }
  }

  if (m == 0 && segment_num == 0) {
    if (recvMsgLen < 8 || pRecvMsg == NULL) {
      NFAPI_TRACE(NFAPI_TRACE_WARN, "Invalid message size: %d, ignoring\n", recvMsgLen);
      return;
    }

    pnf_dispatch_p7_message(pRecvMsg, recvMsgLen, pnf_p7, rx_hr_time);
  } else {
    pnf_p7_rx_message_t* rx_msg = pnf_p7_rx_reassembly_queue_add_segment(pnf_p7,
                                                                         &(pnf_p7->reassembly_queue),
                                                                         rx_hr_time,
                                                                         sequence_num,
                                                                         segment_num,
                                                                         m,
                                                                         pRecvMsg,
                                                                         recvMsgLen);

    if (rx_msg->num_segments_received == rx_msg->num_segments_expected) {
      uint16_t i = 0;
      uint16_t length = 0;
      for (i = 0; i < rx_msg->num_segments_expected; ++i) {
        length += rx_msg->segments[i].length - (i > 0 ? NFAPI_P7_HEADER_LENGTH : 0);
      }

      if (pnf_p7->reassemby_buffer_size < length) {
        pnf_p7_free(pnf_p7, pnf_p7->reassemby_buffer);
        pnf_p7->reassemby_buffer = 0;
      }

      if (pnf_p7->reassemby_buffer == 0) {
        NFAPI_TRACE(NFAPI_TRACE_NOTE, "Resizing PNF_P7 Reassembly buffer %d->%d\n", pnf_p7->reassemby_buffer_size, length);
        pnf_p7->reassemby_buffer = (uint8_t*)pnf_p7_malloc(pnf_p7, length);

        if (pnf_p7->reassemby_buffer == 0) {
          NFAPI_TRACE(NFAPI_TRACE_NOTE, "Failed to allocate PNF_P7 reassemby buffer len:%d\n", length);
          return;
        }
        memset(pnf_p7->reassemby_buffer, 0, length);
        pnf_p7->reassemby_buffer_size = length;
      }

      uint16_t offset = 0;
      for (i = 0; i < rx_msg->num_segments_expected; ++i) {
        if (i == 0) {
          memcpy(pnf_p7->reassemby_buffer, rx_msg->segments[i].buffer, rx_msg->segments[i].length);
          offset += rx_msg->segments[i].length;
        } else {
          memcpy(pnf_p7->reassemby_buffer + offset,
                 rx_msg->segments[i].buffer + NFAPI_P7_HEADER_LENGTH,
                 rx_msg->segments[i].length - NFAPI_P7_HEADER_LENGTH);
          offset += rx_msg->segments[i].length - NFAPI_P7_HEADER_LENGTH;
        }
      }

      pnf_dispatch_p7_message(pnf_p7->reassemby_buffer, length, pnf_p7, rx_msg->rx_hr_time);

      pnf_p7_rx_reassembly_queue_remove_msg(pnf_p7, &(pnf_p7->reassembly_queue), rx_msg);
    }
  }

  pnf_p7_rx_reassembly_queue_remove_old_msgs(pnf_p7, &(pnf_p7->reassembly_queue), rx_hr_time, 1000);
}

void pnf_nfapi_p7_read_dispatch_message(pnf_p7_t* pnf_p7, uint32_t now_hr_time)
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
                               (struct sockaddr*)&remote_addr,
                               &remote_addr_size);

    if (recvfrom_result > 0) {
      nfapi_p7_message_header_t header;
      nfapi_p7_message_header_unpack(header_buffer, NFAPI_P7_HEADER_LENGTH, &header, 34, 0);

      if (header.message_length > pnf_p7->rx_message_buffer_size) {
        NFAPI_TRACE(NFAPI_TRACE_NOTE, "reallocing rx buffer %d\n", header.message_length);
        pnf_p7->rx_message_buffer = realloc(pnf_p7->rx_message_buffer, header.message_length);
        pnf_p7->rx_message_buffer_size = header.message_length;
      }

      recvfrom_result = recvfrom(pnf_p7->p7_sock,
                                 pnf_p7->rx_message_buffer,
                                 pnf_p7->rx_message_buffer_size,
                                 MSG_DONTWAIT | MSG_TRUNC,
                                 (struct sockaddr*)&remote_addr,
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
        pnf_handle_p7_message(pnf_p7->rx_message_buffer, recvfrom_result, pnf_p7, now_hr_time);
      }
    } else if (recvfrom_result == 0) {
      recvfrom_result =
          recvfrom(pnf_p7->p7_sock, header_buffer, 0, MSG_DONTWAIT, (struct sockaddr*)&remote_addr, &remote_addr_size);
    }

    if (recvfrom_result == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // no-op
      } else {
        NFAPI_TRACE(NFAPI_TRACE_WARN, "%s recvfrom failed errno:%d\n", __FUNCTION__, errno);
      }
    }

  } while (recvfrom_result > 0);
}

int pnf_p7_message_pump(pnf_p7_t* pnf_p7)
{
  if (pthread_mutex_init(&(pnf_p7->mutex), NULL) != 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "After P7 mutex init: %d\n", errno);
    return -1;
  }

  if (pthread_mutex_init(&(pnf_p7->pack_mutex), NULL) != 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "After P7 mutex init: %d\n", errno);
    return -1;
  }

  if ((pnf_p7->p7_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "After P7 socket errno: %d\n", errno);
    return -1;
  }
  NFAPI_TRACE(NFAPI_TRACE_INFO, "PNF P7 socket created (%d)...\n", pnf_p7->p7_sock);

  int reuseaddr_enable = 1;
  if (setsockopt(pnf_p7->p7_sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_enable, sizeof(int)) < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "PNF P7 setsockopt (SOL_SOCKET, SO_REUSEADDR) failed  errno: %d\n", errno);
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
  addr.sin_port = htons(pnf_p7->_public.local_p7_port);

  if (pnf_p7->_public.local_p7_addr == 0) {
    addr.sin_addr.s_addr = INADDR_ANY;
  } else {
    if (inet_aton(pnf_p7->_public.local_p7_addr, &addr.sin_addr) == -1) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "inet_aton failed\n");
    }
  }

  NFAPI_TRACE(NFAPI_TRACE_INFO, "PNF P7 binding %d too %s:%d\n", pnf_p7->p7_sock, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
  if (bind(pnf_p7->p7_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "PNF_P7 bind error fd:%d errno: %d\n", pnf_p7->p7_sock, errno);
    return -1;
  }
  NFAPI_TRACE(NFAPI_TRACE_INFO, "PNF P7 bind succeeded...\n");

  while (pnf_p7->terminate == 0) {
    fd_set rfds;
    int selectRetval = 0;

    FD_ZERO(&rfds);
    FD_SET(pnf_p7->p7_sock, &rfds);

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    selectRetval = select(pnf_p7->p7_sock + 1, &rfds, NULL, NULL, &timeout);

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

    if (FD_ISSET(pnf_p7->p7_sock, &rfds)) {
      pnf_nfapi_p7_read_dispatch_message(pnf_p7, now_hr_time);
    }
  }
  NFAPI_TRACE(NFAPI_TRACE_ERROR, "PNF_P7 Terminating..\n");

  if (close(pnf_p7->p7_sock) < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "close failed errno: %d\n", errno);
  }

  if (pthread_mutex_destroy(&(pnf_p7->pack_mutex)) != 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "mutex destroy failed errno: %d\n", errno);
  }

  if (pthread_mutex_destroy(&(pnf_p7->mutex)) != 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "mutex destroy failed errno: %d\n", errno);
  }

  return 0;
}
