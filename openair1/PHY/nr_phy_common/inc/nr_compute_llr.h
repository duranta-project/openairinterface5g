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

// Direct SIMD full ML 2-layer 256QAM (analogous to nr_qam64_llr_2layer for 64QAM).
// 8 bits/RE, PAM-16 constellation, Gray-coded. Computes LLRs for stream0 in presence
// of stream1 interference.
void nr_qam256_llr_2layer(c16_t *stream0_in,
                          c16_t *stream1_in,
                          c16_t *ch_mag,
                          c16_t *ch_mag_i,
                          int16_t *stream0_out,
                          c16_t *rho01,
                          uint32_t length);

// L-best (reduced-search) reference kernel for 2-layer 64QAM (one target layer).
// Floating-point scalar reference; L==64 reproduces the full max-log search.
// seed_lambda: 0.0f = ZF seed, = noise_var for MMSE-regularised seed.
void nr_qam64_llr_2layer_lbest(c16_t *stream0_in,
                               c16_t *stream1_in,
                               c16_t *ch_mag,
                               c16_t *ch_mag_i,
                               int16_t *stream0_out,
                               c16_t *rho01,
                               uint32_t length,
                               int L,
                               float seed_lambda);

// 3-layer hybrid L-best (target layer): project the most-orthogonal nuisance layer (Schur
// deflation), keep the other discrete, run the 2-layer conditional-slice LLR on the deflated
// pair. Qm = 4/6/8 (16/64/256QAM, same for all layers). rho_ab = h_a^H h_b.
void nr_qam_llr_3layer_hybrid(c16_t *zt, c16_t *zn1, c16_t *zn2,
                              c16_t *cmt, c16_t *cmn1, c16_t *cmn2,
                              c16_t *rho_tn1, c16_t *rho_tn2, c16_t *rho_n1n2,
                              int16_t *out, uint32_t length, int Qm, int L, float lambda);

// 3-layer full-ML reference (target layer): exact max-log over discrete (x_t,x_n1) + conditional
// x_n2 slice. For BLER comparison against the hybrid.
void nr_qam_llr_3layer_ml(c16_t *zt, c16_t *zn1, c16_t *zn2,
                          c16_t *cmt, c16_t *cmn1, c16_t *cmn2,
                          c16_t *rho_tn1, c16_t *rho_tn2, c16_t *rho_n1n2,
                          int16_t *out, uint32_t length, int Qm);

// Float reference L-best kernel for 2-layer 16QAM (building block for >2-layer hybrid).
// L==16 == full ML. seed_lambda: 0=ZF.
void nr_qam16_llr_2layer_lbest(c16_t *stream0_in,
                               c16_t *stream1_in,
                               c16_t *ch_mag,
                               c16_t *ch_mag_i,
                               int16_t *stream0_out,
                               c16_t *rho01,
                               uint32_t length,
                               int L,
                               float seed_lambda);

// Float reference L-best kernel for 2-layer 256QAM. L==256 == full ML search
// (analysis vehicle; no SIMD 256QAM full-search kernel exists). seed_lambda: 0=ZF.
void nr_qam256_llr_2layer_lbest(c16_t *stream0_in,
                                c16_t *stream1_in,
                                c16_t *ch_mag,
                                c16_t *ch_mag_i,
                                int16_t *stream0_out,
                                c16_t *rho01,
                                uint32_t length,
                                int L,
                                float seed_lambda);

// Fixed-point (Q15-input) L-best 2-layer 256QAM kernel (ZF seed). Integer twin of
// nr_qam256_llr_2layer_lbest; scale 16*sqrt(170), PAM-16 levels, 8 bits/RE.
// pattern: 0=5x5/25cand[default], 1=3x3/9cand, 2=5-plus/5cand, other=full256.
// Enabled via OAI_LBEST_Q15_256=1; pattern selected by OAI_LBEST_PAT256.
void nr_qam256_llr_2layer_lbest_q15(c16_t *stream0_in,
                                    c16_t *stream1_in,
                                    c16_t *ch_mag,
                                    c16_t *ch_mag_i,
                                    int16_t *stream0_out,
                                    c16_t *rho01,
                                    uint32_t length,
                                    int pattern);

// Fixed-point (Q15-input) L-best 2-layer 64QAM kernel (ZF seed). Integer twin of
// nr_qam64_llr_2layer_lbest; the SIMD-portable form of the reduced search.
void nr_qam64_llr_2layer_lbest_q15(c16_t *stream0_in,
                                   c16_t *stream1_in,
                                   c16_t *ch_mag,
                                   c16_t *ch_mag_i,
                                   int16_t *stream0_out,
                                   c16_t *rho01,
                                   uint32_t length,
                                   int L);

// int16/16-lane AVX2 L-best 2-layer 64QAM kernel (the optimized form).
// pattern: 0 = 3x3 (9 cand, full BLER), 1 = 6-cand (plus + toward-seed diagonal), 2 = 5-plus.
void nr_qam64_llr_2layer_lbest_q15_simd16(c16_t *stream0_in,
                                          c16_t *stream1_in,
                                          c16_t *ch_mag,
                                          c16_t *ch_mag_i,
                                          int16_t *stream0_out,
                                          c16_t *rho01,
                                          uint32_t length,
                                          int pattern);

// int16/16-lane AVX2 L-best 2-layer 256QAM kernel (PAM-16, 8 bits/RE).
// pattern: 0 = 5x5 (25 cand) [default], 1 = 3x3 (9 cand), 2 = 5-plus (5 cand).
// Enabled via OAI_LBEST_Q15_256=1; pattern selected by OAI_LBEST_PAT256.
void nr_qam256_llr_2layer_lbest_q15_simd16(c16_t *stream0_in,
                                           c16_t *stream1_in,
                                           c16_t *ch_mag,
                                           c16_t *ch_mag_i,
                                           int16_t *stream0_out,
                                           c16_t *rho01,
                                           uint32_t length,
                                           int pattern);

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
