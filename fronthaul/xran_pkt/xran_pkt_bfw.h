/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common/platform_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Section Extension 1: Beamforming weights (5.4.7.1).
// Decodes exactly n_weights (bfwI, bfwQ) pairs into weights_out. Returns n_weights, or -1 if the
// extension is malformed, uses an unsupported bfwCompMeth, or its extLen doesn't match n_weights.
int xran_decode_bfw_ext1(const uint8_t *ext, size_t len, int n_weights, c16_t *weights_out);

#ifdef __cplusplus
}
#endif
