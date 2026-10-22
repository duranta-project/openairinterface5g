/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _ORAN_H_
#define _ORAN_H_

#include "common_lib.h"

/* split 7.2 fronthaul callbacks used by the gNB TX path (tx_func()) */
typedef struct fhi72_transport_s {
  void *priv; // fhi_72 library state
  void (*fh_south_out)(int32_t **txdataF_BF, int frame, int slot);
  void (*fh_south_ctrl)(uint16_t **beam_id, int frame, int slot);
} fhi72_transport_t;

/* oran_fhi72_init(): fhi_72 library entry point, loaded via load_transport_shlib(). Fills transport
 * and spawns the FH thread. */
typedef int (*fhi72_init_t)(openair0_config_t *cfg, RU_t *ru, fhi72_transport_t *transport);

#endif /* _ORAN_H_ */
