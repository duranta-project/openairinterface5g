/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef RAN_FUNC_SM_DAPP_EXTERN_AGENT_H
#define RAN_FUNC_SM_DAPP_EXTERN_AGENT_H

#include "openair2/RRC/NR/nr_rrc_defs.h"
#if defined(E3_AGENT)
#include "e3_agent.h"
#include <endian.h>
#include <string.h>
#endif

void generate_e2_indication_from_e3_dapp_report(uint32_t ran_function_id,
                                                uint32_t dapp_id,
                                                size_t report_size,
                                                const uint8_t *report_data);

void notify_dapp_status_changed(void);

#endif
