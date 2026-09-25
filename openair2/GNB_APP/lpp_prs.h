/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef LPP_PRS_H_
#define LPP_PRS_H_

#include <stddef.h>
#include <stdint.h>

struct PHY_VARS_gNB_s;
struct gNB_MAC_INST_s;

void print_lpp_nr_dl_tdoa_assistance_data(const struct PHY_VARS_gNB_s *gNB,
                                          const struct gNB_MAC_INST_s *mac,
                                          size_t muting_pattern1_length,
                                          size_t muting_pattern2_length);
void configure_rrc_possib_prs_assistance_data(const struct PHY_VARS_gNB_s *gNB,
                                              struct gNB_MAC_INST_s *mac,
                                              size_t muting_pattern1_length,
                                              size_t muting_pattern2_length,
                                              uint32_t periodicity_frames);

#endif /* LPP_PRS_H_ */
