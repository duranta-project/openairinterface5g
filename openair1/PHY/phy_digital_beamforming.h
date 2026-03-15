/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __PHY_DIGITAL_BEAMFORMING__H__
#define __PHY_DIGITAL_BEAMFORMING__H__

#include "defs_gNB.h"

void fill_rx_grid_info(const RU_t *ru,
                       const uint32_t frame,
                       const uint32_t slot,
                       const nfapi_nr_ul_tti_request_number_of_pdus_t *ul_pdu,
                       struct nr_grid_slot *nrg);
void send_rx_grid_info(RU_t *ru, struct nr_grid_slot *nrg);
void tx_beamforming_if(RU_t *ru);
void apply_beamforming(const nfapi_nr_dbt_pdu_t *dbt,
                       const NR_DL_FRAME_PARMS *fp,
                       const uint16_t num_bb,
                       c16_t **bb,
                       struct nr_grid *nrg,
                       bool is_tx);

#endif
