/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! 
 * \brief: load library implementing coding/decoding algorithms
 */
#define _GNU_SOURCE
#include <sys/types.h>
#include <stdlib.h>
#include <malloc.h>
#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "PHY/CODING/nrLDPC_extern.h"
#include "common/config/config_userapi.h"
#include "common/utils/load_module_shlib.h"

/* arguments used when called from phy simulators exec's which do not use the config module */
/* arg is used to initialize the config module so that the loader works as expected */
char *arg[64] = {"ldpctest", NULL};

int load_LDPClib(char *version, ldpc_interface_t *itf)
{
  char *ptr = (char *)config_get_if();
  char libname[64] = "ldpc";

  if (ptr == NULL) { // phy simulators, config module possibly not loaded
    uniqCfg = load_configmodule(1, arg, CONFIG_ENABLECMDLINEONLY);
    logInit();
  }
  /* function description array, to be used when loading the encoding/decoding shared lib */
  loader_shlibfunc_t shlib_fdesc[] = {{.fname = "LDPCinit"},
                                      {.fname = "LDPCshutdown"},
                                      {.fname = "LDPCdecoder"},
                                      {.fname = "LDPCencoder"},
                                      {.fname = "LDPCencoder32"}};
  int ret;
  ret = load_module_version_shlib(libname, version, shlib_fdesc, sizeofArray(shlib_fdesc), NULL);
  AssertFatal((ret >= 0), "Error loading ldpc decoder");
  LOG_I(NR_PHY,"loading LDPC version %s\n",version);
  itf->LDPCinit = (LDPC_initfunc_t *)shlib_fdesc[0].fptr;
  itf->LDPCshutdown = (LDPC_shutdownfunc_t *)shlib_fdesc[1].fptr;
  itf->LDPCdecoder = (LDPC_decoderfunc_t *)shlib_fdesc[2].fptr;
  itf->LDPCencoder = (LDPC_encoderfunc_t *)shlib_fdesc[3].fptr;
  itf->LDPCencoder32 = (LDPC_encoderfunc32_t *)shlib_fdesc[4].fptr;

  AssertFatal(itf->LDPCinit() == 0, "error starting LDPC library %s %s\n", libname, version);

  return 0;
}

int free_LDPClib(ldpc_interface_t *ldpc_interface)
{
  return ldpc_interface->LDPCshutdown();
}
