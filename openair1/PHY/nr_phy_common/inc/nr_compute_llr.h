/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __NR_COMPUTE_LLR__H__
#define __NR_COMPUTE_LLR__H__

#include "platform_types.h"

void nr_qpsk_llr(const c16_t *rxdataF_comp, int16_t *llr, uint32_t nb_re);
void nr_16qam_llr(const c16_t *rxdataF_comp, const c16_t *ch_mag_in, int16_t *llr, uint32_t nb_re);
void nr_64qam_llr(const c16_t *rxdataF_comp, const c16_t *ch_mag, const c16_t *ch_mag2, int16_t *llr, uint32_t nb_re);
void nr_256qam_llr(const c16_t *rxdataF_comp,
                   const c16_t *ch_mag,
                   const c16_t *ch_mag2,
                   const c16_t *ch_mag3,
                   int16_t *llr,
                   uint32_t nb_re);

/**
 * @brief Mod-order-dependent LLR-hotness offset for the 2-layer near-ML/L-best log2_maxh.
 *
 * The joint near-ML metric needs a mod-order-dependent fixed-point scale: 256QAM at -2, and lower
 * orders progressively different, else the ML LLRs saturate and the ML gain over linear MMSE is
 * lost (a single 256QAM-tuned -2 makes QPSK/16QAM ML worse than MMSE). Verified on TDL, z4, -gA.
 * lbest: the reduced-search L-best kernel is used (64QAM wants one bit cooler than full ML).
 */
int nr_ml_llr_maxh_off(int mod_order, int lbest);
/// L-best / MMSE seed regularization (noise variance in the compensated Pp/rho units) for log2_maxh = shift
float nr_2layer_seed_lambda(uint32_t nvar, int shift);
/// the 2-layer 64/256QAM L-best / full ML run in FP16 (aarch64 CPUs with FP16 vector arithmetic, OAI_LBEST_F16 != 0)
bool nr_lbest_f16_enabled(void);
/// Channel-estimation error relative to the channel power. The 2-layer MMSE seeds and the MMSE receiver regularize
/// with lambda + eps * (P0 + P1) / 2 per RE: at high SNR this error, not the thermal noise, limits the linear
/// estimate (TDL-A, 2 Rx, 64QAM MCS 27: +3-5 points of L-best throughput at 26-30 dB; neutral with 4 Rx).
#define NR_2LAYER_CSI_EPS 0.003f
/// 2-layer L-best adaptive window: an RE whose target-layer post-MMSE SINR, det / (lambda_eff (P1 + lambda_eff)) as
/// in the seed, is below this (linear) searches the full target grid instead of the 3x3 window. On ill-conditioned
/// 2x2 channels (2 Rx) the seed is too unreliable for the window and the bits it does not span fall back to the seed
/// LLR. TDL-A, 2 Rx, 15 LDPC iterations: full-ML throughput (64QAM MCS 20/27, 256QAM MCS 20) with ~5% (64QAM) /
/// ~10% (256QAM) of the REs searched in full, ~0% with 4 Rx; 5x5 to 9x9 windows recover little.
#define NR_LBEST_WIDE_SINR64 10.0f
#define NR_LBEST_WIDE_SINR256 20.0f
#if defined(__CUDACC__) || defined(__HIPCC__)
#define NR_LLR_HD __host__ __device__ // shared with the GPU detectors (nr_detector_gpu.cu)
#else
#define NR_LLR_HD
#endif
/// NR_LBEST_WIDE_SINR* test for one RE (P = ||h||^2 in the rho units); nb = bits per axis (3: 64QAM, 4: 256QAM)
NR_LLR_HD static inline int nr_lbest_wide_re(float P0, float P1, float rr, float ri, float lambda, int nb)
{
  const float lame = lambda + 0.5f * NR_2LAYER_CSI_EPS * (P0 + P1), P1s = P1 + lame;
  const float det = P0 * P1s - (rr * rr + ri * ri);
  return det < (nb == 4 ? NR_LBEST_WIDE_SINR256 : NR_LBEST_WIDE_SINR64) * (lame * P1s);
}

