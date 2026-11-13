/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#ifndef _PNF_H_
#define _PNF_H_

#include <stddef.h>
#include "nfapi_lte_pnf_interface.h"
#include "nfapi_nr_pnf_interface.h"

typedef struct pnf_s {
  nfapi_pnf_config_t _public; /* LTE P5 config */
  nfapi_nr_pnf_config_t _nr_public; /* NR P5 config */

  int p5_sock;
  uint8_t tx_message_buffer[NFAPI_MAX_PACKED_MESSAGE_SIZE];
  uint8_t sctp;
  uint8_t terminate;
} pnf_t;

/* Recover pnf_t* from a pointer to its _nr_public member */
#define PNF_FROM_NR_CONFIG(c) ((pnf_t*)((char*)(c) - offsetof(pnf_t, _nr_public)))

int pnf_connect(pnf_t *pnf);
int pnf_message_pump(pnf_t *pnf);
int pnf_nr_message_pump(pnf_t* pnf);

void pnf_nr_handle_p5_message(pnf_t* pnf, void* pRecvMsg, int recvMsgLen);
int pnf_pack_and_send_p5_message(pnf_t* pnf, nfapi_p4_p5_message_header_t* msg, uint32_t msg_len);
int pnf_nr_pack_and_send_p5_message(pnf_t* pnf, nfapi_p4_p5_message_header_t* msg, uint32_t msg_len);
int pnf_pack_and_send_p4_message(pnf_t* pnf, nfapi_p4_p5_message_header_t* msg, uint32_t msg_len);
int pnf_send_message(pnf_t* pnf, uint8_t* msg, uint32_t msg_len, uint16_t stream_id);

nfapi_pnf_phy_config_t* nfapi_pnf_phy_config_find(nfapi_pnf_config_t* config, uint16_t phy_id);
void nfapi_pnf_phy_config_delete_all(nfapi_pnf_config_t* config);
void nfapi_pnf_phy_config_add(nfapi_pnf_config_t* config, nfapi_pnf_phy_config_t* phy);

void pnf_handle_vendor_extension(void* pRecvMsg, int recvMsgLen, pnf_t* pnf, uint16_t message_id);
void pnf_nr_handle_vendor_extension(void* pRecvMsg, int recvMsgLen, pnf_t* pnf, uint16_t message_id);

#endif // _PNF_H_
