/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef E2_AGENT_ARGS_H
#define E2_AGENT_ARGS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Wrapper for OAI
typedef struct{
  const char *ip;
  const char *sm_dir;
  uint16_t port;
  const bool enabled;
} e2_agent_args_t;

e2_agent_args_t RCconfig_NR_E2agent(void);

#endif


