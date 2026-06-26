/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#include <time.h>
#include <stdio.h>
#include <string.h>

#include "vnf_p7_nr.h"
#include "nr_fapi_p7_utils.h"

#define SYNC_CYCLE_COUNT 2

uint32_t calculate_nr_t1(int mu, uint16_t sfn, uint16_t slot, uint32_t slot_start_time_hr)
{
	uint32_t now_time_hr = vnf_get_current_time_hr();

	uint32_t slot_time_us = get_slot_time(now_time_hr, slot_start_time_hr);

	uint32_t t1 = NFAPI_SFNSLOT2DEC(mu, sfn,slot) * NFAPI_SLOTLEN(mu) + slot_time_us;

	return t1;
}

uint32_t calculate_nr_t4(uint32_t now_time_hr, int mu, uint16_t sfn, uint16_t slot, uint32_t slot_start_time_hr)
{
	uint32_t slot_time_us = get_slot_time(now_time_hr, slot_start_time_hr);

	uint32_t t4 = NFAPI_SFNSLOT2DEC(mu, sfn,slot) * NFAPI_SLOTLEN(mu) + slot_time_us;

	return t4;

}

int send_mac_slot_indications(vnf_p7_t* vnf_p7)
{
	nfapi_vnf_p7_connection_info_t* curr = vnf_p7->p7_connections;
	while(curr != 0)
	{
		if(curr->in_sync == 1)
		{
			vnf_p7->_public.slot_indication(&(vnf_p7->_public), curr->phy_id, curr->sfn,curr->slot);
		}

		curr = curr->next;
	}

	return 0;
}

int vnf_nr_build_send_dl_node_sync(vnf_p7_t* vnf_p7, nfapi_vnf_p7_connection_info_t* p7_info)
{
  nfapi_vnf_p7_config_t* config = (nfapi_vnf_p7_config_t*)vnf_p7;
	nfapi_nr_dl_node_sync_t dl_node_sync;
	memset(&dl_node_sync, 0, sizeof(dl_node_sync));

	dl_node_sync.header.phy_id = p7_info->phy_id;
	dl_node_sync.header.message_id = NFAPI_NR_PHY_MSG_TYPE_DL_NODE_SYNC;
	dl_node_sync.t1 = calculate_nr_t1(p7_info->mu, p7_info->sfn,p7_info->slot, vnf_p7->slot_start_time_hr);
	dl_node_sync.delta_sfn_slot = 0;

	return config->send_p7_msg(vnf_p7, &dl_node_sync.header);
}

int vnf_nr_sync(vnf_p7_t* vnf_p7, nfapi_vnf_p7_connection_info_t* p7_info)
{

	if(p7_info->in_sync == 1)
	{
		uint16_t dl_sync_period_mask = p7_info->dl_in_sync_period-1;
		uint16_t sfn_slot_dec = NFAPI_SFNSLOT2DEC(p7_info->mu, p7_info->sfn,p7_info->slot);

		if ((((sfn_slot_dec + p7_info->dl_in_sync_offset) % NFAPI_MAX_SFNSLOTDEC(p7_info->mu)) & dl_sync_period_mask) == 0)
		{
			vnf_nr_build_send_dl_node_sync(vnf_p7, p7_info);
		}
	}
	else
	{
		uint16_t dl_sync_period_mask = p7_info->dl_out_sync_period-1;
		uint16_t sfn_slot_dec = NFAPI_SFNSLOT2DEC(p7_info->mu, p7_info->sfn, p7_info->slot);

		if ((((sfn_slot_dec + p7_info->dl_out_sync_offset) % NFAPI_MAX_SFNSLOTDEC(p7_info->mu)) & dl_sync_period_mask) == 0)
		{
			vnf_nr_build_send_dl_node_sync(vnf_p7, p7_info);
		}
	}
	return 0;
}

void vnf_handle_nr_slot_indication(void *pRecvMsg, int recvMsgLen, vnf_p7_t* vnf_p7)
{
	// ensure it's valid
	if (pRecvMsg == NULL || vnf_p7 == NULL)
	{
		NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
	}
	else
	{
		nfapi_nr_slot_indication_scf_t ind = {0};
    const bool result = vnf_p7->_public.unpack_func(pRecvMsg, recvMsgLen, &ind, sizeof(ind), &vnf_p7->_public.codec_config);
		if(!result)
		{
			NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Failed to unpack message\n", __FUNCTION__);
		}
		else
		{
			NFAPI_TRACE(NFAPI_TRACE_DEBUG, "%s: Handling NR SLOT Indication\n", __FUNCTION__);
                        if(vnf_p7->_public.nr_slot_indication)
			{
				(vnf_p7->_public.nr_slot_indication)(&ind);
			}
      free_slot_indication(&ind);
		}

	}
}

