/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FGMM_CONFIGURATION_UPDATE_COMMAND_H
#define FGMM_CONFIGURATION_UPDATE_COMMAND_H

#include <stdbool.h>
#include <stdint.h>
#include "common/utils/ds/byte_array.h"
#include "FGSMobileIdentity.h"

typedef struct {
  // Configuration update indication (Optional)
  bool ack_requested;
  bool registration_requested;
  // 5G-GUTI (Optional)
  FGSMobileIdentity *guti;
} fgmm_configuration_update_command_t;

int decode_fgmm_configuration_update_command(fgmm_configuration_update_command_t *msg, const byte_array_t *buffer);
int encode_fgmm_configuration_update_command(byte_array_t *buffer, const fgmm_configuration_update_command_t *msg);
bool eq_fgmm_configuration_update_command(const fgmm_configuration_update_command_t *a,
                                          const fgmm_configuration_update_command_t *b);
void free_fgmm_configuration_update_command(fgmm_configuration_update_command_t *msg);

#endif /* FGMM_CONFIGURATION_UPDATE_COMMAND_H */
