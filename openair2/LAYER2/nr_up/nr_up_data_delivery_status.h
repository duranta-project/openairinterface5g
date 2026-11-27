/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_UP_DATA_DELIVERY_STATUS_H
#define NR_UP_DATA_DELIVERY_STATUS_H

#include <stdbool.h>
#include <stdint.h>
#include "nr_up.h"

bool nr_up_data_delivery_status_precheck(nr_up_dds_t *st, uint32_t space, bool is_poll);

#endif /* NR_UP_DATA_DELIVERY_STATUS_H */
