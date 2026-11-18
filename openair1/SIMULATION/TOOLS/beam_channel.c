/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include "assertions.h"
#include "beam_channel.h"

void antenna_array_upa_positions(int rows, int cols, double dv, double dh, double (*pos)[3])
{
  for (int r = 0; r < rows; r++) {
    for (int c = 0; c < cols; c++) {
      double *p = pos[r * cols + c];
      p[0] = 0;
      p[1] = c * dh;
      p[2] = r * dv;
    }
  }
}

// TR 38.901 Table 7.3-1, as a field amplitude
static double element_field(antenna_pattern_t pattern, double zenith, double azimuth)
{
  if (pattern == ANTENNA_PATTERN_ISOTROPIC)
    return 1.0;
  const double theta = zenith * 180 / M_PI;
  const double phi = remainder(azimuth * 180 / M_PI, 360);
  const double a_v = -fmin(12 * pow((theta - 90) / 65, 2), 30);
  const double a_h = -fmin(12 * pow(phi / 65, 2), 30);
  const double gain_db = -fmin(-(a_v + a_h), 30) + 8;
  return pow(10, gain_db / 20);
}

// Response of an array to a plane wave from (zenith, azimuth): F(dir) exp(j 2 pi r . d_e)
static double complex array_response(const antenna_array_t *array, int e, double zenith, double azimuth)
{
  const double r[3] = {sin(zenith) * cos(azimuth), sin(zenith) * sin(azimuth), cos(zenith)};
  const double *d = array->pos[e];
  const double phase = 2 * M_PI * (r[0] * d[0] + r[1] * d[1] + r[2] * d[2]);
  return element_field(array->pattern, zenith, azimuth) * cexp(I * phase);
}

void beam_channel_response(const channel_path_t *paths,
                           int num_paths,
                           const antenna_array_t *tx,
                           const cd_t *weights,
                           int num_beams,
                           const antenna_array_t *rx,
                           double f0_hz,
                           double df_hz,
                           int num_freq,
                           cd_t *G)
{
  const int nb_rx = rx->num_elements;
  const int ntx = tx->num_elements;
  const int per_tap = nb_rx * num_beams;

  // Coefficients of each distinct delay ("tap"): c[t][r][b] = sum over its paths of gain a_rx beta
  double complex *coef = calloc((size_t)num_paths * per_tap, sizeof(*coef));
  double *tap_delay = malloc(num_paths * sizeof(*tap_delay));
  AssertFatal(coef && tap_delay, "out of memory\n");
  int num_taps = 0;
  for (int p = 0; p < num_paths; p++) {
    const channel_path_t *path = &paths[p];
    if (num_taps == 0 || path->delay_s != tap_delay[num_taps - 1])
      tap_delay[num_taps++] = path->delay_s;
    double complex *c = coef + (size_t)(num_taps - 1) * per_tap;
    const double complex gain = path->gain.r + I * path->gain.i;
    double complex beta[num_beams];
    for (int b = 0; b < num_beams; b++)
      beta[b] = 0;
    for (int e = 0; e < ntx; e++) {
      const double complex a = array_response(tx, e, path->zod, path->aod);
      for (int b = 0; b < num_beams; b++) {
        const cd_t w = weights[b * ntx + e];
        beta[b] += (w.r + I * w.i) * a;
      }
    }
    for (int r = 0; r < nb_rx; r++) {
      const double complex g = gain * array_response(rx, r, path->zoa, path->aoa);
      for (int b = 0; b < num_beams; b++)
        c[r * num_beams + b] += g * beta[b];
    }
  }

  for (int i = 0; i < per_tap * num_freq; i++)
    G[i] = (cd_t){0};
  for (int t = 0; t < num_taps; t++) {
    const double complex *c = coef + (size_t)t * per_tap;
    // exp(-j 2 pi f_k tau) by recurrence over k; each step is exact to ~1e-16, so the drift over
    // a few thousand subcarriers stays far below the Q15 resolution of the channel's users
    const double complex step = cexp(-I * 2 * M_PI * df_hz * tap_delay[t]);
    double complex rot = cexp(-I * 2 * M_PI * f0_hz * tap_delay[t]);
    for (int k = 0; k < num_freq; k++) {
      for (int i = 0; i < per_tap; i++) {
        const double complex v = c[i] * rot;
        cd_t *g = &G[(size_t)i * num_freq + k];
        g->r += creal(v);
        g->i += cimag(v);
      }
      rot *= step;
    }
  }
  free(coef);
  free(tap_delay);
}
