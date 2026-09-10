/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NTN_SIM_CLOCK_H
#define NTN_SIM_CLOCK_H

#include <stdint.h>

#define NTN_SIM_CLOCK_DEFAULT_SHM "/oai_ntn_sim_clock"
#define NTN_SIM_CLOCK_MAGIC UINT64_C(0x4f41494e544e434c)
#define NTN_SIM_CLOCK_VERSION 1

typedef struct {
  uint64_t magic;
  uint32_t version;
  uint32_t reserved;
  uint64_t unix_time_ns;
} ntn_sim_clock_shared_t;

#endif
