/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#ifndef _NFAPI_PNF_INTERFACE_COMMON_H_
#define _NFAPI_PNF_INTERFACE_COMMON_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include "nfapi_interface.h"
#include "debug.h"
#include <stdbool.h>
#include <sys/types.h>

typedef struct pnf_s pnf_t;
typedef struct pnf_p7_s pnf_p7_t;

typedef enum { NFAPI_PNF_IDLE = 0, NFAPI_PNF_CONFIGURED, NFAPI_PNF_RUNNING } nfapi_pnf_state_e;

typedef enum { NFAPI_PNF_PHY_IDLE = 0, NFAPI_PNF_PHY_CONFIGURED = 1, NFAPI_PNF_PHY_RUNNING = 2 } nfapi_pnf_phy_state_e;

typedef struct nfapi_pnf_phy_config nfapi_pnf_phy_config_t;

typedef struct nfapi_pnf_phy_config {
  uint16_t phy_id;
  nfapi_pnf_phy_state_e state;
  void* user_data;
  struct nfapi_pnf_phy_config* next;
} nfapi_pnf_phy_config_t;

#if defined(__cplusplus)
}
#endif
#endif /* _NFAPI_PNF_INTERFACE_COMMON_H_ */
