/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef GNB_CONFIG_TRP_INFO_H_
#define GNB_CONFIG_TRP_INFO_H_

#include "common/utils/nr/nr_common.h"

trp_info_config_t *RCconfig_nr_trp_info(void);
void dump_trp_info_config(const trp_info_config_t *trp_info_config);

#endif /* GNB_CONFIG_TRP_INFO_H_ */
