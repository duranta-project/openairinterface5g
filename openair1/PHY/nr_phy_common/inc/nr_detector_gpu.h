/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_DETECTOR_GPU_H
#define NR_DETECTOR_GPU_H

#include <stdint.h>
#include "platform_types.h"
#include "nr_inner_rx_offload.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * GPU (CUDA/HIP) 2-layer detectors, one thread per RE: bit-exact counterparts of the fp32 CPU kernels in
 * nr_lbest_simd.c (same inputs, layout and LLR units), so a CPU and a GPU receiver can be swapped and checked
 * against each other. All buffers are device-accessible (device or managed memory); calls are asynchronous on
 * `stream` (a gpuStream_t, NULL = default stream). Return 0, or a negative value on a launch error.
 */

/// Scratch for the L-best adaptive window (list of the REs searched in full), sized for max_re REs per call.
typedef struct nr_gpu_det_ws nr_gpu_det_ws_t;
nr_gpu_det_ws_t *nr_gpu_det_ws_alloc(uint32_t max_re);
void nr_gpu_det_ws_free(nr_gpu_det_ws_t *ws);

/// L-best (pattern 1: 3x3 window, full grid on the REs below NR_LBEST_WIDE_SINR*) or full ML (pattern 3), 64QAM or
/// 256QAM, for the target layer of nr_qam{64,256}_llr_2layer_lbest_f32_simd: stream0_in / ch_mag target, stream1_in
/// / ch_mag_i other layer, rho01 = h0^H h1. Two passes: the window on the other REs, then the full grid on the list.
/// QPSK / 16QAM: the exact ML of nr_qpsk_llr_2layer / nr_qam16_llr_2layer (pattern and ws unused).
int nr_gpu_llr_2layer_lbest(const c16_t *stream0_in,
                            const c16_t *stream1_in,
                            const c16_t *ch_mag,
                            const c16_t *ch_mag_i,
                            int16_t *out,
                            const c16_t *rho01,
                            uint32_t length,
                            int mod_order,
                            int pattern,
                            float lambda,
                            nr_gpu_det_ws_t *ws,
                            void *stream);

/// 2-layer linear MMSE (L = 1) of nr_llr_2layer_mmse_simd, Qm = 2..8.
int nr_gpu_llr_2layer_mmse(const c16_t *z0,
                           const c16_t *z1,
                           const c16_t *g00,
                           const c16_t *g11,
                           const c16_t *rho01,
                           int16_t *out,
                           uint32_t length,
                           int mod_order,
                           float lambda,
                           void *stream);

/// One OFDM symbol of a slot-batched 2-layer inner RX (nr_gpu_inner_rx_2layer): the offload interface's descriptor;
/// device-accessible pointers.
typedef nr_irx_sym_t nr_gpu_rx_sym_t;
#define NR_GPU_MAX_SYM NR_IRX_MAX_SYM
enum { NR_GPU_DET_MMSE = NR_IRX_DET_MMSE, NR_GPU_DET_LBEST = NR_IRX_DET_LBEST, NR_GPU_DET_ML = NR_IRX_DET_ML };

/// Front end of the 2-layer inner RX for one symbol, bit-exact with nr_channel_compensation (nb_layers = 2, rho):
/// out = {comp0, comp1, mag_a0, mag_a1, rho00, rho01, rho10, rho11}, length REs each (mag as (m, m)).
int nr_gpu_compensation_2layer(const c16_t *rxF,
                               const c16_t *chF,
                               uint32_t stride,
                               uint32_t length,
                               int nb_rx,
                               c16_t cpe,
                               int mod_order,
                               int output_shift,
                               c16_t *out[8],
                               void *stream);

/// Fused 2-layer inner RX of nsym (<= NR_GPU_MAX_SYM) symbols in one launch: front end (MRC with the CPE, magnitudes,
/// Gram), detector, the output shift of nr_compute_ML_llr / nr_compute_MMSE_llr, layer demap and descrambling into
/// each symbol's codeword slice. MMSE: Qm 2-8; L-best / ML: the exact int16 ML for QPSK / 16QAM, the fp32 metric for
/// 64QAM / 256QAM. ws must hold 2 x the REs of the call (L-best 64/256QAM).
int nr_gpu_inner_rx_2layer(const nr_gpu_rx_sym_t *sym,
                           int nsym,
                           int nb_rx,
                           int mod_order,
                           int detector,
                           float lambda,
                           int output_shift,
                           nr_gpu_det_ws_t *ws,
                           void *stream);

#ifdef __cplusplus
}
#endif

#endif
