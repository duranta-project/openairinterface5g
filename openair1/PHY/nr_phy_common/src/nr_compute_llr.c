/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_compute_llr.h"
#include "nr_phy_common.h"
#include "bits.h"
#include <complex.h>
#include <stdlib.h>
#include "PHY/sse_intrin.h"
#include "PHY/impl_defs_top.h"
#ifdef __aarch64__
#define USE_128BIT
#endif

void nr_compute_llr(c16_t *rxdataF_comp,
                    c16_t *ch_mag,
                    c16_t *ch_magb,
                    c16_t *ch_magc,
                    int16_t *llr,
                    uint32_t nb_re,
                    uint8_t symbol,
                    uint8_t mod_order)
{
  switch (mod_order) {
    case 2:
      nr_qpsk_llr(rxdataF_comp, llr, nb_re);
      break;
    case 4:
      nr_16qam_llr(rxdataF_comp, ch_mag, llr, nb_re);
      break;
    case 6:
      nr_64qam_llr(rxdataF_comp, ch_mag, ch_magb, llr, nb_re);
      break;
    case 8:
      nr_256qam_llr(rxdataF_comp, ch_mag, ch_magb, ch_magc, llr, nb_re);
      break;
    default:
      AssertFatal(false, "nr_compute_llr: invalid mod_order, symbol=%d, Qm=%d\n", symbol, mod_order);
      break;
  }
}

/*
 * This function computes the LLRs of stream 0 (s_0) in presence of the interfering stream 1 (s_1) assuming that both symbols are
 * QPSK. It can be used for both MU-MIMO interference-aware receiver or for SU-MIMO receivers.
 *
 * Input:
 *   stream0_in:  MF filter output for 1st stream, i.e., y0' = h0'*y0
 *   stream1_in:  MF filter output for 2nd stream, i.e., y1' = h1'*y0
 *   rho01:       Channel cross correlation, i.e., rho01 = h0'*h1
 *   length:      Number of resource elements
 *
 * Output:
 *   stream0_out: Output LLRs for 1st stream
 */
void nr_qpsk_llr_2layer(c16_t *stream0_in, c16_t *stream1_in, int16_t *stream0_out, c16_t *rho01, uint32_t length)
{
#ifdef USE_128BIT
  simde__m128i *rho01_128i = (simde__m128i *)rho01;
  simde__m128i *stream0_128i_in = (simde__m128i *)stream0_in;
  simde__m128i *stream1_128i_in = (simde__m128i *)stream1_in;
  simde__m128i *stream0_128i_out = (simde__m128i *)stream0_out;
  simde__m128i ONE_OVER_2_SQRT_2 = simde_mm_set1_epi16(23170); // round(2 ^ 16 / (2 * sqrt(2)))

  // In each iteration, we take 8 complex symbols
  for (int i = 0; i < length >> 2; i += 2) {
    /// Compute real and imaginary parts of MF output for stream 0 (desired stream)
    simde__m128i y0r, y0i;
    oai_mm_separate_real_imag_parts(&y0r, &y0i, stream0_128i_in[i], stream0_128i_in[i + 1]);
    simde__m128i y0r_over2 = simde_mm_mulhi_epi16(y0r, ONE_OVER_2_SQRT_2);
    y0r_over2 = simde_mm_slli_epi16(y0r_over2, 1); // y0r_over2 = Re(y0) / sqrt(2)
    simde__m128i y0i_over2 = simde_mm_mulhi_epi16(y0i, ONE_OVER_2_SQRT_2);
    y0i_over2 = simde_mm_slli_epi16(y0i_over2, 1); // y0i_over2 = Im(y0) / sqrt(2)

    /// Compute real and imaginary parts of MF output for stream 1 (interference stream)
    simde__m128i y1r_over2, y1i_over2;
    oai_mm_separate_real_imag_parts(&y1r_over2, &y1i_over2, stream1_128i_in[i], stream1_128i_in[i + 1]);
    y1r_over2 = simde_mm_srai_epi16(y1r_over2, 1); // y1r_over2 = Re(y1) / 2
    y1i_over2 = simde_mm_srai_epi16(y1i_over2, 1); // y1i_over2 = Im(y1) / 2

    /// Get real and imaginary parts of rho
    simde__m128i rhor, rhoi;
    oai_mm_separate_real_imag_parts(&rhor, &rhoi, rho01_128i[i], rho01_128i[i + 1]);

    /// Compute |psi_r| and |psi_i|

    // psi_r = rhor * xR + rhoi * xI
    // psi_i = rhor * xI - rhoi * xR

    // Put (rho_r + rho_i)/(2*sqrt(2)) in rho_p
    // rhor * xR + rhoi * xI  --> xR = 1/sqrt(2) and xI = 1/sqrt(2)
    // rhor * xI - rhoi * xR  --> xR = -1/sqrt(2) and xI = 1/sqrt(2)
    simde__m128i rho_p = simde_mm_adds_epi16(rhor, rhoi); // rho_p = Re(rho) + Im(rho)
    rho_p = simde_mm_mulhi_epi16(rho_p, ONE_OVER_2_SQRT_2); // rho_p = rho_p / (2*sqrt(2))

    // Put (rho_r - rho_i)/(2*sqrt(2)) in rho_m
    // rhor * xR + rhoi * xI  --> xR = 1/sqrt(2) and xI = -1/sqrt(2)
    // rhor * xI - rhoi * xR  --> xR = 1/sqrt(2) and xI = 1/sqrt(2)
    simde__m128i rho_m = simde_mm_subs_epi16(rhor, rhoi); // rho_m = Re(rho) - Im(rho)
    rho_m = simde_mm_mulhi_epi16(rho_m, ONE_OVER_2_SQRT_2); // rho_m = rho_m / (2*sqrt(2))

    // xR = 1/sqrt(2) and xI = 1/sqrt(2)
    simde__m128i abs_psi_rpm = simde_mm_subs_epi16(rho_p, y1r_over2); // psi_rpm = rho_p - y1r/2
    abs_psi_rpm = simde_mm_abs_epi16(abs_psi_rpm); // abs_psi_rpm = |psi_rpm|

    // xR = 1/sqrt(2) and xI = 1/sqrt(2)
    simde__m128i abs_psi_imm = simde_mm_subs_epi16(rho_m, y1i_over2); // psi_imm = rho_m - y1i/2
    abs_psi_imm = simde_mm_abs_epi16(abs_psi_imm); // abs_psi_imm = |psi_imm|

    // xR = 1/sqrt(2) and xI = -1/sqrt(2)
    simde__m128i abs_psi_rmm = simde_mm_subs_epi16(rho_m, y1r_over2); // psi_rmm = rho_m - y1r/2
    abs_psi_rmm = simde_mm_abs_epi16(abs_psi_rmm); // abs_psi_rmm = |psi_rmm|

    // xR = -1/sqrt(2) and xI = 1/sqrt(2)
    simde__m128i abs_psi_ipm = simde_mm_subs_epi16(rho_p, y1i_over2); // psi_ipm = rho_p - y1i/2
    abs_psi_ipm = simde_mm_abs_epi16(abs_psi_ipm); // abs_psi_ipm = |psi_ipm|

    // xR = -1/sqrt(2) and xI = -1/sqrt(2)
    simde__m128i abs_psi_rpp = simde_mm_adds_epi16(rho_p, y1r_over2); // psi_rpp = rho_p + y1r/2
    abs_psi_rpp = simde_mm_abs_epi16(abs_psi_rpp); // abs_psi_rpp = |psi_rpp|

    // xR = -1/sqrt(2) and xI = -1/sqrt(2)
    simde__m128i abs_psi_imp = simde_mm_adds_epi16(rho_m, y1i_over2); // psi_imp = rho_m + y1i/2
    abs_psi_imp = simde_mm_abs_epi16(abs_psi_imp); // abs_psi_imp = |psi_imp|

    // xR = -1/sqrt(2) and xI = 1/sqrt(2)
    simde__m128i abs_psi_rmp = simde_mm_adds_epi16(rho_m, y1r_over2); // psi_rmp = rho_m + y1r/2
    abs_psi_rmp = simde_mm_abs_epi16(abs_psi_rmp); // abs_psi_rmp = |psi_rmp|

    // xR = 1/sqrt(2) and xI = -1/sqrt(2)
    simde__m128i abs_psi_ipp = simde_mm_adds_epi16(rho_p, y1i_over2); // psi_ipm = rho_p + y1i/2
    abs_psi_ipp = simde_mm_abs_epi16(abs_psi_ipp); // abs_psi_ipp = |psi_ipm|

    /// Compute bit metrics (lambda)

    // lambda = max { |psi_r - y1r| * |x2R| + |psi_i - y1i| * |x2I| + y0r * xR + y0i * xI}

    // xR = 1/sqrt(2) and xI = 1/sqrt(2)
    // For numerator: bit_met_num_re_p = abs_psi_rpm + abs_psi_imm + y0r/sqrt(2) + y0i/sqrt(2)
    simde__m128i bit_met_num_re_p = simde_mm_adds_epi16(abs_psi_rpm, abs_psi_imm);
    bit_met_num_re_p = simde_mm_adds_epi16(bit_met_num_re_p, y0r_over2);
    bit_met_num_re_p = simde_mm_adds_epi16(bit_met_num_re_p, y0i_over2);

    // xR = 1/sqrt(2) and xI = -1/sqrt(2)
    // For numerator: bit_met_num_re_m = abs_psi_rmm + abs_psi_ipp + y0r/sqrt(2) - y0i/sqrt(2)
    simde__m128i bit_met_num_re_m = simde_mm_adds_epi16(abs_psi_rmm, abs_psi_ipp);
    bit_met_num_re_m = simde_mm_adds_epi16(bit_met_num_re_m, y0r_over2);
    bit_met_num_re_m = simde_mm_subs_epi16(bit_met_num_re_m, y0i_over2);

    // xR = -1/sqrt(2) and xI = 1/sqrt(2)
    // For denominator: bit_met_den_re_p = abs_psi_rmp + abs_psi_ipm - y0r/sqrt(2) + y0i/sqrt(2)
    simde__m128i bit_met_den_re_p = simde_mm_adds_epi16(abs_psi_rmp, abs_psi_ipm);
    bit_met_den_re_p = simde_mm_subs_epi16(bit_met_den_re_p, y0r_over2);
    bit_met_den_re_p = simde_mm_adds_epi16(bit_met_den_re_p, y0i_over2);

    // xR = -1/sqrt(2) and xI = -1/sqrt(2)
    // For denominator: bit_met_den_re_m = abs_psi_rpp + abs_psi_imp - y0r/sqrt(2) - y0i/sqrt(2)
    simde__m128i bit_met_den_re_m = simde_mm_adds_epi16(abs_psi_rpp, abs_psi_imp);
    bit_met_den_re_m = simde_mm_subs_epi16(bit_met_den_re_m, y0r_over2);
    bit_met_den_re_m = simde_mm_subs_epi16(bit_met_den_re_m, y0i_over2);

    // xR = 1/sqrt(2) and xI = 1/sqrt(2)
    // For numerator: bit_met_num_im_p = abs_psi_rpm + abs_psi_imm + y0r/sqrt(2) + y0i/sqrt(2)
    simde__m128i bit_met_num_im_p = simde_mm_adds_epi16(abs_psi_rpm, abs_psi_imm);
    bit_met_num_im_p = simde_mm_adds_epi16(bit_met_num_im_p, y0r_over2);
    bit_met_num_im_p = simde_mm_adds_epi16(bit_met_num_im_p, y0i_over2);

    // xR = -1/sqrt(2) and xI = 1/sqrt(2)
    // For numerator: bit_met_num_im_m = abs_psi_rmp + abs_psi_ipm - y0r/sqrt(2) + y0i/sqrt(2)
    simde__m128i bit_met_num_im_m = simde_mm_adds_epi16(abs_psi_rmp, abs_psi_ipm);
    bit_met_num_im_m = simde_mm_subs_epi16(bit_met_num_im_m, y0r_over2);
    bit_met_num_im_m = simde_mm_adds_epi16(bit_met_num_im_m, y0i_over2);

    // xR = 1/sqrt(2) and xI = -1/sqrt(2)
    // For denominator: bit_met_den_im_p = abs_psi_rmm + abs_psi_ipp + y0r/sqrt(2) - y0i/sqrt(2)
    simde__m128i bit_met_den_im_p = simde_mm_adds_epi16(abs_psi_rmm, abs_psi_ipp);
    bit_met_den_im_p = simde_mm_adds_epi16(bit_met_den_im_p, y0r_over2);
    bit_met_den_im_p = simde_mm_subs_epi16(bit_met_den_im_p, y0i_over2);

    // xR = -1/sqrt(2) and xI = -1/sqrt(2)
    // For denominator: bit_met_den_im_m = abs_psi_rpp + abs_psi_imp - y0r/sqrt(2)- y0i/sqrt(2)
    simde__m128i bit_met_den_im_m = simde_mm_adds_epi16(abs_psi_rpp, abs_psi_imp);
    bit_met_den_im_m = simde_mm_subs_epi16(bit_met_den_im_m, y0r_over2);
    bit_met_den_im_m = simde_mm_subs_epi16(bit_met_den_im_m, y0i_over2);

    /// Compute the LLRs

    // LLR = lambda(c==1) - lambda(c==0)

    simde__m128i logmax_num_re0 = simde_mm_max_epi16(bit_met_num_re_p, bit_met_num_re_m); // LLR of the first bit: Bit = 1
    simde__m128i logmax_den_re0 = simde_mm_max_epi16(bit_met_den_re_p, bit_met_den_re_m); // LLR of the first bit: Bit = 0
    simde__m128i logmax_num_im0 = simde_mm_max_epi16(bit_met_num_im_p, bit_met_num_im_m); // LLR of the second bit: Bit = 1
    simde__m128i logmax_den_im0 = simde_mm_max_epi16(bit_met_den_im_p, bit_met_den_im_m); // LLR of the second bit: Bit = 0

    y0r = simde_mm_subs_epi16(logmax_num_re0, logmax_den_re0); // LLR of first bit [L1(1), L1(2), L1(3), L1(4)]
    y0i = simde_mm_subs_epi16(logmax_num_im0, logmax_den_im0); // LLR of second bit [L2(1), L2(2), L2(3), L2(4)]

    // [L1(1), L2(1), L1(2), L2(2)]
    simde_mm_storeu_si128(&stream0_128i_out[i], simde_mm_unpacklo_epi16(y0r, y0i));

    // false if only 2 REs remain
    if (i < ((length >> 1) - 1)) {
      simde_mm_storeu_si128(&stream0_128i_out[i + 1], simde_mm_unpackhi_epi16(y0r, y0i));
    }
  }
#else

  simde__m256i *rho01_256i = (simde__m256i *)rho01;
  simde__m256i *stream0_256i_in = (simde__m256i *)stream0_in;
  simde__m256i *stream1_256i_in = (simde__m256i *)stream1_in;
  simde__m256i *stream0_256i_out = (simde__m256i *)stream0_out;
  simde__m256i ONE_OVER_2_SQRT_2 = simde_mm256_set1_epi16(23170); // round(2 ^ 16 / (2 * sqrt(2)))

  // In each iteration, we take 16 complex symbols
  for (int i = 0; i < length >> 3; i += 2) {
    /// Compute real and imaginary parts of MF output for stream 0 (desired stream)
    simde__m256i y0r, y0i;
    oai_mm256_separate_real_imag_parts(&y0r, &y0i, stream0_256i_in[i], stream0_256i_in[i + 1]);
    simde__m256i y0r_over2 = simde_mm256_mulhi_epi16(y0r, ONE_OVER_2_SQRT_2);
    y0r_over2 = simde_mm256_slli_epi16(y0r_over2, 1); // y0r_over2 = Re(y0) / sqrt(2)
    simde__m256i y0i_over2 = simde_mm256_mulhi_epi16(y0i, ONE_OVER_2_SQRT_2);
    y0i_over2 = simde_mm256_slli_epi16(y0i_over2, 1); // y0i_over2 = Im(y0) / sqrt(2)

    /// Compute real and imaginary parts of MF output for stream 1 (interference stream)
    simde__m256i y1r_over2, y1i_over2;
    oai_mm256_separate_real_imag_parts(&y1r_over2, &y1i_over2, stream1_256i_in[i], stream1_256i_in[i + 1]);
    y1r_over2 = simde_mm256_srai_epi16(y1r_over2, 1); // y1r_over2 = Re(y1) / 2
    y1i_over2 = simde_mm256_srai_epi16(y1i_over2, 1); // y1i_over2 = Im(y1) / 2

    /// Get real and imaginary parts of rho
    simde__m256i rhor, rhoi;
    oai_mm256_separate_real_imag_parts(&rhor, &rhoi, rho01_256i[i], rho01_256i[i + 1]);

    /// Compute |psi_r| and |psi_i|

    // psi_r = rhor * xR + rhoi * xI
    // psi_i = rhor * xI - rhoi * xR

    // Put (rho_r + rho_i)/(2*sqrt(2)) in rho_p
    // rhor * xR + rhoi * xI  --> xR = 1/sqrt(2) and xI = 1/sqrt(2)
    // rhor * xI - rhoi * xR  --> xR = -1/sqrt(2) and xI = 1/sqrt(2)
    simde__m256i rho_p = simde_mm256_adds_epi16(rhor, rhoi); // rho_p = Re(rho) + Im(rho)
    rho_p = simde_mm256_mulhi_epi16(rho_p, ONE_OVER_2_SQRT_2); // rho_p = rho_p / (2*sqrt(2))

    // Put (rho_r - rho_i)/(2*sqrt(2)) in rho_m
    // rhor * xR + rhoi * xI  --> xR = 1/sqrt(2) and xI = -1/sqrt(2)
    // rhor * xI - rhoi * xR  --> xR = 1/sqrt(2) and xI = 1/sqrt(2)
    simde__m256i rho_m = simde_mm256_subs_epi16(rhor, rhoi); // rho_m = Re(rho) - Im(rho)
    rho_m = simde_mm256_mulhi_epi16(rho_m, ONE_OVER_2_SQRT_2); // rho_m = rho_m / (2*sqrt(2))

    // xR = 1/sqrt(2) and xI = 1/sqrt(2)
    simde__m256i abs_psi_rpm = simde_mm256_subs_epi16(rho_p, y1r_over2); // psi_rpm = rho_p - y1r/2
    abs_psi_rpm = simde_mm256_abs_epi16(abs_psi_rpm); // abs_psi_rpm = |psi_rpm|

    // xR = 1/sqrt(2) and xI = 1/sqrt(2)
    simde__m256i abs_psi_imm = simde_mm256_subs_epi16(rho_m, y1i_over2); // psi_imm = rho_m - y1i/2
    abs_psi_imm = simde_mm256_abs_epi16(abs_psi_imm); // abs_psi_imm = |psi_imm|

    // xR = 1/sqrt(2) and xI = -1/sqrt(2)
    simde__m256i abs_psi_rmm = simde_mm256_subs_epi16(rho_m, y1r_over2); // psi_rmm = rho_m - y1r/2
    abs_psi_rmm = simde_mm256_abs_epi16(abs_psi_rmm); // abs_psi_rmm = |psi_rmm|

    // xR = -1/sqrt(2) and xI = 1/sqrt(2)
    simde__m256i abs_psi_ipm = simde_mm256_subs_epi16(rho_p, y1i_over2); // psi_ipm = rho_p - y1i/2
    abs_psi_ipm = simde_mm256_abs_epi16(abs_psi_ipm); // abs_psi_ipm = |psi_ipm|

    // xR = -1/sqrt(2) and xI = -1/sqrt(2)
    simde__m256i abs_psi_rpp = simde_mm256_adds_epi16(rho_p, y1r_over2); // psi_rpp = rho_p + y1r/2
    abs_psi_rpp = simde_mm256_abs_epi16(abs_psi_rpp); // abs_psi_rpp = |psi_rpp|

    // xR = -1/sqrt(2) and xI = -1/sqrt(2)
    simde__m256i abs_psi_imp = simde_mm256_adds_epi16(rho_m, y1i_over2); // psi_imp = rho_m + y1i/2
    abs_psi_imp = simde_mm256_abs_epi16(abs_psi_imp); // abs_psi_imp = |psi_imp|

    // xR = -1/sqrt(2) and xI = 1/sqrt(2)
    simde__m256i abs_psi_rmp = simde_mm256_adds_epi16(rho_m, y1r_over2); // psi_rmp = rho_m + y1r/2
    abs_psi_rmp = simde_mm256_abs_epi16(abs_psi_rmp); // abs_psi_rmp = |psi_rmp|

    // xR = 1/sqrt(2) and xI = -1/sqrt(2)
    simde__m256i abs_psi_ipp = simde_mm256_adds_epi16(rho_p, y1i_over2); // psi_ipm = rho_p + y1i/2
    abs_psi_ipp = simde_mm256_abs_epi16(abs_psi_ipp); // abs_psi_ipp = |psi_ipm|

    /// Compute bit metrics (lambda)

    // lambda = max { |psi_r - y1r| * |x2R| + |psi_i - y1i| * |x2I| + y0r * xR + y0i * xI}

    // xR = 1/sqrt(2) and xI = 1/sqrt(2)
    // For numerator: bit_met_num_re_p = abs_psi_rpm + abs_psi_imm + y0r/sqrt(2) + y0i/sqrt(2)
    simde__m256i bit_met_num_re_p = simde_mm256_adds_epi16(abs_psi_rpm, abs_psi_imm);
    bit_met_num_re_p = simde_mm256_adds_epi16(bit_met_num_re_p, y0r_over2);
    bit_met_num_re_p = simde_mm256_adds_epi16(bit_met_num_re_p, y0i_over2);

    // xR = 1/sqrt(2) and xI = -1/sqrt(2)
    // For numerator: bit_met_num_re_m = abs_psi_rmm + abs_psi_ipp + y0r/sqrt(2) - y0i/sqrt(2)
    simde__m256i bit_met_num_re_m = simde_mm256_adds_epi16(abs_psi_rmm, abs_psi_ipp);
    bit_met_num_re_m = simde_mm256_adds_epi16(bit_met_num_re_m, y0r_over2);
    bit_met_num_re_m = simde_mm256_subs_epi16(bit_met_num_re_m, y0i_over2);

    // xR = -1/sqrt(2) and xI = 1/sqrt(2)
    // For denominator: bit_met_den_re_p = abs_psi_rmp + abs_psi_ipm - y0r/sqrt(2) + y0i/sqrt(2)
    simde__m256i bit_met_den_re_p = simde_mm256_adds_epi16(abs_psi_rmp, abs_psi_ipm);
    bit_met_den_re_p = simde_mm256_subs_epi16(bit_met_den_re_p, y0r_over2);
    bit_met_den_re_p = simde_mm256_adds_epi16(bit_met_den_re_p, y0i_over2);

    // xR = -1/sqrt(2) and xI = -1/sqrt(2)
    // For denominator: bit_met_den_re_m = abs_psi_rpp + abs_psi_imp - y0r/sqrt(2) - y0i/sqrt(2)
    simde__m256i bit_met_den_re_m = simde_mm256_adds_epi16(abs_psi_rpp, abs_psi_imp);
    bit_met_den_re_m = simde_mm256_subs_epi16(bit_met_den_re_m, y0r_over2);
    bit_met_den_re_m = simde_mm256_subs_epi16(bit_met_den_re_m, y0i_over2);

    // xR = 1/sqrt(2) and xI = 1/sqrt(2)
    // For numerator: bit_met_num_im_p = abs_psi_rpm + abs_psi_imm + y0r/sqrt(2) + y0i/sqrt(2)
    simde__m256i bit_met_num_im_p = simde_mm256_adds_epi16(abs_psi_rpm, abs_psi_imm);
    bit_met_num_im_p = simde_mm256_adds_epi16(bit_met_num_im_p, y0r_over2);
    bit_met_num_im_p = simde_mm256_adds_epi16(bit_met_num_im_p, y0i_over2);

    // xR = -1/sqrt(2) and xI = 1/sqrt(2)
    // For numerator: bit_met_num_im_m = abs_psi_rmp + abs_psi_ipm - y0r/sqrt(2) + y0i/sqrt(2)
    simde__m256i bit_met_num_im_m = simde_mm256_adds_epi16(abs_psi_rmp, abs_psi_ipm);
    bit_met_num_im_m = simde_mm256_subs_epi16(bit_met_num_im_m, y0r_over2);
    bit_met_num_im_m = simde_mm256_adds_epi16(bit_met_num_im_m, y0i_over2);

    // xR = 1/sqrt(2) and xI = -1/sqrt(2)
    // For denominator: bit_met_den_im_p = abs_psi_rmm + abs_psi_ipp + y0r/sqrt(2) - y0i/sqrt(2)
    simde__m256i bit_met_den_im_p = simde_mm256_adds_epi16(abs_psi_rmm, abs_psi_ipp);
    bit_met_den_im_p = simde_mm256_adds_epi16(bit_met_den_im_p, y0r_over2);
    bit_met_den_im_p = simde_mm256_subs_epi16(bit_met_den_im_p, y0i_over2);

    // xR = -1/sqrt(2) and xI = -1/sqrt(2)
    // For denominator: bit_met_den_im_m = abs_psi_rpp + abs_psi_imp - y0r/sqrt(2)- y0i/sqrt(2)
    simde__m256i bit_met_den_im_m = simde_mm256_adds_epi16(abs_psi_rpp, abs_psi_imp);
    bit_met_den_im_m = simde_mm256_subs_epi16(bit_met_den_im_m, y0r_over2);
    bit_met_den_im_m = simde_mm256_subs_epi16(bit_met_den_im_m, y0i_over2);

    /// Compute the LLRs

    // LLR = lambda(c==1) - lambda(c==0)

    simde__m256i logmax_num_re0 = simde_mm256_max_epi16(bit_met_num_re_p, bit_met_num_re_m); // LLR of the first bit: Bit = 1
    simde__m256i logmax_den_re0 = simde_mm256_max_epi16(bit_met_den_re_p, bit_met_den_re_m); // LLR of the first bit: Bit = 0
    simde__m256i logmax_num_im0 = simde_mm256_max_epi16(bit_met_num_im_p, bit_met_num_im_m); // LLR of the second bit: Bit = 1
    simde__m256i logmax_den_im0 = simde_mm256_max_epi16(bit_met_den_im_p, bit_met_den_im_m); // LLR of the second bit: Bit = 0

    y0r = simde_mm256_subs_epi16(logmax_num_re0,
                                 logmax_den_re0); // LLR of first bit [L1(1), L1(2), L1(3), L1(4), L1(5), L1(6), L1(7), L1(8)]
    y0i = simde_mm256_subs_epi16(logmax_num_im0,
                                 logmax_den_im0); // LLR of second bit [L2(1), L2(2), L2(3), L2(4), L2(5), L2(6), L2(7), L2(8)]

    // [L1(1), L2(1), L1(2), L2(2) ...]
    simde__m128i *stream0_128i_out = (simde__m128i *)&stream0_256i_out[i];
    simde__m128i *y0r_128 = (simde__m128i *)&y0r;
    simde__m128i *y0i_128 = (simde__m128i *)&y0i;
    simde_mm_storeu_si128(&stream0_128i_out[0], simde_mm_unpacklo_epi16(y0r_128[0], y0i_128[0]));
    simde_mm_storeu_si128(&stream0_128i_out[1], simde_mm_unpackhi_epi16(y0r_128[0], y0i_128[0]));

    // false if only 4 REs remain
    if (i < ((length >> 2) - 1)) {
      simde__m128i *stream0_128i_out = (simde__m128i *)&stream0_256i_out[i + 1];
      simde_mm_storeu_si128(&stream0_128i_out[0], simde_mm_unpacklo_epi16(y0r_128[1], y0i_128[1]));
      simde_mm_storeu_si128(&stream0_128i_out[1], simde_mm_unpackhi_epi16(y0r_128[1], y0i_128[1]));
    }
  }
#endif
}

#ifdef USE_128BIT
// calculate interference magnitude
// tmp_result = ones in shorts corr. to interval 2<=x<=4, tmp_result2 interval < 2, tmp_result3 interval 4<x<6 and tmp_result4
// interval x>6
static inline simde__m128i interference_abs_64qam_epi16(simde__m128i psi,
                                                        simde__m128i int_ch_mag,
                                                        simde__m128i int_two_ch_mag,
                                                        simde__m128i int_three_ch_mag,
                                                        simde__m128i c1,
                                                        simde__m128i c3,
                                                        simde__m128i c5,
                                                        simde__m128i c7)
{
  simde__m128i tmp_result = simde_mm_cmpgt_epi16(int_two_ch_mag, psi);
  simde__m128i tmp_result3 = simde_mm_xor_si128(tmp_result, allones128());
  simde__m128i tmp_result2 = simde_mm_cmpgt_epi16(int_ch_mag, psi);
  tmp_result = simde_mm_xor_si128(tmp_result, tmp_result2);
  simde__m128i tmp_result4 = simde_mm_cmpgt_epi16(psi, int_three_ch_mag);
  tmp_result3 = simde_mm_xor_si128(tmp_result3, tmp_result4);
  tmp_result = simde_mm_and_si128(tmp_result, c3);
  tmp_result2 = simde_mm_and_si128(tmp_result2, c1);
  tmp_result3 = simde_mm_and_si128(tmp_result3, c5);
  tmp_result4 = simde_mm_and_si128(tmp_result4, c7);
  tmp_result = simde_mm_or_si128(tmp_result, tmp_result2);
  tmp_result3 = simde_mm_or_si128(tmp_result3, tmp_result4);
  return simde_mm_or_si128(tmp_result, tmp_result3);
}

// Calculates psi_a = psi_r * a_r + psi_i * a_i
static inline simde__m128i prodsum_psi_a_epi16(simde__m128i psi_r, simde__m128i a_r, simde__m128i psi_i, simde__m128i a_i)
{
  simde__m128i tmp_result = simde_mm_mulhi_epi16(psi_r, a_r);
  tmp_result = simde_mm_slli_epi16(tmp_result, 1);
  simde__m128i tmp_result2 = simde_mm_mulhi_epi16(psi_i, a_i);
  tmp_result2 = simde_mm_slli_epi16(tmp_result2, 1);
  return simde_mm_adds_epi16(tmp_result, tmp_result2);
}

// Calculate interference magnitude
static inline simde__m128i interference_abs_epi16(simde__m128i psi, simde__m128i int_ch_mag, simde__m128i c1, simde__m128i c2)
{
  simde__m128i tmp_result = simde_mm_cmplt_epi16(psi, int_ch_mag);
  simde__m128i tmp_result2 = simde_mm_xor_si128(tmp_result, allones128());
  tmp_result = simde_mm_and_si128(tmp_result, c1);
  tmp_result2 = simde_mm_and_si128(tmp_result2, c2);
  return simde_mm_or_si128(tmp_result, tmp_result2);
}

// Calculates a_sq = int_ch_mag * (a_r^2 + a_i^2) * scale_factor
static inline simde__m128i square_a_epi16(simde__m128i a_r, simde__m128i a_i, simde__m128i int_ch_mag, simde__m128i scale_factor)
{
  simde__m128i tmp_result = simde_mm_mulhi_epi16(a_r, a_r);
  tmp_result = simde_mm_slli_epi16(tmp_result, 1);
  tmp_result = simde_mm_mulhi_epi16(tmp_result, scale_factor);
  tmp_result = simde_mm_slli_epi16(tmp_result, 1);
  tmp_result = simde_mm_mulhi_epi16(tmp_result, int_ch_mag);
  tmp_result = simde_mm_slli_epi16(tmp_result, 1);
  simde__m128i tmp_result2 = simde_mm_mulhi_epi16(a_i, a_i);
  tmp_result2 = simde_mm_slli_epi16(tmp_result2, 1);
  tmp_result2 = simde_mm_mulhi_epi16(tmp_result2, scale_factor);
  tmp_result2 = simde_mm_slli_epi16(tmp_result2, 1);
  tmp_result2 = simde_mm_mulhi_epi16(tmp_result2, int_ch_mag);
  tmp_result2 = simde_mm_slli_epi16(tmp_result2, 1);
  return simde_mm_adds_epi16(tmp_result, tmp_result2);
}

