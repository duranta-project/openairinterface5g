/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __NR_COMPUTE_LLR__H__
#define __NR_COMPUTE_LLR__H__

#include "PHY/impl_defs_top.h"

void nr_compute_llr(c16_t *rxdataF_comp,
                    c16_t *ch_mag,
                    c16_t *ch_magb,
                    c16_t *ch_magc,
                    int16_t *llr,
                    uint32_t nb_re,
                    uint8_t symbol,
                    uint8_t mod_order);

void nr_qpsk_llr_2layer(c16_t *stream0_in, c16_t *stream1_in, int16_t *stream0_out, c16_t *rho01, uint32_t length);

void nr_qam16_llr_2layer(c16_t *stream0_in,
                         c16_t *stream1_in,
                         c16_t *ch_mag,
                         c16_t *ch_mag_i,
                         int16_t *stream0_out,
                         c16_t *rho01,
                         uint32_t length);

void nr_qam64_llr_2layer(c16_t *stream0_in,
                         c16_t *stream1_in,
                         c16_t *ch_mag,
                         c16_t *ch_mag_i,
                         int16_t *stream0_out,
                         c16_t *rho01,
                         uint32_t length);

void nr_compute_ML_llr(c16_t *rxdataF_comp0,
                       c16_t *rxdataF_comp1,
                       c16_t *ch_mag0,
                       c16_t *ch_mag1,
                       int16_t *llr_layers0,
                       int16_t *llr_layers1,
                       c16_t *rho0,
                       c16_t *rho1,
                       uint32_t nb_re,
                       uint8_t mod_order);

uint8_t nr_mmse_2layers(c16_t **rxdataF_comp,
                        uint32_t buffer_length,
                        uint32_t pdsch_buf_size_max,
                        int nb_rx_ant,
                        int nb_layers,
                        c16_t ch_mag[nb_layers][pdsch_buf_size_max],
                        c16_t ch_magb[nb_layers][pdsch_buf_size_max],
                        c16_t ch_magc[nb_layers][pdsch_buf_size_max],
                        c16_t ch_estimates_ext[][nb_rx_ant][buffer_length],
                        unsigned short nb_rb,
                        unsigned char mod_order,
                        int shift,
                        unsigned char symbol,
                        int length,
                        uint32_t noise_var,
                        c16_t *rho00,
                        c16_t *rho01,
                        c16_t *rho10,
                        c16_t *rho11);

// Fused 2-layer linear MMSE + scalar LLR (L=1). = nr_mmse_2layers (Gram-fed equalize) + per-layer
// nr_compute_llr. UE: symbol=0 + pre-offset rxdataF_comp; gNB: symbol offset applied internally.
uint8_t nr_compute_MMSE_llr(c16_t **rxdataF_comp,
                            uint32_t buffer_length,
                            uint32_t pdsch_buf_size_max,
                            int nb_rx_ant,
                            int nb_layers,
                            c16_t ch_mag[nb_layers][pdsch_buf_size_max],
                            c16_t ch_magb[nb_layers][pdsch_buf_size_max],
                            c16_t ch_magc[nb_layers][pdsch_buf_size_max],
                            c16_t ch_estimates_ext[][nb_rx_ant][buffer_length],
                            unsigned short nb_rb,
                            unsigned char mod_order,
                            int shift,
                            unsigned char symbol,
                            int length,
                            uint32_t noise_var,
                            c16_t *rho00,
                            c16_t *rho01,
                            c16_t *rho10,
                            c16_t *rho11,
                            int16_t **llr);

#endif /* __NR_COMPUTE_LLR__H__ */
