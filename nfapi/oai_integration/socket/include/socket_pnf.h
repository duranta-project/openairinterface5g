/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef SOCKET_PNF_H
#define SOCKET_PNF_H
#include "socket_common.h"
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "pnf.h"
#include "pnf_p7.h"
#include "nr_fapi_p5.h"
#include "nr_nfapi_p7.h"

int pnf_connect_socket(pnf_t *pnf);
int pnf_nr_socket_message_pump(pnf_t *pnf);
bool pnf_nr_send_p5_message(pnf_t *pnf, nfapi_nr_p4_p5_message_header_t *msg, uint32_t msg_len);
bool pnf_nr_send_p7_message(pnf_p7_t* pnf_p7, nfapi_nr_p7_message_header_t* header, uint32_t msg_len);
void *pnf_start_p5_thread(void *ptr);
void *pnf_nr_p7_thread_start(void *ptr);
void socket_nfapi_nr_pnf_stop();
#endif // SOCKET_PNF_H
