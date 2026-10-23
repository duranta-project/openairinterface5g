/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef DU_FHI_ISOLATE_H
#define DU_FHI_ISOLATE_H

#include "du_fhi_core.h"
#include "openair1/PHY/defs_gNB.h"
#include "radio/COMMON/gnb_fhi72_if.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initializes du_fh and the driver's scheduling state from a filled du_fh_config_t; separate from
// oran_fhi72_init() so tests can skip config_get().
du_fhi_state_t *du_fhi_init_from_config(du_fh_config_t *cfg);

void du_fhi_cleanup(du_fhi_state_t *st);

// Library entry point loaded by the gNB (fhi72_init_t, see executables/nr-ru.c start_split72()):
// fills gNB->fhi72_transport callbacks and spawns the split 7.2 FH thread.
int oran_fhi72_init(struct PHY_VARS_gNB_s *gNB);

#ifdef __cplusplus
}
#endif

#endif /* DU_FHI_ISOLATE_H */
