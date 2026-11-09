/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief configuration module, load the shared library implementing the configuration module
 */

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include "common/platform_types.h"

#include "config_load_configmodule.h"
#include "config_userapi.h"
#include "../utils/LOG/log.h"
#define CONFIG_SHAREDLIBFORMAT "libparams_%s.so"
#include "nfapi/oai_integration/vendor_ext.h"
#include "config_common.h"

// clang-format off
static char  config_helpstr [] = "\n lte-softmodem -O [config mode]<:dbgl[debugflags]><:incp[path]>\n \
          debugflags can also be defined in the config section of the config file\n \
          debugflags: mask,    1->print parameters, 2->print memory allocations debug messages\n \
                               4->print command line processing debug messages\n \
          incp parameter can be used to define the include path used for config files (@include directive)\n \
                         defaults is set to the path of the main config file.\n";

static paramdef_t Config_Params[] = {
  /*--------------------------------------------------------------------------------------------------------------------------*/
  /*                                            config parameters for config module                                           */
  /* optname           helpstr             paramflags        XXXptr          defXXXval            type         numelt         */
  /*--------------------------------------------------------------------------------------------------------------------------*/
  {CONFIGP_DEBUGFLAGS, config_helpstr,     0,                .uptr = NULL,   .defintval = 0,      TYPE_MASK,   0},
  {CONFIGP_TMPDIR,     CONFIG_HELP_TMPDIR, PARAMFLAG_NOFREE, .strptr = NULL, .defstrval = "/tmp", TYPE_STRING, 0},
};
// clang-format on

static int load_config_sharedlib(configmodule_interface_t *cfgptr)
{
  void *lib_handle;
  char fname[128];
  char libname[FILENAME_MAX];
  int st;
  st=0;
  sprintf(libname,CONFIG_SHAREDLIBFORMAT,cfgptr->cfgmode);
  lib_handle = dlopen(libname, RTLD_LAZY | RTLD_GLOBAL | RTLD_NODELETE);

  if (!lib_handle) {
    fprintf(stderr,"[CONFIG] %s %d Error calling dlopen(%s): %s\n",__FILE__, __LINE__, libname,dlerror());
    st = -1;
  } else {
    sprintf (fname,"config_%s_init",cfgptr->cfgmode);
    cfgptr->init = dlsym(lib_handle,fname);

    if (cfgptr->init == NULL ) {
      printf("[CONFIG] %s %d no function %s for config mode %s\n",
             __FILE__, __LINE__,fname, cfgptr->cfgmode);
    } else {
      st = cfgptr->init(cfgptr);
      printf("[CONFIG] function %s returned %i\n",
             fname, st);
    }

    sprintf (fname,"config_%s_get",cfgptr->cfgmode);
    cfgptr->get = dlsym(lib_handle,fname);

    if (cfgptr->get == NULL ) {
      printf("[CONFIG] %s %d no function %s for config mode %s\n",
             __FILE__, __LINE__,fname, cfgptr->cfgmode);
      st = -1;
    }

    sprintf (fname,"config_%s_getlist",cfgptr->cfgmode);
    cfgptr->getlist = dlsym(lib_handle,fname);

    if (cfgptr->getlist == NULL ) {
      printf("[CONFIG] %s %d no function %s for config mode %s\n",
             __FILE__, __LINE__,fname, cfgptr->cfgmode);
      st = -1;
    }

    if (cfgptr->rtflags & CONFIG_SAVERUNCFG) {
      sprintf(fname, "config_%s_set", cfgptr->cfgmode);
      cfgptr->set = dlsym(lib_handle, fname);

      if (cfgptr->set == NULL) {
        printf("[CONFIG] %s %d no function %s for config mode %s\n", __FILE__, __LINE__, fname, cfgptr->cfgmode);
        st = -1;
      }
      sprintf(fname, "config_%s_write_parsedcfg", cfgptr->cfgmode);
      cfgptr->write_parsedcfg = dlsym(lib_handle, fname);

      if (cfgptr->write_parsedcfg == NULL) {
        printf("[CONFIG] %s %d no function %s for config mode %s\n", __FILE__, __LINE__, fname, cfgptr->cfgmode);
      }
    }

    sprintf (fname,"config_%s_end",cfgptr->cfgmode);
    cfgptr->end = dlsym(lib_handle,fname);

    if (cfgptr->end == NULL) {
      printf("[CONFIG] %s %d no function %s for config mode %s\n",
             __FILE__, __LINE__,fname, cfgptr->cfgmode);
    }
  }

  return st;
}
/*-----------------------------------------------------------------------------------*/
/* from here: interface implementtion of the configuration module */
static int config_cmdlineonly_init(configmodule_interface_t *cfg)
{
  return 0;
}

