/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _ORAN_H_
#define _ORAN_H_

#include "common_lib.h"

/* split 7.2 fronthaul callbacks used by the gNB TX path (tx_func()) */
typedef struct fhi72_transport_s {
  split7_config_t s7cfg;
  pthread_t pthread;
  void (*fh_south_out)(split7_config_t *s7cfg, int frame, int slot);
  void (*fh_south_ctrl)(split7_config_t *s7cfg, uint16_t **beam_id, int frame, int slot);
} fhi72_transport_t;

typedef int (*fhi72_init_t)(struct PHY_VARS_gNB_s *gNB);

#endif /* _ORAN_H_ */