// calculates a_sq = int_ch_mag*(a_r^2 + a_i^2)*scale_factor for 64-QAM
static inline simde__m128i square_a_64qam_epi16(simde__m128i a_r,
                                                simde__m128i a_i,
                                                simde__m128i int_ch_mag,
                                                simde__m128i scale_factor)
{
  simde__m128i tmp_result = simde_mm_mulhi_epi16(a_r, a_r);
  tmp_result = simde_mm_slli_epi16(tmp_result, 1);
  tmp_result = simde_mm_mulhi_epi16(tmp_result, scale_factor);
  tmp_result = simde_mm_slli_epi16(tmp_result, 3);
  tmp_result = simde_mm_mulhi_epi16(tmp_result, int_ch_mag);
  tmp_result = simde_mm_slli_epi16(tmp_result, 1);
  simde__m128i tmp_result2 = simde_mm_mulhi_epi16(a_i, a_i);
  tmp_result2 = simde_mm_slli_epi16(tmp_result2, 1);
  tmp_result2 = simde_mm_mulhi_epi16(tmp_result2, scale_factor);
  tmp_result2 = simde_mm_slli_epi16(tmp_result2, 3);
  tmp_result2 = simde_mm_mulhi_epi16(tmp_result2, int_ch_mag);
  tmp_result2 = simde_mm_slli_epi16(tmp_result2, 1);
  return simde_mm_adds_epi16(tmp_result, tmp_result2);
}

static inline simde__m128i max_epi16(simde__m128i m0,
                                     simde__m128i m1,
                                     simde__m128i m2,
                                     simde__m128i m3,
                                     simde__m128i m4,
                                     simde__m128i m5,
                                     simde__m128i m6,
                                     simde__m128i m7)
{
  simde__m128i a0 = simde_mm_max_epi16(m0, m1);
  simde__m128i a1 = simde_mm_max_epi16(m2, m3);
  simde__m128i a2 = simde_mm_max_epi16(m4, m5);
  simde__m128i a3 = simde_mm_max_epi16(m6, m7);
  simde__m128i b0 = simde_mm_max_epi16(a0, a1);
  simde__m128i b1 = simde_mm_max_epi16(a2, a3);
  return simde_mm_max_epi16(b0, b1);
}

#else

// calculate interference magnitude
// tmp_result = ones in shorts corr. to interval 2<=x<=4, tmp_result2 interval < 2, tmp_result3 interval 4<x<6 and tmp_result4
// interval x>6
static inline simde__m256i interference_abs_64qam_epi16_256(simde__m256i psi,
                                                            simde__m256i int_ch_mag,
                                                            simde__m256i int_two_ch_mag,
                                                            simde__m256i int_three_ch_mag,
                                                            simde__m256i c1,
                                                            simde__m256i c3,
                                                            simde__m256i c5,
                                                            simde__m256i c7)
{
  simde__m256i tmp_result = simde_mm256_cmpgt_epi16(int_two_ch_mag, psi);
  simde__m256i tmp_result3 = simde_mm256_xor_si256(tmp_result, allones256());
  simde__m256i tmp_result2 = simde_mm256_cmpgt_epi16(int_ch_mag, psi);
  tmp_result = simde_mm256_xor_si256(tmp_result, tmp_result2);
  simde__m256i tmp_result4 = simde_mm256_cmpgt_epi16(psi, int_three_ch_mag);
  tmp_result3 = simde_mm256_xor_si256(tmp_result3, tmp_result4);
  tmp_result = simde_mm256_and_si256(tmp_result, c3);
  tmp_result2 = simde_mm256_and_si256(tmp_result2, c1);
  tmp_result3 = simde_mm256_and_si256(tmp_result3, c5);
  tmp_result4 = simde_mm256_and_si256(tmp_result4, c7);
  tmp_result = simde_mm256_or_si256(tmp_result, tmp_result2);
  tmp_result3 = simde_mm256_or_si256(tmp_result3, tmp_result4);
  return simde_mm256_or_si256(tmp_result, tmp_result3);
}

// calculates psi_a = psi_r*a_r + psi_i*a_i
static inline simde__m256i prodsum_psi_a_epi16_256(simde__m256i psi_r, simde__m256i a_r, simde__m256i psi_i, simde__m256i a_i)
{
  simde__m256i tmp_result = simde_mm256_mulhi_epi16(psi_r, a_r);
  tmp_result = simde_mm256_slli_epi16(tmp_result, 1);
  simde__m256i tmp_result2 = simde_mm256_mulhi_epi16(psi_i, a_i);
  tmp_result2 = simde_mm256_slli_epi16(tmp_result2, 1);
  return simde_mm256_adds_epi16(tmp_result, tmp_result2);
}

// Calculate interference magnitude
static inline simde__m256i interference_abs_epi16_256(simde__m256i psi, simde__m256i int_ch_mag, simde__m256i c1, simde__m256i c2)
{
  simde__m256i tmp_result = simde_mm256_cmpgt_epi16(int_ch_mag, psi);
  simde__m256i tmp_result2 = simde_mm256_xor_si256(tmp_result, allones256());
  tmp_result = simde_mm256_and_si256(tmp_result, c1);
  tmp_result2 = simde_mm256_and_si256(tmp_result2, c2);
  return simde_mm256_or_si256(tmp_result, tmp_result2);
}

// Calculates a_sq = int_ch_mag * (a_r^2 + a_i^2) * scale_factor
static inline simde__m256i square_a_epi16_256(simde__m256i a_r,
                                              simde__m256i a_i,
                                              simde__m256i int_ch_mag,
                                              simde__m256i scale_factor)
{
  simde__m256i tmp_result = simde_mm256_mulhi_epi16(a_r, a_r);
  tmp_result = simde_mm256_slli_epi16(tmp_result, 1);
  tmp_result = simde_mm256_mulhi_epi16(tmp_result, scale_factor);
  tmp_result = simde_mm256_slli_epi16(tmp_result, 1);
  tmp_result = simde_mm256_mulhi_epi16(tmp_result, int_ch_mag);
  tmp_result = simde_mm256_slli_epi16(tmp_result, 1);
  simde__m256i tmp_result2 = simde_mm256_mulhi_epi16(a_i, a_i);
  tmp_result2 = simde_mm256_slli_epi16(tmp_result2, 1);
  tmp_result2 = simde_mm256_mulhi_epi16(tmp_result2, scale_factor);
  tmp_result2 = simde_mm256_slli_epi16(tmp_result2, 1);
  tmp_result2 = simde_mm256_mulhi_epi16(tmp_result2, int_ch_mag);
  tmp_result2 = simde_mm256_slli_epi16(tmp_result2, 1);
  return simde_mm256_adds_epi16(tmp_result, tmp_result2);
}

// calculates a_sq = int_ch_mag*(a_r^2 + a_i^2)*scale_factor for 64-QAM
static inline simde__m256i square_a_64qam_epi16_256(simde__m256i a_r,
                                                    simde__m256i a_i,
                                                    simde__m256i int_ch_mag,
                                                    simde__m256i scale_factor)
{
  simde__m256i tmp_result = simde_mm256_mulhi_epi16(a_r, a_r);
  tmp_result = simde_mm256_slli_epi16(tmp_result, 1);
  tmp_result = simde_mm256_mulhi_epi16(tmp_result, scale_factor);
  tmp_result = simde_mm256_slli_epi16(tmp_result, 3);
  tmp_result = simde_mm256_mulhi_epi16(tmp_result, int_ch_mag);
  tmp_result = simde_mm256_slli_epi16(tmp_result, 1);
  simde__m256i tmp_result2 = simde_mm256_mulhi_epi16(a_i, a_i);
  tmp_result2 = simde_mm256_slli_epi16(tmp_result2, 1);
  tmp_result2 = simde_mm256_mulhi_epi16(tmp_result2, scale_factor);
  tmp_result2 = simde_mm256_slli_epi16(tmp_result2, 3);
  tmp_result2 = simde_mm256_mulhi_epi16(tmp_result2, int_ch_mag);
  tmp_result2 = simde_mm256_slli_epi16(tmp_result2, 1);
  return simde_mm256_adds_epi16(tmp_result, tmp_result2);
}

static inline simde__m256i max_epi16_256(simde__m256i m0,
                                         simde__m256i m1,
                                         simde__m256i m2,
                                         simde__m256i m3,
                                         simde__m256i m4,
                                         simde__m256i m5,
                                         simde__m256i m6,
                                         simde__m256i m7)
{
  simde__m256i a0 = simde_mm256_max_epi16(m0, m1);
  simde__m256i a1 = simde_mm256_max_epi16(m2, m3);
  simde__m256i a2 = simde_mm256_max_epi16(m4, m5);
  simde__m256i a3 = simde_mm256_max_epi16(m6, m7);
  simde__m256i b0 = simde_mm256_max_epi16(a0, a1);
  simde__m256i b1 = simde_mm256_max_epi16(a2, a3);
  return simde_mm256_max_epi16(b0, b1);
}

#endif

/*
 * This function computes the LLRs of stream 0 (s_0) in presence of the interfering stream 1 (s_1) assuming that both symbols are
 * 16QAM. It can be used for both MU-MIMO interference-aware receiver or for SU-MIMO receivers.
 *
 * Input:
 *   stream0_in:  MF filter output for 1st stream, i.e., y0' = h0'*y0
 *   stream1_in:  MF filter output for 2nd stream, i.e., y1' = h1'*y0
 *   ch_mag:      2*h0/sqrt(10), [Re0 Im0 Re1 Im1] s.t. Im0=Re0, Im1=Re1, etc
 *   ch_mag_i:    2*h1/sqrt(10), [Re0 Im0 Re1 Im1] s.t. Im0=Re0, Im1=Re1, etc
 *   rho01:       Channel cross correlation, i.e., rho01 = h0'*h1
 *   length:      Number of resource elements
 *
 * Output:
 *   stream0_out: Output LLRs for 1st stream
 */
void nr_qam16_llr_2layer(c16_t *stream0_in,
                         c16_t *stream1_in,
                         c16_t *ch_mag,
                         c16_t *ch_mag_i,
                         int16_t *stream0_out,
                         c16_t *rho01,
                         uint32_t length)
{
#ifdef USE_128BIT
  simde__m128i *rho01_128i = (simde__m128i *)rho01;
  simde__m128i *stream0_128i_in = (simde__m128i *)stream0_in;
  simde__m128i *stream1_128i_in = (simde__m128i *)stream1_in;
  simde__m128i *stream0_128i_out = (simde__m128i *)stream0_out;
  simde__m128i *ch_mag_128i = (simde__m128i *)ch_mag;
  simde__m128i *ch_mag_128i_i = (simde__m128i *)ch_mag_i;

  simde__m128i ONE_OVER_SQRT_10 = simde_mm_set1_epi16(20724); // round(1/sqrt(10)*2^16)
  simde__m128i ONE_OVER_SQRT_10_Q15 = simde_mm_set1_epi16(10362); // round(1/sqrt(10)*2^15)
  simde__m128i THREE_OVER_SQRT_10 = simde_mm_set1_epi16(31086); // round(3/sqrt(10)*2^15)
  simde__m128i SQRT_10_OVER_FOUR = simde_mm_set1_epi16(25905); // round(sqrt(10)/4*2^15)
  simde__m128i ONE_OVER_TWO_SQRT_10 = simde_mm_set1_epi16(10362); // round(1/2/sqrt(10)*2^16)
  simde__m128i NINE_OVER_TWO_SQRT_10 = simde_mm_set1_epi16(23315); // round(9/2/sqrt(10)*2^14)
  simde__m128i ch_mag_des, ch_mag_int;
  simde__m128i y0r_over_sqrt10;
  simde__m128i y0i_over_sqrt10;
  simde__m128i y0r_three_over_sqrt10;
  simde__m128i y0i_three_over_sqrt10;
  simde__m128i ch_mag_over_10;
  simde__m128i ch_mag_over_2;
  simde__m128i ch_mag_9_over_10;

  simde__m128i xmm0;
  simde__m128i xmm1;
  simde__m128i xmm2;
  simde__m128i xmm3;
  simde__m128i xmm4;
  simde__m128i xmm5;
  simde__m128i xmm6;
  simde__m128i xmm7;

  simde__m128i rho_rpi;
  simde__m128i rho_rmi;
  simde__m128i rho_rs[8];
  simde__m128i psi_rs[16];
  simde__m128i psi_is[16];
  simde__m128i a_rs[16];
  simde__m128i a_is[16];
  simde__m128i psi_as[16];
  simde__m128i a_sqs[16];
  simde__m128i y0_s[8];

  simde__m128i y0r;
  simde__m128i y0i;
  simde__m128i y1r;
  simde__m128i y1i;

  // In one iteration, we deal with 8 REs
  for (int i = 0; i < length >> 2; i += 2) {
    // Get rho
    oai_mm_separate_real_imag_parts(&xmm2, &xmm3, rho01_128i[i], rho01_128i[i + 1]);
    rho_rpi = simde_mm_adds_epi16(xmm2, xmm3); // rho = Re(rho) + Im(rho)
    rho_rmi = simde_mm_subs_epi16(xmm2, xmm3); // rho* = Re(rho) - Im(rho)

    // Compute the different rhos
    rho_rs[0] = simde_mm_mulhi_epi16(rho_rpi, ONE_OVER_SQRT_10);
    rho_rs[4] = simde_mm_mulhi_epi16(rho_rmi, ONE_OVER_SQRT_10);
    rho_rs[3] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(rho_rpi, THREE_OVER_SQRT_10), 1);
    rho_rs[7] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(rho_rmi, THREE_OVER_SQRT_10), 1);

    xmm4 = simde_mm_mulhi_epi16(xmm2, ONE_OVER_SQRT_10); // Re(rho)
    xmm5 = simde_mm_mulhi_epi16(xmm3, THREE_OVER_SQRT_10); // Im(rho)
    xmm5 = simde_mm_slli_epi16(xmm5, 1);

    rho_rs[1] = simde_mm_adds_epi16(xmm4, xmm5);
    rho_rs[5] = simde_mm_subs_epi16(xmm4, xmm5);

    xmm6 = simde_mm_mulhi_epi16(xmm2, THREE_OVER_SQRT_10); // Re(rho)
    xmm7 = simde_mm_mulhi_epi16(xmm3, ONE_OVER_SQRT_10); // Im(rho)
    xmm6 = simde_mm_slli_epi16(xmm6, 1);

    rho_rs[2] = simde_mm_adds_epi16(xmm6, xmm7);
    rho_rs[6] = simde_mm_subs_epi16(xmm6, xmm7);

    // Rearrange interfering MF output
    oai_mm_separate_real_imag_parts(&y1r, &y1i, stream1_128i_in[i], stream1_128i_in[i + 1]);

    // |  [Re(rho)+ Im(rho)]/sqrt(10) - y1r  |
    for (int j = 0; j < 8; j++) { // psi_rs[0~7], rho_rs[0~7]
      psi_rs[j] = simde_mm_abs_epi16(simde_mm_subs_epi16(rho_rs[j], y1r));
    }
    for (int j = 8; j < 16; j++) { // psi_rs[8~16], rho_rs[4,5,6,7,0,1,2,3]
      psi_rs[j] = simde_mm_abs_epi16(simde_mm_adds_epi16(rho_rs[(j - 4) & 7], y1r));
    }
    const uint8_t rho_rs_indexes[16] = {4, 6, 5, 7, 0, 2, 1, 3, 0, 2, 1, 3, 4, 6, 5, 7};
    for (int k = 0; k < 16;
         k += 8) { // psi_is[0~15], sub(rho_rs[4,6,5,7]), add(rho_rs[0,2,1,3]), sub(rho_rs[0,2,1,3]), add(rho_rs[4,6,5,7])
      for (int j = k; j < k + 4; j++) {
        psi_is[j] = simde_mm_abs_epi16(simde_mm_subs_epi16(rho_rs[rho_rs_indexes[j]], y1i));
        psi_is[j + 4] = simde_mm_abs_epi16(simde_mm_adds_epi16(rho_rs[rho_rs_indexes[j + 4]], y1i));
      }
    }

    // Rearrange desired MF output
    oai_mm_separate_real_imag_parts(&y0r, &y0i, stream0_128i_in[i], stream0_128i_in[i + 1]);

    // Rearrange desired channel magnitudes
    // [|h|^2(1),|h|^2(2),|h|^2(3),|h|^2(4)]*(2/sqrt(10))
    oai_mm_separate_real_imag_parts(&ch_mag_des, &xmm2, ch_mag_128i[i], ch_mag_128i[i + 1]);

    // Rearrange interfering channel magnitudes
    oai_mm_separate_real_imag_parts(&ch_mag_int, &xmm2, ch_mag_128i_i[i], ch_mag_128i_i[i + 1]);

    // Scale MF output of desired signal
    y0r_over_sqrt10 = simde_mm_mulhi_epi16(y0r, ONE_OVER_SQRT_10);
    y0i_over_sqrt10 = simde_mm_mulhi_epi16(y0i, ONE_OVER_SQRT_10);
    y0r_three_over_sqrt10 = simde_mm_slli_epi16(simde_mm_mulhi_epi16(y0r, THREE_OVER_SQRT_10), 1);
    y0i_three_over_sqrt10 = simde_mm_slli_epi16(simde_mm_mulhi_epi16(y0i, THREE_OVER_SQRT_10), 1);

    // Compute necessary combination of required terms
    y0_s[0] = simde_mm_adds_epi16(y0r_over_sqrt10, y0i_over_sqrt10);
    y0_s[4] = simde_mm_subs_epi16(y0r_over_sqrt10, y0i_over_sqrt10);

    y0_s[1] = simde_mm_adds_epi16(y0r_over_sqrt10, y0i_three_over_sqrt10);
    y0_s[5] = simde_mm_subs_epi16(y0r_over_sqrt10, y0i_three_over_sqrt10);

    y0_s[2] = simde_mm_adds_epi16(y0r_three_over_sqrt10, y0i_over_sqrt10);
    y0_s[6] = simde_mm_subs_epi16(y0r_three_over_sqrt10, y0i_over_sqrt10);

    y0_s[3] = simde_mm_adds_epi16(y0r_three_over_sqrt10, y0i_three_over_sqrt10);
    y0_s[7] = simde_mm_subs_epi16(y0r_three_over_sqrt10, y0i_three_over_sqrt10);

    for (int j = 0; j < 16; j++) {
      // Compute optimal interfering symbol magnitude
      a_rs[j] = interference_abs_epi16(psi_rs[j], ch_mag_int, ONE_OVER_SQRT_10_Q15, THREE_OVER_SQRT_10);
      a_is[j] = interference_abs_epi16(psi_is[j], ch_mag_int, ONE_OVER_SQRT_10_Q15, THREE_OVER_SQRT_10);

      // Calculation of groups of two terms in the bit metric involving product of psi and interference magnitude
      psi_as[j] = prodsum_psi_a_epi16(psi_rs[j], a_rs[j], psi_is[j], a_is[j]);

      // squared interference magnitude times int. ch. power
      a_sqs[j] = square_a_epi16(a_rs[j], a_is[j], ch_mag_int, SQRT_10_OVER_FOUR);
    }

    // Computing different multiples of channel norms
    ch_mag_over_10 = simde_mm_mulhi_epi16(ch_mag_des, ONE_OVER_TWO_SQRT_10);
    ch_mag_over_2 = simde_mm_mulhi_epi16(ch_mag_des, SQRT_10_OVER_FOUR);
    ch_mag_over_2 = simde_mm_slli_epi16(ch_mag_over_2, 1);
    ch_mag_9_over_10 = simde_mm_mulhi_epi16(ch_mag_des, NINE_OVER_TWO_SQRT_10);
    ch_mag_9_over_10 = simde_mm_slli_epi16(ch_mag_9_over_10, 2);

    /// Compute bit metrics (lambda)
    simde__m128i bit_mets[16];
    for (int j = 0; j < 8; j += 4) {
      bit_mets[j + 0] = simde_mm_subs_epi16(psi_as[j + 0], a_sqs[j + 0]);
      bit_mets[j + 0] = simde_mm_adds_epi16(bit_mets[j + 0], y0_s[j + 0]);
      bit_mets[j + 0] = simde_mm_subs_epi16(bit_mets[j + 0], ch_mag_over_10);

      bit_mets[j + 1] = simde_mm_subs_epi16(psi_as[j + 1], a_sqs[j + 1]);
      bit_mets[j + 1] = simde_mm_adds_epi16(bit_mets[j + 1], y0_s[j + 1]);
      bit_mets[j + 1] = simde_mm_subs_epi16(bit_mets[j + 1], ch_mag_over_2);
      bit_mets[j + 2] = simde_mm_subs_epi16(psi_as[j + 2], a_sqs[j + 2]);
      bit_mets[j + 2] = simde_mm_adds_epi16(bit_mets[j + 2], y0_s[j + 2]);
      bit_mets[j + 2] = simde_mm_subs_epi16(bit_mets[j + 2], ch_mag_over_2);

      bit_mets[j + 3] = simde_mm_subs_epi16(psi_as[j + 3], a_sqs[j + 3]);
      bit_mets[j + 3] = simde_mm_adds_epi16(bit_mets[j + 3], y0_s[j + 3]);
      bit_mets[j + 3] = simde_mm_subs_epi16(bit_mets[j + 3], ch_mag_9_over_10);
    }
    for (int j = 8; j < 16; j += 4) {
      bit_mets[j + 0] = simde_mm_subs_epi16(psi_as[j + 0], a_sqs[j + 0]);
      bit_mets[j + 0] = simde_mm_subs_epi16(bit_mets[j + 0], y0_s[(j - 4) & 0x07]);
      bit_mets[j + 0] = simde_mm_subs_epi16(bit_mets[j + 0], ch_mag_over_10);

      bit_mets[j + 1] = simde_mm_subs_epi16(psi_as[j + 1], a_sqs[j + 1]);
      bit_mets[j + 1] = simde_mm_subs_epi16(bit_mets[j + 1], y0_s[(j - 3) & 0x07]);
      bit_mets[j + 1] = simde_mm_subs_epi16(bit_mets[j + 1], ch_mag_over_2);
      bit_mets[j + 2] = simde_mm_subs_epi16(psi_as[j + 2], a_sqs[j + 2]);
      bit_mets[j + 2] = simde_mm_subs_epi16(bit_mets[j + 2], y0_s[(j - 2) & 0x07]);
      bit_mets[j + 2] = simde_mm_subs_epi16(bit_mets[j + 2], ch_mag_over_2);

      bit_mets[j + 3] = simde_mm_subs_epi16(psi_as[j + 3], a_sqs[j + 3]);
      bit_mets[j + 3] = simde_mm_subs_epi16(bit_mets[j + 3], y0_s[(j - 1) & 0x07]);
      bit_mets[j + 3] = simde_mm_subs_epi16(bit_mets[j + 3], ch_mag_9_over_10);
    }
    /// Compute the LLRs

    // LLR = lambda(c==1) - lambda(c==0)

    // LLR of the first bit: Bit = 1
    simde__m128i logmax_num_re0 =
        max_epi16(bit_mets[8], bit_mets[9], bit_mets[10], bit_mets[11], bit_mets[12], bit_mets[13], bit_mets[14], bit_mets[15]);
    // LLR of the first bit: Bit = 0
    simde__m128i logmax_den_re0 =
        max_epi16(bit_mets[0], bit_mets[1], bit_mets[2], bit_mets[3], bit_mets[4], bit_mets[5], bit_mets[6], bit_mets[7]);
    // LLR of the second bit: Bit = 1
    simde__m128i logmax_num_re1 =
        max_epi16(bit_mets[4], bit_mets[5], bit_mets[6], bit_mets[7], bit_mets[12], bit_mets[13], bit_mets[14], bit_mets[15]);
    // LLR of the second bit: Bit = 0
    simde__m128i logmax_den_re1 =
        max_epi16(bit_mets[0], bit_mets[1], bit_mets[3], bit_mets[2], bit_mets[8], bit_mets[9], bit_mets[10], bit_mets[11]);
    // LLR of the third bit: Bit = 1
    simde__m128i logmax_num_im0 =
        max_epi16(bit_mets[2], bit_mets[3], bit_mets[6], bit_mets[7], bit_mets[10], bit_mets[11], bit_mets[14], bit_mets[15]);
    // LLR of the third bit: Bit = 0
    simde__m128i logmax_den_im0 =
        max_epi16(bit_mets[0], bit_mets[1], bit_mets[4], bit_mets[5], bit_mets[8], bit_mets[9], bit_mets[12], bit_mets[13]);
    // LLR of the fourth bit: Bit = 1
    simde__m128i logmax_num_im1 =
        max_epi16(bit_mets[1], bit_mets[3], bit_mets[5], bit_mets[7], bit_mets[9], bit_mets[11], bit_mets[13], bit_mets[15]);
    // LLR of the fourth bit: Bit = 0
    simde__m128i logmax_den_im1 =
        max_epi16(bit_mets[0], bit_mets[2], bit_mets[4], bit_mets[6], bit_mets[8], bit_mets[10], bit_mets[12], bit_mets[14]);

    y0r = simde_mm_subs_epi16(logmax_den_re0,
                              logmax_num_re0); // LLR of first bit  [L1(1), L1(2), L1(3), L1(4), L1(5), L1(6), L1(7), L1(8)]
    y1r = simde_mm_subs_epi16(logmax_den_re1,
                              logmax_num_re1); // LLR of second bit [L2(1), L2(2), L2(3), L2(4), L2(5), L2(6), L2(7), L2(8)]
    y0i = simde_mm_subs_epi16(logmax_den_im0,
                              logmax_num_im0); // LLR of third bit  [L3(1), L3(2), L3(3), L3(4), L3(5), L3(6), L3(7), L3(8)]
    y1i = simde_mm_subs_epi16(logmax_den_im1,
                              logmax_num_im1); // LLR of fourth bit [L4(1), L4(2), L4(3), L4(4), L4(5), L4(6), L4(7), L4(8)]

    // Pack LLRs in output
    xmm0 = simde_mm_unpacklo_epi16(y0r, y1r); // [L1(1), L2(1), L1(2), L2(2), L1(3), L2(3), L1(4), L2(4)]
    xmm1 = simde_mm_unpackhi_epi16(y0r, y1r); // [L1(5), L2(5), L1(6), L2(6), L1(7), L2(7), L1(8), L2(8)]
    xmm2 = simde_mm_unpacklo_epi16(y0i, y1i); // [L3(1), L4(1), L3(2), L4(2), L3(3), L4(3), L3(4), L4(4)]
    xmm3 = simde_mm_unpackhi_epi16(y0i, y1i); // [L3(5), L4(5), L3(6), L4(6), L3(7), L4(7), L3(8), L4(8)]
    stream0_128i_out[2 * i + 0] = simde_mm_unpacklo_epi32(xmm0, xmm2); // 8 LLRs, 2 REs
    stream0_128i_out[2 * i + 1] = simde_mm_unpackhi_epi32(xmm0, xmm2); // 8 LLRs, 2 REs
    stream0_128i_out[2 * i + 2] = simde_mm_unpacklo_epi32(xmm1, xmm3); // 8 LLRs, 2 REs
    stream0_128i_out[2 * i + 3] = simde_mm_unpackhi_epi32(xmm1, xmm3); // 8 LLRs, 2 REs
  }
