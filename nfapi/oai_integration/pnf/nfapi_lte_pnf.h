/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NFAPI_LTE_PNF_H_
#define NFAPI_LTE_PNF_H_

extern nfapi_ue_release_request_body_t release_rntis;
int oai_nfapi_rach_ind(nfapi_rach_indication_t *rach_ind);
void configure_nfapi_pnf(char *vnf_ip_addr, int vnf_p5_port, char *pnf_ip_addr, int pnf_p7_port, int vnf_p7_port);
void oai_subframe_ind(uint16_t sfn, uint16_t sf);
int oai_nfapi_harq_indication(nfapi_harq_indication_t *harq_ind);
int oai_nfapi_crc_indication(nfapi_crc_indication_t *crc_ind);
int oai_nfapi_cqi_indication(nfapi_cqi_indication_t *ind);
int oai_nfapi_rx_ind(nfapi_rx_indication_t *ind);
int oai_nfapi_sr_indication(nfapi_sr_indication_t *ind);
#endif /* NFAPI_LTE_PNF_H_ */
