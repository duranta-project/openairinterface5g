/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __NR_REFSIG_DEFS__H__
#define __NR_REFSIG_DEFS__H__

#include "PHY/defs_nr_UE.h"

typedef struct port_freq_indices {
  uint8_t p;
  uint16_t k;
} port_freq_indices_t;

typedef struct csi_rs_params {
  uint8_t size;
  uint8_t j[16];
  uint8_t k_n[6];
  uint8_t kprime;
  uint8_t lprime;
  uint8_t ports;
  uint8_t koverline[16];
  uint8_t loverline[16];
  double rho;
  double alpha;
  uint8_t gs;
} csi_rs_params_t;

void sl_generate_pss(SL_NR_UE_INIT_PARAMS_t *sl_init_params, uint8_t n_sl_id2, uint16_t scaling);
void sl_generate_pss_ifft_samples(sl_nr_ue_phy_params_t *sl_ue_params, SL_NR_UE_INIT_PARAMS_t *sl_init_params);
void sl_generate_sss(SL_NR_UE_INIT_PARAMS_t *sl_init_params, uint16_t slss_id, uint16_t scaling);
void sl_init_psbch_dmrs_gold_sequences(PHY_VARS_NR_UE *UE);

void nr_init_pscch_dmrs(NR_DL_FRAME_PARMS *fp, uint32_t ***nr_gold, uint16_t nid);

void nr_init_pssch_dmrs_oneshot(NR_DL_FRAME_PARMS *fp,
                                uint16_t N_id,
                                uint32_t *pssch_dmrs,
                                int slot,
                                int symb);

void get_csi_rs_freq_ind_sl(const NR_DL_FRAME_PARMS* frame_parms,
                            uint16_t n,
                            nfapi_nr_dl_tti_csi_rs_pdu_rel15_t* csi_params,
                            csi_rs_params_t* table_params,
                            port_freq_indices_t* port_freq_indices);

void get_csi_rs_params_from_table(const nfapi_nr_dl_tti_csi_rs_pdu_rel15_t *csi_params,
                                  csi_rs_params_t* table_params);
#endif
