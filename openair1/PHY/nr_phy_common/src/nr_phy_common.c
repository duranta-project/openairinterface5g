/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_phy_common.h"
#include "bits.h"
#include <complex.h>
#include "PHY/sse_intrin.h"
#include "PHY/impl_defs_top.h"
#ifdef __aarch64__
#define USE_128BIT
#endif

#define PEAK_DETECT_THRESHOLD 15
simde__m128i byte2m128i[256];
void init_byte2m128i(void)
{
  for (int s = 0; s < 256; s++) {
    byte2m128i[s] = simde_mm_insert_epi16(byte2m128i[s], (1 - 2 * (s & 1)), 0);
    byte2m128i[s] = simde_mm_insert_epi16(byte2m128i[s], (1 - 2 * ((s >> 1) & 1)), 1);
    byte2m128i[s] = simde_mm_insert_epi16(byte2m128i[s], (1 - 2 * ((s >> 2) & 1)), 2);
    byte2m128i[s] = simde_mm_insert_epi16(byte2m128i[s], (1 - 2 * ((s >> 3) & 1)), 3);
    byte2m128i[s] = simde_mm_insert_epi16(byte2m128i[s], (1 - 2 * ((s >> 4) & 1)), 4);
    byte2m128i[s] = simde_mm_insert_epi16(byte2m128i[s], (1 - 2 * ((s >> 5) & 1)), 5);
    byte2m128i[s] = simde_mm_insert_epi16(byte2m128i[s], (1 - 2 * ((s >> 6) & 1)), 6);
    byte2m128i[s] = simde_mm_insert_epi16(byte2m128i[s], (1 - 2 * ((s >> 7) & 1)), 7);
  }
}

void init_delay_table(uint16_t ofdm_symbol_size,
                      int max_delay_comp,
                      int max_ofdm_symbol_size,
                      c16_t delay_table[][max_ofdm_symbol_size])
{
  for (int delay = -max_delay_comp; delay <= max_delay_comp; delay++) {
    for (int k = 0; k < ofdm_symbol_size; k++) {
      double complex delay_cexp = cexp(I * (2.0 * M_PI * k * delay / ofdm_symbol_size));
      delay_table[max_delay_comp + delay][k].r = (int16_t)round(256 * creal(delay_cexp));
      delay_table[max_delay_comp + delay][k].i = (int16_t)round(256 * cimag(delay_cexp));
    }
  }
}

static inline c16_t saturating_sub(c16_t a, c16_t b)
{
  c32_t tmp = {a.r - abs(b.r), a.i - abs(b.i)};
  tmp = (c32_t){min(tmp.r, INT16_MAX), min(tmp.i, INT16_MAX)};
  c16_t tmp2 = (c16_t){max(tmp.r, -INT16_MAX), max(tmp.i, -INT16_MAX)};
  return tmp2;
}

//----------------------------------------------------------------------------------------------
// QPSK
//----------------------------------------------------------------------------------------------
void nr_qpsk_llr(const c16_t *rxdataF_comp, int16_t *llr, uint32_t nb_re)
{
  const c16_t *rxF = rxdataF_comp;
  c16_t *llr32 = (c16_t *)llr;
  for (int i = 0; i < nb_re; i++) {
    llr32[i].r = rxF[i].r >> 4;
    llr32[i].i = rxF[i].i >> 4;
  }
}

//----------------------------------------------------------------------------------------------
// 16-QAM
//----------------------------------------------------------------------------------------------

