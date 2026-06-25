/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief functions used for PUSCH/ULSCH physical and transport channels for gNB
 */

#ifndef NR_ULSCH_H_
#define NR_ULSCH_H_

#include "PHY/defs_gNB.h"
#include "common/utils/threadPool/thread-pool.h"

#define NUMBER_FRAMES_PHY_UE_INACTIVE 10

void free_gNB_ulsch(NR_gNB_ULSCH_t *ulsch, uint16_t N_RB_UL);

NR_gNB_ULSCH_t new_gNB_ulsch(uint8_t max_ldpc_iterations, uint16_t N_RB_UL);

/*! \brief Perform PUSCH decoding for the whole current received TTI. TS 38.212 V15.4.0 subclause 6.2
  @param phy_vars_gNB, Pointer to PHY data structure for gNB
  @param frame_parms, Pointer to frame descriptor structure
  @param frame, current received frame
  @param nr_tti_rx, current received TTI
  @param ULSCH_ids, array of ULSCH ids
  @param nb_pusch, number of uplink shared channels
*/

int nr_ulsch_decoding(PHY_VARS_gNB *phy_vars_gNB,
                      NR_DL_FRAME_PARMS *frame_parms,
                      uint32_t frame,
                      uint8_t nr_tti_rx,
                      int *ULSCH_ids,
                      int nb_pusch);

void dump_pusch_stats(FILE *fd,PHY_VARS_gNB *gNB);

NR_gNB_SCH_STATS_t *get_ulsch_stats(PHY_VARS_gNB *gNB,NR_gNB_ULSCH_t *ulsch);

#endif /* NR_ULSCH_H_ */
