/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __openair_SCHED_NR_DEFS_H__
#define __openair_SCHED_NR_DEFS_H__

#include "PHY/defs_gNB.h"
#include "PHY/NR_TRANSPORT/nr_dci.h"
#include "phy_frame_config_nr.h"

void nr_set_ssb_first_subcarrier(nfapi_nr_config_request_scf_t *cfg, NR_DL_FRAME_PARMS *fp);
void phy_procedures_gNB_TX(PHY_VARS_gNB *gNB,
                           const nfapi_nr_dl_tti_request_t *DL_req,
                           const nfapi_nr_tx_data_request_t *TX_req,
                           const nfapi_nr_ul_dci_request_t *UL_dci_req,
                           int frame,
                           int slot);
void nr_save_ul_tti_req(PHY_VARS_gNB *gNB, nfapi_nr_ul_tti_request_t *UL_tti_req);
int phy_procedures_gNB_uespec_RX(PHY_VARS_gNB *gNB, int frame_rx, int slot_rx, NR_UL_IND_t *UL_INFO);
void L1_nr_prach_procedures(PHY_VARS_gNB *gNB, prach_item_t *prach_id, nfapi_nr_rach_indication_t *rach_ind);
void nr_common_signal_procedures (PHY_VARS_gNB *gNB,int frame,int slot, const nfapi_nr_dl_tti_ssb_pdu *ssb_pdu);
void nr_feptx_ofdm(RU_t *ru, int frame_tx, int tti_tx);
void nr_feptx_prec(RU_t *ru,int frame_tx,int tti_tx);
void nr_feptx_prec_control(RU_t *ru,int frame,int tti_tx);
void nr_fep_tp(RU_t *ru, int slot);
void nr_feptx_tp(RU_t *ru, int frame_tx, int slot);
void feptx_prec(RU_t *ru,int frame_tx,int tti_tx);
void nr_phy_init_RU(RU_t *ru);
void nr_phy_free_RU(RU_t *ru);
void update_grid_info(struct nr_grid *grid,
                      uint16_t port,
                      uint16_t beam_id,
                      uint16_t start_rb,
                      uint16_t num_rb,
                      uint16_t start_symb,
                      uint16_t num_symb);
#endif
