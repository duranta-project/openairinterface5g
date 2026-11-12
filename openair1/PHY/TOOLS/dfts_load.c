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
#include "tools_defs.h"
#include "common/config/config_userapi.h" 
#include "common/utils/load_module_shlib.h" 


/* function description array, to be used when loading the dfts/idfts lib */
static loader_shlibfunc_t shlib_fdesc[4];
static char *arg[64] = {"phytest", "-O", "cmdlineonly::dbgl0"};
dftfunc_t dft;
idftfunc_t idft;
dftprunedfunc_t dft_pruned;
dftprunedfunc_t idft_pruned;
int load_dftslib(void)
{
  char *ptr = (char *)config_get_if();
  if (ptr == NULL) { // phy simulators, config module possibly not loaded
    uniqCfg = load_configmodule(3, (char **)arg, CONFIG_ENABLECMDLINEONLY);
    logInit();
  }
  shlib_fdesc[0].fname = "dft_implementation";
  shlib_fdesc[1].fname = "idft_implementation";
  shlib_fdesc[2].fname = "dft_pruned_implementation";
  shlib_fdesc[3].fname = "idft_pruned_implementation";
  int ret = load_module_shlib("dfts", shlib_fdesc, sizeof(shlib_fdesc) / sizeof(loader_shlibfunc_t), NULL);
  AssertFatal((ret >= 0), "Error loading dftsc decoder");
  dft = (dftfunc_t)shlib_fdesc[0].fptr;
  idft = (idftfunc_t)shlib_fdesc[1].fptr;
  dft_pruned = (dftprunedfunc_t)shlib_fdesc[2].fptr;
  idft_pruned = (dftprunedfunc_t)shlib_fdesc[3].fptr;
  return 0;
}
