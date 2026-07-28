/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "tools_defs.h"
#include "PHY/impl_defs_top.h"

// Compute Energy of a complex signal vector, removing the DC component!
// input  : points to vector
// length : length of vector in complex samples

#define shift 4

//-----------------------------------------------------------------
// Average Power calculation with DC removing
//-----------------------------------------------------------------
int32_t signal_energy(int32_t *input,uint32_t length)
{
  uint32_t i;
  int32_t temp;
  simde__m128i in, in_clp, i16_min, coe1;
  simde__m128 num0, num1, num2, num3, recp1;

  //init
  num0 = simde_mm_setzero_ps();
  num1 = simde_mm_setzero_ps();
  i16_min = simde_mm_set1_epi16(SHRT_MIN);
  coe1 = simde_mm_set1_epi16(1);
  recp1 = simde_mm_rcp_ps(simde_mm_cvtepi32_ps(simde_mm_set1_epi32(length)));

  //Acc
  for (i = 0; i < (length >> 2); i++) {
    in = simde_mm_loadu_si128((simde__m128i *)input);
    in_clp = simde_mm_subs_epi16(in, simde_mm_cmpeq_epi16(in, i16_min));//if in=SHRT_MIN in+1, else in
    num0 = simde_mm_add_ps(num0, simde_mm_cvtepi32_ps(simde_mm_madd_epi16(in_clp, in_clp)));
    num1 = simde_mm_add_ps(num1, simde_mm_cvtepi32_ps(simde_mm_madd_epi16(in, coe1)));//DC
    input += 4;
  }
  //Ave
  num2 = simde_mm_dp_ps(num0, recp1, 0xFF);//AC power
  num3 = simde_mm_dp_ps(num1, recp1, 0xFF);//DC
  num3 = simde_mm_mul_ps(num3, num3);      //DC power
  //remove DC
  temp = simde_mm_cvtsi128_si32(simde_mm_cvttps_epi32(simde_mm_sub_ps(num2, num3)));

  return temp;
}

uint32_t signal_energy_nodc(const c16_t *input, uint32_t length)
{
#if defined(__riscv) && defined(__riscv_vector)
  /* sum_k(|c_k|^2)/length. The SIMDe path accumulates |c|^2 in float across 4
   * lanes (loses precision above 2^24, and a VLA float reduction would be
   * VLEN-dependent). Accumulate in int64 instead: exact, associative, identical
   * on every VLEN -- strictly more accurate for this power measurement (feeds
   * dB_fixed/SNR/AGC). vlseg2 (re/im) + vwmul/vwmacc (|c|^2) + widening reduction. */
  const int16_t *p = (const int16_t *)input;
  int64_t acc = 0;
  for (uint32_t n = 0; n < length;) {
    size_t vl = __riscv_vsetvl_e16mf2(length - n);
    vint16mf2x2_t v = __riscv_vlseg2e16_v_i16mf2x2(p + 2 * n, vl);
    vint16mf2_t cr = __riscv_vget_v_i16mf2x2_i16mf2(v, 0), ci = __riscv_vget_v_i16mf2x2_i16mf2(v, 1);
    vint32m1_t pw = __riscv_vwmacc_vv_i32m1(__riscv_vwmul_vv_i32m1(cr, cr, vl), ci, ci, vl);
    acc += __riscv_vmv_x_s_i64m1_i64(__riscv_vwredsum_vs_i32m1_i64m1(pw, __riscv_vmv_s_x_i64m1(0, 1), vl));
    n += (uint32_t)vl;
  }
  return (uint32_t)((double)acc / (double)length);
#else
  // init
  simde__m128 mm0 = simde_mm_setzero_ps();

  // Acc
  for (int32_t i = 0; i < (length >> 2); i++) {
    simde__m128i in = simde_mm_loadu_si128((simde__m128i *)input);
    mm0 = simde_mm_add_ps(mm0, simde_mm_cvtepi32_ps(simde_mm_madd_epi16(in, in)));
    input += 4;
  }

  // leftover
  float leftover_sum = 0;
  c16_t *leftover_input = (c16_t *)input;
  uint16_t lefover_count = length - ((length >> 2) << 2);
  for (int32_t i = 0; i < lefover_count; i++) {
    leftover_sum += leftover_input[i].r * leftover_input[i].r + leftover_input[i].i * leftover_input[i].i;
  }

  // Ave
  float sums[4];
  simde_mm_store_ps(sums, mm0);
  return (uint32_t)((sums[0] + sums[1] + sums[2] + sums[3] + leftover_sum) / (float)length);
#endif
}

double signal_energy_fp(double *s_re[2],double *s_im[2],uint32_t nb_antennas,uint32_t length,uint32_t offset)
{

  int32_t aa,i;
  double V=0.0;

  for (i=0; i<length; i++) {
    for (aa=0; aa<nb_antennas; aa++) {
     V= V + (s_re[aa][i+offset]*s_re[aa][i+offset]) + (s_im[aa][i+offset]*s_im[aa][i+offset]);
    }
  }

  return(V/length/nb_antennas);
}

