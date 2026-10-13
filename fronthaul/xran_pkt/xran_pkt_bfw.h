/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "common/platform_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Length in bytes of the section extension at ext, with avail bytes left in the packet. extLen is 16 bits for
// extType 11, 19 and 20 and 8 bits otherwise (CUS v21 7.6.2.3). Returns -1 if the extension header doesn't fit,
// extLen is 0, or the extension runs past avail.
int xran_section_ext_len(const uint8_t *ext, size_t avail);

// Section Extension 1: Beamforming weights (5.4.7.1).
// Decodes exactly n_weights (bfwI, bfwQ) pairs into weights_out. Returns n_weights, or -1 if the
// extension is malformed, uses an unsupported bfwCompMeth, or its extLen doesn't match n_weights.
int xran_decode_bfw_ext1(const uint8_t *ext, size_t len, int n_weights, c16_t *weights_out);

// Section Extension 11 header fields (CUS v21 7.7.11). bfwCompMeth/bfwIqWidth are only set if !disableBFWs.
typedef struct {
  bool disableBFWs; // only beamIds are carried, no weights
  bool RAD;
  uint8_t bundleOffset;
  uint8_t numBundPrb;
  uint8_t bfwCompMeth;
  uint8_t bfwIqWidth; // 1..16
} xran_bfw_ext11_hdr_t;

// Section Extension 11: Flexible beamforming weights (CUS v21 7.7.11), for a section of num_prb PRBs.
// The bundle count is ceil((bundleOffset + num_prb) / numBundPrb); bundle b gets its beamId in beam_ids_out[b]
// and, unless disableBFWs, exactly n_weights (bfwI, bfwQ) pairs in weights_out[b * n_weights ...].
// Both outputs must have room for max_bundles bundles. Returns the bundle count, or -1 if the extension is
// malformed, uses an unsupported bfwCompMeth, has more than max_bundles bundles, or its extLen doesn't match.
int xran_decode_bfw_ext11(const uint8_t *ext,
                          size_t len,
                          int num_prb,
                          int n_weights,
                          int max_bundles,
                          xran_bfw_ext11_hdr_t *hdr_out,
                          uint16_t *beam_ids_out,
                          c16_t *weights_out);

#ifdef __cplusplus
}
#endif
