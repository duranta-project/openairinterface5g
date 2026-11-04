/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <math.h>
#include "sim.h"

/* linear phase noise model */
void phase_noise(double ts, int16_t *InRe, int16_t *InIm)
{
  static uint64_t i = 0;
  double fd = 300; // 0.01*30000
  double phase = (double)(i * fd * ts);
  c16_t rot = get_sin_cos(phase);
  c16_t val = {.r = *InRe, .i = *InIm};
  val = c16mulShift(val, rot, 14);
  *InRe = val.r;
  *InIm = val.i;
  i++;
}

void add_noise(c16_t **rxdata,
               const double **r_re,
               const double **r_im,
               const double sigma,
               const int length,
               const int slot_offset,
               const double ts,
               const int delay,
               const uint16_t pdu_bit_map,
               const uint16_t ptrs_bit_map,
               const uint8_t nb_antennas_rx)
{
  for (int i = 0; i < length; i++) {
    for (int ap = 0; ap < nb_antennas_rx; ap++) {
      c16_t *rxd = &rxdata[ap][slot_offset + i + delay];
      rxd->r = r_re[ap][i] + sqrt(sigma / 2) * gaussZiggurat(0.0, 1.0); // convert to fixed point
      rxd->i = r_im[ap][i] + sqrt(sigma / 2) * gaussZiggurat(0.0, 1.0);
      /* Add phase noise if enabled */
      if (pdu_bit_map & ptrs_bit_map) {
        phase_noise(ts, &rxdata[ap][slot_offset + i + delay].r, &rxdata[ap][slot_offset + i + delay].i);
      }
    }
  }
}

void add_noise_float(c16_t **rxdata,
                     const float **r_re,
                     const float **r_im,
                     const float sigma,
                     const int length,
                     const int slot_offset,
                     const double ts,
                     const int delay,
                     bool apply_phase_noise,
                     const uint8_t nb_antennas_rx)
{
  float sqrt_sigma_div2 = sqrtf(sigma / 2.0f);

  for (int i = 0; i < length; i++) {
    for (int ap = 0; ap < nb_antennas_rx; ap++) {
      c16_t *rxd = &rxdata[ap][slot_offset + i + delay];
      // saturate like an ADC instead of wrapping
      rxd->r = (int16_t)fmaxf(-32768.0f, fminf(32767.0f, r_re[ap][i] + (gaussZiggurat(0.0, 1.0) * sqrt_sigma_div2)));
      rxd->i = (int16_t)fmaxf(-32768.0f, fminf(32767.0f, r_im[ap][i] + (gaussZiggurat(0.0, 1.0) * sqrt_sigma_div2)));

      if (apply_phase_noise) {
        phase_noise(ts, &rxd->r, &rxd->i);
      }
    }
  }
}
