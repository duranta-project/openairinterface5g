/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef OAIORAN_H
#define OAIORAN_H

#include <stdint.h>
#include "xran_fh_o_du.h"

/** @brief xran callback for fronthaul RX, see xran_5g_fronthault_config(). */
void oai_xran_fh_rx_callback(void *pCallbackTag, xran_status_t status, uint8_t mu);
/** @brief xran callback for fronthaul PRACH RX, see xran_5g_prach_req(). */
void oai_xran_fh_rx_prach_callback(void *pCallbackTag, xran_status_t status, uint8_t mu);

#endif /* OAIORAN_H */
