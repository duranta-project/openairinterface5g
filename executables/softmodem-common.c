/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief common code for 5G and LTE softmodem main xNB and UEs source (nr-softmodem.c, lte-softmodem.c...)
 */
#include <time.h>
#include <dlfcn.h>
#include <sys/resource.h>
#include "UTIL/OPT/opt.h"
#include "common/config/config_userapi.h"
#include "common/utils/load_module_shlib.h"
#include "common/utils/telnetsrv/telnetsrv.h"
#include "common/utils/tuntap_if.h"
#include "executables/thread-common.h"
#include "common/utils/LOG/log.h"
#include "softmodem-common.h"
#include "nfapi/oai_integration/vendor_ext.h"

char *parallel_config=NULL;
char *worker_config=NULL;
int usrp_tx_thread = 0;
uint8_t nfapi_mode=0;

static struct timespec start;

static softmodem_params_t softmodem_params;
softmodem_params_t *get_softmodem_params(void) {
  return &softmodem_params;
}

char *get_softmodem_function(void)
{
  optmask_t fmask = *get_softmodem_optmask();
  if (fmask.bit.SOFTMODEM_ENB_BIT)
    return "enb";
  if (fmask.bit.SOFTMODEM_GNB_BIT)
    return "gnb";
  if (fmask.bit.SOFTMODEM_4GUE_BIT)
    return "4Gue";
  if (fmask.bit.SOFTMODEM_5GUE_BIT)
    return "5Gue";
  return "???";
}

void get_common_options(configmodule_interface_t *cfg)
{
  int32_t stats_disabled = 0;
  uint32_t online_log_messages=0;
  uint32_t glog_level=0 ;
  uint32_t start_telnetsrv = 0, start_telnetclt = 0;
  uint32_t start_websrv = 0;
  uint32_t noS1 = 0, nonbiot = 0;
  uint32_t rfsim = 0, do_forms = 0;
  uint32_t enable_imscope = 0;
  uint32_t enable_imscope_record = 0;
  uint32_t tun_backpressure = 0;
  int nfapi_index = 0;
  char *logmem_filename = NULL;

  paramdef_t cmdline_params[] = CMDLINE_PARAMS_DESC;
  checkedparam_t cmdline_CheckParams[] = CMDLINE_PARAMS_CHECK_DESC;
  static_assert(sizeofArray(cmdline_params) == sizeofArray(cmdline_CheckParams),
                "cmdline_params and cmdline_CheckParams should have the same size");
  int numparams = sizeofArray(cmdline_params);
  config_set_checkfunctions(cmdline_params, cmdline_CheckParams, numparams);
  config_get(cfg, cmdline_params, numparams, NULL);
  nfapi_index = config_paramidx_fromname(cmdline_params, numparams, "nfapi");
  AssertFatal(nfapi_index >= 0,"Index for nfapi config option not found!");
  nfapi_mode = config_get_processedint(cfg, &cmdline_params[nfapi_index]);

  paramdef_t cmdline_logparams[] =CMDLINE_LOGPARAMS_DESC ;
  checkedparam_t cmdline_log_CheckParams[] = CMDLINE_LOGPARAMS_CHECK_DESC;
  static_assert(sizeofArray(cmdline_logparams) == sizeofArray(cmdline_log_CheckParams),
                "cmdline_logparams and cmdline_log_CheckParams should have the same size");

  int numlogparams = sizeofArray(cmdline_logparams);
  config_set_checkfunctions(cmdline_logparams, cmdline_log_CheckParams, numlogparams);
  config_get(cfg, cmdline_logparams, numlogparams, NULL);

  if(config_isparamset(cmdline_logparams,config_paramidx_fromname(cmdline_logparams,numparams, CONFIG_FLOG_OPT))) {
    set_glog_onlinelog(online_log_messages);
  }

  if(config_isparamset(cmdline_logparams,config_paramidx_fromname(cmdline_logparams,numparams, CONFIG_LOGL_OPT))) {
    set_glog(glog_level);
  }

  if (start_telnetsrv) {
    load_module_shlib("telnetsrv",NULL,0,NULL);
  }
  
  if (start_telnetclt) {
    IS_SOFTMODEM_TELNETCLT = true;
  }

  if (logmem_filename != NULL && strlen(logmem_filename) > 0) {
    printf("Enabling OPT for log save at memory %s\n",logmem_filename);
    logInit_log_mem(logmem_filename);
  }

  if (noS1) {
    IS_SOFTMODEM_NOS1 = true;
  }

  if (nonbiot) {
    IS_SOFTMODEM_NONBIOT = true;
  }

  if (rfsim) {
    IS_SOFTMODEM_RFSIM = true;
  }

  if (do_forms) {
    IS_SOFTMODEM_DOSCOPE = true;
  }

  if (enable_imscope) {
    IS_SOFTMODEM_IMSCOPE_ENABLED = true;
  }

  if (enable_imscope_record) {
    IS_SOFTMODEM_IMSCOPE_RECORD_ENABLED = true;
  }

  tuntap_set_backpressure(tun_backpressure);

  if (start_websrv) {
    load_module_shlib("websrv", NULL, 0, NULL);
  }

  if(parallel_config != NULL) set_parallel_conf(parallel_config);

  if(worker_config != NULL)   set_worker_conf(worker_config);
  nfapi_setmode(nfapi_mode);
  if (stats_disabled)
    IS_SOFTMODEM_NOSTATS = true;

  // To be removed in a future release (after 1 month)
  AssertFatal(get_softmodem_params()->default_pdu_session_id == -1,
              "Use uicc0.pdu_sessions.[0].id to change the requested PDU session ID (is %d)\n", get_softmodem_params()->default_pdu_session_id);
}