#else
  simde__m256i *rho01_256i = (simde__m256i *)rho01;
  simde__m256i *stream0_256i_in = (simde__m256i *)stream0_in;
  simde__m256i *stream1_256i_in = (simde__m256i *)stream1_in;
  simde__m256i *stream0_256i_out = (simde__m256i *)stream0_out;
  simde__m256i *ch_mag_256i = (simde__m256i *)ch_mag;
  simde__m256i *ch_mag_256i_i = (simde__m256i *)ch_mag_i;

  simde__m256i ONE_OVER_SQRT_10 = simde_mm256_set1_epi16(20724); // round(1/sqrt(10)*2^16)
  simde__m256i ONE_OVER_SQRT_10_Q15 = simde_mm256_set1_epi16(10362); // round(1/sqrt(10)*2^15)
  simde__m256i THREE_OVER_SQRT_10 = simde_mm256_set1_epi16(31086); // round(3/sqrt(10)*2^15)
  simde__m256i SQRT_10_OVER_FOUR = simde_mm256_set1_epi16(25905); // round(sqrt(10)/4*2^15)
  simde__m256i ONE_OVER_TWO_SQRT_10 = simde_mm256_set1_epi16(10362); // round(1/2/sqrt(10)*2^16)
  simde__m256i NINE_OVER_TWO_SQRT_10 = simde_mm256_set1_epi16(23315); // round(9/2/sqrt(10)*2^14)
  simde__m256i ch_mag_des, ch_mag_int;
  simde__m256i y0r_over_sqrt10;
  simde__m256i y0i_over_sqrt10;
  simde__m256i y0r_three_over_sqrt10;
  simde__m256i y0i_three_over_sqrt10;
  simde__m256i ch_mag_over_10;
  simde__m256i ch_mag_over_2;
  simde__m256i ch_mag_9_over_10;

  simde__m256i xmm2;
  simde__m256i xmm3;
  simde__m256i xmm4;
  simde__m256i xmm5;
  simde__m256i xmm6;
  simde__m256i xmm7;

  simde__m256i rho_rpi;
  simde__m256i rho_rmi;
  simde__m256i rho_rs[8];
  simde__m256i psi_rs[16];
  simde__m256i psi_is[16];
  simde__m256i a_rs[16];
  simde__m256i a_is[16];
  simde__m256i psi_as[16];
  simde__m256i a_sqs[16];
  simde__m256i y0_s[8];

  simde__m256i y0r;
  simde__m256i y0i;
  simde__m256i y1r;
  simde__m256i y1i;

  // In one iteration, we deal with 8 REs
  for (int i = 0; i < length >> 3; i += 2) {
    // Get rho
    oai_mm256_separate_real_imag_parts(&xmm2, &xmm3, rho01_256i[i], rho01_256i[i + 1]);
    rho_rpi = simde_mm256_adds_epi16(xmm2, xmm3); // rho = Re(rho) + Im(rho)
    rho_rmi = simde_mm256_subs_epi16(xmm2, xmm3); // rho* = Re(rho) - Im(rho)

    // Compute the different rhos
    rho_rs[0] = simde_mm256_mulhi_epi16(rho_rpi, ONE_OVER_SQRT_10);
    rho_rs[4] = simde_mm256_mulhi_epi16(rho_rmi, ONE_OVER_SQRT_10);
    rho_rs[3] = simde_mm256_slli_epi16(simde_mm256_mulhi_epi16(rho_rpi, THREE_OVER_SQRT_10), 1);
    rho_rs[7] = simde_mm256_slli_epi16(simde_mm256_mulhi_epi16(rho_rmi, THREE_OVER_SQRT_10), 1);

    xmm4 = simde_mm256_mulhi_epi16(xmm2, ONE_OVER_SQRT_10); // Re(rho)
    xmm5 = simde_mm256_mulhi_epi16(xmm3, THREE_OVER_SQRT_10); // Im(rho)
    xmm5 = simde_mm256_slli_epi16(xmm5, 1);

    rho_rs[1] = simde_mm256_adds_epi16(xmm4, xmm5);
    rho_rs[5] = simde_mm256_subs_epi16(xmm4, xmm5);

    xmm6 = simde_mm256_mulhi_epi16(xmm2, THREE_OVER_SQRT_10); // Re(rho)
    xmm7 = simde_mm256_mulhi_epi16(xmm3, ONE_OVER_SQRT_10); // Im(rho)
    xmm6 = simde_mm256_slli_epi16(xmm6, 1);

    rho_rs[2] = simde_mm256_adds_epi16(xmm6, xmm7);
    rho_rs[6] = simde_mm256_subs_epi16(xmm6, xmm7);

    // Rearrange interfering MF output
    oai_mm256_separate_real_imag_parts(&y1r, &y1i, stream1_256i_in[i], stream1_256i_in[i + 1]);

    // |  [Re(rho)+ Im(rho)]/sqrt(10) - y1r  |
    for (int j = 0; j < 8; j++) { // psi_rs[0~7], rho_rs[0~7]
      psi_rs[j] = simde_mm256_abs_epi16(simde_mm256_subs_epi16(rho_rs[j], y1r));
    }
    for (int j = 8; j < 16; j++) { // psi_rs[8~16], rho_rs[4,5,6,7,0,1,2,3]
      psi_rs[j] = simde_mm256_abs_epi16(simde_mm256_adds_epi16(rho_rs[(j - 4) & 7], y1r));
    }
    const uint8_t rho_rs_indexes[16] = {4, 6, 5, 7, 0, 2, 1, 3, 0, 2, 1, 3, 4, 6, 5, 7};
    for (int k = 0; k < 16;
         k += 8) { // psi_is[0~15], sub(rho_rs[4,6,5,7]), add(rho_rs[0,2,1,3]), sub(rho_rs[0,2,1,3]), add(rho_rs[4,6,5,7])
      for (int j = k; j < k + 4; j++) {
        psi_is[j] = simde_mm256_abs_epi16(simde_mm256_subs_epi16(rho_rs[rho_rs_indexes[j]], y1i));
        psi_is[j + 4] = simde_mm256_abs_epi16(simde_mm256_adds_epi16(rho_rs[rho_rs_indexes[j + 4]], y1i));
      }
    }

    // Rearrange desired MF output
    oai_mm256_separate_real_imag_parts(&y0r, &y0i, stream0_256i_in[i], stream0_256i_in[i + 1]);

    // Rearrange desired channel magnitudes
    // [|h|^2(1),|h|^2(2),|h|^2(3),|h|^2(4)]*(2/sqrt(10))
    oai_mm256_separate_real_imag_parts(&ch_mag_des, &xmm2, ch_mag_256i[i], ch_mag_256i[i + 1]);

    // Rearrange interfering channel magnitudes
    oai_mm256_separate_real_imag_parts(&ch_mag_int, &xmm2, ch_mag_256i_i[i], ch_mag_256i_i[i + 1]);

    // Scale MF output of desired signal
    y0r_over_sqrt10 = simde_mm256_mulhi_epi16(y0r, ONE_OVER_SQRT_10);
    y0i_over_sqrt10 = simde_mm256_mulhi_epi16(y0i, ONE_OVER_SQRT_10);
    y0r_three_over_sqrt10 = simde_mm256_slli_epi16(simde_mm256_mulhi_epi16(y0r, THREE_OVER_SQRT_10), 1);
    y0i_three_over_sqrt10 = simde_mm256_slli_epi16(simde_mm256_mulhi_epi16(y0i, THREE_OVER_SQRT_10), 1);

    // Compute necessary combination of required terms
    y0_s[0] = simde_mm256_adds_epi16(y0r_over_sqrt10, y0i_over_sqrt10);
    y0_s[4] = simde_mm256_subs_epi16(y0r_over_sqrt10, y0i_over_sqrt10);

    y0_s[1] = simde_mm256_adds_epi16(y0r_over_sqrt10, y0i_three_over_sqrt10);
    y0_s[5] = simde_mm256_subs_epi16(y0r_over_sqrt10, y0i_three_over_sqrt10);

    y0_s[2] = simde_mm256_adds_epi16(y0r_three_over_sqrt10, y0i_over_sqrt10);
    y0_s[6] = simde_mm256_subs_epi16(y0r_three_over_sqrt10, y0i_over_sqrt10);

    y0_s[3] = simde_mm256_adds_epi16(y0r_three_over_sqrt10, y0i_three_over_sqrt10);
    y0_s[7] = simde_mm256_subs_epi16(y0r_three_over_sqrt10, y0i_three_over_sqrt10);

    for (int j = 0; j < 16; j++) {
      // Compute optimal interfering symbol magnitude
      a_rs[j] = interference_abs_epi16_256(psi_rs[j], ch_mag_int, ONE_OVER_SQRT_10_Q15, THREE_OVER_SQRT_10);
      a_is[j] = interference_abs_epi16_256(psi_is[j], ch_mag_int, ONE_OVER_SQRT_10_Q15, THREE_OVER_SQRT_10);

      // Calculation of groups of two terms in the bit metric involving product of psi and interference magnitude
      psi_as[j] = prodsum_psi_a_epi16_256(psi_rs[j], a_rs[j], psi_is[j], a_is[j]);

      // squared interference magnitude times int. ch. power
      a_sqs[j] = square_a_epi16_256(a_rs[j], a_is[j], ch_mag_int, SQRT_10_OVER_FOUR);
    }

    // Computing different multiples of channel norms
    ch_mag_over_10 = simde_mm256_mulhi_epi16(ch_mag_des, ONE_OVER_TWO_SQRT_10);
    ch_mag_over_2 = simde_mm256_mulhi_epi16(ch_mag_des, SQRT_10_OVER_FOUR);
    ch_mag_over_2 = simde_mm256_slli_epi16(ch_mag_over_2, 1);
    ch_mag_9_over_10 = simde_mm256_mulhi_epi16(ch_mag_des, NINE_OVER_TWO_SQRT_10);
    ch_mag_9_over_10 = simde_mm256_slli_epi16(ch_mag_9_over_10, 2);

    /// Compute bit metrics (lambda)

    simde__m256i bit_mets[16];
    for (int j = 0; j < 8; j += 4) {
      bit_mets[j + 0] = simde_mm256_subs_epi16(psi_as[j + 0], a_sqs[j + 0]);
      bit_mets[j + 0] = simde_mm256_adds_epi16(bit_mets[j + 0], y0_s[j + 0]);
      bit_mets[j + 0] = simde_mm256_subs_epi16(bit_mets[j + 0], ch_mag_over_10);

      bit_mets[j + 1] = simde_mm256_subs_epi16(psi_as[j + 1], a_sqs[j + 1]);
      bit_mets[j + 1] = simde_mm256_adds_epi16(bit_mets[j + 1], y0_s[j + 1]);
      bit_mets[j + 1] = simde_mm256_subs_epi16(bit_mets[j + 1], ch_mag_over_2);
      bit_mets[j + 2] = simde_mm256_subs_epi16(psi_as[j + 2], a_sqs[j + 2]);
      bit_mets[j + 2] = simde_mm256_adds_epi16(bit_mets[j + 2], y0_s[j + 2]);
      bit_mets[j + 2] = simde_mm256_subs_epi16(bit_mets[j + 2], ch_mag_over_2);

      bit_mets[j + 3] = simde_mm256_subs_epi16(psi_as[j + 3], a_sqs[j + 3]);
      bit_mets[j + 3] = simde_mm256_adds_epi16(bit_mets[j + 3], y0_s[j + 3]);
      bit_mets[j + 3] = simde_mm256_subs_epi16(bit_mets[j + 3], ch_mag_9_over_10);
    }
    for (int j = 8; j < 16; j += 4) {
      bit_mets[j + 0] = simde_mm256_subs_epi16(psi_as[j + 0], a_sqs[j + 0]);
      bit_mets[j + 0] = simde_mm256_subs_epi16(bit_mets[j + 0], y0_s[(j - 4) & 0x07]);
      bit_mets[j + 0] = simde_mm256_subs_epi16(bit_mets[j + 0], ch_mag_over_10);

      bit_mets[j + 1] = simde_mm256_subs_epi16(psi_as[j + 1], a_sqs[j + 1]);
      bit_mets[j + 1] = simde_mm256_subs_epi16(bit_mets[j + 1], y0_s[(j - 3) & 0x07]);
      bit_mets[j + 1] = simde_mm256_subs_epi16(bit_mets[j + 1], ch_mag_over_2);
      bit_mets[j + 2] = simde_mm256_subs_epi16(psi_as[j + 2], a_sqs[j + 2]);
      bit_mets[j + 2] = simde_mm256_subs_epi16(bit_mets[j + 2], y0_s[(j - 2) & 0x07]);
      bit_mets[j + 2] = simde_mm256_subs_epi16(bit_mets[j + 2], ch_mag_over_2);

      bit_mets[j + 3] = simde_mm256_subs_epi16(psi_as[j + 3], a_sqs[j + 3]);
      bit_mets[j + 3] = simde_mm256_subs_epi16(bit_mets[j + 3], y0_s[(j - 1) & 0x07]);
      bit_mets[j + 3] = simde_mm256_subs_epi16(bit_mets[j + 3], ch_mag_9_over_10);
    }

    /// Compute the LLRs

    // LLR = lambda(c==1) - lambda(c==0)

    // LLR of the first bit: Bit = 1
    simde__m256i logmax_num_re0 =
        max_epi16_256(bit_mets[8], bit_mets[9], bit_mets[10], bit_mets[11], bit_mets[12], bit_mets[13], bit_mets[14], bit_mets[15]);
    // LLR of the first bit: Bit = 0
    simde__m256i logmax_den_re0 =
        max_epi16_256(bit_mets[0], bit_mets[1], bit_mets[2], bit_mets[3], bit_mets[4], bit_mets[5], bit_mets[6], bit_mets[7]);
    // LLR of the second bit: Bit = 1
    simde__m256i logmax_num_re1 =
        max_epi16_256(bit_mets[4], bit_mets[5], bit_mets[6], bit_mets[7], bit_mets[12], bit_mets[13], bit_mets[14], bit_mets[15]);
    // LLR of the second bit: Bit = 0
    simde__m256i logmax_den_re1 =
        max_epi16_256(bit_mets[0], bit_mets[1], bit_mets[3], bit_mets[2], bit_mets[8], bit_mets[9], bit_mets[10], bit_mets[11]);
    // LLR of the third bit: Bit = 1
    simde__m256i logmax_num_im0 =
        max_epi16_256(bit_mets[2], bit_mets[3], bit_mets[6], bit_mets[7], bit_mets[10], bit_mets[11], bit_mets[14], bit_mets[15]);
    // LLR of the third bit: Bit = 0
    simde__m256i logmax_den_im0 =
        max_epi16_256(bit_mets[0], bit_mets[1], bit_mets[4], bit_mets[5], bit_mets[8], bit_mets[9], bit_mets[12], bit_mets[13]);
    // LLR of the fourth bit: Bit = 1
    simde__m256i logmax_num_im1 =
        max_epi16_256(bit_mets[1], bit_mets[3], bit_mets[5], bit_mets[7], bit_mets[9], bit_mets[11], bit_mets[13], bit_mets[15]);
    // LLR of the fourth bit: Bit = 0
    simde__m256i logmax_den_im1 =
        max_epi16_256(bit_mets[0], bit_mets[2], bit_mets[4], bit_mets[6], bit_mets[8], bit_mets[10], bit_mets[12], bit_mets[14]);

    y0r = simde_mm256_subs_epi16(logmax_den_re0,
                                 logmax_num_re0); // LLR of first bit  [L1(1), L1(2), L1(3), L1(4), L1(5), L1(6), L1(7), L1(8)...]
    y1r = simde_mm256_subs_epi16(logmax_den_re1,
                                 logmax_num_re1); // LLR of second bit [L2(1), L2(2), L2(3), L2(4), L2(5), L2(6), L2(7), L2(8)...]
    y0i = simde_mm256_subs_epi16(logmax_den_im0,
                                 logmax_num_im0); // LLR of third bit  [L3(1), L3(2), L3(3), L3(4), L3(5), L3(6), L3(7), L3(8)...]
    y1i = simde_mm256_subs_epi16(logmax_den_im1,
                                 logmax_num_im1); // LLR of fourth bit [L4(1), L4(2), L4(3), L4(4), L4(5), L4(6), L4(7), L4(8)...]

    // Pack LLRs in output
    simde__m128i *y0r_128 = (simde__m128i *)&y0r;
    simde__m128i *y1r_128 = (simde__m128i *)&y1r;
    simde__m128i *y0i_128 = (simde__m128i *)&y0i;
    simde__m128i *y1i_128 = (simde__m128i *)&y1i;
    simde__m128i xmm0_128, xmm1_128, xmm2_128, xmm3_128;
    xmm0_128 = simde_mm_unpacklo_epi16(y0r_128[0], y1r_128[0]); // [L1(1), L2(1), L1(2), L2(2), L1(3), L2(3), L1(4), L2(4)]
    xmm1_128 = simde_mm_unpackhi_epi16(y0r_128[0], y1r_128[0]); // [L1(5), L2(5), L1(6), L2(6), L1(7), L2(7), L1(8), L2(8)]
    xmm2_128 = simde_mm_unpacklo_epi16(y0i_128[0], y1i_128[0]); // [L3(1), L4(1), L3(2), L4(2), L3(3), L4(3), L3(4), L4(4)]
    xmm3_128 = simde_mm_unpackhi_epi16(y0i_128[0], y1i_128[0]); // [L3(5), L4(5), L3(6), L4(6), L3(7), L4(7), L3(8), L4(8)]
    simde__m128i *stream0_128i_out = (simde__m128i *)&stream0_256i_out[2 * i + 0];
    stream0_128i_out[0] = simde_mm_unpacklo_epi32(xmm0_128, xmm2_128); // 8 LLRs, 2 REs
    stream0_128i_out[1] = simde_mm_unpackhi_epi32(xmm0_128, xmm2_128); // 8 LLRs, 2 REs
    stream0_128i_out[2] = simde_mm_unpacklo_epi32(xmm1_128, xmm3_128); // 8 LLRs, 2 REs
    stream0_128i_out[3] = simde_mm_unpackhi_epi32(xmm1_128, xmm3_128); // 8 LLRs, 2 REs
    xmm0_128 = simde_mm_unpacklo_epi16(y0r_128[1], y1r_128[1]); // [L1(9), L2(9), L1(10), L2(10), L1(11), L2(11), L1(12), L2(12)]
    xmm1_128 = simde_mm_unpackhi_epi16(y0r_128[1], y1r_128[1]); // [L1(13), L2(13), L1(14), L2(14), L1(15), L2(15), L1(16), L2(16)]
    xmm2_128 = simde_mm_unpacklo_epi16(y0i_128[1], y1i_128[1]); // [L3(9), L4(9), L3(10), L4(10), L3(11), L4(11), L3(12), L4(12)]
    xmm3_128 = simde_mm_unpackhi_epi16(y0i_128[1], y1i_128[1]); // [L3(13), L4(13), L3(14), L4(14), L3(15), L4(15), L3(16), L4(16)]
    stream0_128i_out = (simde__m128i *)&stream0_256i_out[2 * i + 2];
    stream0_128i_out[0] = simde_mm_unpacklo_epi32(xmm0_128, xmm2_128); // 8 LLRs, 2 REs
    stream0_128i_out[1] = simde_mm_unpackhi_epi32(xmm0_128, xmm2_128); // 8 LLRs, 2 REs
    stream0_128i_out[2] = simde_mm_unpacklo_epi32(xmm1_128, xmm3_128); // 8 LLRs, 2 REs
    stream0_128i_out[3] = simde_mm_unpackhi_epi32(xmm1_128, xmm3_128); // 8 LLRs, 2 REs
  }
#endif
}

/*
 * This function computes the LLRs of stream 0 (s_0) in presence of the interfering stream 1 (s_1) assuming that both symbols are
 * 64QAM. It can be used for both MU-MIMO interference-aware receiver or for SU-MIMO receivers.
 *
 * Input:
 *   stream0_in:  MF filter output for 1st stream, i.e., y0' = h0'*y0
 *   stream1_in:  MF filter output for 2nd stream, i.e., y1' = h1'*y0
 *   ch_mag:      4*h0/sqrt(42), [Re0 Im0 Re1 Im1] s.t. Im0=Re0, Im1=Re1, etc
 *   ch_mag_i:    4*h0/sqrt(42), [Re0 Im0 Re1 Im1] s.t. Im0=Re0, Im1=Re1, etc
 *   rho01:       Channel cross correlation, i.e., rho01 = h0'*h1
 *   length:      Number of resource elements
 *
 * Output:
 *   stream0_out: Output LLRs for 1st stream
 */
