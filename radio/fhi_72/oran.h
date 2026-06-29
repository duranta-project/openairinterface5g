/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _ORAN_H_
#define _ORAN_H_

#include "shared_buffers.h"
#include "common_lib.h"
void oran_fh_if4p5_south_out(RU_t *ru, int frame, int slot, uint64_t timestamp);

void oran_fh_if4p5_south_in(struct PHY_VARS_gNB_s *gNB, int *frame, int *slot);

int transport_init(openair0_device_t *device, openair0_config_t *openair0_cfg, eth_params_t *eth_params);

#endif /* _ORAN_H_ */
