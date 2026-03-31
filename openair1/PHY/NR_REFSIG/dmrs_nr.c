/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**********************************************************************
*
* FILENAME    :  dmrs_nr.c
*
* MODULE      :  demodulation reference signals
*
* DESCRIPTION :  generation of dmrs sequences
*                3GPP TS 38.211
*
************************************************************************/

#include "PHY/NR_REFSIG/ss_pbch_nr.h"
#include "PHY/NR_REFSIG/dmrs_nr.h"
#include "log.h"
#include "nfapi/open-nFAPI/nfapi/public_inc/nfapi_nr_interface.h"
#include "bits.h"

uint8_t allowed_xlsch_re_in_dmrs_symbol(uint16_t k,
                                        uint16_t start_sc,
                                        uint16_t ofdm_symbol_size,
                                        uint8_t numDmrsCdmGrpsNoData,
                                        uint8_t dmrs_type)
{
  uint8_t delta;
  uint16_t diff;
  if (k > start_sc)
    diff = k-start_sc;
  else
    diff = (ofdm_symbol_size-start_sc)+k;
  for (int i = 0; i<numDmrsCdmGrpsNoData; i++){
    if  (dmrs_type == NFAPI_NR_DMRS_TYPE1) {
      delta = i;
      if (((diff)%2)  == delta)
        return (0);
    }
    else {
      delta = i << 1;
      if (((diff % 6) == delta) || ((diff % 6) == (delta + 1)))
        return (0);
    }
  }
  return (1);
}


/*******************************************************************
*
* NAME :         pseudo_random_gold_sequence
*
* PARAMETERS :
*
* RETURN :       generate pseudo-random sequence which is a length-31 Gold sequence
*
* DESCRIPTION :  3GPP TS 38.211 5.2.1 Pseudo-random sequence generation
*                Sequence generation is a length-31 Gold sequence
*
*********************************************************************/

#define NC                     (1600)
#define GOLD_SEQUENCE_LENGTH   (31)

int pseudo_random_sequence(int M_PN, uint32_t *c, uint32_t cinit)
{
  int n;
  int size_x =  NC + GOLD_SEQUENCE_LENGTH + M_PN;
  uint32_t *x1;
  uint32_t *x2;

  x1 = calloc(size_x, sizeof(uint32_t));

  if (x1 == NULL) {
    msg("Fatal error: memory allocation problem \n");
    assert(0);
  }

  x2 = calloc(size_x, sizeof(uint32_t));

  if (x2 == NULL) {
    free(x1);
    msg("Fatal error: memory allocation problem \n");
    assert(0);
  }

  x1[0] = 1;  /* init first m sequence */

  /* cinit allows to initialise second m-sequence x2 */
  for (n = 0; n < GOLD_SEQUENCE_LENGTH; n++) {
     x2[n] = (cinit >> n) & 0x1;
  }

  for (n = 0; n < (NC + M_PN); n++) {
    x1[n+31] = (x1[n+3] + x1[n])%2;
    x2[n+31] = (x2[n+3] + x2[n+2] + x2[n+1] + x2[n])%2;
  }

  for (int n = 0; n < M_PN; n++) {
    c[n] = (x1[n+NC] + x2[n+NC])%2;
  }

  free(x1);
  free(x2);

  return 0;
}

/*******************************************************************
*
* NAME :         pseudo_random_sequence_optimised
*
* PARAMETERS :
*
* RETURN :       generate pseudo-random sequence which is a length-31 Gold sequence
*
* DESCRIPTION :  3GPP TS 38.211 5.2.1 Pseudo-random sequence generation
*                Sequence generation is a length-31 Gold sequence
*                This is an optimized function based on bitmap variables
*
*                x1(0)=1,x1(1)=0,...x1(30)=0,x1(31)=1
*                x2 <=> cinit, x2(31) = x2(3)+x2(2)+x2(1)+x2(0)
*                x2 <=> cinit = sum_{i=0}^{30} x2(i)2^i
*                c(n) = x1(n+Nc) + x2(n+Nc) mod 2
*
*                                             equivalent to
* x1(n+31) = (x1(n+3)+x1(n))mod 2                   <=>      x1(n) = x1(n-28) + x1(n-31)
* x2(n+31) = (x2(n+3)+x2(n+2)+x2(n+1)+x2(n))mod 2   <=>      x2(n) = x2(n-28) + x2(n-29) + x2(n-30) + x2(n-31)
*
*********************************************************************/

