/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_DBT_H
#define NR_DBT_H

#include "PHY/defs_gNB.h"

/* Whether L1 beamforms the PDU with the digital beam table: a table is configured, the PDU has digital BF
 * interfaces and their beam IDs have the MSB clear ("look up in the Hi-PHY table", FAPI Table 3-43). Otherwise
 * its logical ports go onto the baseband ports of its spatial stream indices: unweighted with no digital BF
 * interface (straight wire), with the beam ID forwarded to the RU when the MSB is set. */
bool nr_dbt_pdu(const NR_gNB_COMMON *common, const nfapi_nr_tx_precoding_and_beamforming_t *pb);

/* Precoding and digital beamforming with the digital beam table, for one symbol of a PDU: adds the num_layers
 * layers (in[l] holds the num_rb RBs from rb_start of layer l), precoded with pm (NULL: layer l on logical
 * port l) onto the num_ports logical ports of the PDU, each beamformed with the beam of its digital BF
 * interface, onto RBs [rb_start, rb_start + num_rb) of every TXRU (baseband port) of txdataF. The phase
 * compensation of the symbol, if enabled, is applied in the same step, so the caller does not rotate in[]. */
void nr_dbt_beamform(PHY_VARS_gNB *gNB,
                     const nfapi_nr_tx_precoding_and_beamforming_t *pb,
                     const nfapi_nr_pm_pdu_t *pm,
                     int num_ports,
                     int num_layers,
                     int slot,
                     int symbol,
                     int prg_origin,
                     int rb_start,
                     int num_rb,
                     const c16_t *const in[]);

#endif
