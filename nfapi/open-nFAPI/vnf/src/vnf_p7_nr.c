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
		        NFAPI_TRACE(NFAPI_TRACE_DEBUG, "%s: Handling RACH Indication\n", __FUNCTION__);
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
  const bool result = vnf_p7->_public.unpack_func(pRecvMsg, recvMsgLen, &ind, sizeof(nfapi_nr_ul_node_sync_t), &vnf_p7->_public.codec_config);
	if(!result)
	{
		NFAPI_TRACE(NFAPI_TRACE_ERROR, "Failed to unpack ul_node_sync\n");
		return;
	}

	nfapi_vnf_p7_connection_info_t* phy = vnf_p7_connection_info_list_find(vnf_p7, ind.header.phy_id);
	uint32_t t4 = calculate_nr_t4(now_time_hr, phy->mu, phy->sfn, phy->slot, vnf_p7->slot_start_time_hr);

	uint32_t tx_2_rx = t4>ind.t1 ? t4 - ind.t1 : t4 + NFAPI_MAX_SFNSLOTDEC(phy->mu) - ind.t1 ;
	uint32_t pnf_proc_time = ind.t3 - ind.t2;

	// divide by 2 using shift operator
	uint32_t latency =  (tx_2_rx - pnf_proc_time) >> 1;

	if(!(phy->filtered_adjust))
	{
		phy->latency[phy->min_sync_cycle_count] = latency;
	}
	else
	{
		phy->latency[phy->min_sync_cycle_count] = latency;

		{
			if (ind.t2 < phy->previous_t2 && ind.t1 > phy->previous_t1)
			{
				// Only t2 wrap has occurred!!!
				phy->slot_offset = (NFAPI_MAX_SFNSLOTDEC(phy->mu) + ind.t2) - ind.t1 - latency;
			}
			else if (ind.t2 > phy->previous_t2 && ind.t1 < phy->previous_t1)
			{
				// Only t1 wrap has occurred
				phy->slot_offset = ind.t2 - ( ind.t1 + NFAPI_MAX_SFNSLOTDEC(phy->mu)) - latency;
			}
			else
			{
				// Either no wrap or both have wrapped
				phy->slot_offset = ind.t2 - ind.t1 - latency;
			}

			if (phy->slot_offset_filtered == 0)
			{
				phy->slot_offset_filtered = phy->slot_offset;
			}
			else
			{
				int32_t oldFilteredValueShifted = phy->slot_offset_filtered << 5;
				int32_t newOffsetShifted = phy->slot_offset << 5;

				// 1/8 of new and 7/8 of old
				phy->slot_offset_filtered = ((newOffsetShifted >> 3) + ((oldFilteredValueShifted * 7) >> 3)) >> 5;
			}
		}

		if(1)
		{
                  struct timespec ts;
                  clock_gettime(CLOCK_MONOTONIC, &ts);
                  (void)ts;
		}

	}

        if (phy->filtered_adjust && (phy->slot_offset_filtered > 1e6 || phy->slot_offset_filtered < -1e6))
        {
          phy->filtered_adjust = 0;
          phy->zero_count=0;
          phy->min_sync_cycle_count = 2;
          phy->in_sync = 0;
          NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s - ADJUST TOO BAD - go out of filtered phy->slot_offset_filtered:%d\n", __FUNCTION__, phy->slot_offset_filtered);
        }

	if(phy->min_sync_cycle_count)
		phy->min_sync_cycle_count--;

	if(phy->min_sync_cycle_count == 0)
	{
		uint32_t curr_sfn = phy->sfn;
		uint32_t curr_slot = phy->slot;
		int32_t sfn_slot_dec = NFAPI_SFNSLOT2DEC(phy->mu, phy->sfn,phy->slot);

		if(!phy->filtered_adjust)
		{
			int i = 0;
			for(i = 0; i < SYNC_CYCLE_COUNT; ++i)
			{
				phy->average_latency += phy->latency[i];

			}
			phy->average_latency /= SYNC_CYCLE_COUNT;

			phy->slot_offset = ind.t2 - (ind.t1 - phy->average_latency);

			sfn_slot_dec += (phy->slot_offset / 500);

			NFAPI_TRACE(NFAPI_TRACE_NOTE, "PNF to VNF slot offset:%d sfn :%d slot:%d \n",phy->slot_offset,NFAPI_SFNSLOTDEC2SFN(phy->mu, sfn_slot_dec),NFAPI_SFNSLOTDEC2SLOT(phy->mu, sfn_slot_dec) );


		}
		else
		{
			sfn_slot_dec += ((phy->slot_offset_filtered + 250) / 500);	//Round up to go from microsecond to slot

		}

		if(sfn_slot_dec < 0)
		{
			sfn_slot_dec += NFAPI_MAX_SFNSLOTDEC(phy->mu);
		}
		else if( sfn_slot_dec >= NFAPI_MAX_SFNSLOTDEC(phy->mu))
		{
			sfn_slot_dec -= NFAPI_MAX_SFNSLOTDEC(phy->mu);
		}


		uint16_t new_sfn = NFAPI_SFNSLOTDEC2SFN(phy->mu, sfn_slot_dec);
		uint16_t new_slot = NFAPI_SFNSLOTDEC2SLOT(phy->mu, sfn_slot_dec);

		{
			phy->adjustment = NFAPI_SFNSLOT2DEC(phy->mu, new_sfn, new_slot) - NFAPI_SFNSLOT2DEC(phy->mu, curr_sfn, curr_slot);

			phy->previous_t1 = 0;
			phy->previous_t2 = 0;

			if(phy->previous_slot_offset_filtered > 0)
			{
				if( phy->slot_offset_filtered > phy->previous_slot_offset_filtered)
				{
					// pnf is getting futher ahead of vnf
					phy->slot_offset_trend = (phy->slot_offset_filtered + phy->previous_slot_offset_filtered)/2;
				}
				else
				{
					// pnf is getting back in sync
				}
			}
			else if(phy->previous_slot_offset_filtered < 0)
			{
				if(phy->slot_offset_filtered < phy->previous_slot_offset_filtered)
				{
					// vnf is getting future ahead of pnf
					phy->slot_offset_trend = (-(phy->slot_offset_filtered + phy->previous_slot_offset_filtered)) /2;
				}
				else
				{
					//  vnf is getting back in sync
				}
			}


			int insync_minor_adjustment_1 = phy->slot_offset_trend / 6;
			int insync_minor_adjustment_2 = phy->slot_offset_trend / 2;


			if(insync_minor_adjustment_1 == 0)
				insync_minor_adjustment_1 = 2;

			if(insync_minor_adjustment_2 == 0)
				insync_minor_adjustment_2 = 10;

			if(!phy->filtered_adjust)
			{
				if(phy->adjustment < 10)
				{
					phy->zero_count++;

					if(phy->zero_count >= 10)
					{
						phy->filtered_adjust = 1;
						phy->zero_count = 0;

						NFAPI_TRACE(NFAPI_TRACE_NOTE, "***** Adjusting VNF SFN/SF switching to filtered mode\n");
					}
				}
				else
				{
					phy->zero_count = 0;
				}
			}
			else
			{
				// Fine level of adjustment
				if (phy->adjustment == 0)
				{
					if (phy->zero_count >= 10)
					{
						if(phy->in_sync == 0)
						{
							NFAPI_TRACE(NFAPI_TRACE_NOTE, "VNF P7 In Sync with phy (phy_id:%d)\n", phy->phy_id);

							if(vnf_p7->_public.sync_indication)
								(vnf_p7->_public.sync_indication)(&(vnf_p7->_public), phy->in_sync);
						}

						phy->in_sync = 1;
					}
					else
					{
						phy->zero_count++;
					}

					if(phy->in_sync)
					{
						// in sync
						if(phy->slot_offset_filtered > 250)
						{
							// VNF is slow
							phy->insync_minor_adjustment = insync_minor_adjustment_1; //25;
							phy->insync_minor_adjustment_duration = ((phy->slot_offset_filtered) / insync_minor_adjustment_1);
						}
						else if(phy->slot_offset_filtered < -250)
						{
							// VNF is fast
							phy->insync_minor_adjustment = -(insync_minor_adjustment_1); //25;
							phy->insync_minor_adjustment_duration = (((phy->slot_offset_filtered) / -(insync_minor_adjustment_1)));
						}
						else
						{
							phy->insync_minor_adjustment = 0;
						}

						if(phy->insync_minor_adjustment != 0)
						{
              NFAPI_TRACE(NFAPI_TRACE_DEBUG,
                          "(%4d/%d) VNF phy_id:%d Apply minor insync adjustment %dus for %d slots (slot_offset_filtered:%d) %d %d "
                          "%d NEW:%d.%d CURR:%d.%d adjustment:%d\n",
                          phy->sfn,
                          phy->slot,
                          ind.header.phy_id,
                          phy->insync_minor_adjustment,
                          phy->insync_minor_adjustment_duration,
                          phy->slot_offset_filtered,
                          insync_minor_adjustment_1,
                          insync_minor_adjustment_2,
                          phy->slot_offset_trend,
                          new_sfn,
                          new_slot,
                          curr_sfn,
                          curr_slot,
                          phy->adjustment);
            }
					}
				}
				else
				{
					if (phy->in_sync)
					{
						if(phy->adjustment == 0)
						{
						}
						else if(phy->adjustment > 0)
						{
							// VNF is slow
							{
								if(phy->slot_offset_filtered > 250)
								{
									// VNF is slow
									phy->insync_minor_adjustment = insync_minor_adjustment_2;
									phy->insync_minor_adjustment_duration = 2 * ((phy->slot_offset_filtered - 250) / insync_minor_adjustment_2);
								}
								else if(phy->slot_offset_filtered < -250)
								{
									// VNF is fast
									phy->insync_minor_adjustment = -(insync_minor_adjustment_2);
									phy->insync_minor_adjustment_duration = 2 * ((phy->slot_offset_filtered + 250) / -(insync_minor_adjustment_2));
								}

							}

              NFAPI_TRACE(NFAPI_TRACE_DEBUG,
                          "(%4d/%d) VNF phy_id:%d Apply minor insync adjustment %dus for %d slots (adjustment:%d "
                          "slot_offset_filtered:%d) %d %d %d NEW:%d.%d CURR:%d.%d adj:%d\n",
                          phy->sfn,
                          phy->slot,
                          ind.header.phy_id,
                          phy->insync_minor_adjustment,
                          phy->insync_minor_adjustment_duration,
                          phy->adjustment,
                          phy->slot_offset_filtered,
                          insync_minor_adjustment_1,
                          insync_minor_adjustment_2,
                          phy->slot_offset_trend,
                          new_sfn,
                          new_slot,
                          curr_sfn,
                          curr_slot,
                          phy->adjustment);

            }
						else if(phy->adjustment < 0)
						{
							// VNF is fast
							{
								if(phy->slot_offset_filtered > 250)
								{
									// VNF is slow
									phy->insync_minor_adjustment = insync_minor_adjustment_2;
									phy->insync_minor_adjustment_duration = 2 * ((phy->slot_offset_filtered - 250) / insync_minor_adjustment_2);
								}
								else if(phy->slot_offset_filtered < -250)
								{
									// VNF is fast
									phy->insync_minor_adjustment = -(insync_minor_adjustment_2);
									phy->insync_minor_adjustment_duration = 2 * ((phy->slot_offset_filtered + 250) / -(insync_minor_adjustment_2));
								}
							}
						}
					}
				}
			}


			if(phy->in_sync == 0)
			{
				phy->sfn = new_sfn;
				phy->slot = new_slot;
			}
		}

		// reset for next cycle
		phy->previous_slot_offset_filtered = phy->slot_offset_filtered;
		phy->min_sync_cycle_count = 2;
		phy->slot_offset_filtered = 0;
		phy->slot_offset = 0;
	}
	else
	{
		phy->previous_t1 = ind.t1;
		phy->previous_t2 = ind.t2;
	}
}

