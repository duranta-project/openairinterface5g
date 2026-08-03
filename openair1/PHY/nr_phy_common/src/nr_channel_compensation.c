/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_channel_compensation.h"
#include "nr_phy_common.h"
#include "bits.h"
#include <complex.h>
#include "PHY/sse_intrin.h"
#include "PHY/impl_defs_top.h"
#ifdef __aarch64__
#define USE_128BIT
#endif

// ---- width-parameterized MRC compensation core (AVX2 / SSE->NEON; +AVX-512 later) ----
// Same source instantiated per width from nr_channel_comp_simd.c.inc; the public entry dispatches.
#define NRLB_W 256
#include "nr_lbest_simd_width.h"
#include "nr_channel_comp_simd.c.inc"
#include "nr_inner_rx_1layer_simd.c.inc"
#include "nr_inner_rx_1layer_reg_simd.c.inc"
#include "nr_inner_rx_2layer_ml_simd.c.inc"
#undef NRLB_W
#define NRLB_W 128
#include "nr_lbest_simd_width.h"
#include "nr_channel_comp_simd.c.inc"
#include "nr_inner_rx_1layer_simd.c.inc"
#include "nr_inner_rx_1layer_reg_simd.c.inc"
#include "nr_inner_rx_2layer_ml_simd.c.inc"
#undef NRLB_W

// x86 width selection (cached): 1 = w128 (SSE->NEON regression path, OAI_COMP_W128), 0 = w256 (default).
// aarch64 always runs w128 (SIMDe maps 256-bit to 2x128 NEON, slower than native 128).
static int nr_comp_simd_width_mode(void)
{
  static int m = -1;
  if (m < 0)
    m = getenv("OAI_COMP_W128") ? 1 : 0;
  return m;
}

void nr_channel_compensation(uint32_t buffer_length,
                             uint32_t pdsch_buf_size_max,
                             int nb_rx_ant,
                             int nb_layers,
                             c16_t rxFext[nb_rx_ant][buffer_length],
                             c16_t chFext[nb_layers][nb_rx_ant][buffer_length],
                             c16_t ch_maga[nb_layers][pdsch_buf_size_max],
                             c16_t ch_magb[nb_layers][pdsch_buf_size_max],
                             c16_t ch_magc[nb_layers][pdsch_buf_size_max],
                             c16_t **rxComp,
                             c16_t (*rho)[nb_layers][pdsch_buf_size_max],
                             int mod_order,
                             uint32_t symbol,
                             uint32_t output_shift)
{
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE) || defined(__aarch64__)
  nr_channel_compensation_w128(buffer_length, pdsch_buf_size_max, nb_rx_ant, nb_layers, rxFext, chFext,
                               ch_maga, ch_magb, ch_magc, rxComp, rho, mod_order, symbol, output_shift);
#else
  if (nr_comp_simd_width_mode() == 1)
    nr_channel_compensation_w128(buffer_length, pdsch_buf_size_max, nb_rx_ant, nb_layers, rxFext, chFext,
                                 ch_maga, ch_magb, ch_magc, rxComp, rho, mod_order, symbol, output_shift);
  else
    nr_channel_compensation_w256(buffer_length, pdsch_buf_size_max, nb_rx_ant, nb_layers, rxFext, chFext,
                                 ch_maga, ch_magb, ch_magc, rxComp, rho, mod_order, symbol, output_shift);
#endif
}

/* Fused single-layer inner RX: MRC channel compensation + per-RE LLR, TILED so the compensated
 * symbols and channel magnitudes live only in L1 scratch and are never materialized as
 * full-symbol arrays (no rxComp/mag DRAM round-trip). Bit-exact with the unfused
 * {nr_channel_compensation(nb_layers==1) + nr_dlsch_llr} path: identical per-RE math, identical
 * Rx-antenna accumulation order. Single-layer only (no rho, no MMSE inversion); the caller must
 * gate out the PTRS case, which sits between compensation and LLR. */
void nr_inner_rx_1layer(uint32_t length,
                        uint32_t buffer_length,
                        int nb_rx_ant,
                        c16_t rxFext[nb_rx_ant][buffer_length],
                        c16_t chFext[nb_rx_ant][buffer_length],
                        int mod_order,
                        int output_shift,
                        int16_t *llr,
                        const int16_t *scramble)
{
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE) || defined(__aarch64__)
  nr_inner_rx_1layer_w128(length, buffer_length, nb_rx_ant, rxFext, chFext, mod_order, output_shift, llr, scramble);
#else
  if (nr_comp_simd_width_mode() == 1)
    nr_inner_rx_1layer_w128(length, buffer_length, nb_rx_ant, rxFext, chFext, mod_order, output_shift, llr, scramble);
  else
    nr_inner_rx_1layer_w256(length, buffer_length, nb_rx_ant, rxFext, chFext, mod_order, output_shift, llr, scramble);
#endif
}

// Register-fused variant: no tile scratch, no per-tile LLR call (per-block MRC+mag+LLR in regs).
void nr_inner_rx_1layer_reg(uint32_t length,
                            uint32_t buffer_length,
                            int nb_rx_ant,
                            c16_t rxFext[nb_rx_ant][buffer_length],
                            c16_t chFext[nb_rx_ant][buffer_length],
                            int mod_order,
                            int output_shift,
                            int16_t *llr)
{
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE) || defined(__aarch64__)
  nr_inner_rx_1layer_reg_w128(length, buffer_length, nb_rx_ant, rxFext, chFext, mod_order, output_shift, llr);
#else
  if (nr_comp_simd_width_mode() == 1)
    nr_inner_rx_1layer_reg_w128(length, buffer_length, nb_rx_ant, rxFext, chFext, mod_order, output_shift, llr);
  else
    nr_inner_rx_1layer_reg_w256(length, buffer_length, nb_rx_ant, rxFext, chFext, mod_order, output_shift, llr);
#endif
}

/* Fused 2-layer near-ML inner RX: MRC compensation + Gram off-diagonal (rho) build + joint ML-LLR
 * (nr_compute_ML_llr), TILED so the two compensated streams, the two per-layer magnitudes and the
 * two off-diagonal rho vectors live only in L1 scratch. Equivalent to
 * {nr_channel_compensation(nb_layers==2) + nr_compute_ML_llr}. nr_compute_ML_llr takes one
 * magnitude per layer (n1-scaled) and derives the other QAM thresholds internally, so only mag_a
 * is built. Requires the 2-layer LLR kernels to be RE-sub-range-composable (RE-exact stores).
 * 256-bit only for now (width parameterization TODO). Caller gates out PTRS. */
void nr_inner_rx_2layer_ml(uint32_t length,
                           uint32_t buffer_length,
                           int nb_rx_ant,
                           c16_t rxFext[nb_rx_ant][buffer_length],
                           c16_t chFext[2][nb_rx_ant][buffer_length],
                           int mod_order,
                           int output_shift,
                           int16_t *llr0,
                           int16_t *llr1)
{
#if defined(SIMDE_ARM_NEON_A64V8_NATIVE) || defined(__aarch64__)
  nr_inner_rx_2layer_ml_w128(length, buffer_length, nb_rx_ant, rxFext, chFext, mod_order, output_shift, llr0, llr1);
#else
  if (nr_comp_simd_width_mode() == 1)
    nr_inner_rx_2layer_ml_w128(length, buffer_length, nb_rx_ant, rxFext, chFext, mod_order, output_shift, llr0, llr1);
  else
    nr_inner_rx_2layer_ml_w256(length, buffer_length, nb_rx_ant, rxFext, chFext, mod_order, output_shift, llr0, llr1);
#endif
}