void nr_16qam_llr(const c16_t *rxdataF_comp, const c16_t *ch_mag_in, int16_t *llr, uint32_t nb_re)
{
  simde__m256i *rxF_256 = (simde__m256i *)rxdataF_comp;
  simde__m256i *ch_mag256 = (simde__m256i *)ch_mag_in;
  int64_t *llr_64 = (int64_t *)llr;

#ifndef USE_128BIT
  for (int i = 0; i < (nb_re >> 3); i++) {
    // registers of even index in xmm0-> |y_R|, registers of odd index in xmm0-> |y_I|
    simde__m256i xmm0 = protected_abs256(*rxF_256);
    // registers of even index in xmm0-> |y_R|-|h|^2, registers of odd index in xmm0-> |y_I|-|h|^2
    xmm0 = simde_mm256_subs_epi16(*ch_mag256, xmm0);

    simde__m256i xmm1 = simde_mm256_unpacklo_epi32(*rxF_256, xmm0);
    simde__m256i xmm2 = simde_mm256_unpackhi_epi32(*rxF_256, xmm0);

    // xmm1 |1st 2ed 3rd 4th  9th 10th 13rd 14th|
    // xmm2 |5th 6th 7th 8th 11st 12ed 15th 16th|

    *llr_64++ = simde_mm256_extract_epi64(xmm1, 0);
    *llr_64++ = simde_mm256_extract_epi64(xmm1, 1);
    *llr_64++ = simde_mm256_extract_epi64(xmm2, 0);
    *llr_64++ = simde_mm256_extract_epi64(xmm2, 1);
    *llr_64++ = simde_mm256_extract_epi64(xmm1, 2);
    *llr_64++ = simde_mm256_extract_epi64(xmm1, 3);
    *llr_64++ = simde_mm256_extract_epi64(xmm2, 2);
    *llr_64++ = simde_mm256_extract_epi64(xmm2, 3);
    rxF_256++;
    ch_mag256++;
  }

  nb_re &= 0x7;
#endif

  simde__m128i *rxF_128 = (simde__m128i *)rxF_256;
  simde__m128i *ch_mag_128 = (simde__m128i *)ch_mag256;
  simde__m128i *llr_128 = (simde__m128i *)llr_64;

  // Each iteration does 4 RE (gives 16 16bit-llrs)
  for (int i = 0; i < (nb_re >> 2); i++) {
    // registers of even index in xmm0-> |y_R|, registers of odd index in xmm0-> |y_I|
    simde__m128i xmm0 = protected_abs128(*rxF_128);
    // registers of even index in xmm0-> |y_R|-|h|^2, registers of odd index in xmm0-> |y_I|-|h|^2
    xmm0 = simde_mm_subs_epi16(*ch_mag_128, xmm0);

    llr_128[0] = simde_mm_unpacklo_epi32(*rxF_128, xmm0); // llr128[0] contains the llrs of the 1st,2nd,5th and 6th REs
    llr_128[1] = simde_mm_unpackhi_epi32(*rxF_128, xmm0); // llr128[1] contains the llrs of the 3rd, 4th, 7th and 8th REs
    llr_128 += 2;
    rxF_128++;
    ch_mag_128++;
  }


  nb_re &= 0x3;
  c16_t *rxDataF = (c16_t *)rxF_128;
  c16_t *ch_mag = (c16_t *)ch_mag_128;
  c16_t *llr_tail = (c16_t *)llr_128;
  for (uint i = 0U; i < nb_re; i++) {
    c16_t tmp = *rxDataF++;
    *llr_tail++ = tmp;
    *llr_tail++ = saturating_sub(*ch_mag++, tmp);
  }
}

//----------------------------------------------------------------------------------------------
// 64-QAM
//----------------------------------------------------------------------------------------------