void pseudo_random_sequence_optimised(unsigned int size, uint32_t *c, uint32_t cinit)
{
  unsigned int n,x1,x2;

  /* init of m-sequences */
  x1 = 1+ (1U<<31);
  x2 = cinit;
  x2=x2 ^ ((x2 ^ (x2>>1) ^ (x2>>2) ^ (x2>>3))<<31);

  /* skip first 50 double words of uint32_t (1600 bits) */
  for (n=1; n<50; n++) {
    x1 = (x1>>1) ^ (x1>>4);
    x1 = x1 ^ (x1<<31) ^ (x1<<28);
    x2 = (x2>>1) ^ (x2>>2) ^ (x2>>3) ^ (x2>>4);
    x2 = x2 ^ (x2<<31) ^ (x2<<30) ^ (x2<<29) ^ (x2<<28);
  }

  for (n=0; n<size; n++) {
    x1 = (x1>>1) ^ (x1>>4);
    x1 = x1 ^ (x1<<31) ^ (x1<<28);
    x2 = (x2>>1) ^ (x2>>2) ^ (x2>>3) ^ (x2>>4);
    x2 = x2 ^ (x2<<31) ^ (x2<<30) ^ (x2<<29) ^ (x2<<28);
    c[n] = x1^x2;
  }
}

/*******************************************************************
*
* NAME :         get_dmrs_freq_idx_ul
*
* PARAMETERS :   n : index of DMRS symbol
*                k_prime  : k_prime = {0,1}
*                delta : given by Tables 6.4.1.1.3-1 and 6.4.1.1.3-2
*                dmrs_type  : DMRS configuration type
*
* RETURN :       demodulation reference signal for PUSCH
*
* DESCRIPTION :  see TS 38.211 V15.4.0 Demodulation reference signals for PUSCH
*
*********************************************************************/

uint16_t get_dmrs_freq_idx_ul(uint16_t n, uint8_t k_prime, uint8_t delta, uint8_t dmrs_type) {

  uint16_t dmrs_idx;

  if (dmrs_type == pusch_dmrs_type1)
    dmrs_idx = ((n<<2)+(k_prime<<1)+delta);
  else
    dmrs_idx = (6*n+k_prime+delta);

  return dmrs_idx;
}

/*******************************************************************
*
* NAME :         get_dmrs_pbch
*
* PARAMETERS :   i_ssb : index of ssb/pbch beam
*                n_hf  : number of the half frame in which PBCH is transmitted in frame
*
* RETURN :       demodulation reference signal for PBCH
*
* DESCRIPTION :  see TS 38.211 7.4.1.4 Demodulation reference signals for PBCH
*
*********************************************************************/


/* return the position of next dmrs symbol in a slot */
int8_t get_next_dmrs_symbol_in_slot(uint16_t  ul_dmrs_symb_pos, uint8_t counter, uint8_t end_symbol)
{
  for(uint8_t symbol = counter; symbol < end_symbol; symbol++) {
    if((ul_dmrs_symb_pos >> symbol) & 0x01) {
      return symbol;
    }
  }
  return -1;
}

int8_t get_num_dmrs_re_per_rb(const uint8_t dmrs_type, const uint8_t num_cdm_grp_no_data)
{
  return (dmrs_type == NFAPI_NR_DMRS_TYPE1 ? 6 * num_cdm_grp_no_data : 4 * num_cdm_grp_no_data);
}

/* return the position of valid dmrs symbol in a slot for channel compensation */
int8_t get_valid_dmrs_idx_for_channel_est(uint16_t dmrs_symb_pos, uint8_t counter)
{
  int8_t  symbIdx = -1;
  /* if current symbol is DMRS then return this index */
  if(is_dmrs_symbol(counter, dmrs_symb_pos) == 1) {
    return counter;
  }
  /* find previous DMRS symbol */
  for(int8_t symbol = counter; symbol >= 0 ; symbol--) {
    if((1 << symbol & dmrs_symb_pos) > 0) {
      symbIdx = symbol;
      break;
    }
  }
  /* if there is no previous dmrs available then find the next possible*/
  if(symbIdx == -1) {
    symbIdx = get_next_dmrs_symbol_in_slot(dmrs_symb_pos, counter, 15);
  }
  return symbIdx;
}

