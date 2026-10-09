/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_DBT_H
#define NR_DBT_H

#include "PHY/defs_gNB.h"

/* Digital beamforming with the digital beam table, for one logical antenna port of one symbol of a PDU:
 * adds in[], the num_rb RBs from rb_start of that port, weighted by w[beam][n] onto RBs [rb_start,
 * rb_start + num_rb) of every baseband port n of txdataF. The beam is the one of digital BF interface
 * dig_bf_interface of the PDU. The phase compensation of the symbol, if enabled, is applied in the same
 * step, so the caller does not rotate in[]. */
void nr_dbt_beamform(PHY_VARS_gNB *gNB,
                     const nfapi_nr_tx_precoding_and_beamforming_t *pb,
                     int dig_bf_interface,
                     int slot,
                     int symbol,
                     int prg_origin,
                     int rb_start,
                     int num_rb,
                     const c16_t *in);

#endif