void nr_qam64_llr_2layer(c16_t *stream0_in,
                         c16_t *stream1_in,
                         c16_t *ch_mag,
                         c16_t *ch_mag_i,
                         int16_t *stream0_out,
                         c16_t *rho01,
                         uint32_t length)
{
#ifdef USE_128BIT
  simde__m128i *rho01_128i = (simde__m128i *)rho01;
  simde__m128i *stream0_128i_in = (simde__m128i *)stream0_in;
  simde__m128i *stream1_128i_in = (simde__m128i *)stream1_in;
  simde__m128i *ch_mag_128i = (simde__m128i *)ch_mag;
  simde__m128i *ch_mag_128i_i = (simde__m128i *)ch_mag_i;

  simde__m128i ONE_OVER_SQRT_42 = simde_mm_set1_epi16(10112); // round(1/sqrt(42)*2^16)
  simde__m128i THREE_OVER_SQRT_42 = simde_mm_set1_epi16(30337); // round(3/sqrt(42)*2^16)
  simde__m128i FIVE_OVER_SQRT_42 = simde_mm_set1_epi16(25281); // round(5/sqrt(42)*2^15)
  simde__m128i SEVEN_OVER_SQRT_42 = simde_mm_set1_epi16(17697); // round(7/sqrt(42)*2^14) Q2.14
  simde__m128i ONE_OVER_SQRT_2 = simde_mm_set1_epi16(23170); // round(1/sqrt(2)*2^15)
  simde__m128i ONE_OVER_SQRT_2_42 = simde_mm_set1_epi16(3575); // round(1/sqrt(2*42)*2^15)
  simde__m128i THREE_OVER_SQRT_2_42 = simde_mm_set1_epi16(10726); // round(3/sqrt(2*42)*2^15)
  simde__m128i FIVE_OVER_SQRT_2_42 = simde_mm_set1_epi16(17876); // round(5/sqrt(2*42)*2^15)
  simde__m128i SEVEN_OVER_SQRT_2_42 = simde_mm_set1_epi16(25027); // round(7/sqrt(2*42)*2^15)
  simde__m128i FORTYNINE_OVER_FOUR_SQRT_42 = simde_mm_set1_epi16(30969); // round(49/(4*sqrt(42))*2^14), Q2.14
  simde__m128i THIRTYSEVEN_OVER_FOUR_SQRT_42 = simde_mm_set1_epi16(23385); // round(37/(4*sqrt(42))*2^14), Q2.14
  simde__m128i TWENTYFIVE_OVER_FOUR_SQRT_42 = simde_mm_set1_epi16(31601); // round(25/(4*sqrt(42))*2^15)
  simde__m128i TWENTYNINE_OVER_FOUR_SQRT_42 = simde_mm_set1_epi16(18329); // round(29/(4*sqrt(42))*2^15), Q2.14
  simde__m128i SEVENTEEN_OVER_FOUR_SQRT_42 = simde_mm_set1_epi16(21489); // round(17/(4*sqrt(42))*2^15)
  simde__m128i NINE_OVER_FOUR_SQRT_42 = simde_mm_set1_epi16(11376); // round(9/(4*sqrt(42))*2^15)
  simde__m128i THIRTEEN_OVER_FOUR_SQRT_42 = simde_mm_set1_epi16(16433); // round(13/(4*sqrt(42))*2^15)
  simde__m128i FIVE_OVER_FOUR_SQRT_42 = simde_mm_set1_epi16(6320); // round(5/(4*sqrt(42))*2^15)
  simde__m128i ONE_OVER_FOUR_SQRT_42 = simde_mm_set1_epi16(1264); // round(1/(4*sqrt(42))*2^15)
  simde__m128i SQRT_42_OVER_FOUR = simde_mm_set1_epi16(13272); // round(sqrt(42)/4*2^13), Q3.12

  simde__m128i ch_mag_des;
  simde__m128i ch_mag_int;
  simde__m128i y0r_one_over_sqrt_21;
  simde__m128i y0r_three_over_sqrt_21;
  simde__m128i y0r_five_over_sqrt_21;
  simde__m128i y0r_seven_over_sqrt_21;
  simde__m128i y0i_one_over_sqrt_21;
  simde__m128i y0i_three_over_sqrt_21;
  simde__m128i y0i_five_over_sqrt_21;
  simde__m128i y0i_seven_over_sqrt_21;
  simde__m128i ch_mag_int_with_sigma2;
  simde__m128i two_ch_mag_int_with_sigma2;
  simde__m128i three_ch_mag_int_with_sigma2;

  for (int i = 0; i < length >> 2; i += 2) {
    // Get rho
    simde__m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8;
    oai_mm_separate_real_imag_parts(&xmm2, &xmm3, rho01_128i[i], rho01_128i[i + 1]);

    simde__m128i rho_rpi = simde_mm_adds_epi16(xmm2, xmm3); // rho = Re(rho) + Im(rho)
    simde__m128i rho_rmi = simde_mm_subs_epi16(xmm2, xmm3); // rho* = Re(rho) - Im(rho)

    // Compute the different rhos
    simde__m128i rho_rs[32];
    rho_rs[27] = simde_mm_mulhi_epi16(rho_rpi, ONE_OVER_SQRT_42);
    rho_rs[28] = simde_mm_mulhi_epi16(rho_rmi, ONE_OVER_SQRT_42);
    rho_rs[18] = simde_mm_mulhi_epi16(rho_rpi, THREE_OVER_SQRT_42);
    rho_rs[21] = simde_mm_mulhi_epi16(rho_rmi, THREE_OVER_SQRT_42);
    rho_rs[9] = simde_mm_mulhi_epi16(rho_rpi, FIVE_OVER_SQRT_42);
    rho_rs[14] = simde_mm_mulhi_epi16(rho_rmi, FIVE_OVER_SQRT_42);
    rho_rs[0] = simde_mm_mulhi_epi16(rho_rpi, SEVEN_OVER_SQRT_42);
    rho_rs[7] = simde_mm_mulhi_epi16(rho_rmi, SEVEN_OVER_SQRT_42);

    rho_rs[9] = simde_mm_slli_epi16(rho_rs[9], 1);
    rho_rs[14] = simde_mm_slli_epi16(rho_rs[14], 1);
    rho_rs[0] = simde_mm_slli_epi16(rho_rs[0], 2);
    rho_rs[7] = simde_mm_slli_epi16(rho_rs[7], 2);

    xmm4 = simde_mm_mulhi_epi16(xmm2, ONE_OVER_SQRT_42);
    xmm5 = simde_mm_mulhi_epi16(xmm3, ONE_OVER_SQRT_42);
    xmm6 = simde_mm_mulhi_epi16(xmm3, THREE_OVER_SQRT_42);
    xmm7 = simde_mm_mulhi_epi16(xmm3, FIVE_OVER_SQRT_42);
    xmm8 = simde_mm_mulhi_epi16(xmm3, SEVEN_OVER_SQRT_42);
    xmm7 = simde_mm_slli_epi16(xmm7, 1);
    xmm8 = simde_mm_slli_epi16(xmm8, 2);

    rho_rs[26] = simde_mm_adds_epi16(xmm4, xmm6);
    rho_rs[29] = simde_mm_subs_epi16(xmm4, xmm6);
    rho_rs[25] = simde_mm_adds_epi16(xmm4, xmm7);
    rho_rs[30] = simde_mm_subs_epi16(xmm4, xmm7);
    rho_rs[24] = simde_mm_adds_epi16(xmm4, xmm8);
    rho_rs[31] = simde_mm_subs_epi16(xmm4, xmm8);

    xmm4 = simde_mm_mulhi_epi16(xmm2, THREE_OVER_SQRT_42);
    rho_rs[19] = simde_mm_adds_epi16(xmm4, xmm5);
    rho_rs[20] = simde_mm_subs_epi16(xmm4, xmm5);
    rho_rs[17] = simde_mm_adds_epi16(xmm4, xmm7);
    rho_rs[22] = simde_mm_subs_epi16(xmm4, xmm7);
    rho_rs[16] = simde_mm_adds_epi16(xmm4, xmm8);
    rho_rs[23] = simde_mm_subs_epi16(xmm4, xmm8);

    xmm4 = simde_mm_mulhi_epi16(xmm2, FIVE_OVER_SQRT_42);
    xmm4 = simde_mm_slli_epi16(xmm4, 1);
    rho_rs[11] = simde_mm_adds_epi16(xmm4, xmm5);
    rho_rs[12] = simde_mm_subs_epi16(xmm4, xmm5);
    rho_rs[10] = simde_mm_adds_epi16(xmm4, xmm6);
    rho_rs[13] = simde_mm_subs_epi16(xmm4, xmm6);
    rho_rs[8] = simde_mm_adds_epi16(xmm4, xmm8);
    rho_rs[15] = simde_mm_subs_epi16(xmm4, xmm8);

    xmm4 = simde_mm_mulhi_epi16(xmm2, SEVEN_OVER_SQRT_42);
    xmm4 = simde_mm_slli_epi16(xmm4, 2);
    rho_rs[3] = simde_mm_adds_epi16(xmm4, xmm5);
    rho_rs[4] = simde_mm_subs_epi16(xmm4, xmm5);
    rho_rs[2] = simde_mm_adds_epi16(xmm4, xmm6);
    rho_rs[5] = simde_mm_subs_epi16(xmm4, xmm6);
    rho_rs[1] = simde_mm_adds_epi16(xmm4, xmm7);
    rho_rs[6] = simde_mm_subs_epi16(xmm4, xmm7);

    // Rearrange interfering MF output
    simde__m128i y1r, y1i;
    oai_mm_separate_real_imag_parts(&y1r, &y1i, stream1_128i_in[i], stream1_128i_in[i + 1]);

    // Psi_r calculation from rho_rpi or rho_rmi
    xmm0 = simde_mm_set1_epi16(0); // ZERO for abs_pi16
    xmm2 = simde_mm_subs_epi16(rho_rs[0], y1r);

    simde__m128i psi_r_s[64];
    for (int j = 0; j < 32; j++) // psi_r_s[0~31], rho_rs[0~31]
      psi_r_s[j] = simde_mm_abs_epi16(simde_mm_subs_epi16(rho_rs[j], y1r));
    for (int j = 32; j < 64; j++) // psi_r_s[32~64], rho_rs[31~0]
      psi_r_s[j] = simde_mm_abs_epi16(simde_mm_adds_epi16(rho_rs[63 - j], y1r));

    // simde__m128i psi_i calculation from rho_rpi or rho_rmi
    simde__m128i psi_i_s[64];
    const uint8_t rho_rs_index[32] = {7, 15, 23, 31, 24, 16, 8,  0, 6, 14, 22, 30, 25, 17, 9,  1,
                                      5, 13, 21, 29, 26, 18, 10, 2, 4, 12, 20, 28, 27, 19, 11, 3};
    for (int k = 0; k < 32; k += 8) { // psi_i_s[0~31]
      for (int j = k; j < k + 4; j++)
        psi_i_s[j] = simde_mm_abs_epi16(simde_mm_subs_epi16(rho_rs[rho_rs_index[j]], y1i));
      for (int j = k + 4; j < k + 8; j++)
        psi_i_s[j] = simde_mm_abs_epi16(simde_mm_adds_epi16(rho_rs[rho_rs_index[j]], y1i));
    }
    for (int k = 32; k < 64; k += 8) { // psi_i_s[32~64]
      for (int j = k; j < k + 4; j++)
        psi_i_s[j] = simde_mm_abs_epi16(simde_mm_subs_epi16(rho_rs[rho_rs_index[63 - j]], y1i));
      for (int j = k + 4; j < k + 8; j++)
        psi_i_s[j] = simde_mm_abs_epi16(simde_mm_adds_epi16(rho_rs[rho_rs_index[63 - j]], y1i));
    }

    // Rearrange desired MF output
    simde__m128i y0r, y0i;
    oai_mm_separate_real_imag_parts(&y0r, &y0i, stream0_128i_in[i], stream0_128i_in[i + 1]);

    // Rearrange desired channel magnitudes
    // [|h|^2(1),|h|^2(1),|h|^2(2),|h|^2(2),...,,|h|^2(7),|h|^2(7)]*(2/sqrt(10))
    // xmm2 is dummy variable that contains the same values as ch_mag_des
    oai_mm_separate_real_imag_parts(&ch_mag_des, &xmm2, ch_mag_128i[i], ch_mag_128i[i + 1]);

    // Rearrange interfering channel magnitudes
    oai_mm_separate_real_imag_parts(&ch_mag_int, &xmm2, ch_mag_128i_i[i], ch_mag_128i_i[i + 1]);

    y0r_one_over_sqrt_21 = simde_mm_mulhi_epi16(y0r, ONE_OVER_SQRT_42);
    y0r_three_over_sqrt_21 = simde_mm_mulhi_epi16(y0r, THREE_OVER_SQRT_42);
    y0r_five_over_sqrt_21 = simde_mm_mulhi_epi16(y0r, FIVE_OVER_SQRT_42);
    y0r_five_over_sqrt_21 = simde_mm_slli_epi16(y0r_five_over_sqrt_21, 1);
    y0r_seven_over_sqrt_21 = simde_mm_mulhi_epi16(y0r, SEVEN_OVER_SQRT_42);
    y0r_seven_over_sqrt_21 = simde_mm_slli_epi16(y0r_seven_over_sqrt_21, 2); // Q2.14

    y0i_one_over_sqrt_21 = simde_mm_mulhi_epi16(y0i, ONE_OVER_SQRT_42);
    y0i_three_over_sqrt_21 = simde_mm_mulhi_epi16(y0i, THREE_OVER_SQRT_42);
    y0i_five_over_sqrt_21 = simde_mm_mulhi_epi16(y0i, FIVE_OVER_SQRT_42);
    y0i_five_over_sqrt_21 = simde_mm_slli_epi16(y0i_five_over_sqrt_21, 1);
    y0i_seven_over_sqrt_21 = simde_mm_mulhi_epi16(y0i, SEVEN_OVER_SQRT_42);
    y0i_seven_over_sqrt_21 = simde_mm_slli_epi16(y0i_seven_over_sqrt_21, 2); // Q2.14

    simde__m128i y0_s[64];
    const simde__m128i y0r_over_s[8] = {y0r_seven_over_sqrt_21,
                                        y0r_five_over_sqrt_21,
                                        y0r_three_over_sqrt_21,
                                        y0r_one_over_sqrt_21};
    for (int j = 0; j < 32; j += 8) {
      y0_s[j + 0] = simde_mm_adds_epi16(y0r_over_s[j >> 3], y0i_seven_over_sqrt_21);
      y0_s[j + 1] = simde_mm_adds_epi16(y0r_over_s[j >> 3], y0i_five_over_sqrt_21);
      y0_s[j + 2] = simde_mm_adds_epi16(y0r_over_s[j >> 3], y0i_three_over_sqrt_21);
      y0_s[j + 3] = simde_mm_adds_epi16(y0r_over_s[j >> 3], y0i_one_over_sqrt_21);
      y0_s[j + 4] = simde_mm_subs_epi16(y0r_over_s[j >> 3], y0i_one_over_sqrt_21);
      y0_s[j + 5] = simde_mm_subs_epi16(y0r_over_s[j >> 3], y0i_three_over_sqrt_21);
      y0_s[j + 6] = simde_mm_subs_epi16(y0r_over_s[j >> 3], y0i_five_over_sqrt_21);
      y0_s[j + 7] = simde_mm_subs_epi16(y0r_over_s[j >> 3], y0i_seven_over_sqrt_21);
    }

    ch_mag_int_with_sigma2 = simde_mm_srai_epi16(ch_mag_int, 1); // *2
    two_ch_mag_int_with_sigma2 = ch_mag_int; // *4
    three_ch_mag_int_with_sigma2 = simde_mm_adds_epi16(ch_mag_int_with_sigma2, two_ch_mag_int_with_sigma2); // *6
    simde__m128i a_r_s[64];
    simde__m128i a_i_s[64];
    simde__m128i psi_a_s[64];
    simde__m128i a_sq_s[64];
    for (int j = 0; j < 64; j++) {
      // Detection of interference term
      a_r_s[j] = interference_abs_64qam_epi16(psi_r_s[j],
                                              ch_mag_int_with_sigma2,
                                              two_ch_mag_int_with_sigma2,
                                              three_ch_mag_int_with_sigma2,
                                              ONE_OVER_SQRT_2_42,
                                              THREE_OVER_SQRT_2_42,
                                              FIVE_OVER_SQRT_2_42,
                                              SEVEN_OVER_SQRT_2_42);
      a_i_s[j] = interference_abs_64qam_epi16(psi_i_s[j],
                                              ch_mag_int_with_sigma2,
                                              two_ch_mag_int_with_sigma2,
                                              three_ch_mag_int_with_sigma2,
                                              ONE_OVER_SQRT_2_42,
                                              THREE_OVER_SQRT_2_42,
                                              FIVE_OVER_SQRT_2_42,
                                              SEVEN_OVER_SQRT_2_42);

      // Calculation of a group of two terms in the bit metric involving product of psi and interference
      psi_a_s[j] = prodsum_psi_a_epi16(psi_r_s[j], a_r_s[j], psi_i_s[j], a_i_s[j]);

      // Multiply by sqrt(2)
      psi_a_s[j] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(psi_a_s[j], ONE_OVER_SQRT_2), 2);

      // Calculation of a group of two terms in the bit metric involving squares of interference
      a_sq_s[j] = square_a_64qam_epi16(a_r_s[j], a_i_s[j], ch_mag_int, SQRT_42_OVER_FOUR);
    }

    // Computing different multiples of ||h0||^2
    simde__m128i ch_mag_with_sigma2[10];
    enum ch_mag_over_42with_sigma2_vals { mag2 = 0, mag10, mag26, mag18, mag34, mag58, mag50, mag74, mag98 };
    // x=1, y=1
    ch_mag_with_sigma2[mag2] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(ch_mag_des, ONE_OVER_FOUR_SQRT_42), 1);
    // x=1, y=3
    ch_mag_with_sigma2[mag10] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(ch_mag_des, FIVE_OVER_FOUR_SQRT_42), 1);
    // x=1, x=5
    ch_mag_with_sigma2[mag26] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(ch_mag_des, THIRTEEN_OVER_FOUR_SQRT_42), 1);
    // x=1, y=7
    ch_mag_with_sigma2[mag50] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(ch_mag_des, TWENTYFIVE_OVER_FOUR_SQRT_42), 1);
    // x=3, y=3
    ch_mag_with_sigma2[mag18] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(ch_mag_des, NINE_OVER_FOUR_SQRT_42), 1);
    // x=3, y=5
    ch_mag_with_sigma2[mag34] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(ch_mag_des, SEVENTEEN_OVER_FOUR_SQRT_42), 1);
    // x=3, y=7
    ch_mag_with_sigma2[mag58] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(ch_mag_des, TWENTYNINE_OVER_FOUR_SQRT_42), 2);
    // x=5, y=5
    ch_mag_with_sigma2[mag50] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(ch_mag_des, TWENTYFIVE_OVER_FOUR_SQRT_42), 1);
    // x=5, y=7
    ch_mag_with_sigma2[mag74] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(ch_mag_des, THIRTYSEVEN_OVER_FOUR_SQRT_42), 2);
    // x=7, y=7
    ch_mag_with_sigma2[mag98] = simde_mm_slli_epi16(simde_mm_mulhi_epi16(ch_mag_des, FORTYNINE_OVER_FOUR_SQRT_42), 2);

    // Computing Metrics
    simde__m128i bit_met_s[64];
    const enum ch_mag_over_42with_sigma2_vals table[] = {
        mag98, mag74, mag58, mag50, mag50, mag58, mag74, mag98, mag74, mag50, mag34, mag26, mag26, mag34, mag50, mag74,
        mag58, mag34, mag18, mag10, mag10, mag18, mag34, mag58, mag50, mag26, mag10, mag2,  mag2,  mag10, mag26, mag50};

    for (int i = 0; i < 32; i++) {
      const simde__m128i x = simde_mm_adds_epi16(simde_mm_subs_epi16(psi_a_s[i], a_sq_s[i]), y0_s[i]);
      bit_met_s[i] = simde_mm_subs_epi16(x, ch_mag_with_sigma2[table[i]]);
    }
    for (int i = 0; i < 32; i++) {
      const simde__m128i x = simde_mm_subs_epi16(simde_mm_subs_epi16(psi_a_s[32 + i], a_sq_s[32 + i]), y0_s[31 - i]);
      bit_met_s[32 + i] = simde_mm_subs_epi16(x, ch_mag_with_sigma2[table[31 - i]]);
    }

    // Detection for bits
    simde__m128i logmax_den_re0;
    simde__m128i logmax_num_re0;
    // Detection for 1st bit
    // bit = 1
    xmm0 = max_epi16(bit_met_s[56],
                     bit_met_s[57],
                     bit_met_s[58],
                     bit_met_s[59],
                     bit_met_s[60],
                     bit_met_s[61],
                     bit_met_s[62],
                     bit_met_s[63]);
    xmm1 = max_epi16(bit_met_s[48],
                     bit_met_s[49],
                     bit_met_s[50],
                     bit_met_s[51],
                     bit_met_s[52],
                     bit_met_s[53],
                     bit_met_s[54],
                     bit_met_s[55]);
    xmm2 = max_epi16(bit_met_s[40],
                     bit_met_s[41],
                     bit_met_s[42],
                     bit_met_s[43],
                     bit_met_s[44],
                     bit_met_s[45],
                     bit_met_s[46],
                     bit_met_s[47]);
    xmm3 = max_epi16(bit_met_s[32],
                     bit_met_s[33],
                     bit_met_s[34],
                     bit_met_s[35],
                     bit_met_s[36],
                     bit_met_s[37],
                     bit_met_s[38],
                     bit_met_s[39]);
    logmax_den_re0 = simde_mm_max_epi16(simde_mm_max_epi16(xmm0, xmm1), simde_mm_max_epi16(xmm2, xmm3));

    // bit = 0
    xmm0 =
        max_epi16(bit_met_s[0], bit_met_s[1], bit_met_s[2], bit_met_s[3], bit_met_s[4], bit_met_s[5], bit_met_s[6], bit_met_s[7]);
    xmm1 = max_epi16(bit_met_s[8],
                     bit_met_s[9],
                     bit_met_s[10],
                     bit_met_s[11],
                     bit_met_s[12],
                     bit_met_s[13],
                     bit_met_s[14],
                     bit_met_s[15]);
    xmm2 = max_epi16(bit_met_s[16],
                     bit_met_s[17],
                     bit_met_s[18],
                     bit_met_s[19],
                     bit_met_s[20],
                     bit_met_s[21],
                     bit_met_s[22],
                     bit_met_s[23]);
    xmm3 = max_epi16(bit_met_s[24],
                     bit_met_s[25],
                     bit_met_s[26],
                     bit_met_s[27],
                     bit_met_s[28],
                     bit_met_s[29],
                     bit_met_s[30],
                     bit_met_s[31]);
    logmax_num_re0 = simde_mm_max_epi16(simde_mm_max_epi16(xmm0, xmm1), simde_mm_max_epi16(xmm2, xmm3));

    y0r = simde_mm_subs_epi16(logmax_num_re0, logmax_den_re0);

    // Detection for 2nd bit
    // bit = 1
    xmm0 = max_epi16(bit_met_s[4],
                     bit_met_s[12],
                     bit_met_s[20],
                     bit_met_s[28],
                     bit_met_s[36],
                     bit_met_s[44],
                     bit_met_s[52],
                     bit_met_s[60]);
    xmm1 = max_epi16(bit_met_s[5],
                     bit_met_s[13],
                     bit_met_s[21],
                     bit_met_s[29],
                     bit_met_s[37],
                     bit_met_s[45],
                     bit_met_s[53],
                     bit_met_s[61]);
    xmm2 = max_epi16(bit_met_s[6],
                     bit_met_s[14],
                     bit_met_s[22],
                     bit_met_s[30],
                     bit_met_s[38],
                     bit_met_s[46],
                     bit_met_s[54],
                     bit_met_s[62]);
    xmm3 = max_epi16(bit_met_s[7],
                     bit_met_s[15],
                     bit_met_s[23],
                     bit_met_s[31],
                     bit_met_s[39],
                     bit_met_s[47],
                     bit_met_s[55],
                     bit_met_s[63]);
    logmax_den_re0 = simde_mm_max_epi16(simde_mm_max_epi16(xmm0, xmm1), simde_mm_max_epi16(xmm2, xmm3));

    // bit = 0
    xmm0 = max_epi16(bit_met_s[3],
                     bit_met_s[11],
                     bit_met_s[19],
                     bit_met_s[27],
                     bit_met_s[35],
                     bit_met_s[43],
                     bit_met_s[51],
                     bit_met_s[59]);
    xmm1 = max_epi16(bit_met_s[2],
                     bit_met_s[10],
                     bit_met_s[18],
                     bit_met_s[26],
                     bit_met_s[34],
                     bit_met_s[42],
                     bit_met_s[50],
                     bit_met_s[58]);
    xmm2 = max_epi16(bit_met_s[1],
                     bit_met_s[9],
                     bit_met_s[17],
                     bit_met_s[25],
                     bit_met_s[33],
                     bit_met_s[41],
                     bit_met_s[49],
                     bit_met_s[57]);
    xmm3 = max_epi16(bit_met_s[0],
                     bit_met_s[8],
                     bit_met_s[16],
                     bit_met_s[24],
                     bit_met_s[32],
                     bit_met_s[40],
                     bit_met_s[48],
                     bit_met_s[56]);
    logmax_num_re0 = simde_mm_max_epi16(simde_mm_max_epi16(xmm0, xmm1), simde_mm_max_epi16(xmm2, xmm3));

    y1r = simde_mm_subs_epi16(logmax_num_re0, logmax_den_re0);

    // Detection for 3rd bit
    xmm0 = max_epi16(bit_met_s[63],
                     bit_met_s[62],
                     bit_met_s[61],
                     bit_met_s[60],
                     bit_met_s[59],
                     bit_met_s[58],
                     bit_met_s[57],
                     bit_met_s[56]);
    xmm1 = max_epi16(bit_met_s[55],
                     bit_met_s[54],
                     bit_met_s[53],
                     bit_met_s[52],
                     bit_met_s[51],
                     bit_met_s[50],
                     bit_met_s[49],
                     bit_met_s[48]);
    xmm2 = max_epi16(bit_met_s[15],
                     bit_met_s[14],
                     bit_met_s[13],
                     bit_met_s[12],
                     bit_met_s[11],
                     bit_met_s[10],
                     bit_met_s[9],
                     bit_met_s[8]);
    xmm3 =
        max_epi16(bit_met_s[7], bit_met_s[6], bit_met_s[5], bit_met_s[4], bit_met_s[3], bit_met_s[2], bit_met_s[1], bit_met_s[0]);
    logmax_den_re0 = simde_mm_max_epi16(simde_mm_max_epi16(xmm0, xmm1), simde_mm_max_epi16(xmm2, xmm3));

    xmm0 = max_epi16(bit_met_s[47],
                     bit_met_s[46],
                     bit_met_s[45],
                     bit_met_s[44],
                     bit_met_s[43],
                     bit_met_s[42],
                     bit_met_s[41],
                     bit_met_s[40]);
    xmm1 = max_epi16(bit_met_s[39],
                     bit_met_s[38],
                     bit_met_s[37],
                     bit_met_s[36],
                     bit_met_s[35],
                     bit_met_s[34],
                     bit_met_s[33],
                     bit_met_s[32]);
    xmm2 = max_epi16(bit_met_s[31],
                     bit_met_s[30],
                     bit_met_s[29],
                     bit_met_s[28],
                     bit_met_s[27],
                     bit_met_s[26],
                     bit_met_s[25],
                     bit_met_s[24]);
    xmm3 = max_epi16(bit_met_s[23],
                     bit_met_s[22],
                     bit_met_s[21],
                     bit_met_s[20],
                     bit_met_s[19],
                     bit_met_s[18],
                     bit_met_s[17],
                     bit_met_s[16]);
    logmax_num_re0 = simde_mm_max_epi16(simde_mm_max_epi16(xmm0, xmm1), simde_mm_max_epi16(xmm2, xmm3));

    simde__m128i y2r = simde_mm_subs_epi16(logmax_num_re0, logmax_den_re0);

    // Detection for 4th bit
    xmm0 = max_epi16(bit_met_s[0],
                     bit_met_s[8],
                     bit_met_s[16],
                     bit_met_s[24],
                     bit_met_s[32],
                     bit_met_s[40],
                     bit_met_s[48],
                     bit_met_s[56]);
    xmm1 = max_epi16(bit_met_s[1],
                     bit_met_s[9],
                     bit_met_s[17],
                     bit_met_s[25],
                     bit_met_s[33],
                     bit_met_s[41],
                     bit_met_s[49],
                     bit_met_s[57]);
    xmm2 = max_epi16(bit_met_s[6],
                     bit_met_s[14],
                     bit_met_s[22],
                     bit_met_s[30],
                     bit_met_s[38],
                     bit_met_s[46],
                     bit_met_s[54],
                     bit_met_s[62]);
    xmm3 = max_epi16(bit_met_s[7],
                     bit_met_s[15],
                     bit_met_s[23],
                     bit_met_s[31],
                     bit_met_s[39],
                     bit_met_s[47],
                     bit_met_s[55],
                     bit_met_s[63]);
    logmax_den_re0 = simde_mm_max_epi16(simde_mm_max_epi16(xmm0, xmm1), simde_mm_max_epi16(xmm2, xmm3));

    xmm0 = max_epi16(bit_met_s[4],
                     bit_met_s[12],
                     bit_met_s[20],
                     bit_met_s[28],
                     bit_met_s[36],
                     bit_met_s[44],
                     bit_met_s[52],
                     bit_met_s[60]);
    xmm1 = max_epi16(bit_met_s[5],
                     bit_met_s[13],
                     bit_met_s[21],
                     bit_met_s[29],
                     bit_met_s[37],
                     bit_met_s[45],
                     bit_met_s[53],
                     bit_met_s[61]);
    xmm2 = max_epi16(bit_met_s[3],
                     bit_met_s[11],
                     bit_met_s[19],
                     bit_met_s[27],
                     bit_met_s[35],
                     bit_met_s[43],
                     bit_met_s[51],
                     bit_met_s[59]);
    xmm3 = max_epi16(bit_met_s[2],
                     bit_met_s[10],
                     bit_met_s[18],
                     bit_met_s[26],
                     bit_met_s[34],
                     bit_met_s[42],
                     bit_met_s[50],
                     bit_met_s[58]);
    logmax_num_re0 = simde_mm_max_epi16(simde_mm_max_epi16(xmm0, xmm1), simde_mm_max_epi16(xmm2, xmm3));

    y0i = simde_mm_subs_epi16(logmax_num_re0, logmax_den_re0);

    // Detection for 5th bit
    xmm0 = max_epi16(bit_met_s[63],
                     bit_met_s[62],
                     bit_met_s[61],
                     bit_met_s[60],
                     bit_met_s[59],
                     bit_met_s[58],
                     bit_met_s[57],
                     bit_met_s[56]);
    xmm1 = max_epi16(bit_met_s[39],
                     bit_met_s[38],
                     bit_met_s[37],
                     bit_met_s[36],
                     bit_met_s[35],
                     bit_met_s[34],
                     bit_met_s[33],
                     bit_met_s[32]);
    xmm2 = max_epi16(bit_met_s[31],
                     bit_met_s[30],
                     bit_met_s[29],
                     bit_met_s[28],
                     bit_met_s[27],
                     bit_met_s[26],
                     bit_met_s[25],
                     bit_met_s[24]);
    xmm3 =
        max_epi16(bit_met_s[7], bit_met_s[6], bit_met_s[5], bit_met_s[4], bit_met_s[3], bit_met_s[2], bit_met_s[1], bit_met_s[0]);
    logmax_den_re0 = simde_mm_max_epi16(simde_mm_max_epi16(xmm0, xmm1), simde_mm_max_epi16(xmm2, xmm3));

    xmm0 = max_epi16(bit_met_s[55],
                     bit_met_s[54],
                     bit_met_s[53],
                     bit_met_s[52],
                     bit_met_s[51],
                     bit_met_s[50],
                     bit_met_s[49],
                     bit_met_s[48]);
    xmm1 = max_epi16(bit_met_s[47],
                     bit_met_s[46],
                     bit_met_s[45],
                     bit_met_s[44],
                     bit_met_s[43],
                     bit_met_s[42],
                     bit_met_s[41],
                     bit_met_s[40]);
    xmm2 = max_epi16(bit_met_s[23],
                     bit_met_s[22],
                     bit_met_s[21],
                     bit_met_s[20],
                     bit_met_s[19],
                     bit_met_s[18],
                     bit_met_s[17],
                     bit_met_s[16]);
    xmm3 = max_epi16(bit_met_s[15],
                     bit_met_s[14],
                     bit_met_s[13],
                     bit_met_s[12],
                     bit_met_s[11],
                     bit_met_s[10],
                     bit_met_s[9],
                     bit_met_s[8]);
    logmax_num_re0 = simde_mm_max_epi16(simde_mm_max_epi16(xmm0, xmm1), simde_mm_max_epi16(xmm2, xmm3));

    y1i = simde_mm_subs_epi16(logmax_num_re0, logmax_den_re0);

    // Detection for 6th bit
    xmm0 = max_epi16(bit_met_s[0],
                     bit_met_s[8],
                     bit_met_s[16],
                     bit_met_s[24],
                     bit_met_s[32],
                     bit_met_s[40],
                     bit_met_s[48],
                     bit_met_s[56]);
    xmm1 = max_epi16(bit_met_s[3],
                     bit_met_s[11],
                     bit_met_s[19],
                     bit_met_s[27],
                     bit_met_s[35],
                     bit_met_s[43],
                     bit_met_s[51],
                     bit_met_s[59]);
    xmm2 = max_epi16(bit_met_s[4],
                     bit_met_s[12],
                     bit_met_s[20],
                     bit_met_s[28],
                     bit_met_s[36],
                     bit_met_s[44],
                     bit_met_s[52],
                     bit_met_s[60]);
    xmm3 = max_epi16(bit_met_s[7],
                     bit_met_s[15],
                     bit_met_s[23],
                     bit_met_s[31],
                     bit_met_s[39],
                     bit_met_s[47],
                     bit_met_s[55],
                     bit_met_s[63]);
    logmax_den_re0 = simde_mm_max_epi16(simde_mm_max_epi16(xmm0, xmm1), simde_mm_max_epi16(xmm2, xmm3));

    xmm0 = max_epi16(bit_met_s[6],
                     bit_met_s[14],
                     bit_met_s[22],
                     bit_met_s[30],
                     bit_met_s[38],
                     bit_met_s[46],
                     bit_met_s[54],
                     bit_met_s[62]);
    xmm1 = max_epi16(bit_met_s[5],
                     bit_met_s[13],
                     bit_met_s[21],
                     bit_met_s[29],
                     bit_met_s[37],
                     bit_met_s[45],
                     bit_met_s[53],
                     bit_met_s[61]);
    xmm2 = max_epi16(bit_met_s[2],
                     bit_met_s[10],
                     bit_met_s[18],
                     bit_met_s[26],
                     bit_met_s[34],
                     bit_met_s[42],
                     bit_met_s[50],
                     bit_met_s[58]);
    xmm3 = max_epi16(bit_met_s[1],
                     bit_met_s[9],
                     bit_met_s[17],
                     bit_met_s[25],
                     bit_met_s[33],
                     bit_met_s[41],
                     bit_met_s[49],
                     bit_met_s[57]);
    logmax_num_re0 = simde_mm_max_epi16(simde_mm_max_epi16(xmm0, xmm1), simde_mm_max_epi16(xmm2, xmm3));

    simde__m128i y2i = simde_mm_subs_epi16(logmax_num_re0, logmax_den_re0);

    // Map to output stream, difficult to do in SIMD since we have 6 16bit LLRs
    for (int re = 0; re < 8; re++) {
      *stream0_out++ = ((short *)&y0r)[re];
      *stream0_out++ = ((short *)&y1r)[re];
      *stream0_out++ = ((short *)&y2r)[re];
      *stream0_out++ = ((short *)&y0i)[re];
      *stream0_out++ = ((short *)&y1i)[re];
      *stream0_out++ = ((short *)&y2i)[re];
    }
  }
