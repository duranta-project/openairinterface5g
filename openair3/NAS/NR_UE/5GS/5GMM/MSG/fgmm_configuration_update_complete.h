/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FGMM_CONFIGURATION_UPDATE_COMPLETE_H
#define FGMM_CONFIGURATION_UPDATE_COMPLETE_H

#include "common/utils/ds/byte_array.h"

/* Configuration Update Complete (8.2.20 of 3GPP TS 24.501) has no IEs besides the 5GMM header */
int encode_fgmm_configuration_update_complete(byte_array_t *buffer);
int decode_fgmm_configuration_update_complete(const byte_array_t *buffer);

#endif /* FGMM_CONFIGURATION_UPDATE_COMPLETE_H */
