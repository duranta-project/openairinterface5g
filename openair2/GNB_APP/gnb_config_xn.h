/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef XNAP_GNB_CONFIG_H_
#define XNAP_GNB_CONFIG_H_
#include <stdint.h>
#include "ngap_messages_types.h"
#include "xnap_messages_types.h"

xnap_net_config_t read_ip_config_xn(uint32_t gnb_idx);
int is_xnap_enabled(void);
xnap_setup_req_t read_ng_setup_info(const ngap_register_gnb_cnf_t *cnf, uint32_t gnb_idx);

#endif /* XNAP_GNB_CONFIG_H_ */
