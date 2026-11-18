/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __MODULATION_DEFS__H__
#define __MODULATION_DEFS__H__
#include "PHY/defs_common.h"
#include "modulation_common.h"
#include "PHY/defs_UE.h"
#include "PHY/defs_nr_UE.h"
/** @addtogroup _PHY_MODULATION_
 * @{
*/



/*!
\brief This function implements the OFDM front end processor on reception (FEP)
\param phy_vars_ue Pointer to PHY variables
\param l symbol within slot (0..6/7)
\param Ns Slot number (0..19)
\param sample_offset offset within rxdata (points to beginning of subframe)
\param no_prefix if 1 prefix is removed by HW
\param reset_freq_est if non-zero it resets the frequency offset estimation loop
*/

int slot_fep(PHY_VARS_UE *phy_vars_ue,
             unsigned char l,
             unsigned char Ns,
             int sample_offset,
             int no_prefix,
	     int reset_freq_est);

int nr_slot_fep(PHY_VARS_NR_UE *ue,
                const NR_DL_FRAME_PARMS *frame_parms,
                unsigned int frame,
                unsigned int slot,
                unsigned int symbol,
                c16_t rxdataF[][frame_parms->samples_per_slot_wCP],
                enum nr_Link linktype,
                uint32_t sample_offset,
                c16_t **rxdata);

/* OFDM demodulation of one DL symbol of the slot the UE thread processes: from the samples of the slot in the RU ring
   (common_vars.rx_slot) in sync, else from common_vars.rxdata as nr_slot_fep() */
int nr_slot_fep_rx(PHY_VARS_NR_UE *ue,
                   const NR_DL_FRAME_PARMS *frame_parms,
                   unsigned int frame,
                   unsigned int slot,
                   unsigned int symbol,
                   c16_t rxdataF[][frame_parms->samples_per_slot_wCP]);

/* cache of the OFDM symbols demodulated from an RU ring shared by several UEs: their samples, so their demodulation, are
   the same for all the UEs of the RU, one of them computes it and the others copy it */
struct nr_fep_cache_s *nr_fep_cache_create(int nb_antennas_rx, int ofdm_symbol_size);
enum { NR_SHARED_SYMB, NR_SHARED_PDCCH_LLR, NR_SHARED_POLAR, NR_SHARED_TABLES };
#define NR_UE_SHARED_PARAMS 4
/* look for the entry of key and params at idx of a table of the cache: 1 = found and copied in dst (nparts x part_len),
   0 = absent and reserved for this UE, that computes it and gives it to nr_ue_shared_put(), -1 = neither (compute it
   without the cache) */
int nr_ue_shared_get(struct nr_fep_cache_s *c,
                     int table,
                     unsigned int idx,
                     uint64_t key,
                     const uint64_t params[NR_UE_SHARED_PARAMS],
                     int nparts,
                     int part_len,
                     c16_t *const dst[nparts]);
void nr_ue_shared_put(struct nr_fep_cache_s *c, int table, unsigned int idx, int nparts, int part_len, c16_t *const src[nparts]);
/* the same for a result identified by its input: key is a hash of the input match (match_len values), compared entirely
   on lookup; the result has len values */
int nr_ue_shared_get_match(struct nr_fep_cache_s *c,
                           int table,
                           uint64_t key,
                           const uint64_t params[NR_UE_SHARED_PARAMS],
                           const c16_t *match,
                           int match_len,
                           int len,
                           c16_t *dst);
void nr_ue_shared_put_match(struct nr_fep_cache_s *c, int table, uint64_t key, int match_len, int len, const c16_t *src);

int slot_fep_mbsfn(PHY_VARS_UE *phy_vars_ue,
                   unsigned char l,
                   int subframe,
                   int sample_offset,
                   int no_prefix);

int slot_fep_mbsfn_khz_1dot25(PHY_VARS_UE *phy_vars_ue,
                   int subframe,
                   int sample_offset);

int front_end_fft(PHY_VARS_UE *ue,
             unsigned char l,
             unsigned char Ns,
             int sample_offset,
             int no_prefix);

int front_end_chanEst(PHY_VARS_UE *ue,
             unsigned char l,
             unsigned char Ns,
            int reset_freq_est);

void apply_7_5_kHz(PHY_VARS_UE *phy_vars_ue,int32_t*txdata,uint8_t subframe);


int compute_BF_weights(int32_t **beam_weights, int32_t **calib_dl_ch_estimates, PRECODE_TYPE_t precode_type, int nb_ant, int nb_freq);



/** @}*/
#endif
