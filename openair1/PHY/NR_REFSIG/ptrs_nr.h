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

#include "nr/nr_common.h"
#include <sys/types.h>
#include <platform_types.h>

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

uint8_t get_ptrs_symbols_in_slot(uint16_t l_prime_mask, uint16_t start_symb, uint16_t nb_symb);

typedef struct {
  uint start_symb;
  uint num_symb;
  uint start_rb;
  uint num_rb;
  uint N_RB;
  uint first_carrier_offset;
  uint ofdm_symbol_size;
  uint symbols_per_slot;
  uint nid;
  uint nscid;
  uint slot;
  uint k_ptrs;
  uint k_re_ref;
  uint16_t rnti;
  uint16_t ptrs_symb_pos;
  uint16_t dmrs_symb_pos;
} ptrs_proc_t;

uint nr_ptrs_process_slot(const c16_t *rxdataF, const c16_t *chest, c16_t cpe[NR_SYMBOLS_PER_SLOT], ptrs_proc_t *p);

/**
 * @brief Runs the common PTRS pipeline on an already-populated ptrs_proc_t: derives p->ptrs_symb_pos,
 *        estimates/interpolates CPE into cpe, and returns the number of PTRS REs per symbol.
 */
uint nr_ptrs_run(ptrs_proc_t *p,
                 uint8_t ptrs_time_density,
                 const c16_t *rxdataF,
                 const c16_t *chest,
                 c16_t cpe[NR_SYMBOLS_PER_SLOT]);

uint get_num_ptrs_re_symbol(uint num_rb, uint k_ptrs, uint nrnti);

uint16_t get_ptrs_re_bitmap(uint rb, uint k_re_ref, uint k_ptrs, uint k_rb_ref);

uint get_ptrs_k_RB(uint n_rb, uint k_ptrs, uint nrnti);
#endif /* PTRS_NR_H */
