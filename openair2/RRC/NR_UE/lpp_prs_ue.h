/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef LPP_PRS_UE_H
#define LPP_PRS_UE_H

#include <stdbool.h>

#include "openair2/COMMON/nr_ue_prs_config.h"

struct LPP_NR_DL_PRS_AssistanceData_r16;

bool lpp_nr_prs_assistance_to_configuration(
    const struct LPP_NR_DL_PRS_AssistanceData_r16 *assistance,
    nr_prs_config_source_t source,
    nr_ue_prs_configuration_t *configuration);

#endif /* LPP_PRS_UE_H */