#else
  simde__m256i *rho01_256i = (simde__m256i *)rho01;
  simde__m256i *stream0_256i_in = (simde__m256i *)stream0_in;
  simde__m256i *stream1_256i_in = (simde__m256i *)stream1_in;
  simde__m256i *ch_mag_256i = (simde__m256i *)ch_mag;
  simde__m256i *ch_mag_256i_i = (simde__m256i *)ch_mag_i;

  simde__m256i ONE_OVER_SQRT_42 = simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(10112)); // round(1/sqrt(42)*2^16)
  simde__m256i THREE_OVER_SQRT_42 = simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(30337)); // round(3/sqrt(42)*2^16)
  simde__m256i FIVE_OVER_SQRT_42 = simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(25281)); // round(5/sqrt(42)*2^15)
  simde__m256i SEVEN_OVER_SQRT_42 = simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(17697)); // round(7/sqrt(42)*2^14) Q2.14
  simde__m256i ONE_OVER_SQRT_2 = simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(23170)); // round(1/sqrt(2)*2^15)
  simde__m256i ONE_OVER_SQRT_2_42 = simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(3575)); // round(1/sqrt(2*42)*2^15)
  simde__m256i THREE_OVER_SQRT_2_42 = simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(10726)); // round(3/sqrt(2*42)*2^15)
  simde__m256i FIVE_OVER_SQRT_2_42 = simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(17876)); // round(5/sqrt(2*42)*2^15)
  simde__m256i SEVEN_OVER_SQRT_2_42 = simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(25027)); // round(7/sqrt(2*42)*2^15)
  simde__m256i FORTYNINE_OVER_FOUR_SQRT_42 =
      simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(30969)); // round(49/(4*sqrt(42))*2^14), Q2.14
  simde__m256i THIRTYSEVEN_OVER_FOUR_SQRT_42 =
      simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(23385)); // round(37/(4*sqrt(42))*2^14), Q2.14
  simde__m256i TWENTYFIVE_OVER_FOUR_SQRT_42 =
      simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(31601)); // round(25/(4*sqrt(42))*2^15)
  simde__m256i TWENTYNINE_OVER_FOUR_SQRT_42 =
      simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(18329)); // round(29/(4*sqrt(42))*2^15), Q2.14
  simde__m256i SEVENTEEN_OVER_FOUR_SQRT_42 =
      simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(21489)); // round(17/(4*sqrt(42))*2^15)
  simde__m256i NINE_OVER_FOUR_SQRT_42 = simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(11376)); // round(9/(4*sqrt(42))*2^15)
  simde__m256i THIRTEEN_OVER_FOUR_SQRT_42 = simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(16433)); // round(13/(4*sqrt(42))*2^15)
  simde__m256i FIVE_OVER_FOUR_SQRT_42 = simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(6320)); // round(5/(4*sqrt(42))*2^15)
  simde__m256i ONE_OVER_FOUR_SQRT_42 = simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(1264)); // round(1/(4*sqrt(42))*2^15)
  simde__m256i SQRT_42_OVER_FOUR = simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(13272)); // round(sqrt(42)/4*2^13), Q3.12

  simde__m256i ch_mag_des;
  simde__m256i ch_mag_int;
  simde__m256i y0r_one_over_sqrt_21;
  simde__m256i y0r_three_over_sqrt_21;
  simde__m256i y0r_five_over_sqrt_21;
  simde__m256i y0r_seven_over_sqrt_21;
  simde__m256i y0i_one_over_sqrt_21;
  simde__m256i y0i_three_over_sqrt_21;
  simde__m256i y0i_five_over_sqrt_21;
  simde__m256i y0i_seven_over_sqrt_21;
  simde__m256i ch_mag_int_with_sigma2;
  simde__m256i two_ch_mag_int_with_sigma2;
  simde__m256i three_ch_mag_int_with_sigma2;

  uint32_t len256 = length >> 3;

  for (int i = 0; i < len256; i += 2) {
    // Get rho
    simde__m256i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8;
    oai_mm256_separate_real_imag_parts(&xmm2, &xmm3, rho01_256i[i], rho01_256i[i + 1]);

    simde__m256i rho_rpi = simde_mm256_adds_epi16(xmm2, xmm3); // rho = Re(rho) + Im(rho)
    simde__m256i rho_rmi = simde_mm256_subs_epi16(xmm2, xmm3); // rho* = Re(rho) - Im(rho)

    // Compute the different rhos
    simde__m256i rho_rs[32];
    rho_rs[27] = simde_mm256_mulhi_epi16(rho_rpi, ONE_OVER_SQRT_42);
    rho_rs[28] = simde_mm256_mulhi_epi16(rho_rmi, ONE_OVER_SQRT_42);
    rho_rs[18] = simde_mm256_mulhi_epi16(rho_rpi, THREE_OVER_SQRT_42);
    rho_rs[21] = simde_mm256_mulhi_epi16(rho_rmi, THREE_OVER_SQRT_42);
    rho_rs[9] = simde_mm256_mulhi_epi16(rho_rpi, FIVE_OVER_SQRT_42);
    rho_rs[14] = simde_mm256_mulhi_epi16(rho_rmi, FIVE_OVER_SQRT_42);
    rho_rs[0] = simde_mm256_mulhi_epi16(rho_rpi, SEVEN_OVER_SQRT_42);
    rho_rs[7] = simde_mm256_mulhi_epi16(rho_rmi, SEVEN_OVER_SQRT_42);

    rho_rs[9] = simde_mm256_slli_epi16(rho_rs[9], 1);
    rho_rs[14] = simde_mm256_slli_epi16(rho_rs[14], 1);
    rho_rs[0] = simde_mm256_slli_epi16(rho_rs[0], 2);
    rho_rs[7] = simde_mm256_slli_epi16(rho_rs[7], 2);

    xmm4 = simde_mm256_mulhi_epi16(xmm2, ONE_OVER_SQRT_42);
    xmm5 = simde_mm256_mulhi_epi16(xmm3, ONE_OVER_SQRT_42);
    xmm6 = simde_mm256_mulhi_epi16(xmm3, THREE_OVER_SQRT_42);
    xmm7 = simde_mm256_mulhi_epi16(xmm3, FIVE_OVER_SQRT_42);
    xmm8 = simde_mm256_mulhi_epi16(xmm3, SEVEN_OVER_SQRT_42);
    xmm7 = simde_mm256_slli_epi16(xmm7, 1);
    xmm8 = simde_mm256_slli_epi16(xmm8, 2);

    rho_rs[26] = simde_mm256_adds_epi16(xmm4, xmm6);
    rho_rs[29] = simde_mm256_subs_epi16(xmm4, xmm6);
    rho_rs[25] = simde_mm256_adds_epi16(xmm4, xmm7);
    rho_rs[30] = simde_mm256_subs_epi16(xmm4, xmm7);
    rho_rs[24] = simde_mm256_adds_epi16(xmm4, xmm8);
    rho_rs[31] = simde_mm256_subs_epi16(xmm4, xmm8);

    xmm4 = simde_mm256_mulhi_epi16(xmm2, THREE_OVER_SQRT_42);
    rho_rs[19] = simde_mm256_adds_epi16(xmm4, xmm5);
    rho_rs[20] = simde_mm256_subs_epi16(xmm4, xmm5);
    rho_rs[17] = simde_mm256_adds_epi16(xmm4, xmm7);
    rho_rs[22] = simde_mm256_subs_epi16(xmm4, xmm7);
    rho_rs[16] = simde_mm256_adds_epi16(xmm4, xmm8);
    rho_rs[23] = simde_mm256_subs_epi16(xmm4, xmm8);

    xmm4 = simde_mm256_mulhi_epi16(xmm2, FIVE_OVER_SQRT_42);
    xmm4 = simde_mm256_slli_epi16(xmm4, 1);
    rho_rs[11] = simde_mm256_adds_epi16(xmm4, xmm5);
    rho_rs[12] = simde_mm256_subs_epi16(xmm4, xmm5);
    rho_rs[10] = simde_mm256_adds_epi16(xmm4, xmm6);
    rho_rs[13] = simde_mm256_subs_epi16(xmm4, xmm6);
    rho_rs[8] = simde_mm256_adds_epi16(xmm4, xmm8);
    rho_rs[15] = simde_mm256_subs_epi16(xmm4, xmm8);

    xmm4 = simde_mm256_mulhi_epi16(xmm2, SEVEN_OVER_SQRT_42);
    xmm4 = simde_mm256_slli_epi16(xmm4, 2);
    rho_rs[3] = simde_mm256_adds_epi16(xmm4, xmm5);
    rho_rs[4] = simde_mm256_subs_epi16(xmm4, xmm5);
    rho_rs[2] = simde_mm256_adds_epi16(xmm4, xmm6);
    rho_rs[5] = simde_mm256_subs_epi16(xmm4, xmm6);
    rho_rs[1] = simde_mm256_adds_epi16(xmm4, xmm7);
    rho_rs[6] = simde_mm256_subs_epi16(xmm4, xmm7);

    // Rearrange interfering MF output
    simde__m256i y1r, y1i;
    oai_mm256_separate_real_imag_parts(&y1r, &y1i, stream1_256i_in[i], stream1_256i_in[i + 1]);

    // Psi_r calculation from rho_rpi or rho_rmi
    xmm0 = simde_mm256_broadcastw_epi16(simde_mm_set1_epi16(0)); // ZERO for abs_pi16
    xmm2 = simde_mm256_subs_epi16(rho_rs[0], y1r);

    simde__m256i psi_r_s[64];
    for (int j = 0; j < 32; j++) // psi_r_s[0~31], rho_rs[0~31]
      psi_r_s[j] = simde_mm256_abs_epi16(simde_mm256_subs_epi16(rho_rs[j], y1r));
    for (int j = 32; j < 64; j++) // psi_r_s[32~64], rho_rs[31~0]
      psi_r_s[j] = simde_mm256_abs_epi16(simde_mm256_adds_epi16(rho_rs[63 - j], y1r));

    // simde__m256i psi_i calculation from rho_rpi or rho_rmi
    simde__m256i psi_i_s[64];
    const uint8_t rho_rs_index[32] = {7, 15, 23, 31, 24, 16, 8,  0, 6, 14, 22, 30, 25, 17, 9,  1,
                                      5, 13, 21, 29, 26, 18, 10, 2, 4, 12, 20, 28, 27, 19, 11, 3};
    for (int k = 0; k < 32; k += 8) { // psi_i_s[0~31]
      for (int j = k; j < k + 4; j++)
        psi_i_s[j] = simde_mm256_abs_epi16(simde_mm256_subs_epi16(rho_rs[rho_rs_index[j]], y1i));
      for (int j = k + 4; j < k + 8; j++)
        psi_i_s[j] = simde_mm256_abs_epi16(simde_mm256_adds_epi16(rho_rs[rho_rs_index[j]], y1i));
    }
    for (int k = 32; k < 64; k += 8) { // psi_i_s[32~64]
      for (int j = k; j < k + 4; j++)
        psi_i_s[j] = simde_mm256_abs_epi16(simde_mm256_subs_epi16(rho_rs[rho_rs_index[63 - j]], y1i));
      for (int j = k + 4; j < k + 8; j++)
        psi_i_s[j] = simde_mm256_abs_epi16(simde_mm256_adds_epi16(rho_rs[rho_rs_index[63 - j]], y1i));
    }

    // Rearrange desired MF output
    simde__m256i y0r, y0i;
    oai_mm256_separate_real_imag_parts(&y0r, &y0i, stream0_256i_in[i], stream0_256i_in[i + 1]);

    // Rearrange desired channel magnitudes
    // [|h|^2(1),|h|^2(1),|h|^2(2),|h|^2(2),...,,|h|^2(7),|h|^2(7)]*(2/sqrt(10))
    // xmm2 is dummy variable that contains the same values as ch_mag_des
    oai_mm256_separate_real_imag_parts(&ch_mag_des, &xmm2, ch_mag_256i[i], ch_mag_256i[i + 1]);

    // Rearrange interfering channel magnitudes
    oai_mm256_separate_real_imag_parts(&ch_mag_int, &xmm2, ch_mag_256i_i[i], ch_mag_256i_i[i + 1]);

    y0r_one_over_sqrt_21 = simde_mm256_mulhi_epi16(y0r, ONE_OVER_SQRT_42);
    y0r_three_over_sqrt_21 = simde_mm256_mulhi_epi16(y0r, THREE_OVER_SQRT_42);
    y0r_five_over_sqrt_21 = simde_mm256_mulhi_epi16(y0r, FIVE_OVER_SQRT_42);
    y0r_five_over_sqrt_21 = simde_mm256_slli_epi16(y0r_five_over_sqrt_21, 1);
    y0r_seven_over_sqrt_21 = simde_mm256_mulhi_epi16(y0r, SEVEN_OVER_SQRT_42);
    y0r_seven_over_sqrt_21 = simde_mm256_slli_epi16(y0r_seven_over_sqrt_21, 2); // Q2.14

    y0i_one_over_sqrt_21 = simde_mm256_mulhi_epi16(y0i, ONE_OVER_SQRT_42);
    y0i_three_over_sqrt_21 = simde_mm256_mulhi_epi16(y0i, THREE_OVER_SQRT_42);
    y0i_five_over_sqrt_21 = simde_mm256_mulhi_epi16(y0i, FIVE_OVER_SQRT_42);
    y0i_five_over_sqrt_21 = simde_mm256_slli_epi16(y0i_five_over_sqrt_21, 1);
    y0i_seven_over_sqrt_21 = simde_mm256_mulhi_epi16(y0i, SEVEN_OVER_SQRT_42);
    y0i_seven_over_sqrt_21 = simde_mm256_slli_epi16(y0i_seven_over_sqrt_21, 2); // Q2.14

    simde__m256i y0_s[64];
    const simde__m256i y0r_over_s[8] = {y0r_seven_over_sqrt_21,
                                        y0r_five_over_sqrt_21,
                                        y0r_three_over_sqrt_21,
                                        y0r_one_over_sqrt_21};
    for (int j = 0; j < 32; j += 8) {
      y0_s[j + 0] = simde_mm256_adds_epi16(y0r_over_s[j >> 3], y0i_seven_over_sqrt_21);
      y0_s[j + 1] = simde_mm256_adds_epi16(y0r_over_s[j >> 3], y0i_five_over_sqrt_21);
      y0_s[j + 2] = simde_mm256_adds_epi16(y0r_over_s[j >> 3], y0i_three_over_sqrt_21);
      y0_s[j + 3] = simde_mm256_adds_epi16(y0r_over_s[j >> 3], y0i_one_over_sqrt_21);
      y0_s[j + 4] = simde_mm256_subs_epi16(y0r_over_s[j >> 3], y0i_one_over_sqrt_21);
      y0_s[j + 5] = simde_mm256_subs_epi16(y0r_over_s[j >> 3], y0i_three_over_sqrt_21);
      y0_s[j + 6] = simde_mm256_subs_epi16(y0r_over_s[j >> 3], y0i_five_over_sqrt_21);
      y0_s[j + 7] = simde_mm256_subs_epi16(y0r_over_s[j >> 3], y0i_seven_over_sqrt_21);
    }

    ch_mag_int_with_sigma2 = simde_mm256_srai_epi16(ch_mag_int, 1); // *2
    two_ch_mag_int_with_sigma2 = ch_mag_int; // *4
    three_ch_mag_int_with_sigma2 = simde_mm256_adds_epi16(ch_mag_int_with_sigma2, two_ch_mag_int_with_sigma2); // *6
    simde__m256i a_r_s[64];
    simde__m256i a_i_s[64];
    simde__m256i psi_a_s[64];
    simde__m256i a_sq_s[64];
    for (int j = 0; j < 64; j++) {
      // Detection of interference term
      a_r_s[j] = interference_abs_64qam_epi16_256(psi_r_s[j],
                                                  ch_mag_int_with_sigma2,
                                                  two_ch_mag_int_with_sigma2,
                                                  three_ch_mag_int_with_sigma2,
                                                  ONE_OVER_SQRT_2_42,
                                                  THREE_OVER_SQRT_2_42,
                                                  FIVE_OVER_SQRT_2_42,
                                                  SEVEN_OVER_SQRT_2_42);
      a_i_s[j] = interference_abs_64qam_epi16_256(psi_i_s[j],
                                                  ch_mag_int_with_sigma2,
                                                  two_ch_mag_int_with_sigma2,
                                                  three_ch_mag_int_with_sigma2,
                                                  ONE_OVER_SQRT_2_42,
                                                  THREE_OVER_SQRT_2_42,
                                                  FIVE_OVER_SQRT_2_42,
                                                  SEVEN_OVER_SQRT_2_42);

      // Calculation of a group of two terms in the bit metric involving product of psi and interference
      psi_a_s[j] = prodsum_psi_a_epi16_256(psi_r_s[j], a_r_s[j], psi_i_s[j], a_i_s[j]);

      // Multiply by sqrt(2)
      psi_a_s[j] = simde_mm256_slli_epi16(simde_mm256_mulhi_epi16(psi_a_s[j], ONE_OVER_SQRT_2), 2);

      // Calculation of a group of two terms in the bit metric involving squares of interference
      a_sq_s[j] = square_a_64qam_epi16_256(a_r_s[j], a_i_s[j], ch_mag_int, SQRT_42_OVER_FOUR);
    }

    // Computing different multiples of ||h0||^2
    simde__m256i ch_mag_with_sigma2[10];
    enum ch_mag_over_42with_sigma2_vals { mag2 = 0, mag10, mag26, mag18, mag34, mag58, mag50, mag74, mag98 };
    // x=1, y=1
    ch_mag_with_sigma2[mag2] = simde_mm256_slli_epi16(simde_mm256_mulhi_epi16(ch_mag_des, ONE_OVER_FOUR_SQRT_42), 1);
    // x=1, y=3
    ch_mag_with_sigma2[mag10] = simde_mm256_slli_epi16(simde_mm256_mulhi_epi16(ch_mag_des, FIVE_OVER_FOUR_SQRT_42), 1);
    // x=1, x=5
    ch_mag_with_sigma2[mag26] = simde_mm256_slli_epi16(simde_mm256_mulhi_epi16(ch_mag_des, THIRTEEN_OVER_FOUR_SQRT_42), 1);
    // x=1, y=7
    ch_mag_with_sigma2[mag50] = simde_mm256_slli_epi16(simde_mm256_mulhi_epi16(ch_mag_des, TWENTYFIVE_OVER_FOUR_SQRT_42), 1);
    // x=3, y=3
    ch_mag_with_sigma2[mag18] = simde_mm256_slli_epi16(simde_mm256_mulhi_epi16(ch_mag_des, NINE_OVER_FOUR_SQRT_42), 1);
    // x=3, y=5
    ch_mag_with_sigma2[mag34] = simde_mm256_slli_epi16(simde_mm256_mulhi_epi16(ch_mag_des, SEVENTEEN_OVER_FOUR_SQRT_42), 1);
    // x=3, y=7
    ch_mag_with_sigma2[mag58] = simde_mm256_slli_epi16(simde_mm256_mulhi_epi16(ch_mag_des, TWENTYNINE_OVER_FOUR_SQRT_42), 2);
    // x=5, y=5
    ch_mag_with_sigma2[mag50] = simde_mm256_slli_epi16(simde_mm256_mulhi_epi16(ch_mag_des, TWENTYFIVE_OVER_FOUR_SQRT_42), 1);
    // x=5, y=7
    ch_mag_with_sigma2[mag74] = simde_mm256_slli_epi16(simde_mm256_mulhi_epi16(ch_mag_des, THIRTYSEVEN_OVER_FOUR_SQRT_42), 2);
    // x=7, y=7
    ch_mag_with_sigma2[mag98] = simde_mm256_slli_epi16(simde_mm256_mulhi_epi16(ch_mag_des, FORTYNINE_OVER_FOUR_SQRT_42), 2);

    // Computing Metrics
    simde__m256i bit_met_s[64];
    const enum ch_mag_over_42with_sigma2_vals table[] = {
        mag98, mag74, mag58, mag50, mag50, mag58, mag74, mag98, mag74, mag50, mag34, mag26, mag26, mag34, mag50, mag74,
        mag58, mag34, mag18, mag10, mag10, mag18, mag34, mag58, mag50, mag26, mag10, mag2,  mag2,  mag10, mag26, mag50};

    for (int i = 0; i < 32; i++) {
      const simde__m256i x = simde_mm256_adds_epi16(simde_mm256_subs_epi16(psi_a_s[i], a_sq_s[i]), y0_s[i]);
      bit_met_s[i] = simde_mm256_subs_epi16(x, ch_mag_with_sigma2[table[i]]);
    }
    for (int i = 0; i < 32; i++) {
      const simde__m256i x = simde_mm256_subs_epi16(simde_mm256_subs_epi16(psi_a_s[32 + i], a_sq_s[32 + i]), y0_s[31 - i]);
      bit_met_s[32 + i] = simde_mm256_subs_epi16(x, ch_mag_with_sigma2[table[31 - i]]);
    }

    // Detection for bits
    simde__m256i logmax_den_re0;
    simde__m256i logmax_num_re0;
    // Detection for 1st bit
    // bit = 1
    xmm0 = max_epi16_256(bit_met_s[56],
                         bit_met_s[57],
                         bit_met_s[58],
                         bit_met_s[59],
                         bit_met_s[60],
                         bit_met_s[61],
                         bit_met_s[62],
                         bit_met_s[63]);
    xmm1 = max_epi16_256(bit_met_s[48],
                         bit_met_s[49],
                         bit_met_s[50],
                         bit_met_s[51],
                         bit_met_s[52],
                         bit_met_s[53],
                         bit_met_s[54],
                         bit_met_s[55]);
    xmm2 = max_epi16_256(bit_met_s[40],
                         bit_met_s[41],
                         bit_met_s[42],
                         bit_met_s[43],
                         bit_met_s[44],
                         bit_met_s[45],
                         bit_met_s[46],
                         bit_met_s[47]);
    xmm3 = max_epi16_256(bit_met_s[32],
                         bit_met_s[33],
                         bit_met_s[34],
                         bit_met_s[35],
                         bit_met_s[36],
                         bit_met_s[37],
                         bit_met_s[38],
                         bit_met_s[39]);
    logmax_den_re0 = simde_mm256_max_epi16(simde_mm256_max_epi16(xmm0, xmm1), simde_mm256_max_epi16(xmm2, xmm3));

    // bit = 0
    xmm0 = max_epi16_256(bit_met_s[0],
                         bit_met_s[1],
                         bit_met_s[2],
                         bit_met_s[3],
                         bit_met_s[4],
                         bit_met_s[5],
                         bit_met_s[6],
                         bit_met_s[7]);
    xmm1 = max_epi16_256(bit_met_s[8],
                         bit_met_s[9],
                         bit_met_s[10],
                         bit_met_s[11],
                         bit_met_s[12],
                         bit_met_s[13],
                         bit_met_s[14],
                         bit_met_s[15]);
    xmm2 = max_epi16_256(bit_met_s[16],
                         bit_met_s[17],
                         bit_met_s[18],
                         bit_met_s[19],
                         bit_met_s[20],
                         bit_met_s[21],
                         bit_met_s[22],
                         bit_met_s[23]);
    xmm3 = max_epi16_256(bit_met_s[24],
                         bit_met_s[25],
                         bit_met_s[26],
                         bit_met_s[27],
                         bit_met_s[28],
                         bit_met_s[29],
                         bit_met_s[30],
                         bit_met_s[31]);
    logmax_num_re0 = simde_mm256_max_epi16(simde_mm256_max_epi16(xmm0, xmm1), simde_mm256_max_epi16(xmm2, xmm3));

    y0r = simde_mm256_subs_epi16(logmax_num_re0, logmax_den_re0);

    // Detection for 2nd bit
    // bit = 1
    xmm0 = max_epi16_256(bit_met_s[4],
                         bit_met_s[12],
                         bit_met_s[20],
                         bit_met_s[28],
                         bit_met_s[36],
                         bit_met_s[44],
                         bit_met_s[52],
                         bit_met_s[60]);
    xmm1 = max_epi16_256(bit_met_s[5],
                         bit_met_s[13],
                         bit_met_s[21],
                         bit_met_s[29],
                         bit_met_s[37],
                         bit_met_s[45],
                         bit_met_s[53],
                         bit_met_s[61]);
    xmm2 = max_epi16_256(bit_met_s[6],
                         bit_met_s[14],
                         bit_met_s[22],
                         bit_met_s[30],
                         bit_met_s[38],
                         bit_met_s[46],
                         bit_met_s[54],
                         bit_met_s[62]);
    xmm3 = max_epi16_256(bit_met_s[7],
                         bit_met_s[15],
                         bit_met_s[23],
                         bit_met_s[31],
                         bit_met_s[39],
                         bit_met_s[47],
                         bit_met_s[55],
                         bit_met_s[63]);
    logmax_den_re0 = simde_mm256_max_epi16(simde_mm256_max_epi16(xmm0, xmm1), simde_mm256_max_epi16(xmm2, xmm3));

    // bit = 0
    xmm0 = max_epi16_256(bit_met_s[3],
                         bit_met_s[11],
                         bit_met_s[19],
                         bit_met_s[27],
                         bit_met_s[35],
                         bit_met_s[43],
                         bit_met_s[51],
                         bit_met_s[59]);
    xmm1 = max_epi16_256(bit_met_s[2],
                         bit_met_s[10],
                         bit_met_s[18],
                         bit_met_s[26],
                         bit_met_s[34],
                         bit_met_s[42],
                         bit_met_s[50],
                         bit_met_s[58]);
    xmm2 = max_epi16_256(bit_met_s[1],
                         bit_met_s[9],
                         bit_met_s[17],
                         bit_met_s[25],
                         bit_met_s[33],
                         bit_met_s[41],
                         bit_met_s[49],
                         bit_met_s[57]);
    xmm3 = max_epi16_256(bit_met_s[0],
                         bit_met_s[8],
                         bit_met_s[16],
                         bit_met_s[24],
                         bit_met_s[32],
                         bit_met_s[40],
                         bit_met_s[48],
                         bit_met_s[56]);
    logmax_num_re0 = simde_mm256_max_epi16(simde_mm256_max_epi16(xmm0, xmm1), simde_mm256_max_epi16(xmm2, xmm3));

    y1r = simde_mm256_subs_epi16(logmax_num_re0, logmax_den_re0);

    // Detection for 3rd bit
    xmm0 = max_epi16_256(bit_met_s[63],
                         bit_met_s[62],
                         bit_met_s[61],
                         bit_met_s[60],
                         bit_met_s[59],
                         bit_met_s[58],
                         bit_met_s[57],
                         bit_met_s[56]);
    xmm1 = max_epi16_256(bit_met_s[55],
                         bit_met_s[54],
                         bit_met_s[53],
                         bit_met_s[52],
                         bit_met_s[51],
                         bit_met_s[50],
                         bit_met_s[49],
                         bit_met_s[48]);
    xmm2 = max_epi16_256(bit_met_s[15],
                         bit_met_s[14],
                         bit_met_s[13],
                         bit_met_s[12],
                         bit_met_s[11],
                         bit_met_s[10],
                         bit_met_s[9],
                         bit_met_s[8]);
    xmm3 = max_epi16_256(bit_met_s[7],
                         bit_met_s[6],
                         bit_met_s[5],
                         bit_met_s[4],
                         bit_met_s[3],
                         bit_met_s[2],
                         bit_met_s[1],
                         bit_met_s[0]);
    logmax_den_re0 = simde_mm256_max_epi16(simde_mm256_max_epi16(xmm0, xmm1), simde_mm256_max_epi16(xmm2, xmm3));

    xmm0 = max_epi16_256(bit_met_s[47],
                         bit_met_s[46],
                         bit_met_s[45],
                         bit_met_s[44],
                         bit_met_s[43],
                         bit_met_s[42],
                         bit_met_s[41],
                         bit_met_s[40]);
    xmm1 = max_epi16_256(bit_met_s[39],
                         bit_met_s[38],
                         bit_met_s[37],
                         bit_met_s[36],
                         bit_met_s[35],
                         bit_met_s[34],
                         bit_met_s[33],
                         bit_met_s[32]);
    xmm2 = max_epi16_256(bit_met_s[31],
                         bit_met_s[30],
                         bit_met_s[29],
                         bit_met_s[28],
                         bit_met_s[27],
                         bit_met_s[26],
                         bit_met_s[25],
                         bit_met_s[24]);
    xmm3 = max_epi16_256(bit_met_s[23],
                         bit_met_s[22],
                         bit_met_s[21],
                         bit_met_s[20],
                         bit_met_s[19],
                         bit_met_s[18],
                         bit_met_s[17],
                         bit_met_s[16]);
    logmax_num_re0 = simde_mm256_max_epi16(simde_mm256_max_epi16(xmm0, xmm1), simde_mm256_max_epi16(xmm2, xmm3));

    simde__m256i y2r = simde_mm256_subs_epi16(logmax_num_re0, logmax_den_re0);

    // Detection for 4th bit
    xmm0 = max_epi16_256(bit_met_s[0],
                         bit_met_s[8],
                         bit_met_s[16],
                         bit_met_s[24],
                         bit_met_s[32],
                         bit_met_s[40],
                         bit_met_s[48],
                         bit_met_s[56]);
    xmm1 = max_epi16_256(bit_met_s[1],
                         bit_met_s[9],
                         bit_met_s[17],
                         bit_met_s[25],
                         bit_met_s[33],
                         bit_met_s[41],
                         bit_met_s[49],
                         bit_met_s[57]);
    xmm2 = max_epi16_256(bit_met_s[6],
                         bit_met_s[14],
                         bit_met_s[22],
                         bit_met_s[30],
                         bit_met_s[38],
                         bit_met_s[46],
                         bit_met_s[54],
                         bit_met_s[62]);
    xmm3 = max_epi16_256(bit_met_s[7],
                         bit_met_s[15],
                         bit_met_s[23],
                         bit_met_s[31],
                         bit_met_s[39],
                         bit_met_s[47],
                         bit_met_s[55],
                         bit_met_s[63]);
    logmax_den_re0 = simde_mm256_max_epi16(simde_mm256_max_epi16(xmm0, xmm1), simde_mm256_max_epi16(xmm2, xmm3));

    xmm0 = max_epi16_256(bit_met_s[4],
                         bit_met_s[12],
                         bit_met_s[20],
                         bit_met_s[28],
                         bit_met_s[36],
                         bit_met_s[44],
                         bit_met_s[52],
                         bit_met_s[60]);
    xmm1 = max_epi16_256(bit_met_s[5],
                         bit_met_s[13],
                         bit_met_s[21],
                         bit_met_s[29],
                         bit_met_s[37],
                         bit_met_s[45],
                         bit_met_s[53],
                         bit_met_s[61]);
    xmm2 = max_epi16_256(bit_met_s[3],
                         bit_met_s[11],
                         bit_met_s[19],
                         bit_met_s[27],
                         bit_met_s[35],
                         bit_met_s[43],
                         bit_met_s[51],
                         bit_met_s[59]);
    xmm3 = max_epi16_256(bit_met_s[2],
                         bit_met_s[10],
                         bit_met_s[18],
                         bit_met_s[26],
                         bit_met_s[34],
                         bit_met_s[42],
                         bit_met_s[50],
                         bit_met_s[58]);
    logmax_num_re0 = simde_mm256_max_epi16(simde_mm256_max_epi16(xmm0, xmm1), simde_mm256_max_epi16(xmm2, xmm3));

    y0i = simde_mm256_subs_epi16(logmax_num_re0, logmax_den_re0);

    // Detection for 5th bit
    xmm0 = max_epi16_256(bit_met_s[63],
                         bit_met_s[62],
                         bit_met_s[61],
                         bit_met_s[60],
                         bit_met_s[59],
                         bit_met_s[58],
                         bit_met_s[57],
                         bit_met_s[56]);
    xmm1 = max_epi16_256(bit_met_s[39],
                         bit_met_s[38],
                         bit_met_s[37],
                         bit_met_s[36],
                         bit_met_s[35],
                         bit_met_s[34],
                         bit_met_s[33],
                         bit_met_s[32]);
    xmm2 = max_epi16_256(bit_met_s[31],
                         bit_met_s[30],
                         bit_met_s[29],
                         bit_met_s[28],
                         bit_met_s[27],
                         bit_met_s[26],
                         bit_met_s[25],
                         bit_met_s[24]);
    xmm3 = max_epi16_256(bit_met_s[7],
                         bit_met_s[6],
                         bit_met_s[5],
                         bit_met_s[4],
                         bit_met_s[3],
                         bit_met_s[2],
                         bit_met_s[1],
                         bit_met_s[0]);
    logmax_den_re0 = simde_mm256_max_epi16(simde_mm256_max_epi16(xmm0, xmm1), simde_mm256_max_epi16(xmm2, xmm3));

    xmm0 = max_epi16_256(bit_met_s[55],
                         bit_met_s[54],
                         bit_met_s[53],
                         bit_met_s[52],
                         bit_met_s[51],
                         bit_met_s[50],
                         bit_met_s[49],
                         bit_met_s[48]);
    xmm1 = max_epi16_256(bit_met_s[47],
                         bit_met_s[46],
                         bit_met_s[45],
                         bit_met_s[44],
                         bit_met_s[43],
                         bit_met_s[42],
                         bit_met_s[41],
                         bit_met_s[40]);
    xmm2 = max_epi16_256(bit_met_s[23],
                         bit_met_s[22],
                         bit_met_s[21],
                         bit_met_s[20],
                         bit_met_s[19],
                         bit_met_s[18],
                         bit_met_s[17],
                         bit_met_s[16]);
    xmm3 = max_epi16_256(bit_met_s[15],
                         bit_met_s[14],
                         bit_met_s[13],
                         bit_met_s[12],
                         bit_met_s[11],
                         bit_met_s[10],
                         bit_met_s[9],
                         bit_met_s[8]);
    logmax_num_re0 = simde_mm256_max_epi16(simde_mm256_max_epi16(xmm0, xmm1), simde_mm256_max_epi16(xmm2, xmm3));

    y1i = simde_mm256_subs_epi16(logmax_num_re0, logmax_den_re0);

    // Detection for 6th bit
    xmm0 = max_epi16_256(bit_met_s[0],
                         bit_met_s[8],
                         bit_met_s[16],
                         bit_met_s[24],
                         bit_met_s[32],
                         bit_met_s[40],
                         bit_met_s[48],
                         bit_met_s[56]);
    xmm1 = max_epi16_256(bit_met_s[3],
                         bit_met_s[11],
                         bit_met_s[19],
                         bit_met_s[27],
                         bit_met_s[35],
                         bit_met_s[43],
                         bit_met_s[51],
                         bit_met_s[59]);
    xmm2 = max_epi16_256(bit_met_s[4],
                         bit_met_s[12],
                         bit_met_s[20],
                         bit_met_s[28],
                         bit_met_s[36],
                         bit_met_s[44],
                         bit_met_s[52],
                         bit_met_s[60]);
    xmm3 = max_epi16_256(bit_met_s[7],
                         bit_met_s[15],
                         bit_met_s[23],
                         bit_met_s[31],
                         bit_met_s[39],
                         bit_met_s[47],
                         bit_met_s[55],
                         bit_met_s[63]);
    logmax_den_re0 = simde_mm256_max_epi16(simde_mm256_max_epi16(xmm0, xmm1), simde_mm256_max_epi16(xmm2, xmm3));

    xmm0 = max_epi16_256(bit_met_s[6],
                         bit_met_s[14],
                         bit_met_s[22],
                         bit_met_s[30],
                         bit_met_s[38],
                         bit_met_s[46],
                         bit_met_s[54],
                         bit_met_s[62]);
    xmm1 = max_epi16_256(bit_met_s[5],
                         bit_met_s[13],
                         bit_met_s[21],
                         bit_met_s[29],
                         bit_met_s[37],
                         bit_met_s[45],
                         bit_met_s[53],
                         bit_met_s[61]);
    xmm2 = max_epi16_256(bit_met_s[2],
                         bit_met_s[10],
                         bit_met_s[18],
                         bit_met_s[26],
                         bit_met_s[34],
                         bit_met_s[42],
                         bit_met_s[50],
                         bit_met_s[58]);
    xmm3 = max_epi16_256(bit_met_s[1],
                         bit_met_s[9],
                         bit_met_s[17],
                         bit_met_s[25],
                         bit_met_s[33],
                         bit_met_s[41],
                         bit_met_s[49],
                         bit_met_s[57]);
    logmax_num_re0 = simde_mm256_max_epi16(simde_mm256_max_epi16(xmm0, xmm1), simde_mm256_max_epi16(xmm2, xmm3));

    simde__m256i y2i = simde_mm256_subs_epi16(logmax_num_re0, logmax_den_re0);

    // Map to output stream, difficult to do in SIMD since we have 6 16bit LLRs
    for (int re = 0; re < 16; re++) {
      *stream0_out++ = ((short *)&y0r)[re];
      *stream0_out++ = ((short *)&y1r)[re];
      *stream0_out++ = ((short *)&y2r)[re];
      *stream0_out++ = ((short *)&y0i)[re];
      *stream0_out++ = ((short *)&y1i)[re];
      *stream0_out++ = ((short *)&y2i)[re];
    }
  }
#endif
}

static void nr_ml_llr_shift(int16_t *llr_layer0, int16_t *llr_layer1, uint32_t nb_re, int shift)
{
  simde__m128i *llr_layers0 = (simde__m128i *)llr_layer0;
  simde__m128i *llr_layers1 = (simde__m128i *)llr_layer1;

  uint8_t mem_offset = ((16 - ((long)llr_layers0)) & 0xF) >> 2;

  if (mem_offset > 0) {
    c16_t *llr_layers0_c16 = (c16_t *)llr_layer0;
    c16_t *llr_layers1_c16 = (c16_t *)llr_layer1;
    for (int i = 0; i < mem_offset; i++) {
      llr_layers0_c16[i] = c16Shift(llr_layers0_c16[i], shift);
      llr_layers1_c16[i] = c16Shift(llr_layers1_c16[i], shift);
    }
    llr_layers0 = (simde__m128i *)&llr_layer0[mem_offset * 2];
    llr_layers1 = (simde__m128i *)&llr_layer1[mem_offset * 2];
  }

  for (int i = 0; i < nb_re >> 2; i++) {
    llr_layers0[i] = simde_mm_srai_epi16(llr_layers0[i], shift);
    llr_layers1[i] = simde_mm_srai_epi16(llr_layers1[i], shift);
  }
}

// ============================================================================
// L-best (reduced-search) reference kernel for 2-layer 64QAM max-log LLR.
//
// PHASE 1 reference: floating-point scalar, correctness over speed. See
// openair1/PHY/nr_phy_common/src/mimo_llr_continuous_approx.md §9 for the plan.
//
// Pipeline (per RE, one target layer):
//   1. inline ZF/MMSE seed  x_hat1 = (R+lambda I)^{-1} z |_1   from (R,z)
//   2. candidate set C1 = the L 64QAM points nearest x_hat1
//   3. for each candidate, the nuisance layer is the exact conditional slice
//      x2*(x1) = Q_S( (z2 - conj(rho)*x1) / P1 )   (O(1), == conditional ML)
//   4. exact max-log metric over C1, per-bit LLR = max_{bit=0} - max_{bit=1}
//   L == 64 evaluates the whole constellation => exact max-log == the reference
//   the SIMD nr_qam64_llr_2layer approximates (validation anchor).
//
// Input buffers mirror nr_qam64_llr_2layer (same scale, drop-in):
//   stream0_in = z1 = H0^H y (desired MRC)     stream1_in = z2 (nuisance MRC)
//   ch_mag     = ||h0||^2 * (4/sqrt(42))        ch_mag_i  = ||h1||^2 * (4/sqrt(42))
//   rho01      = rho = h0^H h1                   (so rho21 = conj(rho))
// Output per RE: { y0r,y1r,y2r, y0i,y1i,y2i } (3 I-bits then 3 Q-bits), same
// order/scale/sign (positive LLR = bit 0) as nr_qam64_llr_2layer.
//
// The metric is computed in Re{x1_n* z1} units (x1_n = normalized constellation
// point), which is exactly the scale of the existing kernel's bit metric, so the
// LLR output needs no rescale (NR_LBEST_LLR_SCALE == 1.0). Energy terms use the
// true 0.5*P*|x|^2 max-log coefficient.
// ============================================================================
#define NR_LBEST_SQRT42       6.48074069840786f   // sqrt(42)
#define NR_LBEST_NA           0.15430334996209f    // 1/sqrt(42), unit-energy 64QAM step
#define NR_LBEST_LLR_SCALE    1.0f                 // matches nr_qam64_llr_2layer metric scale
#define NR_LBEST_LLR_SAT      8192.0f              // fallback magnitude when a bit-subset is empty

// nearest odd level in {-7,-5,-3,-1,1,3,5,7} to v (the 8-PAM slicer)
static inline int nr_lbest_slice_pam8(float v)
{
  static const int levels[8] = {-7, -5, -3, -1, 1, 3, 5, 7};
  int best = levels[0];
  float bestd = (v - levels[0]) * (v - levels[0]);
  for (int k = 1; k < 8; k++) {
    float d = (v - levels[k]) * (v - levels[k]);
    if (d < bestd) {
      bestd = d;
      best = levels[k];
    }
  }
  return best;
}

// 64QAM Gray bits for one axis level (per nr_gen_mod_table.c):
//   bit0 = sign (level<0),  bit1 = |level|>4,  bit2 = (|level|==1 || |level|==7)
static inline void nr_lbest_axis_bits(int level, int *b0, int *b1, int *b2)
{
  int a = level < 0 ? -level : level;
  *b0 = level < 0 ? 1 : 0;
  *b1 = a > 4 ? 1 : 0;
  *b2 = (a == 1 || a == 7) ? 1 : 0;
}

// One target layer of 2-layer 64QAM L-best LLR (scalar float reference).
//   seed_lambda: 0.0f = ZF seed, = noise_var (float) = MMSE-regularised seed
//   L:           candidate-set size (1..64); 64 == full search == max-log reference
static void nr_qam64_llr_2layer_lbest_layer(const c16_t *stream0_in,
                                            const c16_t *stream1_in,
                                            const c16_t *ch_mag,
                                            const c16_t *ch_mag_i,
                                            int16_t *stream0_out,
                                            const c16_t *rho01,
                                            uint32_t length,
                                            int L,
                                            float seed_lambda)
{
  if (L < 1)
    L = 1;
  if (L > 64)
    L = 64;
  static const int levels[8] = {-7, -5, -3, -1, 1, 3, 5, 7};

  for (uint32_t re = 0; re < length; re++) {
    const float z1r = stream0_in[re].r, z1i = stream0_in[re].i;
    const float z2r = stream1_in[re].r, z2i = stream1_in[re].i;
    const float rr = rho01[re].r, ri = rho01[re].i; // rho = h0^H h1
    // recover raw channel powers ||h||^2 from the QAM64_n1-scaled magnitudes
    const float P0 = (float)ch_mag[re].r * (NR_LBEST_SQRT42 / 4.0f);
    const float P1 = (float)ch_mag_i[re].r * (NR_LBEST_SQRT42 / 4.0f);

    // ---- 1. inline ZF/MMSE seed: x_hat1 = ((P1+l) z1 - rho z2) / det ----
    const float l = seed_lambda;
    const float det = (P0 + l) * (P1 + l) - (rr * rr + ri * ri);
    const float invdet = det != 0.0f ? 1.0f / det : 0.0f;
    // rho * z2 (complex)
    const float rz2r = rr * z2r - ri * z2i;
    const float rz2i = rr * z2i + ri * z2r;
    const float xh1r = ((P1 + l) * z1r - rz2r) * invdet; // ~ I-level / sqrt(42)
    const float xh1i = ((P1 + l) * z1i - rz2i) * invdet;
    const float estI = xh1r * NR_LBEST_SQRT42; // soft I level estimate (~ +-1..7)
    const float estQ = xh1i * NR_LBEST_SQRT42;

    // ---- 2. candidate set C1 = L nearest constellation points to x_hat1 ----
    // (reference: rank all 64 by Euclidean distance; production = per-axis slice)
    int candI[64], candQ[64];
    float cand_d[64];
    int nc = 0;
    for (int qi = 0; qi < 8; qi++) {
      for (int ii = 0; ii < 8; ii++) {
        float dI = estI - levels[ii];
        float dQ = estQ - levels[qi];
        candI[nc] = levels[ii];
        candQ[nc] = levels[qi];
        cand_d[nc] = dI * dI + dQ * dQ;
        nc++;
      }
    }
    // partial selection of the L smallest distances to the front
    for (int a = 0; a < L; a++) {
      int m = a;
      for (int b = a + 1; b < 64; b++)
        if (cand_d[b] < cand_d[m])
          m = b;
      if (m != a) {
        float td = cand_d[a]; cand_d[a] = cand_d[m]; cand_d[m] = td;
        int ti = candI[a]; candI[a] = candI[m]; candI[m] = ti;
        int tq = candQ[a]; candQ[a] = candQ[m]; candQ[m] = tq;
      }
    }

    // ---- 3+4. metric over candidates, per-bit max-log reduction ----
    float max0[6] = {-1e30f, -1e30f, -1e30f, -1e30f, -1e30f, -1e30f};
    float max1[6] = {-1e30f, -1e30f, -1e30f, -1e30f, -1e30f, -1e30f};

    for (int c = 0; c < L; c++) {
      const int I1 = candI[c], Q1 = candQ[c];
      const float x1r = I1 * NR_LBEST_NA, x1i = Q1 * NR_LBEST_NA; // normalized x1

      // desired correlation Re{x1_n* z1} and energy 0.5 P0 |x1_n|^2
      float metric = (x1r * z1r + x1i * z1i) - 0.5f * P0 * (x1r * x1r + x1i * x1i);

      // conditional ML nuisance: x2* = Q_S( (z2 - conj(rho) x1_n) / P1 )
      // conj(rho)*x1_n = (rr - j ri)(x1r + j x1i)
      const float cr = rr * x1r + ri * x1i;       // Re{conj(rho) x1_n}
      const float ci = rr * x1i - ri * x1r;       // Im{conj(rho) x1_n}
      const float invP1 = P1 != 0.0f ? 1.0f / P1 : 0.0f;
      const float x2optr = (z2r - cr) * invP1;    // ~ I2-level / sqrt(42)
      const float x2opti = (z2i - ci) * invP1;
      const int I2 = nr_lbest_slice_pam8(x2optr * NR_LBEST_SQRT42);
      const int Q2 = nr_lbest_slice_pam8(x2opti * NR_LBEST_SQRT42);
      const float x2r = I2 * NR_LBEST_NA, x2i = Q2 * NR_LBEST_NA;

      // add nuisance correlation - energy - cross term
      metric += (x2r * z2r + x2i * z2i) - 0.5f * P1 * (x2r * x2r + x2i * x2i);
      // cross = Re{rho * conj(x1_n) * x2_n}
      const float cxr = x1r * x2r + x1i * x2i; // Re{conj(x1_n) x2_n}
      const float cxi = x1r * x2i - x1i * x2r; // Im{conj(x1_n) x2_n}
      metric -= (rr * cxr - ri * cxi);

      // bit labels of this candidate. OAI 64QAM LLR layout interleaves I/Q per
      // magnitude level: {I0,Q0,I1,Q1,I2,Q2} (matches production SIMD
      // nr_qam64_llr_2layer output order), NOT grouped {I0,I1,I2,Q0,Q1,Q2}.
      int bI0, bI1, bI2, bQ0, bQ1, bQ2;
      nr_lbest_axis_bits(I1, &bI0, &bI1, &bI2);
      nr_lbest_axis_bits(Q1, &bQ0, &bQ1, &bQ2);
      const int bit[6] = {bI0, bQ0, bI1, bQ1, bI2, bQ2};
      for (int p = 0; p < 6; p++) {
        if (bit[p] == 0) {
          if (metric > max0[p]) max0[p] = metric;
        } else {
          if (metric > max1[p]) max1[p] = metric;
        }
      }
    }

    // emit LLR = max_{bit=0} - max_{bit=1} (positive => bit 0). Empty subset =>
    // saturate toward the bit value that IS present.
    for (int p = 0; p < 6; p++) {
      float llr;
      if (max0[p] <= -1e29f)
        llr = -NR_LBEST_LLR_SAT;      // no bit-0 candidate => strongly bit 1
      else if (max1[p] <= -1e29f)
        llr = NR_LBEST_LLR_SAT;       // no bit-1 candidate => strongly bit 0
      else
        llr = (max0[p] - max1[p]) * NR_LBEST_LLR_SCALE;
      if (llr > 32767.0f) llr = 32767.0f;
      if (llr < -32768.0f) llr = -32768.0f;
      *stream0_out++ = (int16_t)llr;
    }
  }
}

