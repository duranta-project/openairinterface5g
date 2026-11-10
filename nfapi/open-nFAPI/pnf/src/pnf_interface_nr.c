/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#include "pnf.h"
#include "nfapi_nr_interface.h"
#include "nfapi_nr_interface_scf.h"

int nfapi_nr_pnf_pnf_param_resp(nfapi_pnf_config_t* config, nfapi_nr_pnf_param_response_t* resp)
{
  // ensure it's valid
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);
  AssertFatal(config->send_p5_msg, "Function pointer must be configured!");
  return config->send_p5_msg(_this, &(resp->header), sizeof(nfapi_nr_pnf_param_response_t));
}

int nfapi_nr_pnf_pnf_config_resp(nfapi_pnf_config_t* config, nfapi_nr_pnf_config_response_t* resp)
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
  AssertFatal(config->send_p5_msg, "Function pointer must be configured|");
  return config->send_p5_msg(_this, &(resp->header), sizeof(nfapi_nr_pnf_config_response_t));
}

int nfapi_nr_pnf_pnf_start_resp(nfapi_pnf_config_t* config, nfapi_nr_pnf_start_response_t* resp)
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
  AssertFatal(config->send_p5_msg, "Function pointer must be configured|");
  return config->send_p5_msg(_this, &(resp->header), sizeof(nfapi_nr_pnf_start_response_t));
}

int nfapi_nr_pnf_param_resp(nfapi_pnf_config_t* config, nfapi_nr_param_response_scf_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);
  AssertFatal(config->send_p5_msg, "Function pointer must be configured|");
  return config->send_p5_msg(_this, &(resp->header), sizeof(nfapi_nr_param_response_scf_t));
}

int nfapi_nr_pnf_config_resp(nfapi_pnf_config_t* config, nfapi_nr_config_response_scf_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);

  nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find(config, resp->header.phy_id);

  if (phy) {
    if (resp->error_code == NFAPI_NR_CONFIG_MSG_OK) {
      phy->state = NFAPI_PNF_PHY_CONFIGURED;
    }
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: unknow phy id %d\n", __FUNCTION__, resp->header.phy_id);
    return -1;
  }
  AssertFatal(config->send_p5_msg, "Function pointer must be configured|");
  return config->send_p5_msg(_this, &(resp->header), sizeof(nfapi_nr_config_response_scf_t));
}

int nfapi_nr_pnf_start_resp(nfapi_pnf_config_t* config, nfapi_nr_start_response_scf_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);

  nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find(config, resp->header.phy_id);
  if (phy) {
    if (resp->error_code == NFAPI_NR_START_MSG_OK) {
      phy->state = NFAPI_PNF_PHY_RUNNING;
    }
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: unknown phy id %d\n", __FUNCTION__, resp->header.phy_id);
    return -1;
  }
  AssertFatal(config->send_p5_msg, "Function pointer must be configured|");
  return config->send_p5_msg(_this, &(resp->header), sizeof(nfapi_nr_start_response_scf_t));
}

int nfapi_nr_stop_indication(nfapi_pnf_config_t* config, nfapi_nr_stop_indication_scf_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);
  AssertFatal(config->send_p5_msg, "Function pointer must be configured|");
  return config->send_p5_msg(_this, &(resp->header), sizeof(nfapi_nr_stop_indication_scf_t));
}
