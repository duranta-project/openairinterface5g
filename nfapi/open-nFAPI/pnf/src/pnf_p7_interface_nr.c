/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#include "pnf_p7.h"
#include <stdlib.h>
#include <string.h>

int nfapi_nr_pnf_p7_start(nfapi_nr_pnf_p7_config_t* config)
{
  if (!config)
    return -1;
  pnf_p7_t* _this = PNF_P7_FROM_NR_CONFIG(config);
  return pnf_nr_p7_message_pump(_this);
}

int nfapi_nr_pnf_p7_get_msgs(nfapi_nr_pnf_p7_config_t* config,
                             uint16_t phy_id,
                             uint16_t sfn,
                             uint16_t slot,
                             nfapi_nr_dl_tti_request_t* ret_dl_tti,
                             nfapi_nr_ul_tti_request_t* ret_ul_tti,
                             nfapi_nr_ul_dci_request_t* ret_ul_dci,
                             nfapi_nr_tx_data_request_t* ret_tx_data)
{
  if (config == NULL)
    return -1;

  pnf_p7_t* _this = PNF_P7_FROM_NR_CONFIG(config);

  return nr_pnf_p7_get_msgs(_this, phy_id, sfn, slot, ret_dl_tti, ret_ul_tti, ret_ul_dci, ret_tx_data);
}

//NR UPLINK INDICATION

int nfapi_pnf_p7_nr_slot_ind(nfapi_nr_pnf_p7_config_t* config, nfapi_nr_slot_indication_scf_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return -1;
  }

  pnf_p7_t* _this = PNF_P7_FROM_NR_CONFIG(config);
  AssertFatal(_this->_nr_public.send_p7_msg, "Function pointer must be configured|");
  return _this->_nr_public.send_p7_msg(_this, (nfapi_nr_p7_message_header_t*)ind, sizeof(nfapi_nr_slot_indication_scf_t));
}

int nfapi_pnf_p7_nr_rx_data_ind(nfapi_nr_pnf_p7_config_t* config, nfapi_nr_rx_data_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return -1;
  }

  pnf_p7_t* _this = PNF_P7_FROM_NR_CONFIG(config);
  AssertFatal(_this->_nr_public.send_p7_msg, "Function pointer must be configured|");
  bool ret = _this->_nr_public.send_p7_msg(_this, (nfapi_nr_p7_message_header_t*)ind, sizeof(nfapi_nr_rx_data_indication_t));
  if (ret) {
    for (int i = 0; i < ind->number_of_pdus; ++i)
      _this->nr_stats.ul.bytes += ind->pdu_list[i].pdu_length;
  }
  return ret;
}

int nfapi_pnf_p7_nr_crc_ind(nfapi_nr_pnf_p7_config_t* config, nfapi_nr_crc_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return -1;
  }

  pnf_p7_t* _this = PNF_P7_FROM_NR_CONFIG(config);
  AssertFatal(_this->_nr_public.send_p7_msg, "Function pointer must be configured|");
  return _this->_nr_public.send_p7_msg(_this, (nfapi_nr_p7_message_header_t*)ind, sizeof(nfapi_nr_crc_indication_t));
}

int nfapi_pnf_p7_nr_srs_ind(nfapi_nr_pnf_p7_config_t* config, nfapi_nr_srs_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return -1;
  }

  pnf_p7_t* _this = PNF_P7_FROM_NR_CONFIG(config);
  AssertFatal(_this->_nr_public.send_p7_msg, "Function pointer must be configured|");
  return _this->_nr_public.send_p7_msg(_this, (nfapi_nr_p7_message_header_t*)ind, sizeof(nfapi_nr_srs_indication_t));
}

int nfapi_pnf_p7_nr_srs_toa_vendor_ext_ind(nfapi_nr_pnf_p7_config_t* config, nfapi_nr_srs_toa_vendor_ext_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return -1;
  }

  pnf_p7_t* _this = PNF_P7_FROM_NR_CONFIG(config);
  AssertFatal(_this->_nr_public.send_p7_msg, "Function pointer must be configured|");
  return _this->_nr_public.send_p7_msg(_this, (nfapi_nr_p7_message_header_t*)ind, sizeof(nfapi_nr_srs_toa_vendor_ext_indication_t));
}

int nfapi_pnf_p7_nr_uci_ind(nfapi_nr_pnf_p7_config_t* config, nfapi_nr_uci_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return -1;
  }

  pnf_p7_t* _this = PNF_P7_FROM_NR_CONFIG(config);
  AssertFatal(_this->_nr_public.send_p7_msg, "Function pointer must be configured|");
  return _this->_nr_public.send_p7_msg(_this, (nfapi_nr_p7_message_header_t*)ind, sizeof(nfapi_nr_uci_indication_t));
}

int nfapi_pnf_p7_nr_rach_ind(nfapi_nr_pnf_p7_config_t* config, nfapi_nr_rach_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return -1;
  }

  pnf_p7_t* _this = PNF_P7_FROM_NR_CONFIG(config);
  AssertFatal(_this->_nr_public.send_p7_msg, "Function pointer must be configured|");
  return _this->_nr_public.send_p7_msg(_this, (nfapi_nr_p7_message_header_t*)ind, sizeof(nfapi_nr_rach_indication_t));
}
