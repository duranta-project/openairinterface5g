/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Private to nr_phy_common: fixed-point constants shared by the scalar Q15 and the SIMD
 * 2-layer L-best kernels, and the per-width SIMD kernel entries (nr_lbest_simd.c).
 * These are the kernels' fixed-point scale factors, not general QAM amplitudes (those are
 * the QAMxx_n* values in PHY/impl_defs_top.h).
 */
#ifndef NR_LBEST_SIMD_H
#define NR_LBEST_SIMD_H

#include <stdint.h>
#include "PHY/TOOLS/tools_defs.h"

// ---- 64QAM (PAM-8), scalar Q15 reference kernel ----
#define NR_LBEST_Q_SQRT42_Q12 26545 // round(sqrt(42)        * 2^12)
#define NR_LBEST_Q_SQRT42_OVER4_Q14 26545 // round(sqrt(42)/4      * 2^14) (same constant)
#define NR_LBEST_Q_INVSQRT42_Q15 5057 // round(2^15 / sqrt(42))
// integer metric is 8*sqrt(42) (~51.8x) larger than the float-ref metric; bring
// the emitted LLR back onto the float-ref / SIMD scale.
#define NR_LBEST_Q_LLR_NUM 2533 // round(2^17 / (8*sqrt(42)))
#define NR_LBEST_Q_LLR_SHIFT 17

// ---- 256QAM (PAM-16), scalar Q15 reference kernel ----
#define NR_LBEST_Q_SQRT170_Q12 53405 // round(sqrt(170) * 2^12)
#define NR_LBEST_Q_SQRT170_OVER8_Q14 26690 // round(sqrt(170)/8 * 2^14): ch_mag -> Pp
#define NR_LBEST_Q_INVSQRT170_Q15 2514 // round(2^15 / sqrt(170))
// integer metric is 16*sqrt(170) (~208.6x) larger than float; bring LLR back to float scale.
#define NR_LBEST_Q_LLR256_NUM 628 // round(2^17 / (16*sqrt(170)))
#define NR_LBEST_Q_LLR256_SHIFT 17

// ---- SIMD kernels ----
#define NR_LBEST_Q_INVSQRT42_Q16 10112 // round(1/sqrt(42) * 2^16)
#define NR_LBEST_Q_PP1_OVER32_Q16 3318 // round(sqrt(42)/4/32 * 2^16) -> ||h||^2/32 via mulhi
#define NR_LBEST_SIMD16_SENT (-32768) // empty-subset sentinel (int16)
#define NR_LBEST_Q_SQRT170_OVER8_Q14_SIMD 26705 // round(sqrt(170)/8 * 2^14)
#define NR_LBEST_Q_PP1_OVER16_Q16_256 6676 // round(sqrt(170)/8/16 * 2^16)

// Per-width entries, one object per NRLB_W (suffix _w128/_w256/_w512), see nr_lbest_simd.c.
// lambda: seed regularization in the Pp/rho units (noise variance >> log2_maxh); 0 gives the ZF seed.
#define NR_LBEST_SIMD_PROTOTYPES(W)                                   \
  void nr_qam64_llr_2layer_lbest_q15_simd_w##W(c16_t *stream0_in,     \
                                               c16_t *stream1_in,     \
                                               c16_t *ch_mag,         \
                                               c16_t *ch_mag_i,       \
                                               int16_t *stream0_out,  \
                                               c16_t *rho01,          \
                                               uint32_t length,       \
                                               int pattern,           \
                                               float lambda);       \
  void nr_qam256_llr_2layer_lbest_f32_simd_w##W(c16_t *stream0_in,    \
                                                c16_t *stream1_in,    \
                                                c16_t *ch_mag,        \
                                                c16_t *ch_mag_i,      \
                                                int16_t *stream0_out, \
                                                c16_t *rho01,         \
                                                uint32_t length,      \
                                                int pattern,          \
                                                float lambda);      \
  void nr_qam256_llr_2layer_lbest_q15_simd_w##W(c16_t *stream0_in,    \
                                                c16_t *stream1_in,    \
                                                c16_t *ch_mag,        \
                                                c16_t *ch_mag_i,      \
                                                int16_t *stream0_out, \
                                                c16_t *rho01,         \
                                                uint32_t length,      \
                                                int pattern,          \
                                                float lambda);
#define NR_MMSE2L_SIMD_PROTOTYPES(W)                                                                                  \
  void nr_llr_2layer_mmse_simd_w##W(const c16_t *z0, const c16_t *z1, const c16_t *g00, const c16_t *g11, const c16_t *rho01, \
                                    int16_t *out, uint32_t length, int mod_order, float lambda);
NR_MMSE2L_SIMD_PROTOTYPES(128)
NR_MMSE2L_SIMD_PROTOTYPES(256)
NR_MMSE2L_SIMD_PROTOTYPES(512)
#undef NR_MMSE2L_SIMD_PROTOTYPES
NR_LBEST_SIMD_PROTOTYPES(128)
NR_LBEST_SIMD_PROTOTYPES(256)
NR_LBEST_SIMD_PROTOTYPES(512)
#undef NR_LBEST_SIMD_PROTOTYPES

#endif
