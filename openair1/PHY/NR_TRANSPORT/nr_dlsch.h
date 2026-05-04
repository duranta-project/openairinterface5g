/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief data structures for PDSCH/DLSCH/PUSCH/ULSCH physical and transport channel descriptors (TX/RX)
 */

#ifndef __NR_DLSCH__H
#define __NR_DLSCH__H

#include "PHY/defs_gNB.h"

void nr_generate_pdsch(PHY_VARS_gNB *gNB,
                       int n_dlsch,
                       NR_gNB_DLSCH_t *dlsch_array,
                       int frame,
                       int slot,
                       uint64_t *pdsch_phase_comp_prb_mask,
                       int prb_mask_words);

int nr_dlsch_encoding(PHY_VARS_gNB *gNB,
                      int n_dlsch,
                      NR_gNB_DLSCH_t *dlsch_array,
                      int frame,
                      uint8_t slot,
                      unsigned char *output,
                      time_stats_t *tinput,
                      time_stats_t *tinput_memcpy,
                      time_stats_t *tprep,
                      time_stats_t *tparity,
                      time_stats_t *toutput,
                      time_stats_t *tconcat,
                      time_stats_t *dlsch_rate_matching_stats,
                      time_stats_t *dlsch_interleaving_stats,
                      time_stats_t *dlsch_segmentation_stats);

void dump_pdsch_stats(FILE *fd,PHY_VARS_gNB *gNB);

#endif