/* perform averaging of channel estimates and store result in first symbol buffer */
void nr_chest_time_domain_avg(NR_DL_FRAME_PARMS *frame_parms,
                              int32_t **ch_estimates,
                              uint8_t num_symbols,
                              uint8_t start_symbol,
                              uint16_t dmrs_bitmap,
                              uint16_t num_rbs,
                              uint8_t nb_layers,
                              uint8_t num_streams)
{
  simde__m128i *ul_ch128_0;
  simde__m128i *ul_ch128_1;
  int16_t *ul_ch16_0;
  int total_symbols = start_symbol + num_symbols;
  int num_dmrs_symb = count_bits64_with_mask(dmrs_bitmap, start_symbol, total_symbols);
  int first_dmrs_symb = get_next_dmrs_symbol_in_slot(dmrs_bitmap, start_symbol, total_symbols);
  AssertFatal(first_dmrs_symb > -1, "No DMRS symbol present in this slot\n");
  for (int nl = 0; nl < nb_layers; nl++) {
    for (int aarx = 0; aarx < num_streams; aarx++) {
      int ch_offset = nl * num_streams + aarx;
      for (int symb = first_dmrs_symb + 1; symb < total_symbols; symb++) {
        ul_ch128_0 = (simde__m128i *)&ch_estimates[ch_offset][first_dmrs_symb * frame_parms->ofdm_symbol_size];
        if ((dmrs_bitmap >> symb) & 0x01) {
          ul_ch128_1 = (simde__m128i *)&ch_estimates[ch_offset][symb * frame_parms->ofdm_symbol_size];
          for (int rbIdx = 0; rbIdx < num_rbs; rbIdx++) {
            ul_ch128_0[0] = simde_mm_adds_epi16(ul_ch128_0[0], ul_ch128_1[0]);
            ul_ch128_0[1] = simde_mm_adds_epi16(ul_ch128_0[1], ul_ch128_1[1]);
            ul_ch128_0[2] = simde_mm_adds_epi16(ul_ch128_0[2], ul_ch128_1[2]);
            ul_ch128_0 += 3;
            ul_ch128_1 += 3;
          }
        }
      }
      ul_ch128_0 = (simde__m128i *)&ch_estimates[ch_offset][first_dmrs_symb * frame_parms->ofdm_symbol_size];
      if (num_dmrs_symb == 2) {
        for (int rbIdx = 0; rbIdx < num_rbs; rbIdx++) {
          ul_ch128_0[0] = simde_mm_srai_epi16(ul_ch128_0[0], 1);
          ul_ch128_0[1] = simde_mm_srai_epi16(ul_ch128_0[1], 1);
          ul_ch128_0[2] = simde_mm_srai_epi16(ul_ch128_0[2], 1);
          ul_ch128_0 += 3;
        }
      } else if (num_dmrs_symb == 4) {
        for (int rbIdx = 0; rbIdx < num_rbs; rbIdx++) {
          ul_ch128_0[0] = simde_mm_srai_epi16(ul_ch128_0[0], 2);
          ul_ch128_0[1] = simde_mm_srai_epi16(ul_ch128_0[1], 2);
          ul_ch128_0[2] = simde_mm_srai_epi16(ul_ch128_0[2], 2);
          ul_ch128_0 += 3;
        }
      } else if (num_dmrs_symb == 3) {
        ul_ch16_0 = (int16_t *)&ch_estimates[ch_offset][first_dmrs_symb * frame_parms->ofdm_symbol_size];
        for (int rbIdx = 0; rbIdx < num_rbs; rbIdx++) {
          ul_ch16_0[0] /= 3;
          ul_ch16_0[1] /= 3;
          ul_ch16_0[2] /= 3;
          ul_ch16_0[3] /= 3;
          ul_ch16_0[4] /= 3;
          ul_ch16_0[5] /= 3;
          ul_ch16_0[6] /= 3;
          ul_ch16_0[7] /= 3;
          ul_ch16_0[8] /= 3;
          ul_ch16_0[9] /= 3;
          ul_ch16_0[10] /= 3;
          ul_ch16_0[11] /= 3;
          ul_ch16_0[12] /= 3;
          ul_ch16_0[13] /= 3;
          ul_ch16_0[14] /= 3;
          ul_ch16_0[15] /= 3;
          ul_ch16_0[16] /= 3;
          ul_ch16_0[17] /= 3;
          ul_ch16_0[18] /= 3;
          ul_ch16_0[19] /= 3;
          ul_ch16_0[20] /= 3;
          ul_ch16_0[21] /= 3;
          ul_ch16_0[22] /= 3;
          ul_ch16_0[23] /= 3;
          ul_ch16_0 += 24;
        }
      } else
        AssertFatal((num_dmrs_symb < 5) && (num_dmrs_symb > 0), "Illegal number of DMRS symbols in the slot\n");
    }
  }
}

// to compute the first non dmrs symbol and the first symbol after the first set of consecutive DMRS symbols
void get_dmrs_uci_symbol_info(const uint8_t start_symbol,
                              const uint8_t num_symbols,
                              const uint16_t dmrs_map,
                              int *first_non_dmrs_sym,
                              int *after_dmrs_symb)
{
  // First non-DMRS symbol
  const uint16_t last_sym = start_symbol + num_symbols;
  for (uint_fast8_t s = start_symbol; s < last_sym; s++) {
    if (!is_dmrs_symbol(s, dmrs_map)) {
      *first_non_dmrs_sym = s;
      break;
    }
  }

  // Symbol after first consequtive DMRS symbol
  const int first_dmrs_sym = get_next_dmrs_symbol_in_slot(dmrs_map, start_symbol, last_sym);
  *after_dmrs_symb = first_dmrs_sym + 1;
  while (is_dmrs_symbol(*after_dmrs_symb, dmrs_map) && *after_dmrs_symb < last_sym) {
    (*after_dmrs_symb)++;
  }

  // Return relative symbol idx
  *first_non_dmrs_sym -= start_symbol;
  *after_dmrs_symb -= start_symbol;
}
