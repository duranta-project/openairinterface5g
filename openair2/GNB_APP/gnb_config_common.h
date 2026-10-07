/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <assert.h>
#include "common/5g_platform_types.h"
#include "common/config/config_paramdesc.h"

uint16_t set_snssai_config(nssai_t *nssai, const int max_num_ssi, uint8_t k, uint8_t l);
uint8_t set_plmn_config(plmn_id_t *p, uint8_t idx);
plmn_id_t extract_plmn_from_params(const paramdef_t *params, int n_params);
nssai_t extract_nssai_from_params(const paramdef_t *params, int n_params);