int vnf_nr_extract_timing_info(const nfapi_nr_timing_info_t *ind,
                               nfapi_vnf_p7_connection_info_t *p7_info,
                               vnf_timing_stats_t *out_stats)
{
	if (ind == NULL || p7_info == NULL || out_stats == NULL) {
		return 0;
	}
	int32_t slot_duration_us = 1000 >> p7_info->mu;
	if (slot_duration_us <= 0) {
		return 0;
	}
	int32_t slots_per_frame = 10 << p7_info->mu;
	int64_t frame_duration_us = (int64_t)slots_per_frame * (int64_t)slot_duration_us;
	int64_t timing_window_us = (int64_t)p7_info->timing_window;
	int64_t valid_span_us = timing_window_us + frame_duration_us;
	if (valid_span_us <= 0) {
		valid_span_us = frame_duration_us;
	}
	/*
	 * Latest delay values:
	 * These are the most important values for no-drop policy.
	 * A positive latest_delay means the message was late.
	 * A negative latest_delay means the message arrived before deadline.
	 */
	int32_t latest_delay_values[4] = {
		ind->dl_tti.latest_delay,
		ind->tx_data.latest_delay,
		ind->ul_tti.latest_delay,
		ind->ul_dci.latest_delay
	};
	/*
	 * Earliest arrival values:
	 * These are useful to know how early messages are arriving.
	 * They should not override a positive latest_delay.
	 */
	int32_t earliest_arrival_values[4] = {
		ind->dl_tti.earliest_arrival,
		ind->tx_data.earliest_arrival,
		ind->ul_tti.earliest_arrival,
		ind->ul_dci.earliest_arrival
	};
	int32_t worst_late = INT32_MIN;
	int32_t worst_early = INT32_MAX;
	bool have_latest_delay = false;
	bool have_any_sample = false;
	/*
	 * First pass:
	 *   use latest_delay fields as primary control input.
	 * This avoids an early-arrival value masking a real late sample.
	 */
	for (int i = 0; i < 4; ++i) {
		int32_t value = latest_delay_values[i];
		/*
		 * In current nFAPI timing_info usage, zero is treated as
		 * "not reported".  If the PNF implementation later defines
		 * zero as an explicit exact-deadline sample, this condition
		 * should be revisited.
		 */
		if (value == 0) {
			continue;
		}
		if ((int64_t)value > valid_span_us || (int64_t)value < -valid_span_us) {
			continue;
		}
		have_latest_delay = true;
		have_any_sample = true;
		if (value > worst_late) {
			worst_late = value;
		}
		if (value < worst_early) {
			worst_early = value;
		}
	}
	/*
	 * Second pass:
	 *   collect earliest_arrival for diagnostics / fallback.
	 *
	 * If there were no latest_delay samples at all, the closest
	 * earliest_arrival becomes worst_late.  This keeps the controller
	 * informed that packets are early, without inventing late pressure.
	 */
	int32_t closest_early_to_deadline = INT32_MIN;
	for (int i = 0; i < 4; ++i) {
		int32_t value = earliest_arrival_values[i];
		if (value == 0) {
			continue;
		}
		if ((int64_t)value > valid_span_us || (int64_t)value < -valid_span_us) {
			continue;
		}
		have_any_sample = true;
		if (value < worst_early) {
			worst_early = value;
		}
		/*
		 * For early samples, the largest value is closest to deadline.
		 * Example:
		 *   -100us is closer / riskier than -900us.
		 */
		if (value > closest_early_to_deadline) {
			closest_early_to_deadline = value;
		}
	}
	if (!have_any_sample) {
		return 0;
	}
	if (!have_latest_delay) {
		if (closest_early_to_deadline == INT32_MIN) {
			return 0;
		}
		worst_late = closest_early_to_deadline;
	}
	if (worst_late == INT32_MIN) {
		return 0;
	}
	if (worst_early == INT32_MAX) {
		worst_early = worst_late;
	}
	out_stats->worst_late = worst_late;
	out_stats->worst_early = worst_early;
	return 1;
}