void vnf_handle_nr_rx_data_indication(void *pRecvMsg, int recvMsgLen, vnf_p7_t* vnf_p7)
{
	// ensure it's valid
	if (pRecvMsg == NULL || vnf_p7 == NULL)
	{
		NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
	}
	else
	{
		nfapi_nr_rx_data_indication_t ind;
    const bool result = vnf_p7->_public.unpack_func(pRecvMsg, recvMsgLen, &ind, sizeof(ind), &vnf_p7->_public.codec_config);
		if(!result)
		{
			NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Failed to unpack message\n", __FUNCTION__);
		}
		else
		{
			NFAPI_TRACE(NFAPI_TRACE_DEBUG, "%s: Handling RX Indication\n", __FUNCTION__);
                        if(vnf_p7->_public.nr_rx_data_indication)
			{
				(vnf_p7->_public.nr_rx_data_indication)(&ind);
			}
      free_rx_data_indication(&ind);
		}
	}
}

void vnf_handle_nr_crc_indication(void *pRecvMsg, int recvMsgLen, vnf_p7_t* vnf_p7)
{
	// ensure it's valid
	if (pRecvMsg == NULL || vnf_p7 == NULL)
	{
		NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
	}
	else
	{
		nfapi_nr_crc_indication_t ind;
    const bool result = vnf_p7->_public.unpack_func(pRecvMsg, recvMsgLen, &ind, sizeof(ind), &vnf_p7->_public.codec_config);
		if(!result)
		{
			NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Failed to unpack message\n", __FUNCTION__);
		}
		else
		{
		        NFAPI_TRACE(NFAPI_TRACE_DEBUG, "%s: Handling CRC Indication\n", __FUNCTION__);
			if(vnf_p7->_public.nr_crc_indication)
			{
				(vnf_p7->_public.nr_crc_indication)(&ind);
			}
      free_crc_indication(&ind);
		}
	}
}

void vnf_handle_nr_srs_indication(void *pRecvMsg, int recvMsgLen, vnf_p7_t* vnf_p7)
{
	// ensure it's valid
	if (pRecvMsg == NULL || vnf_p7 == NULL)
	{
		NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
	}
	else
	{
		nfapi_nr_srs_indication_t ind;
    const bool result = vnf_p7->_public.unpack_func(pRecvMsg, recvMsgLen, &ind, sizeof(ind), &vnf_p7->_public.codec_config);
		if(!result)
		{
			NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Failed to unpack message\n", __FUNCTION__);
		}
		else
		{
			if(vnf_p7->_public.nr_srs_indication)
			{
				(vnf_p7->_public.nr_srs_indication)(&ind);
			}
      free_srs_indication(&ind);
		}
	}
}

void vnf_handle_nr_srs_toa_vendor_ext_indication(void* pRecvMsg, int recvMsgLen, vnf_p7_t* vnf_p7)
{
  // ensure it's valid
  if (pRecvMsg == NULL || vnf_p7 == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_nr_srs_toa_vendor_ext_indication_t ind;
    const bool result = vnf_p7->_public.unpack_func(pRecvMsg, recvMsgLen, &ind, sizeof(ind), &vnf_p7->_public.codec_config);
    if (!result) {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Failed to unpack message\n", __FUNCTION__);
    } else {
      if (vnf_p7->_public.nr_srs_toa_vendor_ext_indication) {
        (vnf_p7->_public.nr_srs_toa_vendor_ext_indication)(&ind);
      }
      free_srs_toa_vendor_ext_indication(&ind);
    }
  }
}

