/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#ifndef _PNF_LTE_H_
#define _PNF_LTE_H_

#include <stddef.h>
#include "nfapi_lte_pnf_interface.h"
#include "pnf_common.h"

typedef struct pnf_s {
  nfapi_pnf_config_t _public; /* LTE P5 config — must be first */
  int p5_sock;
  uint8_t tx_message_buffer[NFAPI_MAX_PACKED_MESSAGE_SIZE];
  uint8_t sctp;
  uint8_t terminate;
} pnf_t;

int pnf_connect(pnf_t *pnf);
int pnf_message_pump(pnf_t *pnf);

void pnf_handle_vendor_extension(void *pRecvMsg, int recvMsgLen, pnf_t *pnf, uint16_t message_id);

int pnf_pack_and_send_p5_message(pnf_t *pnf, nfapi_p4_p5_message_header_t *msg, uint32_t msg_len);
int pnf_pack_and_send_p4_message(pnf_t *pnf, nfapi_p4_p5_message_header_t *msg, uint32_t msg_len);
int pnf_send_message(pnf_t *pnf, uint8_t *msg, uint32_t msg_len, uint16_t stream_id);

#endif // _PNF_LTE_H_
