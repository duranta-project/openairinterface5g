/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Frequency response of rays (TR 38.901 eq. 7.5-22, single polarization) through transmit
 * beams: each ray is projected onto each beam once, so applying the result costs
 * nb_rx * nb_beams per RE whatever the array size. GCS of TR 38.901 §7.1, radians, both
 * arrays boresight +x.
 */

#ifndef BEAM_CHANNEL_H
#define BEAM_CHANNEL_H

#include "common/platform_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cd_t gain; ///< amplitude and phase, antenna patterns excluded
  double delay_s;
  double aod; ///< azimuth of departure
  double zod; ///< zenith of departure
  double aoa; ///< azimuth of arrival
  double zoa; ///< zenith of arrival
  double doppler_hz;
} channel_path_t;

typedef enum {
  ANTENNA_PATTERN_ISOTROPIC,
  ANTENNA_PATTERN_TR38901, ///< TR 38.901 Table 7.3-1, field amplitude (8 dBi peak)
} antenna_pattern_t;

typedef struct {
  int num_elements;
  const double (*pos)[3]; ///< element positions in wavelengths
  antenna_pattern_t pattern;
} antenna_array_t;

/// Element positions of a uniform planar array in the y-z plane (TR 38.901 §7.3): element
/// e = row * cols + col at (0, col * dh, row * dv), spacings in wavelengths.
void antenna_array_upa_positions(int rows, int cols, double dv, double dh, double (*pos)[3]);

/// G[r][b][k] = sum_p gain_p a_rx,r(p) beta_p,b exp(-j 2 pi f_k delay_p),
/// beta_p,b = F_tx(p) sum_e w[b][e] exp(j 2 pi r_tx(p) . d_e), f_k = f0 + k df (offset from the
/// carrier). weights: [num_beams][tx->num_elements]. G: [rx->num_elements][num_beams][num_freq].
/// Consecutive paths with equal delays (the rays of a cluster) are summed before the frequency loop.
void beam_channel_response(const channel_path_t *paths,
                           int num_paths,
                           const antenna_array_t *tx,
                           const cd_t *weights,
                           int num_beams,
                           const antenna_array_t *rx,
                           double f0_hz,
                           double df_hz,
                           int num_freq,
                           cd_t *G);

#ifdef __cplusplus
}
#endif

#endif