void nr_64qam_llr(const c16_t *rxdataF_comp, const c16_t *ch_mag, const c16_t *ch_mag2, int16_t *llr, uint32_t nb_re)
{
  simde__m256i *rxF = (simde__m256i *)rxdataF_comp;

  simde__m256i *ch_maga = (simde__m256i *)ch_mag;
  simde__m256i *ch_magb = (simde__m256i *)ch_mag2;

  int32_t *llr_32 = (int32_t *)llr;
#ifndef USE_128BIT
  for (int i = 0; i < (nb_re >> 3); i++) {
    simde__m256i xmm0 = simde_mm256_loadu_si256(rxF);
    // registers of even index in xmm0-> |y_R|, registers of odd index in xmm0-> |y_I|
    simde__m256i xmm1 = protected_abs256(xmm0);
    // registers of even index in xmm0-> |y_R|-|h|^2, registers of odd index in xmm0-> |y_I|-|h|^2
    xmm1 = simde_mm256_subs_epi16(*ch_maga, xmm1);
    simde__m256i xmm2 = protected_abs256(xmm1);
    xmm2 = simde_mm256_subs_epi16(*ch_magb, xmm2);
    // xmm0 |1st 4th 7th 10th 13th 16th 19th 22ed|
    // xmm1 |2ed 5th 8th 11th 14th 17th 20th 23rd|
    // xmm2 |3rd 6th 9th 12th 15th 18th 21st 24th|

    *llr_32++ = simde_mm256_extract_epi32(xmm0, 0);
    *llr_32++ = simde_mm256_extract_epi32(xmm1, 0);
    *llr_32++ = simde_mm256_extract_epi32(xmm2, 0);

    *llr_32++ = simde_mm256_extract_epi32(xmm0, 1);
    *llr_32++ = simde_mm256_extract_epi32(xmm1, 1);
    *llr_32++ = simde_mm256_extract_epi32(xmm2, 1);

    *llr_32++ = simde_mm256_extract_epi32(xmm0, 2);
    *llr_32++ = simde_mm256_extract_epi32(xmm1, 2);
    *llr_32++ = simde_mm256_extract_epi32(xmm2, 2);

    *llr_32++ = simde_mm256_extract_epi32(xmm0, 3);
    *llr_32++ = simde_mm256_extract_epi32(xmm1, 3);
    *llr_32++ = simde_mm256_extract_epi32(xmm2, 3);

    *llr_32++ = simde_mm256_extract_epi32(xmm0, 4);
    *llr_32++ = simde_mm256_extract_epi32(xmm1, 4);
    *llr_32++ = simde_mm256_extract_epi32(xmm2, 4);

    *llr_32++ = simde_mm256_extract_epi32(xmm0, 5);
    *llr_32++ = simde_mm256_extract_epi32(xmm1, 5);
    *llr_32++ = simde_mm256_extract_epi32(xmm2, 5);

    *llr_32++ = simde_mm256_extract_epi32(xmm0, 6);
    *llr_32++ = simde_mm256_extract_epi32(xmm1, 6);
    *llr_32++ = simde_mm256_extract_epi32(xmm2, 6);

    *llr_32++ = simde_mm256_extract_epi32(xmm0, 7);
    *llr_32++ = simde_mm256_extract_epi32(xmm1, 7);
    *llr_32++ = simde_mm256_extract_epi32(xmm2, 7);
    rxF++;
    ch_maga++;
    ch_magb++;
  }

  nb_re &= 0x7;
#endif

  simde__m128i *rxF_128 = (simde__m128i *)rxF;
  simde__m128i *ch_mag_128 = (simde__m128i *)ch_maga;
  simde__m128i *ch_magb_128 = (simde__m128i *)ch_magb;
  // Each iteration does 4 RE (gives 24 16bit-llrs)
  for (int i = 0; i < (nb_re >> 2); i++) {
    simde__m128i xmm0, xmm1, xmm2;
    xmm0 = *rxF_128;
    xmm1 = protected_abs128(xmm0);
    xmm1 = simde_mm_subs_epi16(*ch_mag_128, xmm1);
    xmm2 = protected_abs128(xmm1);
    xmm2 = simde_mm_subs_epi16(*ch_magb_128, xmm2);

    *llr_32++ = simde_mm_extract_epi32(xmm0, 0);
    *llr_32++ = simde_mm_extract_epi32(xmm1, 0);
    *llr_32++ = simde_mm_extract_epi32(xmm2, 0);
    *llr_32++ = simde_mm_extract_epi32(xmm0, 1);
    *llr_32++ = simde_mm_extract_epi32(xmm1, 1);
    *llr_32++ = simde_mm_extract_epi32(xmm2, 1);
    *llr_32++ = simde_mm_extract_epi32(xmm0, 2);
    *llr_32++ = simde_mm_extract_epi32(xmm1, 2);
    *llr_32++ = simde_mm_extract_epi32(xmm2, 2);
    *llr_32++ = simde_mm_extract_epi32(xmm0, 3);
    *llr_32++ = simde_mm_extract_epi32(xmm1, 3);
    *llr_32++ = simde_mm_extract_epi32(xmm2, 3);
    rxF_128++;
    ch_mag_128++;
    ch_magb_128++;
  }

  nb_re &= 0x3;

  c16_t *rxDataF = (c16_t *)rxF_128;
  c16_t *ch_mag_tail = (c16_t *)ch_mag_128;
  c16_t *ch_magb_tail = (c16_t *)ch_magb_128;
  c16_t *llr_tail = (c16_t *)llr_32;
  for (int i = 0; i < nb_re; i++) {
    *llr_tail++ = *rxDataF;
    c16_t tmp = saturating_sub(*ch_mag_tail++, *rxDataF++);
    *llr_tail++ = tmp;
    *llr_tail++ = saturating_sub(*ch_magb_tail++, tmp);
  }
}

