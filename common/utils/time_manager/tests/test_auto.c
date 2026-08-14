/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "common/utils/time_manager/time_manager.h"
#include "common/utils/LOG/log.h"
#include "common/config/config_userapi.h"

configmodule_interface_t *uniqCfg = NULL;

void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  if (s != NULL) {
    printf("%s:%d %s() Exiting OAI softmodem: %s\n", file, line, function, s);
  }
  if (assert)
    abort();
  else
    exit(1);
}

static int rlc_tick_count = 0;
static int pdcp_tick_count = 0;
static int x2ap_tick_count = 0;

void nr_rlc_ms_tick(void)
{
  rlc_tick_count++;
}

void nr_pdcp_ms_tick(void)
{
  pdcp_tick_count++;
}

void x2ap_ms_tick(void)
{
  x2ap_tick_count++;
}

/* return 1 if ok, 0 if error */
static int standalone_realtime(void)
{
  time_manager_tick_function_t tick_functions[] = {
    nr_pdcp_ms_tick,
    nr_rlc_ms_tick,
    x2ap_ms_tick
  };
  int tick_functions_count = sizeofArray(tick_functions);
  time_manager_start(tick_functions, tick_functions_count, TIME_SOURCE_REALTIME);

  sleep(1);

  time_manager_finish();

  LOG_I(UTIL,
        "standalone realtime: rlc ticks: %d pdcp ticks: %d x2ap ticks: %d\n",
        rlc_tick_count,
        pdcp_tick_count,
        x2ap_tick_count);

  /* let's accept 1% deviation from the 1000 counts we expect */
  return abs(rlc_tick_count-1000) < 10
         && abs(pdcp_tick_count - 1000) < 10
         && abs(x2ap_tick_count - 1000) < 10;
}

/* return 1 if ok, 0 if error */
static int standalone_iq_samples(void)
{
  time_manager_tick_function_t tick_functions[] = {
    nr_pdcp_ms_tick,
    nr_rlc_ms_tick,
    x2ap_ms_tick
  };
  int tick_functions_count = sizeofArray(tick_functions);
  time_manager_start(tick_functions, tick_functions_count, TIME_SOURCE_IQ_SAMPLES);

  /* let's pretend we have 2000 samples per second */
  for (int i = 0; i < 1000; i++) {
    time_manager_iq_samples(1, 2000);
    usleep(10);
    time_manager_iq_samples(1, 2000);
    usleep(10);
  }

  time_manager_finish();

  LOG_I(UTIL,
        "standalone iq-samples: rlc ticks: %d pdcp ticks: %d x2ap ticks: %d\n",
        rlc_tick_count,
        pdcp_tick_count,
        x2ap_tick_count);

  /* exact count is required */
  return rlc_tick_count == 1000
         && pdcp_tick_count == 1000
         && x2ap_tick_count == 1000;
}

#if 0
/* disabled - CI fails too much on those */

/* return 1 if ok, 0 if error */
static int client_server(char *program_name, bool iq_samples_time_source)
{
  LOG_I(UTIL, "client/server %s: launch sub-processes\n", iq_samples_time_source ? "iq-samples" : "realtime");

  pid_t server;
  pid_t client;

  /* start server */
  server = fork();
  DevAssert(server >= 0);
  if (server == 0) {
    /* run as server */
    if (iq_samples_time_source)
      execl(program_name, program_name, "--time_management.mode", "server", "--time_management.time_source", "iq_samples", NULL);
    else
      execl(program_name, program_name, "--time_management.mode", "server", NULL);
    exit(1);
  }

  /* wait a little bit so that server is ready to accept clients
   * (this is hackish)
   */
  usleep(1000);

  /* start client */
  client = fork();
  DevAssert(client >= 0);
  if (client == 0) {
    /* run as client */
    execl(program_name, program_name, "--time_management.mode", "client", NULL);
    exit(1);
  }

  /* wait for completion */
  int ret = 1;

  int status;
  pid_t r = waitpid(server, &status, 0);
  DevAssert(r == server);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    ret = 0;

  r = waitpid(client, &status, 0);
  DevAssert(r == client);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    ret = 0;

  return ret;
}

#endif /* #if 0 */

