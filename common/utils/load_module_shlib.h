/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief include file for users of the shared lib loader
 */

#ifndef LOAD_SHLIB_H
#define LOAD_SHLIB_H

typedef struct {
   char *fname;
   int (*fptr)(void);
}loader_shlibfunc_t;

typedef struct {
   char               *name;
   char               *shlib_version;    // 
   char               *shlib_buildversion;
   char               *thisshlib_path;
   uint32_t           numfunc;
   loader_shlibfunc_t *funcarray;
   uint32_t           len_funcarray;
}loader_shlibdesc_t;

typedef struct {
  char               *mainexec_buildversion;
  char               *shlibpath;
  uint32_t           maxshlibs;
  uint32_t           numshlibs;
  loader_shlibdesc_t *shlibs;
}loader_data_t;

/* function type of functions which may be implemented by a module */
/* 1: init function, called when loading, if found in the shared lib */
typedef void (*initfunc_t)(void *);

/* 2: version checking function, called when loading, if it returns -1, trigger main exec abort  */
typedef int(*checkverfunc_t)(char * mainexec_version, char ** shlib_version);
/* 3: get function array function, called when loading when a module doesn't provide */
/* the function array when calling load_module_shlib (farray param NULL)             */
typedef int(*getfarrayfunc_t)(loader_shlibfunc_t **funcarray);

#define LOADER_CONFIG_PREFIX  "loader"
#define DEFAULT_PATH      ""
#define DEFAULT_MAXSHLIBS 10
extern loader_data_t loader_data;

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*                                       LOADER parameters                                                                                                  */
/*   optname      helpstr   paramflags           XXXptr	                           defXXXval	              type       numelt   check func*/
/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/
// clang-format off
#define LOADER_PARAMS_DESC {                                                                                                      \
  {"shlibpath", NULL, PARAMFLAG_NOFREE, .strptr = &loader_data.shlibpath, .defstrval = DEFAULT_PATH,      TYPE_STRING, 0, NULL }, \
  {"maxshlibs", NULL, 0,                .uptr = &(loader_data.maxshlibs), .defintval = DEFAULT_MAXSHLIBS, TYPE_UINT32, 0, NULL }, \
}
// clang-format on

int load_module_version_shlib(char *modname, char *version, loader_shlibfunc_t *farray, int numf, void *initfunc_arg);
void *get_shlibmodule_fptr(const char *modname, const char *fname);
#define load_module_shlib(M, F, N, I) load_module_version_shlib(M, NULL, F, N, I)
void loader_reset();
#endif

