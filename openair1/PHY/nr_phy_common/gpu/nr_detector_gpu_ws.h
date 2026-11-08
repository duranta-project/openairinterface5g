/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_DETECTOR_GPU_WS_H
#define NR_DETECTOR_GPU_WS_H

#include <stdint.h>
#include "nr_inner_rx_offload.h"

// scratch of the L-best adaptive window: the entries searched in full (RE index, or symbol / RE / layer for the
// fused inner RX) and their count
struct nr_gpu_det_ws {
  uint32_t *idx;
  uint32_t *cnt;
  uint32_t max_re;
  // symbol descriptors of nr_gpu_inner_rx_2layer, mapped pinned (host side, device alias): by pointer, not as a kernel
  // argument, which the kernels would copy to per-thread scratch once they take an address inside it (50x slower)
  nr_irx_sym_t *desc, *desc_d;
};

#endif
