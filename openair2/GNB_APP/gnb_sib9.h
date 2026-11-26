/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
#ifndef GNB_SIB9_H
#define GNB_SIB9_H
#include "common/config/config_userapi.h"
#include "openair2/RRC/NR/nr_sib9.h"

bool nr_sib9_read_profile(configmodule_interface_t *cfg, const char *prefix, sib9_config_t *out, char *error, size_t size);
bool nr_sib9_read_mode(configmodule_interface_t *cfg, const char *prefix, nr_sib9_mode_t *mode, char *error, size_t size);
#endif
