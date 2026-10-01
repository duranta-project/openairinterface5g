/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef DU_FHI_ISOLATE_H
#define DU_FHI_ISOLATE_H

#include "du_fhi_core.h"
#include "openair1/PHY/defs_RU.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initializes the native DU fronthaul backend + shim scheduling state from an already-populated
// du_fh_config_t (du_fh_init() plus PRACH occasion info). Exposed separately from
// transport_init() so tests can exercise the shim's scheduling/catch-up logic with a hand-built
// config, without going through config_get().
du_fhi_state_t *du_fhi_init_from_config(du_fh_config_t *cfg);

void du_fhi_cleanup(du_fhi_state_t *st);

// ABI surface expected by RU_t/ru_thread via get_internal_parameter() lookups.
void du_fhi_south_in(RU_t *ru, int *frame, int *slot);
void du_fhi_south_out(RU_t *ru, int frame, int slot, uint64_t timestamp);

void *get_internal_parameter(char *name);
int transport_init(openair0_device_t *device, openair0_config_t *openair0_cfg);

#ifdef __cplusplus
}
#endif

#endif /* DU_FHI_ISOLATE_H */
