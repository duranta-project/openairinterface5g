/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
#include "nr_sib9.h"
#include "common/utils/utils.h"

bool nr_sib9_time_to_utc(const struct timespec *now, int64_t lead_ns, uint64_t *utc)
{
  if (!now || !utc || now->tv_nsec < 0 || now->tv_nsec >= 1000000000L || lead_ns < 0)
    return false;
  const __int128 target = ((__int128)now->tv_sec + INT64_C(2208988800)) * 1000000000 + now->tv_nsec + lead_ns;
  if (target < 0 || target / 10000000 > NR_SIB9_MAX_UTC)
    return false;
  *utc = target / 10000000;
  return true;
}

bool nr_sib9_valid_broadcast(const NR_SIB9_t *sib9)
{
  if (!sib9 || !sib9->timeInfo || asn_check_constraints(&asn_DEF_NR_SIB9, sib9, NULL, NULL) != 0)
    return false;
  /* A parallel reference clock cannot be kept consistent by basic UTC refresh. */
  if (sib9->ext1 && sib9->ext1->referenceTimeInfo_r16)
    return false;
  if (sib9->timeInfo->dayLightSavingTime && (sib9->timeInfo->dayLightSavingTime->buf[0] >> 6) == 3)
    return false;
  return !sib9->lateNonCriticalExtension || sib9->lateNonCriticalExtension->size <= NR_SIB9_MAX_BYTES;
}

NR_SIB9_t *get_SIB9_NR(const sib9_config_t *cfg)
{
  if (!cfg || cfg->time_info_utc > NR_SIB9_MAX_UTC
      || (cfg->day_light_saving_time_present && (cfg->day_light_saving_time < 0 || cfg->day_light_saving_time > 2))
      || (cfg->leap_seconds_present && (cfg->leap_seconds < -127 || cfg->leap_seconds > 128))
      || (cfg->local_time_offset_present && (cfg->local_time_offset < -63 || cfg->local_time_offset > 64)))
    return NULL;
  NR_SIB9_t *sib = calloc_or_fail(1, sizeof(*sib));
  sib->timeInfo = calloc_or_fail(1, sizeof(*sib->timeInfo));
  if (asn_uint642INTEGER(&sib->timeInfo->timeInfoUTC, cfg->time_info_utc) != 0) {
    ASN_STRUCT_FREE(asn_DEF_NR_SIB9, sib);
    return NULL;
  }
  if (cfg->day_light_saving_time_present) {
    BIT_STRING_t *dst = calloc_or_fail(1, sizeof(*dst));
    dst->buf = calloc_or_fail(1, 1);
    dst->size = 1;
    dst->bits_unused = 6;
    dst->buf[0] = cfg->day_light_saving_time << 6;
    sib->timeInfo->dayLightSavingTime = dst;
  }
  if (cfg->leap_seconds_present) {
    sib->timeInfo->leapSeconds = calloc_or_fail(1, sizeof(long));
    *sib->timeInfo->leapSeconds = cfg->leap_seconds;
  }
  if (cfg->local_time_offset_present) {
    sib->timeInfo->localTimeOffset = calloc_or_fail(1, sizeof(long));
    *sib->timeInfo->localTimeOffset = cfg->local_time_offset;
  }
  return sib;
}