void softmodem_verify_mode(const softmodem_params_t *p)
{
  if (IS_SA_MODE(p)) {
    LOG_I(UTIL, "running in SA mode (no --phy-test, --do-ra, --nsa option present)\n");
    return;
  }

  if (p->phy_test)
    LOG_I(UTIL, "running in phy-test mode (--phy-test)\n");
  if (p->do_ra)
    LOG_I(UTIL, "running in do-ra mode (--do-ra)\n");
  if (p->nsa)
    LOG_I(UTIL, "running in NSA mode (--nsa)\n");
  int num_modes = p->phy_test + p->do_ra + p->nsa;
  AssertFatal(num_modes == 1, "--phy-test, --do-ra, and --nsa are mutually exclusive\n");
}

static void softmodem_printresources(telnet_printfunc_t pf)
{
  struct rusage usage;
  struct timespec stop;

  clock_gettime(CLOCK_BOOTTIME, &stop);

  uint64_t elapse = (stop.tv_sec - start.tv_sec) ;   // in seconds

  int st = getrusage(RUSAGE_SELF,&usage);
  if (!st) {
    pf("\nRun time: %lluh %llus\n",(unsigned long long)elapse/3600,(unsigned long long)(elapse - (elapse/3600)));
    pf("\tTime executing user inst.: %lds %ldus\n",(long)usage.ru_utime.tv_sec,(long)usage.ru_utime.tv_usec);
    pf("\tTime executing system inst.: %lds %ldus\n",(long)usage.ru_stime.tv_sec,(long)usage.ru_stime.tv_usec);
    pf("\tMax. Phy. memory usage: %ldkB\n",(long)usage.ru_maxrss);
    pf("\tPage fault number (no io): %ld\n",(long)usage.ru_minflt);
    pf("\tPage fault number (requiring io): %ld\n",(long)usage.ru_majflt);
    pf("\tNumber of file system read: %ld\n",(long)usage.ru_inblock);
    pf("\tNumber of filesystem write: %ld\n",(long)usage.ru_oublock);
    pf("\tNumber of context switch (process origin, io...): %ld\n",(long)usage.ru_nvcsw);
    pf("\tNumber of context switch (os origin, priority...): %ld\n",(long)usage.ru_nivcsw);
  }
}

void signal_handler(int sig) {
  //void *array[10];
  //size_t size;

  if (sig==SIGSEGV) {
    // get void*'s for all entries on the stack
    /* backtrace uses malloc, that is not good in signal handlers
     * I let the code, because it would be nice to make it better
    size = backtrace(array, 10);
    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, 2);
    */
    exit(-1);
  } else {
    if(sig==SIGINT ||sig==SOFTMODEM_RTSIGNAL)
      softmodem_printresources((telnet_printfunc_t)printf);
    if (sig != SOFTMODEM_RTSIGNAL) {
      printf("Linux signal %s...\n",strsignal(sig));
      exit_function(__FILE__, __FUNCTION__, __LINE__, "softmodem starting exit procedure\n", OAI_EXIT_NORMAL);
    }
  }
}



void set_softmodem_sighandler(void) {
  struct sigaction  act,oldact;
  clock_gettime(CLOCK_BOOTTIME, &start);
  memset(&act,0,sizeof(act));
  act.sa_handler=signal_handler;
  sigaction(SOFTMODEM_RTSIGNAL,&act,&oldact);
  // Disabled in order generate a core dump for analysis with gdb
  // Enable for clean exit on CTRL-C (i.e. record player, USRP...) 
  signal(SIGINT,  signal_handler);
  # if 0
  printf("Send signal %d to display resource usage...\n",SIGRTMIN+1);
  signal(SIGSEGV, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGABRT, signal_handler);
  #endif
}

