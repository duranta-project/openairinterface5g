/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
#include "gnb_sib9.h"
#include <stdio.h>
#include <string.h>

static bool invalid(char *error, size_t size, const char *message)
{
  if (error && size)
    snprintf(error, size, "%s", message);
  return false;
}

static bool mode_value(const char *text, nr_sib9_mode_t *mode)
{
  if (!strcmp(text, "static"))
    *mode = NR_SIB9_STATIC;
  else if (!strcmp(text, "realtime"))
    *mode = NR_SIB9_REALTIME;
  else
    return false;
  return true;
}

bool nr_sib9_read_mode(configmodule_interface_t *cfg, const char *prefix, nr_sib9_mode_t *mode, char *error, size_t size)
{
  char *text = NULL;
  paramdef_t param = {"mode", "static or realtime", 0, .strptr = &text, .defstrval = "static", TYPE_STRING, 0};
  nr_sib9_mode_t result;
  if (!cfg || !mode || config_get(cfg, &param, 1, prefix) < 0 || !mode_value(text, &result))
    return invalid(error, size, "SIB9 DU mode must be static or realtime");
  *mode = result;
  return true;
}

bool nr_sib9_read_profile(configmodule_interface_t *cfg, const char *prefix, sib9_config_t *out, char *error, size_t size)
{
  char *source = NULL;
  int64_t utc = 0;
  int64_t dst = 0, leap = 0, offset = 0;
  paramdef_t params[] = {
      {"timeSource", "static or realtime", 0, .strptr = &source, .defstrval = "static", TYPE_STRING, 0},
      {"timeInfoUTC", "10 ms units since 1900-01-01", 0, .i64ptr = &utc, .defint64val = 0, TYPE_INT64, 0},
      {"dayLightSavingTime", "0..2; omitted by default", 0, .i64ptr = &dst, .defint64val = 0, TYPE_INT64, 0},
      {"leapSeconds", "-127..128; omitted by default", 0, .i64ptr = &leap, .defint64val = 0, TYPE_INT64, 0},
      {"localTimeOffset", "-63..64 in 15-minute units; omitted by default", 0, .i64ptr = &offset, .defint64val = 0, TYPE_INT64, 0},
  };
  if (!cfg || !out || config_get(cfg, params, sizeofArray(params), prefix) < 0)
    return invalid(error, size, "cannot read SIB9 profile");
  nr_sib9_mode_t mode;
  if (!mode_value(source, &mode))
    return invalid(error, size, "SIB9 timeSource must be static or realtime");
  const bool explicit_utc = config_isparamset(params, 1);
  if ((mode == NR_SIB9_STATIC) != explicit_utc)
    return invalid(error, size, "static SIB9 requires timeInfoUTC; realtime forbids a fixed timeInfoUTC");
  if (utc < 0 || utc > NR_SIB9_MAX_UTC || dst < 0 || dst > 2 || leap < -127 || leap > 128 || offset < -63 || offset > 64)
    return invalid(error, size, "SIB9 UTC or optional metadata outside its ASN.1 range");
  sib9_config_t result = {.time_info_utc = utc,
                          .day_light_saving_time_present = config_isparamset(params, 2),
                          .day_light_saving_time = dst,
                          .leap_seconds_present = config_isparamset(params, 3),
                          .leap_seconds = leap,
                          .local_time_offset_present = config_isparamset(params, 4),
                          .local_time_offset = offset};
  if (mode == NR_SIB9_REALTIME) {
    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) != 0 || !nr_sib9_time_to_utc(&now, 0, &result.time_info_utc))
      return invalid(error, size, "cannot construct SIB9 from CLOCK_REALTIME");
  }
  *out = result;
  return true;
}
