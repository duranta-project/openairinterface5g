/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef CHANNEL_EMULATION_H
#define CHANNEL_EMULATION_H

#include "common_lib.h"
#include <string>

struct channel_emulation_config {
  bool enabled = false;
  std::string model_name = "AWGN";
  std::string ntn_profile_path;
};

bool install_channel_emulation(openair0_device_t *device, const openair0_config_t *device_config, const char *config_section);

#endif