void vnf_handle_nr_uci_indication(void *pRecvMsg, int recvMsgLen, vnf_p7_t* vnf_p7)
{
	// ensure it's valid
	if (pRecvMsg == NULL || vnf_p7 == NULL)
	{
		NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
	}
	else
	{
		nfapi_nr_uci_indication_t ind;
    const bool result = vnf_p7->_public.unpack_func(pRecvMsg, recvMsgLen, &ind, sizeof(ind), &vnf_p7->_public.codec_config);
		if(!result)
		{
			NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Failed to unpack message\n", __FUNCTION__);
		}
		else
		{
		        NFAPI_TRACE(NFAPI_TRACE_DEBUG, "%s: Handling UCI Indication\n", __FUNCTION__);
			if(vnf_p7->_public.nr_uci_indication)
			{
				(vnf_p7->_public.nr_uci_indication)(&ind);
			}
      free_uci_indication(&ind);
		}
	}
}

void vnf_handle_nr_rach_indication(void *pRecvMsg, int recvMsgLen, vnf_p7_t* vnf_p7)
{
	// ensure it's valid
	if (pRecvMsg == NULL || vnf_p7 == NULL)
	{
		NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
	}
	else
	{
		nfapi_nr_rach_indication_t ind;
    const bool result = vnf_p7->_public.unpack_func(pRecvMsg, recvMsgLen, &ind, sizeof(ind), &vnf_p7->_public.codec_config);
		if(!result)
		{
			NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Failed to unpack message\n", __FUNCTION__);
		}
		else
		{
		        NFAPI_TRACE(NFAPI_TRACE_INFO, "%s: Handling RACH Indication\n", __FUNCTION__);
			if(vnf_p7->_public.nr_rach_indication)
			{
				(vnf_p7->_public.nr_rach_indication)(&ind);
			}
      free_rach_indication(&ind);
		}
	}
}

void vnf_nr_handle_p7_vendor_extension(void *pRecvMsg, int recvMsgLen, vnf_p7_t* vnf_p7, uint16_t message_id)
{
  if (pRecvMsg == NULL || vnf_p7 == NULL)
  {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  }
  else if(vnf_p7->_public.allocate_p7_vendor_ext)
  {
    uint16_t msg_size;
    nfapi_nr_p7_message_header_t* msg = vnf_p7->_public.allocate_p7_vendor_ext(message_id, &msg_size);

    if(msg == 0)
    {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "%s failed to allocate vendor extention structure\n", __FUNCTION__);
      return;
    }
    const bool result = vnf_p7->_public.unpack_func(pRecvMsg, recvMsgLen, msg, msg_size, &vnf_p7->_public.codec_config);
    if(!result)
    {
      if(vnf_p7->_public.vendor_ext)
        vnf_p7->_public.vendor_ext(&(vnf_p7->_public), msg);
    }

    if(vnf_p7->_public.deallocate_p7_vendor_ext)
      vnf_p7->_public.deallocate_p7_vendor_ext(msg);

  }
}

