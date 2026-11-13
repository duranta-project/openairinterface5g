/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#include "pnf_p7.h"
#include <stdlib.h>
#include <string.h>

int nfapi_pnf_p7_start(nfapi_pnf_p7_config_t* config)
{
  // Verify that config is not null
  if (config == 0)
    return -1;

  pnf_p7_t* _this = (pnf_p7_t*)(config);

  NFAPI_TRACE(NFAPI_TRACE_INFO, "%s\n", __FUNCTION__);

  pnf_p7_message_pump(_this);

  return 0;
}

int nfapi_pnf_p7_subframe_ind(nfapi_pnf_p7_config_t* config, uint16_t phy_id, uint16_t sfn_sf)
{
  // Verify that config is not null
  if (config == 0)
    return -1;

  pnf_p7_t* _this = (pnf_p7_t*)(config);

  return pnf_p7_subframe_ind(_this, phy_id, sfn_sf);
}

int nfapi_pnf_p7_harq_ind(nfapi_pnf_p7_config_t* config, nfapi_harq_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return -1;
  }

  pnf_p7_t* _this = (pnf_p7_t*)(config);

  return pnf_p7_pack_and_send_p7_message(_this, (nfapi_p7_message_header_t*)ind, sizeof(nfapi_harq_indication_t));
}

int nfapi_pnf_p7_crc_ind(nfapi_pnf_p7_config_t* config, nfapi_crc_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return -1;
  }

  pnf_p7_t* _this = (pnf_p7_t*)(config);
  return pnf_p7_pack_and_send_p7_message(_this, (nfapi_p7_message_header_t*)ind, sizeof(nfapi_crc_indication_t));
}

int nfapi_pnf_p7_rx_ind(nfapi_pnf_p7_config_t* config, nfapi_rx_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return -1;
  }

  pnf_p7_t* _this = (pnf_p7_t*)(config);
  return pnf_p7_pack_and_send_p7_message(_this, (nfapi_p7_message_header_t*)ind, sizeof(nfapi_rx_indication_t));
}

int nfapi_pnf_p7_rach_ind(nfapi_pnf_p7_config_t* config, nfapi_rach_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return -1;
  }

  pnf_p7_t* _this = (pnf_p7_t*)(config);
  return pnf_p7_pack_and_send_p7_message(_this, (nfapi_p7_message_header_t*)ind, sizeof(nfapi_rach_indication_t));
}

int nfapi_pnf_p7_sr_ind(nfapi_pnf_p7_config_t* config, nfapi_sr_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return -1;
  }

  pnf_p7_t* _this = (pnf_p7_t*)(config);
  return pnf_p7_pack_and_send_p7_message(_this, (nfapi_p7_message_header_t*)ind, sizeof(nfapi_sr_indication_t));
}

int nfapi_pnf_p7_cqi_ind(nfapi_pnf_p7_config_t* config, nfapi_cqi_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return -1;
  }

  pnf_p7_t* _this = (pnf_p7_t*)(config);
  return pnf_p7_pack_and_send_p7_message(_this, (nfapi_p7_message_header_t*)ind, sizeof(nfapi_cqi_indication_t));
}

int nfapi_pnf_p7_lbt_dl_ind(nfapi_pnf_p7_config_t* config, nfapi_lbt_dl_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return -1;
  }

  pnf_p7_t* _this = (pnf_p7_t*)(config);
  return pnf_p7_pack_and_send_p7_message(_this, (nfapi_p7_message_header_t*)ind, sizeof(nfapi_lbt_dl_indication_t));
}

int nfapi_pnf_p7_nb_harq_ind(nfapi_pnf_p7_config_t* config, nfapi_nb_harq_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return -1;
  }

  pnf_p7_t* _this = (pnf_p7_t*)(config);
  return pnf_p7_pack_and_send_p7_message(_this, (nfapi_p7_message_header_t*)ind, sizeof(nfapi_nb_harq_indication_t));
}

int nfapi_pnf_p7_nrach_ind(nfapi_pnf_p7_config_t* config, nfapi_nrach_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return -1;
  }

  pnf_p7_t* _this = (pnf_p7_t*)(config);
  return pnf_p7_pack_and_send_p7_message(_this, (nfapi_p7_message_header_t*)ind, sizeof(nfapi_nrach_indication_t));
}

int nfapi_pnf_p7_vendor_extension(nfapi_pnf_p7_config_t* config, nfapi_p7_message_header_t* msg)
{
  if (config == NULL || msg == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return -1;
  }

  pnf_p7_t* _this = (pnf_p7_t*)(config);
  return pnf_p7_pack_and_send_p7_message(_this, msg, 0);
}

int nfapi_pnf_ue_release_resp(nfapi_pnf_p7_config_t* config, nfapi_ue_release_response_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_p7_t* _this = (pnf_p7_t*)(config);

  return pnf_p7_pack_and_send_p7_message(_this, &(resp->header), sizeof(nfapi_ue_release_response_t));
}
