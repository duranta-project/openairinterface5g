/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**********************************************************************
*
* FILENAME    :  dmrs.h
*
* MODULE      :  demodulation reference signals
*
* DESCRIPTION :  generation of dmrs sequences for NR 5G
*                3GPP TS 38.211
*
************************************************************************/

#ifndef DMRS_NR_H
#define DMRS_NR_H

#include "PHY/defs_nr_common.h"

int pseudo_random_sequence(int M_PN, uint32_t *c, uint32_t cinit);
uint16_t get_dmrs_freq_idx_ul(uint16_t n, uint8_t k_prime, uint8_t delta, uint8_t dmrs_type);

uint8_t allowed_xlsch_re_in_dmrs_symbol(uint16_t k,
                                        uint16_t start_sc,
                                        uint16_t ofdm_symbol_size,
                                        uint8_t numDmrsCdmGrpsNoData,
                                        uint8_t dmrs_type);

void nr_gen_ref_conj_symbols(const uint32_t *in, uint32_t ref_symbols, c16_t *output);
int8_t get_next_dmrs_symbol_in_slot(uint16_t  ul_dmrs_symb_pos, uint8_t counter, uint8_t end_symbol);
int8_t get_valid_dmrs_idx_for_channel_est(uint16_t  dmrs_symb_pos, uint8_t counter);
void nr_chest_time_domain_avg(NR_DL_FRAME_PARMS *frame_parms,
                              int32_t **ch_estimates,
                              uint8_t num_symbols,
                              uint8_t start_symbol,
                              uint16_t dmrs_bitmap,
                              uint16_t num_rbs,
                              uint8_t nb_layers,
                              uint8_t num_streams);

int8_t get_num_dmrs_re_per_rb(const uint8_t dmrs_type, const uint8_t num_cdm_grp_no_data);

static inline uint8_t is_dmrs_symbol(uint8_t l, uint16_t dmrsSymbMask)
{
  DevAssert(l < 32);
  return ((dmrsSymbMask >> l) & 0x1);
}
#undef EXTERN

#endif /* DMRS_NR_H */


