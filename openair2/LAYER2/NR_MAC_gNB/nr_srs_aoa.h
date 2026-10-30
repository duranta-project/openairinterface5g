/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Pure-math helper: phase-slope azimuth AoA estimation from the SRS per-antenna
 * channel (SCF 222.10.04 nfapi_nr_srs_normalized_channel_iq_matrix_t). Header-only
 * and self-contained (no MAC/RRC/logging dependencies) on purpose, so the exact
 * algorithm used by the live path (gNB_scheduler_srs.c / handle_nr_srs_measurements)
 * is directly unit-testable with synthetic channel matrices at known angles -
 * see openair2/LAYER2/NR_MAC_gNB/tests/test_nr_srs_aoa.c.
 */

#ifndef NR_SRS_AOA_H
#define NR_SRS_AOA_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "common/platform_types.h"
#include "nfapi/open-nFAPI/nfapi/public_inc/nfapi_nr_interface_scf.h"

/* Phase-slope azimuth AoA (deg) from the SRS per-antenna channel.
 *
 * The gNB receives SRS on its own antenna array (aperture = num_gnb_antenna_elements), so this
 * estimates the uplink AoA directly at the node that selects the beam - no UE->gNB angle plumbing,
 * no non-standard UCI (SRS gNB-side). By reciprocity UL AoA ~= DL AoD, i.e. the direction to steer
 * the O-RU beam.
 *
 * For a half-wavelength ULA a plane wave at angle theta gives a constant phase step Delta_phi =
 * pi*sin(theta) between adjacent antennas, so theta = asin(Delta_phi/pi). Delta_phi is recovered
 * from the complex cross-correlation of adjacent antennas conj(H[g])*H[g+1], accumulated coherently
 * over all UE SRS ports and PRGs (and adjacent-antenna pairs), taking a single atan2 at the end
 * (never average raw angles - that is noise-fragile and wraps at +-pi).
 *
 * Channel matrix layout (SCF 222.10.04): array[uI*Ng*Np + gI*Np + pI], uI=UE SRS port,
 * gI=gNB antenna, pI=PRG. We correlate adjacent gI for each (uI, pI).
 *
 * Returns azimuth in [-90,90] deg, or NAN if no aperture (Ng<2) / no signal. LOS-only physics:
 * in multipath the slope reflects a power-weighted dominant direction, not a clean geometric angle. */
static inline double nr_srs_estimate_aoa(const nfapi_nr_srs_normalized_channel_iq_matrix_t *m)
{
  const uint16_t Ng = m->num_gnb_antenna_elements; // aperture
  const uint16_t Nu = m->num_ue_srs_ports;
  const uint16_t Np = m->num_prgs;
  if (Ng < 2 || Nu == 0 || Np == 0)
    return NAN; // no aperture -> AoA undefined

  int64_t re = 0, im = 0;
  const bool is16 = (m->normalized_iq_representation != 0); // 0: c8_t, 1: c16_t
  const c16_t *ch16 = (const c16_t *)m->channel_matrix;
  const c8_t *ch8 = (const c8_t *)m->channel_matrix;
  for (int uI = 0; uI < Nu; uI++) {
    for (int gI = 0; gI + 1 < Ng; gI++) { // adjacent antenna pairs (extends to >2 antennas)
      const int base_g = uI * Ng * Np + gI * Np;
      const int base_g1 = uI * Ng * Np + (gI + 1) * Np;
      for (int pI = 0; pI < Np; pI++) {
        int xr, xi, yr, yi;
        if (is16) {
          xr = ch16[base_g + pI].r;
          xi = ch16[base_g + pI].i;
          yr = ch16[base_g1 + pI].r;
          yi = ch16[base_g1 + pI].i;
        } else {
          xr = ch8[base_g + pI].r;
          xi = ch8[base_g + pI].i;
          yr = ch8[base_g1 + pI].r;
          yi = ch8[base_g1 + pI].i;
        }
        re += (int64_t)xr * yr + (int64_t)xi * yi; // Re{conj(x)*y}
        im += (int64_t)xr * yi - (int64_t)xi * yr; // Im{conj(x)*y}
      }
    }
  }
  if (re == 0 && im == 0)
    return NAN; // no signal / passthrough with no cross-antenna phase

  double s = atan2((double)im, (double)re) / M_PI; // = sin(theta) for d = lambda/2
  if (s > 1.0)
    s = 1.0;
  if (s < -1.0)
    s = -1.0;
  return asin(s) * 180.0 / M_PI; // [-90, 90]
}

/* Convert a raw nr_srs_estimate_aoa() result into the array-boresight frame, given the pointing
 * angle beam_deg of the beam whose weights the O-RU applied to the UL samples.
 *
 * The O-RU combines UL with the receive weight conj(w) (combine_ul_beam_fd()). The codebook steers
 * beam_deg with w[n] = exp(-j*n*pi*sin(beam_deg)), so conj(w) adds pi*sin(beam_deg) to the
 * per-antenna phase step the estimator measures: sin(raw) = sin(theta) + sin(beam_deg), modulo 2
 * because the phase step itself wraps at +-pi. Undo it in that same sine/phase domain, with the
 * same wrap. Subtracting degrees is only approximate, and breaks once the sum leaves [-1, 1)
 * (e.g. theta 30 deg on a 45 deg beam reads raw -52.5 deg). */
static inline double nr_srs_aoa_to_absolute(double raw_deg, double beam_deg)
{
  double s = sin(raw_deg * M_PI / 180.0) - sin(beam_deg * M_PI / 180.0);
  s -= 2.0 * floor((s + 1.0) / 2.0); // wrap to [-1, 1)
  return asin(s) * 180.0 / M_PI;
}

/* aoa_beam_angles and the uniform AoA sectors are indexed by beam position: the order of the
 * transmitted SSBs. UE_beam_index holds the FAPI beam index (the beam ID ssb_beams[pos] with
 * bf_method predefined), so convert with ssb_beam = beam_index_list (SSB index -> FAPI beam index, -1 for
 * SSBs not transmitted, filled by fill_beam_index_list()) over the num_beams transmitted SSBs. */
static inline int nr_srs_aoa_beam_pos(int beam_idx, const int16_t *ssb_beam, int ssb_len, int num_beams)
{
  for (int i = 0, pos = 0; i < ssb_len && pos < num_beams; i++) {
    if (ssb_beam[i] < 0)
      continue;
    if (ssb_beam[i] == beam_idx)
      return pos;
    pos++;
  }
  return -1;
}

static inline int nr_srs_aoa_beam_at(int pos, const int16_t *ssb_beam, int ssb_len)
{
  for (int i = 0; i < ssb_len; i++)
    if (ssb_beam[i] >= 0 && pos-- == 0)
      return ssb_beam[i];
  return -1;
}

#endif /* NR_SRS_AOA_H */