void vnf_nr_handle_ul_node_sync(void *pRecvMsg, int recvMsgLen, vnf_p7_t* vnf_p7)
{
	uint32_t now_time_hr = vnf_get_current_time_hr();
	if (pRecvMsg == NULL || vnf_p7  == NULL)
	{
		NFAPI_TRACE(NFAPI_TRACE_ERROR, "vnf_handle_ul_node_sync: NULL parameters\n");
		return;
	}

	nfapi_nr_ul_node_sync_t ind;
	if (!vnf_p7->_public.unpack_func(pRecvMsg, recvMsgLen, &ind, sizeof(ind), &vnf_p7->_public.codec_config)) {
		NFAPI_TRACE(NFAPI_TRACE_ERROR, "Failed to unpack ul_node_sync\n");
		return;
	}

	nfapi_vnf_p7_connection_info_t* p7_info = vnf_p7_connection_info_list_find(vnf_p7, ind.header.phy_id);
	if (!p7_info) {
		NFAPI_TRACE(NFAPI_TRACE_ERROR, "PHY instance not found for phy_id:%d\n", ind.header.phy_id);
		return;
	}
	pthread_mutex_lock(&p7_info->mutex);
	uint32_t t4 = calculate_nr_t4(now_time_hr, p7_info->mu, p7_info->sfn, p7_info->slot, vnf_p7->slot_start_time_hr);
	/*
	* Time Synchronization Algorithm
	*
	* T1 = VNF Transmit Time (t1)    |   T2 = PNF Receive Time (t2)
	* T3 = PNF Transmit Time (t3)    |   T4 = VNF Receive Time (t4)
	*
	* Assuming symmetric network delay:
	* T2 - T1 = Delay + Offset
	* T4 - T3 = Delay - Offset
	* Offset = ((T2 - T1) - (T4 - T3)) / 2
	*/
	int64_t diff1 = (int64_t)ind.t2 - (int64_t)ind.t1;
	int64_t diff2 = (int64_t)t4 - (int64_t)ind.t3;
	int64_t wrap_us = 10240000LL;
	int64_t half_wrap = 5120000LL;
	// 10.24s Wrap-around protection (nFAPI timestamps are constrained by 1024 SFN loop)
	while (diff1 > half_wrap) diff1 -= wrap_us;
	while (diff1 < -half_wrap) diff1 += wrap_us;
	while (diff2 > half_wrap) diff2 -= wrap_us;
	while (diff2 < -half_wrap) diff2 += wrap_us;
	int32_t offset = (int32_t)((diff1 - diff2) / 2);

	int32_t total_correction = offset;

	// Update 5G NR filtered offset (EWMA with alpha = 1/8)
	if (p7_info->nr_offset_filtered == 0) {
		p7_info->nr_offset_filtered = total_correction;
	} else {
		p7_info->nr_offset_filtered = (p7_info->nr_offset_filtered * 7 + total_correction) / 8;
	}

	if (p7_info->sync_locked) {
		// Proportional micro-steering.
		// Use gain 1/16 if |total_correction| > 100 to converge faster.
		// Use gain 1/32 if |total_correction| <= 100 for stability.
		int32_t micro_adj = 0;
		if (total_correction > 100 || total_correction < -100) {
			micro_adj = total_correction / 16;
		} else {
			micro_adj = total_correction / 32;
		}
		p7_info->pending_us -= micro_adj;

		// Drift Monitoring
		if (total_correction <= -2500 || total_correction >= 2500) {
			// 1. Massive raw drift: unlock immediately
			p7_info->sync_locked = 0;
			p7_info->consecutive_drift_violations = 0;
			NFAPI_TRACE(NFAPI_TRACE_WARN, "[P7_SYNC] Massive raw drift detected (%d us). Unlocking sync immediately.\n", total_correction);
		} else if (p7_info->nr_offset_filtered <= -MARGIN_TOLERANCE_LOCKED_US
		           || p7_info->nr_offset_filtered >= MARGIN_TOLERANCE_LOCKED_US) {
			// 2. Persistent smoothed drift: unlock after 3 consecutive samples
			p7_info->consecutive_drift_violations++;
			if (p7_info->consecutive_drift_violations >= 3) {
				p7_info->sync_locked = 0;
				p7_info->consecutive_drift_violations = 0;
				NFAPI_TRACE(NFAPI_TRACE_WARN,
				            "[P7_SYNC] Persistent smoothed drift detected (%d us, raw: %d us). Unlocking sync for re-calibration.\n",
				            p7_info->nr_offset_filtered, total_correction);
			} else {
				NFAPI_TRACE(NFAPI_TRACE_INFO, "[P7_SYNC] Smoothed drift warning (%d us, raw: %d us) (count: %d), waiting to confirm.\n",
				            p7_info->nr_offset_filtered, total_correction, p7_info->consecutive_drift_violations);
			}
		} else {
			p7_info->consecutive_drift_violations = 0;
		}
	}

	if (!p7_info->sync_locked) {
		// Lock when BOTH raw offset and smoothed offset are within lock tolerance
		if (total_correction >= -MARGIN_TOLERANCE_US && total_correction <= MARGIN_TOLERANCE_US &&
		    p7_info->nr_offset_filtered >= -MARGIN_TOLERANCE_US && p7_info->nr_offset_filtered <= MARGIN_TOLERANCE_US) {
			p7_info->sync_locked = 1;
			p7_info->consecutive_drift_violations = 0;
			NFAPI_TRACE(NFAPI_TRACE_INFO, "[P7_SYNC] Sync locked successfully (offset: %d us, smoothed: %d us).\n",
			            total_correction, p7_info->nr_offset_filtered);
		} else {
			int32_t s_adj = 0;
			int32_t p_adj = 0;

			// Symmetrically constrain massive synchronization jumps to prevent system crashes
			int32_t capped_correction = total_correction;
			int32_t max_total_cap = 5 * (int32_t)p7_info->slot_duration_us;
			if (capped_correction > max_total_cap) capped_correction = max_total_cap;
			if (capped_correction < -max_total_cap) capped_correction = -max_total_cap;

			if (capped_correction <= -(int32_t)p7_info->slot_duration_us || capped_correction >= (int32_t)p7_info->slot_duration_us) {
				s_adj = capped_correction / (int32_t)p7_info->slot_duration_us;
				p_adj = capped_correction - (s_adj * (int32_t)p7_info->slot_duration_us);
			} else {
				// Proportional control with gain of 4 for faster unlocked convergence (was 8)
				p_adj = capped_correction / 4;
				if (p_adj == 0 && capped_correction != 0) {
					p_adj = (capped_correction > 0) ? 1 : -1;
				}
			}

			int32_t max_p_adj = 10 * p7_info->slot_duration_us;
			if (p_adj > max_p_adj) p_adj = max_p_adj;
			if (p_adj < -max_p_adj) p_adj = -max_p_adj;

			p7_info->slot_adjustment += s_adj;
			p7_info->pending_us -= p_adj;
		}
	}
	pthread_mutex_unlock(&p7_info->mutex);
}

