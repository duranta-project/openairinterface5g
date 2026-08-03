/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __NR_CHANNEL_COMPENSATION__H__
#define __NR_CHANNEL_COMPENSATION__H__

#include "PHY/impl_defs_top.h"

/**
 * @brief Common channel compensation function shared by DL (PDSCH) and UL (PUSCH) paths.
 *
 * Computes matched-filter output (rxComp) and channel magnitude arrays used for LLR
 * computation. MRC across Rx antennas is performed inline: for each layer, contributions
 * from all Rx antennas are accumulated into rxComp[layer * nb_rx_ant][symbol * buffer_length].
 * Uses AVX2 (256-bit SIMD) for throughput.
 *
 * @param buffer_length   Number of complex samples per symbol (must be a multiple of 8);
 *                        governs the loop count and the inner dim of rxFext/chFext.
 * @param pdsch_buf_size_max     Inner dimension of ch_maga/ch_magb/ch_magc/rho arrays.
 *                        Pass the pre-allocated worst-case size for DL per-actor scratch
 *                        buffers so that multi-layer row strides are correct.
 * @param nb_rx_ant       Number of Rx antennas
 * @param nb_layers       Number of spatial layers
 * @param rxFext          Extracted received signal [nb_rx_ant][buffer_length]
 * @param chFext          Extracted channel estimates [nb_layers][nb_rx_ant][buffer_length]
 * @param ch_maga         Output magnitude array for threshold 'a' [nb_layers][pdsch_buf_size_max]
 * @param ch_magb         Output magnitude array for threshold 'b' [nb_layers][pdsch_buf_size_max]
 * @param ch_magc         Output magnitude array for threshold 'c' [nb_layers][pdsch_buf_size_max]
 * @param rxComp          Output compensated signal; row [l * nb_rx_ant] holds the MRC result
 *                        for layer l at offset [symbol * buffer_length]
 * @param rho             Tx-correlation matrix [nb_layers][nb_layers][pdsch_buf_size_max], or NULL
 * @param mod_order       Modulation order (2=QPSK, 4=16QAM, 6=64QAM, 8=256QAM)
 * @param symbol          OFDM symbol index (used to compute offset into rxComp rows)
 * @param output_shift    Right-shift applied after each complex multiply
 */
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
                             uint32_t output_shift);

/**
 * @brief Fused single-layer inner RX: MRC channel compensation + per-RE LLR, tiled.
 *
 * Equivalent to {nr_channel_compensation(nb_layers==1) + nr_dlsch_llr} but processes the symbol
 * in L1-resident RE-tiles, so the compensated symbols and channel magnitudes are never written
 * to full-symbol arrays (eliminates the rxComp/mag DRAM round-trip). Bit-exact with the unfused
 * path. Single-layer only (no rho / no MMSE inversion); the caller must exclude the PTRS case.
 *
 * @param length          Number of valid REs to demap this symbol
 * @param buffer_length   Padded row stride of rxFext/chFext (multiple of 16)
 * @param nb_rx_ant       Number of Rx antennas
 * @param rxFext          Extracted received signal [nb_rx_ant][buffer_length]
 * @param chFext          Extracted channel estimates for the single layer [nb_rx_ant][buffer_length]
 * @param mod_order       Modulation order (2/4/6/8)
 * @param output_shift    Right-shift applied after each complex multiply (log2_maxh)
 * @param llr             Output LLR buffer (length*mod_order int16, contiguous per RE)
 * @param scramble        Optional descrambling sequence (length*mod_order int16 of +-1): when
 *                        non-NULL, the LLRs are multiplied by it in-place as they are stored
 *                        (descrambling folded into the store). NULL => raw LLR. Single-layer only:
 *                        per-layer LLR order equals the codeword bit order, so no demap is needed.
 */
void nr_inner_rx_1layer(uint32_t length,
                        uint32_t buffer_length,
                        int nb_rx_ant,
                        c16_t rxFext[nb_rx_ant][buffer_length],
                        c16_t chFext[nb_rx_ant][buffer_length],
                        int mod_order,
                        int output_shift,
                        int16_t *llr,
                        const int16_t *scramble);

