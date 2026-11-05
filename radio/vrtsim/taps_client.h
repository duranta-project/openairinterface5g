/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef TAPS_CLIENT_H
#define TAPS_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sim.h"

#define MAX_TAPS_LEN 128

void *taps_client_connect(const char *socket_path, int num_tx_ant, int num_rx_ant);
channel_desc_t *taps_client_get_model(void *handle, int id);
void taps_client_stop(void *handle);

#ifdef __cplusplus
}
#endif

#endif