void vnf_handle_timing_info(void *pRecvMsg, int recvMsgLen, vnf_p7_t* vnf_p7)
{
	if (pRecvMsg == NULL || vnf_p7 == NULL)
	{
		NFAPI_TRACE(NFAPI_TRACE_ERROR, "vnf_handle_timing_info: NULL parameters\n");
		return;
	}

	nfapi_timing_info_t ind;
	if(nfapi_p7_message_unpack(pRecvMsg, recvMsgLen, &ind, sizeof(nfapi_timing_info_t), &vnf_p7->_public.codec_config) < 0)
	{
		NFAPI_TRACE(NFAPI_TRACE_ERROR, "Failed to unpack timing_info\n");
		return;
	}

        if (vnf_p7 && vnf_p7->p7_connections)
        {
          int16_t vnf_pnf_sfnsf_delta = NFAPI_SFNSF2DEC(vnf_p7->p7_connections[0].sfn_sf) - NFAPI_SFNSF2DEC(ind.last_sfn_sf);

          //NFAPI_TRACE(NFAPI_TRACE_INFO, "%s() PNF:SFN/SF:%d VNF:SFN/SF:%d deltaSFNSF:%d\n", __FUNCTION__, NFAPI_SFNSF2DEC(ind.last_sfn_sf), NFAPI_SFNSF2DEC(vnf_p7->p7_connections[0].sfn_sf), vnf_pnf_sfnsf_delta);

          // Panos: Careful here!!! Modification of the original nfapi-code
          //if (vnf_pnf_sfnsf_delta>1 || vnf_pnf_sfnsf_delta < -1)
          if (vnf_pnf_sfnsf_delta>0 || vnf_pnf_sfnsf_delta < 0)
          {
            NFAPI_TRACE(NFAPI_TRACE_INFO, "%s() LARGE SFN/SF DELTA between PNF and VNF delta:%d VNF:%d PNF:%d\n\n\n\n\n\n\n\n\n", __FUNCTION__, vnf_pnf_sfnsf_delta, NFAPI_SFNSF2DEC(vnf_p7->p7_connections[0].sfn_sf), NFAPI_SFNSF2DEC(ind.last_sfn_sf));
            // Panos: Careful here!!! Modification of the original nfapi-code
            vnf_p7->p7_connections[0].sfn_sf = ind.last_sfn_sf;
          }
        }
}

