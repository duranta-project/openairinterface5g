/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef E3_AGENT_H
#define E3_AGENT_H

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <unistd.h>

#include <libe3/c_api.h>

#include "config/e3_config.h"

#ifdef E2_AGENT
#include "ran_func_dapp_extern.h"
#endif

typedef struct {
  e3_agent_handle_t *agent;
  int encoding; /* E3_ENCODING_{ASN1,JSON,PROTOBUF}, from the config file */
} e3_agent_global_t;

typedef struct {
  uint32_t dapp_id;
  uint32_t *e3_ran_func_ids;
  size_t num_e3_ran_funcs;
} e3_dapp_info_t;

typedef struct {
  e3_dapp_info_t *dapps;
  size_t num_dapps;
} e3_dapp_subscription_map_t;

extern e3_agent_global_t e3;

/* Wire encoding for this run (E3_ENCODING_ASN1, JSON or PROTOBUF), as
 * selected in the config file; used by the SM payload encoders/decoders. */
int e3_get_encoding(void);

int e3_init();
int e3_destroy();

int e3_send_xapp_control(uint32_t dapp_id, uint32_t ran_function_id, const uint8_t *data, size_t len);

/**
 * @brief Get all connected dApps and their RAN function subscriptions.
 *
 * Caller must free the result with e3_free_dapp_subscription_map().
 *
 * @return populated map on success, {NULL, 0} on error or if no dApps
 */
e3_dapp_subscription_map_t e3_get_dapp_subscription_map(void);

/**
 * @brief Free a subscription map returned by e3_get_dapp_subscription_map().
 */
void e3_free_dapp_subscription_map(e3_dapp_subscription_map_t *map);

#endif // E3_AGENT_H
