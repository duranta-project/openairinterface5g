/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef DU_FHI_CONFIG_H
#define DU_FHI_CONFIG_H

#include "du_fh.h"
#include "radio/COMMON/gnb_fhi72_if.h"
#include "common/utils/nr/nr_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fill a du_fh_config_t: transport fields (DPDK devices, MACs, timing windows, compression,
 * PRACH eAxC offset) from the "fhi_72_native" section, cell fields from s7cfg.
 *
 * @param cfg Output DU fronthaul config.
 * @param s7cfg Split 7.2 configuration of the cell (numerology, PRB count, duplex mode, TDD
 *        frame structure, PRACH kbar).
 * @param prach_freq_range Output PRACH frequency range (FR1/FR2), derived from numerology.
 * @return 0 on success, negative on error.
 */
int get_du_fh_options(du_fh_config_t *cfg, const split7_config_t *s7cfg, frequency_range_t *prach_freq_range);

#ifdef __cplusplus
}
#endif

#endif /* DU_FHI_CONFIG_H */