void nr_256qam_llr(const c16_t *rxdataF_comp, const c16_t *ch_mag, const c16_t *ch_mag2, const c16_t *ch_mag3, int16_t *llr, uint32_t nb_re)
{
  simde__m256i *rxF_256 = (simde__m256i *)rxdataF_comp;
  simde__m256i *llr256 = (simde__m256i *)llr;

  simde__m256i *ch_maga = (simde__m256i *)ch_mag;
  simde__m256i *ch_magb = (simde__m256i *)ch_mag2;
  simde__m256i *ch_magc = (simde__m256i *)ch_mag3;
#ifndef USE_128BIT
  for (int i = 0; i < (nb_re >> 3); i++) {
    // registers of even index in xmm0-> |y_R|, registers of odd index in xmm0-> |y_I|
    simde__m256i xmm0 = protected_abs256(*rxF_256);
    // registers of even index in xmm0-> |y_R|-|h|^2, registers of odd index in xmm0-> |y_I|-|h|^2
    xmm0 = simde_mm256_subs_epi16(*ch_maga, xmm0);
    //  xmmtmpD2 contains 16 LLRs
    simde__m256i xmm1 = protected_abs256(xmm0);
    xmm1 = simde_mm256_subs_epi16(*ch_magb, xmm1); // contains 16 LLRs
    simde__m256i xmm2 = protected_abs256(xmm1);
    xmm2 = simde_mm256_subs_epi16(*ch_magc, xmm2); // contains 16 LLRs
    // rxF[i] A0 A1 A2 A3 A4 A5 A6 A7 bits 7,6
    // xmm0   B0 B1 B2 B3 B4 B5 B6 B7 bits 5,4
    // xmm1   C0 C1 C2 C3 C4 C5 C6 C7 bits 3,2
    // xmm2   D0 D1 D2 D3 D4 D5 D6 D7 bits 1,0
    simde__m256i xmm3 = simde_mm256_unpacklo_epi32(*rxF_256, xmm0); // A0 B0 A1 B1 A4 B4 A5 B5
    simde__m256i xmm4 = simde_mm256_unpackhi_epi32(*rxF_256, xmm0); // A2 B2 A3 B3 A6 B6 A7 B7
    simde__m256i xmm5 = simde_mm256_unpacklo_epi32(xmm1, xmm2); // C0 D0 C1 D1 C4 D4 C5 D5
    simde__m256i xmm6 = simde_mm256_unpackhi_epi32(xmm1, xmm2); // C2 D2 C3 D3 C6 D6 C7 D7

    xmm0 = simde_mm256_unpacklo_epi64(xmm3, xmm5); // A0 B0 C0 D0 A4 B4 C4 D4
    xmm1 = simde_mm256_unpackhi_epi64(xmm3, xmm5); // A1 B1 C1 D1 A5 B5 C5 D5
    xmm2 = simde_mm256_unpacklo_epi64(xmm4, xmm6); // A2 B2 C2 D2 A6 B6 C6 D6
    xmm3 = simde_mm256_unpackhi_epi64(xmm4, xmm6); // A3 B3 C3 D3 A7 B7 C7 D7
    *llr256++ = simde_mm256_permute2x128_si256(xmm0, xmm1, 0x20); // A0 B0 C0 D0 A1 B1 C1 D1
    *llr256++ = simde_mm256_permute2x128_si256(xmm2, xmm3, 0x20); // A2 B2 C2 D2 A3 B3 C3 D3
    *llr256++ = simde_mm256_permute2x128_si256(xmm0, xmm1, 0x31); // A4 B4 C4 D4 A5 B5 C5 D5
    *llr256++ = simde_mm256_permute2x128_si256(xmm2, xmm3, 0x31); // A6 B6 C6 D6 A7 B7 C7 D7
    ch_magc++;
    ch_magb++;
    ch_maga++;
    rxF_256++;
  }

  nb_re &= 0x7;
#endif

  simde__m128i *rxF_128 = (simde__m128i *)rxF_256;
  simde__m128i *llr_128 = (simde__m128i *)llr256;

  simde__m128i *ch_maga_128 = (simde__m128i *)ch_maga;
  simde__m128i *ch_magb_128 = (simde__m128i *)ch_magb;
  simde__m128i *ch_magc_128 = (simde__m128i *)ch_magc;
  for (int i = 0; i < (nb_re >> 2); i++) {
    simde__m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6;
    // registers of even index in xmm0-> |y_R|, registers of odd index in xmm0-> |y_I|
    xmm0 = protected_abs128(*rxF_128);
    // registers of even index in xmm0-> |y_R|-|h|^2, registers of odd index in xmm0-> |y_I|-|h|^2
    xmm0 = simde_mm_subs_epi16(*ch_maga_128, xmm0);
    xmm1 = protected_abs128(xmm0);
    xmm1 = simde_mm_subs_epi16(*ch_magb_128, xmm1); // contains 8 LLRs
    xmm2 = protected_abs128(xmm1);
    xmm2 = simde_mm_subs_epi16(*ch_magc_128, xmm2); // contains 8 LLRs
    // rxF[i] A0 A1 A2 A3
    // xmm0   B0 B1 B2 B3
    // xmm1   C0 C1 C2 C3
    // xmm2   D0 D1 D2 D3
    xmm3 = simde_mm_unpacklo_epi32(*rxF_128, xmm0); // A0 B0 A1 B1
    xmm4 = simde_mm_unpackhi_epi32(*rxF_128, xmm0); // A2 B2 A3 B3
    xmm5 = simde_mm_unpacklo_epi32(xmm1, xmm2); // C0 D0 C1 D1
    xmm6 = simde_mm_unpackhi_epi32(xmm1, xmm2); // C2 D2 C3 D3

    *llr_128++ = simde_mm_unpacklo_epi64(xmm3, xmm5); // A0 B0 C0 D0
    *llr_128++ = simde_mm_unpackhi_epi64(xmm3, xmm5); // A1 B1 C1 D1
    *llr_128++ = simde_mm_unpacklo_epi64(xmm4, xmm6); // A2 B2 C2 D2
    *llr_128++ = simde_mm_unpackhi_epi64(xmm4, xmm6); // A3 B3 C3 D3

    rxF_128++;
    ch_maga_128++;
    ch_magb_128++;
    ch_magc_128++;
  }

  nb_re &= 0x3;
  c16_t *rxDataF = (c16_t *)rxF_128;
  c16_t *ch_mag_tail = (c16_t *)ch_maga_128;
  c16_t *ch_magb_tail = (c16_t *)ch_magb_128;
  c16_t *ch_magc_tail = (c16_t *)ch_magc_128;
  c16_t *llr_tail = (c16_t *)llr_128;
  for (int i = 0; i < nb_re; i++) {
    c16_t tmp = *rxDataF++;
    *llr_tail++ = tmp;
    c16_t tmp1 = saturating_sub(*ch_mag_tail++, tmp);
    *llr_tail++ = tmp1;
    c16_t tmp2 = saturating_sub(*ch_magb_tail++, tmp1);
    *llr_tail++ = tmp2;
    *llr_tail++ = saturating_sub(*ch_magc_tail++, tmp2);
  }
}

