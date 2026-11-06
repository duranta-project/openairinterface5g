/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/***********************************************************************
*
* FILENAME    :  phy_frame_configuration_nr.h
*
* DESCRIPTION :  functions related to FDD/TDD configuration  for NR
*                see TS 38.213 11.1 Slot configuration
*                and TS 38.331 for RRC configuration
*
************************************************************************/

#include "PHY/defs_gNB.h"

#ifndef PHY_FRAME_CONFIG_NR_H
#define PHY_FRAME_CONFIG_NR_H

/*************** FUNCTIONS *****************************************/

/** @brief This function processes TDD dedicated configuration for NR
 *         by processing the tdd_slot_bitmap and period_cfg, and
 *         allocates memory and fills max_num_of_symbol_per_slot_list
 *         in the nfapi config request (cfg)
 *  @param cfg NR config request structure pointer
 *  @param fs  frame structure pointer
 *  @returns nb_periods_per_frame if TDD has been properly configurated
 *           -1 tdd configuration can not be done
 */
void set_tdd_config_nr(nfapi_nr_config_request_scf_t *cfg, frame_structure_t *fs);

/** \brief This function checks NR slot direction: downlink, uplink, mixed or both
 *  @param cfg NR cell configuration
 *  @param nr_frame : frame number
 *  @param nr_slot  : slot number
    @returns slot type, with both full-slot directions for FDD*/

nr_slot_type_t nr_slot_select(const nfapi_nr_config_request_scf_t *cfg, int nr_frame, int nr_slot);

void do_tdd_config_sim(PHY_VARS_gNB *gNB, int mu);

#endif  /* PHY_FRAME_CONFIG_NR_H */

