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

#ifndef PTRS_NR_H
#define PTRS_NR_H

#include <sys/types.h>
#include <platform_types.h>

uint get_ptrs_k_RB(uint n_rb, uint k_ptrs, uint nrnti);
uint16_t get_ptrs_symb_idx(uint8_t duration_in_symbols, uint8_t start_symbol, uint8_t L_ptrs, uint16_t dmrs_symb_pos);

unsigned int get_first_ptrs_re(const rnti_t rnti, const uint8_t K_ptrs, const uint16_t nRB, const uint8_t k_RE_ref);

uint8_t is_ptrs_subcarrier(uint16_t k,
                           uint16_t n_rnti,
                           uint8_t K_ptrs,
                           uint16_t N_RB,
                           uint8_t k_RE_ref,
                           uint16_t start_sc,
                           uint16_t ofdm_symbol_size);

/*******************************************************************
*
* NAME :         is_ptrs_symbol
*
* PARAMETERS : l                      ofdm symbol index within slot
*              ptrs_symbols           bit mask of ptrs
*
* RETURN :       1 if symbol is ptrs, or 0 otherwise
*
* DESCRIPTION :  3GPP TS 38.211 6.4.1.2 Phase-tracking reference signal for PUSCH
*
*********************************************************************/

static inline uint8_t is_ptrs_symbol(uint8_t l, uint16_t ptrs_symbols) { return ((ptrs_symbols >> l) & 1); }

uint8_t get_ptrs_symbols_in_slot(uint16_t l_prime_mask, uint16_t start_symb, uint16_t nb_symb);
int8_t get_next_ptrs_symbol_in_slot(uint16_t  ptrsSymbPos, uint8_t counter, uint8_t nb_symb);
int8_t get_next_estimate_in_slot(uint16_t  ptrsSymbPos,uint16_t  dmrsSymbPos, uint8_t counter,uint8_t nb_symb);

int8_t nr_ptrs_process_slot(uint16_t dmrsSymbPos,
                            uint16_t ptrsSymbPos,
                            int16_t *estPerSymb,
                            uint16_t startSymbIdx,
                            uint16_t noSymb);

/*  general function to estimate common phase error based upon PTRS */
void nr_ptrs_cpe_estimation(uint8_t K_ptrs,
                            uint8_t ptrsReOffset,
                            uint16_t nb_rb,
                            uint16_t rnti,
                            uint16_t ofdm_symbol_size,
                            c16_t *rxF_comp,
                            const uint32_t *gold_seq,
                            int16_t *error_est,
                            int32_t *ptrs_sc);

void get_slope_from_estimates(uint8_t start, uint8_t end, int16_t *est_p, double *slope_p);
void ptrs_estimate_from_slope(int16_t *error_est, double *slope_p, uint8_t start, uint8_t end);
#endif /* PTRS_NR_H */