void freq2time(uint16_t ofdm_symbol_size, int16_t *freq_signal, int16_t *time_signal)
{
  const idft_size_idx_t idft_size = get_idft(ofdm_symbol_size);
  idft(idft_size, freq_signal, time_signal, 1);
}

void nr_est_delay(int ofdm_symbol_size, const c16_t *ls_est, c16_t *ch_estimates_time, delay_t *delay)
{
  idft(get_idft(ofdm_symbol_size), (int16_t *)ls_est, (int16_t *)ch_estimates_time, 1);

  int max_pos = delay->delay_max_pos;
  int max_val = delay->delay_max_val;
  const int sync_pos = 0;

  uint64_t mean_val = 0;
  for (int i = 0; i < ofdm_symbol_size; i++) {
    int temp = c16amp2(ch_estimates_time[i]) >> 1;
    mean_val += temp;
    if (temp > max_val) {
      max_pos = i;
      max_val = temp;
    }
  }
  mean_val /= ofdm_symbol_size;

  if (max_pos > ofdm_symbol_size / 2)
    max_pos = max_pos - ofdm_symbol_size;

  delay->delay_max_pos = max_pos;
  delay->delay_max_val = max_val;

  // The peak in general is quite clear. It only gives a small peak when the noise is high, generally obtaining an incorrect
  // estimated delay, and causing the delay compensation to worsen the result instead of improving it. After analyzing several
  // peaks, and doing many tests, a PEAK_DETECT_THRESHOLD = 15 is an adequate value, to apply delay compensation only when there is
  // clearly a peak
  delay->valid = mean_val > 0 && max_val / mean_val > PEAK_DETECT_THRESHOLD;
  delay->est_delay = delay->valid ? max_pos - sync_pos : 0;
}