static void config_cmdlineonly_end(configmodule_interface_t *cfg)
{
}

int config_cmdlineonly_getlist(configmodule_interface_t *cfg,
                               paramlist_def_t *ParamList,
                               paramdef_t *params,
                               int numparams,
                               const char *prefix)
{
  UNUSED(cfg);
  UNUSED(params);
  UNUSED(numparams);
  UNUSED(prefix);
  ParamList->numelt = 0;
  return 0;
}

int config_cmdlineonly_get(configmodule_interface_t *cfg, paramdef_t *cfgoptions, int numoptions, const char *prefix)
{
  int defval;
  int fatalerror=0;
  int numdefvals=0;

  for(int i=0; i<numoptions; i++) {
    defval=0;

    switch(cfgoptions[i].type) {
      case TYPE_STRING:
        defval = config_setdefault_string(cfg, &cfgoptions[i], prefix);
        break;

      case TYPE_STRINGLIST:
        defval = config_setdefault_stringlist(cfg, &cfgoptions[i], prefix);
        break;

      case TYPE_UINT8:
      case TYPE_INT8:
      case TYPE_UINT16:
      case TYPE_INT16:
      case TYPE_UINT32:
      case TYPE_INT32:
      case TYPE_MASK:
        defval = config_setdefault_int(cfg, &cfgoptions[i], prefix);
        break;

      case TYPE_UINT64:
      case TYPE_INT64:
        defval = config_setdefault_int64(cfg, &cfgoptions[i], prefix);
        break;

      case TYPE_UINTARRAY:
      case TYPE_INTARRAY:
        defval = config_setdefault_intlist(cfg, &cfgoptions[i]);
        break;

      case TYPE_DOUBLE:
        defval = config_setdefault_double(cfg, &cfgoptions[i]);
        break;

      case TYPE_IPV4ADDR:
        defval = config_setdefault_ipv4addr(cfg, &cfgoptions[i]);
        break;

      default:
        fprintf(stderr,"[CONFIG] %s.%s type %i not supported\n",prefix, cfgoptions[i].optname,cfgoptions[i].type);
        fatalerror=1;
        break;
    } /* switch on param type */

    if (defval == 1) {
      numdefvals++;
      cfgoptions[i].paramflags = cfgoptions[i].paramflags |  PARAMFLAG_PARAMSETDEF;
    }
  } /* for loop on options */

  printf_params(cfg,
                "[CONFIG] %s: %i/%i parameters successfully set \n",
                prefix == NULL ? "(root)" : prefix,
                numdefvals,
                numoptions);

  if (fatalerror == 1) {
    fprintf(stderr,"[CONFIG] fatal errors found when assigning %s parameters \n",
            prefix);
  }

  return numdefvals;
}

