/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nfapi_nr_interface.h"
#include "nfapi_nr_interface_scf.h"
#include "pnf.h"
#include "nfapi/open-nFAPI/fapi/inc/nr_fapi.h"
#include "common/platform_types.h"
#include "common/platform_constants.h"
#include "common/ran_context.h"
#include "common/utils/LOG/log.h"

void pnf_nr_handle_pnf_param_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_nr_pnf_param_request_t req;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "PNF_PARAM.request received\n");

    /// The unpack() function pointer is not called here due to the only possibility of entering being on a nFAPI message
    const bool result =
        nfapi_nr_p5_message_unpack(pRecvMsg, recvMsgLen, &req, sizeof(nfapi_nr_pnf_param_request_t), &pnf->_nr_public.codec_config);
    if (result) {
      if (pnf->_nr_public.state == NFAPI_PNF_IDLE) {
        if (pnf->_nr_public.pnf_nr_param_req) {
          (pnf->_nr_public.pnf_nr_param_req)(&pnf->_nr_public, &req);
        }
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: PNF not in IDLE state\n", __FUNCTION__);

        nfapi_nr_pnf_param_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_PNF_PARAM_RESPONSE;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_nr_pnf_pnf_param_resp(&pnf->_nr_public, &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_nr_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_nr_handle_pnf_config_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_nr_pnf_config_request_t req;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "PNF_CONFIG.request received\n");

    /// The unpack() function pointer is not called here due to the only possibility of entering being on a nFAPI message
    const bool result = nfapi_nr_p5_message_unpack(pRecvMsg, recvMsgLen, &req, sizeof(req), &pnf->_nr_public.codec_config);
    if (result) {
      if (pnf->_nr_public.state != NFAPI_PNF_RUNNING) {
        nfapi_pnf_phy_config_delete_all((nfapi_pnf_config_t*)&pnf->_nr_public);

        if (req.pnf_phy_rf_config.tl.tag == NFAPI_PNF_PHY_RF_TAG) {
          for (int i = 0; i < req.pnf_phy_rf_config.number_phy_rf_config_info; ++i) {
            nfapi_pnf_phy_config_t* phy = calloc_or_fail(1, sizeof(nfapi_pnf_phy_config_t));
            phy->state = NFAPI_PNF_PHY_IDLE;
            phy->phy_id = req.pnf_phy_rf_config.phy_rf_config[i].phy_id;
            nfapi_pnf_phy_config_add((nfapi_pnf_config_t*)&pnf->_nr_public, phy);
          }
        }

        if (pnf->_nr_public.pnf_nr_config_req) {
          (pnf->_nr_public.pnf_nr_config_req)(&pnf->_nr_public, &req);
        }
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: PNF not in correct state: %d\n", __FUNCTION__, pnf->_nr_public.state);

        nfapi_nr_pnf_config_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_PNF_CONFIG_RESPONSE;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_nr_pnf_pnf_config_resp(&pnf->_nr_public, &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_nr_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_nr_handle_pnf_start_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_nr_pnf_start_request_t req;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "PNF_START.request Received\n");

    /// The unpack() function pointer is not called here due to the only possibility of entering being on a nFAPI message
    const bool result = nfapi_nr_p5_message_unpack(pRecvMsg, recvMsgLen, &req, sizeof(req), &pnf->_nr_public.codec_config);
    if (result) {
      if (pnf->_nr_public.state == NFAPI_PNF_CONFIGURED) {
        if (pnf->_nr_public.pnf_nr_start_req) {
          (pnf->_nr_public.pnf_nr_start_req)(&pnf->_nr_public, &req);
        }
      } else {
        nfapi_nr_pnf_start_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_PNF_START_RESPONSE;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_nr_pnf_pnf_start_resp(&pnf->_nr_public, &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_nr_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_nr_handle_param_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_nr_param_request_scf_t req;

    nfapi_nr_pnf_config_t* config = &pnf->_nr_public;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "PARAM.request received\n");

    const bool result = config->unpack_func(pRecvMsg, recvMsgLen, &req, sizeof(req), &config->codec_config);
    if (result) {
      if (config->state == NFAPI_PNF_RUNNING) {
        nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find((nfapi_pnf_config_t*)config, req.header.phy_id);
        if (phy) {
          if (phy->state == NFAPI_PNF_PHY_IDLE) {
            if (config->nr_param_req) {
              (config->nr_param_req)(config, phy, &req);
            }
          } else {
            nfapi_nr_param_response_scf_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE;
            resp.header.phy_id = req.header.phy_id;
            resp.error_code = NFAPI_MSG_INVALID_STATE;
            nfapi_nr_pnf_param_resp(config, &resp);
          }
        } else {
          nfapi_nr_param_response_scf_t resp;
          memset(&resp, 0, sizeof(resp));
          resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE;
          resp.header.phy_id = req.header.phy_id;
          resp.error_code = NFAPI_MSG_INVALID_CONFIG;
          nfapi_nr_pnf_param_resp(config, &resp);
        }
      } else {
        nfapi_nr_param_response_scf_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_nr_pnf_param_resp(config, &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_nr_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_nr_handle_config_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_nr_config_request_scf_t req;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "CONFIG.request received\n");

    nfapi_nr_pnf_config_t* config = &pnf->_nr_public;

    const bool result = config->unpack_func(pRecvMsg, recvMsgLen, &req, sizeof(req), &config->codec_config);
    if (result) {
      if (config->state == NFAPI_PNF_RUNNING) {
        nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find((nfapi_pnf_config_t*)config, req.header.phy_id);

        if (phy) {
          if (phy->state != NFAPI_PNF_PHY_RUNNING) {
            if (config->nr_config_req) {
              (config->nr_config_req)(config, phy, &req);
            }
          } else {
            nfapi_nr_config_response_scf_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.message_id = NFAPI_CONFIG_RESPONSE;
            resp.header.phy_id = req.header.phy_id;
            resp.error_code = NFAPI_MSG_INVALID_STATE;
            nfapi_nr_pnf_config_resp(config, &resp);
          }
        } else {
          nfapi_nr_config_response_scf_t resp;
          memset(&resp, 0, sizeof(resp));
          resp.header.message_id = NFAPI_CONFIG_RESPONSE;
          resp.header.phy_id = req.header.phy_id;
          resp.error_code = NFAPI_MSG_INVALID_CONFIG;
          nfapi_nr_pnf_config_resp(config, &resp);
        }
      } else {
        nfapi_nr_config_response_scf_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_CONFIG_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_nr_pnf_config_resp(config, &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_nr_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_nr_handle_start_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_nr_start_request_scf_t req;

    nfapi_nr_pnf_config_t* config = &pnf->_nr_public;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "%s() START.request received state:%d\n", __FUNCTION__, config->state);

    const bool result = config->unpack_func(pRecvMsg, recvMsgLen, &req, sizeof(req), &config->codec_config);
    if (result) {
      if (config->state == NFAPI_PNF_RUNNING) {
        nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find((nfapi_pnf_config_t*)config, req.header.phy_id);
        if (phy) {
          if (phy->state != NFAPI_PNF_PHY_RUNNING) {
            if (config->nr_start_req) {
              (config->nr_start_req)(config, phy, &req);
            }
          } else {
            nfapi_nr_start_response_scf_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE;
            resp.header.phy_id = req.header.phy_id;
            resp.error_code = NFAPI_NR_START_MSG_INVALID_STATE;
            nfapi_nr_pnf_start_resp(config, &resp);
          }
        } else {
          nfapi_nr_start_response_scf_t resp;
          memset(&resp, 0, sizeof(resp));
          resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE;
          resp.header.phy_id = req.header.phy_id;
          resp.error_code = NFAPI_NR_START_MSG_INVALID_STATE;
          nfapi_nr_pnf_start_resp(config, &resp);
        }
      } else {
        nfapi_nr_start_response_scf_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_NR_START_MSG_INVALID_STATE;
        nfapi_nr_pnf_start_resp(config, &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_nr_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_nr_handle_stop_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_nr_stop_request_scf_t req;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "STOP.request received\n");

    nfapi_nr_pnf_config_t* config = &pnf->_nr_public;

    const bool result = config->unpack_func(pRecvMsg, recvMsgLen, &req, sizeof(req), &config->codec_config);
    if (result) {
      if (config->state == NFAPI_PNF_RUNNING) {
        nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find((nfapi_pnf_config_t*)config, req.header.phy_id);
        if (phy) {
          if (phy->state != NFAPI_PNF_PHY_RUNNING) {
            if (config->nr_stop_req) {
              (config->nr_stop_req)(config, phy, &req);
            }
          } else {
            nfapi_stop_response_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_STOP_RESPONSE;
            resp.header.phy_id = req.header.phy_id;
            resp.error_code = NFAPI_MSG_INVALID_STATE;
            pnf_nr_pack_and_send_p5_message(pnf, (nfapi_p4_p5_message_header_t*)&resp, sizeof(resp));
          }
        } else {
          nfapi_stop_response_t resp;
          memset(&resp, 0, sizeof(resp));
          resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_STOP_RESPONSE;
          resp.header.phy_id = req.header.phy_id;
          resp.error_code = NFAPI_MSG_INVALID_CONFIG;
          pnf_nr_pack_and_send_p5_message(pnf, (nfapi_p4_p5_message_header_t*)&resp, sizeof(resp));
        }
      } else {
        nfapi_stop_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_STOP_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        pnf_nr_pack_and_send_p5_message(pnf, (nfapi_p4_p5_message_header_t*)&resp, sizeof(resp));
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_nr_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_nr_handle_vendor_extension(void* pRecvMsg, int recvMsgLen, pnf_t* pnf, uint16_t message_id)
{
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
    return;
  }

  nfapi_nr_pnf_config_t* config = &pnf->_nr_public;

  if (config->allocate_p4_p5_vendor_ext) {
    uint16_t msg_size;
    nfapi_p4_p5_message_header_t* msg = config->allocate_p4_p5_vendor_ext(message_id, &msg_size);
    if (!msg) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "failed to allocate vendor extension structure\n");
      return;
    }

    int unpack_result = nfapi_nr_p5_message_unpack(pRecvMsg, recvMsgLen, msg, msg_size, &config->codec_config);
    if (unpack_result == 0) {
      if (config->vendor_ext)
        config->vendor_ext(config, msg);
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (config->deallocate_p4_p5_vendor_ext)
      config->deallocate_p4_p5_vendor_ext(msg);
  }
}

void pnf_nr_handle_p5_message(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  nfapi_nr_p4_p5_message_header_t messageHeader;

  if (pRecvMsg == NULL || recvMsgLen < NFAPI_HEADER_LENGTH) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return;
  }

  const bool result =
      pnf->_nr_public.hdr_unpack_func(pRecvMsg, recvMsgLen, &messageHeader, sizeof(messageHeader), &pnf->_nr_public.codec_config);
  if (!result) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Unpack message header failed, ignoring\n");
    return;
  }

  switch (messageHeader.message_id) {
    case NFAPI_NR_PHY_MSG_TYPE_PNF_PARAM_REQUEST:
      pnf_nr_handle_pnf_param_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_PNF_CONFIG_REQUEST:
      pnf_nr_handle_pnf_config_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_PNF_START_REQUEST:
      pnf_nr_handle_pnf_start_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_PNF_STOP_REQUEST:
      NFAPI_TRACE(NFAPI_TRACE_WARN, "%s: PNF_STOP_REQUEST not supported in NR mode, ignoring\n", __FUNCTION__);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_PARAM_REQUEST:
      pnf_nr_handle_param_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_REQUEST:
      pnf_nr_handle_config_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_START_REQUEST:
      pnf_nr_handle_start_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_STOP_REQUEST:
      pnf_nr_handle_stop_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_MEASUREMENT_REQUEST:
    case NFAPI_RSSI_REQUEST:
    case NFAPI_CELL_SEARCH_REQUEST:
    case NFAPI_BROADCAST_DETECT_REQUEST:
    case NFAPI_SYSTEM_INFORMATION_SCHEDULE_REQUEST:
    case NFAPI_SYSTEM_INFORMATION_REQUEST:
    case NFAPI_NMM_STOP_REQUEST:
      NFAPI_TRACE(NFAPI_TRACE_WARN,
                  "%s: LTE-only message 0x%x received in NR mode, ignoring\n",
                  __FUNCTION__,
                  messageHeader.message_id);
      break;

    default: {
      if (messageHeader.message_id >= NFAPI_VENDOR_EXT_MSG_MIN && messageHeader.message_id <= NFAPI_VENDOR_EXT_MSG_MAX) {
        pnf_nr_handle_vendor_extension(pRecvMsg, recvMsgLen, pnf, messageHeader.message_id);
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s P5 Unknown message ID %d\n", __FUNCTION__, messageHeader.message_id);
      }
    } break;
  }
}