unsigned int nr_get_tx_amp(int power_dBm, int power_max_dBm, int total_nb_rb, int nb_rb)
{
  // assume power at AMP is 20dBm
  // if gain = 20 (power == 40)
  int gain_dB = power_dBm - power_max_dBm;
  double gain_lin;

  gain_lin = pow(10, .1 * gain_dB);
  if ((nb_rb > 0) && (nb_rb <= total_nb_rb)) {
    return ((int)(AMP * sqrt(gain_lin * total_nb_rb / (double)nb_rb)));
  } else {
    LOG_E(PHY, "Illegal nb_rb/N_RB_UL combination (%d/%d)\n", nb_rb, total_nb_rb);
    // mac_xface->macphy_exit("");
  }
  return (0);
}

// compute average channel_level on each antenna
void nr_channel_level(const int symbol,
                      const int size_est,
                      const c16_t ch_estimates_ext[][size_est],
                      const int nb_rx,
                      int32_t avg[nb_rx],
                      const uint32_t len)
{
  for (int aarx = 0; aarx < nb_rx; aarx++) {
    // compute average squared module
    avg[aarx] = signal_energy_nodc(ch_estimates_ext[aarx] + symbol * len, len);
    LOG_D(PHY, "Channel level: %d\n", avg[aarx]);
  }
}