configmodule_interface_t *load_configmodule(int argc,
					    char **argv,
					    uint32_t initflags)
{
  char *cfgparam=NULL;
  char *modeparams=NULL;
  char *cfgmode=NULL;
  char *strtokctx=NULL;
  char *atoken;
  uint32_t tmpflags=0;
  int i;
  int OoptIdx=-1;
  int OWoptIdx = -1;

  printf("CMDLINE: ");
  for (int i=0; i<argc; i++)
    printf("\"%s\" ", argv[i]);
  printf("\n");
  fflush(stdout);

  /* first parse the command line to look for the -O option */
  for (i = 0; i<argc; i++) {
    if (strlen(argv[i]) < 2)
    	continue;

    if ( argv[i][1] == 'O' && i < (argc -1)) {
      cfgparam = argv[i+1];
      OoptIdx=i;
    }

    char *OWopt = strstr(argv[i], "OW");
    if (OWopt == argv[i] + 2) {
      OWoptIdx = i;
    }
    if ( strstr(argv[i], "help_config") != NULL  ) {
      config_printhelp(Config_Params, sizeofArray(Config_Params), CONFIG_SECTIONNAME);
      exit(0);
    }

    if ( (strcmp(argv[i]+1, "h") == 0) || (strstr(argv[i]+1, "help_") != NULL ) ) {
      tmpflags = CONFIG_HELP;
    }
  }

  /* look for the OAI_CONFIGMODULE environment variable */
  if ( cfgparam == NULL ) {
    cfgparam = getenv("OAI_CONFIGMODULE");
  }

  /* default different for UE and softmodem because UE may run without config file */
  /* and -O option is not mandatory for UE                                    */
  /* phy simulators behave as UE                                              */
  if (cfgparam == NULL) {
    tmpflags = tmpflags | CONFIG_NOOOPT;

    if ( initflags & CONFIG_ENABLECMDLINEONLY) {
      cfgparam = CONFIG_CMDLINEONLY ":dbgl0" ;
    } else {
      cfgparam = CONFIG_CMDLINEONLY ":dbgl0" ;
      cfgparam = CONFIG_LIBCONFIGFILE ":" DEFAULT_CFGFILENAME;
    }
  }

  /* parse the config parameters to set the config source */
  i = sscanf(cfgparam,"%m[^':']:%ms",&cfgmode,&modeparams);

  if (i< 0) {
    fprintf(stderr,"[CONFIG] %s, %d, sscanf error parsing config source  %s: %s\n", __FILE__, __LINE__,cfgparam, strerror(errno));
    exit(-1) ;
  } else if ( i == 1 ) {
    /* -O argument doesn't contain ":" separator, assume -O <conf file> option, default cfgmode to libconfig
       with one parameter, the path to the configuration file cfgmode must not be NULL */
    modeparams = cfgmode;
    if (strstr(modeparams, ".yaml") != NULL || strstr(modeparams, ".yml") != NULL) {
      cfgmode = strdup("yaml");
    } else {
      cfgmode = strdup(CONFIG_LIBCONFIGFILE);
    }
  }

  // The macros are not thread safe print_params and similar
  configmodule_interface_t *cfgptr = calloc(sizeof(configmodule_interface_t), 1);
  if (!cfgptr) {
    fprintf(stderr, "ERROR: cannot allocate a memory for configuration\n");
    return NULL;
  }
  /* argv_info is used to memorize command line options which have been recognized */
  /* and to detect unrecognized command line options which might have been specified */
    cfgptr->argv_info = calloc(sizeof(int32_t), argc+10);
  /* argv[0] is the exec name, always Ok */
    cfgptr->argv_info[0] |= CONFIG_CMDLINEOPT_PROCESSED;

    /* When reuested _(_--OW or rtflag is 5), a file with config parameters, as defined after all processing, will be created */
    if (OWoptIdx >= 0) {
      cfgptr->argv_info[OWoptIdx] |= CONFIG_CMDLINEOPT_PROCESSED;
      cfgptr->rtflags |= CONFIG_SAVERUNCFG;
    }
  /* when OoptIdx is >0, -O option has been detected at position OoptIdx 
   *  we must memorize arv[OoptIdx is Ok                                  */ 
    if (OoptIdx >= 0) {
      cfgptr->argv_info[OoptIdx] |= CONFIG_CMDLINEOPT_PROCESSED;
      cfgptr->argv_info[OoptIdx+1] |= CONFIG_CMDLINEOPT_PROCESSED;
    }

    cfgptr->rtflags = cfgptr->rtflags | tmpflags;
    cfgptr->argc   = argc;
    cfgptr->argv   = argv;
    cfgptr->cfgmode=strdup(cfgmode);
    cfgptr->num_cfgP=0;
    atoken=strtok_r(modeparams,":",&strtokctx);

    while ( cfgptr->num_cfgP< CONFIG_MAX_OOPT_PARAMS && atoken != NULL) {
    /* look for debug level in the config parameters, it is common to all config mode
       and will be removed from the parameter array passed to the shared module */
      char *aptr;
      aptr=strcasestr(atoken,"dbgl");

      if (aptr != NULL) {
        cfgptr->rtflags = cfgptr->rtflags | strtol(aptr+4,NULL,0);
      } else {
        cfgptr->cfgP[cfgptr->num_cfgP] = strdup(atoken);
        cfgptr->num_cfgP++;
      }

      atoken = strtok_r(NULL,":",&strtokctx);
    }

    for (i = 0; i < cfgptr->num_cfgP; i++) {
      /* check if that file actually exists */
      if (access(cfgptr->cfgP[i], F_OK) != 0) {
        fprintf(stderr, "error: file %s does not exist\n", cfgptr->cfgP[i]);
        for (int j = 0; j < cfgptr->num_cfgP; ++j)
          free(cfgptr->cfgP[j]);
        free(modeparams);
        free(cfgptr->cfgmode);
        free(cfgptr->argv_info);
        free(cfgptr);
        if (cfgmode != NULL)
          free(cfgmode);
        return NULL;
      }
    }

  if (cfgptr->rtflags & CONFIG_PRINTPARAMS) {
    cfgptr->status = malloc(sizeof(configmodule_status_t));
  }
  if (strstr(cfgparam,CONFIG_CMDLINEONLY) == NULL) {
    i=load_config_sharedlib(cfgptr);

    if (i == 0) {
      int idx = config_paramidx_fromname(Config_Params, sizeofArray(Config_Params), CONFIGP_DEBUGFLAGS);
      Config_Params[idx].uptr = &(cfgptr->rtflags);
      idx = config_paramidx_fromname(Config_Params, sizeofArray(Config_Params), CONFIGP_TMPDIR);
      Config_Params[idx].strptr = &(cfgptr->tmpdir);
      config_get(cfgptr, Config_Params, sizeofArray(Config_Params), CONFIG_SECTIONNAME);
    } else {
      fprintf(stderr, "[CONFIG] config module \"%s\" couldn't be loaded\n", cfgmode);
      cfgptr->rtflags = cfgptr->rtflags | CONFIG_HELP | CONFIG_ABORT;
    }
  } else {
    cfgptr->init = config_cmdlineonly_init;
    cfgptr->get = config_cmdlineonly_get;
    cfgptr->getlist = config_cmdlineonly_getlist;
    cfgptr->end = config_cmdlineonly_end;
  }

  printf_params(cfgptr, "[CONFIG] debug flags: 0x%08x\n", cfgptr->rtflags);

  if (modeparams != NULL) free(modeparams);

  if (cfgmode != NULL) free(cfgmode);

  return cfgptr;
}

