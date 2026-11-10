/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#include "pnf.h"
#include <stdlib.h>
#include <string.h>

int nfapi_pnf_pnf_param_resp(nfapi_pnf_config_t* config, nfapi_pnf_param_response_t* resp)
{
  // ensure it's valid
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);

  return pnf_pack_and_send_p5_message(_this, &(resp->header), sizeof(nfapi_pnf_param_response_t));
}

int nfapi_pnf_pnf_config_resp(nfapi_pnf_config_t* config, nfapi_pnf_config_response_t* resp)
{
  // ensure it's valid
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  if (resp->error_code == NFAPI_MSG_OK) {
    config->state = NFAPI_PNF_CONFIGURED;
  }

  pnf_t* _this = (pnf_t*)(config);

  return pnf_pack_and_send_p5_message(_this, &(resp->header), sizeof(nfapi_pnf_config_response_t));
}

int nfapi_pnf_pnf_start_resp(nfapi_pnf_config_t* config, nfapi_pnf_start_response_t* resp)
{
  // ensure it's valid
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  if (resp->error_code == NFAPI_MSG_OK) {
    config->state = NFAPI_PNF_RUNNING;
  }

  pnf_t* _this = (pnf_t*)(config);

  return pnf_pack_and_send_p5_message(_this, &(resp->header), sizeof(nfapi_pnf_start_response_t));
}

int nfapi_pnf_pnf_stop_resp(nfapi_pnf_config_t* config, nfapi_pnf_stop_response_t* resp)
{
  // ensure it's valid
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  if (resp->error_code == NFAPI_MSG_OK) {
    config->state = NFAPI_PNF_CONFIGURED;
  }

  pnf_t* _this = (pnf_t*)(config);

  return pnf_pack_and_send_p5_message(_this, &(resp->header), sizeof(nfapi_pnf_stop_response_t));
}

int nfapi_pnf_param_resp(nfapi_pnf_config_t* config, nfapi_param_response_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);

  return pnf_pack_and_send_p5_message(_this, &(resp->header), sizeof(nfapi_param_response_t));
}

int nfapi_pnf_config_resp(nfapi_pnf_config_t* config, nfapi_config_response_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);

  nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find(config, resp->header.phy_id);

  if (phy) {
    if (resp->error_code == NFAPI_MSG_OK) {
      phy->state = NFAPI_PNF_PHY_CONFIGURED;
    }
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: unknow phy id %d\n", __FUNCTION__, resp->header.phy_id);
    return -1;
  }

  return pnf_pack_and_send_p5_message(_this, &(resp->header), sizeof(nfapi_config_response_t));
}

int nfapi_pnf_start_resp(nfapi_pnf_config_t* config, nfapi_start_response_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);

  nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find(config, resp->header.phy_id);
  if (phy) {
    if (resp->error_code == NFAPI_MSG_OK) {
      phy->state = NFAPI_PNF_PHY_RUNNING;
    }
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: unknown phy id %d\n", __FUNCTION__, resp->header.phy_id);
    return -1;
  }

  return pnf_pack_and_send_p5_message(_this, &(resp->header), sizeof(nfapi_start_response_t));
}

int nfapi_pnf_stop_resp(nfapi_pnf_config_t* config, nfapi_stop_response_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);

  nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find(config, resp->header.phy_id);
  if (phy) {
    if (resp->error_code == NFAPI_MSG_OK) {
      phy->state = NFAPI_PNF_PHY_CONFIGURED;
    }
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: unknow phy id %d\n", __FUNCTION__, resp->header.phy_id);
    return -1;
  }

  return pnf_pack_and_send_p5_message(_this, &(resp->header), sizeof(nfapi_stop_response_t));
}

int nfapi_pnf_measurement_resp(nfapi_pnf_config_t* config, nfapi_measurement_response_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);

  return pnf_pack_and_send_p5_message(_this, &(resp->header), sizeof(nfapi_measurement_response_t));
}

int nfapi_pnf_rssi_resp(nfapi_pnf_config_t* config, nfapi_rssi_response_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);

  return pnf_pack_and_send_p4_message(_this, &(resp->header), sizeof(nfapi_rssi_response_t));
}

int nfapi_pnf_rssi_ind(nfapi_pnf_config_t* config, nfapi_rssi_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);

  return pnf_pack_and_send_p4_message(_this, &(ind->header), sizeof(nfapi_rssi_indication_t));
}

int nfapi_pnf_cell_search_resp(nfapi_pnf_config_t* config, nfapi_cell_search_response_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  NFAPI_TRACE(NFAPI_TRACE_INFO, "Send CELL_SEARCH.response\n");

  pnf_t* _this = (pnf_t*)(config);

  return pnf_pack_and_send_p4_message(_this, &(resp->header), sizeof(nfapi_cell_search_response_t));
}

int nfapi_pnf_cell_search_ind(nfapi_pnf_config_t* config, nfapi_cell_search_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);

  return pnf_pack_and_send_p4_message(_this, &(ind->header), sizeof(nfapi_cell_search_indication_t));
}

int nfapi_pnf_broadcast_detect_resp(nfapi_pnf_config_t* config, nfapi_broadcast_detect_response_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);

  return pnf_pack_and_send_p4_message(_this, &(resp->header), sizeof(nfapi_broadcast_detect_response_t));
}

int nfapi_pnf_broadcast_detect_ind(nfapi_pnf_config_t* config, nfapi_broadcast_detect_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);

  return pnf_pack_and_send_p4_message(_this, &(ind->header), sizeof(nfapi_broadcast_detect_indication_t));
}

int nfapi_pnf_system_information_schedule_resp(nfapi_pnf_config_t* config, nfapi_system_information_schedule_response_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
    ;
  }

  pnf_t* _this = (pnf_t*)(config);

  return pnf_pack_and_send_p4_message(_this, &(resp->header), sizeof(nfapi_system_information_schedule_response_t));
}

int nfapi_pnf_system_information_schedule_ind(nfapi_pnf_config_t* config, nfapi_system_information_schedule_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);

  return pnf_pack_and_send_p4_message(_this, &(ind->header), sizeof(nfapi_system_information_schedule_indication_t));
}

int nfapi_pnf_system_information_resp(nfapi_pnf_config_t* config, nfapi_system_information_response_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);

  return pnf_pack_and_send_p4_message(_this, &(resp->header), sizeof(nfapi_system_information_response_t));
}

int nfapi_pnf_system_information_ind(nfapi_pnf_config_t* config, nfapi_system_information_indication_t* ind)
{
  if (config == NULL || ind == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);

  return pnf_pack_and_send_p4_message(_this, &(ind->header), sizeof(nfapi_system_information_indication_t));
}

int nfapi_pnf_nmm_stop_resp(nfapi_pnf_config_t* config, nfapi_nmm_stop_response_t* resp)
{
  // ensure it's valid
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);

  return pnf_pack_and_send_p4_message(_this, &(resp->header), sizeof(nfapi_nmm_stop_request_t));
}
