/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "fgmm_configuration_update_complete.h"
#include "common/utils/eq_check.h"

/** @brief Encode Configuration Update Complete (8.2.20 of 3GPP TS 24.501): no payload after the 5GMM header */
int encode_fgmm_configuration_update_complete(byte_array_t *buffer)
{
  (void)buffer;
  return 0;
}

/** @brief Decode Configuration Update Complete (8.2.20 of 3GPP TS 24.501): no payload is expected */
int decode_fgmm_configuration_update_complete(const byte_array_t *buffer)
{
  if (buffer->len != 0) {
    PRINT_ERROR("Configuration Update Complete: unexpected %ld bytes after the header\n", (long)buffer->len);
    return -1;
  }
  return 0;
}