/* Possibly write config file, with parameters which have been read and  after command line parsing */
void write_parsedcfg(configmodule_interface_t *cfgptr)
{
  if (cfgptr->status && (cfgptr->rtflags & CONFIG_SAVERUNCFG)) {
    printf_params(cfgptr,
                  "[CONFIG] Runtime params creation status: %i null values, %i errors, %i empty list or array, %i successfull \n",
                  cfgptr->status->num_err_nullvalue,
                  cfgptr->status->num_err_write,
                  cfgptr->status->emptyla,
                  cfgptr->status->num_write);
  }
  if (cfgptr->write_parsedcfg != NULL) {
    printf_params(cfgptr, "[CONFIG] calling config module write_parsedcfg function...\n");
    cfgptr->write_parsedcfg(cfgptr);
  }
}

/* free memory allocated when reading parameters */
/* config module could be initialized again after this call */
void end_configmodule(configmodule_interface_t *cfgptr)
{
  if (cfgptr != NULL) {
    write_parsedcfg(cfgptr);
    if (cfgptr->end != NULL) {
      printf_params(cfgptr, "[CONFIG] calling config module end function...\n");
      cfgptr->end(cfgptr);
    }

    pthread_mutex_lock(&cfgptr->memBlocks_mutex);

    for(int i=0; i<cfgptr->numptrs ; i++) {
      if (cfgptr->oneBlock[i].ptrs != NULL && cfgptr->oneBlock[i].ptrsAllocated== true && cfgptr->oneBlock[i].toFree) {
        free(cfgptr->oneBlock[i].ptrs);
        memset(&cfgptr->oneBlock[i], 0, sizeof(cfgptr->oneBlock[i]));
      }
    }
    
    cfgptr->numptrs=0;
    pthread_mutex_unlock(&cfgptr->memBlocks_mutex);
    for (int i = 0; i < cfgptr->num_cfgP; i++) {
      if (cfgptr->cfgP[i] != NULL) {
        free(cfgptr->cfgP[i]);
      }
    }
    if (cfgptr->cfgmode)
      free(cfgptr->cfgmode);

    if (cfgptr->argv_info)
      free(cfgptr->argv_info);

    if (cfgptr->tmpdir)
      free(cfgptr->tmpdir);

    if (cfgptr->status) {
      if (cfgptr->status->debug_cfgname)
        free(cfgptr->status->debug_cfgname);
      free(cfgptr->status);
    }

    free(cfgptr);
    for (int i = 0; i < sizeofArray(Config_Params); i++) {
      Config_Params[i].voidptr = NULL;
      Config_Params[i].numelt = 0;
      Config_Params[i].paramflags &= ~PARAMFLAG_PARAMSETDEF;
    }
  }
}
