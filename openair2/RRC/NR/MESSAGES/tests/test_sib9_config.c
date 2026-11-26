/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "common/config/config_userapi.h"
#include "common/utils/LOG/log.h"
#include "openair2/GNB_APP/gnb_sib9.h"
#include "openair2/RRC/NR/MESSAGES/asn1_msg.h"
#include "uper_decoder.h"

static bool fail_clock;
int __real_clock_gettime(clockid_t clock, struct timespec *now);
int __wrap_clock_gettime(clockid_t clock, struct timespec *now)
{
  if (clock != CLOCK_REALTIME)
    return __real_clock_gettime(clock, now);
  if (fail_clock)
    return -1;
  *now = (struct timespec){.tv_sec = 1700000000, .tv_nsec = 129999999};
  return 0;
}
static configmodule_interface_t *load_fixture(const char *body, char path[64], bool timing)
{
  strcpy(path, "/tmp/oai-sib9-config-XXXXXX");
  int fd = mkstemp(path);
  AssertFatal(fd >= 0, "temporary fixture\n");
  FILE *stream = fdopen(fd, "w");
  AssertFatal(stream, "fixture stream\n");
  if (body)
    fprintf(stream, "gNBs = ({ %s = { %s }; });\n", timing ? "sib9_timing" : "sib9_config", body);
  else
    fprintf(stream, "gNBs = ({});\n");
  fclose(stream);
  char **argv = calloc_or_fail(4, sizeof(*argv));
  argv[0] = strdup("test_sib9_config");
  argv[1] = strdup("-O");
  argv[2] = strdup(path);
  configmodule_interface_t *cfg = load_configmodule(3, argv, 0);
  AssertFatal(cfg, "configuration backend\n");
  uniqCfg = cfg;
  return cfg;
}

static void release_fixture(configmodule_interface_t *cfg, const char *path)
{
  char **argv = cfg->argv;
  end_configmodule(cfg);
  for (int i = 0; i < 3; ++i)
    free(argv[i]);
  free(argv);
  uniqCfg = NULL;
  unlink(path);
}

static sib9_config_t profile_case(const char *body, bool expected)
{
  char path[64], error[256] = {0};
  configmodule_interface_t *cfg = load_fixture(body, path, false);
  sib9_config_t result = {.time_info_utc = 77}, before = result;
  bool ok = nr_sib9_read_profile(cfg, "gNBs.[0].sib9_config", &result, error, sizeof(error));
  AssertFatal(ok == expected, "profile '%s': expected %d got %d: %s\n", body, expected, ok, error);
  if (!ok)
    AssertFatal(error[0] && memcmp(&result, &before, sizeof(result)) == 0, "rejected output changed\n");
  else {
    NR_SIB9_t *sib = get_SIB9_NR(&result);
    byte_array_t bytes = do_SIB9_NR(sib);
    NR_SIB9_t *decoded = NULL;
    asn_dec_rval_t dec = uper_decode_complete(NULL, &asn_DEF_NR_SIB9, (void **)&decoded, bytes.buf, bytes.len);
    AssertFatal(dec.code == RC_OK && nr_sib9_valid_broadcast(decoded), "decode\n");
    uint64_t value;
    AssertFatal(!asn_INTEGER2uint64(&decoded->timeInfo->timeInfoUTC, &value) && value == result.time_info_utc, "UTC\n");
    AssertFatal((decoded->timeInfo->dayLightSavingTime != NULL) == result.day_light_saving_time_present, "DST presence\n");
    AssertFatal((decoded->timeInfo->leapSeconds != NULL) == result.leap_seconds_present, "leap presence\n");
    AssertFatal((decoded->timeInfo->localTimeOffset != NULL) == result.local_time_offset_present, "offset presence\n");
    ASN_STRUCT_FREE(asn_DEF_NR_SIB9, decoded);
    ASN_STRUCT_FREE(asn_DEF_NR_SIB9, sib);
    free_byte_array(bytes);
  }
  release_fixture(cfg, path);
  return result;
}

static void mode_case(const char *body, bool expected, nr_sib9_mode_t mode)
{
  char path[64], error[128] = {0};
  configmodule_interface_t *cfg = load_fixture(body, path, true);
  nr_sib9_mode_t result = NR_SIB9_REALTIME;
  bool ok = nr_sib9_read_mode(cfg, "gNBs.[0].sib9_timing", &result, error, sizeof(error));
  AssertFatal(ok == expected && (!ok || result == mode), "DU mode '%s': %s\n", body, error);
  release_fixture(cfg, path);
}

