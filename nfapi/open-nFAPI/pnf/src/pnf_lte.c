/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "pnf.h"
#include "common/platform_types.h"
#include "common/platform_constants.h"
#include "common/ran_context.h"
#include "common/utils/LOG/log.h"

void pnf_handle_pnf_param_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  // ensure it's valid
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_pnf_param_request_t req;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "PNF_PARAM.request received\n");

    // unpack the message
    if (nfapi_p5_message_unpack(pRecvMsg, recvMsgLen, &req, sizeof(nfapi_pnf_param_request_t), &pnf->_public.codec_config) >= 0) {
      if (pnf->_public.state == NFAPI_PNF_IDLE) {
        if (pnf->_public.pnf_param_req) {
          (pnf->_public.pnf_param_req)(&pnf->_public, &req);
        }
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: PNF not in IDLE state\n", __FUNCTION__);

        nfapi_pnf_param_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_PNF_PARAM_RESPONSE;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_pnf_pnf_param_resp(&pnf->_public, &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_handle_pnf_config_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  // ensure it's valid
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_pnf_config_request_t req;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "PNF_CONFIG.request received\n");

    // unpack the message
    if (nfapi_p5_message_unpack(pRecvMsg, recvMsgLen, &req, sizeof(req), &pnf->_public.codec_config) >= 0) {
      // ensure correct state
      if (pnf->_public.state != NFAPI_PNF_RUNNING) {
        // delete the phy records
        nfapi_pnf_phy_config_delete_all(&pnf->_public);

        // create the phy records
        if (req.pnf_phy_rf_config.tl.tag == NFAPI_PNF_PHY_RF_TAG) {
          for (int i = 0; i < req.pnf_phy_rf_config.number_phy_rf_config_info; ++i) {
            nfapi_pnf_phy_config_t* phy = calloc_or_fail(1, sizeof(nfapi_pnf_phy_config_t));

            phy->state = NFAPI_PNF_PHY_IDLE;
            phy->phy_id = req.pnf_phy_rf_config.phy_rf_config[i].phy_id;

            nfapi_pnf_phy_config_add(&(pnf->_public), phy);
          }
        }

        if (pnf->_public.pnf_config_req) {
          (pnf->_public.pnf_config_req)(&(pnf->_public), &req);
        }
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: PNF not in correct state: %d\n", __FUNCTION__, pnf->_public.state);

        nfapi_pnf_config_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_PNF_CONFIG_RESPONSE;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_pnf_pnf_config_resp(&(pnf->_public), &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_handle_pnf_start_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  // ensure it's valid
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_pnf_start_request_t req;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "PNF_START.request Received\n");

    // unpack the message
    if (nfapi_p5_message_unpack(pRecvMsg, recvMsgLen, &req, sizeof(req), &pnf->_public.codec_config) >= 0) {
      if (pnf->_public.state == NFAPI_PNF_CONFIGURED) {
        if (pnf->_public.pnf_start_req) {
          (pnf->_public.pnf_start_req)(&(pnf->_public), &req);
        }
      } else {
        nfapi_pnf_start_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_PNF_START_RESPONSE;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_pnf_pnf_start_resp(&(pnf->_public), &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_handle_pnf_stop_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  // ensure it's valid
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_pnf_stop_request_t req;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "PNF_STOP.request Received\n");

    // unpack the message
    if (nfapi_p5_message_unpack(pRecvMsg, recvMsgLen, &req, sizeof(req), &pnf->_public.codec_config) >= 0) {
      if (pnf->_public.state == NFAPI_PNF_RUNNING) {
        if (pnf->_public.pnf_stop_req) {
          (pnf->_public.pnf_stop_req)(&(pnf->_public), &req);
        }
      } else {
        nfapi_pnf_stop_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_PNF_STOP_RESPONSE;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_pnf_pnf_stop_resp(&(pnf->_public), &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_handle_param_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  // ensure it's valid
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_param_request_t req;

    nfapi_pnf_config_t* config = &(pnf->_public);

    NFAPI_TRACE(NFAPI_TRACE_INFO, "PARAM.request received\n");

    // unpack the message
    if (nfapi_p5_message_unpack(pRecvMsg, recvMsgLen, &req, sizeof(req), &config->codec_config) >= 0) {
      if (config->state == NFAPI_PNF_RUNNING) {
        nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find(config, req.header.phy_id);
        if (phy) {
          if (phy->state == NFAPI_PNF_PHY_IDLE) {
            if (config->param_req) {
              (config->param_req)(config, phy, &req);
            }
          } else {
            nfapi_param_response_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.message_id = NFAPI_PARAM_RESPONSE;
            resp.header.phy_id = req.header.phy_id;
            resp.error_code = NFAPI_MSG_INVALID_STATE;
            nfapi_pnf_param_resp(config, &resp);
          }
        } else {
          nfapi_param_response_t resp;
          memset(&resp, 0, sizeof(resp));
          resp.header.message_id = NFAPI_PARAM_RESPONSE;
          resp.header.phy_id = req.header.phy_id;
          resp.error_code = NFAPI_MSG_INVALID_CONFIG;
          nfapi_pnf_param_resp(config, &resp);
        }

      } else {
        nfapi_param_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_PARAM_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_pnf_param_resp(config, &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_handle_config_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  // ensure it's valid
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_config_request_t req;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "CONFIG.request received\n");

    nfapi_pnf_config_t* config = &(pnf->_public);

    // unpack the message
    if (nfapi_p5_message_unpack(pRecvMsg, recvMsgLen, &req, sizeof(req), &config->codec_config) >= 0) {
      if (config->state == NFAPI_PNF_RUNNING) {
        nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find(config, req.header.phy_id);
        if (phy) {
          if (phy->state != NFAPI_PNF_PHY_RUNNING) {
            if (config->config_req) {
              (config->config_req)(config, phy, &req);
            }
          } else {
            nfapi_config_response_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.message_id = NFAPI_CONFIG_RESPONSE;
            resp.header.phy_id = req.header.phy_id;
            resp.error_code = NFAPI_MSG_INVALID_STATE;
            nfapi_pnf_config_resp(config, &resp);
          }
        } else {
          nfapi_config_response_t resp;
          memset(&resp, 0, sizeof(resp));
          resp.header.message_id = NFAPI_CONFIG_RESPONSE;
          resp.header.phy_id = req.header.phy_id;
          resp.error_code = NFAPI_MSG_INVALID_CONFIG;
          nfapi_pnf_config_resp(config, &resp);
        }
      } else {
        nfapi_config_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_CONFIG_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_pnf_config_resp(config, &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_handle_start_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  // ensure it's valid
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_start_request_t req;

    nfapi_pnf_config_t* config = &(pnf->_public);

    NFAPI_TRACE(NFAPI_TRACE_INFO, "%s() START.request received state:%d\n", __FUNCTION__, config->state);

    // unpack the message
    if (nfapi_p5_message_unpack(pRecvMsg, recvMsgLen, &req, sizeof(req), &config->codec_config) >= 0) {
      if (config->state == NFAPI_PNF_RUNNING) {
        nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find(config, req.header.phy_id);
        if (phy) {
          if (phy->state != NFAPI_PNF_PHY_RUNNING) {
            if (config->start_req) {
              (config->start_req)(config, phy, &req);
            }
          } else {
            nfapi_start_response_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.message_id = NFAPI_START_RESPONSE;
            resp.header.phy_id = req.header.phy_id;
            resp.error_code = NFAPI_MSG_INVALID_STATE;
            nfapi_pnf_start_resp(config, &resp);
          }
        } else {
          nfapi_start_response_t resp;
          memset(&resp, 0, sizeof(resp));
          resp.header.message_id = NFAPI_START_RESPONSE;
          resp.header.phy_id = req.header.phy_id;
          resp.error_code = NFAPI_MSG_INVALID_CONFIG;
          nfapi_pnf_start_resp(config, &resp);
        }
      } else {
        nfapi_start_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_START_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_pnf_start_resp(config, &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_handle_stop_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  // ensure it's valid
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_stop_request_t req;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "STOP.request received\n");

    nfapi_pnf_config_t* config = &(pnf->_public);

    // unpack the message
    if (nfapi_p5_message_unpack(pRecvMsg, recvMsgLen, &req, sizeof(req), &config->codec_config) >= 0) {
      if (config->state == NFAPI_PNF_RUNNING) {
        nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find(config, req.header.phy_id);
        if (phy) {
          if (phy->state != NFAPI_PNF_PHY_RUNNING) {
            if (config->stop_req) {
              (config->stop_req)(config, phy, &req);
            }
          } else {
            nfapi_stop_response_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.message_id = NFAPI_STOP_RESPONSE;
            resp.header.phy_id = req.header.phy_id;
            resp.error_code = NFAPI_MSG_INVALID_STATE;
            nfapi_pnf_stop_resp(config, &resp);
          }
        } else {
          nfapi_stop_response_t resp;
          memset(&resp, 0, sizeof(resp));
          resp.header.message_id = NFAPI_STOP_RESPONSE;
          resp.header.phy_id = req.header.phy_id;
          resp.error_code = NFAPI_MSG_INVALID_CONFIG;
          nfapi_pnf_stop_resp(config, &resp);
        }
      } else {
        nfapi_stop_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_STOP_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_pnf_stop_resp(config, &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_handle_measurement_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  // ensure it's valid
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_measurement_request_t req;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "MEASUREMENT.request received\n");

    nfapi_pnf_config_t* config = &(pnf->_public);

    // unpack the message
    if (nfapi_p5_message_unpack(pRecvMsg, recvMsgLen, &req, sizeof(req), &config->codec_config) >= 0) {
      if (config->state == NFAPI_PNF_RUNNING) {
        nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find(config, req.header.phy_id);
        if (phy) {
          if (phy->state != NFAPI_PNF_PHY_RUNNING) {
            if (config->measurement_req) {
              (config->measurement_req)(config, phy, &req);
            }
          } else {
            nfapi_measurement_response_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.message_id = NFAPI_MEASUREMENT_RESPONSE;
            resp.header.phy_id = req.header.phy_id;
            resp.error_code = NFAPI_MSG_INVALID_STATE;
            nfapi_pnf_measurement_resp(config, &resp);
          }
        } else {
          nfapi_measurement_response_t resp;
          memset(&resp, 0, sizeof(resp));
          resp.header.message_id = NFAPI_MEASUREMENT_RESPONSE;
          resp.header.phy_id = req.header.phy_id;
          resp.error_code = NFAPI_MSG_INVALID_CONFIG;
          nfapi_pnf_measurement_resp(config, &resp);
        }
      } else {
        nfapi_measurement_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_MEASUREMENT_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_pnf_measurement_resp(config, &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_handle_rssi_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_rssi_request_t req;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "RSSI.request received\n");

    nfapi_pnf_config_t* config = &(pnf->_public);

    // unpack the message
    if (nfapi_p4_message_unpack(pRecvMsg, recvMsgLen, &req, sizeof(req), &config->codec_config) >= 0) {
      if (config->state == NFAPI_PNF_RUNNING) {
        nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find(config, req.header.phy_id);
        if (phy) {
          if (phy->state == NFAPI_PNF_PHY_RUNNING) {
            if (config->rssi_req) {
              (config->rssi_req)(config, phy, &req);
            }
          } else {
            nfapi_rssi_response_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.message_id = NFAPI_RSSI_RESPONSE;
            resp.header.phy_id = req.header.phy_id;
            resp.error_code = NFAPI_MSG_INVALID_STATE;
            nfapi_pnf_rssi_resp(config, &resp);
          }
        } else {
          nfapi_rssi_response_t resp;
          memset(&resp, 0, sizeof(resp));
          resp.header.message_id = NFAPI_RSSI_RESPONSE;
          resp.header.phy_id = req.header.phy_id;
          resp.error_code = NFAPI_MSG_INVALID_CONFIG;
          nfapi_pnf_rssi_resp(config, &resp);
        }
      } else {
        nfapi_rssi_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_RSSI_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_pnf_rssi_resp(config, &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_handle_cell_search_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_cell_search_request_t req;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "CELL_SEARCH.request received\n");

    nfapi_pnf_config_t* config = &(pnf->_public);

    if (nfapi_p4_message_unpack(pRecvMsg, recvMsgLen, &req, sizeof(req), &config->codec_config) >= 0) {
      if (config->state == NFAPI_PNF_RUNNING) {
        nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find(config, req.header.phy_id);
        if (phy) {
          if (phy->state == NFAPI_PNF_PHY_RUNNING) {
            if (config->cell_search_req) {
              (config->cell_search_req)(config, phy, &req);
            }
          } else {
            nfapi_cell_search_response_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.message_id = NFAPI_CELL_SEARCH_RESPONSE;
            resp.header.phy_id = req.header.phy_id;
            resp.error_code = NFAPI_MSG_INVALID_STATE;
            nfapi_pnf_cell_search_resp(config, &resp);
          }
        } else {
          nfapi_cell_search_response_t resp;
          memset(&resp, 0, sizeof(resp));
          resp.header.message_id = NFAPI_CELL_SEARCH_RESPONSE;
          resp.header.phy_id = req.header.phy_id;
          resp.error_code = NFAPI_MSG_INVALID_CONFIG;
          nfapi_pnf_cell_search_resp(config, &resp);
        }
      } else {
        nfapi_cell_search_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_CELL_SEARCH_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_pnf_cell_search_resp(config, &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_handle_broadcast_detect_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_broadcast_detect_request_t req;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "BROADCAST_DETECT.request received\n");

    nfapi_pnf_config_t* config = &(pnf->_public);

    if (nfapi_p4_message_unpack(pRecvMsg, recvMsgLen, &req, sizeof(req), &config->codec_config) >= 0) {
      if (config->state == NFAPI_PNF_RUNNING) {
        nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find(config, req.header.phy_id);
        if (phy) {
          if (phy->state == NFAPI_PNF_PHY_RUNNING) {
            if (config->broadcast_detect_req) {
              (config->broadcast_detect_req)(config, phy, &req);
            }
          } else {
            nfapi_broadcast_detect_response_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.message_id = NFAPI_BROADCAST_DETECT_RESPONSE;
            resp.header.phy_id = req.header.phy_id;
            resp.error_code = NFAPI_MSG_INVALID_STATE;
            nfapi_pnf_broadcast_detect_resp(config, &resp);
          }
        } else {
          nfapi_broadcast_detect_response_t resp;
          memset(&resp, 0, sizeof(resp));
          resp.header.message_id = NFAPI_BROADCAST_DETECT_RESPONSE;
          resp.header.phy_id = req.header.phy_id;
          resp.error_code = NFAPI_MSG_INVALID_CONFIG;
          nfapi_pnf_broadcast_detect_resp(config, &resp);
        }
      } else {
        nfapi_broadcast_detect_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_BROADCAST_DETECT_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_pnf_broadcast_detect_resp(config, &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_handle_system_information_schedule_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_system_information_schedule_request_t req;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "SYSTEM_INFORMATION_SCHEDULE.request received\n");

    nfapi_pnf_config_t* config = &(pnf->_public);

    if (nfapi_p4_message_unpack(pRecvMsg, recvMsgLen, &req, sizeof(req), &config->codec_config) >= 0) {
      if (config->state == NFAPI_PNF_RUNNING) {
        nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find(config, req.header.phy_id);
        if (phy) {
          if (phy->state == NFAPI_PNF_PHY_RUNNING) {
            if (config->system_information_schedule_req) {
              (config->system_information_schedule_req)(config, phy, &req);
            }
          } else {
            nfapi_system_information_schedule_response_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.message_id = NFAPI_SYSTEM_INFORMATION_SCHEDULE_RESPONSE;
            resp.header.phy_id = req.header.phy_id;
            resp.error_code = NFAPI_MSG_INVALID_STATE;
            nfapi_pnf_system_information_schedule_resp(config, &resp);
          }
        } else {
          nfapi_system_information_schedule_response_t resp;
          memset(&resp, 0, sizeof(resp));
          resp.header.message_id = NFAPI_SYSTEM_INFORMATION_SCHEDULE_RESPONSE;
          resp.header.phy_id = req.header.phy_id;
          resp.error_code = NFAPI_MSG_INVALID_CONFIG;
          nfapi_pnf_system_information_schedule_resp(config, &resp);
        }
      } else {
        nfapi_system_information_schedule_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_SYSTEM_INFORMATION_SCHEDULE_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_pnf_system_information_schedule_resp(config, &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_handle_system_information_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_system_information_request_t req;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "SYSTEM_INFORMATION.request received\n");

    nfapi_pnf_config_t* config = &(pnf->_public);

    if (nfapi_p4_message_unpack(pRecvMsg, recvMsgLen, &req, sizeof(req), &config->codec_config) >= 0) {
      if (config->state == NFAPI_PNF_RUNNING) {
        nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find(config, req.header.phy_id);
        if (phy) {
          if (phy->state == NFAPI_PNF_PHY_RUNNING) {
            if (config->system_information_req) {
              (config->system_information_req)(config, phy, &req);
            }
          } else {
            nfapi_system_information_response_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.message_id = NFAPI_SYSTEM_INFORMATION_RESPONSE;
            resp.header.phy_id = req.header.phy_id;
            resp.error_code = NFAPI_MSG_INVALID_STATE;
            nfapi_pnf_system_information_resp(config, &resp);
          }
        } else {
          nfapi_system_information_response_t resp;
          memset(&resp, 0, sizeof(resp));
          resp.header.message_id = NFAPI_SYSTEM_INFORMATION_RESPONSE;
          resp.header.phy_id = req.header.phy_id;
          resp.error_code = NFAPI_MSG_INVALID_CONFIG;
          nfapi_pnf_system_information_resp(config, &resp);
        }
      } else {
        nfapi_system_information_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_SYSTEM_INFORMATION_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_pnf_system_information_resp(config, &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_handle_nmm_stop_request(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  if (pRecvMsg == NULL || pnf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: NULL parameters\n", __FUNCTION__);
  } else {
    nfapi_nmm_stop_request_t req;

    NFAPI_TRACE(NFAPI_TRACE_INFO, "NMM_STOP.request received\n");

    nfapi_pnf_config_t* config = &(pnf->_public);

    if (nfapi_p4_message_unpack(pRecvMsg, recvMsgLen, &req, sizeof(req), &config->codec_config) >= 0) {
      if (config->state == NFAPI_PNF_RUNNING) {
        nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find(config, req.header.phy_id);
        if (phy) {
          if (phy->state != NFAPI_PNF_PHY_RUNNING) {
            if (config->nmm_stop_req) {
              (config->nmm_stop_req)(config, phy, &req);
            }
          } else {
            nfapi_nmm_stop_response_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.message_id = NFAPI_NMM_STOP_RESPONSE;
            resp.header.phy_id = req.header.phy_id;
            resp.error_code = NFAPI_MSG_INVALID_STATE;
            nfapi_pnf_nmm_stop_resp(config, &resp);
          }
        } else {
          nfapi_nmm_stop_response_t resp;
          memset(&resp, 0, sizeof(resp));
          resp.header.message_id = NFAPI_NMM_STOP_RESPONSE;
          resp.header.phy_id = req.header.phy_id;
          resp.error_code = NFAPI_MSG_INVALID_CONFIG;
          nfapi_pnf_nmm_stop_resp(config, &resp);
        }
      } else {
        nfapi_nmm_stop_response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_NMM_STOP_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        nfapi_pnf_nmm_stop_resp(config, &resp);
      }
    } else {
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: Unpack message failed, ignoring\n", __FUNCTION__);
    }

    if (req.vendor_extension)
      pnf->_public.codec_config.deallocate(req.vendor_extension);
  }
}

void pnf_handle_p5_message(pnf_t* pnf, void* pRecvMsg, int recvMsgLen)
{
  nfapi_p4_p5_message_header_t messageHeader;

  // validate the input params
  if (pRecvMsg == NULL || recvMsgLen < NFAPI_HEADER_LENGTH) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s: invalid input params\n", __FUNCTION__);
    return;
  }

  // unpack the message header
  if (nfapi_p5_message_header_unpack(pRecvMsg,
                                     recvMsgLen,
                                     &messageHeader,
                                     sizeof(nfapi_p4_p5_message_header_t),
                                     &pnf->_public.codec_config)
      < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Unpack message header failed, ignoring\n");
    return;
  }

  switch (messageHeader.message_id) {
    case NFAPI_PNF_PARAM_REQUEST:
      pnf_handle_pnf_param_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_PNF_CONFIG_REQUEST:
      pnf_handle_pnf_config_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_PNF_START_REQUEST:
      pnf_handle_pnf_start_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_PNF_STOP_REQUEST:
      pnf_handle_pnf_stop_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_PARAM_REQUEST:
      pnf_handle_param_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_CONFIG_REQUEST:
      pnf_handle_config_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_START_REQUEST:
      pnf_handle_start_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_STOP_REQUEST:
      pnf_handle_stop_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_MEASUREMENT_REQUEST:
      pnf_handle_measurement_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_RSSI_REQUEST:
      pnf_handle_rssi_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_CELL_SEARCH_REQUEST:
      pnf_handle_cell_search_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_BROADCAST_DETECT_REQUEST:
      pnf_handle_broadcast_detect_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_SYSTEM_INFORMATION_SCHEDULE_REQUEST:
      pnf_handle_system_information_schedule_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_SYSTEM_INFORMATION_REQUEST:
      pnf_handle_system_information_request(pnf, pRecvMsg, recvMsgLen);
      break;

    case NFAPI_NMM_STOP_REQUEST:
      pnf_handle_nmm_stop_request(pnf, pRecvMsg, recvMsgLen);
      break;

    default: {
      if (messageHeader.message_id >= NFAPI_VENDOR_EXT_MSG_MIN && messageHeader.message_id <= NFAPI_VENDOR_EXT_MSG_MAX) {
        pnf_handle_vendor_extension(pRecvMsg, recvMsgLen, pnf, messageHeader.message_id);
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s P5 Unknown message ID %d\n", __FUNCTION__, messageHeader.message_id);
      }
    } break;
  }
}

int pnf_read_dispatch_message(pnf_t* pnf)
{
  int socket_connected = 1;

  // 1. Peek the message header
  // 2. If the message is larger than the stack buffer then create a dynamic buffer
  // 3. Read the buffer
  // 4. Handle the p5 message

  uint32_t header_buffer_size = NFAPI_HEADER_LENGTH;
  uint8_t header_buffer[header_buffer_size];

  uint32_t stack_buffer_size = 32; // should it be the size of then sctp_notificatoin structure
  uint8_t stack_buffer[stack_buffer_size];

  uint8_t* dynamic_buffer = 0;

  uint8_t* read_buffer = &stack_buffer[0];
  uint32_t message_size = 0;

  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);

  struct sctp_sndrcvinfo sndrcvinfo;
  (void)memset(&sndrcvinfo, 0, sizeof(struct sctp_sndrcvinfo));

  {
    int flags = MSG_PEEK;
    message_size = sctp_recvmsg(pnf->p5_sock,
                                header_buffer,
                                header_buffer_size,
                                /*(struct sockaddr*)&addr, &addr_len*/ 0,
                                0,
                                &sndrcvinfo,
                                &flags);

    if (message_size == -1) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "PNF Failed to peek sctp message size errno:%d\n", errno);
      return 0;
    }

    nfapi_p4_p5_message_header_t header;
    int unpack_result = nfapi_p5_message_header_unpack(header_buffer, header_buffer_size, &header, sizeof(header), 0);
    if (unpack_result < 0) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "PNF Failed to unpack p5 message header\n");
      return 0;
    } else {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "PNF Unpack p5 message header success\n");
    }
    message_size = header.message_length + header_buffer_size;

    // now have the size of the mesage
  }

  if (message_size > stack_buffer_size) {
    dynamic_buffer = malloc_or_fail(message_size);

    if (dynamic_buffer == NULL) {
      // todo : add error mesage
      NFAPI_TRACE(NFAPI_TRACE_INFO, "PNF Failed to allocate dynamic buffer for sctp_recvmsg size:%d\n", message_size);
      return -1;
    }

    read_buffer = dynamic_buffer;
  }

  {
    int flags = 0;
    (void)memset(&sndrcvinfo, 0, sizeof(struct sctp_sndrcvinfo));

    int recvmsg_result =
        sctp_recvmsg(pnf->p5_sock, read_buffer, message_size, (struct sockaddr*)&addr, &addr_len, &sndrcvinfo, &flags);
    if (recvmsg_result == -1) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "Failed to read sctp message size errno:%d\n", errno);
    } else {
#if 0
			// print the received message
			printf("\n MESSAGE RECEIVED: \n");
			for(int i=0; i<message_size; i++){
				printf("%d", read_buffer[i]);
			}
			printf("\n");
#endif

      if (flags & MSG_NOTIFICATION) {
        NFAPI_TRACE(NFAPI_TRACE_INFO, "Notification received from %s:%u\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

        // todo - handle the events
      } else {
        /*
        NFAPI_TRACE(NFAPI_TRACE_INFO, "Received message fd:%d from %s:%u assoc:%d on stream %d, PPID %d, length %d, flags 0x%x\n",
            pnf->p5_sock,
            inet_ntoa(addr.sin_addr),
            ntohs(addr.sin_port),
            sndrcvinfo.sinfo_assoc_id,
            sndrcvinfo.sinfo_stream,
            ntohl(sndrcvinfo.sinfo_ppid),
            message_size,
            flags);
        */

        // handle now if complete message in one or more segments
        if ((flags & 0x80) == 0x80) {
          pnf_handle_p5_message(pnf, read_buffer, message_size);
        } else {
          NFAPI_TRACE(NFAPI_TRACE_WARN, "sctp_recvmsg: unhandled mode with flags 0x%x\n", flags);

          // assume socket disconnected
          NFAPI_TRACE(NFAPI_TRACE_WARN, "Disconnected socket\n");
          socket_connected = 0;
        }
      }
    }
  }

  if (dynamic_buffer) {
    free(dynamic_buffer);
  }

  return socket_connected;
}

int pnf_message_pump(pnf_t* pnf)
{
  uint8_t socketConnected = 1;

  while (socketConnected && pnf->terminate == 0) {
    fd_set rfds;
    int selectRetval = 0;

    // select on a timeout and then get the message
    FD_ZERO(&rfds);
    FD_SET(pnf->p5_sock, &rfds);

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    selectRetval = select(pnf->p5_sock + 1, &rfds, NULL, NULL, &timeout);

    if (selectRetval == 0) {
      // timeout
      continue;
    } else if (selectRetval == -1 && (errno == EINTR)) {
      // interrupted by signal
      NFAPI_TRACE(NFAPI_TRACE_WARN, "P5 Signal Interrupt %d\n", errno);
      continue;
    } else if (selectRetval == -1) {
      NFAPI_TRACE(NFAPI_TRACE_WARN, "P5 select() failed\n");
      sleep(1);
      continue;
    }

    if (FD_ISSET(pnf->p5_sock, &rfds)) {
      socketConnected = pnf_read_dispatch_message(pnf);
    } else {
      NFAPI_TRACE(NFAPI_TRACE_WARN, "Why are we here\n");
    }
  }

  // Drop back to idle if we have lost connection
  pnf->_public.state = NFAPI_PNF_IDLE;

  // close the connection and socket
  if (close(pnf->p5_sock) < 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "close(sctpSock) failed errno: %d\n", errno);
  }

  return 0;
}
