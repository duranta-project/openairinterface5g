/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
#ifndef NR_SIB9_H
#define NR_SIB9_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "NR_SIB9.h"

#define NR_SIB9_MAX_UTC UINT64_C(549755813887)
#define NR_SIB9_MAX_BYTES (2976 / 8)

typedef enum { NR_SIB9_STATIC, NR_SIB9_REALTIME } nr_sib9_mode_t;

typedef struct {
  uint64_t time_info_utc;
  bool day_light_saving_time_present;
  int day_light_saving_time;
  bool leap_seconds_present;
  int leap_seconds;
  bool local_time_offset_present;
  int local_time_offset;
} sib9_config_t;

/* On failure the output is unchanged. lead_ns is an estimated radio scheduling lead. */
bool nr_sib9_time_to_utc(const struct timespec *now, int64_t lead_ns, uint64_t *utc);
/* Caller owns the returned ASN.1 tree. */
NR_SIB9_t *get_SIB9_NR(const sib9_config_t *cfg);
bool nr_sib9_valid_broadcast(const NR_SIB9_t *sib9);
#endif
