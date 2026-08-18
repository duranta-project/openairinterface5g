/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __PHY_NR_TRANSPORT_DCI__H
#define __PHY_NR_TRANSPORT_DCI__H

#include "PHY/defs_gNB.h"
#include "PHY/NR_REFSIG/nr_refsig.h"
#include "nfapi/open-nFAPI/nfapi/public_inc/nfapi_nr_interface.h"

void nr_generate_dci(PHY_VARS_gNB *gNB,
                     const nfapi_nr_dl_tti_pdcch_pdu_rel15_t *pdcch_pdu_rel15,
                     NR_DL_FRAME_PARMS *frame_parms,
                     int slot,
                     uint64_t *phase_comp_prb_mask,
                     int prb_mask_words);

void nr_fill_dci(PHY_VARS_gNB *gNB,
                 int frame,
                 int slot,
		 nfapi_nr_dl_tti_pdcch_pdu *pdcch_pdu);

void nr_fill_ul_dci(PHY_VARS_gNB *gNB,
		    int frame,
		    int slot,
		    nfapi_nr_ul_dci_request_pdus_t *pdcch_pdu);

void nr_fill_reg_list(int cce_list[MAX_DCI_CORESET][NR_MAX_PDCCH_AGG_LEVEL * NR_NB_REG_PER_CCE], const nfapi_nr_dl_tti_pdcch_pdu_rel15_t *pdcch_pdu_rel15);

#endif //__PHY_NR_TRANSPORT_DCI__H
