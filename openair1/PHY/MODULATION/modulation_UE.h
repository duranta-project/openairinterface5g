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
