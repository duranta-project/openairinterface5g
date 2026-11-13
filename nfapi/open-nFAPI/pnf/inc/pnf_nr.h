/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#ifndef _PNF_NR_H_
#define _PNF_NR_H_

#include "nfapi_nr_pnf_interface.h"
#include "pnf_common.h"

typedef struct pnf_nr_s {
  nfapi_nr_pnf_config_t _public; /* NR P5 config — must be first */
  int p5_sock;
  uint8_t tx_message_buffer[NFAPI_MAX_PACKED_MESSAGE_SIZE];
  uint8_t sctp;
  uint8_t terminate;
} pnf_nr_t;

int pnf_nr_connect(pnf_nr_t *pnf);
int pnf_nr_message_pump(pnf_nr_t *pnf);

void pnf_nr_handle_p5_message(pnf_nr_t *pnf, void *pRecvMsg, int recvMsgLen);
int pnf_nr_pack_and_send_p5_message(pnf_nr_t *pnf, nfapi_p4_p5_message_header_t *msg, uint32_t msg_len);
int pnf_nr_send_message(pnf_nr_t *pnf, uint8_t *msg, uint32_t msg_len, uint16_t stream_id);
void pnf_nr_handle_vendor_extension(void *pRecvMsg, int recvMsgLen, pnf_nr_t *pnf, uint16_t message_id);

#endif // _PNF_NR_H_
