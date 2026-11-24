/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UE_CSI_H
#define NR_UE_CSI_H

#include <stdbool.h>
#include <stdint.h>
#include "NR_CSI-RS-ResourceMapping.h"
#include "nfapi/open-nFAPI/nfapi/public_inc/fapi_nr_ue_interface.h"

/* On failure the caller must not schedule the resource; the output is unchanged. */
bool configure_csi_resource_mapping(fapi_nr_dl_config_csirs_pdu_rel15_t *csirs_config_pdu,
                                    const NR_CSI_RS_ResourceMapping_t *resourceMapping,
                                    uint32_t bwp_size,
                                    uint32_t bwp_start);

bool nr_ue_csi_ratematching_is_supported(const fapi_nr_dl_config_dlsch_pdu_rel15_t *dlsch_pdu);

#endif
