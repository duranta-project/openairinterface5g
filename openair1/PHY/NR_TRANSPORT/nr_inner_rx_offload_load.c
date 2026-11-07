/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <string.h>
#include "nr_inner_rx_offload.h"
#include "common/utils/LOG/log.h"
#include "common/config/config_userapi.h"
#include "common/utils/load_module_shlib.h"

int load_nr_irx_interface(nr_irx_interface_t *itf, uint32_t max_re_per_symbol, int max_nb_rx, int nb_ctx)
{
  memset(itf, 0, sizeof(*itf));
  // unlike the LDPC there is no default library: load only when a version is configured
  char *version = NULL;
  paramdef_t p[] = {{"shlibversion", NULL, 0, .strptr = &version, .defstrval = "", TYPE_STRING, 0, NULL}};
  if (config_get(config_get_if(), p, sizeofArray(p), LOADER_CONFIG_PREFIX ".inner_rx") < 0 || !version || !version[0])
    return -1;

  loader_shlibfunc_t f[] = {{.fname = "nr_irx_init"},
                            {.fname = "nr_irx_shutdown"},
                            {.fname = "nr_irx_supported"},
                            {.fname = "nr_irx_acquire"},
                            {.fname = "nr_irx_release"},
                            {.fname = "nr_irx_staging"},
                            {.fname = "nr_irx_run"}};
  if (load_module_version_shlib("inner_rx", version, f, sizeofArray(f), NULL) < 0) {
    LOG_E(PHY, "inner RX offload library inner_rx%s unavailable, inner RX on the CPU\n", version);
    return -1;
  }
  nr_irx_interface_t l = {.init = (nr_irx_init_t *)f[0].fptr,
                          .shutdown = (nr_irx_shutdown_t *)f[1].fptr,
                          .supported = (nr_irx_supported_t *)f[2].fptr,
                          .acquire = (nr_irx_acquire_t *)f[3].fptr,
                          .release = (nr_irx_release_t *)f[4].fptr,
                          .staging = (nr_irx_staging_t *)f[5].fptr,
                          .run = (nr_irx_run_t *)f[6].fptr};
  if (l.init(max_re_per_symbol, max_nb_rx, nb_ctx) != 0) {
    LOG_E(PHY, "inner RX offload library inner_rx%s failed to initialize, inner RX on the CPU\n", version);
    return -1;
  }
  *itf = l;
  LOG_I(PHY, "inner RX offload: inner_rx%s (%d contexts, %u REs x %d Rx)\n", version, nb_ctx, max_re_per_symbol, max_nb_rx);
  return 0;
}

void free_nr_irx_interface(nr_irx_interface_t *itf)
{
  if (itf->shutdown)
    itf->shutdown();
  memset(itf, 0, sizeof(*itf));
}
