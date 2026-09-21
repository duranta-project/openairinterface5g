/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef SIM_CLOCK_H
#define SIM_CLOCK_H

#include <stdint.h>

#define SIM_CLOCK_DEFAULT_SHM "/oai_sim_clock"
#define SIM_CLOCK_MAGIC UINT64_C(0x4f414953494d434c)
#define SIM_CLOCK_VERSION 1

typedef struct {
  uint64_t magic;
  uint32_t version;
  uint32_t reserved;
  uint64_t unix_time_ns;
} sim_clock_shared_t;

#endif