// Drop-in float reference for nr_qam64_llr_2layer (target layer 0): reduced
// "L-best" search seeded from the linear (ZF/MMSE) estimate. L==64 reproduces
// the full max-log search; L==8 matches it in BLER at a fraction of the cost
// (validated via nr_dlsim -E, 2-layer 64QAM). Scalar float reference pending a
// SIMD port before it can replace the production kernel.
void nr_qam64_llr_2layer_lbest(c16_t *stream0_in,
                               c16_t *stream1_in,
                               c16_t *ch_mag,
                               c16_t *ch_mag_i,
                               int16_t *stream0_out,
                               c16_t *rho01,
                               uint32_t length,
                               int L,
                               float seed_lambda)
{
  nr_qam64_llr_2layer_lbest_layer(stream0_in, stream1_in, ch_mag, ch_mag_i, stream0_out, rho01, length, L, seed_lambda);
}

// ============================================================================
// Float reference L-best kernel for 2-layer 256QAM (one target layer). Analysis
// vehicle: L==256 reproduces the full 256-point ML search (no SIMD full-search
// 256QAM kernel exists); smaller L is the reduced search. Same algorithm/scaling
// as the 64QAM reference, with PAM-16 levels, 1/sqrt(170) normalization, 8 bits.
// ============================================================================
#define NR_LBEST_SQRT170      13.03840481040530f  // sqrt(170)
#define NR_LBEST_NA256        0.07669649888473f    // 1/sqrt(170), unit-energy 256QAM step

// nearest 256QAM PAM-16 level to v (in level units), O(1) round-to-nearest-odd.
// = 2*floor(v/2)+1, clamped to [-15,15]. The +16 offset makes the argument
// non-negative so (int) truncation equals floor (no <math.h> dependency).
static inline int nr_lbest_slice_pam16(float v)
{
  int o = 2 * (int)((v + 16.0f) * 0.5f) - 15;
  if (o < -15) o = -15; else if (o > 15) o = 15;
  return o;
}

// 256QAM Gray bits for one axis level (per nr_gen_mod_table.c 256QAM map):
//   b0 = sign(level<0); m=|level|: b1=(m>8), b2=(|m-8|>4), b3=(||m-8|-4|>2)
static inline void nr_lbest_axis_bits256(int level, int *b0, int *b1, int *b2, int *b3)
{
  const int m = level < 0 ? -level : level;
  int m2 = m - 8; if (m2 < 0) m2 = -m2;
  int m3 = m2 - 4; if (m3 < 0) m3 = -m3;
  *b0 = level < 0 ? 1 : 0;
  *b1 = m > 8 ? 1 : 0;
  *b2 = m2 > 4 ? 1 : 0;
  *b3 = m3 > 2 ? 1 : 0;
}

// candidate (constellation point + squared distance to the seed) for the L-nearest sort
// (members iL/qL, not I/Q: <complex.h> defines I as the imaginary unit)
struct nr_lbest_cand { float d; int16_t iL, qL; };
static int nr_lbest_cand_cmp(const void *a, const void *b)
{
  const float da = ((const struct nr_lbest_cand *)a)->d, db = ((const struct nr_lbest_cand *)b)->d;
  return (da > db) - (da < db);
}

void nr_qam256_llr_2layer_lbest(c16_t *stream0_in,
                                c16_t *stream1_in,
                                c16_t *ch_mag,
                                c16_t *ch_mag_i,
                                int16_t *stream0_out,
                                c16_t *rho01,
                                uint32_t length,
                                int L,
                                float seed_lambda)
{
  if (L < 1)
    L = 1;
  if (L > 256)
    L = 256;
  static const int levels[16] = {-15, -13, -11, -9, -7, -5, -3, -1, 1, 3, 5, 7, 9, 11, 13, 15};

  for (uint32_t re = 0; re < length; re++) {
    const float z1r = stream0_in[re].r, z1i = stream0_in[re].i;
    const float z2r = stream1_in[re].r, z2i = stream1_in[re].i;
    const float rr = rho01[re].r, ri = rho01[re].i; // rho = h0^H h1
    // recover ||h||^2 from the QAM256_n1 (= 8/sqrt(170)) scaled magnitudes
    const float P0 = (float)ch_mag[re].r * (NR_LBEST_SQRT170 / 8.0f);
    const float P1 = (float)ch_mag_i[re].r * (NR_LBEST_SQRT170 / 8.0f);

    // ---- ZF/MMSE seed: x_hat1 = ((P1+l) z1 - rho z2) / det ----
    const float l = seed_lambda;
    const float det = (P0 + l) * (P1 + l) - (rr * rr + ri * ri);
    const float invdet = det != 0.0f ? 1.0f / det : 0.0f;
    const float rz2r = rr * z2r - ri * z2i;
    const float rz2i = rr * z2i + ri * z2r;
    const float estI = ((P1 + l) * z1r - rz2r) * invdet * NR_LBEST_SQRT170; // ~ +-1..15
    const float estQ = ((P1 + l) * z1i - rz2i) * invdet * NR_LBEST_SQRT170;

    // ---- candidate set C1 = L nearest of the 256 constellation points ----
    // O(256 log 256) sort by distance, take the first L (L==256 == full ML, no sort needed).
    struct nr_lbest_cand cand[256];
    int nc = 0;
    for (int qi = 0; qi < 16; qi++) {
      for (int ii = 0; ii < 16; ii++) {
        const float dI = estI - levels[ii];
        const float dQ = estQ - levels[qi];
        cand[nc].d = dI * dI + dQ * dQ;
        cand[nc].iL = (int16_t)levels[ii];
        cand[nc].qL = (int16_t)levels[qi];
        nc++;
      }
    }
    if (L < 256)
      qsort(cand, 256, sizeof(cand[0]), nr_lbest_cand_cmp);

    // ---- metric over candidates, per-bit max-log reduction (8 bits) ----
    float max0[8], max1[8];
    for (int p = 0; p < 8; p++) { max0[p] = -1e30f; max1[p] = -1e30f; }

    for (int c = 0; c < L; c++) {
      const int I1 = cand[c].iL, Q1 = cand[c].qL;
      const float x1r = I1 * NR_LBEST_NA256, x1i = Q1 * NR_LBEST_NA256;
      float metric = (x1r * z1r + x1i * z1i) - 0.5f * P0 * (x1r * x1r + x1i * x1i);

      // conditional ML nuisance x2* = Q_S((z2 - conj(rho) x1)/P1)
      const float cr = rr * x1r + ri * x1i;
      const float ci = rr * x1i - ri * x1r;
      const float invP1 = P1 != 0.0f ? 1.0f / P1 : 0.0f;
      const int I2 = nr_lbest_slice_pam16((z2r - cr) * invP1 * NR_LBEST_SQRT170);
      const int Q2 = nr_lbest_slice_pam16((z2i - ci) * invP1 * NR_LBEST_SQRT170);
      const float x2r = I2 * NR_LBEST_NA256, x2i = Q2 * NR_LBEST_NA256;

      metric += (x2r * z2r + x2i * z2i) - 0.5f * P1 * (x2r * x2r + x2i * x2i);
      const float cxr = x1r * x2r + x1i * x2i;
      const float cxi = x1r * x2i - x1i * x2r;
      metric -= (rr * cxr - ri * cxi);

      // interleaved bit order {I0,Q0,I1,Q1,I2,Q2,I3,Q3}
      int bI0, bI1, bI2, bI3, bQ0, bQ1, bQ2, bQ3;
      nr_lbest_axis_bits256(I1, &bI0, &bI1, &bI2, &bI3);
      nr_lbest_axis_bits256(Q1, &bQ0, &bQ1, &bQ2, &bQ3);
      const int bit[8] = {bI0, bQ0, bI1, bQ1, bI2, bQ2, bI3, bQ3};
      for (int p = 0; p < 8; p++) {
        if (bit[p] == 0) { if (metric > max0[p]) max0[p] = metric; }
        else             { if (metric > max1[p]) max1[p] = metric; }
      }
    }

    for (int p = 0; p < 8; p++) {
      float llr;
      if (max0[p] <= -1e29f)
        llr = -NR_LBEST_LLR_SAT;
      else if (max1[p] <= -1e29f)
        llr = NR_LBEST_LLR_SAT;
      else
        llr = (max0[p] - max1[p]) * NR_LBEST_LLR_SCALE;
      if (llr > 32767.0f) llr = 32767.0f;
      if (llr < -32768.0f) llr = -32768.0f;
      *stream0_out++ = (int16_t)llr;
    }
  }
}

// ============================================================================
// Float reference L-best kernel for 2-layer 16QAM (one target layer). Mainly a
// BUILDING BLOCK for the >2-layer hybrid (where 16QAM won't be done by exhaustive
// search): the deflated 2-layer sub-problem can be any modulation. L==16 == full
// ML. PAM-4 levels, 1/sqrt(10) norm, 4 bits {I0,Q0,I1,Q1}.
// ============================================================================
#define NR_LBEST_SQRT10       3.16227766016838f   // sqrt(10)
#define NR_LBEST_NA16         0.31622776601684f    // 1/sqrt(10), unit-energy 16QAM step

// nearest 16QAM PAM-4 level to v (level units), O(1) round-to-nearest-odd in [-3,3]
static inline int nr_lbest_slice_pam4(float v)
{
  int o = 2 * (int)((v + 4.0f) * 0.5f) - 3;
  if (o < -3) o = -3; else if (o > 3) o = 3;
  return o;
}

// 16QAM Gray bits for one axis level: b0 = sign(level<0), b1 = (|level|>2)
static inline void nr_lbest_axis_bits16(int level, int *b0, int *b1)
{
  *b0 = level < 0 ? 1 : 0;
  *b1 = (level < 0 ? -level : level) > 2 ? 1 : 0;
}

void nr_qam16_llr_2layer_lbest(c16_t *stream0_in,
                               c16_t *stream1_in,
                               c16_t *ch_mag,
                               c16_t *ch_mag_i,
                               int16_t *stream0_out,
                               c16_t *rho01,
                               uint32_t length,
                               int L,
                               float seed_lambda)
{
  if (L < 1)
    L = 1;
  if (L > 16)
    L = 16;
  static const int levels[4] = {-3, -1, 1, 3};

  for (uint32_t re = 0; re < length; re++) {
    const float z1r = stream0_in[re].r, z1i = stream0_in[re].i;
    const float z2r = stream1_in[re].r, z2i = stream1_in[re].i;
    const float rr = rho01[re].r, ri = rho01[re].i; // rho = h0^H h1
    // recover ||h||^2 from the QAM16_n1 (= 2/sqrt(10)) scaled magnitudes
    const float P0 = (float)ch_mag[re].r * (NR_LBEST_SQRT10 / 2.0f);
    const float P1 = (float)ch_mag_i[re].r * (NR_LBEST_SQRT10 / 2.0f);

    // ---- ZF/MMSE seed: x_hat1 = ((P1+l) z1 - rho z2) / det ----
    const float l = seed_lambda;
    const float det = (P0 + l) * (P1 + l) - (rr * rr + ri * ri);
    const float invdet = det != 0.0f ? 1.0f / det : 0.0f;
    const float rz2r = rr * z2r - ri * z2i;
    const float rz2i = rr * z2i + ri * z2r;
    const float estI = ((P1 + l) * z1r - rz2r) * invdet * NR_LBEST_SQRT10; // ~ +-1,3
    const float estQ = ((P1 + l) * z1i - rz2i) * invdet * NR_LBEST_SQRT10;

    // ---- candidate set = L nearest of the 16 points (sort, take first L) ----
    struct nr_lbest_cand cand[16];
    int nc = 0;
    for (int qi = 0; qi < 4; qi++) {
      for (int ii = 0; ii < 4; ii++) {
        const float dI = estI - levels[ii];
        const float dQ = estQ - levels[qi];
        cand[nc].d = dI * dI + dQ * dQ;
        cand[nc].iL = (int16_t)levels[ii];
        cand[nc].qL = (int16_t)levels[qi];
        nc++;
      }
    }
    if (L < 16)
      qsort(cand, 16, sizeof(cand[0]), nr_lbest_cand_cmp);

    // ---- metric over candidates, per-bit max-log reduction (4 bits) ----
    float max0[4], max1[4];
    for (int p = 0; p < 4; p++) { max0[p] = -1e30f; max1[p] = -1e30f; }

    for (int c = 0; c < L; c++) {
      const int I1 = cand[c].iL, Q1 = cand[c].qL;
      const float x1r = I1 * NR_LBEST_NA16, x1i = Q1 * NR_LBEST_NA16;
      float metric = (x1r * z1r + x1i * z1i) - 0.5f * P0 * (x1r * x1r + x1i * x1i);

      const float cr = rr * x1r + ri * x1i;
      const float ci = rr * x1i - ri * x1r;
      const float invP1 = P1 != 0.0f ? 1.0f / P1 : 0.0f;
      const int I2 = nr_lbest_slice_pam4((z2r - cr) * invP1 * NR_LBEST_SQRT10);
      const int Q2 = nr_lbest_slice_pam4((z2i - ci) * invP1 * NR_LBEST_SQRT10);
      const float x2r = I2 * NR_LBEST_NA16, x2i = Q2 * NR_LBEST_NA16;

      metric += (x2r * z2r + x2i * z2i) - 0.5f * P1 * (x2r * x2r + x2i * x2i);
      const float cxr = x1r * x2r + x1i * x2i;
      const float cxi = x1r * x2i - x1i * x2r;
      metric -= (rr * cxr - ri * cxi);

      // interleaved bit order {I0,Q0,I1,Q1}
      int bI0, bI1, bQ0, bQ1;
      nr_lbest_axis_bits16(I1, &bI0, &bI1);
      nr_lbest_axis_bits16(Q1, &bQ0, &bQ1);
      const int bit[4] = {bI0, bQ0, bI1, bQ1};
      for (int p = 0; p < 4; p++) {
        if (bit[p] == 0) { if (metric > max0[p]) max0[p] = metric; }
        else             { if (metric > max1[p]) max1[p] = metric; }
      }
    }

    for (int p = 0; p < 4; p++) {
      float llr;
      if (max0[p] <= -1e29f)
        llr = -NR_LBEST_LLR_SAT;
      else if (max1[p] <= -1e29f)
        llr = NR_LBEST_LLR_SAT;
      else
        llr = (max0[p] - max1[p]) * NR_LBEST_LLR_SCALE;
      if (llr > 32767.0f) llr = 32767.0f;
      if (llr < -32768.0f) llr = -32768.0f;
      *stream0_out++ = (int16_t)llr;
    }
  }
}

// ============================================================================
// Generic float L-best machinery (any square QAM) for the >2-layer hybrid.
// kbits = Qm/2 (2/3/4 -> 16/64/256QAM); per-axis PAM-2^kbits, levels odd in
// [-(2^kbits-1), 2^kbits-1]. sqrtN = sqrt(norm), norm = 2(4^kbits-1)/3;
// rf = sqrtN / 2^(kbits-1) recovers ||h||^2 from the n1-scaled ch_mag.
// ============================================================================
static const double NR_LBEST_SQRTN[5] = {0, 0, 3.16227766016838, 6.48074069840786, 13.03840481040530};
static const double NR_LBEST_RF[5]    = {0, 0, 1.58113883008419, 1.62018517460196, 1.62980060130066};

// nearest PAM-2^kbits level to v (level units), O(1) round-to-nearest-odd, clamped.
static inline int nr_lbest_slice_gen(double v, int kbits)
{
  const int nlev = 1 << kbits, maxlev = nlev - 1;
  int o = 2 * (int)((v + nlev) * 0.5) - maxlev;
  if (o < -maxlev) o = -maxlev; else if (o > maxlev) o = maxlev;
  return o;
}

// Gray bits for one axis level: b[0]=sign, then nested b[i]=( |..| > 2^(kbits-1-i) ).
static inline void nr_lbest_axis_bits_gen(int level, int kbits, int *b)
{
  int v = level < 0 ? -level : level, t = 1 << (kbits - 1);
  b[0] = level < 0 ? 1 : 0;
  for (int i = 1; i < kbits; i++) {
    b[i] = v > t ? 1 : 0;
    v = v - t < 0 ? t - v : v - t;
    t >>= 1;
  }
}

// Per-RE 2-layer conditional-slice LLR for the target layer (x1), in DOUBLE.
// Inputs are recovered powers P0=||h1||^2, P1=||h2||^2 (NOT n1-scaled ch_mag),
// MF outputs z1,z2, cross rho=(rr,ri)=h1^H h2. Writes Qm=2*kbits int16 LLRs.
static void nr_lbest_2layer_re(double z1r, double z1i, double z2r, double z2i,
                               double P0, double P1, double rr, double ri,
                               int kbits, int L, double lambda, int16_t *out)
{
  const int nlev = 1 << kbits, M = nlev * nlev, nbits = 2 * kbits;
  const double sqrtN = NR_LBEST_SQRTN[kbits], NA = 1.0 / sqrtN;
  if (L < 1) L = 1;
  if (L > M) L = M;

  const double det = (P0 + lambda) * (P1 + lambda) - (rr * rr + ri * ri);
  const double invdet = det != 0.0 ? 1.0 / det : 0.0;
  const double estI = ((P1 + lambda) * z1r - (rr * z2r - ri * z2i)) * invdet * sqrtN;
  const double estQ = ((P1 + lambda) * z1i - (rr * z2i + ri * z2r)) * invdet * sqrtN;

  struct { double d; int iL, qL; } cand[256];
  int nc = 0;
  for (int qi = 0; qi < nlev; qi++) {
    const int lq = 2 * qi - (nlev - 1);
    for (int ii = 0; ii < nlev; ii++) {
      const int li = 2 * ii - (nlev - 1);
      const double dI = estI - li, dQ = estQ - lq;
      cand[nc].d = dI * dI + dQ * dQ;
      cand[nc].iL = li;
      cand[nc].qL = lq;
      nc++;
    }
  }
  if (L < M) // selection of the L smallest (M<=256, reference code)
    for (int a = 0; a < L; a++) {
      int m = a;
      for (int b = a + 1; b < M; b++)
        if (cand[b].d < cand[m].d) m = b;
      if (m != a) { __typeof__(cand[0]) tmp = cand[a]; cand[a] = cand[m]; cand[m] = tmp; }
    }

  double max0[8], max1[8];
  for (int p = 0; p < nbits; p++) { max0[p] = -1e30; max1[p] = -1e30; }

  for (int c = 0; c < L; c++) {
    const int I1 = cand[c].iL, Q1 = cand[c].qL;
    const double x1r = I1 * NA, x1i = Q1 * NA;
    double metric = (x1r * z1r + x1i * z1i) - 0.5 * P0 * (x1r * x1r + x1i * x1i);

    const double cr = rr * x1r + ri * x1i, ci = rr * x1i - ri * x1r;
    const double invP1 = P1 != 0.0 ? 1.0 / P1 : 0.0;
    const int I2 = nr_lbest_slice_gen((z2r - cr) * invP1 * sqrtN, kbits);
    const int Q2 = nr_lbest_slice_gen((z2i - ci) * invP1 * sqrtN, kbits);
    const double x2r = I2 * NA, x2i = Q2 * NA;
    metric += (x2r * z2r + x2i * z2i) - 0.5 * P1 * (x2r * x2r + x2i * x2i);
    metric -= (rr * (x1r * x2r + x1i * x2i) - ri * (x1r * x2i - x1i * x2r));

    int bI[4], bQ[4];
    nr_lbest_axis_bits_gen(I1, kbits, bI);
    nr_lbest_axis_bits_gen(Q1, kbits, bQ);
    for (int j = 0; j < kbits; j++) {
      const int p0 = 2 * j, p1 = 2 * j + 1; // interleaved {I0,Q0,I1,Q1,...}
      if (bI[j] == 0) { if (metric > max0[p0]) max0[p0] = metric; } else { if (metric > max1[p0]) max1[p0] = metric; }
      if (bQ[j] == 0) { if (metric > max0[p1]) max0[p1] = metric; } else { if (metric > max1[p1]) max1[p1] = metric; }
    }
  }

  for (int p = 0; p < nbits; p++) {
    double llr = (max0[p] <= -1e29) ? -NR_LBEST_LLR_SAT : (max1[p] <= -1e29) ? NR_LBEST_LLR_SAT : (max0[p] - max1[p]) * NR_LBEST_LLR_SCALE;
    if (llr > 32767.0) llr = 32767.0;
    if (llr < -32768.0) llr = -32768.0;
    out[p] = (int16_t)llr;
  }
}

// 3-layer HYBRID L-best (target layer t, nuisance n1,n2): per RE, project the
// nuisance layer most orthogonal to the target (smaller |rho|^2/||h||^2), scalar-
// Schur-deflate it, then run the 2-layer conditional-slice LLR on the deflated
// (target, kept-nuisance) pair. Inputs: per-layer MF z, n1-scaled ch_mag, and the
// three cross-Gram arrays rho_ab = h_a^H h_b. Writes Qm LLRs/RE for layer t.
void nr_qam_llr_3layer_hybrid(c16_t *zt, c16_t *zn1, c16_t *zn2,
                              c16_t *cmt, c16_t *cmn1, c16_t *cmn2,
                              c16_t *rho_tn1, c16_t *rho_tn2, c16_t *rho_n1n2,
                              int16_t *out, uint32_t length, int Qm, int L, float lambda)
{
  const int kbits = Qm / 2;
  const double rf = NR_LBEST_RF[kbits];
  for (uint32_t re = 0; re < length; re++) {
    const double Pt = cmt[re].r * rf, Pn1 = cmn1[re].r * rf, Pn2 = cmn2[re].r * rf;
    const double t1r = rho_tn1[re].r, t1i = rho_tn1[re].i;     // h_t^H h_n1
    const double t2r = rho_tn2[re].r, t2i = rho_tn2[re].i;     // h_t^H h_n2
    const double n12r = rho_n1n2[re].r, n12i = rho_n1n2[re].i; // h_n1^H h_n2

    // keep the more-aligned nuisance discrete, project the other (more orthogonal)
    const double p1 = (t1r * t1r + t1i * t1i) / Pn1;
    const double p2 = (t2r * t2r + t2i * t2i) / Pn2;
    double Pc, Pd, zcr, zci, zdr, zdi, tcr, tci, tdr, tdi, dcr, dci;
    if (p1 >= p2) { // keep n1 discrete, project n2
      Pc = Pn2; Pd = Pn1; zcr = zn2[re].r; zci = zn2[re].i; zdr = zn1[re].r; zdi = zn1[re].i;
      tcr = t2r; tci = t2i; tdr = t1r; tdi = t1i; dcr = n12r; dci = n12i; // rho_dc = h_n1^H h_n2
    } else {        // keep n2 discrete, project n1
      Pc = Pn1; Pd = Pn2; zcr = zn1[re].r; zci = zn1[re].i; zdr = zn2[re].r; zdi = zn2[re].i;
      tcr = t1r; tci = t1i; tdr = t2r; tdi = t2i; dcr = n12r; dci = -n12i; // rho_dc = h_n2^H h_n1 = conj
    }
    const double invPc = Pc != 0.0 ? 1.0 / Pc : 0.0;
    const double Pt2 = Pt - (tcr * tcr + tci * tci) * invPc;
    const double Pd2 = Pd - (dcr * dcr + dci * dci) * invPc;
    const double rtdr = tdr - (tcr * dcr + tci * dci) * invPc;          // rho_td - rho_tc conj(rho_dc)/Pc
    const double rtdi = tdi - (tci * dcr - tcr * dci) * invPc;
    const double ztr = zt[re].r - (tcr * zcr - tci * zci) * invPc;      // z_t - rho_tc/Pc z_c
    const double zti = zt[re].i - (tcr * zci + tci * zcr) * invPc;
    const double zdr2 = zdr - (dcr * zcr - dci * zci) * invPc;
    const double zdi2 = zdi - (dcr * zci + dci * zcr) * invPc;

    nr_lbest_2layer_re(ztr, zti, zdr2, zdi2, Pt2, Pd2, rtdr, rtdi, kbits, L, lambda, &out[re * Qm]);
  }
}

// 3-layer FULL ML reference (target t): per RE, exact max-log over discrete (x_t,x_n1)
// with the conditional best x_n2 sliced. For BLER comparison against the hybrid.
void nr_qam_llr_3layer_ml(c16_t *zt, c16_t *zn1, c16_t *zn2,
                          c16_t *cmt, c16_t *cmn1, c16_t *cmn2,
                          c16_t *rho_tn1, c16_t *rho_tn2, c16_t *rho_n1n2,
                          int16_t *out, uint32_t length, int Qm)
{
  const int kbits = Qm / 2, nlev = 1 << kbits, nbits = 2 * kbits;
  const double sqrtN = NR_LBEST_SQRTN[kbits], NA = 1.0 / sqrtN, rf = NR_LBEST_RF[kbits];
  for (uint32_t re = 0; re < length; re++) {
    const double Pt = cmt[re].r * rf, Pn1 = cmn1[re].r * rf, Pn2 = cmn2[re].r * rf;
    const double ztr = zt[re].r, zti = zt[re].i, z1r = zn1[re].r, z1i = zn1[re].i, z2r = zn2[re].r, z2i = zn2[re].i;
    const double t1r = rho_tn1[re].r, t1i = rho_tn1[re].i, t2r = rho_tn2[re].r, t2i = rho_tn2[re].i, n12r = rho_n1n2[re].r, n12i = rho_n1n2[re].i;
    const double invPn2 = Pn2 != 0.0 ? 1.0 / Pn2 : 0.0;
    double max0[8], max1[8];
    for (int p = 0; p < nbits; p++) { max0[p] = -1e30; max1[p] = -1e30; }

    for (int qit = 0; qit < nlev; qit++) for (int iit = 0; iit < nlev; iit++) {
      const int It = 2 * iit - (nlev - 1), Qt = 2 * qit - (nlev - 1);
      const double xtr = It * NA, xti = Qt * NA;
      for (int q1 = 0; q1 < nlev; q1++) for (int i1 = 0; i1 < nlev; i1++) {
        const int I1 = 2 * i1 - (nlev - 1), Q1 = 2 * q1 - (nlev - 1);
        const double x1r = I1 * NA, x1i = Q1 * NA;
        const double ar = z2r - (t2r * xtr + t2i * xti) - (n12r * x1r + n12i * x1i);
        const double ai = z2i - (t2r * xti - t2i * xtr) - (n12r * x1i - n12i * x1r);
        const int I2 = nr_lbest_slice_gen(ar * invPn2 * sqrtN, kbits), Q2 = nr_lbest_slice_gen(ai * invPn2 * sqrtN, kbits);
        const double x2r = I2 * NA, x2i = Q2 * NA;
        double m = (xtr * ztr + xti * zti) - 0.5 * Pt * (xtr * xtr + xti * xti)
                 + (x1r * z1r + x1i * z1i) - 0.5 * Pn1 * (x1r * x1r + x1i * x1i)
                 + (x2r * z2r + x2i * z2i) - 0.5 * Pn2 * (x2r * x2r + x2i * x2i)
                 - (t1r * (xtr * x1r + xti * x1i) - t1i * (xtr * x1i - xti * x1r))
                 - (t2r * (xtr * x2r + xti * x2i) - t2i * (xtr * x2i - xti * x2r))
                 - (n12r * (x1r * x2r + x1i * x2i) - n12i * (x1r * x2i - x1i * x2r));
        int bI[4], bQ[4];
        nr_lbest_axis_bits_gen(It, kbits, bI);
        nr_lbest_axis_bits_gen(Qt, kbits, bQ);
        for (int j = 0; j < kbits; j++) {
          const int p0 = 2 * j, p1 = 2 * j + 1;
          if (bI[j] == 0) { if (m > max0[p0]) max0[p0] = m; } else { if (m > max1[p0]) max1[p0] = m; }
          if (bQ[j] == 0) { if (m > max0[p1]) max0[p1] = m; } else { if (m > max1[p1]) max1[p1] = m; }
        }
      }
    }
    for (int p = 0; p < nbits; p++) {
      double llr = (max0[p] <= -1e29) ? -NR_LBEST_LLR_SAT : (max1[p] <= -1e29) ? NR_LBEST_LLR_SAT : (max0[p] - max1[p]) * NR_LBEST_LLR_SCALE;
      if (llr > 32767.0) llr = 32767.0;
      if (llr < -32768.0) llr = -32768.0;
      out[re * nbits + p] = (int16_t)llr;
    }
  }
}

// ============================================================================
// Fixed-point (Q15-input) prototype of the L-best 2-layer 64QAM kernel.
//
// Integer twin of nr_qam64_llr_2layer_lbest_layer: same algorithm, but every
// step uses the int16 demod buffers directly (no float). Two ideas keep it in
// the existing mulhi/adds Q15 idiom and make it SIMD-portable:
//
//  (1) Scale the whole ML metric by 8*sqrt(42). The desired/nuisance
//      correlation and energy terms then become EXACT integers (no irrational),
//      and a single 1/sqrt(42) survives only on the cross term:
//        Mq = 8*(Xr1.z1r+Xi1.z1i) - chmag0*|X1|^2          (desired)
//           + 8*(Xr2.z2r+Xi2.z2i) - chmag1*|X2|^2          (nuisance)
//           - 8*Re{rho.conj(X1).X2} / sqrt(42)             (cross)
//      where X1,X2 are the *integer* level pairs (odd in [-7,7]) and chmag is
//      the raw ch_mag input (= ||h||^2 * 4/sqrt(42)).
//
//  (2) No per-RE reciprocals. The ZF seed level and the conditional nuisance
//      slice are both decided by comparing a numerator against det- / ||h||^2-
//      scaled thresholds, never by dividing.
//
// Reference/oracle: identical LLRs (within fixed-point rounding) to
// nr_qam64_llr_2layer_lbest at the same L; L==64 == full search.
// ============================================================================
#define NR_LBEST_Q_SQRT42_Q12        26545   // round(sqrt(42)        * 2^12)
#define NR_LBEST_Q_SQRT42_OVER4_Q14  26545   // round(sqrt(42)/4      * 2^14) (same constant)
#define NR_LBEST_Q_INVSQRT42_Q15      5057   // round(2^15 / sqrt(42))
// integer metric is 8*sqrt(42) (~51.8x) larger than the float-ref metric; bring
// the emitted LLR back onto the float-ref / SIMD scale.
#define NR_LBEST_Q_LLR_NUM            2533    // round(2^17 / (8*sqrt(42)))
#define NR_LBEST_Q_LLR_SHIFT            17
#define NR_LBEST_Q_LLR_SAT           8192

