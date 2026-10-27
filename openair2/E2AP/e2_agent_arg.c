/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "e2_agent_arg.h"
#include "e2_agent_paramdef.h"
#include "common/config/config_userapi.h"
#include "common/utils/assertions.h"

e2_agent_args_t RCconfig_NR_E2agent(void)
{
  paramdef_t e2agent_params[] = E2AGENT_PARAMS_DESC;
  int ret = config_get(config_get_if(), e2agent_params, sizeofArray(e2agent_params), CONFIG_STRING_E2AGENT);
  if (ret < 0) {
    printf("configuration file does not contain a \"%s\" section, applying default parameters from FlexRIC\n", CONFIG_STRING_E2AGENT);
    return (e2_agent_args_t) { 0 };
  }

  bool enabled = config_isparamset(e2agent_params, E2AGENT_CONFIG_SMDIR_IDX)
              && config_isparamset(e2agent_params, E2AGENT_CONFIG_IP_IDX);
  e2_agent_args_t dst = {.enabled = enabled};
  if (!enabled) {
    printf("E2 agent is DISABLED (for activation, define .%s.{%s,%s} parameters)\n", CONFIG_STRING_E2AGENT, E2AGENT_CONFIG_IP, E2AGENT_CONFIG_SMDIR);
    return dst;
  }

  if (e2agent_params[E2AGENT_CONFIG_SMDIR_IDX].strptr != NULL)
    dst.sm_dir = *e2agent_params[E2AGENT_CONFIG_SMDIR_IDX].strptr;

  if (e2agent_params[E2AGENT_CONFIG_IP_IDX].strptr != NULL)
    dst.ip = *e2agent_params[E2AGENT_CONFIG_IP_IDX].strptr;

  // Optional, so it is not part of `enabled`: left at 0, FlexRIC applies its own default
  if (e2agent_params[E2AGENT_CONFIG_PORT_IDX].uptr != NULL) {
    uint32_t const port = *e2agent_params[E2AGENT_CONFIG_PORT_IDX].uptr;
    AssertFatal(port > 0 && port <= UINT16_MAX,
                "%s.%s = %u is not a valid port\n",
                CONFIG_STRING_E2AGENT,
                E2AGENT_CONFIG_PORT,
                port);
    dst.port = port;
  }

  return dst;
}
