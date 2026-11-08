/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <string.h>
#include <stdlib.h>
#include "nr_inner_rx_offload.h"
#include "common/utils/utils.h"
#include "common/utils/LOG/log.h"
#include "common/config/config_userapi.h"
#include "common/utils/load_module_shlib.h"

// The module is loaded once per process, at its first use: the RU allocates rxdataF (nr_irx_alloc_rx_buffer) before the gNB
// knows the sizes it initializes the module with (load_nr_irx_interface, from phy_init_nr_gNB).
static nr_irx_interface_t lib;
static int lib_state; // 0 not tried, 1 loaded, -1 none configured or failed
static bool lib_init;

static int irx_dlopen(void)
{
  if (lib_state)
    return lib_state > 0 ? 0 : -1;
  lib_state = -1;
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
                            {.fname = "nr_irx_run"},
                            {.fname = "nr_irx_register_host"},
                            {.fname = "nr_irx_alloc_host"},
                            {.fname = "nr_irx_free_host"}};
  if (load_module_version_shlib("inner_rx", version, f, sizeofArray(f), NULL) < 0) {
    LOG_E(PHY, "inner RX offload library inner_rx%s unavailable, inner RX on the CPU\n", version);
    return -1;
  }
  lib = (nr_irx_interface_t){.init = (nr_irx_init_t *)f[0].fptr,
                             .shutdown = (nr_irx_shutdown_t *)f[1].fptr,
                             .supported = (nr_irx_supported_t *)f[2].fptr,
                             .acquire = (nr_irx_acquire_t *)f[3].fptr,
                             .release = (nr_irx_release_t *)f[4].fptr,
                             .staging = (nr_irx_staging_t *)f[5].fptr,
                             .run = (nr_irx_run_t *)f[6].fptr,
                             .register_host = (nr_irx_register_host_t *)f[7].fptr,
                             .alloc_host = (nr_irx_alloc_host_t *)f[8].fptr,
                             .free_host = (nr_irx_free_host_t *)f[9].fptr};
  lib_state = 1;
  return 0;
}

int load_nr_irx_interface(nr_irx_interface_t *itf, uint32_t max_re_per_symbol, int max_nb_rx, int nb_ctx)
{
  memset(itf, 0, sizeof(*itf));
  if (irx_dlopen() < 0)
    return -1;
  if (!lib_init) {
    if (lib.init(max_re_per_symbol, max_nb_rx, nb_ctx) != 0) {
      LOG_E(PHY, "inner RX offload library failed to initialize, inner RX on the CPU\n");
      return -1;
    }
    lib_init = true;
    LOG_I(PHY, "inner RX offload: %d contexts, %u REs x %d Rx\n", nb_ctx, max_re_per_symbol, max_nb_rx);
  }
  *itf = lib;
  return 0;
}

void free_nr_irx_interface(nr_irx_interface_t *itf)
{
  if (lib_init && itf->shutdown) {
    itf->shutdown();
    lib_init = false;
  }
  memset(itf, 0, sizeof(*itf));
}

void *nr_irx_alloc_rx_buffer(size_t bytes)
{
  if (irx_dlopen() == 0) {
    void *p = lib.alloc_host(bytes);
    if (p)
      return p;
  }
  return malloc16_clear(bytes);
}

void nr_irx_free_rx_buffer(void *ptr)
{
  if (ptr && (lib_state <= 0 || lib.free_host(ptr) != 0))
    free(ptr);
}
