/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#include "pnf.h"
#include "nfapi_nr_interface.h"
#include "nfapi_nr_interface_scf.h"
#include <unistd.h>

int pnf_nr_pack_and_send_p5_message(pnf_t* pnf, nfapi_p4_p5_message_header_t* msg, uint32_t msg_len)
{
  int packed_len =
      nfapi_nr_p5_message_pack(msg, msg_len, pnf->tx_message_buffer, sizeof(pnf->tx_message_buffer), &pnf->_nr_public.codec_config);
  if (packed_len < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "nfapi_nr_p5_message_pack failed (%d)\n", packed_len);
    return -1;
  }
  return pnf_send_message(pnf, pnf->tx_message_buffer, packed_len, 0);
}

int nfapi_nr_pnf_start(nfapi_nr_pnf_config_t* config)
{
  if (!config)
    return -1;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "%s\n", __FUNCTION__);
  pnf_t* _this = PNF_FROM_NR_CONFIG(config);
  /* pnf_connect() reads _public.vnf_ip_addr and _public.vnf_p5_port */
  _this->_public.vnf_ip_addr = config->vnf_ip_addr;
  _this->_public.vnf_p5_port = config->vnf_p5_port;
  while (_this->terminate == 0) {
    int connect_result = pnf_connect(_this);
    if (connect_result > 0)
      pnf_nr_message_pump(_this);
    else if (connect_result < 0)
      return connect_result;
    sleep(1);
  }
  NFAPI_TRACE(NFAPI_TRACE_INFO, "%s() terminate=1 - EXITTING............\n", __FUNCTION__);
  return 0;
}

int nfapi_nr_pnf_vendor_extension(nfapi_nr_pnf_config_t* config, nfapi_p4_p5_message_header_t* msg, uint32_t msg_len)
{
  if (config == NULL || msg == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }
  return pnf_nr_pack_and_send_p5_message(PNF_FROM_NR_CONFIG(config), msg, msg_len);
}

int nfapi_nr_pnf_pnf_param_resp(nfapi_nr_pnf_config_t* config, nfapi_nr_pnf_param_response_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = PNF_FROM_NR_CONFIG(config);
  AssertFatal(config->send_p5_msg, "Function pointer must be configured!");
  return config->send_p5_msg(_this, &(resp->header), sizeof(nfapi_nr_pnf_param_response_t));
}

int nfapi_nr_pnf_pnf_config_resp(nfapi_nr_pnf_config_t* config, nfapi_nr_pnf_config_response_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  if (resp->error_code == NFAPI_MSG_OK)
    config->state = NFAPI_PNF_CONFIGURED;

  pnf_t* _this = PNF_FROM_NR_CONFIG(config);
  AssertFatal(config->send_p5_msg, "Function pointer must be configured|");
  return config->send_p5_msg(_this, &(resp->header), sizeof(nfapi_nr_pnf_config_response_t));
}

int nfapi_nr_pnf_pnf_start_resp(nfapi_nr_pnf_config_t* config, nfapi_nr_pnf_start_response_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  if (resp->error_code == NFAPI_MSG_OK)
    config->state = NFAPI_PNF_RUNNING;

  pnf_t* _this = PNF_FROM_NR_CONFIG(config);
  AssertFatal(config->send_p5_msg, "Function pointer must be configured|");
  return config->send_p5_msg(_this, &(resp->header), sizeof(nfapi_nr_pnf_start_response_t));
}

int nfapi_nr_pnf_param_resp(nfapi_nr_pnf_config_t* config, nfapi_nr_param_response_scf_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = PNF_FROM_NR_CONFIG(config);
  AssertFatal(config->send_p5_msg, "Function pointer must be configured|");
  return config->send_p5_msg(_this, &(resp->header), sizeof(nfapi_nr_param_response_scf_t));
}

int nfapi_nr_pnf_config_resp(nfapi_nr_pnf_config_t* config, nfapi_nr_config_response_scf_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = PNF_FROM_NR_CONFIG(config);

  /* cast via common initial sequence: phys is at same offset in both structs */
  nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find((nfapi_pnf_config_t*)config, resp->header.phy_id);
  if (phy) {
    if (resp->error_code == NFAPI_NR_CONFIG_MSG_OK)
      phy->state = NFAPI_PNF_PHY_CONFIGURED;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: unknow phy id %d\n", __FUNCTION__, resp->header.phy_id);
    return -1;
  }
  AssertFatal(config->send_p5_msg, "Function pointer must be configured|");
  return config->send_p5_msg(_this, &(resp->header), sizeof(nfapi_nr_config_response_scf_t));
}

int nfapi_nr_pnf_start_resp(nfapi_nr_pnf_config_t* config, nfapi_nr_start_response_scf_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = PNF_FROM_NR_CONFIG(config);

  /* cast via common initial sequence: phys is at same offset in both structs */
  nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find((nfapi_pnf_config_t*)config, resp->header.phy_id);
  if (phy) {
    if (resp->error_code == NFAPI_NR_START_MSG_OK)
      phy->state = NFAPI_PNF_PHY_RUNNING;
  } else {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: unknown phy id %d\n", __FUNCTION__, resp->header.phy_id);
    return -1;
  }
  AssertFatal(config->send_p5_msg, "Function pointer must be configured|");
  return config->send_p5_msg(_this, &(resp->header), sizeof(nfapi_nr_start_response_scf_t));
}

int nfapi_nr_stop_indication(nfapi_nr_pnf_config_t* config, nfapi_nr_stop_indication_scf_t* resp)
{
  if (config == NULL || resp == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = PNF_FROM_NR_CONFIG(config);
  AssertFatal(config->send_p5_msg, "Function pointer must be configured|");
  return config->send_p5_msg(_this, &(resp->header), sizeof(nfapi_nr_stop_indication_scf_t));
}
