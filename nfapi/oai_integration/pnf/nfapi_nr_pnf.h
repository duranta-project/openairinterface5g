/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NFAPI_NR_PNF_H_
#define NFAPI_NR_PNF_H_

#include "nfapi_nr_interface_scf.h"

void configure_nr_nfapi_pnf(char *vnf_ip_addr, int vnf_p5_port, char *pnf_ip_addr, int pnf_p7_port, int vnf_p7_port);
void stop_nr_nfapi_pnf(void);

typedef struct NR_Sched_Rsp NR_Sched_Rsp_t;
void handle_nr_slot_ind(uint16_t sfn, uint16_t slot, NR_Sched_Rsp_t *sched_resp);
void sfnslot_add_slot(int mu, uint16_t *sfn, uint16_t *slot, int offset);

int oai_nfapi_nr_slot_indication(nfapi_nr_slot_indication_scf_t *ind);
int oai_nfapi_nr_rx_data_indication(nfapi_nr_rx_data_indication_t *ind);
int oai_nfapi_nr_crc_indication(nfapi_nr_crc_indication_t *ind);
int oai_nfapi_nr_srs_indication(nfapi_nr_srs_indication_t *ind);
int oai_nfapi_nr_srs_toa_vendor_ext_indication(nfapi_nr_srs_toa_vendor_ext_indication_t *ind);
int oai_nfapi_nr_uci_indication(nfapi_nr_uci_indication_t *ind);
int oai_nfapi_nr_rach_indication(nfapi_nr_rach_indication_t *ind);

#endif /* NFAPI_NR_PNF_H_ */