/* return 1 if ok, 0 if error */
static int run_sub_client_server(bool iq_samples_time_source)
{
  time_manager_tick_function_t tick_functions[] = {
    nr_pdcp_ms_tick,
    nr_rlc_ms_tick,
    x2ap_ms_tick
  };
  int tick_functions_count = sizeofArray(tick_functions);
  /* last argument doesn't matter, overwritten by args of execl in client_server() */
  time_manager_start(tick_functions, tick_functions_count, TIME_SOURCE_REALTIME);

  if (iq_samples_time_source) {
    /* 'iq_samples_time_source' is true only for server
     * wait a bit for client to be there otherwise server
     * may finish too early
     */
    usleep(100*1000);

    /* let's pretend we have 2000 samples per second */
    for (int i = 0; i < 1000; i++) {
      time_manager_iq_samples(1, 2000);
      usleep(10);
      time_manager_iq_samples(1, 2000);
      usleep(10);
    }
  } else {
    sleep(1);
  }

  time_manager_finish();

  LOG_I(UTIL, "client/server: rlc ticks: %d pdcp ticks: %d x2ap ticks: %d\n", rlc_tick_count, pdcp_tick_count, x2ap_tick_count);

  if (iq_samples_time_source)
    return rlc_tick_count == 1000
           && pdcp_tick_count == 1000
           && x2ap_tick_count == 1000;
  else
    /* let's accept 1% deviation from the 1000 counts we expect */
    return abs(rlc_tick_count-1000) < 10
           && abs(pdcp_tick_count - 1000) < 10
           && abs(x2ap_tick_count - 1000) < 10;
}

int main(int argc, char **argv)
{
  if ((uniqCfg = load_configmodule(argc, argv, CONFIG_ENABLECMDLINEONLY)) == NULL) {
    fprintf(stderr, "configuration module init failed\n");
    return 1;
  }
  logInit();

  /* for client/server tests, we use sub-processes with command line parameters */
  if (argc > 1) {
    /* if there are command line parameters then it's a sub-process */
    rlc_tick_count = 0;
    pdcp_tick_count = 0;
    x2ap_tick_count = 0;
    /* return value to the main test program
     * let's do as for the shell:  0 means success and 1 means error
     */
    /* we know we are in iq-samples time source mode if the last argument is "iq_samples" */
    int success = run_sub_client_server(!strcmp(argv[argc - 1], "iq_samples")) == 1;
    return 1 - success;
  }

  int err = 0;

  /* test standalone realtime */
  rlc_tick_count = 0;
  pdcp_tick_count = 0;
  x2ap_tick_count = 0;
  int ret = standalone_realtime();
  if (ret == 1)
    LOG_I(UTIL, "standalone realtime: OK\n");
  else {
    LOG_E(UTIL, "standalone realtime: ERROR\n");
    err = 1;
  }

  /* test standalone iq-samples */
  rlc_tick_count = 0;
  pdcp_tick_count = 0;
  x2ap_tick_count = 0;
  ret = standalone_iq_samples();
  if (ret == 1)
    LOG_I(UTIL, "standalone iq-samples: OK\n");
  else {
    LOG_E(UTIL, "standalone iq-samples: ERROR\n");
    err = 1;
  }

#if 0
  /* CI fails, sometimes - those tests are disabled */

  /* client/server tests are not written in a robust way
   * they may fail depending on realtime on the machine
   * de-activate if needed (if CI fails)
   */
  /* test client/server realtime */
  rlc_tick_count = 0;
  pdcp_tick_count = 0;
  x2ap_tick_count = 0;
  ret = client_server(argv[0], false);
  if (ret == 1)
    LOG_I(UTIL, "client-server realtime: OK\n");
  else {
    LOG_E(UTIL, "client-server realtime: ERROR\n");
    err = 1;
  }

  /* test client/server iq-samples */
  rlc_tick_count = 0;
  pdcp_tick_count = 0;
  x2ap_tick_count = 0;
  ret = client_server(argv[0], true);
  if (ret == 1)
    LOG_I(UTIL, "client-server iq-samples: OK\n");
  else {
    LOG_E(UTIL, "client-server iq-samples: ERROR\n");
    err = 1;
  }
#endif

  logTerm();

  return err;
}