static const int32_t global_ewma_alpha_denom = 8;    // 1/8 default
static const int32_t global_ewma_beta_attack_denom = 4;     // 1/4 default (fast attack)
static const int32_t global_ewma_beta_release_denom = 2048;  // 1/2048 default (slow release)

/*
 * Calculate the number of slots between two (SFN, slot) pairs.
 * Accounts for SFN wrap-around (SFN 0-1023).
 * Result: positive if (current_sfn, current_slot) > (prev_sfn, prev_slot)
 */
static inline int32_t calculate_slot_distance(int32_t current_sfn, int32_t current_slot,
                                               int32_t prev_sfn, int32_t prev_slot,
                                               int32_t slots_per_frame)
{
	// Convert to absolute slot numbers within a frame boundary
	int32_t current_absolute = current_sfn * slots_per_frame + current_slot;
	int32_t prev_absolute = prev_sfn * slots_per_frame + prev_slot;

	// Handle wrap-around: if current < prev, add one full hyperframe cycle
	if (current_absolute < prev_absolute) {
		current_absolute += 1024 * slots_per_frame;  // 1024 SFNs per hyperframe
	}

	return current_absolute - prev_absolute;
}

static int32_t abs_i32(int32_t v)
{
	return v < 0 ? -v : v;
}

