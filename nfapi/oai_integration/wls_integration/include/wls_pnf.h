/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef OPENAIRINTERFACE_WLS_PNF_H
#define OPENAIRINTERFACE_WLS_PNF_H
#include "nfapi_lte_pnf_interface.h"
#include "pnf.h"
#include "wls_common.h"

bool wls_pnf_nr_send_p5_message(pnf_t *pnf, nfapi_nr_p4_p5_message_header_t* msg, uint32_t msg_len);
bool wls_pnf_nr_send_p7_message(pnf_p7_t* pnf_p7,nfapi_nr_p7_message_header_t *msg, uint32_t msg_len);
void *wls_fapi_pnf_nr_start_thread(void *ptr);
int wls_fapi_nr_pnf_start();
void wls_pnf_set_p7_config(void *p7_config);
void wls_fapi_nr_pnf_stop();
void wls_pnf_close(pthread_t p5_thread);
#endif // OPENAIRINTERFACE_WLS_PNF_H