/// log2_approx(avgs) at the input level the 2-layer offsets were tuned at (nr_ulsim default level)
#define NR_2LAYER_LOG2_AVGS_NOMINAL 18
/**
 * @brief Channel-level part of log2_maxh for the 2-layer receivers: (log2 avgs) / 2 up to the
 * nominal input level, the full log2 avgs above it, so a hotter input cannot overflow the
 * quadratic 2-layer metrics. Add the receiver offset and the antenna-gain term.
 */
int nr_2layer_log2_maxh(int avgs);

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
// seed_lambda: 0.0f = ZF seed, = noise variance (Pp units) for the unbiased MMSE seed.
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
void nr_qam_llr_3layer_hybrid(c16_t *zt,
                              c16_t *zn1,
                              c16_t *zn2,
                              c16_t *cmt,
                              c16_t *cmn1,
                              c16_t *cmn2,
                              c16_t *rho_tn1,
                              c16_t *rho_tn2,
                              c16_t *rho_n1n2,
                              int16_t *out,
                              uint32_t length,
                              int Qm,
                              int L,
                              float lambda);

// 3-layer full-ML reference (target layer): exact max-log over discrete (x_t,x_n1) + conditional
// x_n2 slice. For BLER comparison against the hybrid.
void nr_qam_llr_3layer_ml(c16_t *zt,
                          c16_t *zn1,
                          c16_t *zn2,
                          c16_t *cmt,
                          c16_t *cmn1,
                          c16_t *cmn2,
                          c16_t *rho_tn1,
                          c16_t *rho_tn2,
                          c16_t *rho_n1n2,
                          int16_t *out,
                          uint32_t length,
                          int Qm);

// Float reference L-best kernel for 2-layer 16QAM (building block for >2-layer hybrid).
// L==16 == full ML. seed_lambda: 0 = ZF, else unbiased MMSE.
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
// (analysis vehicle; no SIMD 256QAM full-search kernel exists). seed_lambda: 0 = ZF, else unbiased MMSE.
void nr_qam256_llr_2layer_lbest(c16_t *stream0_in,
                                c16_t *stream1_in,
                                c16_t *ch_mag,
                                c16_t *ch_mag_i,
                                int16_t *stream0_out,
                                c16_t *rho01,
                                uint32_t length,
                                int L,
                                float seed_lambda);

// Fixed-point (Q15-input) L-best 2-layer 256QAM kernel (unbiased MMSE seed, lambda = 0: ZF). Integer twin of
// nr_qam256_llr_2layer_lbest; scale 16*sqrt(170), PAM-16 levels, 8 bits/RE.
// pattern: 0=5x5/25cand, 1=3x3/9cand, 2=5-plus/5cand, other=full256.
// Scalar reference for the SIMD kernel (and its short-symbol tail).
void nr_qam256_llr_2layer_lbest_q15(c16_t *stream0_in,
                                    c16_t *stream1_in,
                                    c16_t *ch_mag,
                                    c16_t *ch_mag_i,
                                    int16_t *stream0_out,
                                    c16_t *rho01,
                                    uint32_t length,
                                    int pattern,
                                    float lambda);

// Fixed-point (Q15-input) L-best 2-layer 64QAM kernel (unbiased MMSE seed, lambda = 0: ZF). Integer twin of
// nr_qam64_llr_2layer_lbest; the SIMD-portable form of the reduced search.
void nr_qam64_llr_2layer_lbest_q15(c16_t *stream0_in,
                                   c16_t *stream1_in,
                                   c16_t *ch_mag,
                                   c16_t *ch_mag_i,
                                   int16_t *stream0_out,
                                   c16_t *rho01,
                                   uint32_t length,
                                   int L,
                                   float lambda);