/*
 * Integer EWMA helper.
 * Avoids integer EWMA dead-zone:
 *   cur += (target - cur) / denom
 * would otherwise stop changing when abs(target - cur) < denom.
 * This is not a policy hyperparameter.
 */
static inline int32_t p7_ewma_step_i32(int32_t cur, int32_t target, int32_t denom)
{
	if (denom <= 1)
		return target;

	int32_t diff = target - cur;
	if (diff == 0)
		return cur;

	int32_t step = diff / denom;
	if (step == 0)
		step = diff > 0 ? 1 : -1;

	return cur + step;
}

/*
 * Delay Management v2 — Minimalist EWMA-based adaptive slot-ahead control.
 *
 * Step 1: EWMA pre-processing (RFC 6298 inspired)
 *   TimingInfoEWMA[i] = (1-α) * TimingInfoEWMA[i-1] + α * TimingInfo[i]
 *   TimingInfoDev[i]  = (1-β) * TimingInfoDev[i-1]  + β * |TimingInfo[i] - TimingInfoEWMA[i]|
 *
 * Step 2 (Late):  if TimingInfo > 0 or EWMA+Dev > 0 → increase ceil((EWMA+Dev)/slot_dur) slots
 * Step 3 (Early): if EWMA < -4*Dev                  → decrease 1 slot
 *
 * Pacing: wait one timing_info_period between adjustments (= wait for fresh measurement).
 */