void vnf_nr_handle_timing_info(void *pRecvMsg, int recvMsgLen, vnf_p7_t* vnf_p7)
{
	if (pRecvMsg == NULL || vnf_p7 == NULL)
	{
		NFAPI_TRACE(NFAPI_TRACE_ERROR, "vnf_handle_timing_info: NULL parameters\n");
		return;
	}

	nfapi_vnf_p7_connection_info_t *p7_con = &vnf_p7->p7_connections[0];

	nfapi_nr_timing_info_t ind;
	const bool result = vnf_p7->_public.unpack_func(pRecvMsg, recvMsgLen, &ind, sizeof(ind), &vnf_p7->_public.codec_config);
	if(!result)
	{
		NFAPI_TRACE(NFAPI_TRACE_ERROR, "Failed to unpack timing_info\n");
		return;
	}

        if (vnf_p7 && vnf_p7->p7_connections)
        {
            int16_t vnf_pnf_sfnslot_delta = NFAPI_SFNSLOT2DEC(p7_con->mu, p7_con->sfn,p7_con->slot) - NFAPI_SFNSLOT2DEC(p7_con->mu, ind.last_sfn,ind.last_slot);

          if (vnf_pnf_sfnslot_delta > 1)
          {
            NFAPI_TRACE(NFAPI_TRACE_WARN, "%s() LARGE SFN/SLOT DELTA between PNF and VNF. Delta %d slots. PNF:%d.%d VNF:%d.%d\n",
                        __FUNCTION__, vnf_pnf_sfnslot_delta,
                        ind.last_sfn, ind.last_slot,
                        vnf_p7->p7_connections[0].sfn, vnf_p7->p7_connections[0].slot);
            vnf_p7->p7_connections[0].sfn = ind.last_sfn;
            vnf_p7->p7_connections[0].slot = ind.last_slot;
          }
        }
}

void vnf_nr_handle_p7_message(void *pRecvMsg, int recvMsgLen, vnf_p7_t* vnf_p7)
{
  if (vnf_p7->terminate) {
    return;
  }
	nfapi_nr_p7_message_header_t header;

	// validate the input params
	if(pRecvMsg == NULL || recvMsgLen < 4 || vnf_p7 == NULL)
	{
		NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
		return;
	}

	// unpack the message header
  const bool result = vnf_p7->_public.hdr_unpack_func(pRecvMsg, recvMsgLen, &header, sizeof(header), &vnf_p7->_public.codec_config);
	if (!result)
	{
		NFAPI_TRACE(NFAPI_TRACE_ERROR, "Unpack message header failed, ignoring\n");
		return;
	}

	// ensure the message is sensible
	if (recvMsgLen < 8 || pRecvMsg == NULL)
	{
		NFAPI_TRACE(NFAPI_TRACE_WARN, "Invalid message size: %d, ignoring\n", recvMsgLen);
		return;
	}

	switch (header.message_id)
	{
		case NFAPI_NR_PHY_MSG_TYPE_UL_NODE_SYNC:
			vnf_nr_handle_ul_node_sync(pRecvMsg, recvMsgLen, vnf_p7);
			break;

		case NFAPI_TIMING_INFO:
			vnf_nr_handle_timing_info(pRecvMsg, recvMsgLen, vnf_p7);
			break;

		case NFAPI_NR_PHY_MSG_TYPE_SLOT_INDICATION:
			vnf_handle_nr_slot_indication(pRecvMsg, recvMsgLen, vnf_p7);
			break;

		case NFAPI_NR_PHY_MSG_TYPE_RX_DATA_INDICATION:
			vnf_handle_nr_rx_data_indication(pRecvMsg, recvMsgLen, vnf_p7);
			break;

		case NFAPI_NR_PHY_MSG_TYPE_CRC_INDICATION:
			vnf_handle_nr_crc_indication(pRecvMsg, recvMsgLen, vnf_p7);
			break;

		case NFAPI_NR_PHY_MSG_TYPE_UCI_INDICATION:
			vnf_handle_nr_uci_indication(pRecvMsg, recvMsgLen, vnf_p7);
			break;

		case NFAPI_NR_PHY_MSG_TYPE_SRS_INDICATION:
			vnf_handle_nr_srs_indication(pRecvMsg, recvMsgLen, vnf_p7);
			break;

                case NFAPI_NR_PHY_MSG_TYPE_SRS_TOA_VENDOR_EXTENSION_INDICATION:
                        vnf_handle_nr_srs_toa_vendor_ext_indication(pRecvMsg, recvMsgLen, vnf_p7);
                        break;
   
		case NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION:
			vnf_handle_nr_rach_indication(pRecvMsg, recvMsgLen, vnf_p7);
			break;

		default:
			{
				if(header.message_id >= NFAPI_VENDOR_EXT_MSG_MIN &&
				   header.message_id <= NFAPI_VENDOR_EXT_MSG_MAX)
				{
					vnf_nr_handle_p7_vendor_extension(pRecvMsg, recvMsgLen, vnf_p7, header.message_id);
				}
				else
				{
					NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 Unknown message ID %d\n", header.message_id);
				}
			}
			break;
	}
}
