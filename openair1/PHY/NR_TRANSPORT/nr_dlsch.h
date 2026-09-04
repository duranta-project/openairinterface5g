/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief data structures for PDSCH/DLSCH/PUSCH/ULSCH physical and transport channel descriptors (TX/RX)
 */

#ifndef __NR_DLSCH__H
#define __NR_DLSCH__H

#include "PHY/defs_gNB.h"
#include "time_meas.h"

void nr_generate_pdsch(PHY_VARS_gNB *gNB, int n_dlsch, NR_gNB_DLSCH_t *dlsch_array, int frame, int slot);

int nr_dlsch_encoding(PHY_VARS_gNB *gNB,
                      int n_dlsch,
                      NR_gNB_DLSCH_t *dlsch_array,
                      int frame,
                      uint8_t slot,
                      unsigned char *output);

void dump_pdsch_stats(FILE *fd,PHY_VARS_gNB *gNB);

#endif