void nr_fo_compensation(double fo_Hz, int samples_per_ms, int sample_offset, const c16_t *rxdata_in, c16_t *rxdata_out, int size)
{
  const double phase_inc = -fo_Hz / (samples_per_ms * 1000);
  double phase = sample_offset * phase_inc;
  phase -= (int)phase;
#if 1
  // The bottleneck is the calculation of the complex rotation values using get_sin_cos().
  // This code path does not compute these values for the complete OFDM symbol, but only for a smaller CHUNK size.
  // After applying the rotation to a CHUNK size of the output, these rotation values are efficiently rotated further by `rot_vec`.
  // Unfortunately, this propagates small errors from one chunk to the next.
  // Therefore, there is a tradeoff between speed (better with small CHUNK sizes) and accuracy (better with large CHUNK sizes).
#define CHUNK 128
  c16_t rot[CHUNK] __attribute__((aligned(32)));
  for (int i = 0; i < CHUNK; i++) {
    rot[i] = get_sin_cos(phase);
    phase += phase_inc;
  }
  const c16_t rot_vec = get_sin_cos(CHUNK * phase_inc);
  while (size > CHUNK) {
    mult_complex_vectors(rxdata_in, rot, rxdata_out, CHUNK, 14);
    rotate_cpx_vector(rot, rot_vec, rot, CHUNK, 14);
    rxdata_in += CHUNK;
    rxdata_out += CHUNK;
    size -= CHUNK;
  }
  mult_complex_vectors(rxdata_in, rot, rxdata_out, size, 14);
#else
  // This code path computes the complex rotation values for the complete OFDM symbol using get_sin_cos().
  // This is more accurate, but also slower than the code path above.
  c16_t rot[size] __attribute__((aligned(32)));
  for (int i = 0; i < size; i++) {
    rot[i] = get_sin_cos(phase);
    phase += phase_inc;
  }
  mult_complex_vectors(rxdata_in, rot, rxdata_out, size, 14);
#endif
}

/*!
* Setting the first subcarrier
* 3GPP TS 38.211 sections 7.4.3.1 and 4.4.4.2
* for FR1 offsetToPointA and k_SSB are expressed in terms of 15 kHz SCS
* for FR2 offsetToPointA is expressed in terms of 60 kHz SCS and k_SSB expressed in terms of the SCS provided
* by the higher-layer parameter subCarrierSpacingCommon
*/
int nr_get_ssb_start_sc(int scs, int ssb_offset_point_a, int ssb_sco, frequency_range_t freq_range)
{
  const int prb_offset =
      (freq_range == FR1) ? ssb_offset_point_a >> scs : ssb_offset_point_a >> (scs - 2);
  const int sc_offset =
      (freq_range == FR1) ? ssb_sco >> scs : ssb_sco;

  int ssb_start_subcarrier = (12 * prb_offset + sc_offset);

  LOG_D(NR_PHY, "prb_offset:%d, ssb_subcarrier_offset:%d,scs :%d, Fr:%d, ssb_start_subcarrier:%d\n",
                        prb_offset, ssb_sco, scs, freq_range, ssb_start_subcarrier);

  return ssb_start_subcarrier;
}

void get_s1_s2(int *s1, int *s2, int rbsize, int nr_symbols, int start_symb, uint16_t dmrs_symb_pos, uint16_t ptrs_symb, int n_ptrs)
{
  // Calculate s1: total number of non-DMRS REs in allocation
  *s1 = rbsize * NR_NB_SC_PER_RB * (nr_symbols - get_num_dmrs(dmrs_symb_pos));

  // Calculate s2: number of non-DMRS REs after first DMRS symbol
  // __builtin_ctz returns the index of the first set bit
  int first_dmrs_symbol = __builtin_ctz(dmrs_symb_pos);
  // mask with everything from (first_dmrs_symbol + 1) to the end
  uint32_t range_mask = ((1U << nr_symbols) - 1) << start_symb;
  uint32_t post_dmrs_mask = range_mask & ~((1U << (first_dmrs_symbol + 1)) - 1);
  // number of non-DMRS REs bits in that post-DMRS range
  uint32_t non_dmrs_bits = post_dmrs_mask & ~dmrs_symb_pos;
  int num_non_dmrs_symbols = __builtin_popcount(non_dmrs_bits);
  *s2 = num_non_dmrs_symbols * rbsize * NR_NB_SC_PER_RB;

  if (ptrs_symb) {
    // for any OFDM symbol that does not carry DMRS of the PUSCH, M_UCI = M_PUSCH − M_PTRS
    uint32_t non_dmrs_ptrs_mask = ptrs_symb & ~dmrs_symb_pos;
    int ptrs_symb_in_alloc = __builtin_popcount(non_dmrs_ptrs_mask);
    *s1 -= (ptrs_symb_in_alloc * n_ptrs);
    uint32_t ptrs_in_post_window = ptrs_symb & post_dmrs_mask;
    int num_ptrs_symbols_s2 = __builtin_popcount(ptrs_in_post_window);
    *s2 -= (num_ptrs_symbols_s2 * n_ptrs);
  }
}

