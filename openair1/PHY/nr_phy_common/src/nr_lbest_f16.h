/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_LBEST_F16_H
#define NR_LBEST_F16_H

#include <stdint.h>
#include "platform_types.h"

// FP16 2-layer receivers (nr_lbest_f16.c, built when NR_LBEST_F16 is defined: ARMv8.2 FP16 or AVX512-FP16),
// selected at run time by nr_lbest_f16_enabled().

/// L-best (pattern 1) / full ML (pattern 3) of the target layer, the arguments and LLR units of
/// nr_qam{64,256}_llr_2layer_lbest_f32_simd_w*
void nr_qam64_llr_2layer_lbest_f16(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, c16_t *, uint32_t, int, float);
void nr_qam256_llr_2layer_lbest_f16(c16_t *, c16_t *, c16_t *, c16_t *, int16_t *, c16_t *, uint32_t, int, float);

/// fused 2-layer inner RX (MRC + Gram + L-best / full ML), 64/256QAM: the arguments of nr_inner_rx_2layer_ml,
/// rxFext [nb_rx_ant][buffer_length], chFext [2][nb_rx_ant][buffer_length]; length >= one vector of REs
void nr_inner_rx_2layer_f16(uint32_t length,
                            uint32_t buffer_length,
                            int nb_rx_ant,
                            const c16_t *rxFext,
                            const c16_t *chFext,
                            c16_t cpe,
                            int mod_order,
                            int lbest,
                            float lambda,
                            int output_shift,
                            int16_t *llr0,
                            int16_t *llr1,
                            int16_t *llr_cw,
                            const int16_t *scramble);
/// fused single-layer inner RX (MRC + PAM LLRs of nr_compute_llr), Qm 2-8: the arguments of nr_inner_rx_1layer,
/// rxFext / chFext [nb_rx_ant][buffer_length]; length >= one vector of REs. Used with OAI_FUSE only: slower than the
/// int16 chain from int16 inputs (the conversions dominate a few operations per LLR) for the same BLER
void nr_inner_rx_1layer_f16(uint32_t length,
                            uint32_t buffer_length,
                            int nb_rx_ant,
                            const c16_t *rxFext,
                            const c16_t *chFext,
                            c16_t cpe,
                            int mod_order,
                            int output_shift,
                            int16_t *llr,
                            const int16_t *scramble);
/// REs per vector of the FP16 kernels (the minimum length of nr_inner_rx_2layer_f16)
int nr_lbest_f16_lanes(void);

#endif
