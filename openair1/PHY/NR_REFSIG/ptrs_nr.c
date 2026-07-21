/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**********************************************************************
*
* FILENAME    :  ptrs_nr.c
*
* MODULE      :  phase tracking reference signals
*
* DESCRIPTION :  resource element mapping of ptrs sequences
*                3GPP TS 38.211 and 3GPP TS 38.214
*
************************************************************************/

#include "assertions.h"
#include "nr_common.h"
#include "platform_types.h"
#include "utils.h"
#include <stdint.h>
#include <stdio.h>
#include "dmrs_nr.h"
#include "PHY/NR_REFSIG/ptrs_nr.h"
#include "PHY/TOOLS/tools_defs.h"
#include "PHY/NR_REFSIG/nr_refsig.h"

/*******************************************************************
 *
 * NAME :         get_ptrs_symb_idx
 *
 * PARAMETERS :   duration_in_symbols    number of scheduled ofdm symbols
 *                start_symbol           first ofdm symbol within slot
 *                L_ptrs                 the parameter L_ptrs
 *                dmrs_symb_pos          bitmap of the time domain positions of the DMRS symbols
 *
 * RETURN :       bit map of PTRS ofdm symbol indicies
 *
 * DESCRIPTION :  3GPP TS 38.211 6.4.1.2.2.1
 *
 *********************************************************************/

uint16_t get_ptrs_symb_idx(uint8_t duration_in_symbols, uint8_t start_symbol, uint8_t L_ptrs, uint16_t dmrs_symb_pos)
{
  int i = 0;
  int l_ref = start_symbol;
  const int last_symbol = start_symbol + duration_in_symbols - 1;
  AssertFatal(L_ptrs > 0, "Impossible L_ptrs\n");

  uint16_t ptrs_symbols = 0;
  while ((l_ref + i * L_ptrs) <= last_symbol) {
    int is_dmrs_symbol = 0, l_counter;
    for(l_counter = l_ref + i * L_ptrs; l_counter >= max(l_ref + (i - 1) * L_ptrs + 1, l_ref); l_counter--) {
      if((dmrs_symb_pos >> l_counter) & 0x01) {
        is_dmrs_symbol = 1;
        break;
      }
    }
    if (is_dmrs_symbol) {
      l_ref = l_counter;
      i = 1;
      continue;
    }
    ptrs_symbols = ptrs_symbols | (1 << (l_ref + i * L_ptrs));
    i++;
  }
  return ptrs_symbols;
}

unsigned int get_first_ptrs_re(const rnti_t rnti, const uint8_t K_ptrs, const uint16_t nRB, const uint8_t k_RE_ref)
{
  const uint nRB_Kptrs = nRB % K_ptrs;
  const uint k_RB_ref = nRB_Kptrs ? (rnti % nRB_Kptrs) : (rnti % K_ptrs);
  return (k_RE_ref + k_RB_ref * NR_NB_SC_PER_RB);
}

/**
 * @brief Returns k^{RB}_{ref} as per TS 38.211 6.4.1.2.2.1
 *
 * @param n_rb
 * @param k_ptrs
 * @param nrnti
 * @return
 */
uint get_ptrs_k_RB(uint n_rb, uint k_ptrs, uint nrnti)
{
  DevAssert(k_ptrs > 0);
  const uint a = n_rb % k_ptrs;
  return a ? nrnti % a : nrnti % k_ptrs;
}

/*******************************************************************
*
* NAME :         is_ptrs_subcarrier
*
* PARAMETERS : k                      subcarrier index
*              n_rnti                 UE CRNTI
*              K_ptrs                 the parameter K_ptrs
*              N_RB                   number of RBs scheduled
*              k_RE_ref               the parameter k_RE_ref
*              start_sc               first subcarrier index
*              ofdm_symbol_size       number of samples in an OFDM symbol
*
* RETURN :       1 if subcarrier k is PTRS, or 0 otherwise
*
* DESCRIPTION :  3GPP TS 38.211 6.4.1.2 Phase-tracking reference signal
*
*********************************************************************/

uint8_t is_ptrs_subcarrier(uint16_t k,
                           uint16_t n_rnti,
                           uint8_t K_ptrs,
                           uint16_t N_RB,
                           uint8_t k_RE_ref,
                           uint16_t start_sc,
                           uint16_t ofdm_symbol_size)
{
  uint16_t k_RB_ref;

  if (N_RB % K_ptrs == 0)
    k_RB_ref = n_rnti % K_ptrs;
  else
    k_RB_ref = n_rnti % (N_RB % K_ptrs);

  if (k < start_sc)
    k += ofdm_symbol_size;

  if ((k - k_RE_ref - k_RB_ref * NR_NB_SC_PER_RB - start_sc) % (K_ptrs * NR_NB_SC_PER_RB) == 0)
    return 1;

  return 0;
}

