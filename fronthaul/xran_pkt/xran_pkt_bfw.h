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

// Section Extension 1: Beamforming weights
int xran_decode_bfw_ext1(const uint8_t *ext, size_t len, c16_t *weights_out, int max_weights);

#ifdef __cplusplus
}
#endif
