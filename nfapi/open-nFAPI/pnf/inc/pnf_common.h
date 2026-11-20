/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#ifndef _PNF_COMMON_H_
#define _PNF_COMMON_H_

#include "nfapi_lte_pnf_interface.h"

nfapi_pnf_phy_config_t *nfapi_pnf_phy_config_find(nfapi_pnf_config_t *config, uint16_t phy_id);
void nfapi_pnf_phy_config_delete_all(nfapi_pnf_config_t *config);
void nfapi_pnf_phy_config_add(nfapi_pnf_config_t *config, nfapi_pnf_phy_config_t *phy);

#endif /* _PNF_COMMON_H_ */