/* return the total number of ptrs symbol in a slot */
uint8_t get_ptrs_symbols_in_slot(uint16_t l_prime_mask, uint16_t start_symb, uint16_t nb_symb)
{
  uint8_t tmp = 0;
  for (int i = start_symb; i <= nb_symb; i++)
    tmp += (l_prime_mask >> i) & 0x01;
  return tmp;
}

/* return the position of next ptrs symbol in a slot */
int8_t get_next_ptrs_symbol_in_slot(uint16_t  ptrs_symb_pos, uint8_t counter, uint8_t nb_symb)
{
  for(int8_t symbol = counter; symbol < nb_symb; symbol++) {
    if((ptrs_symb_pos >> symbol) & 0x01) {
      return symbol;
    }
  }
  return -1;
}

uint get_num_ptrs_re_symbol(uint num_rb, uint k_ptrs, uint nrnti)
{
  const uint k_rb_ref = get_ptrs_k_RB(num_rb, k_ptrs, nrnti);
  DevAssert(k_rb_ref < k_ptrs);
  uint num_re = num_rb / k_ptrs;
  num_re += (num_rb % k_ptrs) > k_rb_ref ? 1 : 0;

  return num_re;
}

uint16_t get_ptrs_re_bitmap(uint rb, uint k_re_ref, uint k_ptrs, uint k_rb_ref)
{
  if ((rb - k_rb_ref) % k_ptrs)
    return 0;
  else
    return (1U << k_re_ref);
}

static cd_t get_ptrs_phase_diff(const c16_t *rxF,
                                const c16_t *chest,
                                const uint32_t *gold_seq,
                                uint first_sc_offset,
                                uint ofdm_symbol_size,
                                uint k_ptrs,
                                uint k_rb_ref,
                                uint k_re_ref,
                                uint start_rb,
                                uint n_rb)
{
  const uint shift = 15;
  cd_t phase_diff = {0};
  uint k_re = k_re_ref + (start_rb + k_rb_ref) * NR_NB_SC_PER_RB;
  uint i = 0;

  while (k_re < (start_rb + k_rb_ref + n_rb) * NR_NB_SC_PER_RB) {
    const uint rx_re = CIRCULAR_INC(k_re, first_sc_offset, ofdm_symbol_size);
    // Get pilot symbol
    const c16_t pilot = get_modulated(gold_seq, i++, true); // Already a conjugate

    // Coherent complex correlation with DMRS estimates as reference:
    const c32_t re_phase_diff = c32x16mulConj(chest[k_re], c16mulShift(rxF[rx_re], pilot, shift));

    // Sum differences over REs
    phase_diff.r += re_phase_diff.r;
    phase_diff.i += re_phase_diff.i;

    k_re += k_ptrs * NR_NB_SC_PER_RB;
  }

  // normalized error estimation
  return cdNorm(phase_diff);
}

/**
 * @brief Estimates the CPE phasor of a PTRS symbol from its correlation against the reference channel estimate.
 */
static cd_t estimate_ptrs_symbol_phase(const c16_t *rxdataF,
                                       const c16_t *chest,
                                       uint dmrs_idx,
                                       uint symbol,
                                       uint k_rb_ref,
                                       const ptrs_proc_t *p)
{
  const uint32_t *gold_seq = nr_gold_pdsch(p->N_RB, p->symbols_per_slot, p->nid, p->nscid, p->slot, symbol);
  return get_ptrs_phase_diff(rxdataF + symbol * p->ofdm_symbol_size,
                             chest + dmrs_idx * p->ofdm_symbol_size,
                             gold_seq,
                             p->first_carrier_offset,
                             p->ofdm_symbol_size,
                             p->k_ptrs,
                             k_rb_ref,
                             p->k_re_ref,
                             p->start_rb,
                             p->num_rb);
}

static double get_interpolate_step_phase(cd_t start_vector, cd_t end_vector, uint dist)
{
  const cd_t d = cdMulConj(start_vector, end_vector);
  const double d_theta_step = atan2(d.i, d.r) / dist;
  return d_theta_step;
}

static cd_t add_cpx_phasor(cd_t base_vector, double phase)
{
  const cd_t incr = (cd_t){.r = cos(phase), .i = sin(phase)};
  return cdMul(base_vector, incr);
}

/**
 * @brief Estimates CPE from PTRS symbols in a slot and inter/extrapolates to other symbols.
 *
 * @param rxdataF Vector of p->ofdm_symbol_size * p->symbols_per_slot received samples. DC in index 0 of each symbol.
 * @param chest Vector of p->ofdm_symbol_size * p->symbols_per_slot channel estimates. First RB in index 0.
 * @param cpe Estimated CPE output.
 * @param p Struct with necessary information to process PTRS.
 * @return Number of PTRS symbols processed.
 */
