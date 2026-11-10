/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#include "pnf.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

nfapi_pnf_config_t* nfapi_pnf_config_create()
{
  pnf_t* _this = (pnf_t*)malloc(sizeof(pnf_t));

  if (_this == 0)
    return 0;

  memset(_this, 0, sizeof(pnf_t));

  _this->sctp = 1; // enable sctp

  _this->_public.vnf_p5_port = NFAPI_P5_SCTP_PORT;

  _this->_public.malloc = &malloc;
  _this->_public.free = &free;

  _this->_public.codec_config.allocate = &malloc;
  _this->_public.codec_config.deallocate = &free;

  return (nfapi_pnf_config_t*)_this;
}

void nfapi_pnf_config_destory(nfapi_pnf_config_t* config)
{
  free(config);
}

int nfapi_pnf_start(nfapi_pnf_config_t* config)
{
  // Verify that config is not null
  if (config == 0)
    return -1;

  NFAPI_TRACE(NFAPI_TRACE_INFO, "%s\n", __FUNCTION__);

  pnf_t* _this = (pnf_t*)(config);

  while (_this->terminate == 0) {
    int connect_result = pnf_connect(_this);

    if (connect_result > 0) {
      pnf_message_pump(_this);
    } else if (connect_result < 0) {
      return connect_result;
    }

    sleep(1);
  }
  NFAPI_TRACE(NFAPI_TRACE_INFO, "%s() terminate=1 - EXITTING............\n", __FUNCTION__);

  return 0;
}

int nfapi_pnf_stop(nfapi_pnf_config_t* config)
{
  // Verify that config is not null
  if (config == 0)
    return -1;

  pnf_t* _this = (pnf_t*)(config);
  _this->terminate = 1;

  // todo wait for the pnf to stop before returning

  return 0;
}

int nfapi_pnf_vendor_extension(nfapi_pnf_config_t* config, nfapi_p4_p5_message_header_t* msg, uint32_t msg_len)
{
  // ensure it's valid
  if (config == NULL || msg == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return -1;
  }

  pnf_t* _this = (pnf_t*)(config);

  return pnf_pack_and_send_p5_message(_this, msg, msg_len);
}
