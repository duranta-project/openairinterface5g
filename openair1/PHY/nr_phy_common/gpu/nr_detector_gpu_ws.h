/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_DETECTOR_GPU_WS_H
#define NR_DETECTOR_GPU_WS_H

#include <stdint.h>

// scratch of the L-best adaptive window: the entries searched in full (RE index, or symbol / RE / layer for the
// fused inner RX) and their count
struct nr_gpu_det_ws {
  uint32_t *idx;
  uint32_t *cnt;
  uint32_t max_re;
};

#endif