uint nr_ptrs_process_slot(const c16_t *rxdataF, const c16_t *chest, c16_t cpe[NR_SYMBOLS_PER_SLOT], ptrs_proc_t *p)
{
  AssertFatal(!(p->dmrs_symb_pos & p->ptrs_symb_pos), "A symbol can't have both DMRS and PTRS\n");
  const int16_t amp = INT16_MAX;
  const uint k_rb_ref = get_ptrs_k_RB(p->num_rb, p->k_ptrs, p->rnti);
  const uint last_symbol = p->start_symb + p->num_symb - 1;
  const uint16_t combined_ref_pos = p->dmrs_symb_pos | p->ptrs_symb_pos;

  uint base_symbol = p->start_symb;
  int last_processed_ref = -1;
  cd_t base_phase = {.r = 1.0};
  cd_t next_base_phase = {.r = 1.0};
  double step_phase = 0.0;
  uint processed_ptrs = 0;

  // TS38.211 7.4.1.2.2: If PTRS is present, the first symbol is either PTRS or DMRS.
  for (uint symbol = p->start_symb; symbol <= last_symbol; symbol++) {
    // DMRS estimates to be used for compensating this symbol.
    const uint curr_dmrs = get_valid_dmrs_idx_for_channel_est(p->dmrs_symb_pos, symbol);

    if (last_processed_ref == symbol) {
      // Already estimated while interpolating the preceding symbols.
      base_phase = next_base_phase;
      base_symbol = symbol;
    } else if (IS_BIT_SET(p->ptrs_symb_pos, symbol)) {
      // Process PTRS symbol.
      base_phase = estimate_ptrs_symbol_phase(rxdataF, chest, curr_dmrs, symbol, k_rb_ref, p);
      base_symbol = symbol;
      last_processed_ref = symbol;
      processed_ptrs++;
    } else if (IS_BIT_SET(p->dmrs_symb_pos, symbol)) {
      // This symbol has DMRS. Set CPE to 0 because CPE estimated in PTRS symbols are with reference to DMRS.
      base_phase = (cd_t){.r = 1.0};
      base_symbol = symbol;
      last_processed_ref = symbol;
    } else {
      // No DMRS nor PTRS. (Extra/inter)polate CPE from the surrounding reference symbols.
      const int ffs_to_next_ref = __builtin_ffs(combined_ref_pos >> symbol);
      // If there is PTRS or DMRS after this symbol.
      if (ffs_to_next_ref != 0) {
        AssertFatal(ffs_to_next_ref != 1, "DMRS / PTRS symbol should not be processed here\n");
        const uint next_ref = symbol + ffs_to_next_ref - 1;
        if (last_processed_ref != next_ref) {
          if (IS_BIT_SET(p->dmrs_symb_pos, next_ref)) {
            next_base_phase = (cd_t){.r = 1.0};
            if (curr_dmrs == next_ref) {
              // Use next DMRS to compute step_phase. Only till the first DMRS symbol.
              step_phase = get_interpolate_step_phase(base_phase, next_base_phase, next_ref - base_symbol);
            }
            // Else use old step_phase. Extrapolate till next DMRS.
          } else if (IS_BIT_SET(p->ptrs_symb_pos, next_ref)) {
            // Use next PTRS to compute step_phase.
            next_base_phase = estimate_ptrs_symbol_phase(rxdataF, chest, curr_dmrs, next_ref, k_rb_ref, p);
            step_phase = get_interpolate_step_phase(base_phase, next_base_phase, next_ref - base_symbol);
            processed_ptrs++;
          } else {
            DevAssert(0);
          }
          last_processed_ref = next_ref;
        }
      }
    }

    // Interpolate/extrapolate CPE for this symbol.
    // For reference symbols themselves, base_symbol == symbol, so this reduces to base_phase unchanged.
    const uint num_steps = symbol - base_symbol;
    cpe[symbol] = cd2c16(add_cpx_phasor(base_phase, step_phase * num_steps), amp);
    LOG_D(PHY, "symbol %d: cpe %d + j%d\n", symbol, cpe[symbol].r, cpe[symbol].i);
  }
  return processed_ptrs;
}

uint nr_ptrs_run(ptrs_proc_t *p,
                 uint8_t ptrs_time_density,
                 const c16_t *rxdataF,
                 const c16_t *chest,
                 c16_t cpe[NR_SYMBOLS_PER_SLOT])
{
  p->ptrs_symb_pos = get_ptrs_symb_idx(p->num_symb, p->start_symb, 1 << ptrs_time_density, p->dmrs_symb_pos);
  const uint ret = nr_ptrs_process_slot(rxdataF, chest, cpe, p);
  AssertFatal(ret == __builtin_popcount(p->ptrs_symb_pos), "Error in PTRS processing\n");
  return get_num_ptrs_re_symbol(p->num_rb, p->k_ptrs, p->rnti);
}