// nearest 64QAM PAM-8 level magnitude for |a|, thresholds at 2/4/6 * step.
static inline int nr_lbest_q_pam8_abs(int64_t a, int64_t step)
{
  if (a < 2 * step) return 1;
  if (a < 4 * step) return 3;
  if (a < 6 * step) return 5;
  return 7;
}

static void nr_qam64_llr_2layer_lbest_q15_layer(const c16_t *stream0_in,
                                                const c16_t *stream1_in,
                                                const c16_t *ch_mag,
                                                const c16_t *ch_mag_i,
                                                int16_t *stream0_out,
                                                const c16_t *rho01,
                                                uint32_t length,
                                                int L)
{
  if (L < 1)
    L = 1;
  if (L > 64)
    L = 64;
  static const int levels[8] = {-7, -5, -3, -1, 1, 3, 5, 7};

  for (uint32_t re = 0; re < length; re++) {
    const int32_t z1r = stream0_in[re].r, z1i = stream0_in[re].i;
    const int32_t z2r = stream1_in[re].r, z2i = stream1_in[re].i;
    const int32_t rr = rho01[re].r, ri = rho01[re].i;     // rho = h0^H h1
    const int32_t cm0 = ch_mag[re].r;                     // ||h0||^2 * 4/sqrt(42)
    const int32_t cm1 = ch_mag_i[re].r;                   // ||h1||^2 * 4/sqrt(42)

    // recovered channel powers ||h||^2 (for the seed determinant / thresholds)
    const int64_t Pp0 = ((int64_t)cm0 * NR_LBEST_Q_SQRT42_OVER4_Q14) >> 14;
    const int64_t Pp1 = ((int64_t)cm1 * NR_LBEST_Q_SQRT42_OVER4_Q14) >> 14;

    // ---- 1. division-free ZF seed: x_hat1 = ((Pp1) z1 - rho z2) / det --------
    const int64_t det = Pp0 * Pp1 - ((int64_t)rr * rr + (int64_t)ri * ri); // >= 0
    const int64_t numr = Pp1 * z1r - ((int64_t)rr * z2r - (int64_t)ri * z2i);
    const int64_t numi = Pp1 * z1i - ((int64_t)rr * z2i + (int64_t)ri * z2r);
    // soft level estimate in Q6 (level units * 64); clamp to the 64QAM range.
    int32_t estI_q6 = 0, estQ_q6 = 0;
    if (det > 0) {
      estI_q6 = (int32_t)((numr * NR_LBEST_Q_SQRT42_Q12) / (det * 64));
      estQ_q6 = (int32_t)((numi * NR_LBEST_Q_SQRT42_Q12) / (det * 64));
      if (estI_q6 < -512) estI_q6 = -512; else if (estI_q6 > 512) estI_q6 = 512;
      if (estQ_q6 < -512) estQ_q6 = -512; else if (estQ_q6 > 512) estQ_q6 = 512;
    }

    // ---- 2. candidate set C1 = L nearest constellation points to x_hat1 ------
    int candI[64], candQ[64];
    int64_t cand_d[64];
    int nc = 0;
    for (int qi = 0; qi < 8; qi++) {
      for (int ii = 0; ii < 8; ii++) {
        const int64_t dI = estI_q6 - (levels[ii] << 6);
        const int64_t dQ = estQ_q6 - (levels[qi] << 6);
        candI[nc] = levels[ii];
        candQ[nc] = levels[qi];
        cand_d[nc] = dI * dI + dQ * dQ;
        nc++;
      }
    }
    for (int a = 0; a < L; a++) { // partial selection of the L smallest
      int m = a;
      for (int b = a + 1; b < 64; b++)
        if (cand_d[b] < cand_d[m])
          m = b;
      if (m != a) {
        int64_t td = cand_d[a]; cand_d[a] = cand_d[m]; cand_d[m] = td;
        int ti = candI[a]; candI[a] = candI[m]; candI[m] = ti;
        int tq = candQ[a]; candQ[a] = candQ[m]; candQ[m] = tq;
      }
    }

    // ---- 3+4. metric over candidates (scaled by 8*sqrt(42)), max-log per bit -
    int64_t max0[6], max1[6];
    for (int p = 0; p < 6; p++) { max0[p] = INT64_MIN; max1[p] = INT64_MIN; }

    for (int c = 0; c < L; c++) {
      const int I1 = candI[c], Q1 = candQ[c];

      // conditional ML nuisance x2*: slice (z2*sqrt42 - conj(rho).X1) vs Pp1*thr
      const int64_t A2I = (((int64_t)z2r * NR_LBEST_Q_SQRT42_Q12) >> 12) - ((int64_t)rr * I1 + (int64_t)ri * Q1);
      const int64_t A2Q = (((int64_t)z2i * NR_LBEST_Q_SQRT42_Q12) >> 12) - ((int64_t)rr * Q1 - (int64_t)ri * I1);
      const int I2 = (A2I < 0 ? -1 : 1) * nr_lbest_q_pam8_abs(A2I < 0 ? -A2I : A2I, Pp1);
      const int Q2 = (A2Q < 0 ? -1 : 1) * nr_lbest_q_pam8_abs(A2Q < 0 ? -A2Q : A2Q, Pp1);

      // metric (units of 8*sqrt(42) * float-ref metric)
      int64_t metric = 8 * ((int64_t)I1 * z1r + (int64_t)Q1 * z1i) - cm0 * ((int64_t)I1 * I1 + (int64_t)Q1 * Q1);
      metric += 8 * ((int64_t)I2 * z2r + (int64_t)Q2 * z2i) - cm1 * ((int64_t)I2 * I2 + (int64_t)Q2 * Q2);
      // cross = 8 * Re{rho.conj(X1).X2} / sqrt(42)
      const int64_t rec = (int64_t)rr * ((int64_t)I1 * I2 + (int64_t)Q1 * Q2) + (int64_t)ri * ((int64_t)Q1 * I2 - (int64_t)I1 * Q2);
      metric -= (8 * rec * NR_LBEST_Q_INVSQRT42_Q15) >> 15;

      // bit labels, interleaved {I0,Q0,I1,Q1,I2,Q2} (matches nr_qam64_llr_2layer)
      int bI0, bI1, bI2, bQ0, bQ1, bQ2;
      nr_lbest_axis_bits(I1, &bI0, &bI1, &bI2);
      nr_lbest_axis_bits(Q1, &bQ0, &bQ1, &bQ2);
      const int bit[6] = {bI0, bQ0, bI1, bQ1, bI2, bQ2};
      for (int p = 0; p < 6; p++) {
        if (bit[p] == 0) { if (metric > max0[p]) max0[p] = metric; }
        else             { if (metric > max1[p]) max1[p] = metric; }
      }
    }

    for (int p = 0; p < 6; p++) {
      int32_t llr;
      if (max0[p] == INT64_MIN)
        llr = -NR_LBEST_Q_LLR_SAT;
      else if (max1[p] == INT64_MIN)
        llr = NR_LBEST_Q_LLR_SAT;
      else
        llr = (int32_t)(((max0[p] - max1[p]) * NR_LBEST_Q_LLR_NUM) >> NR_LBEST_Q_LLR_SHIFT);
      if (llr > 32767) llr = 32767;
      if (llr < -32768) llr = -32768;
      *stream0_out++ = (int16_t)llr;
    }
  }
}

// Public entry for the fixed-point L-best 64QAM kernel (target layer 0, ZF seed).
void nr_qam64_llr_2layer_lbest_q15(c16_t *stream0_in,
                                   c16_t *stream1_in,
                                   c16_t *ch_mag,
                                   c16_t *ch_mag_i,
                                   int16_t *stream0_out,
                                   c16_t *rho01,
                                   uint32_t length,
                                   int L)
{
  nr_qam64_llr_2layer_lbest_q15_layer(stream0_in, stream1_in, ch_mag, ch_mag_i, stream0_out, rho01, length, L);
}

// ============================================================================
// int16/16-lane variant of the AVX2 L-best kernel. The dominant per-candidate
// work (metric, conditional slice, max-log reduction) runs 16 REs/vector in
// int16; only the ZF seed and z2*sqrt(42) (which overflow int16) use int32 half-
// vectors, once per RE-vector. Metric is carried at scale (8*sqrt(42))/512 so all
// terms fit int16. Single simde-256 source -> NEON on aarch64.
// ============================================================================
#define NR_LBEST_Q_INVSQRT42_Q16 10112   // round(1/sqrt(42) * 2^16)
#define NR_LBEST_Q_PP1_OVER32_Q16 3318   // round(sqrt(42)/4/32 * 2^16) -> ||h||^2/32 via mulhi
#define NR_LBEST_SIMD16_SENT  (-32768)   // empty-subset sentinel (int16)

static inline simde__m256i nr_lbest_widen(simde__m256i v, int hi)
{
  return hi ? simde_mm256_cvtepi16_epi32(simde_mm256_extracti128_si256(v, 1))
            : simde_mm256_cvtepi16_epi32(simde_mm256_castsi256_si128(v));
}

// int16 PAM-8 level (sign*mag, mag in {1,3,5,7}) sliced vs {2,4,6}*thunit.
static inline simde__m256i nr_lbest_simd_level16(simde__m256i num, simde__m256i thunit)
{
  const simde__m256i z = simde_mm256_setzero_si256();
  const simde__m256i a = simde_mm256_abs_epi16(num);
  const simde__m256i t2 = simde_mm256_slli_epi16(thunit, 1);
  const simde__m256i t4 = simde_mm256_slli_epi16(thunit, 2);
  const simde__m256i t6 = simde_mm256_adds_epi16(t2, t4);
  const simde__m256i g2 = simde_mm256_sub_epi16(z, simde_mm256_cmpgt_epi16(a, t2));
  const simde__m256i g4 = simde_mm256_sub_epi16(z, simde_mm256_cmpgt_epi16(a, t4));
  const simde__m256i g6 = simde_mm256_sub_epi16(z, simde_mm256_cmpgt_epi16(a, t6));
  const simde__m256i mag = simde_mm256_adds_epi16(simde_mm256_set1_epi16(1),
                                                  simde_mm256_slli_epi16(simde_mm256_adds_epi16(simde_mm256_adds_epi16(g2, g4), g6), 1));
  const simde__m256i neg = simde_mm256_cmpgt_epi16(z, num);
  return simde_mm256_blendv_epi8(mag, simde_mm256_sub_epi16(z, mag), neg);
}

// load 16 complex int16 (c16_t) -> int16 real/imag vectors, lane l == RE l.
static inline void nr_lbest_simd_load16(const c16_t *p, simde__m256i *re, simde__m256i *im)
{
  const simde__m256i sh = simde_mm256_setr_epi8(0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15,
                                                0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15);
  const simde__m256i v0 = simde_mm256_permute4x64_epi64(simde_mm256_shuffle_epi8(simde_mm256_loadu_si256((const simde__m256i *)p), sh), SIMDE_MM_SHUFFLE(3, 1, 2, 0));
  const simde__m256i v1 = simde_mm256_permute4x64_epi64(simde_mm256_shuffle_epi8(simde_mm256_loadu_si256((const simde__m256i *)(p + 8)), sh), SIMDE_MM_SHUFFLE(3, 1, 2, 0));
  *re = simde_mm256_set_m128i(simde_mm256_castsi256_si128(v1), simde_mm256_castsi256_si128(v0));
  *im = simde_mm256_set_m128i(simde_mm256_extracti128_si256(v1, 1), simde_mm256_extracti128_si256(v0, 1));
}

// z * sqrt(42) / 32, packed to int16 (lane l == RE l).
static inline simde__m256i nr_lbest_z42d32(simde__m256i z16)
{
  const simde__m256i C = simde_mm256_set1_epi32(NR_LBEST_Q_SQRT42_Q12);
  const simde__m256i lo = simde_mm256_srai_epi32(simde_mm256_mullo_epi32(nr_lbest_widen(z16, 0), C), 17); // >>12 (Q12) >>5 (/32)
  const simde__m256i hi = simde_mm256_srai_epi32(simde_mm256_mullo_epi32(nr_lbest_widen(z16, 1), C), 17);
  return simde_mm256_permute4x64_epi64(simde_mm256_packs_epi32(lo, hi), SIMDE_MM_SHUFFLE(3, 1, 2, 0));
}

// ZF-seed nearest-level centers for 16 REs (int16 out). The 2x2 solve is done in
// FLOAT, which is robust to the wide per-RE dynamic range of real channels (TDL
// deep fades) where any fixed int pre-shift either loses precision or underflows.
// Also emits dirImask/dirQmask: int16 mask, all-ones where the seed lies on the
// +2 side of the nearest level (i.e. the toward-seed neighbour is center+2).
static inline void nr_lbest_simd_seed16(simde__m256i z1r16, simde__m256i z1i16, simde__m256i z2r16, simde__m256i z2i16,
                                        simde__m256i rr16, simde__m256i ri16, simde__m256i cm0_16, simde__m256i cm1_16,
                                        simde__m256i *centerI, simde__m256i *centerQ,
                                        simde__m256i *dirImask, simde__m256i *dirQmask)
{
  const simde__m256i SQRT42_OVER4 = simde_mm256_set1_epi32(NR_LBEST_Q_SQRT42_OVER4_Q14);
  const simde__m256 SQRT42 = simde_mm256_set1_ps(6.48074069840786f);
  const simde__m256 HALF = simde_mm256_set1_ps(0.5f), ONE = simde_mm256_set1_ps(1.0f), TWO = simde_mm256_set1_ps(2.0f);
  const simde__m256 P7 = simde_mm256_set1_ps(7.0f), M7 = simde_mm256_set1_ps(-7.0f);
  simde__m256i cI[2], cQ[2], dI[2], dQ[2];
  for (int h = 0; h < 2; h++) {
    const simde__m256 z1r = simde_mm256_cvtepi32_ps(nr_lbest_widen(z1r16, h)), z1i = simde_mm256_cvtepi32_ps(nr_lbest_widen(z1i16, h));
    const simde__m256 z2r = simde_mm256_cvtepi32_ps(nr_lbest_widen(z2r16, h)), z2i = simde_mm256_cvtepi32_ps(nr_lbest_widen(z2i16, h));
    const simde__m256 rr = simde_mm256_cvtepi32_ps(nr_lbest_widen(rr16, h)), ri = simde_mm256_cvtepi32_ps(nr_lbest_widen(ri16, h));
    const simde__m256 Pp0 = simde_mm256_cvtepi32_ps(simde_mm256_srai_epi32(simde_mm256_mullo_epi32(nr_lbest_widen(cm0_16, h), SQRT42_OVER4), 14));
    const simde__m256 Pp1 = simde_mm256_cvtepi32_ps(simde_mm256_srai_epi32(simde_mm256_mullo_epi32(nr_lbest_widen(cm1_16, h), SQRT42_OVER4), 14));
    // ZF 2x2 solve (float): x_hat1 = ((Pp1) z1 - rho z2) / det,  det = Pp0 Pp1 - |rho|^2
    simde__m256 det = simde_mm256_sub_ps(simde_mm256_mul_ps(Pp0, Pp1), simde_mm256_add_ps(simde_mm256_mul_ps(rr, rr), simde_mm256_mul_ps(ri, ri)));
    det = simde_mm256_max_ps(det, ONE); // guard near-singular (high correlation)
    const simde__m256 numr = simde_mm256_sub_ps(simde_mm256_mul_ps(Pp1, z1r), simde_mm256_sub_ps(simde_mm256_mul_ps(rr, z2r), simde_mm256_mul_ps(ri, z2i)));
    const simde__m256 numi = simde_mm256_sub_ps(simde_mm256_mul_ps(Pp1, z1i), simde_mm256_add_ps(simde_mm256_mul_ps(rr, z2i), simde_mm256_mul_ps(ri, z2r)));
    // soft level estimate est = x_hat1 * sqrt(42)  (~ +-1..7)
    const simde__m256 estI = simde_mm256_mul_ps(simde_mm256_div_ps(numr, det), SQRT42);
    const simde__m256 estQ = simde_mm256_mul_ps(simde_mm256_div_ps(numi, det), SQRT42);
    // nearest odd level: o = 2*round((est-1)/2)+1, clamped to [-7,7]
    simde__m256 oI = simde_mm256_add_ps(simde_mm256_mul_ps(simde_mm256_round_ps(simde_mm256_mul_ps(simde_mm256_sub_ps(estI, ONE), HALF), SIMDE_MM_FROUND_TO_NEAREST_INT | SIMDE_MM_FROUND_NO_EXC), TWO), ONE);
    simde__m256 oQ = simde_mm256_add_ps(simde_mm256_mul_ps(simde_mm256_round_ps(simde_mm256_mul_ps(simde_mm256_sub_ps(estQ, ONE), HALF), SIMDE_MM_FROUND_TO_NEAREST_INT | SIMDE_MM_FROUND_NO_EXC), TWO), ONE);
    oI = simde_mm256_min_ps(simde_mm256_max_ps(oI, M7), P7);
    oQ = simde_mm256_min_ps(simde_mm256_max_ps(oQ, M7), P7);
    cI[h] = simde_mm256_cvtps_epi32(oI);
    cQ[h] = simde_mm256_cvtps_epi32(oQ);
    dI[h] = simde_mm256_castps_si256(simde_mm256_cmp_ps(estI, oI, SIMDE_CMP_GT_OQ)); // toward-seed = +2 side
    dQ[h] = simde_mm256_castps_si256(simde_mm256_cmp_ps(estQ, oQ, SIMDE_CMP_GT_OQ));
  }
  *centerI = simde_mm256_permute4x64_epi64(simde_mm256_packs_epi32(cI[0], cI[1]), SIMDE_MM_SHUFFLE(3, 1, 2, 0));
  *centerQ = simde_mm256_permute4x64_epi64(simde_mm256_packs_epi32(cQ[0], cQ[1]), SIMDE_MM_SHUFFLE(3, 1, 2, 0));
  *dirImask = simde_mm256_permute4x64_epi64(simde_mm256_packs_epi32(dI[0], dI[1]), SIMDE_MM_SHUFFLE(3, 1, 2, 0));
  *dirQmask = simde_mm256_permute4x64_epi64(simde_mm256_packs_epi32(dQ[0], dQ[1]), SIMDE_MM_SHUFFLE(3, 1, 2, 0));
}

// Per-candidate evaluation (slice + metric + max-log reduction), updating max0/max1.
// Captures the per-RE-vector operands from the enclosing scope; the per-candidate
// I-only/Q-only terms (bits, corr0/energy0 parts, slice operands) are passed in.
#define NR_LBEST_EVAL16(I1_, Q1_, BI0, BI1, BI2, BQ0, BQ1, BQ2, C0I_, C0Q_, EI_, EQ_, RRI_, RII_, RRQ_, RIQ_) \
  do { \
    const simde__m256i A2I = simde_mm256_subs_epi16(z2r42, simde_mm256_adds_epi16((RRI_), (RIQ_))); \
    const simde__m256i A2Q = simde_mm256_subs_epi16(z2i42, simde_mm256_subs_epi16((RRQ_), (RII_))); \
    const simde__m256i I2 = nr_lbest_simd_level16(A2I, Pp1_5); \
    const simde__m256i Q2 = nr_lbest_simd_level16(A2Q, Pp1_5); \
    const simde__m256i corr0 = simde_mm256_adds_epi16((C0I_), (C0Q_)); \
    const simde__m256i energy0 = simde_mm256_adds_epi16((EI_), (EQ_)); \
    const simde__m256i corr1 = simde_mm256_adds_epi16(simde_mm256_mulhi_epi16(z2r, simde_mm256_mullo_epi16(I2, C13)), simde_mm256_mulhi_epi16(z2i, simde_mm256_mullo_epi16(Q2, C13))); \
    const simde__m256i energy1 = simde_mm256_mulhi_epi16(cm1, simde_mm256_mullo_epi16(simde_mm256_adds_epi16(simde_mm256_mullo_epi16(I2, I2), simde_mm256_mullo_epi16(Q2, Q2)), CE)); \
    const simde__m256i aa = simde_mm256_adds_epi16(simde_mm256_mullo_epi16((I1_), I2), simde_mm256_mullo_epi16((Q1_), Q2)); \
    const simde__m256i bb = simde_mm256_subs_epi16(simde_mm256_mullo_epi16((Q1_), I2), simde_mm256_mullo_epi16((I1_), Q2)); \
    const simde__m256i cross = simde_mm256_adds_epi16(simde_mm256_mulhi_epi16(rr, simde_mm256_mullo_epi16(aa, CX)), simde_mm256_mulhi_epi16(ri, simde_mm256_mullo_epi16(bb, CX))); \
    simde__m256i metric = simde_mm256_max_epi16(simde_mm256_subs_epi16(simde_mm256_adds_epi16(simde_mm256_subs_epi16(corr0, energy0), simde_mm256_subs_epi16(corr1, energy1)), cross), FLOORM); \
    const simde__m256i bm[6] = {(BI0), (BQ0), (BI1), (BQ1), (BI2), (BQ2)}; \
    for (int p = 0; p < 6; p++) { \
      max0[p] = simde_mm256_max_epi16(max0[p], simde_mm256_blendv_epi8(metric, SENT, bm[p])); \
      max1[p] = simde_mm256_max_epi16(max1[p], simde_mm256_blendv_epi8(SENT, metric, bm[p])); \
    } \
  } while (0)

// pattern: 0 = 3x3 (9 cand, full BLER), 1 = 6-cand (plus + toward-seed diagonal),
//          2 = 5-plus (center + +-2 each axis).
void nr_qam64_llr_2layer_lbest_q15_simd16(c16_t *stream0_in,
                                          c16_t *stream1_in,
                                          c16_t *ch_mag,
                                          c16_t *ch_mag_i,
                                          int16_t *stream0_out,
                                          c16_t *rho01,
                                          uint32_t length,
                                          int pattern)
{
  const simde__m256i SENT = simde_mm256_set1_epi16(NR_LBEST_SIMD16_SENT);
  const simde__m256i FLOORM = simde_mm256_set1_epi16(-32000); // keep real metrics above SENT
  const simde__m256i V1 = simde_mm256_set1_epi16(1), V4 = simde_mm256_set1_epi16(4);
  const simde__m256i V7 = simde_mm256_set1_epi16(7), Vm7 = simde_mm256_set1_epi16(-7);
  const simde__m256i z = simde_mm256_setzero_si256();

  uint32_t re = 0;
  for (; re + 16 <= length; re += 16) {
    simde__m256i z1r, z1i, z2r, z2i, rr, ri, cm0, cm1, dummy;
    nr_lbest_simd_load16(&stream0_in[re], &z1r, &z1i);
    nr_lbest_simd_load16(&stream1_in[re], &z2r, &z2i);
    nr_lbest_simd_load16(&rho01[re], &rr, &ri);
    nr_lbest_simd_load16(&ch_mag[re], &cm0, &dummy);
    nr_lbest_simd_load16(&ch_mag_i[re], &cm1, &dummy);

    simde__m256i centerI, centerQ, dirImask, dirQmask;
    nr_lbest_simd_seed16(z1r, z1i, z2r, z2i, rr, ri, cm0, cm1, &centerI, &centerQ, &dirImask, &dirQmask);
    (void)dirImask; (void)dirQmask;

    // per-RE precompute (int16): conditional-slice operands (/32 scale)
    const simde__m256i z2r42 = nr_lbest_z42d32(z2r), z2i42 = nr_lbest_z42d32(z2i);
    const simde__m256i rr5 = simde_mm256_srai_epi16(rr, 5), ri5 = simde_mm256_srai_epi16(ri, 5);
    const simde__m256i Pp1_5 = simde_mm256_mulhi_epi16(cm1, simde_mm256_set1_epi16(NR_LBEST_Q_PP1_OVER32_Q16));
    // metric Q-constants (normal/8 scale; precision-preserving via mulhi, not pre-shift)
    const simde__m256i C13 = simde_mm256_set1_epi16(1264); // 2^13/sqrt(42): mulhi(z, I*C13) = z*I/(8 sqrt42)
    const simde__m256i CE = simde_mm256_set1_epi16(158);   // 2^16/(64 sqrt42): energy = cm*|X|^2/(64 sqrt42)
    const simde__m256i CX = simde_mm256_set1_epi16(195);   // 2^16/336: cross = rec/336

    simde__m256i max0[6], max1[6];
    for (int p = 0; p < 6; p++) { max0[p] = SENT; max1[p] = SENT; }

    // 3x3 candidate block, HOISTED (full BLER). Precompute the I-only / Q-only terms
    // for the 3 distinct I-levels and 3 Q-levels, then combine in the 9-pair loop.
    // corr1/energy1/cross + the two level16 slicers stay per-pair (depend on the
    // conditionally-sliced I2,Q2, which couple I and Q).
    static const int offs3[3] = {-2, 0, 2};
    simde__m256i Iv[3], Qv[3];
    simde__m256i bIv[3][3], bQv[3][3];                       // [level][bit 0..2]
    simde__m256i c0I[3], c0Q[3], eI[3], eQ[3];               // corr0 / energy0 parts
    simde__m256i rrI[3], riI[3], rrQ[3], riQ[3];             // slice operands
    for (int k = 0; k < 3; k++) {
      const simde__m256i I1 = simde_mm256_min_epi16(simde_mm256_max_epi16(simde_mm256_adds_epi16(centerI, simde_mm256_set1_epi16(offs3[k])), Vm7), V7);
      const simde__m256i Q1 = simde_mm256_min_epi16(simde_mm256_max_epi16(simde_mm256_adds_epi16(centerQ, simde_mm256_set1_epi16(offs3[k])), Vm7), V7);
      Iv[k] = I1; Qv[k] = Q1;
      const simde__m256i aI = simde_mm256_abs_epi16(I1), aQ = simde_mm256_abs_epi16(Q1);
      bIv[k][0] = simde_mm256_cmpgt_epi16(z, I1);
      bIv[k][1] = simde_mm256_cmpgt_epi16(aI, V4);
      bIv[k][2] = simde_mm256_or_si256(simde_mm256_cmpeq_epi16(aI, V1), simde_mm256_cmpeq_epi16(aI, V7));
      bQv[k][0] = simde_mm256_cmpgt_epi16(z, Q1);
      bQv[k][1] = simde_mm256_cmpgt_epi16(aQ, V4);
      bQv[k][2] = simde_mm256_or_si256(simde_mm256_cmpeq_epi16(aQ, V1), simde_mm256_cmpeq_epi16(aQ, V7));
      c0I[k] = simde_mm256_mulhi_epi16(z1r, simde_mm256_mullo_epi16(I1, C13));
      c0Q[k] = simde_mm256_mulhi_epi16(z1i, simde_mm256_mullo_epi16(Q1, C13));
      eI[k] = simde_mm256_mulhi_epi16(cm0, simde_mm256_mullo_epi16(simde_mm256_mullo_epi16(I1, I1), CE));
      eQ[k] = simde_mm256_mulhi_epi16(cm0, simde_mm256_mullo_epi16(simde_mm256_mullo_epi16(Q1, Q1), CE));
      rrI[k] = simde_mm256_mullo_epi16(rr5, I1); riI[k] = simde_mm256_mullo_epi16(ri5, I1);
      rrQ[k] = simde_mm256_mullo_epi16(rr5, Q1); riQ[k] = simde_mm256_mullo_epi16(ri5, Q1);
    }
    if (pattern == 0) { // 3x3: all 9 grid pairs
      for (int iI = 0; iI < 3; iI++)
        for (int iQ = 0; iQ < 3; iQ++)
          NR_LBEST_EVAL16(Iv[iI], Qv[iQ], bIv[iI][0], bIv[iI][1], bIv[iI][2], bQv[iQ][0], bQv[iQ][1], bQv[iQ][2],
                          c0I[iI], c0Q[iQ], eI[iI], eQ[iQ], rrI[iI], riI[iI], rrQ[iQ], riQ[iQ]);
    } else { // 5-plus: center (1,1) + I+-2 + Q+-2
      static const int pI[5] = {1, 0, 2, 1, 1}, pQ[5] = {1, 1, 1, 0, 2};
      for (int c = 0; c < 5; c++) {
        const int iI = pI[c], iQ = pQ[c];
        NR_LBEST_EVAL16(Iv[iI], Qv[iQ], bIv[iI][0], bIv[iI][1], bIv[iI][2], bQv[iQ][0], bQv[iQ][1], bQv[iQ][2],
                        c0I[iI], c0Q[iQ], eI[iI], eQ[iQ], rrI[iI], riI[iI], rrQ[iQ], riQ[iQ]);
      }
      if (pattern == 1) { // 6th: toward-seed diagonal, per-lane select corner 0 vs 2
#define NR_LBEST_SI(a) simde_mm256_blendv_epi8((a)[0], (a)[2], dirImask)
#define NR_LBEST_SQ(a) simde_mm256_blendv_epi8((a)[0], (a)[2], dirQmask)
        NR_LBEST_EVAL16(NR_LBEST_SI(Iv), NR_LBEST_SQ(Qv),
                        simde_mm256_blendv_epi8(bIv[0][0], bIv[2][0], dirImask),
                        simde_mm256_blendv_epi8(bIv[0][1], bIv[2][1], dirImask),
                        simde_mm256_blendv_epi8(bIv[0][2], bIv[2][2], dirImask),
                        simde_mm256_blendv_epi8(bQv[0][0], bQv[2][0], dirQmask),
                        simde_mm256_blendv_epi8(bQv[0][1], bQv[2][1], dirQmask),
                        simde_mm256_blendv_epi8(bQv[0][2], bQv[2][2], dirQmask),
                        NR_LBEST_SI(c0I), NR_LBEST_SQ(c0Q), NR_LBEST_SI(eI), NR_LBEST_SQ(eQ),
                        NR_LBEST_SI(rrI), NR_LBEST_SI(riI), NR_LBEST_SQ(rrQ), NR_LBEST_SQ(riQ));
#undef NR_LBEST_SI
#undef NR_LBEST_SQ
      }
    }

    int16_t tmp[6][16];
    for (int p = 0; p < 6; p++) {
      const simde__m256i diff = simde_mm256_subs_epi16(max0[p], max1[p]);
      // metric is normal/8 scale -> LLR = diff*8 (== float-ref / SIMD scale); saturate via int32
      const simde__m256i lo = simde_mm256_slli_epi32(nr_lbest_widen(diff, 0), 3);
      const simde__m256i hi = simde_mm256_slli_epi32(nr_lbest_widen(diff, 1), 3);
      simde__m256i res = simde_mm256_permute4x64_epi64(simde_mm256_packs_epi32(lo, hi), SIMDE_MM_SHUFFLE(3, 1, 2, 0));
      res = simde_mm256_blendv_epi8(res, simde_mm256_set1_epi16(-NR_LBEST_Q_LLR_SAT), simde_mm256_cmpeq_epi16(max0[p], SENT));
      res = simde_mm256_blendv_epi8(res, simde_mm256_set1_epi16(NR_LBEST_Q_LLR_SAT), simde_mm256_cmpeq_epi16(max1[p], SENT));
      simde_mm256_storeu_si256((simde__m256i *)tmp[p], res);
    }
    for (int r = 0; r < 16; r++)
      for (int p = 0; p < 6; p++)
        stream0_out[(re + r) * 6 + p] = tmp[p][r];
  }

  if (re < length)
    nr_qam64_llr_2layer_lbest_q15_layer(&stream0_in[re], &stream1_in[re], &ch_mag[re], &ch_mag_i[re],
                                        &stream0_out[re * 6], &rho01[re], length - re, 9);
}