int main(void)
{
  logInit();
  int64_t values[] = {0, 1, 549755813887LL, -1, 549755813888LL};
  for (size_t i = 0; i < sizeofArray(values); ++i) {
    char body[128];
    snprintf(body, sizeof(body), "timeInfoUTC=%lldL;", (long long)values[i]);
    profile_case(body, i < 3);
  }
  profile_case(NULL, false);
  profile_case("", false);
  profile_case("timeSource=\"manual\"; timeInfoUTC=0L;", false);
  profile_case("timeSource=\"realtime\"; timeInfoUTC=0L;", false);
  sib9_config_t live = profile_case("timeSource=\"realtime\";", true);
  AssertFatal(live.time_info_utc == UINT64_C(390898880012), "fixed independent epoch result\n");
  fail_clock = true;
  profile_case("timeSource=\"realtime\";", false);
  fail_clock = false;
  profile_case("timeInfoUTC=0L; dayLightSavingTime=0; leapSeconds=0; localTimeOffset=0;", true);
  const char *fields[] = {"dayLightSavingTime", "leapSeconds", "localTimeOffset"};
  const int low[] = {0, -127, -63}, high[] = {2, 128, 64};
  for (int i = 0; i < 3; ++i) {
    int endpoints[] = {low[i] - 1, low[i], high[i], high[i] + 1};
    for (int j = 0; j < 4; ++j) {
      char body[128];
      snprintf(body, sizeof(body), "timeInfoUTC=1L; %s=%d;", fields[i], endpoints[j]);
      profile_case(body, j == 1 || j == 2);
    }
  }
  profile_case("timeInfoUTC=0L; dayLightSavingTime=4294967296L;", false);
  profile_case("timeInfoUTC=0L; leapSeconds=4294967314L;", false);
  profile_case("timeInfoUTC=0L; localTimeOffset=-4294967296L;", false);
  mode_case(NULL, true, NR_SIB9_STATIC);
  mode_case("mode=\"static\";", true, NR_SIB9_STATIC);
  mode_case("mode=\"realtime\";", true, NR_SIB9_REALTIME);
  mode_case("mode=\"dynamic\";", false, NR_SIB9_STATIC);
  const uint8_t zero[] = {0x40, 0, 0, 0, 0, 0};
  const uint8_t one[] = {0x40, 0, 0, 0, 0, 8};
  for (int i = 0; i < 2; ++i) {
    sib9_config_t cfg = {.time_info_utc = i};
    NR_SIB9_t *sib = get_SIB9_NR(&cfg);
    byte_array_t bytes = do_SIB9_NR(sib);
    AssertFatal(bytes.len == 6 && !memcmp(bytes.buf, i ? one : zero, 6), "independent UPER vector\n");
    free_byte_array(bytes);
    ASN_STRUCT_FREE(asn_DEF_NR_SIB9, sib);
  }
  sib9_config_t meta = {.time_info_utc = NR_SIB9_MAX_UTC,
                        .day_light_saving_time_present = true,
                        .day_light_saving_time = 2,
                        .leap_seconds_present = true,
                        .leap_seconds = 18,
                        .local_time_offset_present = true,
                        .local_time_offset = 4};
  NR_SIB9_t *tree = get_SIB9_NR(&meta);
  byte_array_t bytes = do_SIB9_NR(tree);
  const uint8_t full[] = {0x5f, 0xff, 0xff, 0xff, 0xff, 0xfd, 0x23, 0x0c};
  AssertFatal(bytes.len == sizeof(full) && !memcmp(bytes.buf, full, sizeof(full)), "metadata UPER vector\n");
  free_byte_array(bytes);
  ASN_STRUCT_FREE(asn_DEF_NR_SIB9, tree);
  meta.day_light_saving_time = 3;
  AssertFatal(get_SIB9_NR(&meta) == NULL && get_SIB9_NR(NULL) == NULL, "constructor rejects invalid input\n");
  uint64_t utc = 12;
  struct timespec now = {.tv_sec = -2208988800LL};
  AssertFatal(nr_sib9_time_to_utc(&now, 0, &utc) && utc == 0, "1900 origin\n");
  now.tv_sec--;
  AssertFatal(!nr_sib9_time_to_utc(&now, 0, &utc) && utc == 0, "before origin\n");
  now = (struct timespec){.tv_sec = 0, .tv_nsec = 999999999};
  AssertFatal(nr_sib9_time_to_utc(&now, 1, &utc) && utc == UINT64_C(220898880100), "second rollover\n");
  now.tv_nsec = 1000000000;
  AssertFatal(!nr_sib9_time_to_utc(&now, 0, &utc), "invalid nanoseconds\n");
  logTerm();
  return 0;
}
