/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef ORAN_CONFIG_H
#define ORAN_CONFIG_H

#include "stdbool.h"
#include "stdint.h"

#include "oran.h"

struct xran_fh_init;
struct xran_fh_config;

/** @brief Fills xran fh_init/fh_config from s7cfg and the config file; overrides s7cfg->prach_kbar if kbar is set. */
bool get_xran_config(void *mplane_api, split7_config_t *s7cfg, struct xran_fh_init *fh_init, struct xran_fh_config *fh_config);

void print_fh_init(const struct xran_fh_init *fh_init);
void print_fh_config(const struct xran_fh_config *fh_config);

#endif /* ORAN_CONFIG_H */