void nr_compute_ML_llr(c16_t *rxdataF_comp0,
                       c16_t *rxdataF_comp1,
                       c16_t *ch_mag0,
                       c16_t *ch_mag1,
                       int16_t *llr_layers0,
                       int16_t *llr_layers1,
                       c16_t *rho0,
                       c16_t *rho1,
                       uint32_t nb_re,
                       uint8_t mod_order)
{
  switch (mod_order) {
    case 2:
      nr_qpsk_llr_2layer(rxdataF_comp0, rxdataF_comp1, llr_layers0, rho0, nb_re);
      nr_qpsk_llr_2layer(rxdataF_comp1, rxdataF_comp0, llr_layers1, rho1, nb_re);
      nr_ml_llr_shift((int16_t *)llr_layers0, (int16_t *)llr_layers1, nb_re, 4);
      break;
    case 4:
      nr_qam16_llr_2layer(rxdataF_comp0, rxdataF_comp1, ch_mag0, ch_mag1, llr_layers0, rho0, nb_re);
      nr_qam16_llr_2layer(rxdataF_comp1, rxdataF_comp0, ch_mag1, ch_mag0, llr_layers1, rho1, nb_re);
      break;
    case 6:
      // 2-layer 64QAM. Default = full ML search (nr_qam64_llr_2layer), correct on all channels.
      // The reduced-search L-best kernel (nr_qam64_llr_2layer_lbest_q15_simd16) is GATED OFF by
      // default: it is only accurate on low-correlation / sparse channels (e.g. mmWave indoor,
      // LOS-dominated), where the linear (ZF) seed predicts the ML symbol well. On correlated /
      // selective channels (e.g. 3GPP TDL) it loses several dB and MUST NOT be used.
      // Enable per-scenario via OAI_LBEST=1; OAI_LBEST_PAT picks the candidate set
      // (0=3x3 [default], 1=6-cand seed-aware, 2=5-plus). TODO: replace the env gate with a
      // proper RX-mode/config flag set by the scenario.
      {
        static int lbest = -1, pat = 0;
        if (lbest < 0) {
          const char *e = getenv("OAI_LBEST");
          lbest = e ? atoi(e) : 0;
          const char *ep = getenv("OAI_LBEST_PAT");
          pat = ep ? atoi(ep) : 0;
        }
        if (lbest) {
          nr_qam64_llr_2layer_lbest_q15_simd16(rxdataF_comp0, rxdataF_comp1, ch_mag0, ch_mag1, llr_layers0, rho0, nb_re, pat);
          nr_qam64_llr_2layer_lbest_q15_simd16(rxdataF_comp1, rxdataF_comp0, ch_mag1, ch_mag0, llr_layers1, rho1, nb_re, pat);
        } else {
          nr_qam64_llr_2layer(rxdataF_comp0, rxdataF_comp1, ch_mag0, ch_mag1, llr_layers0, rho0, nb_re);
          nr_qam64_llr_2layer(rxdataF_comp1, rxdataF_comp0, ch_mag1, ch_mag0, llr_layers1, rho1, nb_re);
        }
      }
      break;
    case 8:
      // 2-layer 256QAM ML via the float L-best reference (ANALYSIS path; only reached when the
      // demod L-best gate routes Qm=8 here). L = OAI_LBEST_L256 (default 256 = full ML search).
      {
        static int L256 = -1;
        if (L256 < 0) { const char *e = getenv("OAI_LBEST_L256"); L256 = e ? atoi(e) : 256; }
        nr_qam256_llr_2layer_lbest(rxdataF_comp0, rxdataF_comp1, ch_mag0, ch_mag1, llr_layers0, rho0, nb_re, L256, 0.0f);
        nr_qam256_llr_2layer_lbest(rxdataF_comp1, rxdataF_comp0, ch_mag1, ch_mag0, llr_layers1, rho1, nb_re, L256, 0.0f);
      }
      break;
    default:
      AssertFatal(1 == 0, "nr_compute_ML_llr: invalid mod_order, Qm = %d\n", mod_order);
  }
}
// Zero Forcing Rx function: nr_det_HhH()
static void nr_det_HhH(c16_t *after_mf_00, // a
                       c16_t *after_mf_01, // b
                       c16_t *after_mf_10, // c
                       c16_t *after_mf_11, // d
                       uint32_t *det_fin, // 1/ad-bc
                       unsigned short nb_rb)
{
  simde__m128i *after_mf_00_128, *after_mf_01_128, *after_mf_10_128, *after_mf_11_128, ad_re_128, bc_re_128; // ad_im_128,
                                                                                                             // bc_im_128;
  simde__m128i *det_fin_128, det_re_128; // det_im_128, tmp_det0, tmp_det1;

  after_mf_00_128 = (simde__m128i *)after_mf_00;
  after_mf_01_128 = (simde__m128i *)after_mf_01;
  after_mf_10_128 = (simde__m128i *)after_mf_10;
  after_mf_11_128 = (simde__m128i *)after_mf_11;

  det_fin_128 = (simde__m128i *)det_fin;

  for (unsigned short rb = 0; rb < 3 * nb_rb; rb++) {
    // complex multiplication (I_a+jQ_a)(I_d+jQ_d) = (I_aI_d - Q_aQ_d) + j(Q_aI_d + I_aQ_d)
    // The imag part is often zero, we compute only the real part
    ad_re_128 = simde_mm_madd_epi16(oai_mm_conj(after_mf_00_128[0]), after_mf_11_128[0]); // Re: I_a0*I_d0 - Q_a1*Q_d1
    // ad_im_128 = simde_mm_madd_epi16(oai_mm_swap(after_mf_00_128[0]),after_mf_11_128[0]);//Im: (Q_aI_d + I_aQ_d)

    // complex multiplication (I_b+jQ_b)(I_c+jQ_c) = (I_bI_c - Q_bQ_c) + j(Q_bI_c + I_bQ_c)
    // The imag part is often zero, we compute only the real part
    bc_re_128 = simde_mm_madd_epi16(oai_mm_conj(after_mf_01_128[0]), after_mf_10_128[0]); // Re: I_b0*I_c0 - Q_b1*Q_c1
    // bc_im_128 = simde_mm_madd_epi16(oai_mm_swap(after_mf_01_128[0]),after_mf_10_128[0]);//Im: (Q_bI_c + I_bQ_c)

    det_re_128 = simde_mm_sub_epi32(ad_re_128, bc_re_128);
    // det_im_128 = simde_mm_sub_epi32(ad_im_128, bc_im_128);

    // det in Q30 format
    det_fin_128[0] = simde_mm_abs_epi32(det_re_128);

#ifdef DEBUG_DLSCH_DEMOD
    printf("\n Computing det_HhH_inv \n");
    // print_ints("det_re_128:",(int32_t*)&det_re_128);
    // print_ints("det_im_128:",(int32_t*)&det_im_128);
    print_ints("det_fin_128:", (int32_t *)&det_fin_128[0]);
#endif
    det_fin_128 += 1;
    after_mf_00_128 += 1;
    after_mf_01_128 += 1;
    after_mf_10_128 += 1;
    after_mf_11_128 += 1;
  }
}

/* Zero Forcing Rx function: nr_conjch0_mult_ch1()
 *
 *
 * */
// TODO: This function is just a wrapper, can be removed.
static void nr_conjch0_mult_ch1(c16_t *ch0, c16_t *ch1, c16_t *ch0conj_ch1, unsigned short nb_rb, unsigned char output_shift0)
{
  // This function is used to compute multiplications in H_hermitian * H matrix
  mult_cpx_conj_vector(ch0, ch1, ch0conj_ch1, 12 * nb_rb, output_shift0);
}

static simde__m128i nr_comp_muli_sum(simde__m128i input_x,
                                     simde__m128i input_y,
                                     simde__m128i input_w,
                                     simde__m128i input_z,
                                     simde__m128i det)
{
  // complex multiplication (x_re + jx_im)*(y_re + jy_im) = (x_re*y_re - x_im*y_im) + j(x_im*y_re + x_re*y_im)
  // complex multiplication (w_re + jw_im)*(z_re + jz_im) = (w_re*z_re - w_im*z_im) + j(w_im*z_re + w_re*z_im)
  // the real part
  simde__m128i xy_re_128 = simde_mm_madd_epi16(oai_mm_conj(input_x), input_y); // Re: (x_re*y_re - x_im*y_im)
  simde__m128i wz_re_128 = simde_mm_madd_epi16(oai_mm_conj(input_w), input_z); // Re: (w_re*z_re - w_im*z_im)
  xy_re_128 = simde_mm_sub_epi32(xy_re_128, wz_re_128);

  // the imag part
  simde__m128i xy_im_128 = simde_mm_madd_epi16(oai_mm_swap(input_x), input_y); // Im: (x_im*y_re + x_re*y_im)
  simde__m128i wz_im_128 = simde_mm_madd_epi16(oai_mm_swap(input_w), input_z); // Im: (w_im*z_re + w_re*z_im)
  xy_im_128 = simde_mm_sub_epi32(xy_im_128, wz_im_128);

  // print_ints("rx_re:",(int32_t*)&xy_re_128[0]);
  // print_ints("rx_Img:",(int32_t*)&xy_im_128[0]);
  // divide by matrix det and convert back to Q15 before packing
  uint64_t sum_det = 0;
  for (int k = 0; k < 4; k++) {
    sum_det += (((uint32_t *)&det)[k]);
  }
  // Add bias to reduce rounding error
  sum_det = (sum_det + 2) >> 2;

  int b = log2_approx(sum_det) - 8;
  if (b > 0) {
    xy_re_128 = simde_mm_srai_epi32(xy_re_128, b);
    xy_im_128 = simde_mm_srai_epi32(xy_im_128, b);
  } else {
    xy_re_128 = simde_mm_slli_epi32(xy_re_128, -b);
    xy_im_128 = simde_mm_slli_epi32(xy_im_128, -b);
  }

  simde__m128i output = oai_mm_pack(xy_re_128, xy_im_128);

  return (output);
}

/* Zero Forcing Rx function: nr_construct_HhH_elements()
 *
 *
 * */
static void nr_construct_HhH_elements(c16_t *conjch00_ch00,
                                      c16_t *conjch01_ch01,
                                      c16_t *conjch11_ch11,
                                      c16_t *conjch10_ch10, //
                                      c16_t *conjch20_ch20,
                                      c16_t *conjch21_ch21,
                                      c16_t *conjch30_ch30,
                                      c16_t *conjch31_ch31,
                                      c16_t *conjch00_ch01, // 00_01
                                      c16_t *conjch01_ch00, // 01_00
                                      c16_t *conjch10_ch11, // 10_11
                                      c16_t *conjch11_ch10, // 11_10
                                      c16_t *conjch20_ch21,
                                      c16_t *conjch21_ch20,
                                      c16_t *conjch30_ch31,
                                      c16_t *conjch31_ch30,
                                      c16_t *after_mf_00,
                                      c16_t *after_mf_01,
                                      c16_t *after_mf_10,
                                      c16_t *after_mf_11,
                                      unsigned short nb_rb)
{
  // This function is used to construct the (H_hermitian * H matrix) matrix elements
  simde__m128i *conjch00_ch00_128 = (simde__m128i *)conjch00_ch00;
  simde__m128i *conjch01_ch01_128 = (simde__m128i *)conjch01_ch01;
  simde__m128i *conjch11_ch11_128 = (simde__m128i *)conjch11_ch11;
  simde__m128i *conjch10_ch10_128 = (simde__m128i *)conjch10_ch10;

  simde__m128i *conjch20_ch20_128 = (simde__m128i *)conjch20_ch20;
  simde__m128i *conjch21_ch21_128 = (simde__m128i *)conjch21_ch21;
  simde__m128i *conjch30_ch30_128 = (simde__m128i *)conjch30_ch30;
  simde__m128i *conjch31_ch31_128 = (simde__m128i *)conjch31_ch31;

  simde__m128i *conjch00_ch01_128 = (simde__m128i *)conjch00_ch01;
  simde__m128i *conjch01_ch00_128 = (simde__m128i *)conjch01_ch00;
  simde__m128i *conjch10_ch11_128 = (simde__m128i *)conjch10_ch11;
  simde__m128i *conjch11_ch10_128 = (simde__m128i *)conjch11_ch10;

  simde__m128i *conjch20_ch21_128 = (simde__m128i *)conjch20_ch21;
  simde__m128i *conjch21_ch20_128 = (simde__m128i *)conjch21_ch20;
  simde__m128i *conjch30_ch31_128 = (simde__m128i *)conjch30_ch31;
  simde__m128i *conjch31_ch30_128 = (simde__m128i *)conjch31_ch30;

  simde__m128i *after_mf_00_128 = (simde__m128i *)after_mf_00;
  simde__m128i *after_mf_01_128 = (simde__m128i *)after_mf_01;
  simde__m128i *after_mf_10_128 = (simde__m128i *)after_mf_10;
  simde__m128i *after_mf_11_128 = (simde__m128i *)after_mf_11;

  for (unsigned short rb = 0; rb < 3 * nb_rb; rb++) {
    after_mf_00_128[0] = simde_mm_adds_epi16(conjch00_ch00_128[0], conjch10_ch10_128[0]); // 00_00 + 10_10
    if (conjch20_ch20 != NULL)
      after_mf_00_128[0] = simde_mm_adds_epi16(after_mf_00_128[0], conjch20_ch20_128[0]);
    if (conjch30_ch30 != NULL)
      after_mf_00_128[0] = simde_mm_adds_epi16(after_mf_00_128[0], conjch30_ch30_128[0]);

    after_mf_11_128[0] = simde_mm_adds_epi16(conjch01_ch01_128[0], conjch11_ch11_128[0]); // 01_01 + 11_11
    if (conjch21_ch21 != NULL)
      after_mf_11_128[0] = simde_mm_adds_epi16(after_mf_11_128[0], conjch21_ch21_128[0]);
    if (conjch31_ch31 != NULL)
      after_mf_11_128[0] = simde_mm_adds_epi16(after_mf_11_128[0], conjch31_ch31_128[0]);

    after_mf_01_128[0] = simde_mm_adds_epi16(conjch00_ch01_128[0], conjch10_ch11_128[0]); // 00_01 + 10_11
    if (conjch20_ch21 != NULL)
      after_mf_01_128[0] = simde_mm_adds_epi16(after_mf_01_128[0], conjch20_ch21_128[0]);
    if (conjch30_ch31 != NULL)
      after_mf_01_128[0] = simde_mm_adds_epi16(after_mf_01_128[0], conjch30_ch31_128[0]);

    after_mf_10_128[0] = simde_mm_adds_epi16(conjch01_ch00_128[0], conjch11_ch10_128[0]); // 01_00 + 11_10
    if (conjch21_ch20 != NULL)
      after_mf_10_128[0] = simde_mm_adds_epi16(after_mf_10_128[0], conjch21_ch20_128[0]);
    if (conjch31_ch30 != NULL)
      after_mf_10_128[0] = simde_mm_adds_epi16(after_mf_10_128[0], conjch31_ch30_128[0]);

#ifdef DEBUG_DLSCH_DEMOD
    if ((rb <= 30)) {
      printf(" \n construct_HhH_elements \n");
      print_shorts("after_mf_00_128:", (int16_t *)&after_mf_00_128[0]);
      print_shorts("after_mf_01_128:", (int16_t *)&after_mf_01_128[0]);
      print_shorts("after_mf_10_128:", (int16_t *)&after_mf_10_128[0]);
      print_shorts("after_mf_11_128:", (int16_t *)&after_mf_11_128[0]);
    }
#endif
    conjch00_ch00_128 += 1;
    conjch10_ch10_128 += 1;
    conjch01_ch01_128 += 1;
    conjch11_ch11_128 += 1;

    if (conjch20_ch20 != NULL)
      conjch20_ch20_128 += 1;
    if (conjch21_ch21 != NULL)
      conjch21_ch21_128 += 1;
    if (conjch30_ch30 != NULL)
      conjch30_ch30_128 += 1;
    if (conjch31_ch31 != NULL)
      conjch31_ch31_128 += 1;

    conjch00_ch01_128 += 1;
    conjch01_ch00_128 += 1;
    conjch10_ch11_128 += 1;
    conjch11_ch10_128 += 1;

    if (conjch20_ch21 != NULL)
      conjch20_ch21_128 += 1;
    if (conjch21_ch20 != NULL)
      conjch21_ch20_128 += 1;
    if (conjch30_ch31 != NULL)
      conjch30_ch31_128 += 1;
    if (conjch31_ch30 != NULL)
      conjch31_ch30_128 += 1;

    after_mf_00_128 += 1;
    after_mf_01_128 += 1;
    after_mf_10_128 += 1;
    after_mf_11_128 += 1;
  }
}

// MMSE Rx function: nr_mmse_2layers()
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
                        uint32_t noise_var)
{
  uint32_t nb_rb_0 = length / 12 + ((length % 12) ? 1 : 0);

  /* we need at least alignment to 16 bytes, let's put 32 to be sure
   * (maybe not necessary but doesn't hurt)
   */
  c16_t conjch00_ch01[12 * nb_rb] __attribute__((aligned(32)));
  c16_t conjch01_ch00[12 * nb_rb] __attribute__((aligned(32)));
  c16_t conjch10_ch11[12 * nb_rb] __attribute__((aligned(32)));
  c16_t conjch11_ch10[12 * nb_rb] __attribute__((aligned(32)));
  c16_t conjch00_ch00[12 * nb_rb] __attribute__((aligned(32)));
  c16_t conjch01_ch01[12 * nb_rb] __attribute__((aligned(32)));
  c16_t conjch10_ch10[12 * nb_rb] __attribute__((aligned(32)));
  c16_t conjch11_ch11[12 * nb_rb] __attribute__((aligned(32)));
  c16_t conjch20_ch20[12 * nb_rb] __attribute__((aligned(32)));
  c16_t conjch21_ch21[12 * nb_rb] __attribute__((aligned(32)));
  c16_t conjch30_ch30[12 * nb_rb] __attribute__((aligned(32)));
  c16_t conjch31_ch31[12 * nb_rb] __attribute__((aligned(32)));
  c16_t conjch20_ch21[12 * nb_rb] __attribute__((aligned(32)));
  c16_t conjch30_ch31[12 * nb_rb] __attribute__((aligned(32)));
  c16_t conjch21_ch20[12 * nb_rb] __attribute__((aligned(32)));
  c16_t conjch31_ch30[12 * nb_rb] __attribute__((aligned(32)));

  c16_t af_mf_00[12 * nb_rb] __attribute__((aligned(32)));
  c16_t af_mf_01[12 * nb_rb] __attribute__((aligned(32)));
  c16_t af_mf_10[12 * nb_rb] __attribute__((aligned(32)));
  c16_t af_mf_11[12 * nb_rb] __attribute__((aligned(32)));
  uint32_t determ_fin[12 * nb_rb] __attribute__((aligned(32)));

  c16_t *ch00, *ch01, *ch10, *ch11;
  c16_t *ch20, *ch30, *ch21, *ch31;
  switch (nb_rx_ant) {
    case 2: //
      ch00 = ch_estimates_ext[0][0];
      ch01 = ch_estimates_ext[1][0];
      ch10 = ch_estimates_ext[0][1];
      ch11 = ch_estimates_ext[1][1];
      ch20 = NULL;
      ch21 = NULL;
      ch30 = NULL;
      ch31 = NULL;
      break;

    case 4: //
      ch00 = ch_estimates_ext[0][0];
      ch01 = ch_estimates_ext[1][0];
      ch10 = ch_estimates_ext[0][1];
      ch11 = ch_estimates_ext[1][1];
      ch20 = ch_estimates_ext[0][2];
      ch21 = ch_estimates_ext[1][2];
      ch30 = ch_estimates_ext[0][3];
      ch31 = ch_estimates_ext[1][3];
      break;

    default:
      return -1;
      break;
  }

  /* 1- Compute the rx channel matrix after compensation: (1/2^log2_max)x(H_herm x H)
   * for n_rx = 2
   * |conj_H_00       conj_H_10|    | H_00         H_01|   |(conj_H_00xH_00+conj_H_10xH_10)   (conj_H_00xH_01+conj_H_10xH_11)|
   * |                         |  x |                  | = |                                                                 |
   * |conj_H_01       conj_H_11|    | H_10         H_11|   |(conj_H_01xH_00+conj_H_11xH_10)   (conj_H_01xH_01+conj_H_11xH_11)|
   *
   */

  if (nb_rx_ant >= 2) {
    // (1/2^log2_maxh)*conj_H_00xH_00: (1/(64*2))conjH_00*H_00*2^15
    nr_conjch0_mult_ch1(ch00, ch00, conjch00_ch00, nb_rb_0, shift);
    // (1/2^log2_maxh)*conj_H_10xH_10: (1/(64*2))conjH_10*H_10*2^15
    nr_conjch0_mult_ch1(ch10, ch10, conjch10_ch10, nb_rb_0, shift);
    // conj_H_00xH_01
    nr_conjch0_mult_ch1(ch00, ch01, conjch00_ch01, nb_rb_0,
                        shift); // this shift is equal to the channel level log2_maxh
    // conj_H_10xH_11
    nr_conjch0_mult_ch1(ch10, ch11, conjch10_ch11, nb_rb_0, shift);
    // conj_H_01xH_01
    nr_conjch0_mult_ch1(ch01, ch01, conjch01_ch01, nb_rb_0, shift);
    // conj_H_11xH_11
    nr_conjch0_mult_ch1(ch11, ch11, conjch11_ch11, nb_rb_0, shift);
    // conj_H_01xH_00
    nr_conjch0_mult_ch1(ch01, ch00, conjch01_ch00, nb_rb_0, shift);
    // conj_H_11xH_10
    nr_conjch0_mult_ch1(ch11, ch10, conjch11_ch10, nb_rb_0, shift);
  }
  if (nb_rx_ant == 4) {
    // (1/2^log2_maxh)*conj_H_20xH_20: (1/(64*2*16))conjH_20*H_20*2^15
    nr_conjch0_mult_ch1(ch20, ch20, conjch20_ch20, nb_rb_0, shift);

    // (1/2^log2_maxh)*conj_H_30xH_30: (1/(64*2*4))conjH_30*H_30*2^15
    nr_conjch0_mult_ch1(ch30, ch30, conjch30_ch30, nb_rb_0, shift);

    // (1/2^log2_maxh)*conj_H_20xH_20: (1/(64*2))conjH_20*H_20*2^15
    nr_conjch0_mult_ch1(ch20, ch21, conjch20_ch21, nb_rb_0, shift);

    nr_conjch0_mult_ch1(ch30, ch31, conjch30_ch31, nb_rb_0, shift);

    nr_conjch0_mult_ch1(ch21, ch21, conjch21_ch21, nb_rb_0, shift);

    nr_conjch0_mult_ch1(ch31, ch31, conjch31_ch31, nb_rb_0, shift);

    // (1/2^log2_maxh)*conj_H_20xH_20: (1/(64*2))conjH_20*H_20*2^15
    nr_conjch0_mult_ch1(ch21, ch20, conjch21_ch20, nb_rb_0, shift);

    nr_conjch0_mult_ch1(ch31, ch30, conjch31_ch30, nb_rb_0, shift);

    nr_construct_HhH_elements(conjch00_ch00,
                              conjch01_ch01,
                              conjch11_ch11,
                              conjch10_ch10, //
                              conjch20_ch20,
                              conjch21_ch21,
                              conjch30_ch30,
                              conjch31_ch31,
                              conjch00_ch01,
                              conjch01_ch00,
                              conjch10_ch11,
                              conjch11_ch10, //
                              conjch20_ch21,
                              conjch21_ch20,
                              conjch30_ch31,
                              conjch31_ch30,
                              af_mf_00,
                              af_mf_01,
                              af_mf_10,
                              af_mf_11,
                              nb_rb_0);
  }
  if (nb_rx_ant == 2) {
    nr_construct_HhH_elements(conjch00_ch00,
                              conjch01_ch01,
                              conjch11_ch11,
                              conjch10_ch10, //
                              NULL,
                              NULL,
                              NULL,
                              NULL,
                              conjch00_ch01,
                              conjch01_ch00,
                              conjch10_ch11,
                              conjch11_ch10, //
                              NULL,
                              NULL,
                              NULL,
                              NULL,
                              af_mf_00,
                              af_mf_01,
                              af_mf_10,
                              af_mf_11,
                              nb_rb_0);
  }

  // Add noise_var such that: H^h * H + noise_var * I
  if (noise_var != 0) {
    simde__m128i nvar_128i = simde_mm_set1_epi32(noise_var >> shift);
    simde__m128i *af_mf_00_128i = (simde__m128i *)af_mf_00;
    simde__m128i *af_mf_11_128i = (simde__m128i *)af_mf_11;
    for (int k = 0; k < 3 * nb_rb_0; k++) {
      af_mf_00_128i[0] = simde_mm_add_epi32(af_mf_00_128i[0], nvar_128i);
      af_mf_11_128i[0] = simde_mm_add_epi32(af_mf_11_128i[0], nvar_128i);
      af_mf_00_128i++;
      af_mf_11_128i++;
    }
  }

  // det_HhH = ad -bc
  nr_det_HhH(af_mf_00, // a
             af_mf_01, // b
             af_mf_10, // c
             af_mf_11, // d
             determ_fin,
             nb_rb_0);
  /* 2- Compute the channel matrix inversion **********************************
   *
   *    |(conj_H_00xH_00+conj_H_10xH_10)   (conj_H_00xH_01+conj_H_10xH_11)|
   * A= |                                                                 |
   *    |(conj_H_01xH_00+conj_H_11xH_10)   (conj_H_01xH_01+conj_H_11xH_11)|
   *
   *
   *
   *inv(A) =(1/det)*[d  -b
   *                 -c  a]
   *
   *
   **************************************************************************/
  simde__m128i *ch_mag128_0 = NULL, *ch_mag128b_0 = NULL, *ch_mag128c_0 = NULL; // Layer 0
  simde__m128i *ch_mag128_1 = NULL, *ch_mag128b_1 = NULL, *ch_mag128c_1 = NULL; // Layer 1
  simde__m128i mmtmpD0, mmtmpD1, mmtmpD2, mmtmpD3;
  simde__m128i QAM_amp128 = {0}, QAM_amp128b = {0}, QAM_amp128c = {0};

  simde__m128i *determ_fin_128 = (simde__m128i *)&determ_fin[0];

  simde__m128i *after_mf_a_128 = (simde__m128i *)af_mf_00;
  simde__m128i *after_mf_b_128 = (simde__m128i *)af_mf_01;
  simde__m128i *after_mf_c_128 = (simde__m128i *)af_mf_10;
  simde__m128i *after_mf_d_128 = (simde__m128i *)af_mf_11;

  simde__m128i *rxdataF_comp128_0 = (simde__m128i *)&rxdataF_comp[0][symbol * buffer_length];
  simde__m128i *rxdataF_comp128_1 = (simde__m128i *)&rxdataF_comp[1][symbol * buffer_length];

  if (mod_order > 2) {
    if (mod_order == 4) {
      QAM_amp128 = simde_mm_set1_epi16(QAM16_n1); // 2/sqrt(10)
      QAM_amp128b = simde_mm_setzero_si128();
      QAM_amp128c = simde_mm_setzero_si128();
    } else if (mod_order == 6) {
      QAM_amp128 = simde_mm_set1_epi16(QAM64_n1); // 4/sqrt{42}
      QAM_amp128b = simde_mm_set1_epi16(QAM64_n2); // 2/sqrt{42}
      QAM_amp128c = simde_mm_setzero_si128();
    } else if (mod_order == 8) {
      QAM_amp128 = simde_mm_set1_epi16(QAM256_n1);
      QAM_amp128b = simde_mm_set1_epi16(QAM256_n2);
      QAM_amp128c = simde_mm_set1_epi16(QAM256_n3);
    }
    ch_mag128_0 = (simde__m128i *)ch_mag[0];
    ch_mag128b_0 = (simde__m128i *)ch_magb[0];
    ch_mag128c_0 = (simde__m128i *)ch_magc[0];
    ch_mag128_1 = (simde__m128i *)ch_mag[1];
    ch_mag128b_1 = (simde__m128i *)ch_magb[1];
    ch_mag128c_1 = (simde__m128i *)ch_magc[1];
  }

  for (int rb = 0; rb < 3 * nb_rb_0; rb++) {
    // Magnitude computation
    if (mod_order > 2) {
      uint64_t sum_det = 0;
      for (int k = 0; k < 4; k++) {
        sum_det += (((uint32_t *)&determ_fin_128[0])[k]);
      }
      // Add bias to reduce rounding error
      sum_det = (sum_det + 2) >> 2;

      int b = log2_approx(sum_det) - 8;
      if (b > 0) {
        mmtmpD2 = simde_mm_srai_epi32(determ_fin_128[0], b);
      } else {
        mmtmpD2 = simde_mm_slli_epi32(determ_fin_128[0], -b);
      }
      mmtmpD3 = simde_mm_unpacklo_epi32(mmtmpD2, mmtmpD2);
      mmtmpD2 = simde_mm_unpackhi_epi32(mmtmpD2, mmtmpD2);
      mmtmpD2 = simde_mm_packs_epi32(mmtmpD3, mmtmpD2);

      // Layer 0
      ch_mag128_0[0] = mmtmpD2;
      ch_mag128b_0[0] = mmtmpD2;
      ch_mag128c_0[0] = mmtmpD2;
      ch_mag128_0[0] = simde_mm_mulhi_epi16(ch_mag128_0[0], QAM_amp128);
      ch_mag128_0[0] = simde_mm_slli_epi16(ch_mag128_0[0], 1);
      ch_mag128b_0[0] = simde_mm_mulhi_epi16(ch_mag128b_0[0], QAM_amp128b);
      ch_mag128b_0[0] = simde_mm_slli_epi16(ch_mag128b_0[0], 1);
      ch_mag128c_0[0] = simde_mm_mulhi_epi16(ch_mag128c_0[0], QAM_amp128c);
      ch_mag128c_0[0] = simde_mm_slli_epi16(ch_mag128c_0[0], 1);

      // Layer 1
      ch_mag128_1[0] = mmtmpD2;
      ch_mag128b_1[0] = mmtmpD2;
      ch_mag128c_1[0] = mmtmpD2;
      ch_mag128_1[0] = simde_mm_mulhi_epi16(ch_mag128_1[0], QAM_amp128);
      ch_mag128_1[0] = simde_mm_slli_epi16(ch_mag128_1[0], 1);
      ch_mag128b_1[0] = simde_mm_mulhi_epi16(ch_mag128b_1[0], QAM_amp128b);
      ch_mag128b_1[0] = simde_mm_slli_epi16(ch_mag128b_1[0], 1);
      ch_mag128c_1[0] = simde_mm_mulhi_epi16(ch_mag128c_1[0], QAM_amp128c);
      ch_mag128c_1[0] = simde_mm_slli_epi16(ch_mag128c_1[0], 1);
    }

    // multiply by channel Inv
    // rxdataF_zf128_0 = rxdataF_comp128_0*d - b*rxdataF_comp128_1
    // rxdataF_zf128_1 = rxdataF_comp128_1*a - c*rxdataF_comp128_0
    // printf("layer_1 \n");
    mmtmpD0 = nr_comp_muli_sum(rxdataF_comp128_0[0], after_mf_d_128[0], rxdataF_comp128_1[0], after_mf_b_128[0], determ_fin_128[0]);

    // printf("layer_2 \n");
    mmtmpD1 = nr_comp_muli_sum(rxdataF_comp128_1[0], after_mf_a_128[0], rxdataF_comp128_0[0], after_mf_c_128[0], determ_fin_128[0]);

    rxdataF_comp128_0[0] = mmtmpD0;
    rxdataF_comp128_1[0] = mmtmpD1;

#ifdef DEBUG_DLSCH_DEMOD
    printf("\n Rx signal after ZF l%d rb%d\n", symbol, rb);
    print_shorts(" Rx layer 1:", (int16_t *)&rxdataF_comp128_0[0]);
    print_shorts(" Rx layer 2:", (int16_t *)&rxdataF_comp128_1[0]);
#endif
    determ_fin_128 += 1;
    ch_mag128_0 += 1;
    ch_mag128_1 += 1;
    ch_mag128b_0 += 1;
    ch_mag128b_1 += 1;
    ch_mag128c_0 += 1;
    ch_mag128c_1 += 1;
    rxdataF_comp128_0 += 1;
    rxdataF_comp128_1 += 1;
    after_mf_a_128 += 1;
    after_mf_b_128 += 1;
    after_mf_c_128 += 1;
    after_mf_d_128 += 1;
  }
  return (0);
}
