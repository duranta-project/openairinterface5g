/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_SCH_DMRS_H
#define NR_SCH_DMRS_H

#include "PHY/defs_nr_common.h"
#include "nfapi/open-nFAPI/nfapi/public_inc/nfapi_nr_interface.h"


void get_antenna_ports(uint8_t *ap, uint8_t n_symbs, uint8_t config);
void get_Wt(int *Wt, const int ap, const nfapi_nr_dmrs_type_e config);
void get_Wf(int *Wf, const int ap, const nfapi_nr_dmrs_type_e config);

uint8_t get_delta(uint8_t ap, uint8_t config);

uint16_t get_dmrs_freq_idx(uint16_t n, uint8_t k_prime, uint8_t delta, uint8_t dmrs_type);

#endif
