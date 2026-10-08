/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Configuration of the CUDA LDPC coding library, read from the config module (section nrLDPC_coding_cuda)
 */

#include <stdbool.h>
#include "nrLDPC_coding_cuda_config.h"
#include "common/config/config_paramdesc.h"
#include "common/config/config_userapi.h"
#include "common/utils/LOG/log.h"

#define CONFIG_STRING_WAIT_MODE "wait_mode"
#define CONFIG_STRING_NUM_CONTEXTS "num_contexts"
#define CONFIG_STRING_CRC_CHECK_INTERVAL "crc_check_interval"
#define CONFIG_STRING_CRC_CHECK "crc_check"

#define HLP_WAIT_MODE "how a host thread waits for the GPU: spin, yield or block"
#define HLP_NUM_CONTEXTS "number of decoder contexts, i.e. transport blocks decoded concurrently (1-8)"
#define HLP_CRC_CHECK_INTERVAL "decoder iterations between two code block CRC checks (early termination, 1-64)"
#define HLP_CRC_CHECK "where the code block CRCs are checked during decoding: gpu or host"

#define CRC_CHECK_INTERVAL_MAX 64

const char *const ldpc_cuda_wait_mode_names[] =
    {[LDPC_CUDA_WAIT_SPIN] = "spin", [LDPC_CUDA_WAIT_YIELD] = "yield", [LDPC_CUDA_WAIT_BLOCK] = "block"};
static const char *const crc_check_names[] = {[LDPC_CUDA_CRC_CHECK_HOST] = "host", [LDPC_CUDA_CRC_CHECK_GPU] = "gpu"};

static const ldpc_cuda_config_t ldpc_cuda_config_default = {
    .wait_mode = LDPC_CUDA_WAIT_YIELD,
    .num_contexts = 4,
    .crc_check_interval = 2,
    .crc_check = LDPC_CUDA_CRC_CHECK_GPU,
};

// value set by config_checkstr_assign_integer() for string option name, def if its check failed (if the config module
// does not abort on it)
static int processed_or_default(paramdef_t *params, int nparams, const char *name, int def)
{
  paramdef_t *p = &params[config_paramidx_fromname(params, nparams, (char *)name)];
  return p->processedvalue ? config_get_processedint(config_get_if(), p) : def;
}

const ldpc_cuda_config_t *ldpc_cuda_get_config(void)
{
  static ldpc_cuda_config_t cfg;
  static bool read = false;
  if (read)
    return &cfg;
  read = true;
  cfg = ldpc_cuda_config_default; // also used if the config module is not available

  // the config module checks the values and converts the strings to the enum values
  checkedparam_t wait_mode_check = {.s3a = {config_checkstr_assign_integer,
                                            {(char *)ldpc_cuda_wait_mode_names[LDPC_CUDA_WAIT_SPIN],
                                             (char *)ldpc_cuda_wait_mode_names[LDPC_CUDA_WAIT_YIELD],
                                             (char *)ldpc_cuda_wait_mode_names[LDPC_CUDA_WAIT_BLOCK]},
                                            {LDPC_CUDA_WAIT_SPIN, LDPC_CUDA_WAIT_YIELD, LDPC_CUDA_WAIT_BLOCK},
                                            3}};
  checkedparam_t num_contexts_check = {.s2 = {config_check_intrange, {1, LDPC_CUDA_MAX_CTX}}};
  checkedparam_t crc_check_interval_check = {.s2 = {config_check_intrange, {1, CRC_CHECK_INTERVAL_MAX}}};
  checkedparam_t crc_check_check = {
      .s3a = {config_checkstr_assign_integer,
              {(char *)crc_check_names[LDPC_CUDA_CRC_CHECK_HOST], (char *)crc_check_names[LDPC_CUDA_CRC_CHECK_GPU]},
              {LDPC_CUDA_CRC_CHECK_HOST, LDPC_CUDA_CRC_CHECK_GPU},
              2}};
  char *wait_mode = NULL;
  char *crc_check = NULL;
  paramdef_t params[] = {
      {CONFIG_STRING_WAIT_MODE,
       HLP_WAIT_MODE,
       0,
       .strptr = &wait_mode,
       .defstrval = (char *)ldpc_cuda_wait_mode_names[ldpc_cuda_config_default.wait_mode],
       TYPE_STRING,
       0,
       &wait_mode_check},
      {CONFIG_STRING_NUM_CONTEXTS,
       HLP_NUM_CONTEXTS,
       0,
       .iptr = &cfg.num_contexts,
       .defintval = ldpc_cuda_config_default.num_contexts,
       TYPE_INT,
       0,
       &num_contexts_check},
      {CONFIG_STRING_CRC_CHECK_INTERVAL,
       HLP_CRC_CHECK_INTERVAL,
       0,
       .iptr = &cfg.crc_check_interval,
       .defintval = ldpc_cuda_config_default.crc_check_interval,
       TYPE_INT,
       0,
       &crc_check_interval_check},
      {CONFIG_STRING_CRC_CHECK,
       HLP_CRC_CHECK,
       0,
       .strptr = &crc_check,
       .defstrval = (char *)crc_check_names[ldpc_cuda_config_default.crc_check],
       TYPE_STRING,
       0,
       &crc_check_check},
  };
  const int nparams = sizeofArray(params);
  if (config_get(config_get_if(), params, nparams, LDPC_CUDA_CONFIG_SECTION) < 0) {
    LOG_W(NR_PHY, "CUDA LDPC: config module not available, default configuration\n");
    cfg = ldpc_cuda_config_default;
    return &cfg;
  }
  cfg.wait_mode = processed_or_default(params, nparams, CONFIG_STRING_WAIT_MODE, ldpc_cuda_config_default.wait_mode);
  cfg.crc_check = processed_or_default(params, nparams, CONFIG_STRING_CRC_CHECK, ldpc_cuda_config_default.crc_check);
  return &cfg;
}
