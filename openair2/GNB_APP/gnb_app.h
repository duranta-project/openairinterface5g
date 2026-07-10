/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef GNB_APP_H_
#define GNB_APP_H_

#include <stdint.h>
#include "ngap_messages_types.h"

void *gNB_app_task(void *args_p);
uint32_t gNB_app_register(uint32_t gnb_id_start, uint32_t gnb_id_end);
uint32_t gNB_app_register_x2(uint32_t gnb_id_start, uint32_t gnb_id_end);
void gNB_app_register_xn(instance_t instance, ngap_register_gnb_cnf_t *cnf);
#endif /* GNB_APP_H_ */