/** @brief Register-fused single-layer inner RX: per-block MRC + mag + LLR in registers, no tile
 * scratch and no per-tile LLR call (bit-exact with nr_inner_rx_1layer). */
void nr_inner_rx_1layer_reg(uint32_t length,
                            uint32_t buffer_length,
                            int nb_rx_ant,
                            c16_t rxFext[nb_rx_ant][buffer_length],
                            c16_t chFext[nb_rx_ant][buffer_length],
                            int mod_order,
                            int output_shift,
                            int16_t *llr);

/**
 * @brief Fused 2-layer near-ML inner RX: MRC compensation + rho build + joint ML-LLR, tiled.
 *
 * Equivalent to {nr_channel_compensation(nb_layers==2) + nr_compute_ML_llr}, processed in
 * L1-resident RE-tiles (the two compensated streams, per-layer magnitudes and off-diagonal rho
 * never touch full-symbol arrays). Requires RE-sub-range-composable 2-layer LLR kernels. Only
 * mag_a is built (nr_compute_ML_llr derives the other thresholds). 256-bit only for now. Caller
 * must exclude the PTRS case.
 *
 * @param llr0,llr1  Per-layer LLR outputs (per-layer contiguous). Used when llr_cw == NULL.
 * @param llr_cw     Optional codeword output: when non-NULL, the two layers' LLRs are written
 *                   layer-demapped (interleaved [RE-l0, RE-l1] per RE) directly into it, folding
 *                   the layer demapping into the store; llr0/llr1 are then unused.
 * @param scramble   Optional descrambling sequence (length*2*mod_order int16 of +-1) applied at the
 *                   store when llr_cw is non-NULL; NULL leaves the codeword raw. Ignored if
 *                   llr_cw == NULL.
 */
void nr_inner_rx_2layer_ml(uint32_t length,
                           uint32_t buffer_length,
                           int nb_rx_ant,
                           c16_t rxFext[nb_rx_ant][buffer_length],
                           c16_t chFext[2][nb_rx_ant][buffer_length],
                           int mod_order,
                           int output_shift,
                           int16_t *llr0,
                           int16_t *llr1,
                           int16_t *llr_cw,
                           const int16_t *scramble);

/**
 * @brief Shared post-extraction inner RX dispatch (UE PDSCH + gNB PUSCH common detector set).
 *
 * Picks the detector for (nb_layer, mod_order, do_ml, fuse_mode) and either writes the layer-demapped
 * (+ descrambled, if scramble!=NULL) codeword to llr_cw, or — when llr_cw==NULL — leaves per-layer
 * LLRs in layer_scratch[] for the caller to demap. Compensation must already be done for the
 * non-fused paths (rxComp/mag_a-c per layer, rho01/rho10 for 2-layer); fuse_mode==1 does MRC inline.
 * do_ml + lbest256 encode the 2-layer ML gate (gNB: do_ml=true, lbest256=gnb_lbest; UE: do_ml, ml256).
 * fuse_mode: 0=standalone LLR, 1=tiled fused kernels, 2=register-fused (returns false, caller-only).
 *
 * @return true if handled; false for caller-specific paths (register-fused single layer, 2-layer
 *         non-ML / 256QAM-MMSE, 3-layer, >2 layers) — the caller runs those and demaps as needed.
 */
bool nr_inner_rx(uint32_t length,
                 uint32_t buffer_length,
                 int nb_rx_ant,
                 int nb_layer,
                 int mod_order,
                 c16_t rxFext[nb_rx_ant][buffer_length],
                 c16_t chFext[nb_layer][nb_rx_ant][buffer_length],
                 c16_t *rxComp[nb_layer],
                 c16_t *mag_a[nb_layer],
                 c16_t *mag_b[nb_layer],
                 c16_t *mag_c[nb_layer],
                 c16_t *rho01,
                 c16_t *rho10,
                 int output_shift,
                 int fuse_mode,
                 bool do_ml,
                 bool lbest256,
                 int16_t *layer_scratch[nb_layer],
                 const int16_t *scramble,
                 int16_t *llr_cw);

#endif /* __NR_CHANNEL_COMPENSATION__H__ */