/*
 * This function gets the CRC size of UCI according to 6.3.1.2.1 of 38.212
 */
static int get_crc_uci(const uint32_t ouci)
{
  int L = 0;
  if (ouci > 19) {
    L = 11;
  } else if (ouci > 11) {
    L = 6;
  } else {
    L = 0;
  }
  return L;
}

uint32_t get_Qd(const uint32_t ouci,
                double beta,
                double alpha,
                const uint32_t eff_bits,
                const uint32_t s1,
                const uint32_t s2,
                const uint32_t sub)
{
  // as described in section 6.3.2.4.1 of 38.212
  if (ouci == 0)
    return 0;
  uint32_t first_term = ceil(((double)ouci + get_crc_uci(ouci)) * (double)beta * s1 / eff_bits);
  uint32_t second_term = ceil(alpha * s2) - sub;
  return (first_term < second_term) ? first_term : second_term;
}

double get_alpha_scaling_value(uint8_t alpha_scaling)
{
  switch (alpha_scaling) {
    case 0:
      return 0.5;
    case 1:
      return 0.65;
    case 2:
      return 0.8;
    case 3:
      return 1.0;
    default:
      AssertFatal(false, "Invalid alpha_scaling value %d, valid range is 0-3", alpha_scaling);
      return 1.0;
  }
}

// Function to lookup beta offset value from Table 9.3-1 in TS 38.213
double get_beta_offset_harq_ack(uint8_t beta_offset_index)
{
  static const double beta_offset_values[21] = {
      1.000, // Index 0
      2.000, // Index 1
      2.500, // Index 2
      3.125, // Index 3
      4.000, // Index 4
      5.000, // Index 5
      6.250, // Index 6
      8.000, // Index 7
      10.000, // Index 8
      12.625, // Index 9
      15.875, // Index 10
      20.000, // Index 11
      31.000, // Index 12
      50.000, // Index 13
      80.000, // Index 14
      126.000, // Index 15
      0.6, // Index 16
      0.4, // Index 17
      0.2, // Index 18
      0.1, // Index 19
      0.05, // Index 20
  };
  if (beta_offset_index > 20) {
    LOG_E(PHY, "Invalid beta_offset_index %d, using default value\n", beta_offset_index);
    return 20.000; // Default value using index 11
  }
  return beta_offset_values[beta_offset_index];
}

// Function to lookup beta offset value from Table 9.3-2 in TS 38.213
double get_beta_offset_csi(const uint8_t beta_offset_idx)
{
  static const double beta_offset_values[19] = {1.125,
                                                1.250,
                                                1.375,
                                                1.625,
                                                1.750,
                                                2.000,
                                                2.250,
                                                2.500,
                                                2.875,
                                                3.125,
                                                3.500,
                                                4.000,
                                                5.000,
                                                6.250,
                                                8.000,
                                                10.000,
                                                12.625,
                                                15.875,
                                                20.000};

  if (beta_offset_idx >= sizeofArray(beta_offset_values)) {
    LOG_E(PHY, "Invalid beta_offset_index %d, using default value\n", beta_offset_idx);
    return beta_offset_values[9];
  }

  return beta_offset_values[beta_offset_idx];
}