// int16/16-lane AVX2 L-best 2-layer 64QAM kernel (the optimized form). lambda: see nr_compute_ML_llr.
// pattern: 0 = 3x3 (9 cand, full BLER), 1 = 6-cand (plus + toward-seed diagonal), 2 = 5-plus.
void nr_qam64_llr_2layer_lbest_q15_simd16(c16_t *stream0_in,
                                          c16_t *stream1_in,
                                          c16_t *ch_mag,
                                          c16_t *ch_mag_i,
                                          int16_t *stream0_out,
                                          c16_t *rho01,
                                          uint32_t length,
                                          int pattern,
                                          float lambda);

// int16/16-lane AVX2 L-best 2-layer 256QAM kernel (PAM-16, 8 bits/RE).
// pattern: 0 = 5x5 (25 cand), 1 = 3x3 (9 cand, the lbest receiver), 2 = 5-plus (5 cand),
// 3 = full 16x16 grid (the ml receiver).
void nr_qam256_llr_2layer_lbest_q15_simd16(c16_t *stream0_in,
                                           c16_t *stream1_in,
                                           c16_t *ch_mag,
                                           c16_t *ch_mag_i,
                                           int16_t *stream0_out,
                                           c16_t *rho01,
                                           uint32_t length,
                                           int pattern,
                                           float lambda);

// fp32-metric variants of the 64QAM and 256QAM L-best kernels (pattern 1 = 3x3, 3 = full grid): same seed,
// candidates and fallback, metric and max-log in fp32 (tracks the float references; the int16 metrics' coarse
// scale flips small-|LLR| bits).
void nr_qam64_llr_2layer_lbest_f32_simd16(c16_t *stream0_in,
                                          c16_t *stream1_in,
                                          c16_t *ch_mag,
                                          c16_t *ch_mag_i,
                                          int16_t *stream0_out,
                                          c16_t *rho01,
                                          uint32_t length,
                                          int pattern,
                                          float lambda);
void nr_qam256_llr_2layer_lbest_f32_simd16(c16_t *stream0_in,
                                           c16_t *stream1_in,
                                           c16_t *ch_mag,
                                           c16_t *ch_mag_i,
                                           int16_t *stream0_out,
                                           c16_t *rho01,
                                           uint32_t length,
                                           int pattern,
                                           float lambda);

void nr_compute_ML_llr(c16_t *rxdataF_comp0,
                       c16_t *rxdataF_comp1,
                       c16_t *ch_mag0,
                       c16_t *ch_mag1,
                       int16_t *llr_layers0,
                       int16_t *llr_layers1,
                       c16_t *rho0,
                       c16_t *rho1,
                       uint32_t nb_re,
                       uint8_t mod_order,
                       int lbest, // 64/256QAM: 0 = full max-log ML, 1 = reduced-search L-best, 2 = L-best with the
                                  // fp32 256QAM metric (high resolution; = 1 for 64QAM)
                       float lambda); // L-best seed regularization: noise variance >> log2_maxh (0: ZF seed)

/// 2-layer linear MMSE receiver (L = 1 member of the L-best family): per-layer LLRs from the MRC outputs, the
/// 2x2 Gram (rho_ab = h_a^H h_b, real part of the diagonal used) and lambda = nr_2layer_seed_lambda().
void nr_compute_MMSE_llr(c16_t *comp0,
                         c16_t *comp1,
                         c16_t *rho00,
                         c16_t *rho01,
                         c16_t *rho10,
                         c16_t *rho11,
                         int16_t *llr_layer0,
                         int16_t *llr_layer1,
                         uint32_t nb_re,
                         uint8_t mod_order,
                         float lambda);
/// scalar float reference of the 2-layer MMSE LLR for one target layer (z0 / g00 target, z1 / g11 other)
void nr_llr_2layer_mmse_ref(const c16_t *z0,
                            const c16_t *z1,
                            const c16_t *g00,
                            const c16_t *g11,
                            const c16_t *rho01,
                            int16_t *out,
                            uint32_t length,
                            int mod_order,
                            float lambda);

#endif /* __NR_COMPUTE_LLR__H__ */