double signal_energy_fp2(struct complexd *s,uint32_t length)
{

  int32_t i;
  double V=0.0;

  for (i=0; i<length; i++) {
		          //    printf("signal_energy_fp2 : %f,%f => %f\n",s[i].x,s[i].y,V);
		  //        //      V= V + (s[i].y*s[i].x) + (s[i].y*s[i].x);
    V= V + (s[i].r*s[i].r) + (s[i].i*s[i].i);
  }
  return(V/length);
}

int32_t signal_power(int32_t *input, uint32_t length)
{

  uint32_t i;
  int32_t temp;

  simde__m128i in, in_clp, i16_min;
  simde__m128  num0, num1;
  simde__m128  recp1;

  //init
  num0 = simde_mm_setzero_ps();
  i16_min = simde_mm_set1_epi16(SHRT_MIN);
  recp1 = simde_mm_rcp_ps(simde_mm_cvtepi32_ps(simde_mm_set1_epi32(length)));
  //Acc
  for (i = 0; i < (length >> 2); i++) {
    in = simde_mm_loadu_si128((simde__m128i *)input);
    in_clp = simde_mm_subs_epi16(in, simde_mm_cmpeq_epi16(in, i16_min));//if in=SHRT_MIN in+1, else in
    num0 = simde_mm_add_ps(num0, simde_mm_cvtepi32_ps(simde_mm_madd_epi16(in_clp, in_clp)));
    input += 4;
  }
  //Ave
  num1 = simde_mm_dp_ps(num0, recp1, 0xFF);
  temp = simde_mm_cvtsi128_si32(simde_mm_cvttps_epi32(num1));

  return temp;
}

int32_t interference_power(int32_t *input, uint32_t length)
{

  uint32_t i;
  int32_t temp;

  simde__m128i in, in_clp, i16_min;
  simde__m128i num0, num1, num2, num3;
  simde__m128  num4, num5, num6;
  simde__m128  recp1;

  //init
  i16_min = simde_mm_set1_epi16(SHRT_MIN);
  num5 = simde_mm_setzero_ps();
  recp1 = simde_mm_rcp_ps(simde_mm_cvtepi32_ps(simde_mm_set1_epi32(length>>2)));// 1/n, n= length/4
  //Acc
  for (i = 0; i < (length >> 2); i++) {
    in = simde_mm_loadu_si128((simde__m128i *)input);
    in_clp = simde_mm_subs_epi16(in, simde_mm_cmpeq_epi16(in, i16_min));           //if in=SHRT_MIN, in+1, else in
    num0 = simde_mm_cvtepi16_epi32(in_clp);                                   //lower 2 complex [0], [1]
    num1 = simde_mm_cvtepi16_epi32(simde_mm_shuffle_epi32(in_clp, 0x4E));          //upper 2 complex [2], [3]
    num2 = simde_mm_srai_epi32(simde_mm_add_epi32(num0, num1), 0x01);              //average A=complex( [0] + [2] ) / 2, B=complex( [1] + [3] ) / 2 
    num3 = simde_mm_sub_epi32(num2, simde_mm_shuffle_epi32(num2, 0x4E));           //complexA-complexB, B-A
    num4 = simde_mm_dp_ps(simde_mm_cvtepi32_ps(num3), simde_mm_cvtepi32_ps(num3), 0x3F);//C = num3 lower complex power, C, C, C
    num5 = simde_mm_add_ps(num5, num4);                                       //Acc Cn, Cn, Cn, Cn, 
    input += 4;
  }
  //Interference ve
  num6 = simde_mm_mul_ps(num5, recp1); //Cn / n
  temp = simde_mm_cvtsi128_si32(simde_mm_cvttps_epi32(num6));

  return temp;
}

// Computes transmitter energy level
double compute_tx_energy_level(c16_t **txdata, int nb_antennas, int offset, int length, int n_trials)
{
  double txlev_sum = 0, atxlev[nb_antennas];
  for (int aa = 0; aa < nb_antennas; aa++) {
    atxlev[aa] = signal_energy((int32_t *)&txdata[aa][offset], length);

    txlev_sum += atxlev[aa];

    if (n_trials == 1)
      printf("txlev[%d] = %f (%f dB) txlev_sum %f\n", aa, atxlev[aa], 10 * log10(atxlev[aa]), txlev_sum);
  }
  return txlev_sum;
}

// Computes noise variance from the input transmit energy and SNR
double compute_noise_variance(double txlev_sum,
                              uint16_t ofdm_symbol_size,
                              int N_RB,
                              uint8_t precod_nbr_layers,
                              double SNR,
                              int n_trials)
{
  // Justification of division by precod_nbr_layers:
  // When the channel is the identity matrix, the results in terms of SNR should be almost equal for 2x2 and 4x4.
  double sigma_dB = 10 * log10(txlev_sum / precod_nbr_layers * ((double)ofdm_symbol_size / (12 * N_RB))) - SNR;
  double sigma = pow(10, sigma_dB / 10);

  if (n_trials == 1)
    printf("sigma %f (%f dB), txlev_sum %f (factor %f)\n",
           sigma,
           sigma_dB,
           10 * log10(txlev_sum),
           (double)(double)ofdm_symbol_size / (12 * N_RB));
  return sigma;
}
