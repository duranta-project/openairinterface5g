/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _NTN_CHANNEL_PROFILE_H
#define _NTN_CHANNEL_PROFILE_H
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  double time_s;
  double delay_ms;
  double dl_doppler_hz;
  double ul_doppler_hz;
} ntn_channel_update_t;

typedef struct ntn_channel_profile_s ntn_channel_profile_t;

ntn_channel_profile_t *load_ntn_channel_profile(const char *path);
bool get_ntn_channel_update(const ntn_channel_profile_t *profile, double time_s, ntn_channel_update_t *update);
bool get_ntn_channel_start_offset(const ntn_channel_profile_t *profile, double unix_time_s, double *offset_s);
bool get_ntn_channel_profile_epoch(const ntn_channel_profile_t *profile, double *unix_time_s);
size_t get_ntn_channel_profile_size(const ntn_channel_profile_t *profile);
void free_ntn_channel_profile(ntn_channel_profile_t *profile);

#ifdef __cplusplus
}
#endif

#endif
