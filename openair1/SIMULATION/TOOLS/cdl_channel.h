/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * TR 38.901 §7.7.1 CDL as rays (beam_channel.h): 20 per cluster (Table 7.5-3, random
 * coupling) plus a LOS ray, single polarization, total power 1. Angles translated towards
 * the UE (§7.7.5.1, no scaling); rays rotate with the UE's Doppler.
 */

#ifndef CDL_CHANNEL_H
#define CDL_CHANNEL_H

#include <stdint.h>
#include "beam_channel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { CDL_A, CDL_B, CDL_C, CDL_D, CDL_E } cdl_model_t;

typedef struct {
  cdl_model_t model;
  double delay_spread_s;
  double carrier_hz;
  uint64_t seed;
} cdl_channel_config_t;

typedef struct cdl_channel cdl_channel_t;

cdl_channel_t *cdl_channel_create(const cdl_channel_config_t *cfg);
void cdl_channel_free(cdl_channel_t *ch);

/// Points the mean departure direction at (aod, zod), the direction of the UE seen from the
/// transmitter, and sets the UE velocity (m/s, GCS) the Doppler shifts are derived from.
void cdl_channel_set_geometry(cdl_channel_t *ch, double aod, double zod, const double velocity[3]);

/// Advances every ray's phase by its Doppler shift over dt_s.
void cdl_channel_advance(cdl_channel_t *ch, double dt_s);

/// The rays, cluster by cluster (the rays of a cluster are consecutive and share its delay).
/// Valid until the next call on ch.
int cdl_channel_paths(const cdl_channel_t *ch, const channel_path_t **paths);

#ifdef __cplusplus
}
#endif

#endif
