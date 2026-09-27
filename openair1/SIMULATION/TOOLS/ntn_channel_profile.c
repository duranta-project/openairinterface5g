/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/utils/LOG/log.h>
#include "ntn_channel_profile.h"

struct ntn_channel_profile_s {
  ntn_channel_update_t *updates;
  size_t count;
  bool has_epoch;
  double start_unix_s;
};

ntn_channel_profile_t *load_ntn_channel_profile(const char *path)
{
  FILE *file = fopen(path, "r");
  if (!file) {
    LOG_E(HW, "Cannot open NTN channel profile %s: %s\n", path, strerror(errno));
    return NULL;
  }

  ntn_channel_profile_t *profile = calloc(1, sizeof(*profile));
  if (!profile) {
    fclose(file);
    return NULL;
  }
  size_t capacity = 0;
  char line[256];
  unsigned int line_number = 0;
  while (fgets(line, sizeof(line), file)) {
    line_number++;
    char *cursor = line;
    while (*cursor == ' ' || *cursor == '\t')
      cursor++;
    if (strncmp(cursor, "# start_unix_s", 14) == 0) {
      char trailing;
      if (profile->has_epoch || profile->count != 0
          || sscanf(cursor, "# start_unix_s=%lf %c", &profile->start_unix_s, &trailing) != 1
          || !isfinite(profile->start_unix_s)) {
        LOG_E(HW, "Invalid NTN profile epoch in %s:%u\n", path, line_number);
        free_ntn_channel_profile(profile);
        fclose(file);
        return NULL;
      }
      profile->has_epoch = true;
      continue;
    }
    if (*cursor == '#' || *cursor == '\n' || *cursor == '\0')
      continue;

    ntn_channel_update_t update;
    char trailing;
    if (sscanf(cursor,
               "%lf , %lf , %lf , %lf %c",
               &update.time_s,
               &update.delay_ms,
               &update.dl_doppler_hz,
               &update.ul_doppler_hz,
               &trailing)
            != 4
        || !isfinite(update.time_s) || !isfinite(update.delay_ms) || !isfinite(update.dl_doppler_hz)
        || !isfinite(update.ul_doppler_hz) || update.time_s < 0.0 || update.delay_ms < 0.0
        || (profile->count > 0 && update.time_s <= profile->updates[profile->count - 1].time_s)) {
      LOG_E(HW, "Invalid NTN channel profile entry in %s:%u\n", path, line_number);
      free_ntn_channel_profile(profile);
      fclose(file);
      return NULL;
    }

    if (profile->count == capacity) {
      capacity = capacity == 0 ? 64 : capacity * 2;
      ntn_channel_update_t *updates = realloc(profile->updates, capacity * sizeof(*updates));
      if (!updates) {
        free_ntn_channel_profile(profile);
        fclose(file);
        return NULL;
      }
      profile->updates = updates;
    }
    profile->updates[profile->count++] = update;
  }
  fclose(file);

  if (profile->count == 0) {
    LOG_E(HW, "NTN channel profile %s contains no updates\n", path);
    free_ntn_channel_profile(profile);
    return NULL;
  }
  return profile;
}

bool get_ntn_channel_update(const ntn_channel_profile_t *profile, double time_s, ntn_channel_update_t *update)
{
  if (!profile || !update || !isfinite(time_s) || time_s < profile->updates[0].time_s
      || (profile->has_epoch && time_s > profile->updates[profile->count - 1].time_s))
    return false;

  size_t low = 0;
  size_t high = profile->count;
  while (low < high) {
    const size_t middle = low + (high - low) / 2;
    if (profile->updates[middle].time_s <= time_s)
      low = middle + 1;
    else
      high = middle;
  }
  *update = profile->updates[low - 1];
  return true;
}

bool get_ntn_channel_start_offset(const ntn_channel_profile_t *profile, double unix_time_s, double *offset_s)
{
  if (!profile || !offset_s || !isfinite(unix_time_s))
    return false;
  if (!profile->has_epoch) {
    *offset_s = 0.0;
    return true;
  }
  const double offset = unix_time_s - profile->start_unix_s;
  ntn_channel_update_t update;
  if (!get_ntn_channel_update(profile, offset, &update))
    return false;
  *offset_s = offset;
  return true;
}

bool get_ntn_channel_profile_epoch(const ntn_channel_profile_t *profile, double *unix_time_s)
{
  if (!profile || !unix_time_s || !profile->has_epoch)
    return false;
  *unix_time_s = profile->start_unix_s;
  return true;
}

size_t get_ntn_channel_profile_size(const ntn_channel_profile_t *profile)
{
  return profile ? profile->count : 0;
}

void free_ntn_channel_profile(ntn_channel_profile_t *profile)
{
  if (profile) {
    free(profile->updates);
    free(profile);
  }
}
