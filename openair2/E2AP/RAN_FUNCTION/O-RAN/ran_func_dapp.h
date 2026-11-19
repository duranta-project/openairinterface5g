/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef RAN_FUNC_SM_DAPP_READ_WRITE_AGENT_H
#define RAN_FUNC_SM_DAPP_READ_WRITE_AGENT_H

#include "openair2/E2AP/flexric/src/agent/../sm/sm_io.h"
#include "openair2/E2AP/flexric/src/sm/dapp_sm/dapp_sm_id.h"
#include "ran_func_dapp_subs.h"
#include "ran_func_dapp_extern.h"
#include "../../flexric/src/agent/e2_agent_api.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "common/ran_context.h"

#if defined(E3_AGENT)
#include "e3_agent.h"
#include <endian.h>
#include "openair2/LAYER2/NR_MAC_gNB/nr_mac_gNB.h"
#endif

/**
 * @brief Populate the DAPP RAN function definition during E2 setup.
 *
 * Called by the E2 agent when building the E2 SETUP REQUEST.
 * Fills the RAN function definition with:
 *   - RAN function name, OID, and description
 *   - Two report styles:
 *       DAPP-E3-DATA-REPORT: E3 data reports only (indication format 1)
 *       DAPP-E3-SUBSCRIPTION-MAP: subscription map only (indication format 2)
 *   - One control style (format 1)
 *   - Current dApp E3 subscription map (if any dApps are connected)
 * Also initializes the global DAPP subscription state (once).
 *
 * @param data  Pointer to a dapp_e2_setup_t to be filled.
 */
void read_dapp_setup_sm(void* data);

/**
 * @brief Handle DAPP SM subscription requests at the agent.
 *
 * Routes the subscription into the appropriate indication list based on the
 * report style type in the action definition:
 *   - DAPP-E3-DATA-REPORT: Format 1 indications (E3 data reports)
 *   - DAPP-E3-SUBSCRIPTION-MAP: Format 2 indications (subscription map)
 *
 * Returns an aperiodic subscription outcome.
 *
 * @param src  Pointer to a wr_dapp_sub_data_t containing the subscription request.
 * @return     Subscription outcome (APERIODIC_SUBSCRIPTION_FLRC).
 */
sm_ag_if_ans_t write_subs_dapp_sm(void const* src);

/**
 * @brief Handle DAPP SM control requests at the agent.
 *
 * For Format 1 control messages (when compiled with E3_AGENT):
 *   - Extracts RAN function ID, dApp ID, and control payload
 *   - Forwards the control to the E3 agent via e3_send_xapp_control()
 *
 * @param data  Pointer to a dapp_ctrl_req_data_t containing the control request.
 * @return      Control outcome of type DAPP_AGENT_IF_CTRL_ANS_V0.
 */
sm_ag_if_ans_t write_ctrl_dapp_sm(void const* data);

/**
 * @brief Read DAPP SM state from the agent.
 *
 * No READ operation is defined for this service model. Always returns false,
 * so a stray READ cannot bring the gNB down.
 *
 * @param data  Unused.
 * @return      Always false.
 */
bool read_dapp_sm(void*);

#endif