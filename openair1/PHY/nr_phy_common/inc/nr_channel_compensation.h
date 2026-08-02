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
 */
void nr_inner_rx_1layer(uint32_t length,
                        uint32_t buffer_length,
                        int nb_rx_ant,
                        c16_t rxFext[nb_rx_ant][buffer_length],
                        c16_t chFext[nb_rx_ant][buffer_length],
                        int mod_order,
                        int output_shift,
                        int16_t *llr);

#endif /* __NR_CHANNEL_COMPENSATION__H__ */