static void vnf_nr_delay_management(nfapi_vnf_p7_connection_info_t *p7_info, const vnf_timing_stats_t *stats)
{
	if (p7_info == NULL || stats == NULL)
		return;
	if (p7_info->mu < 0 || p7_info->slot_duration_us <= 0)
		return;

	int sd = p7_info->slot_duration_us;

	/* --- Initialization: start from baseline (4 slots ahead) --- */
	/* Shared state below is protected by p7_con->mutex, held by the caller. */
	if (p7_info->slot_ahead <= 0)
		p7_info->slot_ahead = 4;

	if (p7_info->estimated_mean_late == 0) {
		p7_info->estimated_mean_late = stats->worst_late;
		p7_info->estimated_jitter_var = abs_i32(stats->worst_late) / 2;
		p7_info->last_adjustment_sfn = p7_info->sfn;
		p7_info->last_adjustment_slot = p7_info->slot;
	}

	/* ===== Step 1: EWMA Pre-processing ===== */
	int32_t TimingInfo = stats->worst_late;

	p7_info->estimated_mean_late = p7_ewma_step_i32(
		p7_info->estimated_mean_late, TimingInfo, global_ewma_alpha_denom);

	int32_t diff = TimingInfo - p7_info->estimated_mean_late;
	int32_t abs_diff = abs_i32(diff);

	if (abs_diff > p7_info->estimated_jitter_var) {
		p7_info->estimated_jitter_var = p7_ewma_step_i32(
			p7_info->estimated_jitter_var, abs_diff, global_ewma_beta_attack_denom);
	} else {
		p7_info->estimated_jitter_var = p7_ewma_step_i32(
			p7_info->estimated_jitter_var, abs_diff, global_ewma_beta_release_denom);
	}

	if (p7_info->estimated_jitter_var < 0)
		p7_info->estimated_jitter_var = 0;

	int32_t TimingInfoEWMA = p7_info->estimated_mean_late;
	int32_t TimingInfoDev  = p7_info->estimated_jitter_var;

	/* ===== Pacing gate: one timing_info_period between decisions ===== */
	int32_t elapsed = calculate_slot_distance(
		p7_info->sfn, p7_info->slot,
		p7_info->last_adjustment_sfn, p7_info->last_adjustment_slot,
		10 << p7_info->mu);

	/* timing_info_period is in subframes; convert to slots */
	int32_t period_slots = (int32_t)p7_info->timing_info_period * (1 << p7_info->mu);
	if (period_slots < 1) period_slots = 1;
	bool gate_open = (elapsed >= period_slots);

	if (!gate_open) {
		return;
	}

	int32_t target = p7_info->slot_ahead;

	/* ===== Step 2: Late → Increase ===== */
	if (TimingInfo > 0 || (TimingInfoEWMA + TimingInfoDev) > 0) {
		int32_t val = TimingInfoEWMA + TimingInfoDev;
		if (val > 0) {
			int32_t inc = (val + sd - 1) / sd;
			target += inc;
		} else {
			/* raw TimingInfo > 0 but EWMA hasn't caught up yet */
			target += 1;
		}
	}
	/* ===== Step 3: Early → Decrease 1 ===== */
	else if (TimingInfoEWMA < -(sd + 4 * TimingInfoDev)) {
		/*
		 * Decrease only when there is enough margin to absorb both:
		 *   (a) the +sd shift from reducing one slot ahead, AND
		 *   (b) 4× jitter deviation as safety margin.
		 *
		 * After decrease, compensated EWMA becomes:
		 *   EWMA' = EWMA + sd > -(4*Dev)
		 * which still satisfies the "early" zone with margin.
		 */
		target -= 1;
	}

	/* Clamp to [2, max_ahead] */
	int32_t max_ahead = (int32_t)(p7_info->timing_window / sd) - 1;
	if (max_ahead > 8) max_ahead = 8;
	if (max_ahead < 2) max_ahead = 2;
	if (target > max_ahead) target = max_ahead;
	if (target < 2) target = 2;

	/* ===== Apply adjustment ===== */
	if (target != p7_info->slot_ahead) {
		int32_t delta = target - p7_info->slot_ahead;
		/* Compensate EWMA mean for the shift in reference frame */
		p7_info->estimated_mean_late -= delta * sd;
		p7_info->slot_ahead = target;
		p7_info->last_adjustment_sfn = p7_info->sfn;
		p7_info->last_adjustment_slot = p7_info->slot;
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

	pthread_mutex_lock(&p7_con->mutex);
	if (!p7_con->initial_timing_info_received) {
		p7_con->sfn = ind.last_sfn;
		p7_con->slot = ind.last_slot;
		p7_con->initial_timing_info_received = 1;
	}
	pthread_cond_signal(&p7_con->initial_timinginfo_cond);
	pthread_mutex_unlock(&p7_con->mutex);
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
