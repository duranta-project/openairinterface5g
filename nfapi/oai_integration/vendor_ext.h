/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#ifndef _VENDOR_EXT_H_
#define _VENDOR_EXT_H_

#include "nfapi_interface.h"
#include "debug.h"
#include "common/utils/LOG/log.h"

typedef enum {
  P5_VENDOR_EXT_REQ = NFAPI_VENDOR_EXT_MSG_MIN,
  P5_VENDOR_EXT_RSP,

  P7_VENDOR_EXT_REQ,
  P7_VENDOR_EXT_IND

} vendor_ext_message_id_e;

typedef struct {
  nfapi_p4_p5_message_header_t header;
  uint16_t dummy1;
  uint16_t dummy2;
} vendor_ext_p5_req;

typedef struct {
  nfapi_p4_p5_message_header_t header;
  uint16_t error_code;
} vendor_ext_p5_rsp;

typedef struct {
  nfapi_p7_message_header_t header;
  uint16_t dummy1;
  uint16_t dummy2;
} vendor_ext_p7_req;

typedef struct {
  nfapi_p7_message_header_t header;
  uint16_t error_code;
} vendor_ext_p7_ind;

typedef struct {
  nfapi_tl_t tl;
  uint32_t dummy;
} vendor_ext_tlv_1;

#define VENDOR_EXT_TLV_1_TAG 0xF001

typedef struct {
  nfapi_tl_t tl;
  uint32_t dummy;
} vendor_ext_tlv_2;

#define VENDOR_EXT_TLV_2_TAG 0xF002

typedef enum {
  NFAPI_MONOLITHIC=0,
  NFAPI_MODE_PNF,
  NFAPI_MODE_VNF,
  NFAPI_UE_STUB_PNF,
  NFAPI_UE_STUB_OFFNET,
  NFAPI_MODE_STANDALONE_PNF,
  NFAPI_MODE_AERIAL,
  NFAPI_MODE_UNKNOWN
} nfapi_mode_t;

const char *nfapi_get_strmode(void);
void nfapi_logmode(void);
nfapi_mode_t nfapi_getmode(void);
void nfapi_setmode(nfapi_mode_t nfapi_mode);
#define NFAPI_MODE (nfapi_getmode())

static inline int nfapitooai_level(int nfapilevel)
{
  switch (nfapilevel) {
    case NFAPI_TRACE_ERROR:
      return OAILOG_ERR;
    case NFAPI_TRACE_WARN:
      return OAILOG_WARNING;
    case NFAPI_TRACE_NOTE:
      return OAILOG_INFO;
    case NFAPI_TRACE_INFO:
      return OAILOG_DEBUG;
    default:
      return OAILOG_ERR;
  }
}
#endif // _VENDOR_EXT_
