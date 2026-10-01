/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef DU_FHI_CONFIG_H
#define DU_FHI_CONFIG_H

#include "du_fh.h"
#include "common_lib.h"
#include "common/utils/nr/nr_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Populate a du_fh_config_t for the "fhi_72_native" transport-config section, mirroring
 * executables/nr-oru.c's get_oru_options() for the transport-specific fields (DPDK devices,
 * MAC addresses, timing windows, compression, PRACH eAxC offset/kbar). numerology, num_prbs,
 * duplex mode, the TDD pattern, and the PRACH configuration index are NOT re-read from a config
 * section -- they are derived from openair0_cfg (populated from servingCellConfigCommon before
 * transport_init() is called), the same source radio/fhi_72/oran-config.c's get_xran_config()
 * uses, so a du_fronthaul-native conf only needs a small transport-specific section on top of
 * the same gNB config that already works with the vendor "fhi_72" section.
 *
 * @param cfg Output DU fronthaul config.
 * @param openair0_cfg Already-populated general RF/split7 config (numerology, PRB count,
 *        duplex mode, TDD frame structure, PRACH index).
 * @param prach_freq_range Output PRACH frequency range (FR1/FR2), derived from numerology.
 * @return 0 on success, negative on error.
 */
int get_du_fh_options(du_fh_config_t *cfg, const openair0_config_t *openair0_cfg, frequency_range_t *prach_freq_range);

#ifdef __cplusplus
}
#endif

#endif /* DU_FHI_CONFIG_H */
